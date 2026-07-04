#include "application/page_common.h"
#include "protocol/weather_client.h"
#include "common/config.h"
#include "driver/display_drv.h"
#include "hal/display_hal.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WEATHER_CACHE_FILE "config/weather_cache.json"
#define WEATHER_REFRESH_MIN_GAP_SECONDS 5

extern char g_local_ip[64];
extern char g_ssid[64];
extern char g_wuhan_weather[128];
extern char g_beijing_weather[128];
extern app_config_t g_config;

static pthread_mutex_t g_weather_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_weather_cache_loaded = 0;
static int g_weather_refreshing = 0;
static time_t g_weather_last_request_time = 0;
static char g_weather_update_time[64] = "--";
static char g_weather_status[160] = "";
static char g_weather_city1_name[64] = "武汉";
static char g_weather_city2_name[64] = "广州";

extern void request_redraw(void);

static void safe_copy(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void weather_now_string(char *buf, size_t buf_size)
{
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", &tm_info);
}

static const char *find_json_value_start(const char *json, const char *key)
{
    const char *pos = strstr(json, key);
    const char *colon;
    const char *quote;

    if (!pos) return NULL;
    colon = strchr(pos, ':');
    if (!colon) return NULL;
    quote = strchr(colon + 1, '"');
    if (!quote) return NULL;
    return quote + 1;
}

static int extract_json_string(const char *json, const char *key, char *out, size_t out_size)
{
    const char *value_start;
    const char *value_end;
    size_t len;

    if (!json || !key || !out || out_size == 0) return -1;

    value_start = find_json_value_start(json, key);
    if (!value_start) return -1;
    value_end = strchr(value_start, '"');
    if (!value_end || value_end <= value_start) return -1;

    len = (size_t)(value_end - value_start);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, value_start, len);
    out[len] = '\0';
    return 0;
}

static void ensure_defaults_locked(void)
{
    if (!g_wuhan_weather[0]) safe_copy(g_wuhan_weather, sizeof(g_wuhan_weather), "未知");
    if (!g_beijing_weather[0]) safe_copy(g_beijing_weather, sizeof(g_beijing_weather), "未知");
    if (!g_weather_update_time[0]) safe_copy(g_weather_update_time, sizeof(g_weather_update_time), "--");
}

static void load_weather_cache_locked(int force_reload)
{
    FILE *fp;
    char buf[2048];
    size_t read_len;

    if (g_weather_cache_loaded && !force_reload) return;

    fp = fopen(WEATHER_CACHE_FILE, "r");
    if (!fp) {
        ensure_defaults_locked();
        if (!g_weather_status[0]) {
            safe_copy(g_weather_status, sizeof(g_weather_status), "暂无缓存，点击刷新获取天气");
        }
        g_weather_cache_loaded = 1;
        return;
    }

    read_len = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[read_len] = '\0';

    if (extract_json_string(buf, "\"wuhan\"", g_wuhan_weather, sizeof(g_wuhan_weather)) != 0) {
        safe_copy(g_wuhan_weather, sizeof(g_wuhan_weather), "未知");
    }
    if (extract_json_string(buf, "\"beijing\"", g_beijing_weather, sizeof(g_beijing_weather)) != 0) {
        safe_copy(g_beijing_weather, sizeof(g_beijing_weather), "未知");
    }
    if (extract_json_string(buf, "\"update_time\"", g_weather_update_time, sizeof(g_weather_update_time)) != 0) {
        safe_copy(g_weather_update_time, sizeof(g_weather_update_time), "--");
    }

    ensure_defaults_locked();
    g_weather_cache_loaded = 1;
}

static void save_weather_cache_locked(void)
{
    FILE *fp;
    char update_time[64];

    if (!g_weather_update_time[0] || strcmp(g_weather_update_time, "--") == 0) {
        weather_now_string(update_time, sizeof(update_time));
        safe_copy(g_weather_update_time, sizeof(g_weather_update_time), update_time);
    }

    fp = fopen(WEATHER_CACHE_FILE, "w");
    if (!fp) {
        safe_copy(g_weather_status, sizeof(g_weather_status), "天气缓存写入失败");
        return;
    }

    fprintf(fp,
            "{\n"
            "  \"wuhan\": \"%s\",\n"
            "  \"beijing\": \"%s\",\n"
            "  \"update_time\": \"%s\"\n"
            "}\n",
            g_wuhan_weather, g_beijing_weather, g_weather_update_time);
    fclose(fp);
    g_weather_cache_loaded = 1;
}

