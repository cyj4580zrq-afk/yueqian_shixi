#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <sys/stat.h>

#define PORT 50001
#define BUFFER_SIZE 4096
#define BACKLOG 5
#define TIMEOUT_SEC 300
#define FILE_MAGIC 0x57415621  // "WAV!"

int recv_wav(char *Path)
{  
    const char *filename = Path;
    
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 端口重用
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 超时设置
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d...\n", PORT);
    printf("Waiting for GEC6818/RK1808 sender...\n");
    
    int addrlen = sizeof(address);
    int client_sock = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
    if (client_sock < 0) {
        perror("Accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &address.sin_addr, client_ip, INET_ADDRSTRLEN);
    printf("Connection from %s\n", client_ip);

    // 接收文件头
    struct {
        uint32_t magic;
        uint32_t file_size;
    } header;
    
    if (recv(client_sock, &header, sizeof(header), MSG_WAITALL) != sizeof(header)) {
        perror("Header receive failed");
        close(client_sock);
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    uint32_t magic = ntohl(header.magic);
    uint32_t file_size = ntohl(header.file_size);
    
    if (magic != FILE_MAGIC) {
        fprintf(stderr, "Invalid magic: 0x%08X (expected 0x%08X)\n", magic, FILE_MAGIC);
        close(client_sock);
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Receiving file: %u bytes (0x%08X)\n", file_size, file_size);

    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("File creation failed");
        close(client_sock);
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 接收文件内容
    char buffer[BUFFER_SIZE];
    uint32_t total_received = 0;
    time_t start_time = time(NULL);
    time_t last_update = start_time;
    
    while (total_received < file_size) {
        size_t to_receive = (file_size - total_received) > BUFFER_SIZE 
                          ? BUFFER_SIZE : (file_size - total_received);
                          
        ssize_t bytes_recv = recv(client_sock, buffer, to_receive, 0);
        
        if (bytes_recv < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                perror("Receive timeout");
                break;
            }
            perror("Receive error");
            break;
        }
        
        if (bytes_recv == 0) {
            printf("\nConnection closed\n");
            break;
        }
        
        size_t written = fwrite(buffer, 1, bytes_recv, file);
        if (written != bytes_recv) {
            perror("File write error");
            break;
        }
        
        total_received += bytes_recv;
        
        // 更新进度
        time_t now = time(NULL);
        if (now - last_update >= 1 || total_received == file_size) {
            float elapsed = difftime(now, start_time);
            float speed = (total_received / 1024.0) / (elapsed > 0 ? elapsed : 1);
            printf("\rReceived: %u/%u bytes (%.1f%%, %.1f KB/s)", 
                   total_received, file_size, 
                   (float)total_received/file_size * 100, 
                   speed);
            fflush(stdout);
            last_update = now;
        }
    }

    // 发送确认
    char ack = 'A';
    send(client_sock, &ack, 1, 0);
    
    printf("\nTransfer completed: %u bytes\n", total_received);
    
    fclose(file);
    close(client_sock);
    close(server_fd);
    
    // 完整性检查
    if (total_received == file_size) {
        printf("File saved: %s\n", filename);
        FILE *f = fopen(filename, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long actual_size = ftell(f);
            fclose(f);
            
            if (actual_size == file_size) {
                printf("Verification passed: %ld bytes\n", actual_size);
            } else {
                printf("WARNING: File size mismatch! Expected %u, got %ld\n", 
                       file_size, actual_size);
            }
        }
    } else {
        printf("ERROR: Incomplete transfer! Expected %u, received %u\n", 
               file_size, total_received);
        remove(filename);
        printf("Partial file removed\n");
        return -1;
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <save_name>\n", argv[0]);
        return -1;
    }
    
    // 自动创建 wav 目录
    mkdir("wav", 0777);

    // 拼接成 wav/文件名 路径
    char save_path[256];
    snprintf(save_path, sizeof(save_path), "wav/%s", argv[1]);
    printf("音频保存目标路径: [%s]\n", save_path);
    
    return recv_wav(save_path);
}
