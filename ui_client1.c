/*
 * ui_client.c — AI Assistant Multi-Page Touch UI (RK1808/GEC6818)
 * ================================================================
 * Screen:   1024x600 DRM Framebuffer (BGRA)
 * Touch:    /dev/input/event2
 * Pages:    Home / Voice / Smart Home / Images / Settings / BMP View
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <dirent.h>
#include <math.h>
#include "./DRMwrap.h"

#include "./inc/display.h"
#include "./inc/font_display.h"

/* stb_truetype embedded font renderer */
extern int font_init(const char *font_path);
extern void font_done(void);
extern void DRM_Draw_Text(int lcd_fd, struct drmHandle *drm,
                          const char *text, int font_size,
                          uint32_t color, int x, int y);

/* ===== Forward Declarations ===== */
static void scan_bmp_dir();
static void wifi_scan();
static void render_page();
static void* record_thread_func(void* arg);
static void* send_thread_func(void* arg);
static void* wifi_scan_thread_func(void* arg);
static void* weather_thread_func(void* arg);
static void* listener_thread(void* arg);
static void* wav_listener_thread(void* arg);


#define SCREEN_W      1024
#define SCREEN_H      600
#define CONTROL_PORT  8888
#define WAV_DATA_PORT 8889
#define TOUCH_DEVICE  "/dev/input/event2"
#define DRM_DEVICE    "/dev/dri/card0"
#define BMP_DIR       "/root/picture"

#define PAGE_HOME      0
#define PAGE_VOICE     1
#define PAGE_SMART     2
#define PAGE_IMAGES    3
#define PAGE_SETTINGS  4
#define PAGE_BMPVIEW   5

#define DEV_LIGHT   0
#define DEV_FAN     1
#define DEV_CURTAIN 2
#define DEV_LOCK    3
#define DEV_SWITCH  4
#define DEV_TEMP    5

#define MAX_WIFI    20
#define MAX_BMP     100
#define MAX_CHAT    20


typedef struct { int x1, y1, x2, y2; } rect_t;

/* ===== Touch Hit Regions ===== */
/* Home page buttons */
static rect_t home_voice_btn  = {80, 180, 480, 340};
static rect_t home_smart_btn  = {510, 180, 910, 340};
static rect_t home_images_btn = {80, 370, 480, 530};
static rect_t home_sett_btn   = {510, 370, 910, 530};

/* Voice page (match drawn layout) */
static rect_t voice_mic_btn   = {392, 340, 502, 420};
static rect_t voice_send_btn  = {522, 340, 632, 420};

/* Smart Home devices */
static rect_t smart_btns[6] = {
    {40, 110, 330, 260}, {360, 110, 650, 260}, {680, 110, 970, 260},
    {40, 290, 330, 440}, {360, 290, 650, 440}, {680, 290, 970, 440}
};

/* Images page */
static rect_t img_up_btn   = {964, 55, 1004, 95};
static rect_t img_down_btn = {964, 105, 1004, 145};
static rect_t img_thumbs[6] = {
    {30, 100, 330, 300}, {350, 100, 650, 300}, {670, 100, 970, 300},
    {30, 320, 330, 520}, {350, 320, 650, 520}, {670, 320, 970, 520}
};

/* BMP View */
static rect_t bmp_prev_btn = {30, 520, 120, 558};
static rect_t bmp_next_btn = {874, 520, 974, 558};

/* Settings */
static rect_t sett_scan_btn  = {864, 102, 994, 148};
static rect_t sett_wifi_btns[6] = {
    {854, 175, 964, 205}, {854, 225, 964, 255}, {854, 275, 964, 305},
    {854, 325, 964, 355}, {854, 375, 964, 405}, {854, 425, 964, 455}
};

/* Back button (universal, top-right) */
static rect_t back_btn = {924, 7, 1004, 43};


/* ===== Globals ===== */
struct drmHandle DRM;
int lcd_fb = -1;
int touch_fd = -1;
pthread_mutex_t ui_lock = PTHREAD_MUTEX_INITIALIZER;
static int ts_min_x = 0, ts_max_x = 1023;
static int ts_min_y = 0, ts_max_y = 599;
static int cur_page = PAGE_HOME;
static int cur_subpage = -1;
static int smart_dev[6] = {0, 0, 0, 0, 0, 0};
static char bmp_files[MAX_BMP][256];
static int bmp_count = 0;
static int bmp_scroll = 0;
static int bmp_index = 0;
static char wifi_ssid[MAX_WIFI][64];
static int wifi_signal[MAX_WIFI];
static int wifi_count = 0;
static int wifi_scanning = 0;
static int rec_state = 0;
static char chat_text[MAX_CHAT][256];
static int chat_lines = 0;
static char status_msg[128] = "AI ready";
static int msg_timer = 0;
static int ui_needs_redraw = 0;
static char g_wsl_ip[64] = "10.200.67.238";
static char weather_info_wuhan[128] = "获取中...";
static char weather_info_beijing[128] = "获取中...";


/* ===== Drawing Primitives ===== */
static void draw_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    if (!DRM.vaddr) return;
    uint32_t *fb = (uint32_t *)DRM.vaddr;
    fb[y * SCREEN_W + x] = color;
}

