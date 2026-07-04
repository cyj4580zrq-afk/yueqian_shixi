#include "common/config.h"
#include "driver/display_drv.h"
#include "driver/touch_drv.h"
#include "hal/touch_hal.h"
#include "hal/display_hal.h"
#include "protocol/wifi_manager.h"
#include "protocol/weather_client.h"
#include "protocol/control_proto.h"
#include "protocol/audio_proto.h"
#include "application/page_common.h"
#include "utils/audio_recorder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <linux/input.h>

// 全局变量定义
pthread_mutex_t g_ui_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_current_page = 0;
static int g_redraw_pending = 1;

char recognized_text[512] = "";
char ai_reply_text[512] = "";

extern app_config_t g_config;
extern int config_load(const char *filepath);

// 页面实例管理
static app_page_t *g_pages[6];

// 全局网络状态
char g_ssid[64] = "";
char g_local_ip[64] = "";
char g_wuhan_weather[128] = "Loading...";
char g_beijing_weather[128] = "Loading...";

void request_redraw(void) {
    pthread_mutex_lock(&g_ui_mutex);
    g_redraw_pending = 1;
    pthread_mutex_unlock(&g_ui_mutex);
}

// 接收 ASR 解析文字和 DeepSeek 回复的回调 (根据 ASR/AI 前缀区分写入相应缓冲区)
static void control_callback(const char *prefix, const char *msg) {
    printf("[Main Callback] Prefix: %s, Msg: %s\n", prefix, msg);
    int notify_asr = 0;
    int notify_ai = 0;
    pthread_mutex_lock(&g_ui_mutex);
    if (prefix && msg) {
        if (strcmp(prefix, "ASR") == 0) {
            strncpy(recognized_text, msg, sizeof(recognized_text) - 1);
            recognized_text[sizeof(recognized_text) - 1] = '\0';
            notify_asr = 1;
        } else if (strcmp(prefix, "AI") == 0) {
            strncpy(ai_reply_text, msg, sizeof(ai_reply_text) - 1);
            ai_reply_text[sizeof(ai_reply_text) - 1] = '\0';
            notify_ai = 1;
        }
    }
    g_redraw_pending = 1;
    pthread_mutex_unlock(&g_ui_mutex);

    // 在互斥锁释放后通知 UI 状态转换，防止重入 request_redraw 造成死锁
    if (notify_asr) {
        extern void page_ai_notify_asr_ready(void);
        page_ai_notify_asr_ready();
    }
    if (notify_ai) {
        extern void page_ai_notify_ai_response(void);
        audio_recorder_mark_done();
        page_ai_notify_ai_response();
    }
}

typedef struct {
    char wav_path[256];
} playback_job_t;

static void *playback_thread(void *arg)
{
    playback_job_t *job = (playback_job_t *)arg;
    char cmd[512];
    int ret;

    extern void page_ai_notify_playback_done(void);
    extern void page_ai_notify_playback_error(void);

    snprintf(cmd, sizeof(cmd), "aplay -D plughw:1,0 -q %s || aplay -q %s", job->wav_path, job->wav_path);
    ret = system(cmd);
    if (ret == 0) {
        page_ai_notify_playback_done();
    } else {
        fputs("[Main Callback] Audio playback failed: ", stdout);
        printf("%d", ret);
        putchar(10);
        page_ai_notify_playback_error();
    }

    free(job);
    return NULL;
}

// 接收 AI 语音合成音频的回调，播放放入独立线程，避免阻塞音频接收线程。
static void audio_callback(const char *wav_path) {
    pthread_t tid;
    playback_job_t *job;
    extern void page_ai_notify_audio_ready(void);
    extern void page_ai_notify_playback_error(void);

    fputs("[Main Callback] Audio Response ready at: ", stdout);
    fputs(wav_path, stdout);
    putchar(10);

    page_ai_notify_audio_ready();

    job = (playback_job_t *)malloc(sizeof(playback_job_t));
    if (!job) {
        page_ai_notify_playback_error();
        return;
    }
    snprintf(job->wav_path, sizeof(job->wav_path), "%s", wav_path);

    if (pthread_create(&tid, NULL, playback_thread, job) != 0) {
        free(job);
        page_ai_notify_playback_error();
        return;
    }
    pthread_detach(tid);
}



// 网络状态与天气定时刷新线程 (非阻塞)
static void *network_monitor_thread(void *arg) {
    int counter = 0;
    (void)arg;

    while (1) {
        // 降低为 10 秒轮询一次 IP/SSID，且仅在发生实际改变时触发重绘
        if (counter % 10 == 0) {
            char new_ip[64] = "";
            char new_ssid[64] = "";
            wifi_manager_refresh_ip(new_ip, sizeof(new_ip), new_ssid, sizeof(new_ssid));
            
            pthread_mutex_lock(&g_ui_mutex);
            int ip_changed = strcmp(g_local_ip, new_ip) != 0;
            int ssid_changed = strcmp(g_ssid, new_ssid) != 0;
            if (ip_changed || ssid_changed) {
                strcpy(g_local_ip, new_ip);
                strcpy(g_ssid, new_ssid);
                pthread_mutex_unlock(&g_ui_mutex);
                request_redraw();
            } else {
                pthread_mutex_unlock(&g_ui_mutex);
            }
        }
        if (counter > 0 && counter % 600 == 0) {
            weather_page_request_refresh_async(0);
        }
        counter++;
        sleep(1);
    }
    return NULL;
}

