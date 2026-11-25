/* ============================================================
 * step4_wall.c —— 第 4 步：加墙 / 加方框，检测「动不下去就停」
 *
 * 【这一步只解决一件事】
 *   方块碰到边界（左右墙、地板）就停住，而不是越界。
 *
 * 【要理解的知识点】
 *   1) 关键设计：把「能不能放这儿」收进一个函数 can_place()。
 *      游戏里所有动作（移动、旋转、下坠、生成新块）都先问它，
 *      这样判定逻辑只有一份，不会到处重复，也不会漏。
 *
 *   2) 判定要检查三件事：
 *        - 左右不能出界（x < 0 或 x + 宽 > W）
 *        - 下面不能出界（y + 高 > H）
 *        - 不能跟已有方块重叠
 *
 *   3) 形状不是一个格子，而是一组格子。
 *      所以要「逐格检查」：只要有一格不合法，整个放置就不合法。
 *
 *   4) 这一版方块会自动往下掉，掉到底就停住并锁定，
 *      然后在顶部生成一个新方块继续掉 —— 这就是「落地」的前奏。
 *
 * 【怎么跑】
 *   Visual Studio：右键 step4_wall → 设为启动项目 → Ctrl+F5
 *   看方块一个个落地堆起来，按 q 退出。
 * ============================================================ */
#include <stdio.h>
#include "key.h"
#include "render.h"
#include "shapes.h"

#define W 10        /* 场地宽（格） */
#define H 12        /* 场地高（格） */

static int board[H][W];   /* 0 空，1 已锁定 */
static int type, rot, x, y;
static int stopped;        /* 1 = 已落地停住 */

static int cell(int cx, int cy) {
    int w = SHAPE_SIZE[type][rot][0];
    if (cy >= SHAPE_SIZE[type][rot][1] || cx >= w) return 0;
    return (SHAPE[type][rot][cy] >> (w - 1 - cx)) & 1;
}

/* 唯一的合法判定：想放就问这里。
 * 逐格检查：只要有一格撞墙/撞地/撞方块，就返回 0。 */
static int can_place(int nx, int ny) {
    int w = SHAPE_SIZE[type][rot][0];
    int h = SHAPE_SIZE[type][rot][1];

    for (int cy = 0; cy < h; cy++)
        for (int cx = 0; cx < w; cx++) {
            if (!cell(cx, cy)) continue;      /* 空格不管 */
            int px = nx + cx, py = ny + cy;
            if (px < 0 || px >= W) return 0;  /* 左右墙 */
            if (py >= H)          return 0;  /* 地板 */
            if (py >= 0 && board[py][px]) return 0;  /* 撞到已有方块 */
        }
    return 1;   /* 所有格子都合法 */
}

/* 把方块固定到棋盘上 */
static void lock(void) {
    for (int cy = 0; cy < SHAPE_SIZE[type][rot][1]; cy++)
        for (int cx = 0; cx < SHAPE_SIZE[type][rot][0]; cx++)
            if (cell(cx, cy)) {
                int px = x + cx, py = y + cy;
                if (py >= 0 && py < H && px >= 0 && px < W) board[py][px] = 1;
            }
}

/* 生成新方块 */
static void spawn(void) {
    type = 6;                              /* 固定用 T 块，方便观察 */
    rot  = 0;
    x    = (W - SHAPE_SIZE[type][rot][0]) / 2;
    y    = 0;
    stopped = 0;
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
            r_put(line, &n, on || board[r][c]);
        }
        line[n++] = '|';
        line[n] = '\0';
        r_line(r + 1, line);
    }

    r_line(H + 1, border);
    r_line(H + 3, stopped ? "STOPPED - spawn next" : "falling...");
}

int main(void) {
#ifndef _WIN32
    key_init();
#endif
    spawn();
    r_enter();
    while (1) {
        show();
        if (key_hit()) {
            int k = key_get();
            if (k == 'Q' || k == 'q') break;
        }
        key_delay(120);

        if (!stopped) {
            if (can_place(x, y + 1)) y++;    /* 还能往下 → 走一格 */
            else { lock(); stopped = 1; }    /* 动不下去 → 停住并锁定 */
        } else {
            spawn();                        /* 停了就换下一个 */
        }
    }
    r_leave();
    return 0;
}
