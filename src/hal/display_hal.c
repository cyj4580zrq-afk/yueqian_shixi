#include "hal/display_hal.h"
#include "driver/display_drv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 声明外部字库接口
extern void DRM_Draw_Text(int lcd_fd, drm_fb_t *drm, const char *text, int font_size, uint32_t color, int x, int y);

static int g_display_current_page = 0;

void display_hal_set_current_page(int page_index) {
    if (page_index < 0) page_index = 0;
    if (page_index > 5) page_index = 5;
    g_display_current_page = page_index;
}

static uint32_t blend_color(uint32_t a, uint32_t b, int t, int max_t)
{
    int ar = (a >> 16) & 0xFF;
    int ag = (a >> 8) & 0xFF;
    int ab = a & 0xFF;
    int br = (b >> 16) & 0xFF;
    int bg = (b >> 8) & 0xFF;
    int bb = b & 0xFF;
    if (max_t <= 0) max_t = 1;
    return (uint32_t)(((ar + (br - ar) * t / max_t) << 16) |
                      ((ag + (bg - ag) * t / max_t) << 8) |
                      (ab + (bb - ab) * t / max_t));
}

// 清屏：低噪声深色渐变背景，保留内容可读性
void display_hal_draw_clear(void) {
    if (!g_lcd_fb) return;
    int screen_w = display_drv_get_width();
    int screen_h = display_drv_get_height();
    if (screen_w <= 0 || screen_h <= 0) {
        screen_w = 1024;
        screen_h = 600;
    }
    for (int y = 0; y < screen_h; y++) {
        uint32_t row = blend_color(0x07101B, 0x102035, y, screen_h - 1);
        for (int x = 0; x < screen_w; x++) {
            uint32_t col = row;
            if (x > 200) {
                int soft = (x - 200) * 18 / (screen_w - 200);
                int r = ((col >> 16) & 0xFF) + soft / 6;
                int g = ((col >> 8) & 0xFF) + soft / 4;
                int b = (col & 0xFF) + soft;
                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;
                col = (uint32_t)((r << 16) | (g << 8) | b);
            }
            if (x > 730 && y < 260) {
                col = blend_color(col, 0x183246, 18, 100);
            }
            g_lcd_fb[y * screen_w + x] = col;
        }
    }
}

// 画矩形区域（带边界防越界保护）
static void draw_rect(int x1, int y1, int x2, int y2, uint32_t color) {
    if (!g_lcd_fb) return;
    int screen_w = display_drv_get_width();
    int screen_h = display_drv_get_height();
    if (screen_w <= 0 || screen_h <= 0) {
        screen_w = 1024;
        screen_h = 600;
    }
    
    if (x1 < 0) x1 = 0;
    if (x2 >= screen_w) x2 = screen_w - 1;
    if (y1 < 0) y1 = 0;
    if (y2 >= screen_h) y2 = screen_h - 1;
    
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            g_lcd_fb[y * screen_w + x] = color;
        }
    }
}
static void draw_hline(int x1, int x2, int y, uint32_t color)
{
    draw_rect(x1, y, x2, y, color);
}

static void draw_vline(int x, int y1, int y2, uint32_t color)
{
    draw_rect(x, y1, x, y2, color);
}

static void draw_border(int x, int y, int w, int h, uint32_t color)
{
    draw_hline(x, x + w, y, color);
    draw_hline(x, x + w, y + h, color);
    draw_vline(x, y, y + h, color);
    draw_vline(x + w, y, y + h, color);
}

// 画文字（带字体初始化安全防护，DRM_Draw_Text内部自带 static 标志安全判断）
void display_hal_draw_text(int x, int y, const char *text, uint32_t color) {
    if (!text) return;
    DRM_Draw_Text(0, &DRM, text, 18, color, x, y);
}

