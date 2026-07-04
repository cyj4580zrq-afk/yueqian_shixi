#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <linux/input.h>
#include <time.h>
#include "./DRMwrap.h"

#define TOUCH_DEVICE       "/dev/input/event2"
#define DRM_DEVICE         "/dev/dri/card0"
#define GRID_SIZE          32
#define COLS               32   // 1024 / 32
#define ROWS               18   // 600 / 32 = 18 (576像素高，底部留白24)

typedef struct {
    int x;
    int y;
} Point;

typedef enum {
    UP,
    DOWN,
    LEFT,
    RIGHT
} Direction;

// 全局硬件及游戏状态
struct drmHandle DRM;
int lcd_fb = -1;
int touch_fd = -1;

Point snake[COLS * ROWS];
int snake_len = 3;
Direction dir = RIGHT;
Direction next_dir = RIGHT;
Point food;
int game_over = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// 画一个格子 (col, row) 填充对应 RGB 颜色
void draw_block(int col, int row, int color) {
    char *share = DRM.vaddr;
    if (!share) return;
    int R = (color >> 16) & 0xFF;
    int G = (color >> 8) & 0xFF;
    int B = color & 0xFF;
    
    for (int y = row * GRID_SIZE; y < (row + 1) * GRID_SIZE && y < 600; y++) {
        for (int x = col * GRID_SIZE; x < (col + 1) * GRID_SIZE && x < 1024; x++) {
            int pos = 4 * (1024 * y + x);
            share[pos+0] = B;
            share[pos+1] = G;
            share[pos+2] = R;
            share[pos+3] = 0;
        }
    }
}

// 刷背景色
void clear_screen(int color) {
    char *share = DRM.vaddr;
    if (!share) return;
    int R = (color >> 16) & 0xFF;
    int G = (color >> 8) & 0xFF;
    int B = color & 0xFF;
    for (int i = 0; i < 1024 * 600; i++) {
        share[4*i+0] = B;
        share[4*i+1] = G;
        share[4*i+2] = R;
        share[4*i+3] = 0;
    }
    DRMshowUp(lcd_fb, &DRM);
}

// 在空白网格随机生成食物
void spawn_food() {
    while (1) {
        food.x = rand() % COLS;
        food.y = rand() % ROWS;
        
        int on_snake = 0;
        for (int i = 0; i < snake_len; i++) {
            if (snake[i].x == food.x && snake[i].y == food.y) {
                on_snake = 1;
                break;
            }
        }
        if (!on_snake) break;
    }
    // 绘制红色食物
    draw_block(food.x, food.y, 0xFF0000);
}

// 初始化游戏属性
void init_game() {
    pthread_mutex_lock(&lock);
    snake_len = 3;
    snake[0].x = 10; snake[0].y = 9;  // 蛇头
    snake[1].x = 9;  snake[1].y = 9;
    snake[2].x = 8;  snake[2].y = 9;  // 蛇尾
    dir = RIGHT;
    next_dir = RIGHT;
    game_over = 0;
    
    // 清背景为全黑
    clear_screen(0x000000);
    
    // 渲染蛇身 (绿) 和蛇头 (蓝)
    for (int i = 1; i < snake_len; i++) {
        draw_block(snake[i].x, snake[i].y, 0x00FF00);
    }
    draw_block(snake[0].x, snake[0].y, 0x0000FF);
    
    // 生成第一个食物
    spawn_food();
    
    DRMshowUp(lcd_fb, &DRM);
    pthread_mutex_unlock(&lock);
}

// 触摸屏方向监听线程（去除按键限制，只要坐标发生变化就实时触发，实现无感延迟）
void *touch_thread(void *arg) {
    struct input_event ts;
    int last_x = -1, last_y = -1;
    
    while (1) {
        if (read(touch_fd, &ts, sizeof(struct input_event)) != sizeof(struct input_event)) {
            continue;
        }
        if (ts.type == EV_ABS && ts.code == ABS_X) {
            last_x = ts.value;
        }
        if (ts.type == EV_ABS && ts.code == ABS_Y) {
            last_y = ts.value;
        }
        
        // 只要接收到触摸屏的有效坐标，就立刻实时调整方向，无需等待手指按下/抬起事件
        if (last_x >= 0 && last_y >= 0) {
            int dx = last_x - 512;
            int dy = last_y - 300;
            
            pthread_mutex_lock(&lock);
            // 采用十字绝对位移算法进行朝向过滤
            if (abs(dx) > abs(dy)) {
                if (dx > 0 && dir != LEFT) {
                    next_dir = RIGHT;
                } else if (dx < 0 && dir != RIGHT) {
                    next_dir = LEFT;
                }
            } else {
                if (dy > 0 && dir != UP) {
                    next_dir = DOWN;
                } else if (dy < 0 && dir != DOWN) {
                    next_dir = UP;
                }
            }
            pthread_mutex_unlock(&lock);
        }
    }
    return NULL;
}