static void draw_rect(int x1, int y1, int x2, int y2, uint32_t color) {
    for (int y = y1; y <= y2; y++)
        for (int x = x1; x <= x2; x++)
            draw_pixel(x, y, color);
}

static void draw_rect_round(int x1, int y1, int x2, int y2, int r, uint32_t color) {
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            int in = 0;
            if (x < x1 + r && y < y1 + r) {
                int dx = x - (x1 + r), dy = y - (y1 + r);
                in = (dx*dx + dy*dy <= r*r);
            } else if (x > x2 - r && y < y1 + r) {
                int dx = x - (x2 - r), dy = y - (y1 + r);
                in = (dx*dx + dy*dy <= r*r);
            } else if (x < x1 + r && y > y2 - r) {
                int dx = x - (x1 + r), dy = y - (y2 - r);
                in = (dx*dx + dy*dy <= r*r);
            } else if (x > x2 - r && y > y2 - r) {
                int dx = x - (x2 - r), dy = y - (y2 - r);
                in = (dx*dx + dy*dy <= r*r);
            } else { in = 1; }
            if (in) draw_pixel(x, y, color);
        }
    }
}

/* Chinese characters: now rendered via stb_truetype (see draw_text) */


static void draw_utf8_text(int x, int y, const char* str, uint32_t color, int scale) {
    /* Use stb_truetype real font rendering.
       Map old scale to font_size: 1->12px, 2->24px, 3->36px */
    int font_size = scale * 14;
    DRM_Draw_Text(lcd_fb, &DRM, str, font_size, color, x, y);
}

/* ASCII chars: now rendered via stb_truetype (see draw_text) */

static void draw_text(int x, int y, const char* str, uint32_t color, int scale) {
    draw_utf8_text(x, y, str, color, scale);
}


static int touch_map_x(int raw) {
    if (ts_max_x == ts_min_x) return raw;
    return SCREEN_W * (raw - ts_min_x) / (ts_max_x - ts_min_x);
}

static int touch_map_y(int raw) {
    if (ts_max_y == ts_min_y) return raw;
    return SCREEN_H * (raw - ts_min_y) / (ts_max_y - ts_min_y);
}

static int pt_in_rect(int px, int py, rect_t r) {
    return px >= r.x1 && px <= r.x2 && py >= r.y1 && py <= r.y2;
}

/* ===== BMP Loading ===== */
static void draw_bmp(int dx, int dy, int dw, int dh, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return;
    uint8_t header[54];
    if (fread(header, 1, 54, f) != 54) { fclose(f); return; }
    int w = *(int*)(header + 18);
    int h = *(int*)(header + 22);
    int bpp = *(short*)(header + 28);
    if (bpp != 24 && bpp != 32) { fclose(f); return; }
    int row_size = ((w * (bpp/8) + 3) / 4) * 4;
    int data_off = *(int*)(header + 10);
    fseek(f, data_off, SEEK_SET);
    uint8_t* row = (uint8_t*)malloc(row_size);
    for (int y = 0; y < h && y < dh; y++) {
        fread(row, 1, row_size, f);
        for (int x = 0; x < w && x < dw; x++) {
            int px = dx + x;
            int py = dy + dh - 1 - y;
            if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                uint32_t color;
                if (bpp == 24) {
                    color = row[x*3] | (row[x*3+1] << 8) | (row[x*3+2] << 16);
                } else {
                    color = *(uint32_t*)(row + x*4);
                }
                draw_pixel(px, py, color);
            }
        }
    }
    free(row);
    fclose(f);
}

static void scan_bmp_dir() {
    DIR* d = opendir(BMP_DIR);
    if (!d) return;
    bmp_count = 0;
    struct dirent* entry;
    while ((entry = readdir(d)) && bmp_count < MAX_BMP) {
        char* n = entry->d_name;
        int len = strlen(n);
        if (len > 4 && (strcasecmp(n + len - 4, ".bmp") == 0)) {
            strncpy(bmp_files[bmp_count], n, 255);
            bmp_count++;
        }
    }
    closedir(d);
}

/* ===== WiFi Scan ===== */
static void wifi_scan() {
    wifi_count = 0;
    wifi_scanning = 1;
    FILE* fp = popen("iwlist wlan0 scan 2>/dev/null", "r");
    if (!fp) { wifi_scanning = 0; return; }
    char buf[512];
    while (fgets(buf, sizeof(buf), fp) && wifi_count < MAX_WIFI) {
        char* essid = strstr(buf, "ESSID:");
        if (essid) {
            char* start = strchr(essid, '"');
            if (start) {
                char* end = strchr(start + 1, '"');
                if (end) {
                    int len = end - start - 1;
                    if (len > 63) len = 63;
                    strncpy(wifi_ssid[wifi_count], start + 1, len);
                    wifi_ssid[wifi_count][len] = 0;
                }
            }
        }
        char* qual = strstr(buf, "Signal level=");
        if (qual && wifi_count < MAX_WIFI) {
            wifi_signal[wifi_count] = atoi(qual + 13);
            wifi_count++;
        }
    }
    pclose(fp);
    wifi_scanning = 0;
}

