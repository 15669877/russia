/* ============================================================
 * step3_keys.c —— 第 3 步：监听键盘上下左右，让方块按预期方向移动
 *
 * 【这一步只解决一件事】
 *   方块的移动由键盘驱动，而不是自动。
 *
 * 【要理解的知识点】
 *   1) 非阻塞读键：
 *        key_hit()  —— 有没有键按下？（没有就返回 0，程序继续跑）
 *        key_get()  —— 取一个键的字符
 *      一定要先判断 key_hit() 再取键，否则程序会卡在等待输入上。
 *
 *   2) 方向键在不同系统上编码不同，key.h 已经统一翻译成：
 *        'L' 左   'R' 右   'U' 上   'D' 下
 *      同时也支持 wasd：a/d 左右、w 上、s 下。
 *      （Windows 的 _getch() 对方向键返回 0 + 'H'/'P'/'M'/'K'，
 *        POSIX 是 ESC [ A/B/C/D —— 两种都在 key.h 里处理好了。）
 *
 *   3) switch 语句把按键映射成动作。
 *
 * 【注意】
 *   这一版还没有边界检测，按到边缘方块会跑出屏幕 —— 下一步才修。
 *
 * 【怎么跑】
 *   Visual Studio：右键 step3_keys → 设为启动项目 → Ctrl+F5
 *   用方向键或 wasd 移动方块，按 q 退出。
 * ============================================================ */
#include <stdio.h>
#include "key.h"
#include "render.h"
#include "shapes.h"

#define W 20
#define H 10

static int type = 0, rot = 0;
static int x = 10, y = 5;

static int cell(int cx, int cy) {
    int w = SHAPE_SIZE[type][rot][0];
    if (cy >= SHAPE_SIZE[type][rot][1] || cx >= w) return 0;
    return (SHAPE[type][rot][cy] >> (w - 1 - cx)) & 1;
}

static void show(void) {
    char line[R_MAXCOLS];
    char border[R_MAXCOLS];
    int n;

    r_frame_start();

    n = 0; border[n] = '\0';
    border[n++] = '+';
    for (int c = 0; c < W * 2; c++) border[n++] = '-';
    border[n++] = '+';
    border[n] = '\0';
    r_line(0, border);

    for (int r = 0; r < H; r++) {
        n = 0; line[n] = '\0';
        line[n++] = '|';
        for (int c = 0; c < W; c++) {
            int on = (c >= x && c < x + SHAPE_SIZE[type][rot][0] &&
                      r >= y && r < y + SHAPE_SIZE[type][rot][1] &&
                      cell(c - x, r - y));
            r_put(line, &n, on);
        }
        line[n++] = '|';
        line[n] = '\0';
        r_line(r + 1, line);
    }

    r_line(H + 1, border);
    r_line(H + 3, "Arrow keys / wasd to move, q to quit");
}

int main(void) {
#ifndef _WIN32
    key_init();
#endif
    r_enter();
    while (1) {
        show();

        if (!key_hit()) {                 /* 没按键就重画一帧 */
            key_delay(16);
            continue;
        }

        switch (key_get()) {
            case 'L': case 'a': x--; break;   /* 左 */
            case 'R': case 'd': x++; break;   /* 右 */
            case 'U': case 'w': y--; break;   /* 上 */
            case 'D': case 's': y++; break;   /* 下 */
            case 'Q': case 'q': goto done;
            default: break;
        }
    }
done:
    r_leave();
    return 0;
}
