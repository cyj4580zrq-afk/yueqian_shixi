# 曜灵 AI 项目完整说明

## 1. 项目简介

**曜灵 AI** 是一个运行在 RK1808 开发板上的嵌入式 Linux AI 助手项目。

项目整体采用：

```text
RK1808 开发板 UI + WSL AI 服务端 + 科大讯飞 ASR/TTS + DeepSeek + TCP 通信
```

系统目标是让开发板具备：

```text
语音交互
天气查询
相册浏览
文件浏览
WiFi 扫描连接
触摸屏 UI
```

工程路径：

```bash
/home/cyj/workspace/ai_assistant
```

---

## 2. 整体架构

```text
RK1808 开发板
  |
  | app_ui
  | 负责 UI、触摸、录音、播放、显示
  |
  | TCP 上传录音
  v
WSL Ubuntu
  |
  | app_server
  | 接收录音、触发 AI pipeline、提供天气 TCP 服务
  |
  v
scripts/ai_pipeline.py
  |
  | ASR -> DeepSeek -> TTS
  |
  v
回传文字和 reply.wav 到 RK1808
```

---

## 3. 程序分别运行在哪里

### 3.1 RK1808 开发板运行

```bash
/root/code/app_ui
```

开发板负责：

```text
显示 UI
处理触摸
录音
播放 TTS 音频
接收 WSL 回传文本
接收 WSL 回传音频
WiFi 扫描连接
文件浏览
相册显示
天气页面显示
```

### 3.2 WSL Ubuntu 运行

```bash
/home/cyj/workspace/ai_assistant/app_server
```

WSL 负责：

```text
接收开发板上传的 cmd.wav
启动 AI pipeline
执行 ASR
请求 DeepSeek
执行 TTS
回传 AI 文本
回传 reply.wav
提供天气 TCP 缓存服务
```

---

## 4. 主要目录说明

### 4.1 src/application

```text
板端主程序和 UI 页面入口
```

### 4.2 src/application/pages

```text
六个 UI 页面：
首页
语音助手
天气
相册
文件浏览
设置/WiFi
```

### 4.3 src/hal

```text
显示 HAL、触摸 HAL
当前 UI 风格主要在 display_hal.c
```

### 4.4 src/driver

```text
DRM 显示驱动
触摸输入驱动
字体绘制驱动
```

### 4.5 src/protocol

```text
TCP 控制协议
TCP 音频协议
天气客户端
WiFi 管理
```

### 4.6 src/utils

```text
录音模块
文件工具模块
```

### 4.7 src/app_server

```text
WSL 端 app_server
监听开发板上传的 wav
启动 AI pipeline
提供天气 TCP 服务
```

### 4.8 scripts

```text
AI pipeline
天气获取脚本
辅助脚本
```

### 4.9 bin

```text
科大讯飞 ASR/TTS 可执行程序
DeepSeek 脚本
wav 文件目录
```

### 4.10 config

```text
主配置
天气城市配置
天气缓存
WiFi 密码文件
```

---

## 5. 核心源码文件说明

### 5.1 src/application/main.c

板端 app_ui 入口。

主要职责：

```text
初始化配置
初始化 DRM 显示
初始化字体
初始化触摸
注册六个页面
启动 8888 控制监听
启动 8889 音频监听
启动网络监控线程
启动天气刷新
进入 UI 主循环
```

### 5.2 src/app_server/server.c

WSL 服务端。

主要职责：

```text
监听 8888 legacy trigger
监听 8890 wav 上传
监听 8891 天气请求
收到 cmd.wav 后启动 ai_pipeline.py
天气请求优先返回缓存，并后台刷新天气
```

### 5.3 scripts/ai_pipeline.py

WSL AI 流水线。

当前结构：

```text
audio_queue -> ASR Worker
text_queue -> DeepSeek Worker
reply_queue -> TTS Worker
audio_out_queue -> Dispatch Worker
```

### 5.4 bin/chat_deepseek.py

DeepSeek 请求与联网搜索逻辑。

主要职责：

```text
DeepSeek 对话
联网搜索
天气缓存优先回复
错误重试
```

### 5.5 src/utils/audio_recorder.c

板端录音模块。

主要职责：

```text
独立录音线程
独立 TCP 上传线程
AI 状态机
防重复点击
防重复录音
```

### 5.6 src/application/pages/page_ai.c

AI 页面。

主要职责：

```text
显示录音状态
显示 ASR 文本
显示 AI 回复
按钮触发录音
```

### 5.7 src/application/pages/page_weather.c

天气页面。

主要职责：

```text
显示两个城市天气
5 秒内不重复请求
显示 WSL 返回的真实更新时间
```