/* ===== Background Thread Implementations ===== */
static void* record_thread_func(void* arg) {
    pthread_mutex_lock(&ui_lock);
    rec_state = 1;
    snprintf(status_msg, sizeof(status_msg), "正在录音 (3秒)...");
    render_page();
    DRMshowUp(lcd_fb, &DRM);
    pthread_mutex_unlock(&ui_lock);

    // Record using card 1: rk809-codec (plughw:1,0)
    system("arecord -D plughw:1,0 -d 3 -c 1 -r 16000 -t wav -f S16_LE /root/code/cmd.wav >/dev/null 2>&1");

    pthread_mutex_lock(&ui_lock);
    rec_state = 0;
    snprintf(status_msg, sizeof(status_msg), "录音完毕，请发送");
    render_page();
    DRMshowUp(lcd_fb, &DRM);
    pthread_mutex_unlock(&ui_lock);
    return NULL;
}

static void* send_thread_func(void* arg) {
    pthread_mutex_lock(&ui_lock);
    snprintf(status_msg, sizeof(status_msg), "正在发送音频...");
    render_page();
    DRMshowUp(lcd_fb, &DRM);
    pthread_mutex_unlock(&ui_lock);

    char scp_cmd[512];
    snprintf(scp_cmd, sizeof(scp_cmd),
             "scp -o StrictHostKeyChecking=no /root/code/cmd.wav cyj@%s:/home/cyj/workspace/ai_assistant/bin/wav/cmd.wav >/dev/null 2>&1",
             g_wsl_ip);
    int ret = system(scp_cmd);

    pthread_mutex_lock(&ui_lock);
    if (ret == 0) {
        snprintf(status_msg, sizeof(status_msg), "已发送，等待AI回复...");
        if (chat_lines < MAX_CHAT) {
            snprintf(chat_text[chat_lines], 256, "> 语音已发送");
            chat_lines++;
        }
    } else {
        snprintf(status_msg, sizeof(status_msg), "发送失败，请检查连接");
    }
    render_page();
    DRMshowUp(lcd_fb, &DRM);
    pthread_mutex_unlock(&ui_lock);
    return NULL;
}

static void* wifi_scan_thread_func(void* arg) {
    pthread_mutex_lock(&ui_lock);
    wifi_scanning = 1;
    snprintf(status_msg, sizeof(status_msg), "正在扫描Wi-Fi...");
    render_page();
    DRMshowUp(lcd_fb, &DRM);
    pthread_mutex_unlock(&ui_lock);

    system("wpa_cli -p /var/run/wpa_supplicant -i wlan0 scan >/dev/null 2>&1");
    usleep(1500000); // Wait for scan results to be populated

    FILE* fp = popen("wpa_cli -p /var/run/wpa_supplicant -i wlan0 scan_results 2>/dev/null", "r");
    pthread_mutex_lock(&ui_lock);
    wifi_count = 0;
    if (fp) {
        char buf[512];
        // Skip header line
        if (fgets(buf, sizeof(buf), fp)) {}
        
        while (fgets(buf, sizeof(buf), fp) && wifi_count < MAX_WIFI) {
            int signal = 0;
            char ssid[128] = {0};
            
            char* tab1 = strchr(buf, '\t'); if (!tab1) continue; *tab1 = '\0';
            char* tab2 = strchr(tab1 + 1, '\t'); if (!tab2) continue; *tab2 = '\0';
            char* tab3 = strchr(tab2 + 1, '\t'); if (!tab3) continue; *tab3 = '\0';
            char* tab4 = strchr(tab3 + 1, '\t'); if (!tab4) continue; *tab4 = '\0';
            
            signal = atoi(tab2 + 1);
            char* ssid_ptr = tab4 + 1;
            int slen = strlen(ssid_ptr);
            while (slen > 0 && (ssid_ptr[slen-1] == '\n' || ssid_ptr[slen-1] == '\r')) {
                ssid_ptr[slen-1] = '\0';
                slen--;
            }
            strncpy(ssid, ssid_ptr, sizeof(ssid) - 1);
            
            if (strlen(ssid) > 0) {
                strncpy(wifi_ssid[wifi_count], ssid, sizeof(wifi_ssid[wifi_count]) - 1);
                wifi_ssid[wifi_count][sizeof(wifi_ssid[wifi_count]) - 1] = '\0';
                wifi_signal[wifi_count] = signal;
                wifi_count++;
            }
        }
        pclose(fp);
    }
    wifi_scanning = 0;
    snprintf(status_msg, sizeof(status_msg), "扫描完成，发现 %d 个网络", wifi_count);
    render_page();
    DRMshowUp(lcd_fb, &DRM);
    pthread_mutex_unlock(&ui_lock);
    return NULL;
}

