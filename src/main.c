#include "ui.h"

#include <ctype.h>
#include <stdarg.h>

struct drmHandle DRM;
int lcd_fb = -1;
int touch_fd = -1;
pthread_mutex_t ui_lock = PTHREAD_MUTEX_INITIALIZER;
int ts_min_x = 0;
int ts_max_x = 1023;
int ts_min_y = 0;
int ts_max_y = 599;
int cur_page = PAGE_HOME;
int last_page = PAGE_HOME;
int ui_needs_redraw = 1;
int keep_running = 1;
char g_wsl_ip[64] = "10.222.106.238";
char local_ip[64] = "";
char connected_ssid[64] = "";
char selected_ssid[64] = "";
char weather_info_wuhan[128] = "武汉: 获取中...";
char weather_info_beijing[128] = "北京: 获取中...";
char weather_source[128] = "等待 WSL 天气服务...";
char status_msg[MAX_STATUS_LEN] = "系统初始化中...";
char recognized_text[256] = "";
char ai_reply_text[256] = "";
char chat_text[MAX_CHAT][256];
int chat_lines = 0;
int rec_state = 0;
int send_state = 0;
int wifi_scanning = 0;
int weather_loading = 0;
int wifi_count = 0;
char wifi_ssid[MAX_WIFI][64];
int wifi_signal[MAX_WIFI];
int image_count = 0;
int image_page = 0;
int image_selected = -1;
image_item_t image_items[MAX_IMAGE_ITEMS];
char image_root[MAX_PATH_LEN] = DEFAULT_IMAGE_DIR;
int file_count = 0;
int file_page = 0;
int file_selected = -1;
file_item_t file_items[MAX_FILE_ITEMS];
char file_root[MAX_PATH_LEN] = PROJECT_DIR;
char file_dir[MAX_PATH_LEN] = PROJECT_DIR;

rect_t back_btn = {18, 12, 110, 50};
rect_t sidebar_btns[6] = {{8, 86, 150, 134}, {8, 158, 150, 206}, {8, 230, 150, 278}, {8, 302, 150, 350}, {8, 374, 150, 422}, {8, 446, 150, 494}};
rect_t home_cards[6] = {{178, 78, 518, 348}, {530, 78, 1006, 248}, {530, 266, 1006, 422}, {178, 366, 606, 494}, {618, 366, 1006, 494}, {618, 506, 1006, 566}};
rect_t home_entry_btns[5] = {{790, 188, 956, 230}, {790, 362, 980, 404}, {374, 442, 560, 484}, {790, 442, 976, 484}, {790, 518, 986, 558}};
rect_t voice_mic_btn = {198, 502, 376, 548};
rect_t voice_send_btn = {412, 502, 642, 548};
rect_t voice_clear_btn = {678, 502, 906, 548};
rect_t weather_refresh_btn = {798, 96, 968, 138};
rect_t img_prev_btn = {530, 96, 644, 138};
rect_t img_next_btn = {850, 96, 980, 138};
rect_t img_thumb_btns[6] = {{530, 160, 746, 256}, {760, 160, 976, 256}, {530, 272, 746, 368}, {760, 272, 976, 368}, {530, 384, 746, 480}, {760, 384, 976, 480}};
rect_t bmp_prev_btn = {220, 516, 348, 556};
rect_t bmp_next_btn = {812, 516, 954, 556};
rect_t file_up_btn = {836, 98, 982, 138};
rect_t file_prev_btn = {714, 190, 826, 230};
rect_t file_next_btn = {854, 190, 982, 230};
rect_t file_row_btns[8] = {{190, 250, 994, 286}, {190, 292, 994, 328}, {190, 334, 994, 370}, {190, 376, 994, 412}, {190, 418, 994, 454}, {190, 460, 994, 496}, {190, 502, 994, 538}, {190, 544, 994, 580}};
rect_t sett_scan_btn = {204, 344, 372, 388};
rect_t sett_connect_btn = {204, 404, 430, 448};
rect_t sett_wifi_btns[6] = {{610, 126, 984, 184}, {610, 196, 984, 254}, {610, 266, 984, 324}, {610, 336, 984, 394}, {610, 406, 984, 464}, {610, 476, 984, 534}};

