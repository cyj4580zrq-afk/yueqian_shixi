#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define READ_SIZE       1024        // 恢复为 1024 字节包

typedef struct TCP_Cilent
{
    char file_name[50];
    int file_size;
}send_head_info;

int Socket_Send_FILE(char *File_Name, int jackfd)
{
    char send_msg[READ_SIZE];
    int readnum = 0;
    int filefd = open(File_Name, O_RDONLY);
    if (filefd == -1)
    {
        printf("open %s fail!\n",File_Name);
        return -1;
    }

    int file_size = lseek(filefd,0,SEEK_END);
    printf("发送的文件信息：\n《%s》 <file_size>:<%d>byte\n",File_Name,file_size);
    send_head_info head_info;
    
    char *base_name = strrchr(File_Name, '/');
    if (base_name) {
        strcpy(head_info.file_name, base_name + 1);
    } else {
        strcpy(head_info.file_name, File_Name);
    }
    head_info.file_size = file_size;

    printf("head_info:<file_name> [%s]\n\t  <file_size> [%d]\n",head_info.file_name,head_info.file_size);
    lseek(filefd,0,SEEK_SET);
    send(jackfd,&head_info,sizeof(head_info),0);
    
    int remaining = file_size;
    while (remaining > 0)
    {
        bzero(send_msg, READ_SIZE);
        readnum = read(filefd,send_msg,READ_SIZE);
        if (readnum <= 0)
        {
            break;
        }
        write(jackfd,send_msg,readnum);
        remaining -= readnum;
    }
    printf("文件发送完成!\n");

    close(filefd);
    return 0;
}

int main(int argc, char const *argv[])
{
    if (argc < 4) {
        printf("使用方法: %s <服务器IP> <端口号> <要发送的文件路径>\n", argv[0]);
        return -1;
    }

    int jackfd = socket(AF_INET, SOCK_STREAM, 0);
    if (jackfd == -1) {
        perror("socket init error");
        return -1;
    }

    struct sockaddr_in Rose_addr;
    bzero(&Rose_addr, sizeof(Rose_addr));
    Rose_addr.sin_family = AF_INET;
    Rose_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &Rose_addr.sin_addr);

    printf("正在连接服务器 %s:%s...\n", argv[1], argv[2]);
    int ret = connect(jackfd, (struct sockaddr*)&Rose_addr, sizeof(Rose_addr));
    if (ret != 0) {
        perror("连接服务器失败");
        close(jackfd);
        return -1;
    }
    printf("连接成功，开始发送文件: %s...\n", argv[3]);

    Socket_Send_FILE((char *)argv[3], jackfd);

    close(jackfd);
    return 0;
}