static void* weather_thread_func(void* arg) {
    pthread_mutex_lock(&ui_lock);
    snprintf(status_msg, sizeof(status_msg), "正在获取天气预报...");
    render_page();
    DRMshowUp(lcd_fb, &DRM);
    pthread_mutex_unlock(&ui_lock);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "ssh -o StrictHostKeyChecking=no cyj@%s 'python3 /home/cyj/workspace/ai_assistant/get_weather.py' 2>/dev/null", g_wsl_ip);
    FILE* fp = popen(cmd, "r");
    
    pthread_mutex_lock(&ui_lock);
    if (fp) {
        char buf[256] = {0};
        if (fgets(buf, sizeof(buf), fp)) {
            int len = strlen(buf);
            while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
                buf[len-1] = '\0';
                len--;
            }
            char* parts = strchr(buf, '|');
            if (parts) {
                *parts = '\0';
                char* wh = strchr(buf, ':');
                char* bj = strchr(parts + 1, ':');
                if (wh && bj) {
                    strncpy(weather_info_wuhan, wh + 1, sizeof(weather_info_wuhan) - 1);
                    strncpy(weather_info_beijing, bj + 1, sizeof(weather_info_beijing) - 1);
                }
            }
        }
        pclose(fp);
        snprintf(status_msg, sizeof(status_msg), "天气更新完成");
    } else {
        snprintf(status_msg, sizeof(status_msg), "获取天气失败");
    }
    render_page();
    DRMshowUp(lcd_fb, &DRM);
    pthread_mutex_unlock(&ui_lock);
    return NULL;
}

static void* listener_thread(void* arg) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return NULL;
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(CONTROL_PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server_fd);
        return NULL;
    }
    if (listen(server_fd, 3) < 0) {
        close(server_fd);
        return NULL;
    }

    printf("[Listener] TCP server listening on port %d...\n", CONTROL_PORT);

    while (1) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int csock = accept(server_fd, (struct sockaddr*)&caddr, &clen);
        if (csock < 0) continue;

        char buf[1024] = {0};
        int len = recv(csock, buf, sizeof(buf) - 1, 0);
        if (len > 0) {
            buf[len] = '\0';
            printf("[Listener] Received AI command: %s\n", buf);

            // Play the received wav file in background
            system("aplay -D plughw:1,0 /root/code/reply.wav >/dev/null 2>&1 &");

            pthread_mutex_lock(&ui_lock);
            if (chat_lines < MAX_CHAT) {
                snprintf(chat_text[chat_lines], 256, "< AI: %s", buf);
                chat_lines++;
            }
            snprintf(status_msg, sizeof(status_msg), "AI: %s", buf);
            render_page();
            DRMshowUp(lcd_fb, &DRM);
            pthread_mutex_unlock(&ui_lock);
        }
        close(csock);
    }
    close(server_fd);
    return NULL;
}

/* WAV binary listener: 接收 WSL 端 SCP 失败时的降级 WAV 数据 */
static void* wav_listener_thread(void* arg) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return NULL;
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(WAV_DATA_PORT);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("[WAV Listener] Port %d bind failed, skip wav receiver\n", WAV_DATA_PORT);
        close(server_fd);
        return NULL;
    }
    if (listen(server_fd, 3) < 0) {
        close(server_fd);
        return NULL;
    }

    printf("[WAV Listener] TCP server listening on port %d...\n", WAV_DATA_PORT);

    while (1) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int csock = accept(server_fd, (struct sockaddr*)&caddr, &clen);
        if (csock < 0) continue;

        /* 读取 4 字节长度头 */
        uint32_t data_len = 0;
        int n = recv(csock, &data_len, 4, MSG_WAITALL);
        if (n != 4 || data_len == 0 || data_len > 1024*1024) {
            close(csock);
            continue;
        }
        data_len = ntohl(data_len);

        /* 读取 WAV 数据并写入文件 */
        FILE* fp = fopen("/root/code/reply.wav", "wb");
        if (!fp) { close(csock); continue; }

        char buf[8192];
        uint32_t remaining = data_len;
        while (remaining > 0) {
            int chunk = remaining > sizeof(buf) ? sizeof(buf) : remaining;
            int r = recv(csock, buf, chunk, 0);
            if (r <= 0) break;
            fwrite(buf, 1, r, fp);
            remaining -= r;
        }
        fclose(fp);
        close(csock);

        struct stat st;
        if (stat("/root/code/reply.wav", &st) == 0 && st.st_size > 1000) {
            printf("[WAV Listener] Received wav (%ld bytes), playing...\n", (long)st.st_size);
            system("aplay -D plughw:1,0 /root/code/reply.wav >/dev/null 2>&1 &");
        } else {
            printf("[WAV Listener] WAV file invalid, skip playback\n");
        }
    }
    close(server_fd);
    return NULL;
}


/* ===== Page Renderers ===== */

static void draw_bottom_status() {
    draw_rect(0, SCREEN_H - 36, SCREEN_W, SCREEN_H, 0xFF1a1a2e);
    draw_text(12, SCREEN_H - 28, status_msg, 0xFFaaaaaa, 1);
}