### 5.8 src/application/pages/page_album.c

相册页面。

主要职责：

```text
显示图片
上一张
下一张
放大
缩小
```

### 5.9 src/application/pages/page_file.c

文件浏览页面。

主要职责：

```text
从 / 根目录浏览
进入目录
返回上级
上一页/下一页
上移/下移选中项
```

### 5.10 src/application/pages/page_settings.c

设置/WiFi 页面。

主要职责：

```text
扫描 WiFi
WiFi 列表分页
点击 WiFi 弹出键盘
输入密码
显示/隐藏密码
连接 WiFi
```

### 5.11 src/hal/display_hal.c

全局 UI 绘制。

主要职责：

```text
背景
顶部栏
左侧菜单
当前页面高亮
按钮
卡片
文字
BMP 图片绘制
```

当前标题：

```text
曜灵 AI
```

---

## 6. 配置文件说明

主配置文件：

```bash
/home/cyj/workspace/ai_assistant/config/app_config.json
```

当前关键配置：

```json
{
  "wsl_server_ip": "10.203.129.104",
  "wsl_username": "cyj",
  "board_ip": "10.203.129.238",
  "touch_device": "/dev/input/event2",
  "drm_device": "/dev/dri/card0",
  "image_dir": "/root/picture",
  "file_browser_root": "/",
  "local_voice_cmd_wav": "/tmp/rec.wav",
  "local_voice_reply_wav": "/tmp/reply.wav",
  "wsl_voice_cmd_wav": "/home/cyj/workspace/ai_assistant/bin/wav/cmd.wav",
  "wsl_ai_pipeline": "/home/cyj/workspace/ai_assistant/scripts/ai_pipeline.py",
  "wsl_get_weather": "/home/cyj/workspace/ai_assistant/scripts/get_weather.py",
  "wifi_password_file": "config/wifi_passwords.conf",
  "font_path": "assets/simsun.ttc"
}
```

开发板运行时读取：

```bash
/root/code/config/app_config.json
```

---

## 7. IP 配置说明

### 7.1 修改 WSL IP

修改：

```json
"wsl_server_ip": "新的 WSL IP"
```

例如：

```json
"wsl_server_ip": "10.203.129.104"
```

### 7.2 修改开发板 IP

修改：

```json
"board_ip": "新的开发板 IP"
```

例如：

```json
"board_ip": "10.203.129.238"
```

### 7.3 同步配置到开发板

```bash
scp config/app_config.json root@开发板IP:/root/code/config/
```

---

## 8. 端口说明

```text
WSL 端口 8888：
legacy AI trigger 服务

WSL 端口 8890：
接收 RK1808 上传的 cmd.wav

WSL 端口 8891：
天气 TCP 缓存服务

RK1808 端口 8888：
接收 WSL 回传的 ASR/AI 文本

RK1808 端口 8889：
接收 WSL 回传的 reply.wav
```

注意：

```text
WSL 和开发板都可能使用 8888。
它们在不同设备上，不冲突。
```

---

## 9. 从 0 开始编译

进入工程：

```bash
cd /home/cyj/workspace/ai_assistant
```

### 9.1 编译板端 UI

```bash
make board
```

生成：

```bash
/home/cyj/workspace/ai_assistant/app_ui
```

### 9.2 编译 WSL 服务端

```bash
make host
```

生成：

```bash
/home/cyj/workspace/ai_assistant/app_server
```

### 9.3 编译 ASR/TTS 工具

```bash
make ai_tools
```

生成：

```bash
/home/cyj/workspace/ai_assistant/bin/iat_online_sample
/home/cyj/workspace/ai_assistant/bin/tts_online_sample
```

### 9.4 全部编译

```bash
make all
```

---

## 10. 部署到 RK1808 开发板

复制主程序：

```bash
cd /home/cyj/workspace/ai_assistant
scp app_ui root@10.203.129.238:/root/code/
```

复制配置：

```bash
scp config/app_config.json root@10.203.129.238:/root/code/config/
```

复制资源：

```bash
scp -r assets root@10.203.129.238:/root/code/
scp -r libs root@10.203.129.238:/root/code/
```

如果开发板 IP 是 `.239`：

```bash
scp app_ui root@10.203.129.239:/root/code/
scp config/app_config.json root@10.203.129.239:/root/code/config/
scp -r assets root@10.203.129.239:/root/code/
scp -r libs root@10.203.129.239:/root/code/
```

---

## 11. 运行顺序

### 11.1 先启动 WSL 服务端

```bash
cd /home/cyj/workspace/ai_assistant
./app_server
```

正常输出：

