from pathlib import Path
from datetime import datetime
p = Path('/home/cyj/workspace/ai_assistant/ui_client.c')
s = p.read_text(errors='ignore')
bak = p.with_name('ui_client.c.bak_before_ui_rebuild_' + datetime.now().strftime('%Y%m%d_%H%M%S'))
bak.write_text(s, errors='ignore')
print('backup:', bak)

if '#define PAGE_FILES' not in s:
    s = s.replace('#define PAGE_BMPVIEW   5', '#define PAGE_BMPVIEW   5\n#define PAGE_FILES     6')
if 'home_files_btn' not in s:
    s = s.replace('static rect_t home_sett_btn   = {510, 370, 910, 530};', 'static rect_t home_files_btn  = {180, 356, 612, 566};\nstatic rect_t home_sett_btn   = {628, 356, 1000, 566};')
s = s.replace('static rect_t voice_mic_btn   = {392, 340, 502, 420};\nstatic rect_t voice_send_btn  = {522, 340, 632, 420};', 'static rect_t voice_mic_btn   = {520, 336, 620, 436};\nstatic rect_t voice_send_btn  = {910, 476, 1000, 520};')

start = s.index('static void draw_bottom_status()')
end = s.index('static void handle_touch')
new = r'''
static void draw_card(int x1,int y1,int x2,int y2,const char* title){
    draw_rect_round(x1,y1,x2,y2,8,0xFF18314A);
    draw_rect_round(x1+1,y1+1,x2-1,y2-1,8,0xFF10263A);
    if(title) draw_text(x1+16,y1+14,title,0xFFFFFFFF,2);
}
static void draw_top_bar(const char* title,const char* icon,int back,int show_bt){
    draw_rect(0,0,SCREEN_W,62,0xFF041320); draw_rect(0,61,SCREEN_W,62,0xFF1E3A55);
    if(back) draw_text(18,16,"<",0xFFEAF2FF,3);
    draw_text(back?70:18,12,icon,0xFF2B8CFF,3); draw_text(back?116:62,8,title,0xFFFFFFFF,3);
    if(!back) draw_text(62,38,"RK1808 Dev Board",0xFFD8DEE9,1);
    draw_text(410,10,"CPU",0xFFFFFFFF,1); draw_text(410,28,"32%",0xFFFFFFFF,1); draw_rect_round(410,45,475,49,2,0xFF31465A); draw_rect_round(410,45,462,49,2,0xFF2B8CFF);
    draw_text(535,10,"RAM",0xFFFFFFFF,1); draw_text(535,28,"48%",0xFFFFFFFF,1); draw_rect_round(535,45,600,49,2,0xFF31465A); draw_rect_round(535,45,566,49,2,0xFF2B8CFF);
    draw_text(660,10,"Wi-Fi",0xFFFFFFFF,1); draw_text(660,31,"已连接",0xFFFFFFFF,1);
    if(show_bt){ draw_text(735,10,"蓝牙",0xFFFFFFFF,1); draw_text(735,31,"已连接",0xFFFFFFFF,1); }
    draw_text(790,18,"85%",0xFFFFFFFF,2); draw_text(905,8,"15:30:25",0xFFFFFFFF,2); draw_text(905,35,"2025-05-20",0xFFFFFFFF,1);
}
static void draw_sidebar(const char* active){
    draw_rect(0,62,160,SCREEN_H,0xFF06172A); draw_rect(159,62,160,SCREEN_H,0xFF1F4A78);
    const char* items[]={"首页","AI交互","天气查询","相册查询","文件查询","设置"};
    for(int i=0;i<6;i++){ int y=86+i*72; if(strcmp(active,items[i])==0) draw_rect_round(8,y,150,y+48,5,0xFF1268DD); draw_text(48,y+14,items[i],0xFFFFFFFF,2); }
}
static void draw_status_footer(){ draw_text(330,580,status_msg,0xFF9FB3C8,1); }

static void render_home_page(){
    draw_rect(0,0,SCREEN_W,SCREEN_H,0xFF071827); draw_top_bar("开发板主界面","[]",0,1); draw_sidebar("首页");
    draw_card(180,82,425,342,"1. AI交互");
    draw_rect_round(268,132,410,198,8,0xFF155BD3); draw_text(282,145,"我",0xFFFFFFFF,1); draw_text(282,168,"今天天气怎么样？",0xFFFFFFFF,1);
    draw_text(228,218,"AI助手",0xFFD8DEE9,1); draw_rect_round(224,238,370,295,8,0xFF26384A); draw_text(238,252,"深圳，28℃，多云，",0xFFFFFFFF,1); draw_text(238,274,"适合出行。",0xFFFFFFFF,1); draw_rect_round(278,304,338,364,30,0xFF1E73E8); draw_text(300,322,"●",0xFFFFFFFF,2);
    draw_card(442,82,702,342,"2. 天气查询"); draw_text(474,135,"深圳",0xFFFFFFFF,2); draw_text(475,175,"云",0xFFFFFFFF,4); draw_text(548,166,"28℃",0xFFFFFFFF,4); draw_text(552,218,"多云",0xFFFFFFFF,2); draw_text(474,252,"湿度：68%   |   风速：3级",0xFFD8DEE9,1); draw_text(474,300,"明天 27/32℃   后天 26/31℃",0xFFFFFFFF,1);
    draw_card(718,82,1000,342,"3. 相册查询"); for(int r=0;r<2;r++)for(int c=0;c<2;c++){int x=732+c*132,y=130+r*88; draw_rect_round(x,y,x+118,y+76,4,0xFF2D6E9F); draw_text(x+35,y+25,(r+c)%2?"照片":"风景",0xFFFFFFFF,1);} draw_text(820,320,"共 128 张照片",0xFFFFFFFF,1);
    draw_card(180,356,612,566,"4. 文件查询"); draw_text(204,405,"名称              类型       大小       修改时间",0xFFD8DEE9,1); draw_text(204,435,"[D] 文档          文件夹     —      2025-05-20",0xFFFFFFFF,1); draw_text(204,462,"[D] 图片          文件夹     —      2025-05-20",0xFFFFFFFF,1); draw_text(204,489,"main.c           C 文件     3.2KB",0xFFFFFFFF,1); draw_text(204,516,"config.json      JSON文件   1.1KB",0xFFFFFFFF,1);
    draw_card(628,356,1000,566,"5. 设置"); draw_text(652,404,"Wi-Fi        已连接  TP-Link_5G    >",0xFFFFFFFF,1); draw_text(652,440,"蓝牙         已连接               >",0xFFFFFFFF,1); draw_text(652,476,"亮度         ======== 70%",0xFFFFFFFF,1); draw_text(652,512,"音量         ====== 60%",0xFFFFFFFF,1); draw_status_footer();
}
static void render_voice_page(){
    draw_rect(0,0,SCREEN_W,SCREEN_H,0xFF071827); draw_top_bar("AI交互","AI",1,0); draw_sidebar("AI交互"); draw_card(168,74,1014,588,NULL);
    draw_text(530,104,"AI助手   今天 15:29",0xFFB9C7D6,1); draw_rect_round(775,135,958,222,8,0xFF155BD3); draw_text(792,154,"我",0xFFFFFFFF,1); draw_text(792,180,"今天天气怎么样？",0xFFFFFFFF,2); draw_text(910,205,"15:29",0xFFEAF2FF,1);
    draw_rect_round(226,228,470,312,8,0xFF26384A); draw_text(242,246,"AI助手",0xFFB9C7D6,1); draw_text(242,274,"深圳，28℃，多云，适合出行。",0xFFFFFFFF,2); draw_text(426,298,"15:29",0xFFEAF2FF,1);
    draw_rect_round(520,336,620,436,50,0xFF1E73E8); draw_text(555,366,"●",0xFFFFFFFF,3); draw_text(546,454,rec_state?"录音中":"点击说话",0xFFFFFFFF,1);
    draw_rect_round(184,476,890,520,8,0xFF14283C); draw_text(202,492,"输入文字或点击语音按钮说话...",0xFF8EA0B3,1); draw_rect_round(910,476,1000,520,8,0xFF1268DD); draw_text(942,492,"发送",0xFFFFFFFF,2);
    draw_rect_round(184,534,444,578,8,0xFF14283C); draw_text(280,550,"语音输入",0xFF2B8CFF,2); draw_rect_round(462,534,710,578,8,0xFF14283C); draw_text(560,550,"文本输入",0xFFFFFFFF,2); draw_rect_round(730,534,1000,578,8,0xFF14283C); draw_text(820,550,"清空对话",0xFFFFFFFF,2);
}
static void render_smart_page(){
    draw_rect(0,0,SCREEN_W,SCREEN_H,0xFF071827); draw_top_bar("天气查询","云",1,0); draw_card(25,78,998,124,NULL); draw_text(45,92,"深圳",0xFFFFFFFF,2); draw_text(930,92,"刷新",0xFFFFFFFF,1);
    draw_card(25,138,650,382,"当前天气"); draw_text(45,186,"15:30 更新",0xFFD8DEE9,1); draw_text(160,220,"云",0xFFFFFFFF,5); draw_text(298,182,"28℃",0xFFFFFFFF,5); draw_text(300,268,"多云",0xFFFFFFFF,3); draw_text(142,330,"湿度 68%",0xFFFFFFFF,1); draw_text(315,330,"东南风 3级",0xFFFFFFFF,2); draw_text(525,330,"优 28",0xFF55D66B,2);
    draw_card(662,138,998,562,"日出日落"); draw_text(705,270,"05:50 日出",0xFFFFFFFF,2); draw_text(860,270,"19:00 日落",0xFFFFFFFF,2); draw_text(682,350,"逐小时预报",0xFFFFFFFF,2); for(int i=0;i<6;i++){char b[32]; sprintf(b,"%02d:00",16+i); draw_text(680+i*52,400,b,0xFFD8DEE9,1); draw_text(680+i*52,430,"云",0xFFFFFFFF,1); sprintf(b,"%d℃",28-(i>1?i-1:0)); draw_text(680+i*52,470,b,0xFFFFFFFF,1);}
    draw_card(25,394,650,562,"4天预报"); draw_text(70,440,"明天 05/21   多云   27℃ / 32℃",0xFFFFFFFF,2); draw_text(70,485,"后天 05/22   小雨   26℃ / 31℃",0xFFFFFFFF,2); draw_text(70,530,"周五 05/23   多云   27℃ / 32℃",0xFFFFFFFF,2); draw_text(360,578,"数据来源：中国天气网    |    最后更新：2025-05-20 15:30",0xFF9FB3C8,1);
}
static void render_images_page(){
    draw_rect(0,0,SCREEN_W,SCREEN_H,0xFF071827); draw_top_bar("相册查询","图",1,0); draw_card(16,78,318,566,NULL); draw_rect_round(30,92,304,278,6,0xFF2D6E9F); draw_text(48,296,"IMG_20250520_142015.jpg",0xFFFFFFFF,1); draw_text(48,326,"2025-05-20 14:20:15",0xFFD8DEE9,1); draw_text(48,356,"4.2 MB  |  4032x3024",0xFFD8DEE9,1); draw_text(48,420,"位置                 深圳市 南山区",0xFFFFFFFF,1); draw_text(48,455,"设备                 Camera",0xFFFFFFFF,1); draw_text(48,490,"描述                 美丽的湖光山色",0xFFFFFFFF,1);
    draw_card(334,78,1008,566,NULL); draw_text(350,100,"全部    风景    城市    人物    收藏",0xFFFFFFFF,2); draw_text(884,100,"共 128 张照片",0xFFFFFFFF,1); for(int r=0;r<3;r++)for(int c=0;c<4;c++){int x=346+c*166,y=148+r*134; draw_rect_round(x,y,x+154,y+112,5,0xFF2D6E9F); draw_text(x+48,y+45,(r+c)%3==0?"风景":((r+c)%3==1?"城市":"自然"),0xFFFFFFFF,1);} draw_status_footer();
}
static void render_bmpview_page(){ render_images_page(); }
static void render_files_page(){
    draw_rect(0,0,SCREEN_W,SCREEN_H,0xFF071827); draw_top_bar("文件查询","<",1,0); draw_card(15,84,1010,190,NULL); draw_text(32,110,"存储设备   >   home   >   rk1808   >   workspace   >   project",0xFFFFFFFF,2); draw_rect_round(765,104,995,142,6,0xFF14283C); draw_text(802,116,"搜索文件或文件夹",0xFFB9C7D6,1); draw_rect_round(30,146,150,182,5,0xFF1268DD); draw_text(50,156,"+ 新建文件夹",0xFFFFFFFF,1); draw_text(185,156,"复制        删除        重命名",0xFFFFFFFF,1);
    draw_card(15,204,1010,575,NULL); draw_text(95,232,"名称",0xFFFFFFFF,2); draw_text(450,232,"类型",0xFFFFFFFF,2); draw_text(610,232,"大小",0xFFFFFFFF,2); draw_text(800,232,"修改时间",0xFFFFFFFF,2);
    const char* rows[][4]={{"[D] 文档","文件夹","—","2025-05-20 14:20"},{"[D] 图片","文件夹","—","2025-05-20 14:18"},{"main.c","C 文件","3.2 KB","2025-05-20 13:55"},{"config.json","JSON 文件","1.1 KB","2025-05-20 13:50"},{"readme.md","MD 文件","2.0 KB","2025-05-20 13:45"},{"数据记录.xlsx","XLSX 文件","12.6 KB","2025-05-20 13:40"}};
    for(int i=0;i<6;i++){int y=280+i*47; draw_text(95,y,rows[i][0],0xFFFFFFFF,2); draw_text(450,y,rows[i][1],0xFFFFFFFF,1); draw_text(610,y,rows[i][2],0xFFFFFFFF,1); draw_text(800,y,rows[i][3],0xFFFFFFFF,1);} draw_text(35,552,"共 6 项",0xFFFFFFFF,1); draw_text(760,552,"第 1 页，共 1 页",0xFFFFFFFF,1);
}
static void render_settings_page(){
    draw_rect(0,0,SCREEN_W,SCREEN_H,0xFF071827); draw_top_bar("设置","*",0,1); draw_rect(0,62,205,SCREEN_H,0xFF06172A); const char* it[]={"网络","蓝牙","显示","声音","存储","系统信息"}; for(int i=0;i<6;i++){int y=86+i*72; if(i==0) draw_rect_round(15,y,188,y+48,5,0xFF1268DD); draw_text(70,y+14,it[i],0xFFFFFFFF,2);}
    draw_card(222,74,1005,588,"网络"); draw_rect_round(244,120,985,225,8,0xFF14283C); draw_text(265,145,"Wi-Fi",0xFFFFFFFF,2); draw_text(770,145,"已连接  TP-Link_5G   >",0xFFFFFFFF,1); draw_text(265,184,"IP 地址",0xFFD8DEE9,1); draw_text(815,184,"192.168.1.100",0xFFFFFFFF,1); draw_text(265,208,"MAC 地址",0xFFD8DEE9,1); draw_text(790,208,"3C:5A:B4:18:08:08",0xFFFFFFFF,1);
    draw_rect_round(244,236,985,294,8,0xFF14283C); draw_text(265,252,"蓝牙",0xFFFFFFFF,2); draw_text(760,252,"已连接",0xFFFFFFFF,1); draw_text(265,278,"蓝牙已开启，可被发现为 RK1808",0xFFD8DEE9,1);
    draw_rect_round(244,306,985,364,8,0xFF14283C); draw_text(265,322,"显示  亮度",0xFFFFFFFF,2); draw_text(650,328,"========== 70%  >",0xFFFFFFFF,1);
    draw_rect_round(244,376,985,434,8,0xFF14283C); draw_text(265,392,"声音  音量",0xFFFFFFFF,2); draw_text(650,398,"======== 60%  >",0xFFFFFFFF,1);
    draw_rect_round(244,446,985,504,8,0xFF14283C); draw_text(265,462,"存储  内部存储",0xFFFFFFFF,2); draw_text(650,468,"已使用 12.8 GB / 32.0 GB  >",0xFFFFFFFF,1);
    draw_rect_round(244,516,985,576,8,0xFF14283C); draw_text(265,532,"系统信息",0xFFFFFFFF,2); draw_text(800,532,"RK1808 Dev Board",0xFFFFFFFF,1); draw_text(800,558,"Linux 5.10.110",0xFFFFFFFF,1);
}
static void render_page(){
    switch(cur_page){
        case PAGE_HOME: render_home_page(); break;
        case PAGE_VOICE: render_voice_page(); break;
        case PAGE_SMART: render_smart_page(); break;
        case PAGE_IMAGES: render_images_page(); break;
        case PAGE_SETTINGS: render_settings_page(); break;
        case PAGE_BMPVIEW: render_bmpview_page(); break;
        case PAGE_FILES: render_files_page(); break;
        default: render_home_page(); break;
    }
}

'''
s = s[:start] + new + s[end:]
old = '''        if (pt_in_rect(px, py, home_images_btn)) { cur_page = PAGE_IMAGES; scan_bmp_dir(); }
        if (pt_in_rect(px, py, home_sett_btn)) cur_page = PAGE_SETTINGS;'''
new_touch = '''        if (pt_in_rect(px, py, home_images_btn)) { cur_page = PAGE_IMAGES; scan_bmp_dir(); }
        if (pt_in_rect(px, py, home_files_btn)) cur_page = PAGE_FILES;
        if (pt_in_rect(px, py, home_sett_btn)) cur_page = PAGE_SETTINGS;'''
if old in s:
    s = s.replace(old, new_touch)
else:
    print('warning: home touch pattern not found')
p.write_text(s, errors='ignore')
print('ui_client.c updated')
