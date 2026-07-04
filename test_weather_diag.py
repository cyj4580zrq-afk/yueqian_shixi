import subprocess

def get_weather(city):
    try:
        url = f"wttr.in/{city}?format=%t+%C"
        res = subprocess.run(["curl", "-s", "-m", "5", url], capture_output=True, text=True)
        if res.returncode != 0:
            return f"curl_err_{res.returncode}"
        val = res.stdout.strip()
        print(f"val for {city}: {repr(val)}")
        if not val:
            return "empty_val"
        if "<html" in val.lower():
            return "html_val"
        if len(val) > 40:
            return f"long_val_{len(val)}"
            
        translations = {
            "Partly cloudy": "多云",
            "Partly Cloudy": "多云",
            "Sunny": "晴",
            "Clear": "晴",
            "Cloudy": "阴",
            "Overcast": "阴",
            "Rainy": "雨",
            "Light rain": "小雨",
            "Moderate rain": "中雨",
            "Heavy rain": "大雨",
            "Patchy rain possible": "局部有雨",
            "Smoky haze": "霾",
            "Mist": "雾",
            "Fog": "雾",
            "Snow": "雪",
            "Thunderstorm": "雷阵雨"
        }
        parts = val.split(" ")
        temp = parts[0]
        cond = " ".join(parts[1:])
        cond_zh = translations.get(cond, cond)
        return f"{temp} {cond_zh}"
    except Exception as e:
        return f"exception_{str(e)}"

wuhan = get_weather("Wuhan")
beijing = get_weather("Beijing")
print(f"武汉:{wuhan}|北京:{beijing}")
