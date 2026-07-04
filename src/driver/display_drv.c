#include "driver/display_drv.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

// 声明外部 RK1808 DRM 库接口
extern int DRMinit(int fd);
extern int DRMcreateFB(int fd, drm_fb_t *fb);
extern void DRMfreeResources(int fd, drm_fb_t *fb);
extern void DRMshowUp(int fd, drm_fb_t *fb);

// 全局变量实现
uint32_t *g_lcd_fb = NULL;
drm_fb_t DRM;
static int g_drm_fd = -1;

int display_drv_init(const char *drm_dev) {
    g_drm_fd = open(drm_dev, O_RDWR);
    if (g_drm_fd < 0) {
        perror("[DRM Driver] Open failed");
        return -1;
    }
    
    // 兼容 Rockchip 专有 libDRMwrap 库的非标准返回值设计（不进行阻断式校验）
    DRMinit(g_drm_fd);
    
    // 初始化宽高属性 (RK1808 默认是 1024x600)
    DRM.width = 1024;
    DRM.height = 600;
    
    if (DRMcreateFB(g_drm_fd, &DRM) < 0) {
        perror("[DRM Driver] DRMcreateFB failed");
        close(g_drm_fd);
        g_drm_fd = -1;
        return -1;
    }
    
    // 把映射后的 vaddr 赋给全局 g_lcd_fb 绘图缓冲区
    g_lcd_fb = DRM.vaddr;
    printf("[DRM Driver] Initialized successfully. FB Mapped at %p (Width:%d Height:%d)\n", g_lcd_fb, DRM.width, DRM.height);
    return 0;
}

void display_drv_exit(void) {
    if (g_drm_fd >= 0) {
        DRMfreeResources(g_drm_fd, &DRM);
        close(g_drm_fd);
        g_drm_fd = -1;
        g_lcd_fb = NULL;
    }
}

void display_drv_flush(void) {
    if (g_drm_fd >= 0) {
        DRMshowUp(g_drm_fd, &DRM);
    }
}

int display_drv_get_width(void) {
    return DRM.width;
}

int display_drv_get_height(void) {
    return DRM.height;
}
