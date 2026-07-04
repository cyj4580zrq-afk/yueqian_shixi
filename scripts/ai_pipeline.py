#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
WSL AI pipeline.

Stage 1:
- keep the existing file paths and network ports
- split the serial flow into worker threads and queues
- keep DeepSeek requests inside the AI worker
- replace blocking subprocess.run with subprocess.Popen in worker-side helpers
"""

import fcntl
import importlib.util
import json
import os
import queue
import re
import socket
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, field
from itertools import count
from pathlib import Path

PROJECT_DIR = Path("/home/cyj/workspace/ai_assistant")
APP_CONFIG = PROJECT_DIR / "config" / "app_config.json"
DEFAULT_CMD_WAV = PROJECT_DIR / "bin" / "wav" / "cmd.wav"
REPLY_WAV = PROJECT_DIR / "bin" / "wav" / "reply.wav"
ASR_TOOL = PROJECT_DIR / "bin" / "iat_online_sample"
TTS_TOOL = PROJECT_DIR / "bin" / "tts_online_sample"
CHAT_TOOL = PROJECT_DIR / "bin" / "chat_deepseek.py"
CONTROL_PORT = 8888
WAV_DATA_PORT = 8889
XF_LOCK_FILE = Path("/tmp/ai_assistant_xunfei.lock")

audio_queue = queue.Queue(maxsize=8)
text_queue = queue.Queue(maxsize=8)
reply_queue = queue.Queue(maxsize=8)
audio_out_queue = queue.Queue(maxsize=8)

_PIPELINE_WORKERS_STARTED = False
_PIPELINE_WORKERS_LOCK = threading.Lock()
_CHAT_DEEPSEEK_FN = None
_JOB_SEQ = count(1)


@dataclass
class PipelineJob:
    job_id: int
    board_ip: str
    cmd_wav_path: Path
    config: dict
    question: str = ""
    answer: str = ""
    error: str = ""
    done_event: threading.Event = field(default_factory=threading.Event)
    result_code: int = 0


def _tool_env():
    env = os.environ.copy()
    lib_dir = str(PROJECT_DIR / "libs" / "x86_64")
    env["LD_LIBRARY_PATH"] = f"{lib_dir}:{env.get('LD_LIBRARY_PATH', '')}".rstrip(":")
    return env


def _load_chat_deepseek_fn():
    global _CHAT_DEEPSEEK_FN
    if _CHAT_DEEPSEEK_FN is not None:
        return _CHAT_DEEPSEEK_FN

    if not CHAT_TOOL.exists():
        raise FileNotFoundError(f"DeepSeek tool not found: {CHAT_TOOL}")

    spec = importlib.util.spec_from_file_location("chat_deepseek_worker", str(CHAT_TOOL))
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Failed to load DeepSeek tool module: {CHAT_TOOL}")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    if not hasattr(module, "chat_deepseek"):
        raise RuntimeError(f"chat_deepseek() not found in: {CHAT_TOOL}")

    _CHAT_DEEPSEEK_FN = module.chat_deepseek
    return _CHAT_DEEPSEEK_FN


def load_config() -> dict:
    if not APP_CONFIG.exists():
        return {}
    try:
        with open(APP_CONFIG, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception as e:
        print(f"[Config Error] Failed to load app_config.json: {e}")
        return {}


def _run_popen(cmd, timeout, cwd=None, env=None):
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        cwd=cwd,
        env=env,
    )
    try:
        stdout, stderr = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        stdout, stderr = proc.communicate()
        raise TimeoutError(f"Command timeout: {' '.join(cmd)}")
    if proc.returncode != 0:
        raise RuntimeError((stderr or stdout or "").strip())
    return stdout or "", stderr or ""


def _run_xunfei_tool(cmd, timeout, attempts=3):
    last_error = None
    for attempt in range(1, attempts + 1):
        with open(XF_LOCK_FILE, "w") as lock_file:
            fcntl.flock(lock_file, fcntl.LOCK_EX)
            try:
                print(f"[Xunfei] running attempt {attempt}/{attempts}: {' '.join(cmd)}")
                return _run_popen(cmd, timeout=timeout, cwd=str(PROJECT_DIR), env=_tool_env())
            except Exception as exc:
                last_error = exc
                print(f"[Xunfei] attempt {attempt}/{attempts} failed: {exc}")
            finally:
                fcntl.flock(lock_file, fcntl.LOCK_UN)
        if attempt < attempts:
            time.sleep(1.0)
    raise last_error


def do_asr(wav_path: Path) -> str:
    if not ASR_TOOL.exists():
        raise FileNotFoundError(f"ASR tool not found: {ASR_TOOL}")
    stdout, stderr = _run_xunfei_tool([str(ASR_TOOL), str(wav_path)], timeout=120, attempts=3)
    output = stdout + "\n" + stderr
    for line in reversed(output.splitlines()):
        line = line.strip()
        if line.startswith("ASR:"):
            text = line[4:].strip()
            if text:
                return text
        match = re.search(r"识别文字结果[:：]\s*(.+)", line)
        if match and match.group(1).strip():
            return match.group(1).strip()
    raise RuntimeError(f"ASR failed. output={output[:500]}")

def do_ai_chat(question: str) -> str:
    chat_fn = _load_chat_deepseek_fn()
    answer = chat_fn(question)
    answer = (answer or "").strip()
    if not answer:
        raise RuntimeError("DeepSeek returned empty output")
    return answer


def do_tts(text: str, output_path: Path) -> None:
    if not TTS_TOOL.exists():
        raise FileNotFoundError(f"TTS tool not found: {TTS_TOOL}")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    last_output = ""
    for attempt in range(1, 4):
        try:
            if output_path.exists():
                output_path.unlink()
            stdout, stderr = _run_xunfei_tool([str(TTS_TOOL), text, str(output_path)], timeout=120, attempts=1)
            last_output = stdout + "\n" + stderr
            if output_path.exists() and output_path.stat().st_size >= 1000:
                return
            raise RuntimeError(f"TTS output missing or too small. output={last_output[:500]}")
        except Exception as exc:
            last_output = str(exc)
            print(f"[Pipeline] TTS attempt {attempt}/3 failed: {exc}")
            if attempt < 3:
                time.sleep(1.0)
    raise RuntimeError(f"TTS failed. output={last_output[:500]}")

def send_control_message(board_ip: str, message: str) -> bool:
    try:
        print(f"[Control] text target: {board_ip}:{CONTROL_PORT} payload={message}")
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect((board_ip, CONTROL_PORT))
        sock.send(message.encode("utf-8"))
        sock.close()
        return True
    except Exception as exc:
        print(f"[Control] send failed: target={board_ip}:{CONTROL_PORT} error={exc}")
        return False


def _send_wav_via_tcp(board_ip: str, reply_wav: Path) -> bool:
    try:
        wav_data = reply_wav.read_bytes()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        print(f"[Deploy] TCP WAV target: {board_ip}:{WAV_DATA_PORT}")
        sock.connect((board_ip, WAV_DATA_PORT))
        sock.sendall(wav_data)
        sock.close()
        return True
    except Exception as exc:
        print(f"[Deploy] TCP WAV send failed: target={board_ip}:{WAV_DATA_PORT} error={exc}")
        return False


def send_text_to_board(board_ip: str, recognized_text: str = "", reply_text: str = "") -> bool:
    ok = True
    if recognized_text:
        print(f"[Control] selected board_ip for text: {board_ip}")
        ok = send_control_message(board_ip, f"ASR:{recognized_text}") and ok
    if reply_text:
        print(f"[Control] selected board_ip for text: {board_ip}")
        ok = send_control_message(board_ip, f"AI:{reply_text}") and ok
    return ok


def send_wav_to_board(board_ip: str, reply_wav: Path) -> bool:
    print(f"[Deploy] selected board_ip for wav: {board_ip}")
    return _send_wav_via_tcp(board_ip, reply_wav)


def _mark_job_done(job: PipelineJob, result_code: int = 0, error: str = "") -> None:
    job.result_code = result_code
    job.error = error
    job.done_event.set()


def _queue_job(target_queue, job: PipelineJob, stage: str) -> None:
    try:
        target_queue.put(job, timeout=0.1)
    except queue.Full as exc:
        raise RuntimeError(f"{stage} queue full") from exc


def _asr_worker() -> None:
    while True:
        job = audio_queue.get()
        try:
            print(f"[Pipeline] ASR worker picked job #{job.job_id}: {job.cmd_wav_path}")
            job.question = do_asr(job.cmd_wav_path)
            print(f"[Pipeline] ASR Result #{job.job_id}: {job.question}")
            send_text_to_board(job.board_ip, recognized_text=job.question)
            _queue_job(text_queue, job, "text")
        except Exception as exc:
            err = f"ASR worker failed for job #{job.job_id}: {exc}"
            print(f"[Pipeline Error] {err}")
            send_text_to_board(job.board_ip, reply_text="语音识别失败，请再说一次。")
            _mark_job_done(job, result_code=1, error=err)
        finally:
            audio_queue.task_done()


def _ai_worker() -> None:
    while True:
        job = text_queue.get()
        try:
            print(f"[Pipeline] AI worker picked job #{job.job_id}")
            job.answer = do_ai_chat(job.question)
            print(f"[Pipeline] AI Reply #{job.job_id}: {job.answer}")
            send_text_to_board(job.board_ip, reply_text=job.answer)
            _queue_job(reply_queue, job, "reply")
        except Exception as exc:
            err = f"AI worker failed for job #{job.job_id}: {exc}"
            print(f"[Pipeline Error] {err}")
            _mark_job_done(job, result_code=1, error=err)
        finally:
            text_queue.task_done()


def _tts_worker() -> None:
    while True:
        job = reply_queue.get()
        try:
            print(f"[Pipeline] TTS worker picked job #{job.job_id}")
            do_tts(job.answer, REPLY_WAV)
            _queue_job(audio_out_queue, job, "audio_out")
        except Exception as exc:
            err = f"TTS worker failed for job #{job.job_id}: {exc}"
            print(f"[Pipeline Error] {err}")
            _mark_job_done(job, result_code=1, error=err)
        finally:
            reply_queue.task_done()


def _dispatch_worker() -> None:
    while True:
        job = audio_out_queue.get()
        try:
            print(f"[Pipeline] Dispatch worker picked job #{job.job_id}")
            ok = send_wav_to_board(job.board_ip, REPLY_WAV)
            if not ok:
                raise RuntimeError("send wav to board failed")
            print(f"[Pipeline] Process complete for job #{job.job_id}")
            _mark_job_done(job, result_code=0, error="")
        except Exception as exc:
            err = f"Dispatch worker failed for job #{job.job_id}: {exc}"
            print(f"[Pipeline Error] {err}")
            _mark_job_done(job, result_code=1, error=err)
        finally:
            audio_out_queue.task_done()


def _start_pipeline_workers() -> None:
    global _PIPELINE_WORKERS_STARTED
    with _PIPELINE_WORKERS_LOCK:
        if _PIPELINE_WORKERS_STARTED:
            return
        workers = [
            ("asr-worker", _asr_worker),
            ("ai-worker", _ai_worker),
            ("tts-worker", _tts_worker),
            ("dispatch-worker", _dispatch_worker),
        ]
        for name, target in workers:
            thread = threading.Thread(target=target, name=name, daemon=True)
            thread.start()
        _PIPELINE_WORKERS_STARTED = True


def submit_job(board_ip: str, cmd_wav_path: Path, config: dict = None) -> PipelineJob:
    if config is None:
        config = load_config()
    job = PipelineJob(
        job_id=next(_JOB_SEQ),
        board_ip=board_ip,
        cmd_wav_path=Path(cmd_wav_path),
        config=config,
    )
    _queue_job(audio_queue, job, "audio")
    return job


def process_pipeline(board_ip: str, cmd_wav: Path) -> None:
    job = submit_job(board_ip, cmd_wav)
    job.done_event.wait()
    if job.result_code != 0:
        raise RuntimeError(job.error or "pipeline failed")


def run_once(board_ip: str, cmd_wav_path: Path) -> int:
    if not board_ip:
        print("[Pipeline] Missing board_ip", file=sys.stderr)
        return 2
    if not cmd_wav_path.exists() or cmd_wav_path.stat().st_size <= 0:
        print(f"[Pipeline] cmd wav missing or empty: {cmd_wav_path}", file=sys.stderr)
        return 3
    try:
        _start_pipeline_workers()
        process_pipeline(board_ip, cmd_wav_path)
        return 0
    except Exception as e:
        print(f"[Pipeline Error] {e}", file=sys.stderr)
        return 1


def run_resident() -> int:
    print("=" * 50)
    print(" WSL AI Pipeline Resident Service Started")
    print("=" * 50)

    config = load_config()
    board_ip = config.get("board_ip", "10.203.129.238")
    cmd_wav_path = Path(config.get("wsl_voice_cmd_wav", str(DEFAULT_CMD_WAV)))

    print(f"[Service] Target Board IP: {board_ip}")
    print(f"[Service] Watching file: {cmd_wav_path}")

    last_mtime = 0.0
    if cmd_wav_path.exists():
        last_mtime = cmd_wav_path.stat().st_mtime
        print(f"[Service] Initial file mtime: {last_mtime}")

    _start_pipeline_workers()

    while True:
        try:
            config = load_config()
            board_ip = config.get("board_ip", board_ip)
            cmd_wav_path = Path(config.get("wsl_voice_cmd_wav", str(cmd_wav_path)))

            if cmd_wav_path.exists():
                mtime = cmd_wav_path.stat().st_mtime
                if mtime > last_mtime:
                    size_before = cmd_wav_path.stat().st_size
                    if size_before > 0:
                        time.sleep(0.3)
                        size_after = cmd_wav_path.stat().st_size
                        if size_before == size_after:
                            last_mtime = mtime
                            submit_job(board_ip, cmd_wav_path, config)

            time.sleep(0.4)
        except KeyboardInterrupt:
            print("\n[Service] Interrupted by user. Exiting.")
            return 0
        except Exception as e:
            print(f"[Service Error] Exception occurred: {e}")
            time.sleep(1.0)


def main() -> int:
    config = load_config()
    _start_pipeline_workers()
    if len(sys.argv) >= 2:
        board_ip = sys.argv[1]
        cmd_wav_path = Path(config.get("wsl_voice_cmd_wav", str(DEFAULT_CMD_WAV)))
        if len(sys.argv) >= 3:
            cmd_wav_path = Path(sys.argv[2])
        return run_once(board_ip, cmd_wav_path)
    return run_resident()


if __name__ == "__main__":
    sys.exit(main())

