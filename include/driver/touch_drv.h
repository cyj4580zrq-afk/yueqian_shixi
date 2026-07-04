#ifndef DRIVER_TOUCH_DRV_H
#define DRIVER_TOUCH_DRV_H

#include <linux/input.h>

// 初始化触摸设备并返回 fd 句柄
int touch_drv_init(const char *touch_device);

// 关闭触摸设备
void touch_drv_close(int fd);

// 非阻塞/阻塞读取触摸输入事件
int touch_drv_read(int fd, struct input_event *ev);

#endif // DRIVER_TOUCH_DRV_H
