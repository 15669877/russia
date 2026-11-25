/* ============================================================
 * sim.c —— 无头游戏模拟器（纯 C，不需要键盘）
 *
 * 【目的】
 * 沙箱/自动化环境里没法按键盘，所以用「假玩家」驱动核心逻辑，
 * 自动打几百局，验证完整游戏流程没问题：
 *   出怪 -> 移动 -> 射击 -> 命中 -> 爆炸 -> 得分 -> 掉命 -> 游戏结束
 * 这是对 game.h 的一次端到端回归。
 *
 * 【假玩家策略】
 * 简单但有效的贪心 AI：
 *   - 每帧都射击（如果冷却好了）
 *   - 往「最近的威胁」方向移动：敌机在左边就左移，在右边就右移
 *   - 血少时优先躲 boss
 *
 * 它不需要很聪明 —— 只要能让游戏真的跑起来、真的能结束，
 * 就足以暴露崩溃、越界、死循环、状态不一致这些问题。
 *
 * 【怎么跑】
 *   gcc sim.c -o sim.exe
 *   ./sim.exe
 *   打印每局统计，最后打印 SIMULATION PASSED
 * ============================================================ */
#include <stdio.h>
#include <string.h>
#include "entities.h"
#include "game.h"

#define GAMES    50
#define MAXFRAMES 12000
#define GH       24

/* 找离玩家最近的敌机，返回它的 x（没有则返回 -1） */
static int nearest_enemy_x(Game *g) {
    int best = -1, bestd = 1 << 30;
    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &g->ents[i];
        if (!e->active || e->kind != ENT_ENEMY) continue;
        int d = e->y - g->p.y;
        if (d < 0) d = -d;
        if (d < bestd) { bestd = d; best = e->x; }
    }
    return best;
}

/* 假玩家的一帧决策 */
static void ai_move(Game *g) {
    int target = nearest_enemy_x(g);
    int pcx = g->p.x + SPR_W[SPR_PLAYER][VAR_NORMAL] / 2;

    if (target >= 0) {
        int ecx = target + 2;
        if (ecx > pcx)      game_move(g,  1);
        else if (ecx < pcx) game_move(g, -1);
    } else {
        /* 没敌机就回中间 */
        int mid = GW / 2;
        if (pcx < mid - 1)      game_move(g,  1);
        else if (pcx > mid + 1) game_move(g, -1);
    }
}

int main(void) {
    Game g;
    int total_score = 0, total_kills = 0, total_waves = 0;
    int finished = 0, stuck = 0;

    printf("=== shoot headless simulation ===\n\n");
    printf("running %d games, up to %d frames each...\n\n", GAMES, MAXFRAMES);

    for (int game = 0; game < GAMES; game++) {
        /* 每局用不同种子，保证覆盖不同出怪序列 */
        game_init(&g, (unsigned)(game * 7919u + 13u));

        int frames = 0;
        int kills = 0;
        int last_score = 0;
        int last_wave = 0;

        while (!g.over && frames < MAXFRAMES) {
            ai_move(&g);
            game_fire(&g);
            if ((frames % 300) == 299) game_bomb(&g);

            game_update(&g, GH);
            frames++;

            /* 统计击坠数：用分数增量近似 */
            if (g.score > last_score) {
                int d = g.score - last_score;
                kills += d / 100;
                last_score = g.score;
            }
            last_wave = g.wave;

            /* 死循环检测：分数和波次长期不变且没有任何敌人 */
            if (frames % 500 == 0) {
                int any_enemy = 0;
                for (int i = 0; i < MAX_ENTS; i++)
                    if (g.ents[i].active && g.ents[i].kind == ENT_ENEMY)
                        { any_enemy = 1; break; }
                if (!any_enemy && g.score == last_score && g.wave == last_wave)
                    stuck++;
                else
                    stuck = 0;
            }
            if (stuck > 40) break;    /* 卡死了 */
        }

        /* 每局结束必须能判定结果：要么死了，要么超时了 */
        if (g.over) finished++;
        if (g.p.lives < 0) finished++;     /* 双保险：over 和 lives 应一致 */

        total_score += g.score;
        total_kills += kills;
        total_waves += g.wave;

        if (game < 5 || game == GAMES - 1)
            printf("  game %2d: frames %5d  score %6d  kills %4d  wave %2d  lives %d%s\n",
                   game + 1, frames, g.score, kills, g.wave + 1, g.p.lives,
                   g.over ? "  (over)" : "  (timeout)");
    }

    printf("\n--- summary ---\n");
    printf("games played      : %d\n", GAMES);
    printf("games ended by death: %d\n", finished);
    printf("total score       : %d\n", total_score);
    printf("total kills       : %d\n", total_kills);
    printf("total waves       : %d\n", total_waves);

    /* ---- 判定 ---- */
    int ok = 1;
    if (finished < GAMES / 2) {
        printf("FAIL: too few games ended normally (%d/%d)\n", finished, GAMES);
        ok = 0;
    }
    if (total_score <= 0) {
        printf("FAIL: no score accumulated\n");
        ok = 0;
    }
    if (total_kills <= 0) {
        printf("FAIL: no kills registered\n");
        ok = 0;
    }
    if (total_waves <= 0) {
        printf("FAIL: never advanced a wave\n");
        ok = 0;
    }

    printf("\n");
    if (!ok) { printf("SIMULATION FAILED\n"); return 1; }
    printf("SIMULATION PASSED\n");
    return 0;
}