static void safe_copy(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void sort_images(void)
{
    int i, j;
    for (i = 0; i < image_count; ++i) {
        for (j = i + 1; j < image_count; ++j) {
            if (strcmp(image_items[i].name, image_items[j].name) > 0) {
                image_item_t tmp = image_items[i];
                image_items[i] = image_items[j];
                image_items[j] = tmp;
            }
        }
    }
}

static void sort_files(void)
{
    int i, j;
    for (i = 0; i < file_count; ++i) {
        for (j = i + 1; j < file_count; ++j) {
            int swap = 0;
            if (file_items[i].is_dir != file_items[j].is_dir) swap = file_items[j].is_dir > file_items[i].is_dir;
            else if (strcmp(file_items[i].name, file_items[j].name) > 0) swap = 1;
            if (swap) {
                file_item_t tmp = file_items[i];
                file_items[i] = file_items[j];
                file_items[j] = tmp;
            }
        }
    }
}

static int shell_run_capture(const char *cmd, char *buf, size_t buf_size)
{
    FILE *fp;
    size_t len;
    if (!buf || buf_size == 0) return -1;
    buf[0] = '\0';
    fp = popen(cmd, "r");
    if (!fp) return -1;
    if (!fgets(buf, (int)buf_size, fp)) {
        pclose(fp);
        return -1;
    }
    pclose(fp);
    len = strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) buf[--len] = '\0';
    return 0;
}

static void get_file_mtime(const struct stat *st, char *buf, size_t buf_size)
{
    struct tm tm_info;
    localtime_r(&st->st_mtime, &tm_info);
    strftime(buf, buf_size, "%Y-%m-%d %H:%M", &tm_info);
}

static void goto_page(int page)
{
    if (cur_page == page) return;
    last_page = cur_page;
    cur_page = page;
    request_redraw();
}

static void ensure_roots(void)
{
    if (access(DEFAULT_IMAGE_DIR, R_OK) != 0) safe_copy(image_root, sizeof(image_root), PROJECT_DIR);
}

static void select_image(int idx)
{
    if (idx >= 0 && idx < image_count) {
        if (image_selected == idx) return;
        image_selected = idx;
        set_status("已选择图片: %s", image_items[idx].name);
    }
}

static void update_file_status(void)
{
    set_status("文件目录: %s", file_dir);
}

static int find_saved_network_id(const char *ssid, char *net_id, size_t net_id_size)
{
    FILE *fp;
    char line[256];
    if (!ssid || !ssid[0]) return -1;
    fp = popen("wpa_cli -p /var/run/wpa_supplicant -i wlan0 list_networks 2>/dev/null", "r");
    if (!fp) return -1;
    if (fgets(line, sizeof(line), fp)) {
    }
    while (fgets(line, sizeof(line), fp)) {
        char *tab1 = strchr(line, '\t');
        char *tab2;
        if (!tab1) continue;
        *tab1 = '\0';
        tab2 = strchr(tab1 + 1, '\t');
        if (!tab2) continue;
        *tab2 = '\0';
        if (strcmp(tab1 + 1, ssid) == 0) {
            safe_copy(net_id, net_id_size, line);
            pclose(fp);
            return 0;
        }
    }
    pclose(fp);
    return -1;
}

static int lookup_wifi_password(const char *ssid, char *password, size_t password_size)
{
    FILE *fp;
    char line[256];
    if (!ssid || !ssid[0]) return 0;
    fp = fopen(WIFI_PASSWORD_FILE, "r");
    if (!fp) return 0;
    while (fgets(line, sizeof(line), fp)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        line[strcspn(line, "\r\n")] = '\0';
        ++eq;
        eq[strcspn(eq, "\r\n")] = '\0';
        if (strcmp(line, ssid) == 0) {
            safe_copy(password, password_size, eq);
            fclose(fp);
            return password[0] != '\0';
        }
    }
    fclose(fp);
    return 0;
}

int pt_in_rect(int px, int py, rect_t r) { return px >= r.x1 && px <= r.x2 && py >= r.y1 && py <= r.y2; }
int touch_map_x(int raw) { return ts_max_x == ts_min_x ? raw : SCREEN_W * (raw - ts_min_x) / (ts_max_x - ts_min_x); }
int touch_map_y(int raw) { return ts_max_y == ts_min_y ? raw : SCREEN_H * (raw - ts_min_y) / (ts_max_y - ts_min_y); }
void request_redraw(void) { ui_needs_redraw = 1; }

