#include "utils/file_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

int file_helper_get_files(const char *dir_path, const char *ext, char out_list[MAX_FILES][256], int max_count) {
    DIR *dir = opendir(dir_path);
    if (!dir) return 0;
    
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_count) {
        // 过滤普通文件
        if (entry->d_type == DT_REG) {
            // 判断后缀名
            char *dot = strrchr(entry->d_name, '.');
            if (dot && strcmp(dot, ext) == 0) {
                strncpy(out_list[count], entry->d_name, 255);
                out_list[count][255] = '\0';
                count++;
            }
        }
    }
    closedir(dir);
    return count;
}

int file_helper_read_content(const char *filepath, char *out_buf, int buf_len) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) return -1;
    
    int read_bytes = fread(out_buf, 1, buf_len - 1, fp);
    if (read_bytes >= 0) {
        out_buf[read_bytes] = '\0';
    }
    fclose(fp);
    return read_bytes;
}
