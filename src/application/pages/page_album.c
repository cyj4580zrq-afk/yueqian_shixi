#include "application/page_common.h"
#include "utils/file_helper.h"
#include "common/config.h"
#include "driver/display_drv.h"
#include "hal/display_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char g_local_ip[64];
extern char g_ssid[64];
extern app_config_t g_config;

static char g_image_files[MAX_FILES][256];
static int g_image_count = -1;
static int g_image_index = 0;
static int g_zoom_percent = 100;

extern void request_redraw(void);

static void album_draw(void) {
    display_hal_draw_clear();
    display_hal_draw_framework(g_ssid, g_local_ip);
    
    if (g_image_count < 0) {
        g_image_count = file_helper_get_files(g_config.image_dir, ".bmp", g_image_files, MAX_FILES);
        g_image_index = 0;
        g_zoom_percent = 100;
    }
    
    if (g_image_count <= 0) {
        display_hal_draw_text(250, 200, "未找到可预览的 BMP 图片。", 0xFFFFFF);
    } else {
        char title[512];
        char zoom_info[64];
        char fullpath[512];

        snprintf(title, sizeof(title), "相册 [%d/%d]：%s", g_image_index + 1, g_image_count, g_image_files[g_image_index]);
        snprintf(zoom_info, sizeof(zoom_info), "缩放：%d%%", g_zoom_percent);
        snprintf(fullpath, sizeof(fullpath), "%s/%s", g_config.image_dir, g_image_files[g_image_index]);

        display_hal_draw_text(220, 100, title, 0x00FF00);
        display_hal_draw_text(865, 100, zoom_info, 0xFFFFFF);
        display_hal_draw_card(220, 130, 780, 350, 0x101820, "图片预览", "");

        if (display_hal_draw_bmp_scaled(230, 160, 760, 305, fullpath, g_zoom_percent) != 0) {
            display_hal_draw_text(250, 260, "图片加载失败，仅支持 24/32 位 BMP。", 0xFF5555);
        }

        display_hal_draw_button(220, 495, 150, 45, 0x1E1E1E, "上一张", 0xFFFFFF);
        display_hal_draw_button(395, 495, 130, 45, 0x1E1E1E, "缩小", 0xFFFFFF);
        display_hal_draw_button(550, 495, 130, 45, 0x1E1E1E, "放大", 0xFFFFFF);
        display_hal_draw_button(705, 495, 150, 45, 0x1E1E1E, "下一张", 0xFFFFFF);
    }
    
    display_drv_flush();
}

static void album_handle_click(int x, int y, int event_type) {
    if (event_type == 1 && g_image_count > 0) {
        if (x >= 220 && x <= 370 && y >= 495 && y <= 540) {
            g_image_index--;
            if (g_image_index < 0) g_image_index = g_image_count - 1;
            g_zoom_percent = 100;
            request_redraw();
            return;
        }
        if (x >= 395 && x <= 525 && y >= 495 && y <= 540) {
            if (g_zoom_percent > 50) g_zoom_percent -= 25;
            request_redraw();
            return;
        }
        if (x >= 550 && x <= 680 && y >= 495 && y <= 540) {
            if (g_zoom_percent < 300) g_zoom_percent += 25;
            request_redraw();
            return;
        }
        if (x >= 705 && x <= 855 && y >= 495 && y <= 540) {
            g_image_index++;
            if (g_image_index >= g_image_count) g_image_index = 0;
            g_zoom_percent = 100;
            request_redraw();
            return;
        }
    }
}

static app_page_t g_album_page = {
    .draw = album_draw,
    .handle_click = album_handle_click
};

app_page_t *page_album_get_instance(void) {
    return &g_album_page;
}
