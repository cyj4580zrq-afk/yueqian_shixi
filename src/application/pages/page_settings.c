#include "application/page_common.h"
#include "protocol/wifi_manager.h"
#include "common/config.h"
#include "driver/display_drv.h"
#include "hal/display_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

extern char g_local_ip[64];
extern char g_ssid[64];
extern app_config_t g_config;

#define WIFI_PAGE_SIZE 7
#define KB_KEY_W 45
#define KB_KEY_H 30
#define KB_KEY_GAP 5
#define KB_KEY_X 245
#define KB_KEY_Y 350

typedef struct {
    int x;
    int y;
    int w;
    int h;
    const char *label;
} key_button_t;

static wifi_info_t g_wifis[MAX_WIFI];
static int g_wifi_count = -1;
static int g_wifi_state = 0;
static int g_wifi_page = 0;
static int g_keyboard_visible = 0;
static int g_password_visible = 0;
static char g_connecting_ssid[64] = "";
static char g_input_password[128] = "";

static pthread_t g_scan_thread;
static pthread_t g_connect_thread;

extern void request_redraw(void);

static void append_password_char(const char *text)
{
    size_t len;
    if (!text || !text[0]) return;
    len = strlen(g_input_password);
    if (len + strlen(text) >= sizeof(g_input_password)) return;
    strcat(g_input_password, text);
}

static void password_backspace(void)
{
    size_t len = strlen(g_input_password);
    if (len > 0) g_input_password[len - 1] = '\0';
}

static void make_masked_password(char *out, size_t out_size)
{
    size_t len = strlen(g_input_password);
    size_t show_len = len;
    if (show_len >= out_size) show_len = out_size - 1;
    memset(out, '*', show_len);
    out[show_len] = '\0';
}

static void *wifi_scan_thread(void *arg)
{
    (void)arg;
    g_wifi_state = 1;
    g_keyboard_visible = 0;
    g_password_visible = 0;
    request_redraw();

    g_wifi_count = wifi_manager_scan(g_wifis, MAX_WIFI);
    g_wifi_page = 0;

    g_wifi_state = 0;
    request_redraw();
    return NULL;
}

static void *wifi_connect_thread(void *arg)
{
    (void)arg;
    g_wifi_state = 2;
    g_keyboard_visible = 0;
    g_password_visible = 0;
    request_redraw();

    wifi_manager_connect_with_password(g_connecting_ssid, g_input_password);

    sleep(3);
    wifi_manager_refresh_ip(g_local_ip, sizeof(g_local_ip), g_ssid, sizeof(g_ssid));

    g_wifi_state = 0;
    request_redraw();
    return NULL;
}

static void start_scan(void)
{
    pthread_attr_t attr;
    if (g_wifi_state != 0) return;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&g_scan_thread, &attr, wifi_scan_thread, NULL);
    pthread_attr_destroy(&attr);
}

static void start_connect(void)
{
    pthread_attr_t attr;
    if (g_wifi_state != 0 || !g_connecting_ssid[0]) return;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&g_connect_thread, &attr, wifi_connect_thread, NULL);
    pthread_attr_destroy(&attr);
}

static void draw_keyboard_key(int x, int y, int w, int h, const char *label, uint32_t color)
{
    display_hal_draw_button(x, y, w, h, color, label, 0xFFFFFF);
}

static void draw_keyboard_row(const char *keys, int x, int y)
{
    int i;
    for (i = 0; keys[i]; ++i) {
        char label[2] = { keys[i], '\0' };
        draw_keyboard_key(x + i * (KB_KEY_W + KB_KEY_GAP), y, KB_KEY_W, KB_KEY_H, label, 0x26384F);
    }
}