static void render_home_page() {
    draw_rect(0, 50, SCREEN_W, SCREEN_H - 36, 0xFF0f0f23);

    /* Title */
    draw_text(SCREEN_W/2 - 96, 80, "智能助手", 0xFFFFFFFF, 3);

    /* Subtitle */
    draw_text(SCREEN_W/2 - 32, 130, "系统信息", 0xFF8888cc, 1);

    /* 4 menu buttons in 2x2 grid */
    int bx = 80, by = 180, bw = 400, bh = 160, gap = 30;
    uint32_t colors[] = {0xFF16213e, 0xFF0f3460, 0xFF533483, 0xFF1a1a2e};

    /* Voice */
    draw_rect_round(bx, by, bx+bw, by+bh, 12, colors[0]);
    draw_text(bx + bw/2 - 32, by + 40, "语音", 0xFFFFFFFF, 2);
    draw_text(bx + bw/2 - 32, by + 80, "助手", 0xFFe94560, 2);

    /* Weather */
    draw_rect_round(bx + bw + gap, by, (bx + bw + gap) + bw, by+bh, 12, colors[1]);
    draw_text(bx + bw + gap + bw/2 - 32, by + 40, "天气", 0xFFFFFFFF, 2);
    draw_text(bx + bw + gap + bw/2 - 32, by + 80, "预报", 0xFF0a97b0, 2);

    /* Images */
    draw_rect_round(bx, by + bh + gap, bx+bw, by+bh+gap+bh, 12, colors[2]);
    draw_text(bx + bw/2 - 32, by+bh+gap + 40, "图片", 0xFFFFFFFF, 2);
    draw_text(bx + bw/2 - 32, by+bh+gap + 80, "浏览", 0xFFf9a826, 2);

    /* Settings */
    draw_rect_round(bx + bw + gap, by + bh + gap, (bx+bw+gap)+bw, by+bh+gap+bh, 12, colors[3]);
    draw_text(bx+bw+gap + bw/2 - 32, by+bh+gap + 55, "设置", 0xFFFFFFFF, 2);
}

static void render_voice_page() {
    draw_rect(0, 50, SCREEN_W, SCREEN_H - 36, 0xFF0f0f23);

    /* Title */
    draw_text(SCREEN_W/2 - 64, 60, "语音助手", 0xFFFFFFFF, 2);

    /* Chat area */
    draw_rect_round(20, 100, SCREEN_W - 20, 310, 8, 0xFF1a1a2e);
    int chat_start = chat_lines > 8 ? chat_lines - 8 : 0;
    for (int i = chat_start; i < chat_lines; i++) {
        draw_text(30, 110 + (i - chat_start)*24, chat_text[i], 0xFFcccccc, 1);
    }

    /* Mic button */
    uint32_t mic_color = rec_state ? 0xFFe94560 : 0xFF0f3460;
    draw_rect_round(SCREEN_W/2 - 120, 340, SCREEN_W/2 - 10, 420, 15, mic_color);
    draw_text(SCREEN_W/2 - 98, 368, "录音", 0xFFFFFFFF, 2);

    /* Send button */
    draw_rect_round(SCREEN_W/2 + 10, 340, SCREEN_W/2 + 120, 420, 15, 0xFF16213e);
    draw_text(SCREEN_W/2 + 32, 368, "发送", 0xFFFFFFFF, 2);

    /* Voice status indicator */
    draw_rect_round(20, 440, SCREEN_W - 20, 480, 6, 0xFF1a1a2e);
    if (rec_state) {
        draw_rect_round(22, 442, SCREEN_W - 22, 478, 5, 0xFF3a1020);
        draw_text(30, 452, "正在录音中...", 0xFFe94560, 1);
    } else {
        draw_text(30, 452, status_msg, 0xFF888888, 1);
    }
}

static void render_smart_page() {
    draw_rect(0, 50, SCREEN_W, SCREEN_H - 36, 0xFF0f0f23);

    draw_text(20, 60, "天气预报", 0xFFFFFFFF, 2);

    int card_w = 400, card_h = 300;
    int by = 150;

    // Wuhan card (Left)
    int bx1 = 80;
    draw_rect_round(bx1, by, bx1 + card_w, by + card_h, 12, 0xFF16213e);
    draw_text(bx1 + 40, by + 40, "武汉市", 0xFFFFFFFF, 2);

    char wh_temp[64] = "";
    char wh_cond[64] = "";
    char* wh_space = strchr(weather_info_wuhan, ' ');
    if (wh_space) {
        int len = wh_space - weather_info_wuhan;
        if (len > 63) len = 63;
        strncpy(wh_temp, weather_info_wuhan, len);
        wh_temp[len] = '\0';
        strncpy(wh_cond, wh_space + 1, sizeof(wh_cond) - 1);
    } else {
        strncpy(wh_temp, weather_info_wuhan, sizeof(wh_temp) - 1);
    }
    draw_text(bx1 + 40, by + 110, wh_temp, 0xFFf9a826, 3);
    draw_text(bx1 + 40, by + 200, wh_cond, 0xFF0a97b0, 2);

    // Beijing card (Right)
    int bx2 = 544;
    draw_rect_round(bx2, by, bx2 + card_w, by + card_h, 12, 0xFF16213e);
    draw_text(bx2 + 40, by + 40, "北京市", 0xFFFFFFFF, 2);

    char bj_temp[64] = "";
    char bj_cond[64] = "";
    char* bj_space = strchr(weather_info_beijing, ' ');
    if (bj_space) {
        int len = bj_space - weather_info_beijing;
        if (len > 63) len = 63;
        strncpy(bj_temp, weather_info_beijing, len);
        bj_temp[len] = '\0';
        strncpy(bj_cond, bj_space + 1, sizeof(bj_cond) - 1);
    } else {
        strncpy(bj_temp, weather_info_beijing, sizeof(bj_temp) - 1);
    }
    draw_text(bx2 + 40, by + 110, bj_temp, 0xFFf9a826, 3);
    draw_text(bx2 + 40, by + 200, bj_cond, 0xFF0a97b0, 2);
}

