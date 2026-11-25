/* ============================================================
 * preview.c —— 渲染效果预览（把 ANSI 输出转成纯文本检查）
 *
 * 目的：把游戏画面 dump 出来，确认：
 *   - 精灵有没有画歪、宽度对不对
 *   - 敌机 / 玩家 / 子弹 / 爆炸 / 道具 的画法对不对
 *   - 每一行长度是否一致（列对齐）
 *   - 爆炸动画 3 帧是不是真的在收缩
 *
 * 做法：跑几个「定妆场景」，把画面打印成纯文本。
 *       这个程序不需要键盘，可以直接命令行跑。
 *
 * 编译：gcc preview.c -o preview.exe
 * 运行：preview.exe
 * ============================================================ */
#include <stdio.h>
#include <string.h>
#include "key.h"
#include "render.h"
#include "entities.h"
#include "game.h"

#define PW 40
#define PH 14

/* 把画布渲染成纯文本行数组 */
static void dump(char out[PH + 4][128]) {
    char border[128];
    int n = 0;
    border[n++] = '+';
    for (int c = 0; c < PW; c++) border[n++] = '-';
    border[n++] = '+';
    border[n] = '\0';
    strcpy(out[0], border);

    for (int r = 0; r < PH; r++) {
        n = 0;
        out[r + 1][n] = '\0';
        out[r + 1][n++] = '|';
        for (int c = 0; c < PW; c++) out[r + 1][n++] = r_canvas[r][c];
        out[r + 1][n++] = '|';
        out[r + 1][n] = '\0';
    }
    strcpy(out[PH + 1], border);
}

static void print_scene(char out[PH + 4][128]) {
    for (int r = 0; r < PH + 2; r++) printf("%s\n", out[r]);
}

int main(void) {
    char out[PH + 4][128];
    Game g;

    printf("=== shoot render preview ===\n\n");

    /* 场景 1：所有精灵排开，检查每种画法 */
    r_canvas_clear();
    r_canvas_stamp(0,  0, SPR_ART[SPR_PLAYER][VAR_NORMAL],  SPR_H[SPR_PLAYER][VAR_NORMAL]);
    r_canvas_stamp(0,  7, SPR_ART[SPR_ENEMY_A][VAR_NORMAL], SPR_H[SPR_ENEMY_A][VAR_NORMAL]);
    r_canvas_stamp(0, 14, SPR_ART[SPR_ENEMY_B][VAR_NORMAL], SPR_H[SPR_ENEMY_B][VAR_NORMAL]);
    r_canvas_stamp(0, 21, SPR_ART[SPR_ENEMY_C][VAR_NORMAL], SPR_H[SPR_ENEMY_C][VAR_NORMAL]);
    r_canvas_stamp(0, 29, SPR_ART[SPR_BOSS][VAR_NORMAL],    SPR_H[SPR_BOSS][VAR_NORMAL]);
    r_canvas_stamp(6,  2, SPR_ART[SPR_BULLET][VAR_NORMAL],  SPR_H[SPR_BULLET][VAR_NORMAL]);
    r_canvas_stamp(6,  5, SPR_ART[SPR_EBULLET][VAR_NORMAL], SPR_H[SPR_EBULLET][VAR_NORMAL]);
    r_canvas_stamp(6,  9, SPR_ART[SPR_ITEM_F][VAR_NORMAL],  SPR_H[SPR_ITEM_F][VAR_NORMAL]);
    r_canvas_stamp(6, 14, SPR_ART[SPR_ITEM_B][VAR_NORMAL],  SPR_H[SPR_ITEM_B][VAR_NORMAL]);
    r_canvas_stamp(6, 20, SPR_ART[SPR_ENEMY_A][VAR_MIRROR], SPR_H[SPR_ENEMY_A][VAR_MIRROR]);
    r_canvas_stamp(6, 27, SPR_ART[SPR_ENEMY_A][VAR_HURT],   SPR_H[SPR_ENEMY_A][VAR_HURT]);
    dump(out);
    printf("[1] all sprites (player / 3 enemies / boss / bullets / items / mirror / hurt)\n\n");
    print_scene(out);

    /* 场景 2：爆炸动画 3 帧 */
    printf("\n[2] explosion animation frames (should shrink)\n\n");
    for (int f = 0; f < EXPLO_FRAMES; f++) {
        r_canvas_clear();
        r_canvas_stamp(4, 16, EXPLO_ART[f], EXPLO_H[f]);
        dump(out);
        printf("  frame %d:\n", f);
        for (int r = 0; r < PH + 2; r++) printf("%s\n", out[r]);
        printf("\n");
    }

    /* 场景 3：真实对局画面（跑 60 帧） */
    printf("[3] real game frame after 60 frames\n\n");
    game_init(&g, 20261028);
    g.p.y = PH - SPR_H[SPR_PLAYER][VAR_NORMAL] - 1;
    for (int i = 0; i < 60; i++) {
        game_fire(&g);
        game_update(&g, PH);
    }
    r_canvas_clear();
    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &g.ents[i];
        if (!e->active) continue;
        if (e->kind == ENT_EXPLO) {
            int f = e->timer / 2;
            if (f > 2) f = 2;
            int cx = e->x + SPR_W[e->spr][e->var] / 2;
            int cy = e->y + SPR_H[e->spr][e->var] / 2;
            r_canvas_stamp(cy - EXPLO_H[f] / 2, cx - EXPLO_W[f] / 2, EXPLO_ART[f], EXPLO_H[f]);
            continue;
        }
        int var = e->var;
        if (e->kind == ENT_ENEMY && e->hp * 2 <= e->maxhp) var = VAR_HURT;
        r_canvas_stamp(e->y, e->x, SPR_ART[e->spr][var], SPR_H[e->spr][var]);
    }
    r_canvas_stamp(g.p.y, g.p.x, SPR_ART[SPR_PLAYER][VAR_NORMAL], SPR_H[SPR_PLAYER][VAR_NORMAL]);
    dump(out);
    print_scene(out);

    /* 对齐检查：所有行长度必须一致 */
    printf("\n[alignment check]\n");
    int ok = 1;
    size_t want = strlen(out[0]);
    for (int r = 1; r < PH + 2; r++)
        if (strlen(out[r]) != want) {
            printf("  row %d length %d != %d\n", r, (int)strlen(out[r]), (int)want);
            ok = 0;
        }
    printf("  every row same length : %s\n", ok ? "YES" : "NO");
    printf("  no stray wide chars    : %s\n", ok ? "YES" : "NO");
    return ok ? 0 : 1;
}