static void draw_password_keyboard(void)
{
    char pwd_mask[128];
    char title[192];
    int y;

    if (g_password_visible) snprintf(pwd_mask, sizeof(pwd_mask), "%s", g_input_password);
    else make_masked_password(pwd_mask, sizeof(pwd_mask));
    snprintf(title, sizeof(title), "已选择：%s", g_connecting_ssid[0] ? g_connecting_ssid : "未选择");

    display_hal_draw_card(220, 292, 780, 245, 0x0B1726, "输入 WiFi 密码", "");
    display_hal_draw_text(240, 322, title, 0x00FF00);
    display_hal_draw_text(560, 322, pwd_mask[0] ? pwd_mask : "请输入密码", pwd_mask[0] ? 0xFFFFFF : 0x888888);
    draw_keyboard_key(860, 316, 100, 30, g_password_visible ? "隐藏" : "显示", 0x2C4868);

    y = KB_KEY_Y;
    draw_keyboard_row("1234567890", KB_KEY_X, y);
    y += KB_KEY_H + 6;
    draw_keyboard_row("qwertyuiop", KB_KEY_X, y);
    y += KB_KEY_H + 6;
    draw_keyboard_row("asdfghjkl", KB_KEY_X + 24, y);
    draw_keyboard_key(KB_KEY_X + 24 + 9 * (KB_KEY_W + KB_KEY_GAP), y, 75, KB_KEY_H, "删除", 0x7A2630);
    y += KB_KEY_H + 6;
    draw_keyboard_row("zxcvbnm.-_", KB_KEY_X, y);
    y += KB_KEY_H + 6;
    draw_keyboard_key(KB_KEY_X, y, 55, KB_KEY_H, "@", 0x26384F);
    draw_keyboard_key(KB_KEY_X + 62, y, 55, KB_KEY_H, "#", 0x26384F);
    draw_keyboard_key(KB_KEY_X + 124, y, 55, KB_KEY_H, "!", 0x26384F);
    draw_keyboard_key(KB_KEY_X + 186, y, 55, KB_KEY_H, "?", 0x26384F);
    draw_keyboard_key(KB_KEY_X + 248, y, 55, KB_KEY_H, "*", 0x26384F);
    draw_keyboard_key(KB_KEY_X + 318, y, 120, KB_KEY_H, "清空", 0x5A3A1E);
    draw_keyboard_key(KB_KEY_X + 448, y, 120, KB_KEY_H, "取消", 0x4A4A4A);
    draw_keyboard_key(KB_KEY_X + 578, y, 150, KB_KEY_H, "连接", 0x168A45);
}

static void settings_draw(void)
{
    char ip_buf[256];

    display_hal_draw_clear();
    display_hal_draw_framework(g_ssid, g_local_ip);

    snprintf(ip_buf, sizeof(ip_buf), "IP 地址：%s", g_local_ip[0] ? g_local_ip : "未连接");

    display_hal_draw_card(250, 120, 280, 160, 0x1E1E1E, "WiFi 连接状态", ip_buf);

    if (g_wifi_state == 0) {
        display_hal_draw_button(250, 310, 200, 50, 0x0288D1, "扫描 WiFi", 0xFFFFFF);
    } else if (g_wifi_state == 1) {
        display_hal_draw_button(250, 310, 200, 50, 0xB0BEC5, "扫描中...", 0xFFFFFF);
    } else if (g_wifi_state == 2) {
        char conn_btn[128];
        snprintf(conn_btn, sizeof(conn_btn), "正在连接 %s...", g_connecting_ssid);
        display_hal_draw_button(250, 310, 250, 50, 0xEF6C00, conn_btn, 0xFFFFFF);
    }

    display_hal_draw_text(580, 100, "扫描结果：", 0x00FF00);

    if (g_wifi_count < 0) {
        display_hal_draw_text(580, 150, "请先点击“扫描 WiFi”。", 0x888888);
    } else if (g_wifi_count == 0) {
        display_hal_draw_text(580, 150, "未发现网络。", 0xFF0000);
    } else {
        int page_count = (g_wifi_count + WIFI_PAGE_SIZE - 1) / WIFI_PAGE_SIZE;
        int start;
        int remain;
        int draw_limit;
        char page_txt[64];

        if (g_wifi_page >= page_count) g_wifi_page = page_count - 1;
        if (g_wifi_page < 0) g_wifi_page = 0;
        start = g_wifi_page * WIFI_PAGE_SIZE;
        remain = g_wifi_count - start;
        draw_limit = remain > WIFI_PAGE_SIZE ? WIFI_PAGE_SIZE : remain;

        for (int i = 0; i < draw_limit; i++) {
            int idx = start + i;
            char item_txt[256];
            uint32_t color = strcmp(g_wifis[idx].ssid, g_connecting_ssid) == 0 ? 0x245C42 : 0x2A2A2A;
            snprintf(item_txt, sizeof(item_txt), "%s (%d dBm)", g_wifis[idx].ssid, g_wifis[idx].signal);
            display_hal_draw_button(580, 130 + i * 55, 350, 45, color, item_txt, 0xFFFFFF);
        }

        snprintf(page_txt, sizeof(page_txt), "%d/%d", g_wifi_page + 1, page_count);
        display_hal_draw_text(760, 100, page_txt, 0xFFFFFF);
        if (!g_keyboard_visible) {
            if (g_wifi_page > 0) display_hal_draw_button(580, 520, 150, 40, 0x2C4868, "上一页", 0xFFFFFF);
            if (g_wifi_page + 1 < page_count) display_hal_draw_button(780, 520, 150, 40, 0x2C4868, "下一页", 0xFFFFFF);
        }
    }

    if (g_keyboard_visible) draw_password_keyboard();

    display_drv_flush();
}

