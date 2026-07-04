#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import json
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

PROJECT_DIR = "/home/cyj/workspace/ai_assistant"
CACHE_JSON_PATH = f"{PROJECT_DIR}/config/weather_cache.json"
CACHE_TEXT_PATH = f"{PROJECT_DIR}/weather_cache.txt"
CITY_CONFIG_PATH = f"{PROJECT_DIR}/config/weather_cities.json"

DEFAULT_CITIES = [
    {"key": "Wuhan", "name": "武汉", "latitude": 30.5928, "longitude": 114.3055},
    {"key": "Beijing", "name": "北京", "latitude": 39.9042, "longitude": 116.4074},
]

WMO_WEATHER_CODES = {
    0: "晴",
    1: "大部晴朗",
    2: "多云",
    3: "阴",
    45: "雾",
    48: "冻雾",
    51: "小毛雨",
    53: "毛雨",
    55: "大毛雨",
    56: "冻毛雨",
    57: "强冻毛雨",
    61: "小雨",
    63: "中雨",
    65: "大雨",
    66: "冻雨",
    67: "强冻雨",
    71: "小雪",
    73: "中雪",
    75: "大雪",
    77: "雪粒",
    80: "小阵雨",
    81: "阵雨",
    82: "强阵雨",
    85: "小阵雪",
    86: "强阵雪",
    95: "雷阵雨",
    96: "雷阵雨伴小冰雹",
    99: "雷阵雨伴大冰雹",
}
TRANSLATIONS = {
    "Partly cloudy": "多云",
    "Partly Cloudy": "多云",
    "Sunny": "晴",
    "Clear": "晴",
    "Cloudy": "阴",
    "Overcast": "阴",
    "Rainy": "雨",
    "Rain With Thunderstorm": "雷阵雨",
    "Rain with thunderstorm": "雷阵雨",
    "Thunderstorm with rain": "雷阵雨",
    "Patchy light rain with thunder": "局部雷阵雨",
    "Moderate or heavy rain with thunder": "强雷阵雨",
    "Light Rain": "小雨",
    "Light rain": "小雨",
    "Light Rain Shower": "小阵雨",
    "Light rain shower": "小阵雨",
    "Light rain showers": "小阵雨",
    "Moderate rain": "中雨",
    "Heavy rain": "大雨",
    "Patchy rain possible": "局部有雨",
    "Patchy rain nearby": "附近有零星小雨",
    "Smoky haze": "霾",
    "Mist": "雾",
    "Fog": "雾",
    "Snow": "雪",
    "Thunderstorm": "雷阵雨",
    "Light drizzle": "小毛雨",
    "Drizzle": "毛雨",
    "Heavy drizzle": "大毛雨",
    "Light shower": "阵雨",
    "Light showers": "阵雨",
    "Shower": "阵雨",
    "Showers": "阵雨",
    "Heavy shower": "强阵雨",
    "Heavy showers": "强阵雨",
    "Torrential rain": "暴雨",
    "Very heavy rain": "大暴雨",
    "Violent rain": "特大暴雨",
    "Thundery outbreaks possible": "可能雷阵雨",
    "Thunder": "雷",
    "Light snow": "小雪",
    "Moderate snow": "中雪",
    "Heavy snow": "大雪",
    "Blizzard": "暴风雪",
    "Sleet": "雨夹雪",
    "Foggy": "雾",
    "Freezing fog": "冻雾",
    "Haze": "霾",
    "Hazy": "霾",
    "Smoke": "烟",
    "Hail": "冰雹",
    "Windy": "有风",
    "Breezy": "微风",
    "Strong wind": "大风",
    "Fair": "晴朗",
    "Clear sky": "晴空",
    "Unknown": "未知",
}


def translate_desc(desc):
    return TRANSLATIONS.get(desc, desc or "未知")

def translate_weather_text(text):
    if not text:
        return "未知"
    result = str(text)
    for en in sorted(TRANSLATIONS, key=len, reverse=True):
        result = result.replace(en, TRANSLATIONS[en])
    return result


def load_weather_cities():
    try:
        if os.path.exists(CITY_CONFIG_PATH):
            with open(CITY_CONFIG_PATH, "r", encoding="utf-8-sig") as fp:
                data = json.load(fp)
            cities = data.get("cities") or []
            valid = []
            for item in cities[:2]:
                key = item.get("key") or item.get("name")
                name = item.get("name") or key
                lat = item.get("latitude")
                lon = item.get("longitude")
                if key and name and lat is not None and lon is not None:
                    valid.append({"key": str(key), "name": str(name), "latitude": float(lat), "longitude": float(lon)})
            if len(valid) >= 2:
                return valid[:2]
    except Exception as exc:
        print(f"load weather city config failed: {exc}", file=sys.stderr)
    return DEFAULT_CITIES


