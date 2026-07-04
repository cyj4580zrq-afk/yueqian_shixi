/*
1.注册drm设备
2.打开图像文件
3.虚拟内存映射
4.将图像数据写入到虚拟内存当中
5.释放虚拟内存，关闭打开的文件
*/
//DRM刷新全屏颜色
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "./DRMwrap.h"

#define DRM_DEVICE      "/dev/dri/card0"


//需要创建一个DRM结构体
struct drmHandle DRM;

int DRM_LCD_DEV_Init(void);
void DRM_LCD_DEV_Del(int fb);
void DRM_Color_Show(int fb, int R, int G, int B);


int main(){
    int lcd_fb = DRM_LCD_DEV_Init();
    if (lcd_fb < 0) return -1;
    
    DRM_Color_Show(lcd_fb, 255, 255, 255);
    
    //关闭DRM映射和LCD驱动文件
    DRM_LCD_DEV_Del(lcd_fb);
    return 0;
}


int DRM_LCD_DEV_Init(void)
{
    //1、打开文件
    int fb = open(DRM_DEVICE,O_RDWR);
    if(fb == -1){
        perror("open file error");
        return -1;
    }
    //2、对DRM进行初始化
    DRMinit(fb);
    //3、创建一个虚拟屏幕并将DRM与这个虚拟屏幕进行绑定
    DRMcreateFB(fb,&DRM);
    return fb;
}

void DRM_LCD_DEV_Del(int fb)
{
    if (fb < 3) return;
    DRMfreeResources(fb,&DRM);
    close(fb);
}


void DRM_Color_Show(int fb, int R, int G, int B)
{
    //4、获取一下虚拟屏幕的显存地址,share就相当于我们文件IO创建的数组
    char * share = DRM.vaddr;
    //5、写入颜色数据
    for(int x = 0;x<1024;x++){
        for(int y = 0;y<600;y++){
            int pos = 4*(1024*y+x);
            share[pos+0] = B;
            share[pos+1] = G;
            share[pos+2] = R;
            share[pos+3] = 0;
        }
    }
    //6、利用函数展示
    while(1){
        DRMshowUp(fb,&DRM);//更新显存
    }
}

