#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>


#define TOUCH_DEVICE       "/dev/input/event2"

int TOUCH_DEV_Init(void);
void TOUCH_DEV_DEL(int touch_fd);
void Get_xy(int touch_fd);

/* 
    1.打开一个名为 1.txt 文件
        判定程序开启是否异常
    2.使用完文件之后关闭对应的文件
*/
int main(int argc, char const *argv[])
{
    int touch_ret = TOUCH_DEV_Init();

    while (1)
    {
        Get_xy(touch_ret);
    }
    
    TOUCH_DEV_DEL(touch_ret);
    return 0;
}

int TOUCH_DEV_Init(void)
{
    int touch_fd = open(TOUCH_DEVICE, O_RDWR);//以读写权限打开文件
    if (touch_fd == -1)
    {
        printf("文件打开失败!\n");
        perror("open falure:\n\t");
        return -1;
    }
    
    printf("open %s successfully!\n", TOUCH_DEVICE);
    printf("fd: %d\n",touch_fd);
    return touch_fd;
}

void TOUCH_DEV_DEL(int touch_fd)
{
    close(touch_fd);
}


void Get_xy(int touch_fd)
{
    struct input_event ts;
    int tmp_x = 0, tmp_y = 0;
    while (1)
    {
        read(touch_fd, &ts, sizeof(struct input_event));//读取驱动文件内部数据
        if (ts.type == EV_ABS && ts.code == ABS_X)
            tmp_x = ts.value;//x轴事件结果----坐标值
        if (ts.type == EV_ABS && ts.code == ABS_Y)
            tmp_y = ts.value;//y轴事件结果----坐标值  
        if (ts.type == EV_KEY && ts.code == BTN_TOUCH && ts.value == 0)
            break;
    }
    
    printf("(%3d,%3d)\n",tmp_x, tmp_y);
}


