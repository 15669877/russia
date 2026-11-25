/* ============================================================
 * step3_shoot.c —— 第 3 步：发射子弹
 *
 * 这一步只解决一件事：**理解「子弹 = 独立实体」**。
 *
 * 子弹不是「画在飞机上的一个符号」，而是一个有自己坐标的小实体，
 * 每帧向上移动一格，出了顶边就消失。
 * 玩家按一次空格 -> 造一个子弹实体 -> 之后它自己飞。
 *
 * 这里还没有敌机和碰撞，先只练「生成实体 + 实体自动运动 + 出界回收」。
 *
 * 操作：←/→ 移动，空格 射击，q 退出。
 *
 * 编译：gcc step3_shoot.c -o step3_shoot.exe
 * ============================================================ */
#include <stdio.h>
#include "key.h"
#include "render.h"
#include "entities.h"

#define W 40
#define H 12
#define MAX_BULLETS 30

/* 子弹：x 是列，y 是行，alive 表示还在不在场 */
static int bx[MAX_BULLETS], by[MAX_BULLETS], balive[MAX_BULLETS];

int main(void) {
    char line[R_MAXCOLS];
    char border[R_MAXCOLS];
    int px = W / 2 - SPR_W[SPR_PLAYER][VAR_NORMAL] / 2;
    int py = H - SPR_H[SPR_PLAYER][VAR_NORMAL] - 1;
    int cooldown = 0;

    for (int i = 0; i < MAX_BULLETS; i++) balive[i] = 0;

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
                case ' ':
                    /* 造一颗子弹：从机头顶端射出 */
                    if (cooldown == 0) {
                        for (int i = 0; i < MAX_BULLETS; i++) {
                            if (!balive[i]) {
                                bx[i] = px + SPR_W[SPR_PLAYER][VAR_NORMAL] / 2;
                                by[i] = py - 1;
                                balive[i] = 1;
                                break;
                            }
                        }
                        cooldown = 10;    /* 冷却，避免连发刷屏 */
                    }
                    break;
                case 'Q': case 'q':
                    goto done;
                default: break;
            }
        }
        if (cooldown > 0) cooldown--;

        /* ---- 子弹自己往上飞，出界就回收 ---- */
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (!balive[i]) continue;
            by[i]--;
            if (by[i] < -1) balive[i] = 0;
        }

        /* ---- 重建画面 ---- */
        r_canvas_clear();
        r_canvas_stamp(py, px, SPR_ART[SPR_PLAYER][VAR_NORMAL],
                       SPR_H[SPR_PLAYER][VAR_NORMAL]);
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (!balive[i]) continue;
            r_canvas_stamp(by[i], bx[i], SPR_ART[SPR_BULLET][VAR_NORMAL],
                           SPR_H[SPR_BULLET][VAR_NORMAL]);
        }

        r_frame_start();
        r_border(border, W);
        r_line(0, border);
        for (int r = 0; r < H; r++) {
            r_field_row(line, r, W);
            r_line(r + 1, line);
        }
        r_line(H + 1, border);
        r_line(H + 3, "step3: press SPACE to shoot. arrows to move, q to quit.");

        key_delay(30);
    }

done:
    r_leave();
    return 0;
}