static void render_images_page() {
    draw_rect(0, 50, SCREEN_W, SCREEN_H - 36, 0xFF0f0f23);
    draw_text(20, 60, "图片浏览", 0xFFFFFFFF, 2);

    int cols = 3, thumb_w = 300, thumb_h = 200;
    int gap = 20, start_x = 30;
    for (int i = 0; i < 6 && (i + bmp_scroll * cols) < bmp_count; i++) {
        int idx = i + bmp_scroll * cols;
        int col = i % cols;
        int row = i / cols;
        int dx = start_x + col * (thumb_w + gap);
        int dy = 100 + row * (thumb_h + gap);
        draw_rect_round(dx, dy, dx + thumb_w, dy + thumb_h, 8, 0xFF1a1a2e);
        char fpath[512];
        snprintf(fpath, sizeof(fpath), "%s/%s", BMP_DIR, bmp_files[idx]);
        draw_bmp(dx + 4, dy + 4, thumb_w - 8, thumb_h - 32, fpath);
        draw_text(dx + 8, dy + thumb_h - 24, bmp_files[idx], 0xFFcccccc, 1);
    }

    /* Scroll indicators */
    if (bmp_scroll > 0) {
        draw_rect_round(SCREEN_W - 60, 55, SCREEN_W - 20, 95, 6, 0xFF16213e);
        draw_text(SCREEN_W - 52, 64, "上", 0xFFFFFFFF, 1);
    }
    if ((bmp_scroll + 2) * cols < bmp_count) {
        draw_rect_round(SCREEN_W - 60, 105, SCREEN_W - 20, 145, 6, 0xFF16213e);
        draw_text(SCREEN_W - 52, 114, "下", 0xFFFFFFFF, 1);
    }
}

static void render_bmpview_page() {
    draw_rect(0, 50, SCREEN_W, SCREEN_H - 36, 0xFF000000);
    if (bmp_index >= 0 && bmp_index < bmp_count) {
        char fpath[512];
        snprintf(fpath, sizeof(fpath), "%s/%s", BMP_DIR, bmp_files[bmp_index]);
        draw_bmp(0, 50, SCREEN_W, SCREEN_H - 86, fpath);
        draw_rect(0, 50, SCREEN_W, 80, 0xCC000000);
        draw_text(20, 65, bmp_files[bmp_index], 0xFFFFFFFF, 1);
    }

    /* Nav buttons */
    if (bmp_index > 0) {
        draw_rect_round(30, SCREEN_H - 80, 120, SCREEN_H - 42, 8, 0xCC16213e);
        draw_text(50, SCREEN_H - 72, "上一", 0xFFFFFFFF, 1);
    }
    if (bmp_index < bmp_count - 1) {
        draw_rect_round(SCREEN_W - 150, SCREEN_H - 80, SCREEN_W - 50, SCREEN_H - 42, 8, 0xCC16213e);
        draw_text(SCREEN_W - 130, SCREEN_H - 72, "下一", 0xFFFFFFFF, 1);
    }
}

static void render_settings_page() {
    draw_rect(0, 50, SCREEN_W, SCREEN_H - 36, 0xFF0f0f23);
    draw_text(20, 60, "设置", 0xFFFFFFFF, 2);

    /* WiFi Section header */
    draw_rect_round(20, 100, SCREEN_W - 20, 155, 8, 0xFF1a1a2e);
    draw_text(30, 115, "网络设置", 0xFFFFFFFF, 1);

    /* Scan button */
    draw_rect_round(SCREEN_W - 160, 105, SCREEN_W - 30, 148, 8, 0xFF0f3460);
    draw_text(SCREEN_W - 148, 118, "扫描网络", 0xFFFFFFFF, 1);

    /* WiFi list */
    if (wifi_scanning) {
        draw_text(30, 175, "正在扫描...", 0xFFf9a826, 1);
    } else if (wifi_count == 0) {
        draw_text(30, 175, "无网络", 0xFF888888, 1);
    } else {
        for (int i = 0; i < wifi_count && i < 6; i++) {
            int dy = 165 + i * 55;
            draw_rect_round(30, dy, SCREEN_W - 30, dy + 45, 6, 0xFF1a1a2e);
            draw_text(50, dy + 14, wifi_ssid[i], 0xFFFFFFFF, 1);

            /* Signal bars */
            int sig = wifi_signal[i] + 100;
            int bars = sig / 17;
            if (bars > 5) bars = 5;
            if (bars < 1) bars = 1;
            for (int b = 0; b < bars; b++)
                draw_rect(SCREEN_W - 230 + b*16, dy + 30 - b*4, SCREEN_W - 222 + b*16, dy + 34, 0xFF0a97b0);

            /* Connect button */
            draw_rect_round(SCREEN_W - 160, dy + 5, SCREEN_W - 50, dy + 40, 6, 0xFF0a97b0);
            draw_text(SCREEN_W - 140, dy + 14, "连接", 0xFFFFFFFF, 1);
        }
    }

    /* System Info */
    draw_rect_round(20, SCREEN_H - 80, SCREEN_W - 20, SCREEN_H - 45, 6, 0xFF1a1a2e);
    draw_text(30, SCREEN_H - 72, "IP: 10.200.67.239  |  v1.0.0", 0xFF888888, 1);
}

