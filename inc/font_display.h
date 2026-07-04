#ifndef _FONT_DISPLAY_H_
#define _FONT_DISPLAY_H_

#include <stdint.h>
#include "../DRMwrap.h"   // 你的 DRM 结构体定义

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化字体引擎，加载指定 ttf 文件 */
int font_init(const char *font_path);

/* 释放字体资源 */
void font_done(void);

/* 在 DRM 屏幕上绘制 UTF-8 字符串（支持中文）
   lcd_fd:     DRM 设备文件描述符
   drm:        DRM 结构体指针
   text:       UTF-8 字符串
   font_size:  字体大小（像素）
   color:      前景色，格式 0xRRGGBB
   x, y:       起始坐标（像素） */
void DRM_Draw_Text(int lcd_fd, struct drmHandle *drm,
                   const char *text, int font_size,
                   uint32_t color, int x, int y);

#ifdef __cplusplus
}
#endif

#endif