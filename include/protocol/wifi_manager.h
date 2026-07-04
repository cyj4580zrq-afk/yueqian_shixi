#ifndef PROTOCOL_WIFI_MANAGER_H
#define PROTOCOL_WIFI_MANAGER_H

#define MAX_WIFI 64

typedef struct {
    char ssid[128];
    int signal;
} wifi_info_t;

// 统一为返回 int
int wifi_manager_refresh_ip(char *out_ip, int ip_len, char *out_ssid, int ssid_len);

// 扫描周边 wifi 列表并返回数量
int wifi_manager_scan(wifi_info_t *out_wifis, int max_count);

// 连接指定 SSID 传入密码配置文件路径
int wifi_manager_connect(const char *ssid, const char *passwords_conf_path);
int wifi_manager_connect_with_password(const char *ssid, const char *password);

#endif // PROTOCOL_WIFI_MANAGER_H

