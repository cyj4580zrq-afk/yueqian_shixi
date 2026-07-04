#include "ui.h"

void render_settings_page(void)
{
    int i;
    char line[160];

    draw_rect(0, 0, SCREEN_W - 1, SCREEN_H - 1, 0xFF071827);
    draw_top_bar("Settings", 1);
    draw_sidebar(PAGE_SETTINGS);

    draw_card(178, 78, 560, 566, "Network");
    draw_text(204, 118, connected_ssid[0] ? connected_ssid : "WiFi disconnected", 0xFFFFFFFF, 2);
    snprintf(line, sizeof(line), "IP: %s", local_ip[0] ? local_ip : "pending...");
    draw_text(204, 156, line, 0xFFFFFFFF, 1);
    draw_text(204, 188, "IP updates are refreshed in background", 0xFF9BB8D2, 1);
    draw_text(204, 226, "Tap a network then press connect", 0xFFFFFFFF, 1);
    draw_text(204, 258, "Selected:", 0xFF9BB8D2, 1);
    draw_text(292, 258, selected_ssid[0] ? selected_ssid : "none", 0xFFFFFFFF, 1);
    draw_text(204, 292, "Saved/open networks can connect directly", 0xFF9BB8D2, 1);
    draw_text(204, 320, "For password WiFi, use " WIFI_PASSWORD_FILE, 0xFF9BB8D2, 1);
    draw_text(204, 348, "Format: SSID=password", 0xFF9BB8D2, 1);
    draw_button(sett_scan_btn, wifi_scanning ? "Scanning..." : "Scan WiFi", wifi_scanning ? 0xFF7F5A18 : 0xFF1268DD);
    draw_button(sett_connect_btn, "Connect", 0xFF1E7B58);

    draw_card(590, 78, 1006, 566, "Visible WiFi");
    for (i = 0; i < 6; ++i) {
        draw_rect_round(sett_wifi_btns[i].x1, sett_wifi_btns[i].y1, sett_wifi_btns[i].x2, sett_wifi_btns[i].y2, 8,
            (i < wifi_count && strcmp(selected_ssid, wifi_ssid[i]) == 0) ? 0xFF1268DD : 0xFF0E2234);
        if (i < wifi_count) {
            snprintf(line, sizeof(line), "%s  (%d dBm)", wifi_ssid[i], wifi_signal[i]);
            draw_text(sett_wifi_btns[i].x1 + 12, sett_wifi_btns[i].y1 + 16, line, 0xFFFFFFFF, 1);
        } else {
            draw_text(sett_wifi_btns[i].x1 + 12, sett_wifi_btns[i].y1 + 16, wifi_scanning ? "Scanning..." : "empty", 0xFF9BB8D2, 1);
        }
    }
    draw_status_footer();
}
