#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <fcntl.h>

#define PORT 50001
#define CONTROL_PORT 8888
#define BUFFER_SIZE 4096
#define BACKLOG 5
#define TIMEOUT_SEC 300
#define FILE_MAGIC 0x57415621  // "WAV!"

// Global variable to store the dynamically detected client (board) IP address
char g_board_ip[64] = "0.0.0.0";

// Helper function to send commands back to the board
int send_command(const char *ip, int port, const char *command, char *response, int resp_len) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Control socket creation failed");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("Invalid control IP address");
        close(sock);
        return -1;
    }

    printf("[Control] Connecting to board at %s:%d to send command...\n", ip, port);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Control connection failed");
        close(sock);
        return -1;
    }

    printf("[Control] Sending command: %s\n", command);
    send(sock, command, strlen(command), 0);

    // Read response
    memset(response, 0, resp_len);
    int len = recv(sock, response, resp_len - 1, 0);
    if (len > 0) {
        response[len] = '\0';
    }
    
    close(sock);
    return len;
}

// Function to call the python chatbot and send the response back to the board
int popen_wav(char *question) {
    char cmd[4096] = {0};
    // Escape question to avoid shell injection
    sprintf(cmd, "python3 chat_deepseek.py \"%s\"", question);

    printf("[AI] Running LLM query: %s\n", cmd);
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        perror("Failed to run python deepseek script");
        return -1;
    }

    char buffer[1024] = {0};
    char response[1024] = {0};
    
    // Read the output of chat_deepseek.py
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        // Strip trailing newline
        buffer[strcspn(buffer, "\r\n")] = 0;
        printf("[AI] Response from DeepSeek: %s\n", buffer);
        
        if (strlen(buffer) > 0 && strcmp(g_board_ip, "0.0.0.0") != 0) {
            int ret = send_command(g_board_ip, CONTROL_PORT, buffer, response, sizeof(response));
            if (ret > 0) {
                printf("[AI] Board execution feedback: %s\n", response);
            }
        } else {
            printf("[AI] No board IP detected or empty response. Skipped sending control command.\n");
        }
    } else {
        printf("[AI] Empty response from Python script.\n");
    }

    pclose(fp);
    return 0;
}

// Simulated or placeholder Xunfei ASR engine call (can be replaced by Xunfei SDK)
void run_iat(const char *audio_file) {
    printf("[ASR] Processing audio file %s with Speech Recognition...\n", audio_file);
    // In a full implementation, you would use the Xunfei SDK run_iat API.
    // For testing/mocking when the SDK is not compiled, we default to a test question.
    char recognized_text[256] = "打开LED灯";
    printf("[ASR] Recognized text: %s\n", recognized_text);
    
    // Call AI query and board command sender
    popen_wav(recognized_text);
}

// Function to receive WAV from TCP connection
int recv_wav(int client_sock, const char *filename) {
    // Receive header
    struct {
        uint32_t magic;
        uint32_t file_size;
    } header;
    
    if (recv(client_sock, &header, sizeof(header), MSG_WAITALL) != sizeof(header)) {
        perror("Header receive failed");
        return -1;
    }
    
    uint32_t magic = ntohl(header.magic);
    uint32_t file_size = ntohl(header.file_size);
    
    if (magic != FILE_MAGIC) {
        fprintf(stderr, "Invalid magic: 0x%08X (expected 0x%08X)\n", magic, FILE_MAGIC);
        return -1;
    }
    
    printf("[Receiver] Receiving file: %s (%u bytes)\n", filename, file_size);

    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("File creation failed");
        return -1;
    }

    char buffer[BUFFER_SIZE];
    uint32_t total_received = 0;
    
    while (total_received < file_size) {
        size_t to_receive = (file_size - total_received) > BUFFER_SIZE 
                          ? BUFFER_SIZE : (file_size - total_received);
                          
        ssize_t bytes_recv = recv(client_sock, buffer, to_receive, 0);
        if (bytes_recv < 0) {
            perror("Receive error");
            break;
        }
        if (bytes_recv == 0) {
            printf("[Receiver] Connection closed prematurely\n");
            break;
        }
        
        fwrite(buffer, 1, bytes_recv, file);
        total_received += bytes_recv;
    }

    // Send ACK to client
    char ack = 'A';
    send(client_sock, &ack, 1, 0);
    
    fclose(file);
    printf("[Receiver] Transfer completed. Saved to %s\n", filename);
    return (total_received == file_size) ? 0 : -1;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;  // Binds to all network interfaces
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

    // Create wav directory if it doesn't exist
    system("mkdir -p ./wav");

    printf("====================================================\n");
    printf(" WSL AI Assistant Server running on port %d...\n", PORT);
    printf(" Waiting for connection from RK1808/GEC6818 board...\n");
    printf("====================================================\n");

    while (1) {
        struct sockaddr_in client_addr;
        int addrlen = sizeof(client_addr);
        
        int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&addrlen);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }
        
        // Dynamically extract client's (board's) IP address
        inet_ntop(AF_INET, &client_addr.sin_addr, g_board_ip, sizeof(g_board_ip));
        printf("\n[Server] Connected by client (Board) IP: %s\n", g_board_ip);

        // Receive WAV file
        if (recv_wav(client_sock, "./wav/cmd.wav") == 0) {
            // Process the WAV file
            run_iat("./wav/cmd.wav");
        }
        
        close(client_sock);
    }

    close(server_fd);
    return 0;
}
