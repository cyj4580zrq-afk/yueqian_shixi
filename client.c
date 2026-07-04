#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <fcntl.h>
#include "./DRMwrap.h"

#define PORT 50001
#define CONTROL_PORT 8888
#define BUFFER_SIZE 1024
#define FILE_MAGIC 0x57415621  // "WAV!"
#define DRM_DEVICE "/dev/dri/card0"

struct drmHandle DRM;
int lcd_fb = -1;

int DRM_LCD_DEV_Init(void) {
    int fb = open(DRM_DEVICE, O_RDWR);
    if (fb == -1) {
        perror("[DRM] Open file error");
        return -1;
    }
    DRMinit(fb);
    DRMcreateFB(fb, &DRM);
    return fb;
}

void DRM_LCD_DEV_Del(int fb) {
    if (fb < 3) return;
    DRMfreeResources(fb, &DRM);
    close(fb);
}

void DRM_Color_Show(int fb, int R, int G, int B) {
    char *share = (char *)DRM.vaddr;
    if (!share) return;
    for (int x = 0; x < 1024; x++) {
        for (int y = 0; y < 600; y++) {
            int pos = 4 * (1024 * y + x);
            share[pos + 0] = B;
            share[pos + 1] = G;
            share[pos + 2] = R;
            share[pos + 3] = 0;
        }
    }
    DRMshowUp(fb, &DRM);
}

// Thread function that listens for incoming commands from the WSL server on port 8888
void *control_listener_thread(void *arg) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("[Control Listener] Socket creation failed");
        return NULL;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(CONTROL_PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("[Control Listener] Bind failed");
        close(server_fd);
        return NULL;
    }

    if (listen(server_fd, 3) < 0) {
        perror("[Control Listener] Listen failed");
        close(server_fd);
        return NULL;
    }

    printf("[Control Listener] Ready on port %d to receive commands...\n", CONTROL_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &addrlen);
        if (client_sock < 0) {
            perror("[Control Listener] Accept failed");
            continue;
        }

        char buffer[1024] = {0};
        int len = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (len > 0) {
            buffer[len] = '\0';
            printf("\n==============================================\n");
            printf(" [Received Command from AI]: %s\n", buffer);
            printf("==============================================\n\n");
            
            // Simulating hardware execution & LCD display update
            if (strstr(buffer, "打开LED灯") != NULL || strstr(buffer, "开灯") != NULL) {
                printf("[Hardware] --> LED turned ON!\n");
                if (lcd_fb >= 0) {
                    DRM_Color_Show(lcd_fb, 0, 255, 0); // Green color for ON
                }
                send(client_sock, "LED ON SUCCESS", 14, 0);
            } else if (strstr(buffer, "关闭LED灯") != NULL || strstr(buffer, "关灯") != NULL) {
                printf("[Hardware] --> LED turned OFF!\n");
                if (lcd_fb >= 0) {
                    DRM_Color_Show(lcd_fb, 0, 0, 0); // Black color for OFF
                }
                send(client_sock, "LED OFF SUCCESS", 15, 0);
            } else {
                printf("[Hardware] --> Answer: %s\n", buffer);
                if (lcd_fb >= 0) {
                    DRM_Color_Show(lcd_fb, 0, 0, 255); // Blue color for generic command/dialog
                }
                send(client_sock, "COMMAND RECEIVED", 16, 0);
            }
        }
        close(client_sock);
    }

    close(server_fd);
    return NULL;
}

int Socket_Send_FILE(const char *filename, int server_sock) {
    int filefd = open(filename, 0); // O_RDONLY
    if (filefd < 0) {
        printf("[Client] Failed to open local audio file %s\n", filename);
        return -1;
    }

    int file_size = lseek(filefd, 0, 2); // SEEK_END
    lseek(filefd, 0, 0); // SEEK_SET
    printf("[Client] Sending file %s (%d bytes)...\n", filename, file_size);

    // Send magic number and size
    struct {
        uint32_t magic;
        uint32_t file_size;
    } header;
    header.magic = htonl(FILE_MAGIC);
    header.file_size = htonl(file_size);
    
    if (send(server_sock, &header, sizeof(header), 0) != sizeof(header)) {
        perror("[Client] Failed to send header");
        close(filefd);
        return -1;
    }

    char buffer[1024];
    int read_bytes;
    while ((read_bytes = read(filefd, buffer, sizeof(buffer))) > 0) {
        send(server_sock, buffer, read_bytes, 0);
    }
    close(filefd);

    // Read ACK from server
    char ack;
    if (recv(server_sock, &ack, 1, 0) > 0 && ack == 'A') {
        printf("[Client] Server acknowledged file reception.\n");
        return 0;
    }
    
    printf("[Client] Failed to receive acknowledgment.\n");
    return -1;
}

int main(int argc, char *argv[]) {
    // We default to the host's actual IP address: 10.200.67.238
    const char *server_ip = "10.200.67.238";
    if (argc >= 2) {
        server_ip = argv[1];
    }

    printf("====================================================\n");
    printf(" Board client starting. Target Server: %s:%d\n", server_ip, PORT);
    printf("====================================================\n");

    // Initialize DRM LCD Screen
    lcd_fb = DRM_LCD_DEV_Init();
    if (lcd_fb >= 0) {
        printf("[DRM] LCD initialized successfully. Resolution: %dx%d\n", DRM.width, DRM.height);
        DRM_Color_Show(lcd_fb, 0, 0, 0); // Turn black by default
    } else {
        printf("[DRM] LCD initialization failed. Continuing without screen.\n");
    }

    // Start the control listener thread on port 8888
    pthread_t listener_tid;
    if (pthread_create(&listener_tid, NULL, control_listener_thread, NULL) != 0) {
        perror("Failed to create control listener thread");
        if (lcd_fb >= 0) DRM_LCD_DEV_Del(lcd_fb);
        exit(EXIT_FAILURE);
    }
    pthread_detach(listener_tid);

    while (1) {
        printf("\nPress [Enter] to start recording 3s of voice...");
        getchar();

        // Call arecord to capture audio from development board microphone
        printf("[Client] Recording...\n");
        system("arecord -d3 -c1 -r16000 -twav -fS16_LE cmd.wav");
        printf("[Client] Recording finished.\n");

        // Establish TCP connection to WSL server
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("Socket creation failed");
            continue;
        }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT);
        if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
            perror("Invalid server IP address");
            close(sock);
            continue;
        }

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connection to WSL server failed");
            close(sock);
            continue;
        }

        // Send the WAV file
        Socket_Send_FILE("cmd.wav", sock);
        close(sock);
    }

    if (lcd_fb >= 0) {
        DRM_LCD_DEV_Del(lcd_fb);
    }
    return 0;
}

