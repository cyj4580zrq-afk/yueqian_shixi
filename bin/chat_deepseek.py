import sys
import os
import requests
import subprocess
import json
import re
from html import unescape
from urllib.parse import quote_plus
from datetime import datetime
import xml.etree.ElementTree as ET

# Read configuration from environment variables
api_key = os.environ.get("DEEPSEEK_API_KEY", "")
base_url = os.environ.get("DEEPSEEK_BASE_URL", "https://api.deepseek.com")
model = os.environ.get("DEEPSEEK_MODEL", "deepseek-chat")

# If not set, check if we can fall back or warn
if not api_key:
    # Print error to stderr but return a graceful mock message for initial testing if key is missing
    print("WARNING: DEEPSEEK_API_KEY environment variable is not set. Please set it in WSL.", file=sys.stderr)
    # We will return a mock answer so the workflow doesn't break entirely if the user hasn't configured it yet
    mock_responses = {
        "开灯": "打开LED灯",
        "打开灯": "打开LED灯",
        "打开绿色灯": "打开LED灯",
        "关灯": "关闭LED灯",
        "关闭灯": "关闭LED灯"
    }
    # Simple keyword match for testing
    question = sys.argv[1] if len(sys.argv) >= 2 else ""
    matched = False
    for k, v in mock_responses.items():
        if k in question:
            print(v)
            matched = True
            break
    if not matched:
        print("您好，我是deepseek智能语音助手")
    sys.exit(0)

import time

HISTORY_FILE = "/tmp/deepseek_history.json"
MAX_HISTORY_TURNS = 10  # 10轮对话 (user + assistant共20条消息)
SESSION_TIMEOUT = 300   # 5分钟超时自动清除
PROJECT_DIR = "/home/cyj/workspace/ai_assistant"
WEATHER_SCRIPT = os.path.join(PROJECT_DIR, "scripts", "get_weather.py")
WEATHER_TEXT_CACHE = os.path.join(PROJECT_DIR, "weather_cache.txt")
WEATHER_CONFIG_CACHE = os.path.join(PROJECT_DIR, "config", "weather_cache.txt")

def current_datetime_text():
    now = datetime.now()
    weekdays = ["星期一", "星期二", "星期三", "星期四", "星期五", "星期六", "星期日"]
    return now, f"{now.year}年{now.month}月{now.day}日，{weekdays[now.weekday()]}"

def should_search_web(prompt):
    local_keywords = ["开灯", "关灯", "打开灯", "关闭灯", "清空对话", "清除记忆", "重新开始"]
    if any(kw in prompt for kw in local_keywords):
        return False
    if any(kw in prompt for kw in ["几月几日", "今天几号", "今天是几号", "星期", "几点", "时间", "现在几点"]):
        return False
    search_keywords = [
        "搜索", "查一下", "查找", "联网", "上网", "最新", "新闻", "今天", "现在",
        "今年", "最近", "目前", "冠军", "比赛", "结果", "比分", "股价", "价格",
        "什么是", "多少", "排名"
    ]
    return any(kw in prompt for kw in search_keywords)


def is_invalid_search_text(text):
    if not text:
        return True
    bad_keywords = [
        "SecurityCompromiseError", '"code":451', '"data":null', "status\":451",
        "Access Denied", "unusual traffic", "captcha", "Too Many Requests"
    ]
    return any(kw in text for kw in bad_keywords)


def _format_weather_line(line):
    line = (line or "").strip()
    if not line:
        return ""
    line = line.replace("Wuhan:", "武汉")
    line = line.replace("Beijing:", "北京")
    line = line.replace("|", "；")
    return line[:80]


def load_weather_cache():
    for path in [WEATHER_TEXT_CACHE, WEATHER_CONFIG_CACHE]:
        try:
            if os.path.exists(path):
                with open(path, "r", encoding="utf-8") as fp:
                    line = fp.read().strip()
                formatted = _format_weather_line(line)
                if formatted:
                    return formatted
        except Exception as exc:
            print(f"[WeatherTool] cache read failed path={path}: {exc}", file=sys.stderr)
    return ""