static int handle_keyboard_click(int x, int y)
{
    const char *rows[] = { "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm.-_" };
    const int row_x[] = { KB_KEY_X, KB_KEY_X, KB_KEY_X + 24, KB_KEY_X };
    const int row_y[] = { KB_KEY_Y, KB_KEY_Y + 36, KB_KEY_Y + 72, KB_KEY_Y + 108 };
    int i;

    if (!g_keyboard_visible) return 0;

    for (int r = 0; r < 4; ++r) {
        int len = strlen(rows[r]);
        for (i = 0; i < len; ++i) {
            int kx = row_x[r] + i * (KB_KEY_W + KB_KEY_GAP);
            int ky = row_y[r];
            if (x >= kx && x <= kx + KB_KEY_W && y >= ky && y <= ky + KB_KEY_H) {
                char text[2] = { rows[r][i], '\0' };
                append_password_char(text);
                request_redraw();
                return 1;
            }
        }
    }

    if (x >= 860 && x <= 960 && y >= 316 && y <= 346) {
        g_password_visible = !g_password_visible;
        request_redraw();
        return 1;
    }

    if (x >= KB_KEY_X + 24 + 9 * (KB_KEY_W + KB_KEY_GAP) && x <= KB_KEY_X + 24 + 9 * (KB_KEY_W + KB_KEY_GAP) + 75 &&
        y >= KB_KEY_Y + 72 && y <= KB_KEY_Y + 72 + KB_KEY_H) {
        password_backspace();
        request_redraw();
        return 1;
    }

    if (x >= KB_KEY_X && x <= KB_KEY_X + 55 && y >= KB_KEY_Y + 144 && y <= KB_KEY_Y + 144 + KB_KEY_H) append_password_char("@");
    else if (x >= KB_KEY_X + 62 && x <= KB_KEY_X + 117 && y >= KB_KEY_Y + 144 && y <= KB_KEY_Y + 144 + KB_KEY_H) append_password_char("#");
    else if (x >= KB_KEY_X + 124 && x <= KB_KEY_X + 179 && y >= KB_KEY_Y + 144 && y <= KB_KEY_Y + 144 + KB_KEY_H) append_password_char("!");
    else if (x >= KB_KEY_X + 186 && x <= KB_KEY_X + 241 && y >= KB_KEY_Y + 144 && y <= KB_KEY_Y + 144 + KB_KEY_H) append_password_char("?");
    else if (x >= KB_KEY_X + 248 && x <= KB_KEY_X + 303 && y >= KB_KEY_Y + 144 && y <= KB_KEY_Y + 144 + KB_KEY_H) append_password_char("*");
    else if (x >= KB_KEY_X + 318 && x <= KB_KEY_X + 438 && y >= KB_KEY_Y + 144 && y <= KB_KEY_Y + 144 + KB_KEY_H) g_input_password[0] = '\0';
    else if (x >= KB_KEY_X + 448 && x <= KB_KEY_X + 568 && y >= KB_KEY_Y + 144 && y <= KB_KEY_Y + 144 + KB_KEY_H) {
        g_keyboard_visible = 0;
    } else if (x >= KB_KEY_X + 578 && x <= KB_KEY_X + 728 && y >= KB_KEY_Y + 144 && y <= KB_KEY_Y + 144 + KB_KEY_H) {
        start_connect();
    } else {
        return 0;
    }

    request_redraw();
    return 1;
}

static void settings_handle_click(int x, int y, int event_type)
{
    if (event_type != 1) return;
    if (g_wifi_state != 0) return;

    if (handle_keyboard_click(x, y)) return;

    if (x >= 250 && x <= 450 && y >= 310 && y <= 360) {
        start_scan();
        return;
    }

    if (g_wifi_count > 0) {
        int page_count = (g_wifi_count + WIFI_PAGE_SIZE - 1) / WIFI_PAGE_SIZE;
        int start = g_wifi_page * WIFI_PAGE_SIZE;
        int remain = g_wifi_count - start;
        int draw_limit = remain > WIFI_PAGE_SIZE ? WIFI_PAGE_SIZE : remain;

        if (!g_keyboard_visible && x >= 580 && x <= 730 && y >= 520 && y <= 560 && g_wifi_page > 0) {
            --g_wifi_page;
            request_redraw();
            return;
        }
        if (!g_keyboard_visible && x >= 780 && x <= 930 && y >= 520 && y <= 560 && g_wifi_page + 1 < page_count) {
            ++g_wifi_page;
            request_redraw();
            return;
        }
        if (x >= 580 && x <= 930) {
            for (int i = 0; i < draw_limit; i++) {
                int idx = start + i;
                int y_start = 130 + i * 55;
                int y_end = y_start + 45;
                if (y >= y_start && y <= y_end) {
                    strncpy(g_connecting_ssid, g_wifis[idx].ssid, sizeof(g_connecting_ssid) - 1);
                    g_connecting_ssid[sizeof(g_connecting_ssid) - 1] = '\0';
                    g_input_password[0] = '\0';
                    g_password_visible = 0;
                    g_keyboard_visible = 1;
                    request_redraw();
                    return;
                }
            }
        }
    }
}

static app_page_t g_settings_page = {
    .draw = settings_draw,
    .handle_click = settings_handle_click
};

app_page_t *page_settings_get_instance(void)
{
    return &g_settings_page;
}



