/* ============================================================
 * step5_wave.c —— 第 5 步：敌机成波出现 + 难度递增
 *
 * 这一步只解决一件事：**理解「波次表驱动出怪」**。
 *
 * 之前敌机是手摆的固定位置。现在改成：每一波有一张「出怪表」，
 * 写着这一波要出哪些敌机、第几帧出。表是**生成器算出来的**
 * （见 entities.h 的 SHOOT_WAVES），不是手写的。
 *
 * 波次越高 -> 敌机更多、更快、血更厚。这三条都由波次表的
 * count / spd_mul / hp_mul 三个字段控制，不用改代码。
 *
 * 本步还加入了敌机开火和玩家掉命，接近完整版。
 * 操作：←/→ 移动，空格 射击，x 炸弹，q 退出。
 *
 * 编译：gcc step5_wave.c -o step5_wave.exe
 * ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "key.h"
#include "render.h"
#include "entities.h"
#include "game.h"

static Game G;
static int  GH = 16;

static void show(void) {
    char line[R_MAXCOLS];
    char border[R_MAXCOLS];

    r_canvas_clear();

    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &G.ents[i];
        if (!e->active) continue;

        if (e->kind == ENT_EXPLO) {
            int f = e->timer / 2;
            if (f > 2) f = 2;
            int cx = e->x + SPR_W[e->spr][e->var] / 2;
            int cy = e->y + SPR_H[e->spr][e->var] / 2;
            r_canvas_stamp(cy - EXPLO_H[f] / 2, cx - EXPLO_W[f] / 2,
                           EXPLO_ART[f], EXPLO_H[f]);
            continue;
        }
        int var = e->var;
        if (e->kind == ENT_ENEMY && e->hp * 2 <= e->maxhp) var = VAR_HURT;
        r_canvas_stamp(e->y, e->x, SPR_ART[e->spr][var], SPR_H[e->spr][var]);
    }

    if (!(G.p.invuln > 0 && (G.frame / 4) % 2 == 0))
        r_canvas_stamp(G.p.y, G.p.x, SPR_ART[SPR_PLAYER][VAR_NORMAL],
                       SPR_H[SPR_PLAYER][VAR_NORMAL]);

    r_frame_start();
    r_border(border, GW);
    r_line(0, border);
    for (int r = 0; r < GH; r++) {
        r_field_row(line, r, GW);
        r_line(r + 1, line);
    }
    r_line(GH + 1, border);
    snprintf(line, sizeof line,
             "step5: waves. score %d  wave %d/%d  lives %d  fire %d  bombs %d",
             G.score, G.wave + 1, SHOOT_WAVE_MAX, G.p.lives,
             G.p.fire_lv + 1, G.p.bombs);
    r_line(GH + 3, line);
}

int main(void) {
#ifndef _WIN32
    key_init();
#endif
    {
        int rows = term_rows();
        if (rows > 0) {
            GH = rows - 7;
            if (GH > GHMAX) GH = GHMAX;
            if (GH < 12)   GH = 12;
        }
    }

    game_init(&G, (unsigned int)time(NULL));
    G.p.y = GH - SPR_H[SPR_PLAYER][VAR_NORMAL] - 1;

    r_enter();

    while (!G.over) {
        show();
        if (key_hit()) {
            switch (key_get()) {
                case 'L': case 'a': game_move(&G, -1); break;
                case 'R': case 'd': game_move(&G,  1); break;
                case ' ':              game_fire(&G);   break;
                case 'X': case 'x':    game_bomb(&G);   break;
                case 'Q': case 'q':    goto done;
                default: break;
            }
        }
        game_update(&G, GH);
        key_delay(16);
    }
    show();

done:
    r_leave();
    return 0;
}