void set_status(const char *fmt, ...)
{
    char next[MAX_STATUS_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(next, sizeof(next), fmt, ap);
    va_end(ap);
    if (strcmp(status_msg, next) == 0) return;
    safe_copy(status_msg, sizeof(status_msg), next);
    request_redraw();
}

void append_chat(const char *line)
{
    if (!line || !line[0]) return;
    if (chat_lines < MAX_CHAT) {
        safe_copy(chat_text[chat_lines], sizeof(chat_text[chat_lines]), line);
        ++chat_lines;
        return;
    }
    memmove(chat_text, chat_text + 1, sizeof(chat_text[0]) * (MAX_CHAT - 1));
    safe_copy(chat_text[MAX_CHAT - 1], sizeof(chat_text[MAX_CHAT - 1]), line);
}

void format_size(long size_bytes, char *out, size_t out_size)
{
    if (size_bytes >= 1024 * 1024) snprintf(out, out_size, "%.1f MB", size_bytes / 1024.0 / 1024.0);
    else if (size_bytes >= 1024) snprintf(out, out_size, "%.1f KB", size_bytes / 1024.0);
    else snprintf(out, out_size, "%ld B", size_bytes);
}

int path_is_image(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot && (!strcasecmp(dot, ".bmp") || !strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg") || !strcasecmp(dot, ".png"));
}

int path_is_bmp(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot && !strcasecmp(dot, ".bmp");
}

void refresh_image_list(void)
{
    DIR *dir;
    struct dirent *ent;
    struct stat st;
    char path[MAX_PATH_LEN];
    image_count = 0;
    dir = opendir(image_root);
    if (!dir) return;
    while ((ent = readdir(dir)) && image_count < MAX_IMAGE_ITEMS) {
        if (ent->d_name[0] == '.' || !path_is_image(ent->d_name)) continue;
        if (snprintf(path, sizeof(path), "%s/%s", image_root, ent->d_name) >= (int)sizeof(path)) continue;
        if (stat(path, &st) != 0) continue;
        safe_copy(image_items[image_count].name, sizeof(image_items[image_count].name), ent->d_name);
        safe_copy(image_items[image_count].path, sizeof(image_items[image_count].path), path);
        get_file_mtime(&st, image_items[image_count].mtime, sizeof(image_items[image_count].mtime));
        image_items[image_count].size_bytes = (long)st.st_size;
        image_items[image_count].is_bmp = path_is_bmp(ent->d_name);
        ++image_count;
    }
    closedir(dir);
    sort_images();
    if (image_count > 0 && image_selected < 0) image_selected = 0;
    else if (image_selected >= image_count) image_selected = image_count - 1;
}

void refresh_file_list(void)
{
    DIR *dir;
    struct dirent *ent;
    struct stat st;
    char path[MAX_PATH_LEN];
    file_count = 0;
    dir = opendir(file_dir);
    if (!dir) return;
    while ((ent = readdir(dir)) && file_count < MAX_FILE_ITEMS) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        if (snprintf(path, sizeof(path), "%s/%s", file_dir, ent->d_name) >= (int)sizeof(path)) continue;
        if (stat(path, &st) != 0) continue;
        safe_copy(file_items[file_count].name, sizeof(file_items[file_count].name), ent->d_name);
        safe_copy(file_items[file_count].path, sizeof(file_items[file_count].path), path);
        get_file_mtime(&st, file_items[file_count].mtime, sizeof(file_items[file_count].mtime));
        file_items[file_count].size_bytes = (long)st.st_size;
        file_items[file_count].is_dir = S_ISDIR(st.st_mode);
        ++file_count;
    }
    closedir(dir);
    sort_files();
    if (file_selected >= file_count) file_selected = file_count - 1;
}

void refresh_network_info(void)
{
    char ip_buf[64] = "";
    char ssid_buf[64] = "";
    shell_run_capture("sh -lc \"iwgetid -r 2>/dev/null\"", ssid_buf, sizeof(ssid_buf));
    if (shell_run_capture("sh -lc \"ip -4 -o addr show scope global | sed -n 's/.* inet \\([0-9.]*\\)\\/.*/\\1/p' | head -n1\"", ip_buf, sizeof(ip_buf)) != 0) {
        shell_run_capture("sh -lc \"hostname -I 2>/dev/null | cut -d' ' -f1\"", ip_buf, sizeof(ip_buf));
    }
    if (strcmp(local_ip, ip_buf) != 0 || strcmp(connected_ssid, ssid_buf) != 0) {
        safe_copy(local_ip, sizeof(local_ip), ip_buf);
        safe_copy(connected_ssid, sizeof(connected_ssid), ssid_buf);
        request_redraw();
    }
}

void render_page(void)
{
    switch (cur_page) {
    case PAGE_HOME: render_home_page(); break;
    case PAGE_VOICE: render_voice_page(); break;
    case PAGE_WEATHER: render_weather_page(); break;
    case PAGE_IMAGES: render_images_page(); break;
    case PAGE_FILES: render_files_page(); break;
    case PAGE_SETTINGS: render_settings_page(); break;
    case PAGE_BMPVIEW: render_bmpview_page(); break;
    default: render_home_page(); break;
    }
}

void spawn_detached(void *(*fn)(void *), void *arg)
{
    pthread_t tid;
    if (pthread_create(&tid, NULL, fn, arg) == 0) pthread_detach(tid);
}

static void enter_file_item(int idx)
{
    if (idx < 0 || idx >= file_count) return;
    file_selected = idx;
    if (file_items[idx].is_dir) {
        safe_copy(file_dir, sizeof(file_dir), file_items[idx].path);
        file_page = 0;
        refresh_file_list();
        update_file_status();
    } else {
        set_status("已选中文件: %s", file_items[idx].name);
    }
    request_redraw();
}

static void goto_sidebar_index(int idx)
{
    if (idx == 0) goto_page(PAGE_HOME);
    else if (idx == 1) goto_page(PAGE_VOICE);
    else if (idx == 2) { goto_page(PAGE_WEATHER); spawn_detached(weather_thread_func, NULL); }
    else if (idx == 3) { refresh_image_list(); goto_page(PAGE_IMAGES); }
    else if (idx == 4) { refresh_file_list(); goto_page(PAGE_FILES); }
    else if (idx == 5) goto_page(PAGE_SETTINGS);
}

void handle_touch(int px, int py)
{
    int i;
    for (i = 0; i < 6; ++i) {
        if (pt_in_rect(px, py, sidebar_btns[i])) {
            int target = i;
            if (target == cur_page) return;
            goto_sidebar_index(i);
            return;
        }
    }
    if (cur_page != PAGE_HOME && pt_in_rect(px, py, back_btn)) {
        if (cur_page == PAGE_BMPVIEW) goto_page(PAGE_IMAGES);
        else goto_page(PAGE_HOME);
        return;
    }
    switch (cur_page) {
    case PAGE_HOME:
        for (i = 0; i < 5; ++i) {
            if (pt_in_rect(px, py, home_entry_btns[i])) {
                if (i == 0) goto_sidebar_index(1);
                else if (i == 1) goto_sidebar_index(2);
                else if (i == 2) goto_sidebar_index(3);
                else if (i == 3) goto_sidebar_index(4);
                else goto_sidebar_index(5);
                return;
            }
        }
        break;
    case PAGE_VOICE:
        if (pt_in_rect(px, py, voice_mic_btn) && !rec_state) spawn_detached(record_thread_func, NULL);
        else if (pt_in_rect(px, py, voice_send_btn) && !send_state) spawn_detached(send_thread_func, NULL);
        else if (pt_in_rect(px, py, voice_clear_btn)) { chat_lines = 0; recognized_text[0] = '\0'; ai_reply_text[0] = '\0'; set_status("已清空对话"); }
        break;
    case PAGE_WEATHER:
        if (pt_in_rect(px, py, weather_refresh_btn) && !weather_loading) spawn_detached(weather_thread_func, NULL);
        break;
    case PAGE_IMAGES:
        if (pt_in_rect(px, py, img_prev_btn) && image_page > 0) { --image_page; request_redraw(); }
        else if (pt_in_rect(px, py, img_next_btn) && (image_page + 1) * 6 < image_count) { ++image_page; request_redraw(); }
        else {
            int base = image_page * 6;
            for (i = 0; i < 6; ++i) {
                if (pt_in_rect(px, py, img_thumb_btns[i])) {
                    select_image(base + i);
                    if (base + i < image_count) goto_page(PAGE_BMPVIEW);
                    break;
                }
            }
        }
        break;
    case PAGE_BMPVIEW:
        if (pt_in_rect(px, py, bmp_prev_btn) && image_selected > 0) select_image(image_selected - 1);
        else if (pt_in_rect(px, py, bmp_next_btn) && image_selected + 1 < image_count) select_image(image_selected + 1);
        break;
    case PAGE_FILES:
        if (pt_in_rect(px, py, file_up_btn)) {
            if (strcmp(file_dir, file_root) != 0) {
                char *slash = strrchr(file_dir, '/');
                if (slash && slash != file_dir) {
                    *slash = '\0';
                    refresh_file_list();
                    file_page = 0;
                    update_file_status();
                }
            }
        } else if (pt_in_rect(px, py, file_prev_btn) && file_page > 0) {
            --file_page;
            request_redraw();
        } else if (pt_in_rect(px, py, file_next_btn) && (file_page + 1) * 8 < file_count) {
            ++file_page;
            request_redraw();
        } else {
            int base = file_page * 8;
            for (i = 0; i < 8; ++i) if (pt_in_rect(px, py, file_row_btns[i])) { enter_file_item(base + i); break; }
        }
        break;
    case PAGE_SETTINGS:
        if (pt_in_rect(px, py, sett_scan_btn) && !wifi_scanning) spawn_detached(wifi_scan_thread_func, NULL);
        else if (pt_in_rect(px, py, sett_connect_btn) && selected_ssid[0]) spawn_detached(wifi_connect_thread_func, NULL);
        else for (i = 0; i < 6; ++i) if (pt_in_rect(px, py, sett_wifi_btns[i]) && i < wifi_count) { safe_copy(selected_ssid, sizeof(selected_ssid), wifi_ssid[i]); set_status("已选择 WiFi: %s", selected_ssid); break; }
        break;
    default:
        break;
    }
}

void init_touch_scaling(void)
{
    struct input_absinfo ai;
    if (ioctl(touch_fd, EVIOCGABS(ABS_X), &ai) == 0 || ioctl(touch_fd, EVIOCGABS(ABS_MT_POSITION_X), &ai) == 0) { ts_min_x = ai.minimum; ts_max_x = ai.maximum; }
    if (ioctl(touch_fd, EVIOCGABS(ABS_Y), &ai) == 0 || ioctl(touch_fd, EVIOCGABS(ABS_MT_POSITION_Y), &ai) == 0) { ts_min_y = ai.minimum; ts_max_y = ai.maximum; }
}

void *touch_thread(void *arg)
{
    struct input_event ev;
    int last_x = 0, last_y = 0;
    (void)arg;
    touch_fd = open(TOUCH_DEVICE, O_RDONLY);
    if (touch_fd < 0) { set_status("触摸设备打开失败: %s", TOUCH_DEVICE); return NULL; }
    init_touch_scaling();
    while (keep_running) {
        if (read(touch_fd, &ev, sizeof(ev)) != sizeof(ev)) continue;
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X) last_x = ev.value;
            if (ev.code == ABS_Y || ev.code == ABS_MT_POSITION_Y) last_y = ev.value;
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH && ev.value == 0) {
            pthread_mutex_lock(&ui_lock);
            handle_touch(touch_map_x(last_x), touch_map_y(last_y));
            pthread_mutex_unlock(&ui_lock);
        }
    }
    close(touch_fd);
    return NULL;
}

