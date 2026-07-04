#include "protocol/control_proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

static int g_listen_fd = -1;
static pthread_t g_thread_id;
static control_proto_callback_t g_callback = NULL;

static void *listen_thread_func(void *arg) {
    int port = (int)(long)arg;
    g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_fd < 0) return NULL;
    
    int opt = 1;
    setsockopt(g_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(g_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(g_listen_fd);
        g_listen_fd = -1;
        return NULL;
    }
    
    if (listen(g_listen_fd, 5) < 0) {
        close(g_listen_fd);
        g_listen_fd = -1;
        return NULL;
    }
    
    while (1) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int csock = accept(g_listen_fd, (struct sockaddr *)&caddr, &clen);
        if (csock < 0) break;
        
        char buf[1024];
        int len = recv(csock, buf, sizeof(buf) - 1, 0);
        if (len > 0) {
            buf[len] = '\0';
            
            char *colon = strchr(buf, ':');
            if (colon) {
                *colon = '\0';
                char *prefix = buf;
                char *msg = colon + 1;
                if (g_callback) {
                    g_callback(prefix, msg);
                }
            }
        }
        close(csock);
    }
    
    close(g_listen_fd);
    g_listen_fd = -1;
    return NULL;
}

int control_proto_start_listen(int port, control_proto_callback_t cb) {
    g_callback = cb;
    if (pthread_create(&g_thread_id, NULL, listen_thread_func, (void *)(long)port) != 0) {
        return -1;
    }
    return 0;
}

void control_proto_stop_listen(void) {
    if (g_listen_fd >= 0) {
        shutdown(g_listen_fd, SHUT_RDWR);
        close(g_listen_fd);
        g_listen_fd = -1;
    }
    pthread_join(g_thread_id, NULL);
}
