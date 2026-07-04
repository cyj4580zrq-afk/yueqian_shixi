#include "hal/touch_hal.h"
#include <stdio.h>
#include <stdlib.h>

static int g_min_x = 0;
static int g_max_x = 1023;
static int g_min_y = 0;
static int g_max_y = 599;

static int g_last_raw_x = -1;
static int g_last_raw_y = -1;
static int g_touch_pressed = 0;

void touch_hal_init(int min_x, int max_x, int min_y, int max_y) {
    g_min_x = min_x;
    g_max_x = max_x;
    g_min_y = min_y;
    g_max_y = max_y;
    printf("[Touch HAL] Calibrated with MinX:%d MaxX:%d MinY:%d MaxY:%d\n", min_x, max_x, min_y, max_y);
}

void touch_hal_exit(void) {
    // 无需特别清理
}

static int map_x(int raw) {
    if (g_max_x == g_min_x) return 0;
    int mapped = (raw - g_min_x) * 1024 / (g_max_x - g_min_x);
    if (mapped < 0) mapped = 0;
    if (mapped > 1023) mapped = 1023;
    return mapped;
}

static int map_y(int raw) {
    if (g_max_y == g_min_y) return 0;
    int mapped = (raw - g_min_y) * 600 / (g_max_y - g_min_y);
    if (mapped < 0) mapped = 0;
    if (mapped > 599) mapped = 599;
    return mapped;
}

int touch_hal_process_event(struct input_event *ev, int *out_x, int *out_y) {
    if (!ev) return 0;
    
    // 多点/单点触摸坐标提取
    if (ev->type == EV_ABS) {
        if (ev->code == ABS_MT_POSITION_X || ev->code == ABS_X) {
            g_last_raw_x = ev->value;
        } else if (ev->code == ABS_MT_POSITION_Y || ev->code == ABS_Y) {
            g_last_raw_y = ev->value;
        } else if (ev->code == ABS_MT_TRACKING_ID) {
            if (ev->value >= 0) {
                g_touch_pressed = 1;
            } else {
                g_touch_pressed = 0;
                *out_x = map_x(g_last_raw_x);
                *out_y = map_y(g_last_raw_y);
                return -1; // 抬起事件
            }
        }
    } else if (ev->type == EV_KEY) {
        if (ev->code == BTN_TOUCH) {
            g_touch_pressed = ev->value;
            if (g_touch_pressed == 0) {
                *out_x = map_x(g_last_raw_x);
                *out_y = map_y(g_last_raw_y);
                return -1; // 抬起
            }
        }
    } else if (ev->type == EV_SYN) {
        if (ev->code == SYN_REPORT && g_touch_pressed) {
            if (g_last_raw_x >= 0 && g_last_raw_y >= 0) {
                *out_x = map_x(g_last_raw_x);
                *out_y = map_y(g_last_raw_y);
                return 1; // 按下/移动
            }
        }
    }
    
    return 0;
}
