#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>

#define SERVER_PORT 8888
#define UPLOAD_PORT 8890
#define WEATHER_PORT 8891
#define PIPELINE_SCRIPT "/home/cyj/workspace/ai_assistant/scripts/ai_pipeline.py"
#define PIPELINE_LOG "/tmp/ai_pipeline.log"
#define APP_CONFIG_PATH "/home/cyj/workspace/ai_assistant/config/app_config.json"
#define CMD_WAV_PATH "/home/cyj/workspace/ai_assistant/bin/wav/cmd.wav"
#define CMD_WAV_TMP_PATH "/home/cyj/workspace/ai_assistant/bin/wav/cmd.wav.tmp"
#define WEATHER_SCRIPT "/home/cyj/workspace/ai_assistant/scripts/get_weather.py"
#define WEATHER_CACHE_TEXT "/home/cyj/workspace/ai_assistant/weather_cache.txt"
#define WEATHER_CACHE_JSON "/home/cyj/workspace/ai_assistant/config/weather_cache.json"
#define WEATHER_REFRESH_LOG "/tmp/weather_refresh.log"

static pthread_mutex_t g_weather_refresh_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_weather_refreshing = 0;

static int is_loopback_ip(const char *ip)
{
    return ip && strcmp(ip, "127.0.0.1") == 0;
}

static void load_config_board_ip(char *board_ip, size_t size)
{
    FILE *fp;
    char buf[4096];
    char *key;
    char *colon;
    char *quote1;
    char *quote2;
    size_t len;

    if (!board_ip || size == 0) return;
    board_ip[0] = '\0';

    fp = fopen(APP_CONFIG_PATH, "r");
    if (!fp) return;

    len = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[len] = '\0';

    key = strstr(buf, "\"board_ip\"");
    if (!key) return;
    colon = strchr(key, ':');
    if (!colon) return;
    quote1 = strchr(colon, '"');
    if (!quote1) return;
    quote2 = strchr(quote1 + 1, '"');
    if (!quote2) return;

    len = (size_t)(quote2 - quote1 - 1);
    if (len >= size) len = size - 1;
    memcpy(board_ip, quote1 + 1, len);
    board_ip[len] = '\0';
}

static void select_board_ip(const char *peer_ip, char *board_ip, size_t size)
{
    char config_board_ip[64] = "";

    if (!board_ip || size == 0) return;
    board_ip[0] = '\0';

    if (peer_ip && peer_ip[0] && !is_loopback_ip(peer_ip)) {
        snprintf(board_ip, size, "%s", peer_ip);
        return;
    }

    load_config_board_ip(config_board_ip, sizeof(config_board_ip));
    if (config_board_ip[0]) {
        snprintf(board_ip, size, "%s", config_board_ip);
        return;
    }
}

static int receive_wav_file(int csock, const char *path)
{
    FILE *fp = fopen(CMD_WAV_TMP_PATH, "wb");
    char buf[4096];
    long total = 0;

    if (!fp) {
        printf("[Upload] open failed path=%s errno=%d\n", CMD_WAV_TMP_PATH, errno);
        return -1;
    }

    while (1) {
        ssize_t n = recv(csock, buf, sizeof(buf), 0);
        if (n == 0) break;
        if (n < 0) {
            printf("[Upload] recv failed errno=%d bytes=%ld\n", errno, total);
            fclose(fp);
            unlink(CMD_WAV_TMP_PATH);
            return -2;
        }
        if (fwrite(buf, 1, (size_t)n, fp) != (size_t)n) {
            printf("[Upload] write failed errno=%d bytes=%ld\n", errno, total);
            fclose(fp);
            unlink(CMD_WAV_TMP_PATH);
            return -3;
        }
        total += n;
    }

    fclose(fp);

    if (total <= 44) {
        printf("[Upload] file too small bytes=%ld\n", total);
        unlink(CMD_WAV_TMP_PATH);
        return -4;
    }

    if (rename(CMD_WAV_TMP_PATH, path) != 0) {
        printf("[Upload] rename failed tmp=%s path=%s errno=%d\n", CMD_WAV_TMP_PATH, path, errno);
        unlink(CMD_WAV_TMP_PATH);
        return -5;
    }

    printf("[Upload] wav saved path=%s bytes=%ld\n", path, total);
    return 0;
}

static void launch_pipeline_async(const char *board_ip)
{
    char cmd[1024];
    if (!board_ip || !board_ip[0]) return;
    snprintf(cmd, sizeof(cmd),
             "sh -lc 'python3 %s %s >>%s 2>&1 &'",
             PIPELINE_SCRIPT, board_ip, PIPELINE_LOG);
    (void)system(cmd);
}

