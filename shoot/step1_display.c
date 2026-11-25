/* ============================================================
 * step1_display.c —— 第 1 步：把一架飞机画出来
 *
 * 这一步只解决一件事：**理解「精灵 = 一组字符串」**。
 *
 * 敌机和玩家都是字符画精灵，每个精灵就是「每行一个字符串」的数组。
 * 空格是透明的，非空格就是实体像素。宽度 = 最长那行的长度。
 *
 * 本步只看画面，不动、不打子弹。
 * 按 q 退出。
 *
 * 编译：gcc step1_display.c -o step1_display.exe
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

    r_enter();

    while (1) {
        r_canvas_clear();

        /* 把玩家机放在中间 —— 精灵左上角是 (x, y) */
        r_canvas_stamp(4, W / 2 - SPR_W[SPR_PLAYER][VAR_NORMAL] / 2,
                       SPR_ART[SPR_PLAYER][VAR_NORMAL],
                       SPR_H[SPR_PLAYER][VAR_NORMAL]);

        /* 把三种敌机并排放在上面 */
        r_canvas_stamp(0, 2,  SPR_ART[SPR_ENEMY_A][VAR_NORMAL], SPR_H[SPR_ENEMY_A][VAR_NORMAL]);
        r_canvas_stamp(0, 12, SPR_ART[SPR_ENEMY_B][VAR_NORMAL], SPR_H[SPR_ENEMY_B][VAR_NORMAL]);
        r_canvas_stamp(0, 22, SPR_ART[SPR_ENEMY_C][VAR_NORMAL], SPR_H[SPR_ENEMY_C][VAR_NORMAL]);
        r_canvas_stamp(0, 30, SPR_ART[SPR_BOSS][VAR_NORMAL],    SPR_H[SPR_BOSS][VAR_NORMAL]);

        r_frame_start();
        r_border(border, W);
        r_line(0, border);
        for (int r = 0; r < H; r++) {
            r_field_row(line, r, W);
            r_line(r + 1, line);
        }
        r_line(H + 1, border);
        r_line(H + 3, "step1: sprite drawing only. press q to quit.");

        if (key_hit() && key_get() == 'q') break;
        key_delay(40);
    }

    r_leave();
    return 0;
}
