#ifndef UTILS_FILE_HELPER_H
#define UTILS_FILE_HELPER_H

#define MAX_FILES 50

int file_helper_get_files(const char *dir_path, const char *ext, char out_list[MAX_FILES][256], int max_count);
int file_helper_read_content(const char *filepath, char *out_buf, int buf_len);

#endif // UTILS_FILE_HELPER_H