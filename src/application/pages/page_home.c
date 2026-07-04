#include "application/page_common.h"
#include "driver/display_drv.h"
#include "hal/display_hal.h"
#include <stdio.h>
#include <string.h>

extern char g_local_ip[64];
extern char g_ssid[64];
extern char g_wuhan_weather[128];
extern char g_beijing_weather[128];

static void home_draw(void) {
    display_hal_draw_clear();
    display_hal_draw_framework(g_ssid, g_local_ip);
    
    char wuhan_buf[256];
    char beijing_buf[256];
    snprintf(wuhan_buf, sizeof(wuhan_buf), "武汉：%s", g_wuhan_weather[0] ? g_wuhan_weather : "加载中...");
    snprintf(beijing_buf, sizeof(beijing_buf), "北京：%s", g_beijing_weather[0] ? g_beijing_weather : "加载中...");
    
    display_hal_draw_card(250, 115, 650, 120, 0x10243A, "智能中枢", "语音、天气、相册、文件与网络控制已就绪");
    display_hal_draw_card(250, 270, 300, 150, 0x123A2F, "武汉天气", wuhan_buf);
    display_hal_draw_card(600, 270, 300, 150, 0x142B4A, "北京天气", beijing_buf);
    
    display_hal_draw_text(250, 465, "欢迎使用 C/C++ AI 助手，触摸左侧菜单开始操作。", 0xD8F3FF);
    
    display_drv_flush();
}

static void home_handle_click(int x, int y, int event_type) {
    // 首页暂时没有特定可点击交互
}

static app_page_t g_home_page = {
    .draw = home_draw,
    .handle_click = home_handle_click
};

app_page_t *page_home_get_instance(void) {
    return &g_home_page;
}