def get_weather_answer():
    cached = load_weather_cache()
    if cached:
        return cached
    try:
        proc = subprocess.run(
            ["python3", WEATHER_SCRIPT],
            cwd=PROJECT_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=6,
        )
        output = (proc.stdout or proc.stderr or "").strip()
        if proc.returncode == 0 and output:
            output = output.splitlines()[-1].strip()
            formatted = _format_weather_line(output)
            if formatted:
                return formatted
        print(f"[WeatherTool] failed rc={proc.returncode} output={output[:120]}", file=sys.stderr)
    except Exception as exc:
        print(f"[WeatherTool] exception: {exc}", file=sys.stderr)
    cached = load_weather_cache()
    if cached:
        return cached
    return "天气获取失败，请稍后再试。"
def _parse_search_html(html, max_results=3):
    results = []
    patterns = [
        r'<a rel="nofollow" class="result__a" href=".*?">(.*?)</a>.*?<a class="result__snippet".*?>(.*?)</a>',
        r'class="result__a"[^>]*>(.*?)</a>.*?class="result__snippet"[^>]*>(.*?)</a>',
    ]
    for pattern in patterns:
        blocks = re.findall(pattern, html, re.S)
        for title, snippet in blocks[:max_results]:
            title = re.sub(r"<.*?>", "", title)
            snippet = re.sub(r"<.*?>", "", snippet)
            title = unescape(title).strip()
            snippet = unescape(snippet).strip()
            if title or snippet:
                results.append(f"{title}：{snippet}")
        if results:
            break
    return "\n".join(results[:max_results])


def _parse_search_text(text, max_results=3):
    lines = []
    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith(("Title:", "URL Source:", "Markdown Content:", "[Skip", "[Accessibility", "[News]", "About ", "Open links", "Date", "![Image")):
            continue
        if line.startswith("!") or "javascript:void" in line or "blob:http" in line or "Sponsored" in line or "About our ads" in line or "databricks.com" in line or "getsmarter.com" in line or "photos you provided" in line or "Privacy Policy" in line or "Terms of Use" in line or "Advanced search" in line or "Rewards" in line or "Can't use this link" in line or "Check that your link starts" in line or "Unable to process this search" in line or "try a different image or keyword" in line:
            continue
        line = re.sub(r"\s+", " ", line)
        if len(line) < 12:
            continue
        lines.append(line)
        if len(lines) >= max_results * 2:
            break
    return "\n".join(lines[:max_results])


def _curl_text(url, timeout=8):
    proc = subprocess.run(
        ["curl", "-k", "-L", "--http1.1", "-A", "Mozilla/5.0", "-m", str(timeout), "-s", url],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout + 2,
    )
    if proc.returncode == 0 and proc.stdout:
        return proc.stdout
    print(f"[WebSearch] curl failed rc={proc.returncode} err={proc.stderr[:120]}", file=sys.stderr)
    return ""


def normalize_search_query(query):
    if "AI" in query and any(kw in query for kw in ["新闻", "最新", "最近"]):
        return "AI news"
    if "MSI" in query.upper() and "冠军" in query:
        return "MSI champion 2025 League of Legends"
    return query

def web_news_search(query, max_results=3):
    search_query = normalize_search_query(query)
    encoded = quote_plus(search_query)
    urls = [
        "https://news.google.com/rss/search?q=" + encoded + "&hl=zh-CN&gl=CN&ceid=CN:zh-Hans",
        "https://news.google.com/rss/search?q=" + encoded + "&hl=en-US&gl=US&ceid=US:en",
    ]
    for url in urls:
        xml_text = _curl_text(url, timeout=10)
        if not xml_text or is_invalid_search_text(xml_text):
            continue
        try:
            root = ET.fromstring(xml_text)
            items = root.findall(".//item")
            results = []
            for item in items[:max_results]:
                title = item.findtext("title") or ""
                source = item.findtext("source") or ""
                title = re.sub(r"\s+", " ", unescape(title)).strip()
                source = re.sub(r"\s+", " ", unescape(source)).strip()
                if title:
                    if source and source not in title:
                        results.append(f"{title}，来源：{source}")
                    else:
                        results.append(title)
            if results:
                print(f"[WebSearch] google news rss ok query={search_query} results={len(results)}", file=sys.stderr)
                return "\n".join(results)
        except Exception as exc:
            print(f"[WebSearch] google news rss parse failed: {exc}", file=sys.stderr)
    return ""
