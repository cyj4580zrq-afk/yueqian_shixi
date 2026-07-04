#include "ui.h"

void render_files_page(void)
{
    int i, start = file_page * 8, idx;
    char size_buf[32];
    draw_rect(0, 0, SCREEN_W - 1, SCREEN_H - 1, 0xFF071827);
    draw_top_bar("文件查询", 1);
    draw_sidebar(PAGE_FILES);
    draw_card(178, 78, 1006, 160, "当前目录");
    draw_text(198, 116, file_dir, 0xFFFFFFFF, 1);
    draw_button(file_up_btn, "返回上级", 0xFF2C4868);
    draw_card(178, 178, 1006, 566, "本地文件列表");
    draw_button(file_prev_btn, "< 上一页", 0xFF2C4868);
    draw_button(file_next_btn, "下一页 >", 0xFF2C4868);
    draw_text(204, 224, "名称", 0xFFFFFFFF, 2);
    draw_text(562, 224, "类型", 0xFFFFFFFF, 2);
    draw_text(690, 224, "大小", 0xFFFFFFFF, 2);
    draw_text(816, 224, "修改时间", 0xFFFFFFFF, 2);
    for (i = 0; i < 8; ++i) {
        idx = start + i;
        draw_rect_round(file_row_btns[i].x1, file_row_btns[i].y1, file_row_btns[i].x2, file_row_btns[i].y2, 8, idx == file_selected ? 0xFF11304C : 0xFF0E2234);
        if (idx < file_count) {
            draw_text(206, file_row_btns[i].y1 + 12, file_items[idx].name, 0xFFFFFFFF, 1);
            draw_text(564, file_row_btns[i].y1 + 12, file_items[idx].is_dir ? "目录" : "文件", 0xFFFFFFFF, 1);
            format_size(file_items[idx].size_bytes, size_buf, sizeof(size_buf));
            draw_text(692, file_row_btns[i].y1 + 12, file_items[idx].is_dir ? "--" : size_buf, 0xFFFFFFFF, 1);
            draw_text(816, file_row_btns[i].y1 + 12, file_items[idx].mtime, 0xFF9BB8D2, 1);
        }
    }
    draw_status_footer();
}
