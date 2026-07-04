#include "protocol/weather_client.h"
#include "common/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define WEATHER_LOCAL_CACHE_FILE "config/weather_cache.txt"
#define WEATHER_OUTPUT_SIZE 2048
#define WEATHER_TCP_PORT 8891

static char g_weather_city1_name[64] = "武汉";
static char g_weather_city2_name[64] = "北京";
static char g_weather_update_time[64] = "--";

static void replace_all(char *text, size_t size, const char *from, const char *to)
{
    char buf[256];
    char *pos;
    size_t prefix_len;

    if (!text || !from || !to || !from[0]) return;
    pos = strstr(text, from);
    if (!pos) return;

    prefix_len = (size_t)(pos - text);
    snprintf(buf, sizeof(buf), "%.*s%s%s", (int)prefix_len, text, to, pos + strlen(from));
    strncpy(text, buf, size - 1);
    text[size - 1] = '\0';
}

static void translate_weather_text(char *text, size_t size)
{
    if (!text || size == 0) return;
    replace_all(text, size, "Rain With Thunderstorm", "雷阵雨");
    replace_all(text, size, "Rain with thunderstorm", "雷阵雨");
    replace_all(text, size, "Thunderstorm with rain", "雷阵雨");
    replace_all(text, size, "Patchy light rain with thunder", "局部雷阵雨");
    replace_all(text, size, "Moderate or heavy rain with thunder", "强雷阵雨");
    replace_all(text, size, "Partly Cloudy", "多云");
    replace_all(text, size, "Partly cloudy", "多云");
    replace_all(text, size, "Light Rain Shower", "小阵雨");
    replace_all(text, size, "Light rain shower", "小阵雨");
    replace_all(text, size, "Light rain showers", "小阵雨");
    replace_all(text, size, "Light Rain", "小雨");
    replace_all(text, size, "Light rain", "小雨");
    replace_all(text, size, "Patchy rain nearby", "附近有零星小雨");
    replace_all(text, size, "Cloudy", "阴");
    replace_all(text, size, "Sunny", "晴");
    replace_all(text, size, "Clear", "晴");
    replace_all(text, size, "Haze", "霾");
}
static void normalize_city_name(char *name, size_t size)
{
    if (!name || size == 0) return;
    if (strcmp(name, "Wuhan") == 0) snprintf(name, size, "%s", "武汉");
    else if (strcmp(name, "Beijing") == 0) snprintf(name, size, "%s", "北京");
}

static void copy_weather_part(char *part, char *city_name, int city_name_len, char *weather, int weather_len)
{
    char *colon;
    if (!part || !city_name || !weather) return;
    colon = strchr(part, ':');
    if (colon) {
        *colon = '\0';
        strncpy(city_name, part, city_name_len - 1);
        city_name[city_name_len - 1] = '\0';
        normalize_city_name(city_name, (size_t)city_name_len);
        strncpy(weather, colon + 1, weather_len - 1);
    } else {
        strncpy(weather, part, weather_len - 1);
    }
    weather[weather_len - 1] = '\0';
    translate_weather_text(weather, (size_t)weather_len);
}
static int parse_weather_line(const char *line, char *result_wuhan, int wuhan_len, char *result_beijing, int beijing_len)
{
    char buf[256];
    char *split;

    if (!line || !line[0]) return -1;
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    split = strchr(buf, '|');
    if (!split) return -2;
    *split = '\0';
    char *third = strchr(split + 1, '|');
    if (third) {
        *third = '\0';
        if (strncmp(third + 1, "更新时间:", strlen("更新时间:")) == 0) {
            strncpy(g_weather_update_time, third + 1 + strlen("更新时间:"), sizeof(g_weather_update_time) - 1);
            g_weather_update_time[sizeof(g_weather_update_time) - 1] = '\0';
        }
    }

    copy_weather_part(buf, g_weather_city1_name, sizeof(g_weather_city1_name), result_wuhan, wuhan_len);
    copy_weather_part(split + 1, g_weather_city2_name, sizeof(g_weather_city2_name), result_beijing, beijing_len);
    return 0;
}