def web_search(query, max_results=3):
    if any(kw in query for kw in ["新闻", "news", "News"]):
        news_result = web_news_search(query, max_results=max_results)
        if news_result:
            return news_result
    search_query = normalize_search_query(query)
    custom_url = os.environ.get("SEARCH_API_URL", "").strip()
    if custom_url:
        url = custom_url.replace("{query}", quote_plus(search_query))
        try:
            response = requests.get(url, timeout=8)
            response.raise_for_status()
            text = response.text.strip()
            try:
                data = response.json()
                text = json.dumps(data, ensure_ascii=False)[:1200]
            except Exception:
                text = text[:1200]
            if is_invalid_search_text(text):
                print(f"[WebSearch] custom api invalid response query={search_query}: {text[:120]}", file=sys.stderr)
            else:
                print(f"[WebSearch] custom api ok query={search_query}", file=sys.stderr)
                return text
        except Exception as exc:
            print(f"[WebSearch] custom api failed: {exc}", file=sys.stderr)

    encoded = quote_plus(search_query)
    jina_urls = [
        "https://r.jina.ai/http://r.jina.ai/http://https://www.bing.com/search?q=" + encoded,
        "https://r.jina.ai/http://r.jina.ai/http://https://news.google.com/search?q=" + encoded,
    ]
    for url in jina_urls:
        text = _curl_text(url, timeout=10)
        if text and not is_invalid_search_text(text):
            parsed = _parse_search_text(text, max_results=max_results)
            if parsed and not is_invalid_search_text(parsed):
                print(f"[WebSearch] jina ok query={search_query} results={len(parsed.splitlines())}", file=sys.stderr)
                return parsed
            print(f"[WebSearch] jina parsed invalid/empty query={search_query}", file=sys.stderr)

    url = "https://duckduckgo.com/html/?q=" + encoded
    html = _curl_text(url, timeout=8)
    if html:
        parsed = _parse_search_html(html, max_results=max_results)
        if parsed:
            print(f"[WebSearch] duckduckgo ok query={search_query} results={len(parsed.splitlines())}", file=sys.stderr)
            return parsed

    headers = {"User-Agent": "Mozilla/5.0"}
    try:
        response = requests.get(url, headers=headers, timeout=6, verify=False)
        response.raise_for_status()
        parsed = _parse_search_html(response.text, max_results=max_results)
        if parsed:
            print(f"[WebSearch] requests ok query={search_query} results={len(parsed.splitlines())}", file=sys.stderr)
            return parsed
    except Exception as exc:
        print(f"[WebSearch] requests failed: {exc}", file=sys.stderr)
    return ""
def summarize_search_context(search_context):
    for raw in search_context.splitlines():
        line = raw.strip()
        if not line:
            continue
        line = re.sub(r"\[[^\]]+\]\([^\)]+\)", "", line)
        line = re.sub(r"https?://\S+", "", line)
        line = re.sub(r"\s+", " ", line).strip(" -：:")
        if len(line) >= 8:
            if len(line) > 70:
                line = line[:70] + "。"
            return "搜索结果：" + line
    return "已联网搜索，但结果摘要为空。"
def load_history():
    if not os.path.exists(HISTORY_FILE):
        return []
    try:
        with open(HISTORY_FILE, "r", encoding="utf-8") as f:
            data = json.load(f)
            last_time = data.get("timestamp", 0)
            if time.time() - last_time > SESSION_TIMEOUT:
                return []
            return data.get("messages", [])
    except Exception:
        return []

