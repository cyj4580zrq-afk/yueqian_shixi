#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main()
{
    char pathname[50] = "./1.txt";
    // 1. 以只读形式打开输入文件 1.txt
    int fd = open(pathname, O_RDONLY);
    if(fd == -1)
    {
        perror("打开 1.txt 失败");
        return -1;
    }
    printf("成功打开 1.txt!\n");

    // 2. 创建一个临时的播放音频文件
    int out_fd = open("novel_temp.wav", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out_fd == -1) {
        perror("创建音频临时文件失败");
        close(fd);
        return -1;
    }

    // 3. 循环读取 1.txt 的所有内容并写入 novel_temp.wav
    char buf[1024];
    int read_size = 0;
    int total_size = 0;

    while ((read_size = read(fd, buf, sizeof(buf))) > 0) {
        write(out_fd, buf, read_size);
        total_size += read_size;
    }

    close(fd);
    close(out_fd);
    
    printf("读取小说数据完成，共读取 %d 字节数据。\n", total_size);
    
    // 4. 调用开发板的播放器播放生成的 wav 音频
    printf("正在使用 aplay 播放小说音频...\n");
    system("aplay novel_temp.wav");
    
    printf("小说播放结束！\n");
    return 0;
}
