#include "ui.h"

static void draw_image_metadata(const image_item_t *item)
{
    char size_buf[32];
    draw_text(204, 118, "本地图片目录", 0xFF9BB8D2, 1);
    draw_text(204, 144, image_root, 0xFFFFFFFF, 1);
    if (!item) {
        draw_text(204, 220, "未发现图片文件", 0xFFFFFFFF, 2);
        return;
    }
    draw_text(204, 182, item->name, 0xFFFFFFFF, 2);
    format_size(item->size_bytes, size_buf, sizeof(size_buf));
    draw_text(204, 214, size_buf, 0xFF9BB8D2, 1);
    draw_text(204, 240, item->mtime, 0xFF9BB8D2, 1);
    draw_rect_round(204, 272, 470, 520, 10, 0xFF0F2234);
    if (item->is_bmp && access(item->path, R_OK) == 0) {
        draw_bmp(214, 282, 246, 228, item->path);
    } else {
        draw_text(228, 384, "仅 BMP 支持板端预览", 0xFFFFFFFF, 2);
    }
}

void render_images_page(void)
{
    int i, start = image_page * 6, idx;
    const image_item_t *selected = (image_selected >= 0 && image_selected < image_count) ? &image_items[image_selected] : NULL;
    draw_rect(0, 0, SCREEN_W - 1, SCREEN_H - 1, 0xFF071827);
    draw_top_bar("图片查询", 1);
    draw_sidebar(PAGE_IMAGES);
    draw_card(178, 78, 486, 566, "图片详情");
    draw_image_metadata(selected);
    draw_card(510, 78, 1006, 566, "本地图片列表");
    draw_button(img_prev_btn, "< 上一页", 0xFF2C4868);
    draw_button(img_next_btn, "下一页 >", 0xFF2C4868);
    for (i = 0; i < 6; ++i) {
        idx = start + i;
        draw_rect_round(img_thumb_btns[i].x1, img_thumb_btns[i].y1, img_thumb_btns[i].x2, img_thumb_btns[i].y2, 8, idx == image_selected ? 0xFF1268DD : 0xFF12283A);
        if (idx < image_count) {
            draw_text(img_thumb_btns[i].x1 + 14, img_thumb_btns[i].y1 + 18, image_items[idx].name, 0xFFFFFFFF, 1);
            draw_text(img_thumb_btns[i].x1 + 14, img_thumb_btns[i].y1 + 50, image_items[idx].mtime, 0xFF9BB8D2, 1);
            draw_text(img_thumb_btns[i].x1 + 14, img_thumb_btns[i].y1 + 82, image_items[idx].is_bmp ? "BMP 可预览" : "非 BMP 仅查看信息", 0xFF9BB8D2, 1);
        } else draw_text(img_thumb_btns[i].x1 + 30, img_thumb_btns[i].y1 + 42, "空", 0xFF9BB8D2, 2);
    }
    draw_status_footer();
}

void render_bmpview_page(void)
{
    const image_item_t *item = (image_selected >= 0 && image_selected < image_count) ? &image_items[image_selected] : NULL;
    draw_rect(0, 0, SCREEN_W - 1, SCREEN_H - 1, 0xFF071827);
    draw_top_bar("原图查看", 1);
    draw_sidebar(PAGE_IMAGES);
    draw_card(178, 78, 1006, 566, item ? item->name : "未选中图片");
    draw_button(bmp_prev_btn, "< 上一张", 0xFF2C4868);
    draw_button(bmp_next_btn, "下一张 >", 0xFF2C4868);
    if (!item) draw_text(430, 280, "没有可显示的图片", 0xFFFFFFFF, 2);
    else if (item->is_bmp && access(item->path, R_OK) == 0) {
        draw_rect_round(220, 136, 964, 500, 12, 0xFF0F2234);
        draw_bmp(234, 148, 710, 332, item->path);
        draw_text(236, 516, item->mtime, 0xFF9BB8D2, 1);
    } else {
        draw_text(390, 270, "当前图片不是 BMP，无法板端直接渲染", 0xFFFFFFFF, 2);
        draw_text(390, 316, item->path, 0xFF9BB8D2, 1);
    }
    draw_status_footer();
}
