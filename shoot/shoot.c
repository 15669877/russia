/* ============================================================
 * shoot.c —— 极简打飞机（纯终端，无任何 UI 装饰）
 *
 * 模块划分（每块都能单独拆出来学）：
 *   1) 素材数据        —— entities.h（由 gen_entities.c 生成，勿手改）
 *   2) 核心逻辑        —— game.h（与 selftest / sim 共用同一份）
 *   3) 显示端 show()   —— 画布绘制 + 增量渲染，画面完全不闪
 *   4) 输入端          —— key.h 跨平台非阻塞读键
 *
 * 编译：
 *   Visual Studio：打开 shoot.sln，Ctrl+F5
 *   MinGW       ：gcc shoot.c -o shoot.exe
 *
 * 操作：←/→ 或 a/d 移动   空格 射击   x 炸弹   q 退出
 * ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "key.h"
#include "render.h"
#include "entities.h"
#include "game.h"

static Game G;
static int  GH = 24;          /* 实际战场高，main() 里按窗口高度算出来 */

/* ---------- 显示端 ----------
 * 把所有实体盖到字符画布上，再逐行输出。
 * 画布每帧清空重建，所以不会出现任何残影。
 */
static void show(void) {
    char line[R_MAXCOLS];
    char border[R_MAXCOLS];

    r_canvas_clear();

    /* 1) 实体：按 y 从小到大画，后面的自然盖在前面 */
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < MAX_ENTS; i++) {
            Ent *e = &G.ents[i];
            if (!e->active) continue;

            if (e->kind == ENT_EXPLO) {
                /* 爆炸特效：3 帧动画，按 timer 选帧 */
                int f = e->timer / 2;
                if (f > 2) f = 2;
                int w = EXPLO_W[f], h = EXPLO_H[f];
                /* 爆炸以敌机中心为心 */
                int cx = e->x + SPR_W[e->spr][e->var] / 2;
                int cy = e->y + SPR_H[e->spr][e->var] / 2;
                r_canvas_stamp(cy - h / 2, cx - w / 2, EXPLO_ART[f], h);
                continue;
            }

            /* 敌机血量掉到一半以下显示受损形态 */
            int var = e->var;
            if (e->kind == ENT_ENEMY && e->hp * 2 <= e->maxhp)
                var = VAR_HURT;

            r_canvas_stamp(e->y, e->x, SPR_ART[e->spr][var],
                           SPR_H[e->spr][var]);
        }
    }

    /* 2) 玩家：无敌闪烁 */
    if (!(G.p.invuln > 0 && (G.frame / 4) % 2 == 0)) {
        r_canvas_stamp(G.p.y, G.p.x, SPR_ART[SPR_PLAYER][VAR_NORMAL],
                       SPR_H[SPR_PLAYER][VAR_NORMAL]);
    }

    /* 3) 组装输出 */
    r_frame_start();

    r_border(border, GW);
    r_line(0, border);

    for (int r = 0; r < GH; r++) {
        r_field_row(line, r, GW);
        r_line(r + 1, line);
    }

    r_line(GH + 1, border);

    /* 状态行 */
    snprintf(line, sizeof line,
             "score %-6d  wave %d/%d  lives %d  fire %d  bombs %d%s",
             G.score, G.wave + 1, SHOOT_WAVE_MAX, G.p.lives,
             G.p.fire_lv + 1, G.p.bombs,
             G.over ? "    GAME OVER" : "");
    r_line(GH + 3, line);

    /* BOSS 血条 */
    if (G.boss_max > 0 && G.boss_hp > 0) {
        int n = 0;
        line[n++] = ' ';
        line[n++] = 'B';
        line[n++] = 'o';
        line[n++] = 's';
        line[n++] = 's';
        line[n++] = ' ';
        line[n] = '\0';
        int bars = GW * G.boss_hp / G.boss_max;
        for (int i = 0; i < GW; i++)
            line[n++] = (i < bars) ? '#' : '.';
        line[n] = '\0';
        r_line(GH + 4, line);
    } else {
        r_line(GH + 4, " ");
    }
}

int main(void) {
#ifndef _WIN32
    key_init();
#endif
    /* 按终端窗口高度定战场高度：上下边框 2 行 + 状态行留 4 行。
     * 打印行数一旦超过窗口高度，终端就会滚动 —— 滚动看起来就是画面在跳。 */
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
        key_delay(16);          /* 约 60 帧上限 */
    }
    show();

done:
    r_leave();
    return 0;
}
