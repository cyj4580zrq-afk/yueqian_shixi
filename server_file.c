#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define READ_SIZE       1024        //单次读取文件数据的大小

typedef struct TCP_Server_File
{
    char file_name[50];
    int file_size;
}recv_head_info;

int Socket_Recv_FILE(int jackfd)
{
    //接收对方发来的文件名与文件大小
    recv_head_info head_info;
    bzero(&head_info,sizeof(head_info));
    recv(jackfd,&head_info,sizeof(head_info),0);
    printf("接收的文件信息：\nhead_info:<file_name> [%s]\n\t  <file_size> [%d]\n",head_info.file_name,head_info.file_size);

    // 确保本地存在 wav/ 目录，没有则创建
    mkdir("wav", 0777);

    // 自动拼接成 wav/文件名 路径
    char save_path[256];
    snprintf(save_path, sizeof(save_path), "wav/%s", head_info.file_name);
    printf("保存的目标路径: [%s]\n", save_path);

    char recv_msg[READ_SIZE];
    int recvnum = 0,recv_size = 0;
    int filefd = open(save_path, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (filefd == -1)
    {
        printf("open %s fail!\n", save_path);
        return -1;
    }

    while (1)
    {
        bzero(recv_msg, READ_SIZE);
        recvnum = read(jackfd,recv_msg,READ_SIZE);
        if (recvnum <= 0)
        {
            break;
        }
        recv_size += recvnum;
        write(filefd,recv_msg,recvnum);
        
        if (recv_size >= head_info.file_size)
        {
            break;
        }
    }
    
    if (recv_size != head_info.file_size)
    {
        printf("recv file error!\n\t<recv_size>:[%d]|<file_size>:[%d]\n",recv_size,head_info.file_size);
        close(filefd);
        return -1;
    }
    else
    {
        printf("文件接收完成! 已成功保存至: %s\n", save_path);
    }
    
    close(filefd);
    return 0;
}

int main(int argc, char const *argv[])
{
    if (argc < 2) {
        printf("使用方法: %s <端口号>\n", argv[0]);
        return -1;
    }

    int rosefd = socket(AF_INET, SOCK_STREAM, 0);
    if (rosefd == -1) {
        perror("socket init error");
        return -1;
    }

    int opt = 1;
    setsockopt(rosefd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in Rose_addr;
    bzero(&Rose_addr, sizeof(Rose_addr));
    Rose_addr.sin_family = AF_INET;
    Rose_addr.sin_port   = htons(atoi(argv[1]));
    Rose_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(rosefd, (struct sockaddr *)&Rose_addr, sizeof(Rose_addr)) < 0) {
        perror("bind error");
        close(rosefd);
        return -1;
    }

    listen(rosefd, 5);
    printf("文件传输服务端已启动，等待连接...\n");

    struct sockaddr_in Jack_addr;
    socklen_t addrlen = sizeof(Jack_addr);
    int confd = accept(rosefd, (struct sockaddr *)&Jack_addr, &addrlen);
    if (confd == -1) {
        perror("accept error");
        close(rosefd);
        return -1;
    }
    printf("客户端连接成功，准备开始接收文件...\n");

    Socket_Recv_FILE(confd);

    close(confd);
    close(rosefd);
    return 0;
}