static void render_page() {
    // Clear top bar region (0 to 50) with black/dark color
    draw_rect(0, 0, SCREEN_W, 50, 0xFF0f0f23);

    switch (cur_page) {
        case PAGE_HOME:     render_home_page(); break;
        case PAGE_VOICE:    render_voice_page(); break;
        case PAGE_SMART:    render_smart_page(); break;
        case PAGE_IMAGES:   render_images_page(); break;
        case PAGE_SETTINGS: render_settings_page(); break;
        case PAGE_BMPVIEW:  render_bmpview_page(); break;
    }

    // Draw the universal Back button if not on Home page
    if (cur_page != PAGE_HOME) {
        draw_rect_round(back_btn.x1, back_btn.y1, back_btn.x2, back_btn.y2, 8, 0xFFe94560);
        draw_text(back_btn.x1 + 24, back_btn.y1 + 10, "返回", 0xFFFFFFFF, 1);
    }

    draw_bottom_status();
}


/* ===== Touch Handler ===== */
static void handle_touch(int px, int py) {
    if (pt_in_rect(px, py, back_btn) && cur_page != PAGE_HOME) {
        if (cur_page == PAGE_BMPVIEW) {
            cur_page = PAGE_IMAGES;
        } else {
            cur_page = PAGE_HOME;
        }
        snprintf(status_msg, sizeof(status_msg), "AI ready");
        ui_needs_redraw = 1;
        return;
    }
    switch (cur_page) {
        case PAGE_HOME:
            if (pt_in_rect(px, py, home_voice_btn)) {
                cur_page = PAGE_VOICE;
                snprintf(status_msg, sizeof(status_msg), "Voice Assistant");
                ui_needs_redraw = 1;
            } else if (pt_in_rect(px, py, home_smart_btn)) {
                cur_page = PAGE_SMART;
                snprintf(status_msg, sizeof(status_msg), "Smart Home");
                ui_needs_redraw = 1;
                // Fetch weather immediately when entering the weather page
                pthread_t tid;
                pthread_create(&tid, NULL, weather_thread_func, NULL);
                pthread_detach(tid);
            } else if (pt_in_rect(px, py, home_images_btn)) {
                scan_bmp_dir();
                bmp_scroll = 0;
                cur_page = PAGE_IMAGES;
                snprintf(status_msg, sizeof(status_msg), "Image Browser");
                ui_needs_redraw = 1;
            } else if (pt_in_rect(px, py, home_sett_btn)) {
                cur_page = PAGE_SETTINGS;
                snprintf(status_msg, sizeof(status_msg), "Settings");
                ui_needs_redraw = 1;
            }
            break;
        case PAGE_VOICE:
            if (pt_in_rect(px, py, voice_mic_btn)) {
                if (!rec_state) {
                    pthread_t tid;
                    pthread_create(&tid, NULL, record_thread_func, NULL);
                    pthread_detach(tid);
                }
            } else if (pt_in_rect(px, py, voice_send_btn)) {
                pthread_t tid;
                pthread_create(&tid, NULL, send_thread_func, NULL);
                pthread_detach(tid);
            }
            break;
        case PAGE_SMART:
            // Click anywhere to refresh weather
            if (py >= 100 && py <= 500) {
                pthread_t tid;
                pthread_create(&tid, NULL, weather_thread_func, NULL);
                pthread_detach(tid);
            }
            break;
        case PAGE_IMAGES:
            if (pt_in_rect(px, py, img_up_btn) && bmp_scroll > 0) {
                bmp_scroll--;
                ui_needs_redraw = 1;
            } else if (pt_in_rect(px, py, img_down_btn) && (bmp_scroll + 2) * 3 < bmp_count) {
                bmp_scroll++;
                ui_needs_redraw = 1;
            } else {
                for (int i = 0; i < 6; i++) {
                    if (pt_in_rect(px, py, img_thumbs[i])) {
                        int idx = i + bmp_scroll * 3;
                        if (idx < bmp_count) {
                            bmp_index = idx;
                            cur_subpage = cur_page;
                            cur_page = PAGE_BMPVIEW;
                            snprintf(status_msg, sizeof(status_msg), "%s", bmp_files[idx]);
                            ui_needs_redraw = 1;
                        }
                    }
                }
            }
            break;
        case PAGE_BMPVIEW:
            if (pt_in_rect(px, py, bmp_prev_btn) && bmp_index > 0) {
                bmp_index--;
                snprintf(status_msg, sizeof(status_msg), "%s", bmp_files[bmp_index]);
                ui_needs_redraw = 1;
            } else if (pt_in_rect(px, py, bmp_next_btn) && bmp_index < bmp_count - 1) {
                bmp_index++;
                snprintf(status_msg, sizeof(status_msg), "%s", bmp_files[bmp_index]);
                ui_needs_redraw = 1;
            }
            break;
        case PAGE_SETTINGS:
            if (pt_in_rect(px, py, sett_scan_btn)) {
                pthread_t tid;
                pthread_create(&tid, NULL, wifi_scan_thread_func, NULL);
                pthread_detach(tid);
            } else {
                for (int i = 0; i < 6 && i < wifi_count; i++) {
                    /* Dynamic hit region based on drawn layout */
                    rect_t conn_btn = {SCREEN_W - 160, 165 + i*55 + 5, SCREEN_W - 50, 165 + i*55 + 40};
                    if (pt_in_rect(px, py, conn_btn)) {
                        snprintf(status_msg, sizeof(status_msg), "已选定: %s", wifi_ssid[i]);
                        ui_needs_redraw = 1;
                    }
                }
            }
            break;
    }
}

