#define STB_TRUETYPE_IMPLEMENTATION
#include "../inc/stb_truetype.h"
#include "../inc/font_display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* 内部全局变量 */
static unsigned char *g_ttf_buffer = NULL;   // 整个字体文件映射到内存
static stbtt_fontinfo g_font;
static int g_font_inited = 0;

/* UTF-8 解码，同之前 */
static unsigned int utf8_decode(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    unsigned int cp = 0;
    if (p[0] < 0x80) {
        cp = p[0];
        *s += 1;
    } else if ((p[0] & 0xE0) == 0xC0) {
        cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        *s += 2;
    } else if ((p[0] & 0xF0) == 0xE0) {
        cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        *s += 3;
    } else if ((p[0] & 0xF8) == 0xF0) {
        cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) |
             ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        *s += 4;
    } else {
        *s += 1;  // 非法
        cp = 0xFFFD;
    }
    return cp;
}

int font_init(const char *font_path)
{
    if (g_font_inited) return 0;

    /* 以二进制方式读取整个字体文件 */
    FILE *fp = fopen(font_path, "rb");
    if (!fp) {
        printf("Failed to open font file: %s\n", font_path);
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    g_ttf_buffer = (unsigned char *)malloc(size);
    if (!g_ttf_buffer) {
        fclose(fp);
        return -1;
    }
    fread(g_ttf_buffer, 1, size, fp);
    fclose(fp);

    /* 初始化 stb_truetype */
    int offset = stbtt_GetFontOffsetForIndex(g_ttf_buffer, 0); // 通常索引 0
    if (offset < 0 || !stbtt_InitFont(&g_font, g_ttf_buffer, offset)) {
        printf("Failed to init font (invalid ttf?)\n");
        free(g_ttf_buffer);
        g_ttf_buffer = NULL;
        return -1;
    }

    g_font_inited = 1;
    printf("Font loaded: %s\n", font_path);
    return 0;
}

void font_done(void)
{
    if (g_ttf_buffer) {
        free(g_ttf_buffer);
        g_ttf_buffer = NULL;
    }
    g_font_inited = 0;
}

void DRM_Draw_Text(int lcd_fd, struct drmHandle *drm,
                   const char *text, int font_size,
                   uint32_t color, int x, int y)
{
    if (!g_font_inited || !text || !drm || !drm->vaddr) return;

    int screen_w = drm->width;
    int screen_h = drm->height;
    int pitch = drm->pitch;
    unsigned char *vaddr = (unsigned char *)drm->vaddr;

    /* 解析颜色 */
    unsigned char fr = (color >> 16) & 0xFF;
    unsigned char fg = (color >> 8)  & 0xFF;
    unsigned char fb = color & 0xFF;

    /* 计算缩放比例：stb_truetype 内部坐标以 1pt 为单位，font_size 为像素高度 */
    float scale = stbtt_ScaleForPixelHeight(&g_font, font_size);

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&g_font, &ascent, &descent, &lineGap);
    int baseline = (int)(ascent * scale);  // 基线到顶部的像素距离

    int pen_x = x;
    int pen_y = y + baseline;  // 基线位置

    const char *p = text;
    while (*p) {
        if (*p == '\n') {
            pen_x = x;
            pen_y += font_size;  // 换行
            p++;
            continue;
        }

        unsigned int cp = utf8_decode(&p);

        /* 获取字形度量 */
        int advanceWidth, leftBearing;
        stbtt_GetCodepointHMetrics(&g_font, cp, &advanceWidth, &leftBearing);
        int ax = (int)(advanceWidth * scale);

        /* 渲染字形位图（灰度 0~255） */
        int c_x1, c_y1, c_x2, c_y2;
        stbtt_GetCodepointBitmapBox(&g_font, cp, scale, scale,
                                    &c_x1, &c_y1, &c_x2, &c_y2);
        int bw = c_x2 - c_x1;
        int bh = c_y2 - c_y1;
        if (bw <= 0 || bh <= 0) {
            pen_x += ax;  // 空格等无位图，直接前进
            continue;
        }

        unsigned char *bitmap = malloc(bw * bh);
        if (!bitmap) continue;
        stbtt_MakeCodepointBitmap(&g_font, bitmap, bw, bh, bw,
                                   scale, scale, cp);

        /* 直接混合到 framebuffer (vaddr) */
        int draw_x = pen_x + c_x1;
        int draw_y = pen_y + c_y1;
        for (int row = 0; row < bh; row++) {
            int real_y = draw_y + row;
            if (real_y < 0 || real_y >= screen_h) continue;
            for (int col = 0; col < bw; col++) {
                int real_x = draw_x + col;
                if (real_x < 0 || real_x >= screen_w) continue;
                unsigned char alpha = bitmap[row * bw + col];
                if (alpha == 0) continue;

                unsigned char *dst = vaddr + real_y * pitch + real_x * 4;
                unsigned char bg_b = dst[0];
                unsigned char bg_g = dst[1];
                unsigned char bg_r = dst[2];

                if (alpha == 255) {
                    dst[0] = fb; dst[1] = fg; dst[2] = fr;
                } else {
                    dst[0] = (fb * alpha + bg_b * (255 - alpha)) / 255;
                    dst[1] = (fg * alpha + bg_g * (255 - alpha)) / 255;
                    dst[2] = (fr * alpha + bg_r * (255 - alpha)) / 255;
                }
            }
        }
        free(bitmap);

        pen_x += ax;
        // 简单自动换行
        if (pen_x + font_size > screen_w) {
            pen_x = x;
            pen_y += font_size;
        }
    }
}