// 更新蛇的位置和得分情况
void update_game() {
    pthread_mutex_lock(&lock);
    dir = next_dir;
    
    Point head = snake[0];
    switch (dir) {
        case UP:    head.y--; break;
        case DOWN:  head.y++; break;
        case LEFT:  head.x--; break;
        case RIGHT: head.x++; break;
    }
    
    // 撞墙或撞到自己
    if (head.x < 0 || head.x >= COLS || head.y < 0 || head.y >= ROWS) {
        game_over = 1;
        pthread_mutex_unlock(&lock);
        return;
    }
    for (int i = 0; i < snake_len; i++) {
        if (snake[i].x == head.x && snake[i].y == head.y) {
            game_over = 1;
            pthread_mutex_unlock(&lock);
            return;
        }
    }
    
    // 吃食物判断
    int eat = 0;
    if (head.x == food.x && head.y == food.y) {
        eat = 1;
    }
    
    // 抹除原蛇尾颜色
    Point old_tail = snake[snake_len - 1];
    if (!eat) {
        draw_block(old_tail.x, old_tail.y, 0x000000);
    }
    
    // 移动蛇身数据结构
    int loop_len = eat ? snake_len : snake_len - 1;
    for (int i = loop_len; i > 0; i--) {
        snake[i] = snake[i - 1];
    }
    
    snake[0] = head;
    if (eat) {
        snake_len++;
    }
    
    // 刷新显示：原来的头画为绿色，新头画为蓝色
    if (snake_len > 1) {
        draw_block(snake[1].x, snake[1].y, 0x00FF00);
    }
    draw_block(snake[0].x, snake[0].y, 0x0000FF);
    
    if (eat) {
        spawn_food();
    }
    
    DRMshowUp(lcd_fb, &DRM);
    pthread_mutex_unlock(&lock);
}

// 死亡闪红效果
void flash_red() {
    for (int k = 0; k < 3; k++) {
        clear_screen(0xFF0000);
        usleep(150000);
        clear_screen(0x000000);
        usleep(150000);
    }
}

int main()
{
    srand(time(NULL));
    
    // 1. 初始化 DRM LCD
    lcd_fb = open(DRM_DEVICE, O_RDWR);
    if (lcd_fb < 0) {
        perror("Open DRM device failed");
        return -1;
    }
    DRMinit(lcd_fb);
    DRMcreateFB(lcd_fb, &DRM);
    
    // 2. 初始化触摸屏
    touch_fd = open(TOUCH_DEVICE, O_RDONLY);
    if (touch_fd < 0) {
        perror("Open touch device failed");
        DRMfreeResources(lcd_fb, &DRM);
        close(lcd_fb);
        return -1;
    }
    
    // 3. 开启触摸侦听线程
    pthread_t tid;
    if (pthread_create(&tid, NULL, touch_thread, NULL) != 0) {
        perror("Create touch thread failed");
        close(touch_fd);
        DRMfreeResources(lcd_fb, &DRM);
        close(lcd_fb);
        return -1;
    }
    pthread_detach(tid);
    
    // 4. 游戏主循环
    while (1) {
        printf("--- 游戏开始 / 重置 ---\n");
        init_game();
        
        while (!game_over) {
            update_game();
            usleep(150000); // 将前进周期从 250ms 缩短到 150ms，大幅提升反应速度与操作手感
        }
        
        printf("--- 游戏结束！ ---\n");
        flash_red();
    }
    
    // 清理
    close(touch_fd);
    DRMfreeResources(lcd_fb, &DRM);
    close(lcd_fb);
    return 0;
}