void weather_client_get_city_names(char *city1, int city1_len, char *city2, int city2_len)
{
    if (city1 && city1_len > 0) {
        strncpy(city1, g_weather_city1_name, city1_len - 1);
        city1[city1_len - 1] = '\0';
    }
    if (city2 && city2_len > 0) {
        strncpy(city2, g_weather_city2_name, city2_len - 1);
        city2[city2_len - 1] = '\0';
    }
}
void weather_client_get_update_time(char *update_time, int update_time_len)
{
    if (update_time && update_time_len > 0) {
        strncpy(update_time, g_weather_update_time, update_time_len - 1);
        update_time[update_time_len - 1] = '\0';
    }
}
static void trim_text(char *text)
{
    size_t len;

    if (!text) return;
    len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r' || text[len - 1] == ' ' || text[len - 1] == '\t')) {
        text[--len] = '\0';
    }
}

static int run_command_capture(const char *cmd, char *output, size_t output_size)
{
    FILE *fp;
    size_t used = 0;
    int status;

    if (!output || output_size == 0) return -1;
    output[0] = '\0';

    fp = popen(cmd, "r");
    if (!fp) return -1;

    while (used + 1 < output_size && fgets(output + used, (int)(output_size - used), fp)) {
        used = strlen(output);
    }

    status = pclose(fp);
    trim_text(output);
    return status == 0 ? 0 : -1;
}

static int read_weather_cache_file(const char *cache_path, char *result_wuhan, int wuhan_len, char *result_beijing, int beijing_len)
{
    FILE *fp;
    char line[256];

    fp = fopen(cache_path, "r");
    if (!fp) return -1;
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    trim_text(line);
    return parse_weather_line(line, result_wuhan, wuhan_len, result_beijing, beijing_len);
}

static int ends_with(const char *text, const char *suffix)
{
    size_t text_len;
    size_t suffix_len;

    if (!text || !suffix) return 0;
    text_len = strlen(text);
    suffix_len = strlen(suffix);
    if (suffix_len > text_len) return 0;
    return strcmp(text + text_len - suffix_len, suffix) == 0;
}

static void derive_remote_cache_path(const char *script_path, char *cache_path, size_t cache_path_size)
{
    char base_dir[512];
    char *slash;
    size_t len;

    if (!script_path || !script_path[0]) {
        snprintf(cache_path, cache_path_size, "weather_cache.txt");
        return;
    }

    strncpy(base_dir, script_path, sizeof(base_dir) - 1);
    base_dir[sizeof(base_dir) - 1] = '\0';
    slash = strrchr(base_dir, '/');
    if (slash) {
        *slash = '\0';
    }

    len = strlen(base_dir);
    if (len >= 8 && ends_with(base_dir, "/scripts")) {
        base_dir[len - 8] = '\0';
    } else if (len >= 4 && ends_with(base_dir, "/bin")) {
        base_dir[len - 4] = '\0';
    }

    if (!base_dir[0]) {
        snprintf(cache_path, cache_path_size, "weather_cache.txt");
    } else {
        snprintf(cache_path, cache_path_size, "%s/weather_cache.txt", base_dir);
    }
}