def save_history(messages):
    try:
        with open(HISTORY_FILE, "w", encoding="utf-8") as f:
            json.dump({
                "timestamp": time.time(),
                "messages": messages
            }, f, ensure_ascii=False, indent=2)
    except Exception:
        pass

def chat_deepseek(prompt):
    clear_keywords = ["清空对话", "清除记忆", "重新开始", "清除对话", "清空记忆"]
    if any(kw in prompt for kw in clear_keywords):
        if os.path.exists(HISTORY_FILE):
            try:
                os.remove(HISTORY_FILE)
            except:
                pass
        return "好的，已为您清空对话记忆。"

    if any(kw in prompt for kw in ["联网了没有", "能联网", "可以联网", "能不能联网", "会上网"]):
        return "我可以通过WSL联网搜索信息。"

    now, current_date = current_datetime_text()
    if any(kw in prompt for kw in ["几月几日", "日期", "今天几号", "今天是几号"]):
        return f"今天是{current_date}。"
    if "星期" in prompt:
        return current_date.split("，", 1)[1] + "。"
    if any(kw in prompt for kw in ["几点", "时间", "现在几点"]):
        return f"现在是{now.hour}点{now.minute:02d}分。"

    if "天气" in prompt:
        return get_weather_answer()

    search_context = ""
    if should_search_web(prompt):
        search_context = web_search(prompt)

    if should_search_web(prompt) and not search_context:
        return "搜索服务暂时不可用，请稍后再试。"
    history_messages = load_history()
    
    # 限制历史记录轮数，保持最近的 N 轮
    if len(history_messages) > MAX_HISTORY_TURNS * 2:
        history_messages = history_messages[-(MAX_HISTORY_TURNS * 2):]

    url = f"{base_url.rstrip('/')}/chat/completions"
    headers = {
        "Authorization": f"Bearer {api_key}",
        "Content-Type": "application/json"
    }
    
    if any(kw in prompt for kw in ["联网了没有", "能联网", "可以联网", "能不能联网", "会上网"]):
        return "我可以通过WSL联网搜索信息。"

    now, current_date = current_datetime_text()

    system_message = {
        "role": "system",
        "content": f"你是智能语音助手。当前日期是{current_date}。回答要简短（50字以内），格式为指令或简单回答。回答日期、星期、时间时必须使用当前日期，不要猜测。你可以根据搜索结果回答实时信息，也可以回答天气、开关灯、家电控制等问题。例如：当前温度25度、打开LED灯、今天是{current_date}。"
    }
    
    user_content = prompt
    if search_context:
        user_content = f"用户问题：{prompt}\n\n以下是联网搜索结果，请只基于这些结果和常识简短回答：\n{search_context}"

    messages = [system_message] + history_messages + [{"role": "user", "content": user_content}]

    data = {
        "model": model,
        "messages": messages,
        "stream": False,
        "max_tokens": 120
    }

    last_error = None
    for attempt in range(1, 4):
        try:
            response = requests.post(url, headers=headers, json=data, timeout=15)
            response.raise_for_status()
            result = response.json()
            answer = result["choices"][0]["message"]["content"].strip()
            if search_context and any(kw in answer for kw in ["未联网", "无法联网", "无法搜索", "没有联网", "未获得有效搜索结果", "需要联网搜索", "搜索结果不完整", "未获取到搜索结果", "未获取到联网搜索结果", "无法提供具体", "结果不相关", "抱歉"]):
                answer = summarize_search_context(search_context)
            
            # 保存新的对话内容到历史中
            history_messages.append({"role": "user", "content": prompt})
            history_messages.append({"role": "assistant", "content": answer})
            save_history(history_messages)
            
            return answer
        except Exception as e:
            last_error = e
            print(f"[DeepSeek] attempt {attempt}/3 failed: {e}", file=sys.stderr)
            if attempt < 3:
                time.sleep(1.0)

    print(f"[DeepSeek] all attempts failed: {last_error}", file=sys.stderr)
    return "网络有点不稳定，请再试一次。"


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 chat_deepseek.py <question>")
        sys.exit(1)
    
    question = sys.argv[1]
    answer = chat_deepseek(question)
    print(answer)














