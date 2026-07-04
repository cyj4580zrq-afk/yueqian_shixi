import sys
import os
from zhipuai import ZhipuAI

# Default API key from the course document, can be overridden by ZHIPU_API_KEY env var
DEFAULT_API_KEY = ""
api_key = os.environ.get("ZHIPU_API_KEY", DEFAULT_API_KEY)

def chat(prompt):
    client = ZhipuAI(api_key=api_key)
    try:
        response = client.chat.completions.create(
            model="glm-4-flash",  # Using the latest free-tier model
            messages=[
                {"role": "system", "content": "You are a smart home assistant. Keep your responses extremely short (under 10 words) and format them as commands or simple answers. Example: '打开LED灯', '当前温度是25度'"},
                {"role": "user", "content": prompt},
            ],
            max_tokens=60,
        )
        return response.choices[0].message.content
    except Exception as e:
        return f"Error: {str(e)}"

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 chat.py <question>")
        sys.exit(1)
    
    question = sys.argv[1]
    answer = chat(question)
    print(answer)
