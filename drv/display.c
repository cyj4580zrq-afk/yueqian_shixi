#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "../inc/display.h"

/* ==================== 全局变量定义 ==================== */
#ifdef __DRM_DISPLAY
extern struct drmHandle DRM;
#endif

/* ==================== 设备初始化 / 销毁 ==================== */
int RK1808_DEVICE_Init(void)
{
    int lcd_fd = open(RK1808_LCD_DEV, O_RDWR);
    if (lcd_fd == -1) {
#ifdef __LOG_EN
        printf("open %s failed!\n", RK1808_LCD_DEV);
#endif
        return -1;
    }
#ifdef __LOG_EN
    printf("open %s successfully!\n", RK1808_LCD_DEV);
#endif

#ifdef __DRM_DISPLAY
    DRMinit(lcd_fd);
    DRMcreateFB(lcd_fd, &DRM);
    printf("__DRM_Init() successfully!\n");
#endif
    return lcd_fd;
}

void RK1808_DEVICE_DEL(int lcd_fd)
{
    if (lcd_fd == -1) return;
#ifdef __DRM_DISPLAY
    DRMfreeResources(lcd_fd, &DRM);
#endif
    close(lcd_fd);
}

/* ==================== DRM 绘图函数 ==================== */
#ifdef __DRM_DISPLAY

void DRM_Color_Display(int lcd_fd, int color)
{
    unsigned char *lcd_data = (unsigned char *)DRM.vaddr;
    for (int j = 0; j < DRM.height; j++)
        for (int i = 0; i < DRM.width; i++)
        {
            int offset = 4 * i + 4 * j * DRM.width;
            lcd_data[offset + 0] = color;          // B
            lcd_data[offset + 1] = (color >> 8);   // G
            lcd_data[offset + 2] = (color >> 16);  // R
            lcd_data[offset + 3] = (color >> 24);  // A
        }
    DRMshowUp(lcd_fd, &DRM);
#ifdef __LOG_EN
    printf("DRM_Color_Display() successfully!\n");
#endif
}

void DRM_Draw_Line(int lcd_fd, int line_len, int line_wide, int line_color, int x, int y)
{
    if (lcd_fd == -1 || line_len == 0 || line_wide == 0 || line_color > 0xFFFFFF)
        return;

    int screen_w = DRM.width;
    int screen_h = DRM.height;

    int line_x = x + line_len;
    if (line_x > screen_w) line_x = screen_w;
    int line_y = y + line_wide;
    if (line_y > screen_h) line_y = screen_h;
    if (x < 0 || y < 0 || x >= screen_w || y >= screen_h) return;

    char lcd_tmp[screen_w * screen_h * 4];
    memcpy(lcd_tmp, DRM.vaddr, sizeof(lcd_tmp));

    for (int j = y; j < line_y; j++)
        for (int i = x; i < line_x; i++)
        {
            int offset = 4 * i + 4 * j * screen_w;
            lcd_tmp[offset + 0] = line_color;
            lcd_tmp[offset + 1] = (line_color >> 8);
            lcd_tmp[offset + 2] = (line_color >> 16);
            lcd_tmp[offset + 3] = (line_color >> 24);
        }

    memcpy(DRM.vaddr, lcd_tmp, sizeof(lcd_tmp));
    DRMshowUp(lcd_fd, &DRM);
#ifdef __LOG_EN
    printf("DRM_Draw_Line() successfully!\n");
#endif
}

int DRM_Draw_Bmp(int lcd_fd, char *pathname, int x, int y)
{
    int bmp_fd = open(pathname, O_RDWR);
    if (bmp_fd == -1) {
#ifdef __LOG_EN
        printf("open %s failed!\n", pathname);
#endif
        return -1;
    }

    char Head[54];
    read(bmp_fd, Head, 54);

    int len, wide, size;
    memcpy(&len,  &Head[18], sizeof(int));
    memcpy(&wide, &Head[22], sizeof(int));
    memcpy(&size, &Head[34], sizeof(int));

    char bmp_data[len * wide * 4];   // 足量缓冲，实际 24bit 用 3 字节
    read(bmp_fd, bmp_data, sizeof(bmp_data));
    close(bmp_fd);

#ifdef __LOG_EN
    int bpp = DRM.pitch / DRM.width * 8;
    printf("\t|__显示器尺寸:%d x %d\n", DRM.width, DRM.height);
    printf("\t|__显示器色深:%d\n", bpp);
    printf("\t|__图片尺寸:%d x %d\n", len, wide);
    printf("\t|__图片大小:%d\n", size);
#endif

    int screen_w = DRM.width;
    int screen_h = DRM.height;
    char lcd_tmp[screen_w * screen_h * 4];
    memcpy(lcd_tmp, DRM.vaddr, sizeof(lcd_tmp));

    for (int j = y; j < screen_h && (j - y) < wide; j++)
        for (int i = x; i < screen_w && (i - x) < len; i++)
        {
            int src_x = i - x;
            int src_y = (wide - 1) - (j - y);    // BMP 垂直翻转
            int src_offset = 3 * src_x + 3 * src_y * len;
            int dst_offset = 4 * i + 4 * j * screen_w;

            lcd_tmp[dst_offset + 0] = bmp_data[src_offset + 0]; // B
            lcd_tmp[dst_offset + 1] = bmp_data[src_offset + 1]; // G
            lcd_tmp[dst_offset + 2] = bmp_data[src_offset + 2]; // R
            lcd_tmp[dst_offset + 3] = 0;                         // A
        }

    memcpy(DRM.vaddr, lcd_tmp, sizeof(lcd_tmp));
    DRMshowUp(lcd_fd, &DRM);
#ifdef __LOG_EN
    printf("DRM_Draw_Bmp() successfully!\n");
#endif
    return 0;
}

#endif /* __DRM_DISPLAY */
