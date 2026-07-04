#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main(int argc, char const *argv[])
{
    if (argc < 2) {
        printf("使用方法: %s <端口号>\n", argv[0]);
        return -1;
    }

    // 1. 创建未连接的套接字
    int rosefd = socket(AF_INET, SOCK_STREAM, 0);
    if (rosefd == -1) {
        perror("socket init error");
        return -1;
    }
    printf("服务端监听套接字创建成功: %d\n", rosefd);

    // 2. 绑定端口号和 IP 地址
    struct sockaddr_in Rose_addr;
    bzero(&Rose_addr, sizeof(Rose_addr));
    Rose_addr.sin_family = AF_INET;
    Rose_addr.sin_port   = htons(atoi(argv[1])); // 端口号由命令行参数传入
    Rose_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 绑定本机所有IP

    if (bind(rosefd, (struct sockaddr *)&Rose_addr, sizeof(Rose_addr)) < 0) {
        perror("bind error");
        close(rosefd);
        return -1;
    }

    // 3. 设置监听
    listen(rosefd, 5);
    printf("等待客户端连接中...\n");

    // 4. 等待客户端连接 (阻塞)
    struct sockaddr_in Jack_addr;
    socklen_t addrlen = sizeof(Jack_addr);
    int confd = accept(rosefd, (struct sockaddr *)&Jack_addr, &addrlen);
    if (confd == -1) {
        perror("accept error");
        close(rosefd);
        return -1;
    }
    printf("客户端连接成功！分配的通信套接字: %d\n", confd);

    // 5. 循环接收客户端数据
    char recv_data[50];
    while (1) {
        bzero(recv_data, sizeof(recv_data));
        int read_len = read(confd, recv_data, sizeof(recv_data));
        if (read_len <= 0) {
            printf("客户端断开连接。\n");
            break;
        }
        printf("收到来自开发板的数据: [ %s ]\n", recv_data);
        
        // 如果收到 exit 则退出
        if (!strncmp(recv_data, "exit", 4)) {
            printf("收到退出指令！\n");
            break;
        }
    }
    
    // 6. 关闭连接
    close(confd);
    close(rosefd);
    printf("服务器关闭。\n");

    return 0;
}
