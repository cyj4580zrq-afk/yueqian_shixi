#include "protocol/wifi_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int wifi_manager_refresh_ip(char *out_ip, int ip_len, char *out_ssid, int ssid_len) {
    // 赋空
    out_ip[0] = '\0';
    out_ssid[0] = '\0';
    
    // 1. 读取 wlan0 的 SSID
    FILE *fp_ssid = popen("wpa_cli -p /var/run/wpa_supplicant -i wlan0 status 2>/dev/null | awk -F= '/^ssid=/{print $2}'", "r");
    if (fp_ssid) {
        if (fgets(out_ssid, ssid_len, fp_ssid)) {
            int len = strlen(out_ssid);
            if (len > 0 && out_ssid[len - 1] == '\n') {
                out_ssid[len - 1] = '\0';
            }
        }
        pclose(fp_ssid);
    }
    
    // 2. 读取当前 IP (遍历 wlan0)
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd >= 0) {
        struct ifreq ifr;
        strcpy(ifr.ifr_name, "wlan0");
        if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
            strncpy(out_ip, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), ip_len - 1);
            out_ip[ip_len - 1] = '\0';
        } else {
            // fallback eth0
            strcpy(ifr.ifr_name, "eth0");
            if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
                strncpy(out_ip, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), ip_len - 1);
                out_ip[ip_len - 1] = '\0';
            }
        }
        close(fd);
    }
    
    return 0;
}

int wifi_manager_scan(wifi_info_t *out_wifis, int max_count) {
    // 1. 确保网卡处于 up 状态，并启动 wpa_supplicant（如果未运行）
    system("ifconfig wlan0 up >/dev/null 2>&1");
    usleep(100000); // 等待网卡启动稳定
    system("pidof wpa_supplicant >/dev/null || wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant.conf >/dev/null 2>&1");
    usleep(200000); // 等待 wpa_supplicant 稳定启动
    
    // 2. 触发 wpa_cli 扫描并等待 1.5 秒完成
    system("wpa_cli -p /var/run/wpa_supplicant -i wlan0 scan >/dev/null 2>&1 || wpa_cli -i wlan0 scan >/dev/null 2>&1");
    usleep(1500000);
    
    FILE *fp = popen("(wpa_cli -p /var/run/wpa_supplicant -i wlan0 scan_results 2>/dev/null || wpa_cli -i wlan0 scan_results 2>/dev/null) | tail -n +3", "r");
    if (!fp) return 0;
    
    int count = 0;
    char line[512];
    while (fgets(line, sizeof(line), fp) && count < max_count) {
        // scan_results 输出格式一般为:
        // bssid / frequency / signal level / flags / ssid (可能有空格)
        char bssid[64] = "";
        int freq = 0;
        int signal = 0;
        char flags[128] = "";
        char ssid[128] = "";
        
        // 仅匹配前4个字段，然后提取 flags 后面的全部内容作为 SSID，支持带空格的 SSID
        int items = sscanf(line, "%s\t%d\t%d\t%s", bssid, &freq, &signal, flags);
        if (items == 4) {
            char *pos = strstr(line, flags);
            if (pos) {
                pos += strlen(flags);
                // 跳过分隔的空白字符
                while (*pos == ' ' || *pos == '\t') pos++;
                strncpy(ssid, pos, sizeof(ssid) - 1);
                ssid[sizeof(ssid) - 1] = '\0';
                
                // 去除末尾换行和回车
                int slen = strlen(ssid);
                while (slen > 0 && (ssid[slen - 1] == '\n' || ssid[slen - 1] == '\r')) {
                    ssid[--slen] = '\0';
                }
            }
            
            if (ssid[0]) {
                strncpy(out_wifis[count].ssid, ssid, sizeof(out_wifis[count].ssid) - 1);
                out_wifis[count].ssid[sizeof(out_wifis[count].ssid) - 1] = '\0';
                out_wifis[count].signal = signal;
                count++;
            }
        }
    }
    pclose(fp);
    return count;
}

