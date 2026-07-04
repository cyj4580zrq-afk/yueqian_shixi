#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <stdio.h>
#include <stdint.h>
#include "../DRMwrap.h"

#define __LOG_FLAG              1
#define __DRM_FLAG              1

#if __LOG_FLAG
#define __LOG_EN
#endif

#if __DRM_FLAG
#define __DRM_DISPLAY
#endif

#ifndef __DRM_DISPLAY 
    #define RK1808_LCD_DEV      "/dev/fb0"
#else
    #define RK1808_LCD_DEV      "/dev/dri/card0"
#endif

#ifdef __DRM_DISPLAY
extern struct drmHandle DRM;
#endif

int  RK1808_DEVICE_Init(void);
void RK1808_DEVICE_DEL(int lcd_fd);

#ifdef __DRM_DISPLAY
void DRM_Color_Display(int lcd_fd, int color);
void DRM_Draw_Line(int lcd_fd, int line_len, int line_wide, int line_color, int x, int y);
int  DRM_Draw_Bmp(int lcd_fd, char *pathname, int x, int y);
#else
void Color_Display(int lcd_fd, int color);
void Draw_Line(int lcd_fd, int line_len, int line_wide, int line_color, int x, int y);
#endif

#endif /* _DISPLAY_H_ */
