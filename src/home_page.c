#include "ui.h"

void render_home_page(void)
{
    char info[64];

    draw_rect(0, 0, SCREEN_W - 1, SCREEN_H - 1, 0xFF071827);
    draw_top_bar("AI Assistant", 0);
    draw_sidebar(PAGE_HOME);

    draw_card(home_cards[0].x1, home_cards[0].y1, home_cards[0].x2, home_cards[0].y2, "Hero Image");
    draw_text(206, 108, "Original splash asset retained", 0xFFB3CAE2, 1);
    draw_rect_round(204, 138, 504, 334, 10, 0xFF102437);
    if (access(HOME_HERO_BMP, R_OK) == 0) {
        draw_bmp(214, 146, 280, 176, HOME_HERO_BMP);
        draw_text(214, 328, "GGBond.bmp", 0xFFFFFFFF, 1);
    } else {
        draw_text(246, 214, "GGBond.bmp not found", 0xFFFFFFFF, 2);
    }

    draw_card(home_cards[1].x1, home_cards[1].y1, home_cards[1].x2, home_cards[1].y2, "AI Voice");
    draw_button(home_entry_btns[0], "Open AI", 0xFF1268DD);
    draw_text(554, 114, recognized_text[0] ? recognized_text : "ASR text will appear here", 0xFFFFFFFF, 1);
    draw_text(554, 154, ai_reply_text[0] ? ai_reply_text : "AI reply and TTS status appear here", 0xFF8EB2D3, 1);

    draw_card(home_cards[2].x1, home_cards[2].y1, home_cards[2].x2, home_cards[2].y2, "Weather");
    draw_button(home_entry_btns[1], "Open Weather", 0xFF1268DD);
    draw_text(554, 316, weather_info_wuhan, 0xFFFFFFFF, 2);
    draw_text(554, 352, weather_info_beijing, 0xFFB7D0E7, 2);
    draw_text(554, 394, weather_loading ? "Background fetch running..." : weather_source, 0xFF8EB2D3, 1);

    draw_card(home_cards[3].x1, home_cards[3].y1, home_cards[3].x2, home_cards[3].y2, "Images");
    draw_button(home_entry_btns[2], "Open Images", 0xFF1268DD);
    draw_text(206, 404, image_root, 0xFF8EB2D3, 1);
    snprintf(info, sizeof(info), "%d local images", image_count);
    draw_text(206, 434, info, 0xFFFFFFFF, 2);
    if (image_count > 0) draw_text(206, 470, image_items[image_selected >= 0 ? image_selected : 0].name, 0xFFFFFFFF, 1);

    draw_card(home_cards[4].x1, home_cards[4].y1, home_cards[4].x2, home_cards[4].y2, "Files");
    draw_button(home_entry_btns[3], "Open Files", 0xFF1268DD);
    draw_text(632, 404, file_dir, 0xFF8EB2D3, 1);
    snprintf(info, sizeof(info), "%d local entries", file_count);
    draw_text(632, 438, info, 0xFFFFFFFF, 2);
    if (file_count > 0) draw_text(632, 474, file_items[file_selected >= 0 ? file_selected : 0].name, 0xFFFFFFFF, 1);

    draw_card(home_cards[5].x1, home_cards[5].y1, home_cards[5].x2, home_cards[5].y2, "Settings");
    draw_button(home_entry_btns[4], "Open Settings", 0xFF1268DD);
    draw_text(632, 506, connected_ssid[0] ? connected_ssid : "WiFi disconnected", 0xFFFFFFFF, 1);
    draw_text(632, 532, local_ip[0] ? local_ip : "IP pending...", 0xFFFFFFFF, 1);
    draw_status_footer();
}