/* ===== Touch Thread ===== */
void* touch_thread(void* arg) {
    touch_fd = open(TOUCH_DEVICE, O_RDONLY);
    if (touch_fd < 0) {
        printf("[Touch] Failed to open %s\n", TOUCH_DEVICE);
        return NULL;
    }
    struct input_event ev;
    int pressed = 0, last_x = 0, last_y = 0;
    while (1) {
        if (read(touch_fd, &ev, sizeof(ev)) == sizeof(ev)) {
            if (ev.type == EV_ABS) {
                if (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X) last_x = ev.value;
                if (ev.code == ABS_Y || ev.code == ABS_MT_POSITION_Y) last_y = ev.value;
            } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
                if (ev.value == 1) {
                    pressed = 1;
                } else if (ev.value == 0 && pressed) {
                    pressed = 0;
                    int px = touch_map_x(last_x);
                    int py = touch_map_y(last_y);
                    printf("[Touch] (%d,%d) -> (%d,%d)\n", last_x, last_y, px, py);
                    pthread_mutex_lock(&ui_lock);
                    ui_needs_redraw = 0;
                    handle_touch(px, py);
                    if (ui_needs_redraw) {
                        render_page();
                        DRMshowUp(lcd_fb, &DRM);
                    }
                    pthread_mutex_unlock(&ui_lock);
                }
            }
        }
    }
    close(touch_fd);
    return NULL;
}

/* ===== Main ===== */
int main(int argc, char* argv[]) {
    if (argc >= 2) {
        strncpy(g_wsl_ip, argv[1], sizeof(g_wsl_ip) - 1);
        g_wsl_ip[sizeof(g_wsl_ip) - 1] = '\0';
    }
    printf("[UI] Target WSL IP: %s\n", g_wsl_ip);

    printf("[UI] Initializing DRM...\n");
    lcd_fb = open(DRM_DEVICE, O_RDWR);
    if (lcd_fb < 0) {
        perror("[DRM] Open error");
        return 1;
    }
    DRMinit(lcd_fb);
    int ret = DRMcreateFB(lcd_fb, &DRM);
    if (ret < 0) {
        printf("[UI] DRM createFB failed: %d\n", ret);
        return 1;
    }
    printf("[UI] Screen %dx%d mapped\n", SCREEN_W, SCREEN_H);

    /* Initialize TrueType font engine with stb_truetype */
    if (font_init("/usr/share/fonts/truetype/wqy/wqy-microhei.ttc") == 0) {
        printf("[Font] wqy-microhei.ttc loaded OK\n");
    } else {
        printf("[Font] WARNING: TTF not found, text display may be broken\n");
    }

    /* Clear screen to dark background via DRM color fill */
    DRM_Color_Display(lcd_fb, 0x0f0f23);

    scan_bmp_dir();
    printf("[UI] Found %d BMP files\n", bmp_count);
    snprintf(chat_text[0], 256, "Welcome to AI Assistant");
    chat_lines = 1;
    
    // Start listener thread for AI responses
    pthread_t listen_tid;
    pthread_create(&listen_tid, NULL, listener_thread, NULL);
    pthread_detach(listen_tid);

    // Start WAV binary listener for SCP-fallback
    pthread_t wav_tid;
    pthread_create(&wav_tid, NULL, wav_listener_thread, NULL);
    pthread_detach(wav_tid);

    pthread_t tid;
    pthread_create(&tid, NULL, touch_thread, NULL);
    render_page();
    DRMshowUp(lcd_fb, &DRM);
    printf("[UI] Ready.\n");
    while (1) {
        sleep(1);
        if (msg_timer > 0) {
            msg_timer--;
            if (msg_timer == 0) {
                pthread_mutex_lock(&ui_lock);
                snprintf(status_msg, sizeof(status_msg), "AI ready");
                render_page();
                DRMshowUp(lcd_fb, &DRM);
                pthread_mutex_unlock(&ui_lock);
            }
        }
    }
    return 0;
}