```text
WSL WAV Upload Server running on port 8890...
WSL Weather TCP Server running on port 8891...
WSL AI Server running on port 8888...
```

### 11.2 再启动开发板 UI

```bash
cd /root/code
./app_ui
```

正常输出：

```text
Starting AI Assistant Board GUI...
[Config] Loaded successfully from /root/code/config/app_config.json.
[DRM Driver] Initialized successfully.
Font loaded: assets/simsun.ttc
UI Client Started. Event Loop running...
```

---

## 12. AI 语音交互完整流程

```text
1. 用户点击 AI 页面“开始录音”

2. page_ai.c 调用 audio_recorder_start_record()

3. audio_recorder.c 创建录音线程

4. 录音线程执行 arecord

5. 开发板生成：
   /tmp/rec.wav

6. 发送线程通过 TCP 上传到 WSL：
   WSL_IP:8890

7. WSL app_server 保存为：
   /home/cyj/workspace/ai_assistant/bin/wav/cmd.wav

8. app_server 启动：
   scripts/ai_pipeline.py

9. ai_pipeline.py 把任务放入 audio_queue

10. ASR Worker 调用：
    bin/iat_online_sample

11. ASR 文本通过 TCP 回传开发板：
    board_ip:8888
    格式：
    ASR:你好

12. AI Worker 调用：
    bin/chat_deepseek.py

13. DeepSeek 回复通过 TCP 回传开发板：
    board_ip:8888
    格式：
    AI:你好，我是曜灵 AI

14. TTS Worker 调用：
    bin/tts_online_sample

15. WSL 生成：
    /home/cyj/workspace/ai_assistant/bin/wav/reply.wav

16. Dispatch Worker 通过 TCP 回传音频：
    board_ip:8889

17. 开发板保存：
    /tmp/reply.wav

18. 开发板播放 reply.wav

19. AI 页面状态进入 DONE
```

---

## 13. AI 状态机

当前状态：

```text
IDLE
RECORDING
SENDING
WAITING_RESPONSE
PLAYING
DONE
ERROR
```

作用：

```text
防止重复点击
防止多个录音线程
防止多个发送线程
失败后允许重新开始
```

---

## 14. 天气模块流程

天气页面文件：

```bash
src/application/pages/page_weather.c
```

开发板请求：

```text
weather_client_fetch()
```

优先 TCP：

```text
RK1808 -> WSL:8891
```

WSL 返回缓存格式：

```text
武汉:25°C 小雨|广州:28°C 小阵雨|更新时间:2026-07-04 10:08:45
```

WSL 后台刷新：

```bash
python3 scripts/get_weather.py
```

天气城市配置：

```bash
/home/cyj/workspace/ai_assistant/config/weather_cities.json
```

示例：

```json
{
  "cities": [
    { "key": "Wuhan", "name": "武汉", "latitude": 30.5928, "longitude": 114.3055 },
    { "key": "Guangzhou", "name": "广州", "latitude": 23.1291, "longitude": 113.2644 }
  ]
}
```

天气刷新策略：

```text
5 秒内不重复请求
优先显示缓存
后台刷新天气
失败不阻塞 UI
显示 WSL 获取天气时的真实更新时间
```

---

## 15. 相册模块流程

页面文件：

```bash
src/application/pages/page_album.c
```

图片目录配置：

```json
"image_dir": "/root/picture"
```

开发板图片目录：

```bash
/root/picture
```

支持功能：

```text
显示 BMP 图片
上一张
下一张
放大
缩小
```

部署图片：

```bash
scp your.bmp root@开发板IP:/root/picture/
```

---

## 16. 文件浏览模块流程

页面文件：

```bash
src/application/pages/page_file.c
```

根目录配置：

```json
"file_browser_root": "/"
```

当前功能：

```text
从 / 根目录开始浏览
显示目录
显示文件
进入目录
返回上级
上一页
下一页
上移
下移
```

如果只想浏览 `/root/code`：

```json
"file_browser_root": "/root/code"
```

---

## 17. WiFi 模块流程

页面文件：

```bash
src/application/pages/page_settings.c
```

WiFi 管理文件：

```bash
src/protocol/wifi_manager.c
```

当前功能：

```text
扫描 WiFi
最多显示 7 个
上一页/下一页翻页
点击 WiFi 后弹出键盘
输入密码
显示/隐藏密码
删除/清空/取消/连接
连接后刷新 SSID 和 IP
```

扫描命令主要使用：

```bash
wpa_cli -p /var/run/wpa_supplicant -i wlan0 scan
wpa_cli -p /var/run/wpa_supplicant -i wlan0 scan_results
```

连接命令主要使用：

