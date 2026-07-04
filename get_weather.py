import os
import subprocess

def main():
    cache_path = "/home/cyj/workspace/ai_assistant/weather_cache.txt"
    # First try to update it using update_weather.py
    try:
        subprocess.run(["python3", "/home/cyj/workspace/ai_assistant/update_weather.py"], timeout=10, capture_output=True)
    except Exception:
        pass

    # Read cache
    try:
        if os.path.exists(cache_path):
            with open(cache_path, "r", encoding="utf-8") as f:
                content = f.read().strip()
                # Clean up + and °C to avoid rendering issues on board
                content = content.replace("+", "").replace("°C", "度")
                print(content)
                return
    except Exception:
        pass

    # Fallback default values
    print("武汉:28度 多云|北京:32度 晴")

if __name__ == "__main__":
    main()
