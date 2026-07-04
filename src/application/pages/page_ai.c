#include "application/page_common.h"
#include "utils/audio_recorder.h"
#include "common/config.h"
#include "driver/display_drv.h"
#include "hal/display_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char g_local_ip[64];
extern char g_ssid[64];
extern char recognized_text[512];
extern char ai_reply_text[512];
extern app_config_t g_config;

static audio_recorder_state_t g_ai_state = AUDIO_RECORDER_IDLE;
static int g_audio_hook_ready = 0;

extern void request_redraw(void);

static const char *ai_state_text(audio_recorder_state_t state)
{
    switch (state) {
    case AUDIO_RECORDER_IDLE: return "空闲";
    case AUDIO_RECORDER_RECORDING: return "正在录音（3 秒）...";
    case AUDIO_RECORDER_SENDING: return "发送中...";
    case AUDIO_RECORDER_WAITING_RESPONSE: return "等待 AI 回复...";
    case AUDIO_RECORDER_PLAYING: return "正在播放回复...";
    case AUDIO_RECORDER_DONE: return "完成";
    case AUDIO_RECORDER_ERROR: return "发生错误，请重试";
    default: return "未知状态";
    }
}

static int ai_button_width(audio_recorder_state_t state)
{
    return (state == AUDIO_RECORDER_IDLE || state == AUDIO_RECORDER_ERROR || state == AUDIO_RECORDER_DONE) ? 200 : 220;
}

static void ai_state_changed(audio_recorder_state_t state)
{
    g_ai_state = state;
    request_redraw();
}

static void ensure_audio_hook(void)
{
    if (g_audio_hook_ready) return;
    g_audio_hook_ready = 1;
    audio_recorder_set_state_callback(ai_state_changed);
    g_ai_state = audio_recorder_get_state();
}

void page_ai_notify_asr_ready(void)
{
    ensure_audio_hook();
    g_ai_state = AUDIO_RECORDER_WAITING_RESPONSE;
    request_redraw();
}

void page_ai_notify_ai_response(void)
{
    ensure_audio_hook();
    g_ai_state = AUDIO_RECORDER_DONE;
    request_redraw();
}

void page_ai_notify_audio_ready(void)
{
    ensure_audio_hook();
    g_ai_state = AUDIO_RECORDER_PLAYING;
    request_redraw();
}

void page_ai_notify_playback_done(void)
{
    ensure_audio_hook();
    g_ai_state = AUDIO_RECORDER_DONE;
    request_redraw();
}

void page_ai_notify_playback_error(void)
{
    ensure_audio_hook();
    g_ai_state = AUDIO_RECORDER_ERROR;
    request_redraw();
}

static void ai_draw(void)
{
    char user_bubble[576];
    char ai_bubble[576];

    ensure_audio_hook();

    display_hal_draw_clear();
    display_hal_draw_framework(g_ssid, g_local_ip);
    display_hal_draw_card(220, 90, 260, 90, 0x1E1E1E, "AI 状态", ai_state_text(g_ai_state));

    snprintf(user_bubble, sizeof(user_bubble), "你：%s", recognized_text[0] ? recognized_text : "...");
    snprintf(ai_bubble, sizeof(ai_bubble), "AI：%s", ai_reply_text[0] ? ai_reply_text : "...");

    display_hal_draw_card(220, 190, 780, 110, 0x1B2C1B, "识别内容", user_bubble);
    display_hal_draw_card(220, 310, 780, 185, 0x1B1B2C, "AI 回复", ai_bubble);

    if (g_ai_state == AUDIO_RECORDER_IDLE || g_ai_state == AUDIO_RECORDER_ERROR || g_ai_state == AUDIO_RECORDER_DONE) {
        display_hal_draw_button(220, 505, ai_button_width(g_ai_state), 60, 0x0288D1, "开始录音", 0xFFFFFF);
    } else if (g_ai_state == AUDIO_RECORDER_RECORDING) {
        display_hal_draw_button(220, 505, ai_button_width(g_ai_state), 60, 0xD32F2F, "停止录音", 0xFFFFFF);
    } else if (g_ai_state == AUDIO_RECORDER_SENDING) {
        display_hal_draw_button(220, 505, ai_button_width(g_ai_state), 60, 0x8D99AE, "发送中...", 0xFFFFFF);
    } else if (g_ai_state == AUDIO_RECORDER_WAITING_RESPONSE) {
        display_hal_draw_button(220, 505, ai_button_width(g_ai_state), 60, 0x5D6D7E, "等待回复...", 0xFFFFFF);
    } else if (g_ai_state == AUDIO_RECORDER_PLAYING) {
        display_hal_draw_button(220, 505, ai_button_width(g_ai_state), 60, 0x2E7D32, "正在播放...", 0xFFFFFF);
    } else {
        display_hal_draw_button(220, 505, ai_button_width(g_ai_state), 60, 0xB71C1C, "错误，点击重试", 0xFFFFFF);
    }

    display_drv_flush();
}

static void ai_handle_click(int x, int y, int event_type)
{
    int ret;

    ensure_audio_hook();

    printf("[AI Page] ai_handle_click enter x=%d y=%d event_type=%d state=%d\n", x, y, event_type, (int)g_ai_state);

    if (event_type != 1) {
        printf("[AI Page] ai_handle_click return: ignore event_type=%d\n", event_type);
        return;
    }
    if (x < 220 || x > 470 || y < 505 || y > 545) {
        printf("[AI Page] ai_handle_click return: outside button area x=%d y=%d\n", x, y);
        return;
    }

    if (g_ai_state == AUDIO_RECORDER_IDLE || g_ai_state == AUDIO_RECORDER_ERROR || g_ai_state == AUDIO_RECORDER_DONE) {
        printf("[AI Page] Start record requested. state=%d\n", (int)g_ai_state);
        ret = audio_recorder_start_record(3, g_config.wsl_server_ip, g_config.wsl_voice_cmd_wav);
        if (ret != 0) {
            printf("[AI Page] Start record failed: %d\n", ret);
            g_ai_state = AUDIO_RECORDER_ERROR;
            request_redraw();
        }
    } else if (g_ai_state == AUDIO_RECORDER_RECORDING) {
        printf("[AI Page] Stop record requested.\n");
        ret = audio_recorder_stop_record();
        if (ret != 0) {
            printf("[AI Page] Stop record failed: %d\n", ret);
            g_ai_state = AUDIO_RECORDER_ERROR;
            request_redraw();
        }
    }
}

void page_ai_start_record(void)
{
    ensure_audio_hook();
    printf("[AI Page] page_ai_start_record enter state=%d\n", (int)g_ai_state);
    if (g_ai_state == AUDIO_RECORDER_IDLE || g_ai_state == AUDIO_RECORDER_ERROR || g_ai_state == AUDIO_RECORDER_DONE) {
        int ret = audio_recorder_start_record(3, g_config.wsl_server_ip, g_config.wsl_voice_cmd_wav);
        if (ret != 0) {
            printf("[AI Page] page_ai_start_record return: audio_recorder_start_record failed ret=%d\n", ret);
            g_ai_state = AUDIO_RECORDER_ERROR;
            request_redraw();
        } else {
            printf("[AI Page] page_ai_start_record accepted\n");
        }
    } else {
        printf("[AI Page] page_ai_start_record return: state=%d not allowed\n", (int)g_ai_state);
    }
}

static app_page_t g_ai_page = {
    .draw = ai_draw,
    .handle_click = ai_handle_click
};

app_page_t *page_ai_get_instance(void)
{
    return &g_ai_page;
}


