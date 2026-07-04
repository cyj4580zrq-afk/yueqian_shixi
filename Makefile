# Makefile for RK1808 + WSL Distributed AI Assistant Project
# Senior Embedded Linux C++ Architect Standard

# 1. 选用 RK1808 SDK 官方专用的低版本交叉编译工具链
CC_BOARD = /opt/rk1808-sdk/buildroot/output/rockchip_rk1808/host/bin/aarch64-buildroot-linux-gnu-gcc
CC_HOST = gcc

# 2. 编译选项与头文件包含路径
# 显式链接 -ldrm 解决 libDRMwrap.so 在板端运行时 undefined symbol: drmModeGetResources 符号查询失败的问题
BOARD_CFLAGS = -O2 -Wall -I./include -I./3rdparty/stb -I./3rdparty/msc
BOARD_LDFLAGS = -L./libs/aarch64 -lDRMwrap -ldrm -lpthread -lm -Wl,-rpath,/root/code/libs

HOST_CFLAGS = -O2 -Wall -I./include -I./samples/iat_online_sample -I./samples/tts_online_sample
HOST_LDFLAGS = -L./libs/x86_64 -lmsc -lrt -ldl -lpthread -lstdc++

# 3. 源文件定义
BOARD_SRCS = \
    src/common/config.c \
    src/driver/display_drv.c \
    src/driver/touch_drv.c \
    src/driver/font_display_drv.c \
    src/hal/display_hal.c \
    src/hal/touch_hal.c \
    src/protocol/wifi_manager.c \
    src/protocol/weather_client.c \
    src/protocol/control_proto.c \
    src/protocol/audio_proto.c \
    src/utils/audio_recorder.c \
    src/utils/file_helper.c \
    src/application/pages/page_home.c \
    src/application/pages/page_ai.c \
    src/application/pages/page_weather.c \
    src/application/pages/page_album.c \
    src/application/pages/page_file.c \
    src/application/pages/page_settings.c \
    src/application/main.c

HOST_SRCS = \
    src/app_server/server.c

AI_ASR_SRCS = \
    samples/iat_online_sample/iat_online_sample.c

AI_TTS_SRCS = \
    samples/tts_online_sample/tts_online_sample.c

# 4. 构建目标
TARGET_BOARD = app_ui
TARGET_HOST = app_server
TARGET_AI_ASR = bin/iat_online_sample
TARGET_AI_TTS = bin/tts_online_sample

.PHONY: all board host ai_tools clean install

all: board host ai_tools

board: $(TARGET_BOARD)

host: $(TARGET_HOST)

ai_tools: $(TARGET_AI_ASR) $(TARGET_AI_TTS)

$(TARGET_BOARD): $(BOARD_SRCS)
	$(CC_BOARD) $(BOARD_CFLAGS) $^ -o $@ $(BOARD_LDFLAGS)
	@echo "RK1808 Board UI Client Compiled Successfully!"

$(TARGET_HOST): $(HOST_SRCS)
	$(CC_HOST) $(HOST_CFLAGS) $^ -o $@ $(HOST_LDFLAGS)
	@echo "PC WSL AI Server Compiled Successfully!"

$(TARGET_AI_ASR): $(AI_ASR_SRCS)
	mkdir -p bin
	$(CC_HOST) $(HOST_CFLAGS) $^ -o $@ $(HOST_LDFLAGS)
	@echo "WSL ASR CLI Compiled Successfully!"

$(TARGET_AI_TTS): $(AI_TTS_SRCS)
	mkdir -p bin
	$(CC_HOST) $(HOST_CFLAGS) $^ -o $@ $(HOST_LDFLAGS)
	@echo "WSL TTS CLI Compiled Successfully!"

install: board host ai_tools
	@echo "Creating deployment package directory..."
	mkdir -p package/libs
	mkdir -p package/config
	mkdir -p package/assets
	mkdir -p package/scripts
	cp $(TARGET_BOARD) package/
	cp libs/aarch64/libDRMwrap.so package/libs/
	cp config/app_config.json package/config/
	cp config/weather_cache.json package/config/ 2>/dev/null || true
	cp config/weather_cache.txt package/config/ 2>/dev/null || true
	cp config/wifi_passwords.conf package/config/ 2>/dev/null || true
	cp -r assets/* package/assets/ 2>/dev/null || true
	cp scripts/*.py package/scripts/ 2>/dev/null || true
	@echo "Package directory created successfully."

clean:
	rm -rf $(TARGET_BOARD) $(TARGET_HOST) $(TARGET_AI_ASR) $(TARGET_AI_TTS) package/
	@echo "Clean completed."