void *record_thread_func(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&ui_lock);
    rec_state = 1;
    set_status("正在录音 3 秒...");
    pthread_mutex_unlock(&ui_lock);
    system("mkdir -p /root/code >/dev/null 2>&1");
    system("arecord -D plughw:1,0 -d 3 -c 1 -r 16000 -t wav -f S16_LE " VOICE_RECORD_PATH " >/dev/null 2>&1");
    pthread_mutex_lock(&ui_lock);
    rec_state = 0;
    set_status("录音完成，可以发送到 WSL");
    pthread_mutex_unlock(&ui_lock);
    return NULL;
}

void *send_thread_func(void *arg)
{
    char wsl_ip[64], board_ip[64], cmd[1024];
    int ok = 1;
    (void)arg;
    pthread_mutex_lock(&ui_lock);
    send_state = 1;
    safe_copy(wsl_ip, sizeof(wsl_ip), g_wsl_ip);
    safe_copy(board_ip, sizeof(board_ip), local_ip);
    append_chat("> 语音已发送，等待识别");
    set_status("准备发送录音并触发远端 AI 流水线...");
    pthread_mutex_unlock(&ui_lock);

    snprintf(cmd, sizeof(cmd), "scp -o StrictHostKeyChecking=no -o ConnectTimeout=10 " VOICE_RECORD_PATH " cyj@%s:%s >/dev/null 2>&1", wsl_ip, REMOTE_CMD_WAV);
    if (system(cmd) != 0) ok = 0;
    if (ok) {
        snprintf(cmd, sizeof(cmd), "ssh -o StrictHostKeyChecking=no cyj@%s 'python3 %s %s' >/tmp/ai_pipeline.log 2>&1", wsl_ip, REMOTE_AI_PIPELINE, board_ip[0] ? board_ip : "127.0.0.1");
        if (system(cmd) != 0) ok = 0;
    }

    pthread_mutex_lock(&ui_lock);
    send_state = 0;
    set_status(ok ? "已触发 WSL AI 流水线，等待回传文字和语音" : "发送失败，请检查 WSL IP、SSH 与录音文件");
    pthread_mutex_unlock(&ui_lock);
    return NULL;
}