static void *upload_server_thread(void *arg)
{
    int listen_fd;
    int opt = 1;
    struct sockaddr_in addr;

    (void)arg;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        printf("[Upload] socket failed errno=%d\n", errno);
        return NULL;
    }

    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(UPLOAD_PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("[Upload] bind failed port=%d errno=%d\n", UPLOAD_PORT, errno);
        close(listen_fd);
        return NULL;
    }

    if (listen(listen_fd, 5) < 0) {
        printf("[Upload] listen failed port=%d errno=%d\n", UPLOAD_PORT, errno);
        close(listen_fd);
        return NULL;
    }

    printf("WSL WAV Upload Server running on port %d...\n", UPLOAD_PORT);
    fflush(stdout);

    while (1) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int csock = accept(listen_fd, (struct sockaddr *)&caddr, &clen);
        char board_ip_buf[64] = "";
        const char *peer_ip;

        if (csock < 0) continue;

        peer_ip = inet_ntoa(caddr.sin_addr);
        select_board_ip(peer_ip, board_ip_buf, sizeof(board_ip_buf));
        printf("[Upload] received peer_ip: %s\n", peer_ip ? peer_ip : "(null)");
        printf("[Upload] selected board_ip: %s\n", board_ip_buf[0] ? board_ip_buf : "(empty)");
        printf("[Upload] wav target path: %s\n", CMD_WAV_PATH);
        fflush(stdout);

        if (receive_wav_file(csock, CMD_WAV_PATH) == 0 && board_ip_buf[0]) {
            printf("[Upload] triggering AI pipeline asynchronously for board_ip=%s\n", board_ip_buf);
            fflush(stdout);
            launch_pipeline_async(board_ip_buf);
        } else {
            printf("[Upload] upload failed or board_ip empty, pipeline not triggered.\n");
            fflush(stdout);
        }

        close(csock);
    }

    close(listen_fd);
    return NULL;
}

static void trim_newline(char *text)
{
    size_t len;
    if (!text) return;
    len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
        text[--len] = '\0';
    }
}