static void shell_quote(char *out, size_t out_size, const char *in)
{
    size_t pos = 0;
    if (!out || out_size == 0) return;
    out[pos++] = '\'';
    while (in && *in && pos + 5 < out_size) {
        if (*in == '\'') {
            out[pos++] = '\'';
            out[pos++] = '\\';
            out[pos++] = '\'';
            out[pos++] = '\'';
        } else {
            out[pos++] = *in;
        }
        ++in;
    }
    if (pos + 1 < out_size) out[pos++] = '\'';
    out[pos] = '\0';
}

static int wifi_manager_connect_raw(const char *ssid, const char *password)
{
    char cmd[768];
    char ssid_value[192];
    char psk_value[192];
    char ssid_arg[256];
    char psk_arg[256];
    int net_id = -1;
    FILE *fp_add;

    if (!ssid || !ssid[0]) return -1;
    if (!password) password = "";

    printf("[WiFi] Attempting to connect to SSID: %s\n", ssid);

    fp_add = popen("wpa_cli -p /var/run/wpa_supplicant -i wlan0 add_network 2>/dev/null | tail -n 1", "r");
    if (fp_add) {
        char id_buf[32] = "";
        if (fgets(id_buf, sizeof(id_buf), fp_add)) {
            net_id = atoi(id_buf);
        }
        pclose(fp_add);
    }

    if (net_id < 0) net_id = 0;

    snprintf(ssid_value, sizeof(ssid_value), "\"%s\"", ssid);
    snprintf(psk_value, sizeof(psk_value), "\"%s\"", password);
    shell_quote(ssid_arg, sizeof(ssid_arg), ssid_value);
    shell_quote(psk_arg, sizeof(psk_arg), psk_value);

    snprintf(cmd, sizeof(cmd), "wpa_cli -p /var/run/wpa_supplicant -i wlan0 set_network %d ssid %s >/dev/null 2>&1", net_id, ssid_arg);
    system(cmd);

    if (password[0]) {
        snprintf(cmd, sizeof(cmd), "wpa_cli -p /var/run/wpa_supplicant -i wlan0 set_network %d psk %s >/dev/null 2>&1", net_id, psk_arg);
    } else {
        snprintf(cmd, sizeof(cmd), "wpa_cli -p /var/run/wpa_supplicant -i wlan0 set_network %d key_mgmt NONE >/dev/null 2>&1", net_id);
    }
    system(cmd);

    snprintf(cmd, sizeof(cmd), "wpa_cli -p /var/run/wpa_supplicant -i wlan0 select_network %d >/dev/null 2>&1", net_id);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "wpa_cli -p /var/run/wpa_supplicant -i wlan0 enable_network %d >/dev/null 2>&1", net_id);
    system(cmd);

    system("udhcpc -i wlan0 -q >/dev/null 2>&1 &");
    return 0;
}

int wifi_manager_connect_with_password(const char *ssid, const char *password)
{
    return wifi_manager_connect_raw(ssid, password);
}

int wifi_manager_connect(const char *ssid, const char *passwords_conf_path) {
    if (!ssid || !ssid[0]) return -1;
    
    // 从密码文件里提取该 SSID 对应的密码
    char password[128] = "";
    FILE *fp = fopen(passwords_conf_path, "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            // 格式为 SSID=PASSWORD
            char *equal = strchr(line, '=');
            if (equal) {
                *equal = '\0';
                char *cur_ssid = line;
                char *cur_pwd = equal + 1;
                // 去除换行符
                int len = strlen(cur_pwd);
                if (len > 0 && cur_pwd[len - 1] == '\n') cur_pwd[len - 1] = '\0';
                if (len > 1 && cur_pwd[len - 2] == '\r') cur_pwd[len - 2] = '\0';
                
                if (strcmp(cur_ssid, ssid) == 0) {
                    strncpy(password, cur_pwd, sizeof(password) - 1);
                    password[sizeof(password) - 1] = '\0';
                    break;
                }
            }
        }
        fclose(fp);
    }
    
    // 如果没有找到密码，默认使用 "12345678" 以防无法进行任何连接
    if (!password[0]) {
        strcpy(password, "12345678");
    }
    
    return wifi_manager_connect_raw(ssid, password);

}