void *wifi_scan_thread_func(void *arg)
{
    FILE *fp;
    char buf[512];
    char local_ssid[MAX_WIFI][64];
    int local_signal[MAX_WIFI];
    int local_count = 0;

    (void)arg;
    pthread_mutex_lock(&ui_lock);
    wifi_scanning = 1;
    set_status("正在扫描 WiFi...");
    pthread_mutex_unlock(&ui_lock);

    system("wpa_cli -p /var/run/wpa_supplicant -i wlan0 scan >/dev/null 2>&1");
    usleep(1500000);
    fp = popen("wpa_cli -p /var/run/wpa_supplicant -i wlan0 scan_results 2>/dev/null", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
        }
        while (fgets(buf, sizeof(buf), fp) && local_count < MAX_WIFI) {
            char *tab1 = strchr(buf, '\t');
            char *tab2, *tab3, *tab4, *ssid_ptr;
            int signal;
            if (!tab1) continue;
            *tab1 = '\0';
            tab2 = strchr(tab1 + 1, '\t'); if (!tab2) continue; *tab2 = '\0';
            tab3 = strchr(tab2 + 1, '\t'); if (!tab3) continue; *tab3 = '\0';
            tab4 = strchr(tab3 + 1, '\t'); if (!tab4) continue; *tab4 = '\0';
            signal = atoi(tab2 + 1);
            ssid_ptr = tab4 + 1;
            ssid_ptr[strcspn(ssid_ptr, "\r\n")] = '\0';
            if (!ssid_ptr[0]) continue;
            safe_copy(local_ssid[local_count], sizeof(local_ssid[0]), ssid_ptr);
            local_signal[local_count] = signal;
            ++local_count;
        }
        pclose(fp);
    }

    pthread_mutex_lock(&ui_lock);
    wifi_count = local_count;
    memcpy(wifi_ssid, local_ssid, sizeof(local_ssid));
    memcpy(wifi_signal, local_signal, sizeof(local_signal));
    wifi_scanning = 0;
    set_status("WiFi 扫描完成，发现 %d 个网络", local_count);
    pthread_mutex_unlock(&ui_lock);
    return NULL;
}