static int read_first_line(const char *path, char *out, size_t out_size)
{
    FILE *fp;
    if (!out || out_size == 0) return -1;
    out[0] = '\0';
    fp = fopen(path, "r");
    if (!fp) return -1;
    if (!fgets(out, (int)out_size, fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    trim_newline(out);
    return out[0] ? 0 : -1;
}

static void *weather_refresh_worker(void *arg)
{
    char cmd[512];
    (void)arg;

    snprintf(cmd, sizeof(cmd), "python3 %s >%s 2>&1", WEATHER_SCRIPT, WEATHER_REFRESH_LOG);
    printf("[WeatherTCP] refresh start: %s\n", cmd);
    fflush(stdout);
    (void)system(cmd);
    printf("[WeatherTCP] refresh done. log=%s\n", WEATHER_REFRESH_LOG);
    fflush(stdout);

    pthread_mutex_lock(&g_weather_refresh_mutex);
    g_weather_refreshing = 0;
    pthread_mutex_unlock(&g_weather_refresh_mutex);
    return NULL;
}

static int read_json_string_value(const char *path, const char *key, char *out, size_t out_size)
{
    FILE *fp;
    char buf[4096];
    char pattern[128];
    char *pos;
    char *colon;
    char *quote1;
    char *quote2;
    size_t len;

    if (!out || out_size == 0) return -1;
    out[0] = '\0';
    fp = fopen(path, "r");
    if (!fp) return -1;
    len = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[len] = '\0';

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    pos = strstr(buf, pattern);
    if (!pos) return -1;
    colon = strchr(pos, ':');
    if (!colon) return -1;
    quote1 = strchr(colon + 1, '"');
    if (!quote1) return -1;
    quote2 = strchr(quote1 + 1, '"');
    if (!quote2 || quote2 <= quote1) return -1;

    len = (size_t)(quote2 - quote1 - 1);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, quote1 + 1, len);
    out[len] = '\0';
    return out[0] ? 0 : -1;
}
static void refresh_weather_cache_async(void)
{
    pthread_t tid;
    pthread_mutex_lock(&g_weather_refresh_mutex);
    if (g_weather_refreshing) {
        pthread_mutex_unlock(&g_weather_refresh_mutex);
        printf("[WeatherTCP] refresh already running, skip duplicate request.\n");
        fflush(stdout);
        return;
    }
    g_weather_refreshing = 1;
    pthread_mutex_unlock(&g_weather_refresh_mutex);

    if (pthread_create(&tid, NULL, weather_refresh_worker, NULL) == 0) {
        pthread_detach(tid);
    } else {
        pthread_mutex_lock(&g_weather_refresh_mutex);
        g_weather_refreshing = 0;
        pthread_mutex_unlock(&g_weather_refresh_mutex);
        printf("[WeatherTCP] failed to start refresh worker.\n");
        fflush(stdout);
    }
}

static void *weather_server_thread(void *arg)
{
    int listen_fd;
    int opt = 1;
    struct sockaddr_in addr;

    (void)arg;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        printf("[WeatherTCP] socket failed errno=%d\n", errno);
        return NULL;
    }
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(WEATHER_PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("[WeatherTCP] bind failed port=%d errno=%d\n", WEATHER_PORT, errno);
        close(listen_fd);
        return NULL;
    }
    if (listen(listen_fd, 5) < 0) {
        printf("[WeatherTCP] listen failed port=%d errno=%d\n", WEATHER_PORT, errno);
        close(listen_fd);
        return NULL;
    }

    printf("WSL Weather TCP Server running on port %d...\n", WEATHER_PORT);
    fflush(stdout);

    while (1) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int csock = accept(listen_fd, (struct sockaddr *)&caddr, &clen);
        char line[256] = "";
        const char *reply;

        if (csock < 0) continue;
        printf("[WeatherTCP] request from %s:%d\n", inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));

        if (read_first_line(WEATHER_CACHE_TEXT, line, sizeof(line)) != 0) {
            snprintf(line, sizeof(line), "weather fetch failed");
        } else if (!strstr(line, "|更新时间:")) {
            char update_time[64] = "";
            if (read_json_string_value(WEATHER_CACHE_JSON, "update_time", update_time, sizeof(update_time)) == 0) {
                size_t used = strlen(line);
                snprintf(line + used, sizeof(line) - used, "|更新时间:%s", update_time);
            }
        }
        reply = line;
        send(csock, reply, strlen(reply), 0);
        send(csock, "\n", 1, 0);
        close(csock);
        printf("[WeatherTCP] reply: %s\n", line);
        fflush(stdout);

        refresh_weather_cache_async();
    }

    close(listen_fd);
    return NULL;
}
int main(int argc, char **argv)
{
    pthread_t upload_tid;
    pthread_t weather_tid;

    (void)argc;
    (void)argv;

    if (pthread_create(&upload_tid, NULL, upload_server_thread, NULL) == 0) {
        pthread_detach(upload_tid);
    } else {
        printf("[Upload] failed to start upload server thread.\n");
    }
    if (pthread_create(&weather_tid, NULL, weather_server_thread, NULL) == 0) {
        pthread_detach(weather_tid);
    } else {
        printf("[WeatherTCP] failed to start weather server thread.\n");
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        printf("Error creating socket.\n");
        return -1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SERVER_PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("Bind Failed.\n");
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, 5) < 0) {
        printf("Listen Failed.\n");
        close(listen_fd);
        return -1;
    }

    printf("WSL AI Server running on port %d...\n", SERVER_PORT);

    while (1) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int csock = accept(listen_fd, (struct sockaddr *)&caddr, &clen);
        if (csock < 0) continue;

        char board_ip_buf[64] = "";
        char *peer_ip = inet_ntoa(caddr.sin_addr);
        char buf[1024];
        int len = recv(csock, buf, sizeof(buf) - 1, 0);
        close(csock);
        if (len <= 0) continue;
        buf[len] = '\0';

        select_board_ip(peer_ip, board_ip_buf, sizeof(board_ip_buf));
        printf("Received peer_ip: %s\n", peer_ip ? peer_ip : "(null)");
        printf("Selected board_ip: %s\n", board_ip_buf[0] ? board_ip_buf : "(empty)");
        printf("Payload: %s\n", buf);

        if (strcmp(buf, "TRIGGER") != 0) {
            printf("Ignoring non-trigger payload from %s\n", peer_ip ? peer_ip : "(null)");
            fflush(stdout);
            continue;
        }

        if (!board_ip_buf[0]) {
            printf("Ignoring trigger because selected board_ip is empty.\n");
            fflush(stdout);
            continue;
        }

        printf("Triggering AI pipeline asynchronously for board_ip=%s\n", board_ip_buf);
        fflush(stdout);
        launch_pipeline_async(board_ip_buf);
    }

    close(listen_fd);
    return 0;
}






