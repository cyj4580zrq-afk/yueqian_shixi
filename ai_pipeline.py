#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
WSL AI 助手常驻循环服务：监听 cmd.wav -> 讯飞ASR -> DeepSeek -> 讯飞TTS -> 回传开发板
"""

import os
import re
import socket
import subprocess
import sys
import json
import time
from pathlib import Path

PROJECT_DIR = Path("/home/cyj/workspace/ai_assistant")
APP_CONFIG = PROJECT_DIR / "config" / "app_config.json"
DEFAULT_CMD_WAV = PROJECT_DIR / "bin" / "wav" / "cmd.wav"
REPLY_WAV = PROJECT_DIR / "bin" / "wav" / "reply.wav"
BOARD_REPLY_WAV = "/root/code/wav/reply.wav"
ASR_TOOL = PROJECT_DIR / "bin" / "iat_online_sample"
TTS_TOOL = PROJECT_DIR / "bin" / "tts_online_sample"
CHAT_TOOL = PROJECT_DIR / "bin" / "chat_deepseek.py"
CONTROL_PORT = 8888
WAV_DATA_PORT = 8889


def _tool_env():
    env = os.environ.copy()
    lib_dir = str(PROJECT_DIR / "libs" / "x86_64")
    env["LD_LIBRARY_PATH"] = f"{lib_dir}:{env.get('LD_LIBRARY_PATH', '')}".rstrip(":")
    return env


def load_config() -> dict:
    if not APP_CONFIG.exists():
        return {}
    try:
        with open(APP_CONFIG, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception as e:
        print(f"[Config Error] Failed to load app_config.json: {e}")
        return {}


def do_asr(wav_path: Path) -> str:
    if not ASR_TOOL.exists():
        raise FileNotFoundError(f"ASR tool not found: {ASR_TOOL}")
    result = subprocess.run(
        [str(ASR_TOOL), str(wav_path)],
        capture_output=True,
        text=True,
        cwd=str(PROJECT_DIR),
        env=_tool_env(),
        timeout=120,
    )
    output = (result.stdout or "") + "\n" + (result.stderr or "")
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
    if not CHAT_TOOL.exists():
        raise FileNotFoundError(f"DeepSeek tool not found: {CHAT_TOOL}")
    result = subprocess.run(
        ["python3", str(CHAT_TOOL), question],
        capture_output=True,
        text=True,
        cwd=str(PROJECT_DIR),
        timeout=60,
    )
    answer = (result.stdout or "").strip().splitlines()
    answer = answer[-1].strip() if answer else ""
    if not answer:
        raise RuntimeError(f"DeepSeek returned empty output: {result.stderr}")
    return answer


def do_tts(text: str, output_path: Path) -> None:
    if not TTS_TOOL.exists():
        raise FileNotFoundError(f"TTS tool not found: {TTS_TOOL}")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        [str(TTS_TOOL), text, str(output_path)],
        capture_output=True,
        text=True,
        cwd=str(PROJECT_DIR),
        env=_tool_env(),
        timeout=120,
    )
    output = (result.stdout or "") + "\n" + (result.stderr or "")
    if not output_path.exists() or output_path.stat().st_size < 1000:
        raise RuntimeError(f"TTS failed. output={output[:500]}")


def send_control_message(board_ip: str, message: str) -> bool:
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect((board_ip, CONTROL_PORT))
        sock.send(message.encode("utf-8"))
        sock.close()
        return True
    except Exception as exc:
        print(f"[Control] send failed: {exc}")
        return False


def _send_wav_via_tcp(board_ip: str, reply_wav: Path) -> bool:
    try:
        wav_data = reply_wav.read_bytes()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(10)
        print(f"[Deploy] TCP WAV target: {board_ip}:{WAV_DATA_PORT}")
        sock.connect((board_ip, WAV_DATA_PORT))
        sock.sendall(wav_data)  # 修正：直接发送原始音频流，不带 4 字节的头长度
        sock.close()
        return True
    except Exception as exc:
        print(f"[Deploy] TCP WAV send failed: target={board_ip}:{WAV_DATA_PORT} error={exc}")
        return False


def send_to_board(board_ip: str, reply_wav: Path, recognized_text: str, reply_text: str, config: dict) -> bool:
    ok = True
    board_reply_path = config.get("local_voice_reply_wav", "/tmp/reply.wav")
    
    dir_to_make = os.path.dirname(board_reply_path)
    if dir_to_make and dir_to_make != "/" and dir_to_make != "/tmp":
        mkdir_cmd = [
            "ssh", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=no", "-o", "ConnectTimeout=5",
            f"root@{board_ip}", f"mkdir -p {dir_to_make}"
        ]
        print(f"[Deploy] SSH mkdir command: {' '.join(mkdir_cmd)}")
        mkdir_result = subprocess.run(mkdir_cmd, capture_output=True, text=True)
        if mkdir_result.returncode != 0:
            print(f"[Deploy] SSH mkdir failed: {mkdir_result.stderr.strip() or mkdir_result.stdout.strip()}")
            ok = False

    scp_cmd = [
        "scp", "-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=no", "-o", "ConnectTimeout=5",
        str(reply_wav), f"root@{board_ip}:{board_reply_path}"
    ]
    print(f"[Deploy] SCP command: {' '.join(scp_cmd)}")
    scp_result = subprocess.run(scp_cmd, capture_output=True, text=True)
    if scp_result.returncode != 0:
        print(f"[Deploy] SCP failed: {scp_result.stderr.strip() or scp_result.stdout.strip()}")
        ok = False

    # 双通道传输
    ok = _send_wav_via_tcp(board_ip, reply_wav) and ok
    if recognized_text:
        ok = send_control_message(board_ip, f"ASR:{recognized_text}") and ok
    if reply_text:
        ok = send_control_message(board_ip, f"AI:{reply_text}") and ok
    return ok


def process_pipeline(board_ip: str, cmd_wav: Path, config: dict) -> None:
    print("-" * 40)
    print(f"[Pipeline] Processing audio: {cmd_wav}")
    
    # 1. 语音识别 (ASR)
    question = do_asr(cmd_wav)
    print(f"[Pipeline] ASR Result: {question}")
    
    # 2. AI 对话 (DeepSeek)
    answer = do_ai_chat(question)
    print(f"[Pipeline] AI Reply: {answer}")
    
    # 3. TTS 语音合成
    do_tts(answer, REPLY_WAV)
    
    # 4. 回传至板端
    send_to_board(board_ip, REPLY_WAV, question, answer, config)
    print("[Pipeline] Process complete!")
    print("-" * 40)


def main() -> None:
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

    while True:
        try:
            # 重新加载配置
            config = load_config()
            board_ip = config.get("board_ip", board_ip)
            cmd_wav_path = Path(config.get("wsl_voice_cmd_wav", str(cmd_wav_path)))

            if cmd_wav_path.exists():
                mtime = cmd_wav_path.stat().st_mtime
                if mtime > last_mtime:
                    size_before = cmd_wav_path.stat().st_size
                    if size_before > 0:
                        # 延迟小段时间确保 SCP 文件完全写入完毕
                        time.sleep(0.3)
                        size_after = cmd_wav_path.stat().st_size
                        if size_before == size_after:
                            last_mtime = mtime
                            process_pipeline(board_ip, cmd_wav_path, config)
            
            time.sleep(0.1)
        except KeyboardInterrupt:
            print("\n[Service] Interrupted by user. Exiting.")
            break
        except Exception as e:
            print(f"[Service Error] Exception occurred: {e}")
            time.sleep(1.0)


if __name__ == "__main__":
    main()