void *wifi_connect_thread_func(void *arg)
{
    char ssid[64];
    char net_id[32] = "";
    char password[128] = "";
    char out[128];
    char cmd[1024];
    int ok = 0;
    int has_password;

    (void)arg;
    pthread_mutex_lock(&ui_lock);
    safe_copy(ssid, sizeof(ssid), selected_ssid);
    set_status("尝试连接 WiFi: %s", ssid);
    pthread_mutex_unlock(&ui_lock);
    if (!ssid[0]) return NULL;

    has_password = lookup_wifi_password(ssid, password, sizeof(password));
    if (find_saved_network_id(ssid, net_id, sizeof(net_id)) == 0 && net_id[0]) {
        snprintf(cmd, sizeof(cmd), "wpa_cli -p /var/run/wpa_supplicant -i wlan0 select_network %s >/dev/null 2>&1 && wpa_cli -p /var/run/wpa_supplicant -i wlan0 reassociate >/dev/null 2>&1", net_id);
        ok = system(cmd) == 0;
    } else if (has_password) {
        snprintf(cmd, sizeof(cmd), "sh -lc 'nid=$(wpa_cli -p /var/run/wpa_supplicant -i wlan0 add_network | tail -n1); wpa_cli -p /var/run/wpa_supplicant -i wlan0 set_network \"$nid\" ssid '\"%s\"' >/dev/null 2>&1; wpa_cli -p /var/run/wpa_supplicant -i wlan0 set_network \"$nid\" psk '\"%s\"' >/dev/null 2>&1; wpa_cli -p /var/run/wpa_supplicant -i wlan0 enable_network \"$nid\" >/dev/null 2>&1; wpa_cli -p /var/run/wpa_supplicant -i wlan0 save_config >/dev/null 2>&1'", ssid, password);
        ok = system(cmd) == 0;
    } else {
        snprintf(cmd, sizeof(cmd), "sh -lc 'nid=$(wpa_cli -p /var/run/wpa_supplicant -i wlan0 add_network | tail -n1); wpa_cli -p /var/run/wpa_supplicant -i wlan0 set_network \"$nid\" ssid '\"%s\"' >/dev/null 2>&1; wpa_cli -p /var/run/wpa_supplicant -i wlan0 set_network \"$nid\" key_mgmt NONE >/dev/null 2>&1; wpa_cli -p /var/run/wpa_supplicant -i wlan0 enable_network \"$nid\" >/dev/null 2>&1; wpa_cli -p /var/run/wpa_supplicant -i wlan0 save_config >/dev/null 2>&1'", ssid);
        ok = system(cmd) == 0;
    }

    usleep(2000000);
    refresh_network_info();
    safe_copy(out, sizeof(out), connected_ssid);
    pthread_mutex_lock(&ui_lock);
    if (ok && out[0] && strcmp(out, ssid) == 0) {
        set_status("WiFi 已连接: %s", ssid);
    } else if (!has_password) {
        set_status("WiFi 未连上: %s。开放网络可直连；加密网络请写入 %s", ssid, WIFI_PASSWORD_FILE);
    } else {
        set_status("WiFi 未连上: %s。请检查保存密码。", ssid);
    }
    pthread_mutex_unlock(&ui_lock);
    return NULL;
}

