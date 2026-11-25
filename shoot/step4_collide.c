/* ============================================================
 * step4_collide.c —— 第 4 步：子弹打中敌机
 *
 * 这一步只解决一件事：**理解「碰撞判定」**。
 *
 * 碰撞有两种做法：
 *   1) 矩形相交 —— 快，但会把精灵的透明角也算进去，斜角会误伤
 *   2) 逐实心格比对 —— 准，代价是慢一点
 * 打飞机用的是第 2 种：只有精灵 art 里非空格的格子才算实体。
 *
 * 注意「实心格」这个概念来自 entities.h 的素材本身：
 * 空格透明 = 那一格不算机身。这让「谁打中谁」完全由美术决定。
 *
 * 本步加入了敌机和碰撞，但敌机是固定不动的（只是让你看清判定过程）。
 * 操作：←/→ 移动，空格 射击，q 退出。
 *
 * 编译：gcc step4_collide.c -o step4_collide.exe
 * ============================================================ */
#include <stdio.h>
#include "key.h"
#include "render.h"
#include "entities.h"

#define W 40
#define H 14
#define MAX_BULLETS 30
#define MAX_ENEMIES 6

static int bx[MAX_BULLETS], by[MAX_BULLETS], balive[MAX_BULLETS];
static int ex[MAX_ENEMIES], ey[MAX_ENEMIES], ehp[MAX_ENEMIES];
static int espr[MAX_ENEMIES];

/* 判断精灵 (spr,var) 放在 (x,y) 时，第 (cx,cy) 格是不是实心 */
static int solid(int spr, int var, int cx, int cy) {
    int h = SPR_H[spr][var];
    if (cy < 0 || cy >= h) return 0;
    const char *row = SPR_ART[spr][var][cy];
    if (!row) return 0;
    int w = SPR_W[spr][var];
    if (cx < 0 || cx >= w) return 0;
    return row[cx] != ' ';
}

/* 两个精灵是否重叠（逐实心格比对） */
static int overlap(int s1, int v1, int x1, int y1,
                   int s2, int v2, int x2, int y2) {
    /* 先用矩形快速排除 —— 大部分情况到这里就返回了 */
    if (x1 + SPR_W[s1][v1] <= x2 || x2 + SPR_W[s2][v2] <= x1) return 0;
    if (y1 + SPR_H[s1][v1] <= y2 || y2 + SPR_H[s2][v2] <= y1) return 0;

    for (int cy = 0; cy < SPR_H[s1][v1]; cy++)
        for (int cx = 0; cx < SPR_W[s1][v1]; cx++) {
            if (!solid(s1, v1, cx, cy)) continue;
            for (int dy = 0; dy < SPR_H[s2][v2]; dy++)
                for (int dx = 0; dx < SPR_W[s2][v2]; dx++) {
                    if (!solid(s2, v2, dx, dy)) continue;
                    if (x1 + cx == x2 + dx && y1 + cy == y2 + dy) return 1;
                }
        }
    return 0;
}

int main(void) {
    char line[R_MAXCOLS];
    char border[R_MAXCOLS];
    int px = W / 2 - SPR_W[SPR_PLAYER][VAR_NORMAL] / 2;
    int py = H - SPR_H[SPR_PLAYER][VAR_NORMAL] - 1;
    int cooldown = 0, score = 0;

    for (int i = 0; i < MAX_BULLETS; i++) balive[i] = 0;

    /* 摆 6 架敌机，两种机型 */
    for (int i = 0; i < MAX_ENEMIES; i++) {
        espr[i] = (i % 2) ? SPR_ENEMY_B : SPR_ENEMY_A;
        ex[i] = 4 + i * 6;
        ey[i] = 2 + (i % 3) * 3;
        ehp[i] = 3;
    }

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
                    if (cooldown == 0) {
                        for (int i = 0; i < MAX_BULLETS; i++) {
                            if (!balive[i]) {
                                bx[i] = px + SPR_W[SPR_PLAYER][VAR_NORMAL] / 2;
                                by[i] = py - 1;
                                balive[i] = 1;
                                break;
                            }
                        }
                        cooldown = 8;
                    }
                    break;
                case 'Q': case 'q':
                    goto done;
                default: break;
            }
        }
        if (cooldown > 0) cooldown--;

        /* ---- 子弹飞行 ---- */
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (!balive[i]) continue;
            by[i]--;
            if (by[i] < -1) balive[i] = 0;
        }

        /* ---- 碰撞：子弹 -> 敌机 ---- */
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (!balive[i]) continue;
            for (int j = 0; j < MAX_ENEMIES; j++) {
                if (ehp[j] <= 0) continue;
                if (!overlap(SPR_BULLET, VAR_NORMAL, bx[i], by[i],
                             espr[j], VAR_NORMAL, ex[j], ey[j])) continue;

                balive[i] = 0;          /* 子弹消失 */
                ehp[j]--;
                if (ehp[j] <= 0) score += 100;
                break;                  /* 一颗子弹只打一架 */
            }
        }

        /* ---- 重建画面 ---- */
        r_canvas_clear();
        for (int j = 0; j < MAX_ENEMIES; j++) {
            if (ehp[j] <= 0) continue;
            /* 血量过半就显示受损形态 */
            int var = (ehp[j] * 2 <= 3) ? VAR_HURT : VAR_NORMAL;
            r_canvas_stamp(ey[j], ex[j], SPR_ART[espr[j]][var], SPR_H[espr[j]][var]);
        }
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (!balive[i]) continue;
            r_canvas_stamp(by[i], bx[i], SPR_ART[SPR_BULLET][VAR_NORMAL],
                           SPR_H[SPR_BULLET][VAR_NORMAL]);
        }
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
        snprintf(line, sizeof line,
                 "step4: collision. score %d. SPACE shoot, arrows move, q quit.", score);
        r_line(H + 3, line);

        key_delay(30);
    }

done:
    r_leave();
    return 0;
}