static int weather_tcp_fetch(const char *wsl_ip, char *result_wuhan, int wuhan_len, char *result_beijing, int beijing_len)
{
    int sock;
    struct sockaddr_in addr;
    char buf[256];
    int total = 0;

    if (!wsl_ip || !wsl_ip[0]) return -1;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(WEATHER_TCP_PORT);
    if (inet_pton(AF_INET, wsl_ip, &addr.sin_addr) != 1) {
        close(sock);
        return -1;
    }

    printf("[Weather] TCP request target=%s:%d\n", wsl_ip, WEATHER_TCP_PORT);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        printf("[Weather] TCP connect failed errno=%d target=%s:%d\n", errno, wsl_ip, WEATHER_TCP_PORT);
        close(sock);
        return -1;
    }

    while (total + 1 < (int)sizeof(buf)) {
        int n = recv(sock, buf + total, sizeof(buf) - 1 - total, 0);
        if (n <= 0) break;
        total += n;
    }
    close(sock);
    buf[total] = '\0';
    trim_text(buf);
    printf("[Weather] TCP output: %s\n", buf[0] ? buf : "(empty)");

    if (parse_weather_line(buf, result_wuhan, wuhan_len, result_beijing, beijing_len) == 0) {
        FILE *fp = fopen(WEATHER_LOCAL_CACHE_FILE, "w");
        if (fp) {
            fprintf(fp, "%s\n", buf);
            fclose(fp);
        }
        return 0;
    }
    return -1;
}
int weather_client_fetch(const char *wsl_ip, const char *script_path, char *result_wuhan, int wuhan_len, char *result_beijing, int beijing_len)
{
    char ssh_cmd[1024];
    char ssh_output[WEATHER_OUTPUT_SIZE];
    char scp_cmd[1024];
    char scp_output[WEATHER_OUTPUT_SIZE];
    char remote_cache[512];
    const char *wsl_username = g_config.wsl_username[0] ? g_config.wsl_username : "cyj";

    if (!wsl_ip || !wsl_ip[0] || !script_path || !script_path[0]) {
        printf("[Weather] Invalid WSL config. ip=%s script=%s\n",
               wsl_ip ? wsl_ip : "(null)",
               script_path ? script_path : "(null)");
        return -2;
    }
    if (weather_tcp_fetch(wsl_ip, result_wuhan, wuhan_len, result_beijing, beijing_len) == 0) {
        printf("[Weather] TCP fetch succeeded.\n");
        return 0;
    }
    if (read_weather_cache_file(WEATHER_LOCAL_CACHE_FILE, result_wuhan, wuhan_len, result_beijing, beijing_len) == 0) {
        printf("[Weather] TCP fetch failed, using local cache immediately: %s\n", WEATHER_LOCAL_CACHE_FILE);
        return 1;
    }
    printf("[Weather] TCP fetch failed and local cache unavailable, falling back to SSH/SCP.\n");

    derive_remote_cache_path(script_path, remote_cache, sizeof(remote_cache));

    snprintf(ssh_cmd, sizeof(ssh_cmd),
             "ssh -o BatchMode=yes -o StrictHostKeyChecking=no -o ConnectTimeout=3 %s@%s \"python3 %s\" 2>&1",
             wsl_username, wsl_ip, script_path);
    printf("[Weather] SSH command: %s\n", ssh_cmd);
    if (run_command_capture(ssh_cmd, ssh_output, sizeof(ssh_output)) == 0) {
        if (ssh_output[0]) {
            printf("[Weather] SSH output: %s\n", ssh_output);
        }

        snprintf(scp_cmd, sizeof(scp_cmd),
                 "scp -o BatchMode=yes -o StrictHostKeyChecking=no -o ConnectTimeout=3 %s@%s:%s %s 2>&1",
                 wsl_username, wsl_ip, remote_cache, WEATHER_LOCAL_CACHE_FILE);
        printf("[Weather] SCP command: %s\n", scp_cmd);
        if (run_command_capture(scp_cmd, scp_output, sizeof(scp_output)) == 0) {
            if (scp_output[0]) {
                printf("[Weather] SCP output: %s\n", scp_output);
            }
            if (read_weather_cache_file(WEATHER_LOCAL_CACHE_FILE, result_wuhan, wuhan_len, result_beijing, beijing_len) == 0) {
                return 0;
            }
            printf("[Weather] Local cache pull succeeded but cache parse failed: %s\n", WEATHER_LOCAL_CACHE_FILE);
        } else {
            printf("[Weather] SCP pull failed. output=%s\n", scp_output[0] ? scp_output : "(empty)");
        }

        if (parse_weather_line(ssh_output, result_wuhan, wuhan_len, result_beijing, beijing_len) == 0) {
            return 0;
        }
        printf("[Weather] SSH succeeded but weather output parse failed. output=%s\n", ssh_output[0] ? ssh_output : "(empty)");
    } else {
        printf("[Weather] SSH failed. output=%s\n", ssh_output[0] ? ssh_output : "(empty)");
    }

    if (read_weather_cache_file(WEATHER_LOCAL_CACHE_FILE, result_wuhan, wuhan_len, result_beijing, beijing_len) == 0) {
        printf("[Weather] Using local cache only: %s\n", WEATHER_LOCAL_CACHE_FILE);
        return 1;
    }

    printf("[Weather] Local cache unavailable: %s\n", WEATHER_LOCAL_CACHE_FILE);
    return -2;
}






