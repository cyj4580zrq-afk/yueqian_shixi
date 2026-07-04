#include "common/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

app_config_t g_config;

static void parse_value(const char *line, const char *key, char *dest, int dest_len) {
    char *pos = strstr(line, key);
    if (!pos) return;
    
    char *colon = strchr(pos, ':');
    if (!colon) return;
    
    char *quote1 = strchr(colon, '"');
    if (!quote1) return;
    
    char *quote2 = strchr(quote1 + 1, '"');
    if (!quote2) return;
    
    int len = quote2 - quote1 - 1;
    if (len >= dest_len) len = dest_len - 1;
    
    strncpy(dest, quote1 + 1, len);
    dest[len] = '\0';
}

static int parse_int(const char *line, const char *key, int *dest) {
    char *pos = strstr(line, key);
    if (!pos) return -1;
    
    char *colon = strchr(pos, ':');
    if (!colon) return -1;
    
    // 直接用 atoi 提取冒号后面的整型值
    *dest = atoi(colon + 1);
    return 0;
}

int config_load(const char *filepath) {
    // 1. 赋默认值：与源码配置保持一致，避免配置文件缺失时退化到错误路径
    strcpy(g_config.wsl_server_ip, "10.203.129.104");
    strcpy(g_config.wsl_username, "cyj");
    strcpy(g_config.board_ip, "10.203.129.239");
    strcpy(g_config.touch_device, "/dev/input/event2");
    strcpy(g_config.drm_device, "/dev/dri/card0");
    strcpy(g_config.image_dir, "assets");
    strcpy(g_config.file_browser_root, ".");
    strcpy(g_config.local_voice_cmd_wav, "/tmp/rec.wav");
    strcpy(g_config.local_voice_reply_wav, "/tmp/reply.wav");
    strcpy(g_config.wsl_voice_cmd_wav, "/home/cyj/workspace/ai_assistant/bin/wav/cmd.wav");
    strcpy(g_config.wsl_ai_pipeline, "/home/cyj/workspace/ai_assistant/scripts/ai_pipeline.py");
    strcpy(g_config.wsl_get_weather, "/home/cyj/workspace/ai_assistant/scripts/get_weather.py");
    strcpy(g_config.wifi_password_file, "config/wifi_passwords.conf");
    strcpy(g_config.font_path, "assets/simsun.ttc");
    
    g_config.ts_min_x = 0;
    g_config.ts_max_x = 1023;
    g_config.ts_min_y = 0;
    g_config.ts_max_y = 599;

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        printf("[Config] Cannot open config %s, using default settings.\n", filepath ? filepath : "NULL");
        return -1;
    }

    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        parse_value(line, "wsl_server_ip", g_config.wsl_server_ip, sizeof(g_config.wsl_server_ip));
        parse_value(line, "wsl_username", g_config.wsl_username, sizeof(g_config.wsl_username));
        parse_value(line, "board_ip", g_config.board_ip, sizeof(g_config.board_ip));
        parse_value(line, "touch_device", g_config.touch_device, sizeof(g_config.touch_device));
        parse_value(line, "drm_device", g_config.drm_device, sizeof(g_config.drm_device));
        parse_value(line, "image_dir", g_config.image_dir, sizeof(g_config.image_dir));
        parse_value(line, "file_browser_root", g_config.file_browser_root, sizeof(g_config.file_browser_root));
        parse_value(line, "local_voice_cmd_wav", g_config.local_voice_cmd_wav, sizeof(g_config.local_voice_cmd_wav));
        parse_value(line, "local_voice_reply_wav", g_config.local_voice_reply_wav, sizeof(g_config.local_voice_reply_wav));
        parse_value(line, "wsl_voice_cmd_wav", g_config.wsl_voice_cmd_wav, sizeof(g_config.wsl_voice_cmd_wav));
        parse_value(line, "wsl_ai_pipeline", g_config.wsl_ai_pipeline, sizeof(g_config.wsl_ai_pipeline));
        parse_value(line, "wsl_get_weather", g_config.wsl_get_weather, sizeof(g_config.wsl_get_weather));
        parse_value(line, "wifi_password_file", g_config.wifi_password_file, sizeof(g_config.wifi_password_file));
        parse_value(line, "font_path", g_config.font_path, sizeof(g_config.font_path));
        
        parse_int(line, "ts_min_x", &g_config.ts_min_x);
        parse_int(line, "ts_max_x", &g_config.ts_max_x);
        parse_int(line, "ts_min_y", &g_config.ts_min_y);
        parse_int(line, "ts_max_y", &g_config.ts_max_y);
    }

    fclose(fp);
    printf("[Config] Loaded successfully from %s.\n", filepath);
    return 0;
}