void *weather_thread_func(void *arg)
{
    char cmd[1024];
    char result[256];
    char *sep;
    (void)arg;
    pthread_mutex_lock(&ui_lock);
    weather_loading = 1;
    set_status("正在通过 WSL 获取天气...");
    pthread_mutex_unlock(&ui_lock);
    snprintf(cmd, sizeof(cmd), "ssh -o StrictHostKeyChecking=no cyj@%s 'python3 %s' 2>/dev/null", g_wsl_ip, REMOTE_GET_WEATHER);
    if (shell_run_capture(cmd, result, sizeof(result)) == 0) {
        sep = strchr(result, '|');
        if (sep) {
            *sep = '\0';
            pthread_mutex_lock(&ui_lock);
            safe_copy(weather_info_wuhan, sizeof(weather_info_wuhan), result);
            safe_copy(weather_info_beijing, sizeof(weather_info_beijing), sep + 1);
            snprintf(weather_source, sizeof(weather_source), "WSL %s 返回成功", g_wsl_ip);
            weather_loading = 0;
            set_status("天气已更新");
            pthread_mutex_unlock(&ui_lock);
            return NULL;
        }
    }
    pthread_mutex_lock(&ui_lock);
    weather_loading = 0;
    snprintf(weather_source, sizeof(weather_source), "WSL %s 查询失败", g_wsl_ip);
    set_status("天气获取失败，请检查 WSL 服务器");
    pthread_mutex_unlock(&ui_lock);
    return NULL;
}