int main(int argc, char **argv) {
    printf("Starting AI Assistant Board GUI...\n");
    
    // 1. 初始化配置
    if (config_load("/root/code/config/app_config.json") != 0) {
        if (config_load("./config/app_config.json") != 0) {
            printf("Config Load Failed. Using default parameters.\n");
            // 使用保底默认参数，绝对不要退出
            config_load(NULL);
        }
    }
    
    // 2. 初始化显示与字体
    if (display_drv_init(g_config.drm_device) != 0) {
        printf("Display Init Failed.\n");
        return -1;
    }
    
    // 6. 字体路径动态加载，防止硬编码开发机路径
    extern int font_init(const char *font_path);
    if (font_init(g_config.font_path) != 0) {
        if (font_init("assets/simsun.ttc") != 0) {
            if (font_init("./assets/simsun.ttc") != 0) {
                if (font_init("/root/code/assets/simsun.ttc") != 0) {
                    if (font_init("assets/simsun.ttf") != 0) {
                        printf("Warning: Failed to load font file. Display will degrade safely.\n");
                    }
                }
            }
        }
    }
    
    // 初始化 HAL 触摸
    touch_hal_init(g_config.ts_min_x, g_config.ts_max_x, g_config.ts_min_y, g_config.ts_max_y);
    int touch_fd = touch_drv_init(g_config.touch_device);
    if (touch_fd < 0) {
        printf("Touch Init Failed on %s.\n", g_config.touch_device);
        display_drv_exit();
        return -1;
    }
    
    // 3. 获取各页面实例，并进行严格的 NULL 空指针校验
    g_pages[0] = page_home_get_instance();
    g_pages[1] = page_ai_get_instance();
    g_pages[2] = page_weather_get_instance();
    g_pages[3] = page_album_get_instance();
    g_pages[4] = page_file_get_instance();
    g_pages[5] = page_settings_get_instance();
    
    for (int i = 0; i < 6; i++) {
        if (!g_pages[i]) {
            printf("Fatal Error: Page [%d] instance is NULL!\n", i);
            touch_drv_close(touch_fd);
            display_drv_exit();
            return -1;
        }
    }
    
    // 4. 启动 TCP 监听服务 (连接控制流与音频流)
    control_proto_start_listen(8888, control_callback);
    audio_proto_start_listen(8889, "/tmp", audio_callback);
    
    // 5. 启动网络状态刷新监视线程
    pthread_t net_tid;
    pthread_create(&net_tid, NULL, network_monitor_thread, NULL);
    pthread_detach(net_tid);

    weather_page_request_refresh_async(1);
    
    printf("UI Client Started. Event Loop running...\n");
    
    // 6. 主事件轮询循环 (触摸优先，排空队列，避免重复刷新)
    while (1) {
        // 先以最高优先级排空所有输入事件，消除卡顿并降低触控延迟
        struct input_event ev;
        while (touch_drv_read(touch_fd, &ev) == 0) {
            int px = -1, py = -1;
            int type = touch_hal_process_event(&ev, &px, &py);
            if (type > 0 && px >= 0 && py >= 0) {
                if (px < 200) {
                    if (type == 1) { // 仅在按下时切换菜单
                        int btn_idx = (py - 80) / 80;
                        if (btn_idx >= 0 && btn_idx < 6) {
                            pthread_mutex_lock(&g_ui_mutex);
                            if (g_current_page != btn_idx) {
                                g_current_page = btn_idx;
                                g_redraw_pending = 1;
                            }
                            pthread_mutex_unlock(&g_ui_mutex);
                        }
                    }
                } else {
                    if (g_pages[g_current_page] && g_pages[g_current_page]->handle_click) {
                        g_pages[g_current_page]->handle_click(px, py, type);
                    }
                }
            }
        }
        
        // 排空完所有触摸事件后，若存在重绘请求则进行单次重绘
        pthread_mutex_lock(&g_ui_mutex);
        if (g_redraw_pending) {
            g_redraw_pending = 0;
            pthread_mutex_unlock(&g_ui_mutex);
            display_hal_set_current_page(g_current_page);
            if (g_pages[g_current_page] && g_pages[g_current_page]->draw) {
                g_pages[g_current_page]->draw();
            }
        } else {
            pthread_mutex_unlock(&g_ui_mutex);
        }
        
        // 降低主线程 CPU 轮询占用，sleep 20ms
        usleep(20000);
    }
    
    control_proto_stop_listen();
    audio_proto_stop_listen();
    touch_drv_close(touch_fd);
    extern void font_done(void);
    font_done();
    display_drv_exit();
    return 0;
}

