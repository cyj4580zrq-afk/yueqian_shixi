#include "ui.h"

void render_voice_page(void)
{
    int i, start, y = 118;
    char line[280];

    draw_rect(0, 0, SCREEN_W - 1, SCREEN_H - 1, 0xFF071827);
    draw_top_bar("AI交互", 1);
    draw_sidebar(PAGE_VOICE);
    draw_card(178, 78, 1006, 566, "语音识别 + AI + TTS");
    draw_text(198, 112, "识别文字", 0xFF9BB8D2, 1);
    draw_rect_round(198, 136, 626, 188, 10, 0xFF102537);
    draw_text(212, 152, recognized_text[0] ? recognized_text : "等待录音并识别...", 0xFFFFFFFF, 1);
    draw_text(658, 112, "AI 回复", 0xFF9BB8D2, 1);
    draw_rect_round(658, 136, 986, 188, 10, 0xFF102537);
    draw_text(672, 152, ai_reply_text[0] ? ai_reply_text : "等待 WSL 返回文本与语音...", 0xFFFFFFFF, 1);
    draw_text(198, 214, "对话记录", 0xFFFFFFFF, 2);
    draw_rect_round(198, 246, 986, 444, 10, 0xFF0F2234);
    start = chat_lines > 7 ? chat_lines - 7 : 0;
    for (i = start; i < chat_lines && y < 430; ++i) {
        snprintf(line, sizeof(line), "%s", chat_text[i]);
        draw_text(214, y, line, 0xFFFFFFFF, 1);
        y += 40;
    }
    draw_button(voice_mic_btn, rec_state ? "录音中..." : "开始录音", rec_state ? 0xFFB34D36 : 0xFF1268DD);
    draw_button(voice_send_btn, send_state ? "发送中..." : "发送到 WSL", send_state ? 0xFF7F5A18 : 0xFF1E7B58);
    draw_button(voice_clear_btn, "清空对话", 0xFF364A5E);
    draw_text(232, 474, "录音结束后点击发送，识别文字经 8888 端口回传，TTS 音频经 8889 端口回传。", 0xFF9BB8D2, 1);
    draw_status_footer();
}
