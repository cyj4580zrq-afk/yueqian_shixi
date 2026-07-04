from pathlib import Path
p = Path('/home/cyj/workspace/ai_assistant/ui_client.c')
s = p.read_text(errors='ignore')
old = '''            } else if (pt_in_rect(px, py, home_sett_btn)) {
                cur_page = PAGE_SETTINGS;
                snprintf(status_msg, sizeof(status_msg), "Settings");
                ui_needs_redraw = 1;
            }
            break;
        case PAGE_VOICE:'''
new = '''            } else if (pt_in_rect(px, py, home_files_btn)) {
                cur_page = PAGE_FILES;
                snprintf(status_msg, sizeof(status_msg), "File Browser");
                ui_needs_redraw = 1;
            } else if (pt_in_rect(px, py, home_sett_btn)) {
                cur_page = PAGE_SETTINGS;
                snprintf(status_msg, sizeof(status_msg), "Settings");
                ui_needs_redraw = 1;
            }
            break;
        case PAGE_VOICE:'''
if old in s:
    s = s.replace(old, new)
    p.write_text(s, errors='ignore')
    print('touch patched ok')
else:
    print('pattern not found')
