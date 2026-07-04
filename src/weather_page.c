#include "ui.h"

void render_weather_page(void)
{
    draw_rect(0, 0, SCREEN_W - 1, SCREEN_H - 1, 0xFF071827);
    draw_top_bar("天气查询", 1);
    draw_sidebar(PAGE_WEATHER);
    draw_card(178, 78, 1006, 566, "通过 WSL 服务器查询");
    draw_button(weather_refresh_btn, weather_loading ? "刷新中..." : "刷新天气", weather_loading ? 0xFF7F5A18 : 0xFF1268DD);
    draw_card(208, 158, 574, 360, "武汉");
    draw_text(234, 210, weather_info_wuhan, 0xFFFFFFFF, 3);
    draw_text(234, 268, "数据来自远端 get_weather.py", 0xFF9AB8D5, 1);
    draw_card(610, 158, 976, 360, "北京");
    draw_text(636, 210, weather_info_beijing, 0xFFFFFFFF, 3);
    draw_text(636, 268, g_wsl_ip, 0xFF9AB8D5, 1);
    draw_card(208, 388, 976, 542, "状态");
    draw_text(236, 428, weather_loading ? "后台线程正在通过 ssh 查询 WSL，不阻塞触摸与渲染。" : weather_source, 0xFFFFFFFF, 1);
    draw_text(236, 468, "WSL 脚本路径: " REMOTE_GET_WEATHER, 0xFF9AB8D5, 1);
    draw_status_footer();
}
