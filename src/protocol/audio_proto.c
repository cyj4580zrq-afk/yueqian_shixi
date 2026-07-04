#include "protocol/audio_proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

static int g_audio_listen_fd = -1;
static pthread_t g_audio_thread_id;
static char g_audio_save_dir[256] = "/tmp";
static audio_proto_callback_t g_audio_cb = NULL;

static void *audio_listen_thread_func(void *arg) {
    int port = (int)(long)arg;
    g_audio_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_audio_listen_fd < 0) return NULL;
    
    int opt = 1;
    setsockopt(g_audio_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(g_audio_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(g_audio_listen_fd);
        g_audio_listen_fd = -1;
        return NULL;
    }
    
    if (listen(g_audio_listen_fd, 5) < 0) {
        close(g_audio_listen_fd);
        g_audio_listen_fd = -1;
        return NULL;
    }
    
    while (1) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int csock = accept(g_audio_listen_fd, (struct sockaddr *)&caddr, &clen);
        if (csock < 0) break;
        
        char dest_path[512];
        long total = 0;
        int recv_error = 0;
        snprintf(dest_path, sizeof(dest_path), "%s/reply.wav", g_audio_save_dir);
        printf("[AudioProto] accepted wav connection from %s:%d\n", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
        
        FILE *fp = fopen(dest_path, "wb");
        if (fp) {
            char buf[4096];
            int len;
            while ((len = recv(csock, buf, sizeof(buf), 0)) > 0) {
                if (fwrite(buf, 1, len, fp) != (size_t)len) {
                    recv_error = 1;
                    printf("[AudioProto] write failed errno=%d bytes=%ld\n", errno, total);
                    break;
                }
                total += len;
            }
            if (len < 0) {
                recv_error = 1;
                printf("[AudioProto] recv failed errno=%d bytes=%ld\n", errno, total);
            }
            fclose(fp);
            printf("[AudioProto] wav saved path=%s bytes=%ld error=%d\n", dest_path, total, recv_error);
            
            if (!recv_error && total > 44 && g_audio_cb) {
                g_audio_cb(dest_path);
            } else {
                printf("[AudioProto] ignore invalid wav bytes=%ld error=%d\n", total, recv_error);
            }
        } else {
            printf("[AudioProto] open failed path=%s errno=%d\n", dest_path, errno);
        }
        close(csock);
    }
    
    close(g_audio_listen_fd);
    g_audio_listen_fd = -1;
    return NULL;
}

int audio_proto_start_listen(int port, const char *save_dir, audio_proto_callback_t cb) {
    strncpy(g_audio_save_dir, save_dir, sizeof(g_audio_save_dir) - 1);
    g_audio_save_dir[sizeof(g_audio_save_dir) - 1] = '\0';
    g_audio_cb = cb;
    
    if (pthread_create(&g_audio_thread_id, NULL, audio_listen_thread_func, (void *)(long)port) != 0) {
        return -1;
    }
    return 0;
}

void audio_proto_stop_listen(void) {
    if (g_audio_listen_fd >= 0) {
        shutdown(g_audio_listen_fd, SHUT_RDWR);
        close(g_audio_listen_fd);
        g_audio_listen_fd = -1;
    }
    pthread_join(g_audio_thread_id, NULL);
}