static int utf8_char_len(unsigned char c)
{
    if ((c & 0x80) == 0x00) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static int utf8_char_px(const char *s)
{
    unsigned char c = (unsigned char)s[0];
    if ((c & 0x80) == 0x00) return 9;
    return 18;
}
// 画卡片：克制的深色玻璃面板，少边框、轻阴影
void display_hal_draw_card(int x, int y, int w, int h, uint32_t bg_color, const char *title, const char *content) {
    char title_buf[256];
    uint32_t panel = bg_color ? blend_color(bg_color, 0x0A1420, 42, 100) : 0x101820;

    draw_rect(x + 3, y + 4, x + w + 3, y + h + 4, 0x030912);
    draw_rect(x, y, x + w, y + h, panel);
    draw_rect(x + 1, y + 1, x + w - 1, y + 2, 0x24556A);
    draw_border(x, y, w, h, 0x1E4A5D);

    if (title && title[0]) {
        snprintf(title_buf, sizeof(title_buf), "%s", title);
        draw_rect(x + 15, y + 40, x + 68, y + 42, 0x3CE6C2);
        display_hal_draw_text(x + 15, y + 15, title_buf, 0x8EF7E0);
    }
    
    if (content && content[0]) {
        char tmp[256];
        int len = strlen(content);
        int line_y = y + 50;
        int content_w = w - 30;
        const int line_h = 26;

        for (int i = 0; i < len; ) {
            int chunk = 0;
            int used_px = 0;

            while (i + chunk < len) {
                int clen = utf8_char_len((unsigned char)content[i + chunk]);
                int cpx = utf8_char_px(content + i + chunk);
                if (chunk > 0 && used_px + cpx > content_w) break;
                if (chunk + clen >= (int)sizeof(tmp)) break;
                chunk += clen;
                used_px += cpx;
            }

            if (chunk <= 0) chunk = utf8_char_len((unsigned char)content[i]);
            if (chunk >= (int)sizeof(tmp)) chunk = (int)sizeof(tmp) - 1;
            memcpy(tmp, content + i, chunk);
            tmp[chunk] = '\0';
            display_hal_draw_text(x + 15, line_y, tmp, 0xEAF7FF);
            line_y += line_h;
            i += chunk;
            if (line_y > y + h - 14) break;
        }
    }
}

// 画交互按钮：柔和实体按钮，减少霓虹边框
void display_hal_draw_button(int x, int y, int w, int h, uint32_t fill_color, const char *text, uint32_t text_color) {
    uint32_t body = blend_color(fill_color, 0x0B1724, 18, 100);
    uint32_t highlight = blend_color(body, 0xFFFFFF, 10, 100);
    draw_rect(x + 2, y + 3, x + w + 2, y + h + 3, 0x030912);
    draw_rect(x, y, x + w, y + h, body);
    draw_rect(x + 1, y + 1, x + w - 1, y + 3, highlight);
    draw_border(x, y, w, h, 0x2D6C82);
    display_hal_draw_text(x + 15, y + (h - 20) / 2, text, text_color);
}

// 绘制主屏框架：更干净的顶栏、侧边导航、底部状态栏
void display_hal_draw_framework(const char *wifi_ssid, const char *ip_addr) {
    int screen_w = display_drv_get_width();
    if (screen_w <= 0) screen_w = 1024;

    draw_rect(0, 0, screen_w - 1, 80, 0x0A1420);
    draw_rect(0, 0, screen_w - 1, 2, 0x3CE6C2);
    draw_rect(0, 78, screen_w - 1, 80, 0x1B3E52);
    draw_rect(28, 22, 38, 46, 0x3CE6C2);
    draw_rect(42, 18, 52, 46, 0x5AA7FF);
    display_hal_draw_text(66, 25, "曜灵 AI", 0x8EF7E0);
    
    char net_info[256];
    snprintf(net_info, sizeof(net_info), "无线: %s  |  IP: %s", wifi_ssid[0] ? wifi_ssid : "未连接", ip_addr[0] ? ip_addr : "离线");
    draw_rect(535, 20, screen_w - 34, 54, 0x0E1D2D);
    draw_border(535, 20, screen_w - 569, 34, 0x25485A);
    display_hal_draw_text(550, 28, net_info, 0xD8F3FF);
    
    draw_rect(0, 80, 200, 599, 0x070E18);
    draw_rect(198, 80, 200, 599, 0x173B4E);
    
    static const char *nav_items[] = {
        "1. 首页",
        "2. 语音助手",
        "3. 天气",
        "4. 相册",
        "5. 文件浏览",
        "6. 设置"
    };
    
    for (int i = 0; i < 6; i++) {
        int y = 80 + i * 80;
        int active = (i == g_display_current_page);
        uint32_t bg = active ? 0x102C3A : 0x070E18;
        draw_rect(0, y, 198, y + 78, bg);
        draw_rect(0, y, 198, y + 1, 0x173B57);
        if (active) {
            draw_rect(0, y + 12, 4, y + 66, 0x3CE6C2);
            draw_rect(10, y + 12, 188, y + 66, 0x0C2330);
            display_hal_draw_text(20, y + 25, nav_items[i], 0x8EF7E0);
        } else {
            display_hal_draw_text(20, y + 25, nav_items[i], 0xBFD3DE);
        }
    }
    
    draw_rect(200, 550, screen_w - 1, 599, 0x08131E);
    draw_rect(200, 550, screen_w - 1, 552, 0x183848);
    display_hal_draw_text(220, 565, "系统状态：正常运行。", 0x9BB8D2);
}

// 渲染 BMP 图像到当前 framebuffer
int display_hal_draw_bmp(int x, int y, const char *bmp_path) {
    FILE *fp;
    unsigned char header[54];
    unsigned char *row;
    int width, height, bpp, data_off, row_size;
    int screen_w, screen_h;
    int row_idx, col_idx;

    if (!bmp_path || !g_lcd_fb) return -1;

    fp = fopen(bmp_path, "rb");
    if (!fp) {
        printf("[BMP] Failed to open: %s\n", bmp_path);
        return -1;
    }
    if (fread(header, 1, sizeof(header), fp) != sizeof(header)) {
        printf("[BMP] Invalid header: %s\n", bmp_path);
        fclose(fp);
        return -1;
    }

    width = *(int *)(header + 18);
    height = *(int *)(header + 22);
    bpp = *(short *)(header + 28);
    data_off = *(int *)(header + 10);
    if ((bpp != 24 && bpp != 32) || width <= 0 || height <= 0) {
        printf("[BMP] Unsupported format: %s (bpp=%d)\n", bmp_path, bpp);
        fclose(fp);
        return -1;
    }

    row_size = ((width * (bpp / 8) + 3) / 4) * 4;
    row = (unsigned char *)malloc(row_size);
    if (!row) {
        fclose(fp);
        return -1;
    }

    fseek(fp, data_off, SEEK_SET);
    screen_w = display_drv_get_width();
    screen_h = display_drv_get_height();
    if (screen_w <= 0) screen_w = 1024;
    if (screen_h <= 0) screen_h = 600;

    for (row_idx = 0; row_idx < height; ++row_idx) {
        if (fread(row, 1, row_size, fp) != (size_t)row_size) break;
        for (col_idx = 0; col_idx < width; ++col_idx) {
            int px = x + col_idx;
            int py = y + (height - 1 - row_idx);
            unsigned char b, g, r;
            if (px < 0 || px >= screen_w || py < 0 || py >= screen_h) continue;
            if (bpp == 24) {
                b = row[col_idx * 3 + 0];
                g = row[col_idx * 3 + 1];
                r = row[col_idx * 3 + 2];
            } else {
                unsigned int argb = *(unsigned int *)(row + col_idx * 4);
                b = argb & 0xFF;
                g = (argb >> 8) & 0xFF;
                r = (argb >> 16) & 0xFF;
            }
            g_lcd_fb[py * screen_w + px] = (r << 16) | (g << 8) | b;
        }
    }

    free(row);
    fclose(fp);
    return 0;
}



// 渲染 BMP 到指定区域，zoom_percent=100 表示适配区域，>100 放大居中裁切，<100 缩小居中显示。
int display_hal_draw_bmp_scaled(int x, int y, int area_w, int area_h, const char *bmp_path, int zoom_percent) {
    FILE *fp;
    unsigned char header[54];
    unsigned char *pixels;
    unsigned char *row;
    int width, height, bpp, data_off, row_size, bytes_per_pixel;
    int screen_w, screen_h;
    int row_idx, col_idx;
    int fit_w, fit_h, draw_w, draw_h;
    int dst_x0, dst_y0;

    if (!bmp_path || !g_lcd_fb || area_w <= 0 || area_h <= 0) return -1;
    if (zoom_percent < 25) zoom_percent = 25;
    if (zoom_percent > 400) zoom_percent = 400;

    fp = fopen(bmp_path, "rb");
    if (!fp) {
        printf("[BMP] Failed to open: %s\n", bmp_path);
        return -1;
    }
    if (fread(header, 1, sizeof(header), fp) != sizeof(header)) {
        printf("[BMP] Invalid header: %s\n", bmp_path);
        fclose(fp);
        return -1;
    }

    width = *(int *)(header + 18);
    height = *(int *)(header + 22);
    bpp = *(short *)(header + 28);
    data_off = *(int *)(header + 10);
    if ((bpp != 24 && bpp != 32) || width <= 0 || height <= 0) {
        printf("[BMP] Unsupported format: %s (bpp=%d)\n", bmp_path, bpp);
        fclose(fp);
        return -1;
    }

    bytes_per_pixel = bpp / 8;
    row_size = ((width * bytes_per_pixel + 3) / 4) * 4;
    pixels = (unsigned char *)malloc((size_t)width * height * 3);
    row = (unsigned char *)malloc(row_size);
    if (!pixels || !row) {
        free(pixels);
        free(row);
        fclose(fp);
        return -1;
    }

    fseek(fp, data_off, SEEK_SET);
    for (row_idx = 0; row_idx < height; ++row_idx) {
        if (fread(row, 1, row_size, fp) != (size_t)row_size) break;
        for (col_idx = 0; col_idx < width; ++col_idx) {
            int src_y = height - 1 - row_idx;
            unsigned char b, g, r;
            if (bpp == 24) {
                b = row[col_idx * 3 + 0];
                g = row[col_idx * 3 + 1];
                r = row[col_idx * 3 + 2];
            } else {
                unsigned int argb = *(unsigned int *)(row + col_idx * 4);
                b = argb & 0xFF;
                g = (argb >> 8) & 0xFF;
                r = (argb >> 16) & 0xFF;
            }
            pixels[(src_y * width + col_idx) * 3 + 0] = r;
            pixels[(src_y * width + col_idx) * 3 + 1] = g;
            pixels[(src_y * width + col_idx) * 3 + 2] = b;
        }
    }

    free(row);
    fclose(fp);

    fit_w = area_w;
    fit_h = (height * fit_w) / width;
    if (fit_h > area_h) {
        fit_h = area_h;
        fit_w = (width * fit_h) / height;
    }
    if (fit_w <= 0) fit_w = 1;
    if (fit_h <= 0) fit_h = 1;

    draw_w = fit_w * zoom_percent / 100;
    draw_h = fit_h * zoom_percent / 100;
    if (draw_w <= 0) draw_w = 1;
    if (draw_h <= 0) draw_h = 1;

    dst_x0 = x + (area_w - draw_w) / 2;
    dst_y0 = y + (area_h - draw_h) / 2;

    screen_w = display_drv_get_width();
    screen_h = display_drv_get_height();
    if (screen_w <= 0) screen_w = 1024;
    if (screen_h <= 0) screen_h = 600;

    for (int dy = 0; dy < draw_h; ++dy) {
        int py = dst_y0 + dy;
        int sy = dy * height / draw_h;
        if (py < y || py >= y + area_h || py < 0 || py >= screen_h) continue;
        for (int dx = 0; dx < draw_w; ++dx) {
            int px = dst_x0 + dx;
            int sx = dx * width / draw_w;
            unsigned char *rgb;
            if (px < x || px >= x + area_w || px < 0 || px >= screen_w) continue;
            rgb = pixels + (sy * width + sx) * 3;
            g_lcd_fb[py * screen_w + px] = (rgb[0] << 16) | (rgb[1] << 8) | rgb[2];
        }
    }

    free(pixels);
    return 0;
}





