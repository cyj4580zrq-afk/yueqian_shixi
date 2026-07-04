import sys
import os
import requests
import json

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

def chat_deepseek(prompt):
    url = f"{base_url.rstrip('/')}/chat/completions"
    headers = {
        "Authorization": f"Bearer {api_key}",
        "Content-Type": "application/json"
    }
    data = {
        "model": model,
        "messages": [
            {
                "role": "system",
                "content": "你是智能语音助手。现在是2026年06月27日 星期六 09:56。回答要简短（20字以内），格式为指令或简单回答。可以回答日期、天气、开关灯、家电控制等问题。例如：今天是2026年06月27日、打开LED灯、当前温度25度"
            },
            {
                "role": "user",
                "content": prompt
            }
        ],
        "stream": False,
        "max_tokens": 120
    }

    try:
        response = requests.post(url, headers=headers, json=data, timeout=15)
        response.raise_for_status()
        result = response.json()
        return result["choices"][0]["message"]["content"].strip()
    except Exception as e:
        return f"Error: {str(e)}"

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 chat_deepseek.py <question>")
        sys.exit(1)
    
    question = sys.argv[1]
    answer = chat_deepseek(question)
    print(answer)
