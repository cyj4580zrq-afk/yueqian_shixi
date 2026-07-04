#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char const *argv[])
{
    if (argc < 3) {
        printf("使用方法: %s <服务器IP> <端口号>\n", argv[0]);
        return -1;
    }

    // 1. 创建套接字
    int jackfd = socket(AF_INET, SOCK_STREAM, 0);
    if (jackfd == -1) {
        perror("socket init error");
        return -1;
    }
    printf("客户端套接字创建成功: %d\n", jackfd);

    // 2. 配置服务器 IP 地址和端口号并请求连接
    struct sockaddr_in Rose_addr;
    bzero(&Rose_addr, sizeof(Rose_addr));
    Rose_addr.sin_family = AF_INET;
    Rose_addr.sin_port = htons(atoi(argv[2]));      // 设置服务器端口号
    inet_pton(AF_INET, argv[1], &Rose_addr.sin_addr); // 将服务器 IP 转为网络字节序

    printf("正在尝试连接服务器 %s:%s...\n", argv[1], argv[2]);
    int ret = connect(jackfd, (struct sockaddr*)&Rose_addr, sizeof(Rose_addr));
    if (ret == 0) {
        printf("请求连接成功！你可以开始输入数据发送给服务器了。\n");
    } else {
        perror("连接服务器失败");
        close(jackfd);
        return -1;
    }
    
    // 3. 循环获取标准输入并发送
    char buf[50];
    while (1) {
        printf("请输入发送的数据 (输入 exit 退出): ");
        fflush(stdout);
        bzero(buf, sizeof(buf));
        
        // 从终端获取一行输入
        if (scanf("%s", buf) == EOF) break;
        
        // 发送给服务器 (包含字符串结尾 \0，所以长度 +1)
        write(jackfd, buf, strlen(buf) + 1);
        
        if (!strncmp(buf, "exit", 4)) {
            printf("退出客户端！\n");
            break;
        }
    }
    
    // 4. 关闭客户端
    close(jackfd);
    return 0;
}