void *listener_thread(void *arg)
{
    int server_fd, opt = 1;
    struct sockaddr_in addr;
    (void)arg;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return NULL;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(CONTROL_PORT);
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(server_fd); return NULL; }
    if (listen(server_fd, 5) < 0) { close(server_fd); return NULL; }
    while (keep_running) {
        int csock, len;
        char buf[1024];
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        csock = accept(server_fd, (struct sockaddr *)&caddr, &clen);
        if (csock < 0) continue;
        len = recv(csock, buf, sizeof(buf) - 1, 0);
        close(csock);
        if (len <= 0) continue;
        buf[len] = '\0';
        pthread_mutex_lock(&ui_lock);
        if (!strncmp(buf, "ASR:", 4)) {
            safe_copy(recognized_text, sizeof(recognized_text), buf + 4);
            append_chat(buf + 4);
            set_status("已收到识别文本");
        } else if (!strncmp(buf, "AI:", 3)) {
            safe_copy(ai_reply_text, sizeof(ai_reply_text), buf + 3);
            snprintf(buf, sizeof(buf), "< AI: %s", ai_reply_text);
            append_chat(buf);
            set_status("已收到 AI 回复，等待音频或正在播放");
        } else {
            safe_copy(ai_reply_text, sizeof(ai_reply_text), buf);
            snprintf(buf, sizeof(buf), "< AI: %s", ai_reply_text);
            append_chat(buf);
            set_status("已收到 AI 回复");
        }
        pthread_mutex_unlock(&ui_lock);
    }
    close(server_fd);
    return NULL;
}

void *wav_listener_thread(void *arg)
{
    int server_fd, opt = 1;
    struct sockaddr_in addr;
    (void)arg;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return NULL;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(WAV_DATA_PORT);
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(server_fd); return NULL; }
    if (listen(server_fd, 3) < 0) { close(server_fd); return NULL; }
    while (keep_running) {
        int csock;
        uint32_t data_len;
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        FILE *fp;
        char chunk[8192];
        uint32_t remaining;
        csock = accept(server_fd, (struct sockaddr *)&caddr, &clen);
        if (csock < 0) continue;
        if (recv(csock, &data_len, 4, MSG_WAITALL) != 4) { close(csock); continue; }
        data_len = ntohl(data_len);
        if (data_len == 0 || data_len > 4 * 1024 * 1024) { close(csock); continue; }
        fp = fopen(VOICE_REPLY_PATH, "wb");
        if (!fp) { close(csock); continue; }
        remaining = data_len;
        while (remaining > 0) {
            int want = remaining > sizeof(chunk) ? (int)sizeof(chunk) : (int)remaining;
            int got = recv(csock, chunk, want, 0);
            if (got <= 0) break;
            fwrite(chunk, 1, got, fp);
            remaining -= got;
        }
        fclose(fp);
        close(csock);
        system("aplay -D plughw:1,0 " VOICE_REPLY_PATH " >/dev/null 2>&1 &");
        pthread_mutex_lock(&ui_lock);
        set_status("已收到 TTS 语音并播放");
        pthread_mutex_unlock(&ui_lock);
    }
    close(server_fd);
    return NULL;
}

void *network_monitor_thread(void *arg)
{
    (void)arg;
    while (keep_running) {
        pthread_mutex_lock(&ui_lock);
        refresh_network_info();
        pthread_mutex_unlock(&ui_lock);
        sleep(3);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    setenv("TZ", "Asia/Shanghai", 1);
    tzset();
    if (argc > 1) safe_copy(g_wsl_ip, sizeof(g_wsl_ip), argv[1]);
    ensure_roots();
    refresh_image_list();
    refresh_file_list();
    refresh_network_info();
    append_chat("系统已启动");
    set_status("主界面已就绪");

    lcd_fb = open(DRM_DEVICE, O_RDWR);
    if (lcd_fb < 0) { perror("open drm"); return 1; }
    DRMinit(lcd_fb);
    if (DRMcreateFB(lcd_fb, &DRM) < 0) { perror("DRMcreateFB"); close(lcd_fb); return 1; }
    if (font_init("/usr/share/fonts/truetype/wqy/wqy-microhei.ttc") != 0) font_init("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc");

    spawn_detached(listener_thread, NULL);
    spawn_detached(wav_listener_thread, NULL);
    spawn_detached(touch_thread, NULL);
    spawn_detached(network_monitor_thread, NULL);
    spawn_detached(weather_thread_func, NULL);
    spawn_detached(wifi_scan_thread_func, NULL);

    while (keep_running) {
        pthread_mutex_lock(&ui_lock);
        if (ui_needs_redraw) {
            render_page();
            DRMshowUp(lcd_fb, &DRM);
            ui_needs_redraw = 0;
        }
        pthread_mutex_unlock(&ui_lock);
        usleep(30000);
    }

    font_done();
    DRMfreeResources(lcd_fb, &DRM);
    close(lcd_fb);
    return 0;
}


