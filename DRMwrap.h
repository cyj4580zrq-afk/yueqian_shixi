#ifndef _DRM_WRAP_H_
#define _DRM_WRAP_H_

#include <stdint.h>

struct drmHandle {
    uint32_t width;      // 屏幕宽度
    uint32_t height;     // 屏幕高度
    uint32_t pitch;      // 屏幕 pitch
    uint32_t handle;     // 显存 bo 句柄
    uint32_t size;       // 显存大小
    uint32_t padding;    // 对齐字段
    void *vaddr;         // 显存虚拟映射地址
    uint32_t fb_id;      // frame buffer ID
};

int DRMinit(int fd);
int DRMcreateFB(int fd, struct drmHandle *drm);
void DRMfreeResources(int fd, struct drmHandle *drm);
void DRMshowUp(int fd, struct drmHandle *drm);

#endif
