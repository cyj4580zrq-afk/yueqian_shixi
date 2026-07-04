#ifndef HAL_TOUCH_HAL_H
#define HAL_TOUCH_HAL_H

#include "driver/touch_drv.h"

// 接收 4 个校准坐标参数进行初始化
void touch_hal_init(int min_x, int max_x, int min_y, int max_y);
void touch_hal_exit(void);

// 读取原始输入事件并映射到 1024x600 坐标系
int touch_hal_process_event(struct input_event *ev, int *out_x, int *out_y);

#endif // HAL_TOUCH_HAL_H