static void *weather_refresh_thread(void *arg)
{
    char wuhan[128] = "";
    char beijing[128] = "";
    int ret;

    (void)arg;

    pthread_mutex_lock(&g_weather_mutex);
    safe_copy(g_weather_status, sizeof(g_weather_status), "天气更新中...");
    pthread_mutex_unlock(&g_weather_mutex);
    request_redraw();

    ret = weather_client_fetch(g_config.wsl_server_ip,
                               g_config.wsl_get_weather,
                               wuhan, sizeof(wuhan),
                               beijing, sizeof(beijing));

    pthread_mutex_lock(&g_weather_mutex);
    if (ret == 0 && wuhan[0] && beijing[0]) {
        safe_copy(g_wuhan_weather, sizeof(g_wuhan_weather), wuhan);
        safe_copy(g_beijing_weather, sizeof(g_beijing_weather), beijing);
        weather_client_get_city_names(g_weather_city1_name, sizeof(g_weather_city1_name), g_weather_city2_name, sizeof(g_weather_city2_name));
        weather_client_get_update_time(g_weather_update_time, sizeof(g_weather_update_time));
        save_weather_cache_locked();
        safe_copy(g_weather_status, sizeof(g_weather_status), "天气已更新");
    } else if (ret == 1) {
        load_weather_cache_locked(1);
        safe_copy(g_weather_status, sizeof(g_weather_status), "天气更新失败，显示本地缓存");
    } else {
        load_weather_cache_locked(1);
        ensure_defaults_locked();
        safe_copy(g_weather_status, sizeof(g_weather_status), "天气更新失败，继续显示旧缓存");
    }
    g_weather_refreshing = 0;
    pthread_mutex_unlock(&g_weather_mutex);

    request_redraw();
    return NULL;
}

int weather_page_request_refresh_async(int force_refresh)
{
    pthread_t thread;
    pthread_attr_t attr;
    time_t now = time(NULL);

    pthread_mutex_lock(&g_weather_mutex);
    load_weather_cache_locked(0);

    if (g_weather_refreshing) {
        safe_copy(g_weather_status, sizeof(g_weather_status), "天气更新中...");
        pthread_mutex_unlock(&g_weather_mutex);
        request_redraw();
        return 0;
    }

    if (!force_refresh && g_weather_last_request_time > 0 && (now - g_weather_last_request_time) < WEATHER_REFRESH_MIN_GAP_SECONDS) {
        safe_copy(g_weather_status, sizeof(g_weather_status), "5秒内不重复请求，先显示缓存");
        pthread_mutex_unlock(&g_weather_mutex);
        request_redraw();
        return 0;
    }

    g_weather_refreshing = 1;
    g_weather_last_request_time = now;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&thread, &attr, weather_refresh_thread, NULL) != 0) {
        g_weather_refreshing = 0;
        safe_copy(g_weather_status, sizeof(g_weather_status), "天气更新线程启动失败");
        pthread_attr_destroy(&attr);
        pthread_mutex_unlock(&g_weather_mutex);
        request_redraw();
        return -1;
    }

    pthread_attr_destroy(&attr);
    pthread_mutex_unlock(&g_weather_mutex);
    request_redraw();
    return 0;
}

static void weather_draw(void)
{
    char wuhan_detail[256];
    char beijing_detail[256];
    char update_line[128];
    char status_line[192];
    int refreshing;

    pthread_mutex_lock(&g_weather_mutex);
    load_weather_cache_locked(0);
    refreshing = g_weather_refreshing;
    snprintf(wuhan_detail, sizeof(wuhan_detail), "%s", g_wuhan_weather[0] ? g_wuhan_weather : "未知");
    snprintf(beijing_detail, sizeof(beijing_detail), "%s", g_beijing_weather[0] ? g_beijing_weather : "未知");
    snprintf(update_line, sizeof(update_line), "更新时间：%s", g_weather_update_time[0] ? g_weather_update_time : "--");
    snprintf(status_line, sizeof(status_line), "%s", g_weather_status[0] ? g_weather_status : (refreshing ? "天气更新中..." : "点击刷新获取天气"));
    pthread_mutex_unlock(&g_weather_mutex);

    display_hal_draw_clear();
    display_hal_draw_framework(g_ssid, g_local_ip);
    display_hal_draw_card(250, 120, 300, 200, 0x1E3A1E, g_weather_city1_name, wuhan_detail);
    display_hal_draw_card(600, 120, 300, 200, 0x1E1E3A, g_weather_city2_name, beijing_detail);
    display_hal_draw_card(250, 340, 720, 130, 0x101820, "天气状态", status_line);
    display_hal_draw_text(276, 430, update_line, 0x9BB8D2);
    display_hal_draw_button(250, 500, 200, 56, refreshing ? 0xB0BEC5 : 0x0288D1, refreshing ? "更新中..." : "刷新天气", 0xFFFFFF);
    display_drv_flush();
}

static void weather_handle_click(int x, int y, int event_type)
{
    if (event_type != 1) return;
    if (x >= 250 && x <= 450 && y >= 500 && y <= 556) {
        weather_page_request_refresh_async(0);
    }
}

static app_page_t g_weather_page = {
    .draw = weather_draw,
    .handle_click = weather_handle_click
};

app_page_t *page_weather_get_instance(void)
{
    return &g_weather_page;
}




