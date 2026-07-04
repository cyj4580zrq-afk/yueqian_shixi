#include "ui.h"

static void draw_time_and_ip(void)
{
    char date_buf[32];
    char time_buf[32];
    time_t now = time(NULL);
    struct tm tm_now;

    localtime_r(&now, &tm_now);
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &tm_now);
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_now);
    draw_text(770, 14, time_buf, 0xFFFFFFFF, 2);
    draw_text(770, 38, date_buf, 0xFFD7E1EA, 1);
    draw_text(520, 20, connected_ssid[0] ? connected_ssid : "WiFi: 未连接", 0xFFFFFFFF, 1);
    draw_text(520, 40, local_ip[0] ? local_ip : "IP: 获取中...", 0xFF8EB2D3, 1);
}

void draw_pixel(int x, int y, uint32_t color)
{
    uint32_t *fb;
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H || !DRM.vaddr) return;
    fb = (uint32_t *)DRM.vaddr;
    fb[y * SCREEN_W + x] = color;
}

void draw_rect(int x1, int y1, int x2, int y2, uint32_t color)
{
    int x, y;
    if (x1 > x2 || y1 > y2) return;
    for (y = y1; y <= y2; ++y) for (x = x1; x <= x2; ++x) draw_pixel(x, y, color);
}

void draw_rect_round(int x1, int y1, int x2, int y2, int r, uint32_t color)
{
    int x, y;
    for (y = y1; y <= y2; ++y) {
        for (x = x1; x <= x2; ++x) {
            int draw = 0;
            if (x < x1 + r && y < y1 + r) {
                int dx = x - (x1 + r), dy = y - (y1 + r);
                draw = (dx * dx + dy * dy <= r * r);
            } else if (x > x2 - r && y < y1 + r) {
                int dx = x - (x2 - r), dy = y - (y1 + r);
                draw = (dx * dx + dy * dy <= r * r);
            } else if (x < x1 + r && y > y2 - r) {
                int dx = x - (x1 + r), dy = y - (y2 - r);
                draw = (dx * dx + dy * dy <= r * r);
            } else if (x > x2 - r && y > y2 - r) {
                int dx = x - (x2 - r), dy = y - (y2 - r);
                draw = (dx * dx + dy * dy <= r * r);
            } else draw = 1;
            if (draw) draw_pixel(x, y, color);
        }
    }
}

void draw_text(int x, int y, const char *str, uint32_t color, int scale)
{
    if (!str) return;
    DRM_Draw_Text(lcd_fb, &DRM, str, scale * 14, color, x, y);
}

void draw_bmp(int dx, int dy, int dw, int dh, const char *path)
{
    FILE *fp = fopen(path, "rb");
    unsigned char header[54];
    int w, h, bpp, row_size, data_off;
    unsigned char *row;
    int x, y;
    if (!fp) return;
    if (fread(header, 1, sizeof(header), fp) != sizeof(header)) { fclose(fp); return; }
    w = *(int *)(header + 18);
    h = *(int *)(header + 22);
    bpp = *(short *)(header + 28);
    data_off = *(int *)(header + 10);
    if ((bpp != 24 && bpp != 32) || w <= 0 || h <= 0) { fclose(fp); return; }
    row_size = ((w * (bpp / 8) + 3) / 4) * 4;
    row = (unsigned char *)malloc(row_size);
    if (!row) { fclose(fp); return; }
    fseek(fp, data_off, SEEK_SET);
    for (y = 0; y < h && y < dh; ++y) {
        if (fread(row, 1, row_size, fp) != (size_t)row_size) break;
        for (x = 0; x < w && x < dw; ++x) {
            int px = dx + x;
            int py = dy + dh - 1 - y;
            uint32_t c = (bpp == 24) ? (uint32_t)(row[x * 3] | (row[x * 3 + 1] << 8) | (row[x * 3 + 2] << 16)) : *(uint32_t *)(row + x * 4);
            draw_pixel(px, py, c);
        }
    }
    free(row);
    fclose(fp);
}

void draw_card(int x1, int y1, int x2, int y2, const char *title)
{
    draw_rect_round(x1, y1, x2, y2, 12, 0xFF173049);
    draw_rect_round(x1 + 2, y1 + 2, x2 - 2, y2 - 2, 10, 0xFF0D2235);
    if (title && title[0]) draw_text(x1 + 16, y1 + 12, title, 0xFFFFFFFF, 2);
}

void draw_button(rect_t r, const char *label, uint32_t fill)
{
    draw_rect_round(r.x1, r.y1, r.x2, r.y2, 9, fill);
    draw_text(r.x1 + 16, r.y1 + 12, label, 0xFFFFFFFF, 2);
}

void draw_top_bar(const char *title, int show_back)
{
    draw_rect(0, 0, SCREEN_W - 1, 61, 0xFF041421);
    draw_rect(0, 61, SCREEN_W - 1, 62, 0xFF1F4060);
    if (show_back) draw_button(back_btn, "< 返回", 0xFF11589E);
    draw_text(show_back ? 126 : 22, 14, title, 0xFFFFFFFF, 3);
    draw_time_and_ip();
}

void draw_sidebar(int active_page)
{
    static const char *items[] = {"首页", "AI交互", "天气查询", "图片查询", "文件查询", "设置"};
    int i;
    draw_rect(0, 63, 160, SCREEN_H - 1, 0xFF061828);
    draw_rect(159, 63, 160, SCREEN_H - 1, 0xFF214F77);
    for (i = 0; i < 6; ++i) {
        draw_rect_round(sidebar_btns[i].x1, sidebar_btns[i].y1, sidebar_btns[i].x2, sidebar_btns[i].y2, 10, active_page == i ? 0xFF1369DD : 0xFF0F2133);
        draw_text(sidebar_btns[i].x1 + 24, sidebar_btns[i].y1 + 14, items[i], 0xFFFFFFFF, 2);
    }
}

void draw_status_footer(void)
{
    draw_rect(160, 576, SCREEN_W - 1, SCREEN_H - 1, 0xFF081A2B);
    draw_text(180, 580, status_msg, 0xFF9AB8D5, 1);
}
