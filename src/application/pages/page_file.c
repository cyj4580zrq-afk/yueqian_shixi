#include "application/page_common.h"
#include "common/config.h"
#include "driver/display_drv.h"
#include "hal/display_hal.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

extern char g_local_ip[64];
extern char g_ssid[64];
extern app_config_t g_config;

#define FILE_ROW_COUNT 6
#define MAX_FILE_ITEMS 256
#define MAX_PATH_LEN 512

typedef struct {
    char name[128];
    char path[MAX_PATH_LEN];
    char mtime[32];
    long size_bytes;
    int is_dir;
} browser_item_t;

static browser_item_t g_items[MAX_FILE_ITEMS];
static int g_item_count = -1;
static int g_item_page = 0;
static int g_selected_index = -1;
static char g_root_dir[MAX_PATH_LEN] = "";
static char g_current_dir[MAX_PATH_LEN] = "";
static char g_preview[256] = "";

extern void request_redraw(void);

static void safe_copy(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void normalize_root(void)
{
    if (g_root_dir[0]) return;
    safe_copy(g_root_dir, sizeof(g_root_dir), g_config.file_browser_root[0] ? g_config.file_browser_root : ".");
    safe_copy(g_current_dir, sizeof(g_current_dir), g_root_dir);
}

static void get_mtime(const struct stat *st, char *buf, size_t buf_size)
{
    struct tm tm_info;
    localtime_r(&st->st_mtime, &tm_info);
    strftime(buf, buf_size, "%Y-%m-%d %H:%M", &tm_info);
}

static void format_size(long size_bytes, char *out, size_t out_size)
{
    if (size_bytes >= 1024 * 1024) snprintf(out, out_size, "%.1f MB", size_bytes / 1024.0 / 1024.0);
    else if (size_bytes >= 1024) snprintf(out, out_size, "%.1f KB", size_bytes / 1024.0);
    else snprintf(out, out_size, "%ld B", size_bytes);
}

static void sort_items(void)
{
    for (int i = 0; i < g_item_count; ++i) {
        for (int j = i + 1; j < g_item_count; ++j) {
            int swap = 0;
            if (g_items[i].is_dir != g_items[j].is_dir) swap = g_items[j].is_dir > g_items[i].is_dir;
            else if (strcmp(g_items[i].name, g_items[j].name) > 0) swap = 1;
            if (swap) {
                browser_item_t tmp = g_items[i];
                g_items[i] = g_items[j];
                g_items[j] = tmp;
            }
        }
    }
}

static void set_preview(const browser_item_t *item)
{
    if (!item) {
        g_preview[0] = '\0';
        return;
    }
    if (item->is_dir) snprintf(g_preview, sizeof(g_preview), "目录：%s", item->path);
    else snprintf(g_preview, sizeof(g_preview), "文件：%s | %ld 字节", item->path, item->size_bytes);
}

static void load_dir(const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    struct dirent *ent;
    struct stat st;
    char path[MAX_PATH_LEN];

    g_item_count = 0;
    if (!dir) {
        snprintf(g_preview, sizeof(g_preview), "无法打开目录：%s", dir_path);
        return;
    }

    while ((ent = readdir(dir)) != NULL && g_item_count < MAX_FILE_ITEMS) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        if (snprintf(path, sizeof(path), "%s/%s", dir_path, ent->d_name) >= (int)sizeof(path)) continue;
        if (stat(path, &st) != 0) continue;

        int is_dir = S_ISDIR(st.st_mode);

        safe_copy(g_items[g_item_count].name, sizeof(g_items[g_item_count].name), ent->d_name);
        safe_copy(g_items[g_item_count].path, sizeof(g_items[g_item_count].path), path);
        g_items[g_item_count].is_dir = is_dir;
        g_items[g_item_count].size_bytes = (long)st.st_size;
        get_mtime(&st, g_items[g_item_count].mtime, sizeof(g_items[g_item_count].mtime));
        ++g_item_count;
    }
    closedir(dir);
    sort_items();

    if (g_selected_index >= g_item_count) g_selected_index = g_item_count - 1;
    if (g_selected_index < 0 && g_item_count > 0) g_selected_index = 0;
    if (g_selected_index >= 0 && g_selected_index < g_item_count) set_preview(&g_items[g_selected_index]);
    else snprintf(g_preview, sizeof(g_preview), "当前目录：%s", g_current_dir);
}

static void refresh_browser(void)
{
    normalize_root();
    if (g_current_dir[0] == '\0') safe_copy(g_current_dir, sizeof(g_current_dir), g_root_dir);
    load_dir(g_current_dir);
    g_item_page = 0;
    request_redraw();
}

static void enter_dir(const char *path)
{
    safe_copy(g_current_dir, sizeof(g_current_dir), path);
    g_selected_index = -1;
    refresh_browser();
}

static void go_parent(void)
{
    size_t root_len;
    char parent[MAX_PATH_LEN];
    char *slash;

    normalize_root();
    if (strcmp(g_current_dir, g_root_dir) == 0) return;

    safe_copy(parent, sizeof(parent), g_current_dir);
    slash = strrchr(parent, '/');
    if (!slash) {
        safe_copy(g_current_dir, sizeof(g_current_dir), g_root_dir);
    } else if (slash == parent) {
        parent[1] = '\0';
        safe_copy(g_current_dir, sizeof(g_current_dir), parent);
    } else {
        *slash = '\0';
        safe_copy(g_current_dir, sizeof(g_current_dir), parent);
    }

    root_len = strlen(g_root_dir);
    if (strncmp(g_current_dir, g_root_dir, root_len) != 0) safe_copy(g_current_dir, sizeof(g_current_dir), g_root_dir);
    g_selected_index = -1;
    refresh_browser();
}

