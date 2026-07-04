#ifndef HAL_DISPLAY_HAL_H
#define HAL_DISPLAY_HAL_H

#include <stdint.h>

// 清屏（填充全黑）
void display_hal_draw_clear(void);

// 画普通文字
void display_hal_draw_text(int x, int y, const char *text, uint32_t color);

// 画圆角卡片
void display_hal_draw_card(int x, int y, int w, int h, uint32_t bg_color, const char *title, const char *content);

// 画交互按钮
void display_hal_draw_button(int x, int y, int w, int h, uint32_t fill_color, const char *text, uint32_t text_color);

// 设置当前页面索引，用于左侧导航高亮
void display_hal_set_current_page(int page_index);

// 绘制板端背景主界面框架（包含顶部栏、底部栏、左侧菜单栏）
void display_hal_draw_framework(const char *wifi_ssid, const char *ip_addr);

// 渲染 BMP 图案文件
int display_hal_draw_bmp(int x, int y, const char *bmp_path);
int display_hal_draw_bmp_scaled(int x, int y, int area_w, int area_h, const char *bmp_path, int zoom_percent);

#endif // HAL_DISPLAY_HAL_H



