#ifndef COMMON_CONFIG_H
#define COMMON_CONFIG_H

typedef struct {
    char wsl_server_ip[64];
    char wsl_username[64];
    char board_ip[64];
    char touch_device[64];
    char drm_device[64];
    char image_dir[256];
    char file_browser_root[256];
    char local_voice_cmd_wav[256];
    char local_voice_reply_wav[256];
    char wsl_voice_cmd_wav[256];
    char wsl_ai_pipeline[256];
    char wsl_get_weather[256];
    char wifi_password_file[256];
    char font_path[256]; // 新增：字库文件配置路径以避免代码硬编码
    int ts_min_x;
    int ts_max_x;
    int ts_min_y;
    int ts_max_y;
} app_config_t;

// 声明全局配置结构体
extern app_config_t g_config;

// 加载配置文件 (JSON)
int app_config_load(const char *config_path);

#endif // COMMON_CONFIG_H
