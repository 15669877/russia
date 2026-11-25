/* ============================================================
 * step2_move.c —— 第 2 步：让飞机左右移动
 *
 * 这一步只解决一件事：**理解「移动 + 不留残影」**。
 *
 * 关键：不要在旧位置画空格来「擦掉」飞机 —— 那样会闪烁，而且容易漏。
 * 正确做法是**每帧先清空整个画布，再重新画一遍所有东西**。
 * 画布清空是纯内存操作，比往终端输出便宜得多；
 * 终端那边由 render.h 的增量渲染负责只输出变化的行。
 *
 * 本步还没有敌机，只有一架飞机在左右移动。
 * 操作：←/→ 或 a/d 移动，q 退出。
 *
 * 编译：gcc step2_move.c -o step2_move.exe
 * ============================================================ */
#include <stdio.h>
#include "key.h"
#include "render.h"
#include "entities.h"

#define W 40
#define H 12

int main(void) {
    char line[R_MAXCOLS];
    char border[R_MAXCOLS];
    int px = W / 2 - SPR_W[SPR_PLAYER][VAR_NORMAL] / 2;
    int py = H - SPR_H[SPR_PLAYER][VAR_NORMAL] - 1;

    r_enter();

    while (1) {
        /* ---- 输入 ---- */
        if (key_hit()) {
            switch (key_get()) {
                case 'L': case 'a':
                    if (px > 0) px--;
                    break;
                case 'R': case 'd':
                    if (px + SPR_W[SPR_PLAYER][VAR_NORMAL] < W) px++;
                    break;
                case 'Q': case 'q':
                    goto done;
                default: break;
            }
        }

        /* ---- 每帧重建画面：先清空，再画 ---- */
        r_canvas_clear();
        r_canvas_stamp(py, px, SPR_ART[SPR_PLAYER][VAR_NORMAL],
                       SPR_H[SPR_PLAYER][VAR_NORMAL]);

        r_frame_start();
        r_border(border, W);
        r_line(0, border);
        for (int r = 0; r < H; r++) {
            r_field_row(line, r, W);
            r_line(r + 1, line);
        }
        r_line(H + 1, border);
        r_line(H + 3, "step2: move with arrows / a d. press q to quit.");

        key_delay(30);
    }

done:
    r_leave();
    return 0;
}
