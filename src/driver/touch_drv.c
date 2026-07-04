#include "driver/touch_drv.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int touch_drv_init(const char *touch_device) {
    // 采用 O_NONBLOCK 非阻塞模式打开，实现流畅的无卡顿 UI 轮询
    int fd = open(touch_device, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("[Touch Driver] Open failed");
        return -1;
    }
    return fd;
}

void touch_drv_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

int touch_drv_read(int fd, struct input_event *ev) {
    if (fd < 0) return -1;
    
    int ret = read(fd, ev, sizeof(struct input_event));
    if (ret == sizeof(struct input_event)) {
        return 0; // 成功读到一个事件
    }
    
    // 如果是因为没有事件可读 (EAGAIN / EWOULDBLOCK)，不认为出错，返回 -2 提示空闲
    if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return -2;
    }
    
    return -1; // 真正的读取错误
}
