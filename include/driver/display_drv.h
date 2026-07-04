#ifndef DRIVER_DISPLAY_DRV_H
#define DRIVER_DISPLAY_DRV_H

#include <stdint.h>

// 彻底还原官方原汁原味的 struct drmHandle 布局，消除任何因为结构体内存对齐及字段错位造成的段错误崩溃
struct drmHandle {
    uint32_t width;      // 屏幕宽度
    uint32_t height;     // 屏幕高度
    uint32_t pitch;      // 屏幕 pitch
    uint32_t handle;     // bo 句柄
    uint32_t size;       // 显存大小
    uint32_t padding;    // 对齐字段
    void *vaddr;         // 显存虚拟映射地址
    uint32_t fb_id;      // frame buffer ID
};

// 保持别名定义以兼容其他代码
typedef struct drmHandle drm_fb_t;

extern struct drmHandle DRM;
extern uint32_t *g_lcd_fb;

// 初始化 DRM 设备并映射显存
int display_drv_init(const char *device_path);

// 释放显存映射并关闭设备
void display_drv_exit(void);

// 刷新显存数据到 LCD 屏幕上呈现
void display_drv_flush(void);

// 获取宽高
int display_drv_get_width(void);
int display_drv_get_height(void);

#endif // DRIVER_DISPLAY_DRV_H
