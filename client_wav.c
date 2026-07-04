#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdint.h>

#define PORT 50001
#define BUFFER_SIZE 1024
#define FILE_MAGIC 0x57415621  // "WAV!"

int send_wav(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Usage: %s <server_ip> <filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    const char *server_ip = argv[1];
    const char *filename = argv[2];
    
    FILE *file = fopen(filename, "rb");
    if (!file) {
        char path[256];
        snprintf(path, sizeof(path), "/mnt/sdcard/%s", filename);
        file = fopen(path, "rb");
        if (!file) {
            perror("File open failed");
            exit(EXIT_FAILURE);
        }
    }

    fseek(file, 0, SEEK_END);
    uint32_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    printf("File size: %u bytes (0x%08X)\n", file_size, file_size);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        struct hostent *he = gethostbyname(server_ip);
        if (!he) {
            perror("Address resolution failed");
            close(sock);
            fclose(file);
            exit(EXIT_FAILURE);
        }
        memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
    }

    printf("Connecting to %s:%d...\n", server_ip, PORT);
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr))) {
        perror("Connection failed");
        close(sock);
        fclose(file);
        exit(EXIT_FAILURE);
    }
    printf("Connection established\n");

    struct {
        uint32_t magic;
        uint32_t file_size;
    } header;
    
    header.magic = htonl(FILE_MAGIC);
    header.file_size = htonl(file_size);
    
    if (send(sock, &header, sizeof(header), 0) != sizeof(header)) {
        perror("Failed to send header");
        close(sock);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    uint32_t total_sent = 0;
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        ssize_t sent = send(sock, buffer, bytes_read, 0);
        if (sent < 0) {
            perror("Send failed");
            break;
        }
        total_sent += sent;
        printf("\rSent: %u/%u bytes (%.1f%%)", 
               total_sent, file_size, (float)total_sent/file_size * 100);
        fflush(stdout);
    }

    printf("\nFile sent successfully (%u bytes)\n", total_sent);
    
    char ack;
    if (recv(sock, &ack, 1, 0) <= 0) {
        perror("Ack not received");
    } else {
        printf("Transfer confirmed\n");
    }
    
    fclose(file);
    close(sock);
    return 0;
}

int main(int argc, char *argv[]) {
    return send_wav(argc, argv);
}