static void move_selection(int delta)
{
    int new_index;

    if (g_item_count <= 0) return;
    if (g_selected_index < 0) g_selected_index = 0;

    new_index = g_selected_index + delta;
    if (new_index < 0) new_index = 0;
    if (new_index >= g_item_count) new_index = g_item_count - 1;

    g_selected_index = new_index;
    g_item_page = g_selected_index / FILE_ROW_COUNT;
    set_preview(&g_items[g_selected_index]);
    request_redraw();
}
static void file_draw(void)
{
    int start = g_item_page * FILE_ROW_COUNT;
    char size_buf[32];
    char line[256];

    normalize_root();
    if (g_item_count < 0) refresh_browser();

    display_hal_draw_clear();
    display_hal_draw_framework(g_ssid, g_local_ip);

    display_hal_draw_card(250, 90, 720, 70, 0x1E1E1E, "文件查询范围", g_current_dir[0] ? g_current_dir : g_root_dir);
    display_hal_draw_button(860, 96, 140, 40, 0x2C4868, "返回上级", 0xFFFFFF);
    display_hal_draw_button(704, 96, 140, 40, 0x2C4868, "刷新", 0xFFFFFF);

    display_hal_draw_card(250, 170, 720, 280, 0x101820, "目录内容", "");
    display_hal_draw_text(270, 210, "名称", 0xFFFFFF);
    display_hal_draw_text(540, 210, "类型", 0xFFFFFF);
    display_hal_draw_text(642, 210, "大小", 0xFFFFFF);
    display_hal_draw_text(766, 210, "修改时间", 0xFFFFFF);

    for (int i = 0; i < FILE_ROW_COUNT; ++i) {
        int idx = start + i;
        int y1 = 238 + i * 34;
        if (idx < g_item_count) {
            const browser_item_t *item = &g_items[idx];
            uint32_t text_color = idx == g_selected_index ? 0x00FF00 : 0xFFFFFF;
            snprintf(line, sizeof(line), "%s%s", item->is_dir ? "[D] " : "[F] ", item->name);
            display_hal_draw_text(272, y1, line, text_color);
            display_hal_draw_text(540, y1, item->is_dir ? "目录" : "文件", text_color);
            format_size(item->size_bytes, size_buf, sizeof(size_buf));
            display_hal_draw_text(642, y1, item->is_dir ? "--" : size_buf, text_color);
            display_hal_draw_text(766, y1, item->mtime, 0x9BB8D2);
        }
    }

    display_hal_draw_card(250, 455, 720, 50, 0x1B1B2C, "选中项", "");
    display_hal_draw_text(270, 488, g_preview[0] ? g_preview : "暂无选择", 0xFFFFFF);

    display_hal_draw_button(250, 512, 110, 32, 0x2C4868, "上移", 0xFFFFFF);
    display_hal_draw_button(370, 512, 110, 32, 0x2C4868, "下移", 0xFFFFFF);
    if (g_item_page > 0) display_hal_draw_button(500, 512, 110, 32, 0x2C4868, "上一页", 0xFFFFFF);
    if ((g_item_page + 1) * FILE_ROW_COUNT < g_item_count) display_hal_draw_button(620, 512, 110, 32, 0x2C4868, "下一页", 0xFFFFFF);

    display_drv_flush();
}

static int hit_row(int x, int y, int *row_index)
{
    for (int i = 0; i < FILE_ROW_COUNT; ++i) {
        int y1 = 235 + i * 34;
        int y2 = y1 + 28;
        if (x >= 260 && x <= 958 && y >= y1 && y <= y2) {
            if (row_index) *row_index = i;
            return 1;
        }
    }
    return 0;
}

static void open_selected(int idx)
{
    if (idx < 0 || idx >= g_item_count) return;
    g_selected_index = idx;
    set_preview(&g_items[idx]);
    if (g_items[idx].is_dir) {
        enter_dir(g_items[idx].path);
    } else {
        snprintf(g_preview, sizeof(g_preview), "文件：%s", g_items[idx].path);
        request_redraw();
    }
}

static void file_handle_click(int x, int y, int event_type)
{
    int row = -1;
    int idx;

    if (event_type != 1) return;

    if (x >= 860 && x <= 1000 && y >= 96 && y <= 136) {
        go_parent();
        return;
    }
    if (x >= 704 && x <= 844 && y >= 96 && y <= 136) {
        refresh_browser();
        return;
    }
    if (x >= 250 && x <= 360 && y >= 512 && y <= 544) {
        move_selection(-1);
        return;
    }
    if (x >= 370 && x <= 480 && y >= 512 && y <= 544) {
        move_selection(1);
        return;
    }
    if (x >= 500 && x <= 610 && y >= 512 && y <= 544 && g_item_page > 0) {
        --g_item_page;
        request_redraw();
        return;
    }
    if (x >= 620 && x <= 730 && y >= 512 && y <= 544 && (g_item_page + 1) * FILE_ROW_COUNT < g_item_count) {
        ++g_item_page;
        request_redraw();
        return;
    }
    if (hit_row(x, y, &row)) {
        idx = g_item_page * FILE_ROW_COUNT + row;
        open_selected(idx);
    }
}

static app_page_t g_file_page = {
    .draw = file_draw,
    .handle_click = file_handle_click
};

app_page_t *page_file_get_instance(void)
{
    return &g_file_page;
}