def city_lookup(cities, key):
    for city in cities:
        if city["key"] == key:
            return city
    return None
def fetch_weather_open_meteo(city):
    lat = city["latitude"]
    lon = city["longitude"]
    try:
        url = (
            "https://api.open-meteo.com/v1/forecast"
            f"?latitude={lat}&longitude={lon}"
            "&current=temperature_2m,weather_code"
            "&timezone=Asia%2FShanghai"
        )
        res = subprocess.run(["curl", "-k", "--http1.1", "-s", "--connect-timeout", "1", "-m", "2", url], capture_output=True, text=True)
        if res.returncode == 0 and res.stdout.strip():
            data = json.loads(res.stdout)
            current = data.get("current") or {}
            temp_c = current.get("temperature_2m")
            code = current.get("weather_code")
            if temp_c is not None and code is not None:
                desc = WMO_WEATHER_CODES.get(int(code), "未知")
                return f"{round(float(temp_c))}°C {desc}"
    except Exception as exc:
        print(f"open-meteo fetch failed for {city['key']}: {exc}", file=sys.stderr)
    return None

def fetch_weather_wttr(city):
    try:
        url = f"https://wttr.in/{city['key']}?format=j1"
        res = subprocess.run(["curl", "-k", "--http1.1", "-s", "--connect-timeout", "1", "-m", "2", url], capture_output=True, text=True)
        if res.returncode == 0 and res.stdout.strip():
            data = json.loads(res.stdout)
            current = (data.get("current_condition") or [{}])[0]
            temp_c = current.get("temp_C") or ""
            desc_list = current.get("weatherDesc") or []
            desc = desc_list[0].get("value", "") if desc_list else ""
            if temp_c:
                return f"{temp_c}°C {translate_desc(desc)}"
    except Exception:
        pass
    return None


def fetch_weather(city):
    # Open-Meteo 是免 key 天气源，返回结构稳定，速度比 wttr 更可控。
    return fetch_weather_open_meteo(city)

def load_cache():
    if not os.path.exists(CACHE_JSON_PATH):
        return None
    try:
        with open(CACHE_JSON_PATH, "r", encoding="utf-8-sig") as fp:
            data = json.load(fp)
        cities = data.get("cities") or []
        if len(cities) >= 2:
            return {
                "cities": cities[:2],
                "update_time": data.get("update_time", "--"),
            }
    except Exception:
        pass
    return None


def save_cache(cities, update_time=None):
    payload = {
        "cities": cities[:2],
        "update_time": update_time or time.strftime("%Y-%m-%d %H:%M:%S", time.localtime()),
    }
    os.makedirs(os.path.dirname(CACHE_JSON_PATH), exist_ok=True)
    with open(CACHE_JSON_PATH, "w", encoding="utf-8") as fp:
        json.dump(payload, fp, ensure_ascii=False, indent=2)
        fp.write("\n")
    with open(CACHE_TEXT_PATH, "w", encoding="utf-8") as fp:
        first, second = payload["cities"]
        fp.write(f"{first['name']}:{first['weather']}|{second['name']}:{second['weather']}")
    return payload

def main():
    cities = load_weather_cities()
    old_cache = load_cache()
    old_by_key = {}
    if old_cache:
        for item in old_cache.get("cities", []):
            old_by_key[item.get("key") or item.get("name")] = item.get("weather")
            old_by_key[item.get("name")] = item.get("weather")

    with ThreadPoolExecutor(max_workers=2) as executor:
        futures = [executor.submit(fetch_weather, city) for city in cities]
        weather_values = []
        for city, future in zip(cities, futures):
            try:
                value = future.result(timeout=3)
            except Exception as exc:
                print(f"weather future failed for {city['key']}: {exc}", file=sys.stderr)
                value = None
            if not value:
                value = old_by_key.get(city["key"]) or old_by_key.get(city["name"])
            weather_values.append(value)

    if any(not value for value in weather_values):
        print("weather fetch failed and cache unavailable", file=sys.stderr)
        raise SystemExit(1)

    result_cities = []
    for city, weather in zip(cities, weather_values):
        result_cities.append({
            "key": city["key"],
            "name": city["name"],
            "weather": translate_weather_text(weather),
        })

    payload = save_cache(result_cities)
    first, second = payload["cities"]
    print(f"{first['name']}:{first['weather']}|{second['name']}:{second['weather']}")

if __name__ == "__main__":
    main()











