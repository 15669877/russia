/* ============================================================
 * step5_rotate.c —— 第 5 步：方块变方向（旋转）
 *
 * 【这一步只解决一件事】
 *   按键让方块旋转，并且靠墙旋转时不会被卡死或越界。
 *
 * 【要理解的知识点】
 *   1) 旋转数据：每个形状有 4 个旋转态，存在 shapes.h 里。
 *      rot 变量就是「当前是第几个旋转态」，0~3 循环。
 *
 *   2) 旋转后尺寸会变！比如 I 块：
 *        横躺时 4x1（宽 4 高 1）
 *        立起来时 1x4（宽 1 高 4）
 *      所以旋转不只是换个图形，宽高也跟着变，边界检查必须用新的宽高。
 *
 *   3) 旋转的「手感」问题：
 *      如果直接原地旋转，靠墙时新图形会超出屏幕，旋转就失败了。
 *      解决办法：先让「新旧包围盒的中心」尽量重合，
 *      如果还是放不下，再左右上下微调几格试试（这就是所谓 wall kick）。
 *
 *   4) 关键点：所有尝试都还要用 can_place() 检查。
 *
 * 【怎么跑】
 *   Visual Studio：右键 step5_rotate → 设为启动项目 → Ctrl+F5
 *   方向键移动，w 或 ↑ 旋转，数字键 0~6 换形状，q 退出。
 * ============================================================ */
#include <stdio.h>
#include "key.h"
#include "render.h"
#include "shapes.h"

#define W 14
#define H 12

static int board[H][W];
static int type = 1, rot = 0;   /* 默认 I 块，最能看出旋转变化 */
static int x = 5, y = 4;

static int cell(int cx, int cy) {
    int w = SHAPE_SIZE[type][rot][0];
    if (cy >= SHAPE_SIZE[type][rot][1] || cx >= w) return 0;
    return (SHAPE[type][rot][cy] >> (w - 1 - cx)) & 1;
}

static int can_place(int nt, int nr, int nx, int ny) {
    int w = SHAPE_SIZE[nt][nr][0], h = SHAPE_SIZE[nt][nr][1];
    for (int cy = 0; cy < h; cy++)
        for (int cx = 0; cx < w; cx++) {
            if (!cell(cx, cy)) continue;
            int px = nx + cx, py = ny + cy;
            if (px < 0 || px >= W) return 0;
            if (py >= H)          return 0;
            if (py >= 0 && board[py][px]) return 0;
        }
    return 1;
}

/* 旋转一次：先让新旧中心重合，再尝试微调 */
static void rotate(int d) {
    int nr = (rot + d + 4) % 4;
    int ow = SHAPE_SIZE[type][rot][0], oh = SHAPE_SIZE[type][rot][1];
    int nw = SHAPE_SIZE[type][nr][0], nh = SHAPE_SIZE[type][nr][1];

    int cx = x + (ow - 1) / 2, cy = y + (oh - 1) / 2;   /* 旧中心 */
    int bx = cx - (nw - 1) / 2, by = cy - (nh - 1) / 2;   /* 新位置 */

    int cand[6][2] = { {bx,by}, {bx-1,by}, {bx+1,by},
                        {bx-2,by}, {bx+2,by}, {bx,by-1} };
    for (int i = 0; i < 6; i++)
        if (can_place(type, nr, cand[i][0], cand[i][1])) {
            x = cand[i][0]; y = cand[i][1]; rot = nr; return;
        }
    /* 一个位置都放不下 → 保持原状（旋转失败） */
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

    snprintf(line, sizeof line, "type %d  rot %d  size %dx%d",
             type, rot, SHAPE_SIZE[type][rot][0], SHAPE_SIZE[type][rot][1]);
    r_line(H + 3, line);
    r_line(H + 4, "arrows move   w/up rotate   0-6 change shape   q quit");
}

int main(void) {
#ifndef _WIN32
    key_init();
#endif
    r_enter();
    while (1) {
        show();
        if (!key_hit()) { key_delay(16); continue; }

        int k = key_get();
        switch (k) {
            case 'L': case 'a': if (can_place(type, rot, x - 1, y)) x--; break;
            case 'R': case 'd': if (can_place(type, rot, x + 1, y)) x++; break;
            case 'U': case 'w': rotate(1); break;
            case 'D': case 's': if (can_place(type, rot, x, y + 1)) y++; break;
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6':
                type = k - '0'; rot = 0;
                x = (W - SHAPE_SIZE[type][rot][0]) / 2;
                y = 0;
                break;
            case 'Q': case 'q': goto done;
            default: break;
        }
    }
done:
    r_leave();
    return 0;
}