```bash
wpa_cli add_network
wpa_cli set_network ssid
wpa_cli set_network psk
wpa_cli select_network
wpa_cli enable_network
udhcpc -i wlan0
```

---

## 18. UI 模块

当前项目标题：

```text
曜灵 AI
```

全局 UI 绘制文件：

```bash
src/hal/display_hal.c
```

主要负责：

```text
背景
顶部栏
左侧菜单
当前页面高亮
按钮
卡片
文字
BMP 图片绘制
```

当前 UI 风格：

```text
克制深色玻璃风格
```

---

## 19. 启动前检查清单

### 19.1 WSL 检查

```bash
cd /home/cyj/workspace/ai_assistant
ls app_server
ls bin/iat_online_sample
ls bin/tts_online_sample
ls scripts/ai_pipeline.py
ls bin/chat_deepseek.py
```

### 19.2 开发板检查

```bash
cd /root/code
ls app_ui
ls config/app_config.json
ls assets/simsun.ttc
ls libs
```

### 19.3 网络检查

WSL 查看 IP：

```bash
ip addr
```

开发板查看 IP：

```bash
ifconfig
```

互相 ping：

```bash
ping 10.203.129.104
ping 10.203.129.238
```

---

## 20. 常用测试命令

### 20.1 WSL 单独测试天气

```bash
cd /home/cyj/workspace/ai_assistant
python3 scripts/get_weather.py
cat weather_cache.txt
```

### 20.2 WSL 单独测试 DeepSeek

```bash
cd /home/cyj/workspace/ai_assistant
python3 bin/chat_deepseek.py "你好"
```

### 20.3 查看 AI pipeline 日志

```bash
cat /tmp/ai_pipeline.log
```

### 20.4 查看天气刷新日志

```bash
cat /tmp/weather_refresh.log
```

---

## 21. 推荐完整启动流程

### 第一步：确认 WSL IP

```bash
ip addr
```

### 第二步：修改配置

```bash
vim /home/cyj/workspace/ai_assistant/config/app_config.json
```

### 第三步：编译

```bash
cd /home/cyj/workspace/ai_assistant
make host
make board
make ai_tools
```

### 第四步：部署到开发板

```bash
scp app_ui root@开发板IP:/root/code/
scp config/app_config.json root@开发板IP:/root/code/config/
```

### 第五步：WSL 启动服务

```bash
cd /home/cyj/workspace/ai_assistant
./app_server
```

### 第六步：开发板启动 UI

```bash
cd /root/code
./app_ui
```

### 第七步：屏幕测试

```text
首页
语音助手
天气
相册
文件浏览
设置/WiFi
```

---

## 22. 当前项目完成状态

已完成：

```text
AI 页面异步录音
AI 页面 TCP 上传 cmd.wav
WSL Worker + Queue AI Pipeline
ASR/DeepSeek/TTS 串联
AI 文本回传
AI 音频回传
AI 状态机
天气 TCP 缓存服务
天气 5 秒刷新节流
天气中文显示
天气真实更新时间
相册放大/缩小
相册上一张/下一张
文件浏览根目录 /
文件浏览分页/上下移动
WiFi 扫描分页
WiFi 密码软键盘
WiFi 显示/隐藏密码
全局 UI 美化
项目名称改为曜灵 AI
```

---

## 23. 重要配置对照表

| 功能 | 文件 | 修改项 |
|---|---|---|
| WSL IP | `/home/cyj/workspace/ai_assistant/config/app_config.json` | `wsl_server_ip` |
| 开发板 IP | `/home/cyj/workspace/ai_assistant/config/app_config.json` | `board_ip` |
| 图片目录 | `/home/cyj/workspace/ai_assistant/config/app_config.json` | `image_dir` |
| 文件浏览目录 | `/home/cyj/workspace/ai_assistant/config/app_config.json` | `file_browser_root` |
| 天气城市 | `/home/cyj/workspace/ai_assistant/config/weather_cities.json` | `cities` |
| 字体路径 | `/home/cyj/workspace/ai_assistant/config/app_config.json` | `font_path` |
| 录音文件 | `/home/cyj/workspace/ai_assistant/config/app_config.json` | `local_voice_cmd_wav` |
| WSL cmd.wav | `/home/cyj/workspace/ai_assistant/config/app_config.json` | `wsl_voice_cmd_wav` |

---

## 24. 一句话总结

曜灵 AI 是一个基于 RK1808 开发板的嵌入式 AI 语音终端，板端负责 UI、触摸、录音、播放和设备交互，WSL 负责 ASR、DeepSeek、TTS、天气缓存和网络服务，双方通过 TCP 通信完成完整的智能助手闭环。
