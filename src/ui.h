#ifndef UI_H
#define UI_H

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../DRMwrap.h"
#include "../inc/display.h"
#include "../inc/font_display.h"

#define SCREEN_W 1024
#define SCREEN_H 600
#define CONTROL_PORT 8888
#define WAV_DATA_PORT 8889
#define TOUCH_DEVICE "/dev/input/event2"
#define DRM_DEVICE "/dev/dri/card0"
#define PROJECT_DIR "/home/cyj/workspace/ai_assistant"
#define DEFAULT_IMAGE_DIR "/root/picture"
#define HOME_HERO_BMP PROJECT_DIR "/GGBond.bmp"
#define VOICE_RECORD_PATH "/root/code/cmd.wav"
#define VOICE_REPLY_PATH "/root/code/reply.wav"
#define REMOTE_CMD_WAV PROJECT_DIR "/bin/wav/cmd.wav"
#define REMOTE_AI_PIPELINE PROJECT_DIR "/ai_pipeline.py"
#define REMOTE_GET_WEATHER PROJECT_DIR "/get_weather.py"
#define WIFI_PASSWORD_FILE "/root/code/wifi_passwords.conf"
#define PAGE_HOME 0
#define PAGE_VOICE 1
#define PAGE_WEATHER 2
#define PAGE_IMAGES 3
#define PAGE_FILES 4
#define PAGE_SETTINGS 5
#define PAGE_BMPVIEW 6
#define MAX_CHAT 24
#define MAX_WIFI 20
#define MAX_IMAGE_ITEMS 128
#define MAX_FILE_ITEMS 256
#define MAX_STATUS_LEN 256
#define MAX_PATH_LEN 512

typedef struct { int x1; int y1; int x2; int y2; } rect_t;

typedef struct {
    char name[128];
    char path[MAX_PATH_LEN];
    char mtime[32];
    long size_bytes;
    int is_bmp;
} image_item_t;

typedef struct {
    char name[128];
    char path[MAX_PATH_LEN];
    char mtime[32];
    long size_bytes;
    int is_dir;
} file_item_t;

extern struct drmHandle DRM;
extern int lcd_fb;
extern int touch_fd;
extern pthread_mutex_t ui_lock;
extern int ts_min_x;
extern int ts_max_x;
extern int ts_min_y;
extern int ts_max_y;
extern int cur_page;
extern int last_page;
extern int ui_needs_redraw;
extern int keep_running;
extern char g_wsl_ip[64];
extern char local_ip[64];
extern char connected_ssid[64];
extern char selected_ssid[64];
extern char weather_info_wuhan[128];
extern char weather_info_beijing[128];
extern char weather_source[128];
extern char status_msg[MAX_STATUS_LEN];
extern char recognized_text[256];
extern char ai_reply_text[256];
extern char chat_text[MAX_CHAT][256];
extern int chat_lines;
extern int rec_state;
extern int send_state;
extern int wifi_scanning;
extern int weather_loading;
extern int wifi_count;
extern char wifi_ssid[MAX_WIFI][64];
extern int wifi_signal[MAX_WIFI];
extern int image_count;
extern int image_page;
extern int image_selected;
extern image_item_t image_items[MAX_IMAGE_ITEMS];
extern char image_root[MAX_PATH_LEN];
extern int file_count;
extern int file_page;
extern int file_selected;
extern file_item_t file_items[MAX_FILE_ITEMS];
extern char file_root[MAX_PATH_LEN];
extern char file_dir[MAX_PATH_LEN];

extern rect_t back_btn;
extern rect_t sidebar_btns[6];
extern rect_t home_cards[6];
extern rect_t home_entry_btns[5];
extern rect_t voice_mic_btn;
extern rect_t voice_send_btn;
extern rect_t voice_clear_btn;
extern rect_t weather_refresh_btn;
extern rect_t img_prev_btn;
extern rect_t img_next_btn;
extern rect_t img_thumb_btns[6];
extern rect_t bmp_prev_btn;
extern rect_t bmp_next_btn;
extern rect_t file_up_btn;
extern rect_t file_prev_btn;
extern rect_t file_next_btn;
extern rect_t file_row_btns[8];
extern rect_t sett_scan_btn;
extern rect_t sett_connect_btn;
extern rect_t sett_wifi_btns[6];

int pt_in_rect(int px, int py, rect_t r);
int touch_map_x(int raw);
int touch_map_y(int raw);
void init_touch_scaling(void);
void request_redraw(void);
void set_status(const char *fmt, ...);
void append_chat(const char *line);
void format_size(long size_bytes, char *out, size_t out_size);
int path_is_image(const char *name);
int path_is_bmp(const char *name);
void refresh_image_list(void);
void refresh_file_list(void);
void refresh_network_info(void);
void render_page(void);
void handle_touch(int px, int py);
void spawn_detached(void *(*fn)(void *), void *arg);
void draw_pixel(int x, int y, uint32_t color);
void draw_rect(int x1, int y1, int x2, int y2, uint32_t color);
void draw_rect_round(int x1, int y1, int x2, int y2, int r, uint32_t color);
void draw_text(int x, int y, const char *str, uint32_t color, int scale);
void draw_bmp(int dx, int dy, int dw, int dh, const char *path);
void draw_card(int x1, int y1, int x2, int y2, const char *title);
void draw_top_bar(const char *title, int show_back);
void draw_sidebar(int active_page);
void draw_status_footer(void);
void draw_button(rect_t r, const char *label, uint32_t fill);
void render_home_page(void);
void render_voice_page(void);
void render_weather_page(void);
void render_images_page(void);
void render_bmpview_page(void);
void render_files_page(void);
void render_settings_page(void);
void *touch_thread(void *arg);
void *record_thread_func(void *arg);
void *send_thread_func(void *arg);
void *wifi_scan_thread_func(void *arg);
void *wifi_connect_thread_func(void *arg);
void *weather_thread_func(void *arg);
void *listener_thread(void *arg);
void *wav_listener_thread(void *arg);
void *network_monitor_thread(void *arg);

#endif
