import json
import subprocess
import os


def fetch_weather(city):
    try:
        url = f"wttr.in/{city}?format=j1"
        res = subprocess.run(["curl", "-s", "-m", "10", url], capture_output=True, text=True)
        if res.returncode == 0 and res.stdout.strip():
            data = json.loads(res.stdout)
            current = (data.get("current_condition") or [{}])[0]
            temp_c = current.get("temp_C")
            desc_list = current.get("weatherDesc") or []
            cond = desc_list[0].get("value", "") if desc_list else ""
            translations = {
                "Partly cloudy": "??",
                "Partly Cloudy": "??",
                "Sunny": "?",
                "Clear": "?",
                "Cloudy": "?",
                "Overcast": "?",
                "Rainy": "?",
                "Light rain": "??",
                "Moderate rain": "??",
                "Heavy rain": "??",
                "Patchy rain possible": "????",
                "Smoky haze": "?",
                "Mist": "?",
                "Fog": "?",
                "Snow": "?",
                "Thunderstorm": "???",
                "Light drizzle": "??",
                "Drizzle": "??",
                "Heavy drizzle": "??",
                "Light shower": "???",
                "Light showers": "???",
                "Shower": "??",
                "Showers": "??",
                "Heavy shower": "???",
                "Heavy showers": "???",
                "Torrential rain": "??",
                "Very heavy rain": "??",
                "Violent rain": "??",
                "Thundery outbreaks possible": "???",
                "Thunder": "?",
                "Light snow": "??",
                "Moderate snow": "??",
                "Heavy snow": "??",
                "Blizzard": "??",
                "Sleet": "??",
                "Foggy": "?",
                "Freezing fog": "?",
                "Haze": "?",
                "Hazy": "?",
                "Smoke": "?",
                "Hail": "??",
                "Windy": "??",
                "Breezy": "?",
                "Strong wind": "??",
                "Fair": "?",
                "Clear sky": "?",
                "Unknown": "??",
            }
            cond_zh = translations.get(cond, cond)
            if temp_c not in (None, ""):
                return f"{temp_c}??? {cond_zh}"
    except Exception:
        pass
    return None

wuhan = fetch_weather("Wuhan")
beijing = fetch_weather("Beijing")
cache_path = "/home/cyj/workspace/ai_assistant/weather_cache.txt"
current_wuhan, current_beijing = "28??? ?", "32??? ??"

if os.path.exists(cache_path):
    try:
        with open(cache_path, "r", encoding="utf-8") as f:
            line = f.read().strip()
            parts = line.split("|")
            current_wuhan = parts[0].split(":", 1)[1]
            current_beijing = parts[1].split(":", 1)[1]
    except Exception:
        pass

if wuhan:
    current_wuhan = wuhan
if beijing:
    current_beijing = beijing

with open(cache_path, "w", encoding="utf-8") as f:
    f.write(f"??:{current_wuhan}|??:{current_beijing}")

print("Weather cache updated successfully.")
