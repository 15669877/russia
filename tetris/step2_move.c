/* ============================================================
 * step2_move.c —— 第 2 步：让方块动起来，而且不留残影
 *
 * 【这一步只解决一件事】
 *   方块能按预期方向移动，且屏幕上不会留下拖影、画面不会闪。
 *
 * 【要理解的知识点】
 *   1) 动画的本质 = 一帧一帧重复画。
 *      但「每帧清屏 + 重画整屏」会让画面抖动（终端要整屏擦除再重绘，
 *      眼睛能看出闪烁）。render.h 里的做法是：
 *          把每行内容缓存下来，每帧只输出「和上一帧不同」的行，
 *          用 \033[row;1H 直接跳到那一行重写。
 *      没变化的行一个字符都不输出 → 屏幕上什么都不发生 → 完全不闪。
 *      （残留？用 \033[K 清掉行尾多余内容即可。）
 *
 *   2) 坐标系统：x 向右增大，y 向下增大。棋盘是 H 行 W 列。
 *      一个形状用 (x, y) 表示它「包围盒左上角」在棋盘上的位置。
 *
 *   3) 移动 = 修改 x / y，然后重画。
 *
 *   4) 一个格子占两个字符（"[]"），这样才不会被终端的瘦高字形拉长。
 *
 * 【怎么跑】
 *   Visual Studio：右键 step2_move → 设为启动项目 → Ctrl+F5
 *   方块会自动左右来回移动。按 q 退出。
 * ============================================================ */
#include <stdio.h>
#include "key.h"
#include "render.h"
#include "shapes.h"

#define W 20        /* 场地宽（格） */
#define H 10        /* 场地高（格） */

static int type = 0;   /* 0 = O 方块 */
static int rot  = 0;   /* 旋转态 */
static int x = 0, y = 5;   /* 包围盒左上角坐标 */
static int dir = 1;     /* 移动方向：1 右，-1 左 */

/* 画一个格子：形状的包围盒左上角在 (x,y) */
static int cell(int cx, int cy) {
    int w = SHAPE_SIZE[type][rot][0];
    if (cy >= SHAPE_SIZE[type][rot][1] || cx >= w) return 0;
    return (SHAPE[type][rot][cy] >> (w - 1 - cx)) & 1;
}

/* 每帧：只重画变化的行 */
static void show(void) {
    char line[R_MAXCOLS];
    char border[R_MAXCOLS];
    int n;

    r_frame_start();

    /* 上下边框 */
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
            /* 这个格子是不是方块的一部分？ */
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
}

int main(void) {
#ifndef _WIN32
    key_init();
#endif
    r_enter();
    while (1) {
        show();
        if (key_hit()) {
            int k = key_get();
            if (k == 'Q' || k == 'q') break;
        }
        key_delay(80);            /* 每帧停一下，肉眼才看得清移动 */

        x += dir;                 /* 移动一格 */
        /* 撞到左右边界就掉头（本步只是演示，还没有真正的碰撞检测） */
        if (x + SHAPE_SIZE[type][rot][0] >= W || x < 0) {
            x += dir;             /* 退回来 */
            dir = -dir;
        }
    }
    r_leave();
    return 0;
}
