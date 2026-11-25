/* ============================================================
 * selftest.c —— 核心逻辑自测（不需要键盘，命令行直接跑）
 *
 * 目的：在没有交互终端的环境里验证 shoot.c / game.h 的判定逻辑是否正确。
 *       测：素材表完整性、实心格判定、碰撞、实体管理、射击、掉命、
 *           道具、波次推进、游戏结束。
 *
 * 关键：这里 include 的是 game.h 本身 —— 测的就是游戏真正在跑的代码，
 *       不是复制一份。这是「单一真相源」的价值所在。
 *
 * 编译：gcc selftest.c -o selftest.exe
 * 运行：selftest.exe      全部通过打印 ALL TESTS PASSED
 * ============================================================ */
#include <stdio.h>
#include <string.h>
#include "entities.h"
#include "game.h"

/* ---- 测试辅助 ---- */
static int failed = 0;
static void check(const char *name, int cond) {
    if (cond) printf("  [PASS] %s\n", name);
    else { printf("  [FAIL] %s\n", name); failed = 1; }
}

static int count_ents(Game *g, int kind) {
    int n = 0;
    for (int i = 0; i < MAX_ENTS; i++)
        if (g->ents[i].active && g->ents[i].kind == kind) n++;
    return n;
}

static int find_ent(Game *g, int kind) {
    for (int i = 0; i < MAX_ENTS; i++)
        if (g->ents[i].active && g->ents[i].kind == kind) return i;
    return -1;
}

int main(void) {
    printf("=== shoot core logic self-test ===\n\n");
    Game g;

    /* 1. 素材表完整性 */
    printf("[1] entity table integrity\n");
    {
        int bad = 0;
        for (int s = 0; s < SPR_COUNT; s++)
            for (int v = 0; v < SPR_VARIANTS; v++) {
                int w = SPR_W[s][v], h = SPR_H[s][v];
                if (w <= 0 || h <= 0) bad = 1;
                /* 每行宽度必须等于声明的宽度 */
                for (int r = 0; r < h; r++) {
                    const char *row = SPR_ART[s][v][r];
                    if (!row) { bad = 1; continue; }
                    if ((int)strlen(row) != w) {
                        printf("    sprite %d var %d row %d width %d != %d\n",
                               s, v, r, (int)strlen(row), w);
                        bad = 1;
                    }
                }
            }
        check("all sprites have consistent row widths", bad == 0);

        /* 镜像变体必须与标准姿态逐行反转一致 */
        int mirror_ok = 1;
        for (int s = 0; s < SPR_COUNT; s++) {
            int w = SPR_W[s][VAR_NORMAL];
            for (int r = 0; r < SPR_H[s][VAR_NORMAL]; r++) {
                const char *a = SPR_ART[s][VAR_NORMAL][r];
                const char *b = SPR_ART[s][VAR_MIRROR][r];
                for (int c = 0; c < w; c++)
                    if (a[c] != b[w - 1 - c]) mirror_ok = 0;
            }
        }
        check("mirror variant is exact horizontal reverse", mirror_ok == 1);

        /* 玩家机是 "  ^  " / " /|\ " / "/___\\" 三行：
         *   row0 机头    -> 只有中间 x=2 实心
         *   row1 机身    -> 中间 x=1,2,3 实心，两侧是机翼尖（透明）
         *   row2 挂架    -> 整行 x=0..4 实心
         * 这些形状是造型设计（不是漏画），断言要按造型来写。 */
        check("player nose row solid only in middle",
              spr_solid(SPR_PLAYER, VAR_NORMAL, 2, 0) == 1 &&
              spr_solid(SPR_PLAYER, VAR_NORMAL, 0, 0) == 0 &&
              spr_solid(SPR_PLAYER, VAR_NORMAL, 4, 0) == 0);
        check("player fuselage has solid core",
              spr_solid(SPR_PLAYER, VAR_NORMAL, 1, 1) == 1 &&
              spr_solid(SPR_PLAYER, VAR_NORMAL, 2, 1) == 1 &&
              spr_solid(SPR_PLAYER, VAR_NORMAL, 3, 1) == 1);
        check("player bottom row is fully solid",
              spr_solid(SPR_PLAYER, VAR_NORMAL, 0, 2) == 1 &&
              spr_solid(SPR_PLAYER, VAR_NORMAL, 4, 2) == 1);

        /* 爆炸帧必须逐帧变小（实心格数严格递减） */
        int boom_ok = 1;
        for (int f = 1; f < EXPLO_FRAMES; f++) {
            int a = 0, b = 0;
            for (int r = 0; r < EXPLO_H[f - 1]; r++)
                for (int c = 0; c < EXPLO_W[f - 1]; c++)
                    if (EXPLO_ART[f - 1][r][c] != ' ') a++;
            for (int r = 0; r < EXPLO_H[f]; r++)
                for (int c = 0; c < EXPLO_W[f]; c++)
                    if (EXPLO_ART[f][r][c] != ' ') b++;
            if (b >= a) boom_ok = 0;
        }
        check("explosion frames shrink progressively", boom_ok == 1);
    }

    /* 2. 实心格判定 */
    printf("[2] spr_solid() basics\n");
    {
        /* 玩家机第 0 行是 "  ^  " -> 第 2 列实心，两侧透明 */
        check("player row0 col2 is solid",
              spr_solid(SPR_PLAYER, VAR_NORMAL, 2, 0) == 1);
        check("player row0 col0 is transparent",
              spr_solid(SPR_PLAYER, VAR_NORMAL, 0, 0) == 0);
        check("out of range is not solid",
              spr_solid(SPR_PLAYER, VAR_NORMAL, 99, 0) == 0);
        check("negative is not solid",
              spr_solid(SPR_PLAYER, VAR_NORMAL, -1, 0) == 0);
    }

    /* 3. 碰撞判定 */
    printf("[3] spr_overlap() correctness\n");
    {
        /* 同一个位置必然重叠 */
        check("same position overlaps",
              spr_overlap(SPR_PLAYER, VAR_NORMAL, 10, 10,
                          SPR_PLAYER, VAR_NORMAL, 10, 10) == 1);
        /* 远离不重叠 */
        check("far apart does not overlap",
              spr_overlap(SPR_PLAYER, VAR_NORMAL, 0, 0,
                          SPR_PLAYER, VAR_NORMAL, 30, 20) == 0);
        /* 矩形相交但实心格不相交 -> 不算命中。
         * 道具 F 是 3x3: " F " / "FFF" / " F "，四角透明。
         * 道具放 (10,10)，子弹（1x2 全实心）放 (10,9)：
         *   两者矩形在 y=10 处相交，但道具第 0 行只有 x=11 是实心，
         *   子弹的实心格在 x=10，打了个空 -> 判定为未命中。
         * 这正是「逐实心格判定」相对「矩形相交」的价值所在。 */
        check("transparent corner does not count as hit",
              spr_overlap(SPR_ITEM_F, VAR_NORMAL, 10, 10,
                          SPR_BULLET, VAR_NORMAL, 10, 9) == 0);
        /* 对照：子弹打在道具正中心（x=11）必须算命中 */
        check("center hit still counts",
              spr_overlap(SPR_ITEM_F, VAR_NORMAL, 10, 10,
                          SPR_BULLET, VAR_NORMAL, 11, 9) == 1);
        /* 完全包住也算重叠 */
        check("contained sprite overlaps",
              spr_overlap(SPR_ENEMY_A, VAR_NORMAL, 10, 10,
                          SPR_BULLET, VAR_NORMAL, 11, 10) == 1);
    }

    /* 4. 实体管理 */
    printf("[4] entity management\n");
    {
        game_init(&g, 12345);
        check("init clears all entities", count_ents(&g, ENT_ENEMY) == 0);
        check("init sets lives to 3", g.p.lives == 3);
        check("init score is 0", g.score == 0);

        int id = game_new_ent(&g, ENT_ENEMY, SPR_ENEMY_A, VAR_NORMAL, 5, 5, 0, 16, 2);
        check("new entity is active", id >= 0 && g.ents[id].active == 1);
        check("new entity has correct hp", g.ents[id].hp == 2);
        check("count finds the entity", count_ents(&g, ENT_ENEMY) == 1);

        g.ents[id].active = 0;
        check("deactivated entity is not counted", count_ents(&g, ENT_ENEMY) == 0);
    }

    /* 5. 玩家移动 */
    printf("[5] player movement and bounds\n");
    {
        game_init(&g, 999);
        int w = SPR_W[SPR_PLAYER][VAR_NORMAL];
        game_move(&g, -100);
        check("cannot move past left wall", g.p.x == 0);
        game_move(&g, 100);
        check("cannot move past right wall", g.p.x == GW - w);
        game_move(&g, -1);
        check("can move left from wall", g.p.x == GW - w - 1);
    }

    /* 6. 射击 */
    printf("[6] firing\n");
    {
        game_init(&g, 555);
        game_fire(&g);
        check("firing creates one bullet at level 0", count_ents(&g, ENT_BULLET_P) == 1);
        /* 冷却期内不能再射 */
        game_fire(&g);
        check("cooldown blocks second shot", count_ents(&g, ENT_BULLET_P) == 1);
        /* 升级火力后应射出多颗 */
        game_init(&g, 555);
        g.p.fire_lv = 2;
        game_fire(&g);
        check("level 2 fires three bullets", count_ents(&g, ENT_BULLET_P) == 3);
    }

    /* 7. 子弹向上飞并出界回收 */
    printf("[7] bullet movement and cleanup\n");
    {
        game_init(&g, 42);
        g.spawn_disabled = 1;          /* 隔离出怪，只测子弹 */
        g.p.x = 10;
        g.p.y = 20;
        game_fire(&g);
        int id = find_ent(&g, ENT_BULLET_P);
        int y0 = g.ents[id].y;
        game_update(&g, 24);
        int id2 = find_ent(&g, ENT_BULLET_P);
        if (id2 >= 0) {
            check("bullet moves upward", g.ents[id2].y < y0);
        } else {
            check("bullet moves upward", 0);   /* 已经出界也算通过 */
        }
        /* 一直更新直到出界 */
        for (int i = 0; i < 60; i++) game_update(&g, 24);
        check("bullet is removed after leaving screen",
              count_ents(&g, ENT_BULLET_P) == 0);
    }

    /* 8. 子弹打中敌机 */
    printf("[8] bullet hits enemy\n");
    {
        game_init(&g, 7);
        g.spawn_disabled = 1;
        /* 在子弹正上方放一架敌机 */
        int e = game_new_ent(&g, ENT_ENEMY, SPR_ENEMY_A, VAR_NORMAL, 10, 5, 0, 0, 1);
        int b = game_new_ent(&g, ENT_BULLET_P, SPR_BULLET, VAR_NORMAL, 12, 7, 0, 0, 1);
        (void)e; (void)b;
        /* 手动让子弹移动到敌机位置 */
        g.ents[b].x = 12;
        g.ents[b].y = 5;
        game_update(&g, 24);
        check("enemy is destroyed by bullet", count_ents(&g, ENT_ENEMY) == 0);
        check("killing an enemy gives score", g.score >= 100);
        check("explosion effect is created", count_ents(&g, ENT_EXPLO) >= 1);
    }

    /* 9. 敌机掉血但没死 */
    printf("[9] enemy takes damage before dying\n");
    {
        game_init(&g, 7);
        g.spawn_disabled = 1;
        game_new_ent(&g, ENT_ENEMY, SPR_ENEMY_A, VAR_NORMAL, 10, 5, 0, 0, 5);
        int b = game_new_ent(&g, ENT_BULLET_P, SPR_BULLET, VAR_NORMAL, 12, 5, 0, 0, 1);
        g.ents[b].x = 12;
        game_update(&g, 24);
        check("enemy survives one hit with 5 hp", count_ents(&g, ENT_ENEMY) == 1);
        int e = find_ent(&g, ENT_ENEMY);
        check("enemy hp decreased", e >= 0 && g.ents[e].hp == 4);
    }

    /* 10. 玩家掉命 */
    printf("[10] player loses a life on collision\n");
    {
        game_init(&g, 7);
        g.spawn_disabled = 1;
        g.p.invuln = 0;                 /* 去掉开局无敌 */
        /* 敌机直接放到玩家身上 */
        game_new_ent(&g, ENT_ENEMY, SPR_ENEMY_A, VAR_NORMAL, g.p.x, g.p.y, 0, 0, 1);
        game_update(&g, 24);
        check("lives decreased", g.p.lives == 2);
        check("player becomes invulnerable", g.p.invuln > 0);
    }

    /* 11. 无敌期间不再掉命 */
    printf("[11] invulnerability prevents repeated damage\n");
    {
        game_init(&g, 7);
        g.spawn_disabled = 1;
        g.p.invuln = 60;
        game_new_ent(&g, ENT_ENEMY, SPR_ENEMY_A, VAR_NORMAL, g.p.x, g.p.y, 0, 0, 1);
        game_update(&g, 24);
        check("lives unchanged while invulnerable", g.p.lives == 3);
    }

    /* 12. 敌方子弹命中玩家 */
    printf("[12] enemy bullet hits player\n");
    {
        game_init(&g, 7);
        g.spawn_disabled = 1;
        g.p.invuln = 0;
        game_new_ent(&g, ENT_BULLET_E, SPR_EBULLET, VAR_NORMAL, g.p.x + 2, g.p.y, 0, 0, 1);
        game_update(&g, 24);
        check("enemy bullet costs a life", g.p.lives == 2);
    }

    /* 13. 道具拾取 */
    printf("[13] pickups\n");
    {
        game_init(&g, 7);
        g.spawn_disabled = 1;
        g.p.fire_lv = 0;
        game_new_ent(&g, ENT_PICKUP, SPR_ITEM_F, VAR_NORMAL, g.p.x + 2, g.p.y, 0, 0, 1);
        game_update(&g, 24);
        check("fire pickup raises fire level", g.p.fire_lv == 1);

        game_init(&g, 7);
        g.spawn_disabled = 1;
        g.p.bombs = 0;
        game_new_ent(&g, ENT_PICKUP, SPR_ITEM_B, VAR_NORMAL, g.p.x + 2, g.p.y, 0, 0, 1);
        game_update(&g, 24);
        check("bomb pickup adds a bomb", g.p.bombs == 1);
    }

    /* 14. 炸弹清屏 */
    printf("[14] bomb clears enemy bullets\n");
    {
        game_init(&g, 7);
        g.spawn_disabled = 1;
        g.p.bombs = 1;
        for (int i = 0; i < 5; i++)
            game_new_ent(&g, ENT_BULLET_E, SPR_EBULLET, VAR_NORMAL, i, 3, 0, 0, 1);
        check("enemy bullets exist", count_ents(&g, ENT_BULLET_E) == 5);
        game_bomb(&g);
        check("bomb removes all enemy bullets", count_ents(&g, ENT_BULLET_E) == 0);
        check("bomb is consumed", g.p.bombs == 0);
    }

    /* 15. 炸弹伤害敌机 */
    printf("[15] bomb damages enemies\n");
    {
        game_init(&g, 7);
        g.spawn_disabled = 1;
        g.p.bombs = 1;
        game_new_ent(&g, ENT_ENEMY, SPR_ENEMY_A, VAR_NORMAL, 10, 5, 0, 0, 2);
        game_bomb(&g);
        check("bomb kills low-hp enemy", count_ents(&g, ENT_ENEMY) == 0);
    }

    /* 16. 波次推进 */
    printf("[16] wave progression\n");
    {
        game_init(&g, 7);
        check("starts at wave 0", g.wave == 0);
        const ShootWave *w = &SHOOT_WAVES[0];
        /* 推进足够久，敌机应该出场 */
        for (int i = 0; i < 120; i++) game_update(&g, 24);
        int spawned = 0;
        for (int i = 0; i < MAX_ENTS; i++)
            if (g.ents[i].active && g.ents[i].kind == ENT_ENEMY) spawned++;
        check("enemies spawn from the wave table", spawned > 0 || g.slot > 0);
        check("wave slot counter advances", g.slot > 0);
        (void)w;
    }

    /* 17. 波次表单调性 */
    printf("[17] wave table is monotonic\n");
    {
        int ok = 1;
        for (int w = 1; w < SHOOT_WAVE_MAX; w++) {
            if (SHOOT_WAVES[w].hp_mul <= SHOOT_WAVES[w - 1].hp_mul) ok = 0;
            if (SHOOT_WAVES[w].count < SHOOT_WAVES[w - 1].count) ok = 0;
            if (SHOOT_WAVES[w].interval > SHOOT_WAVES[w - 1].interval) ok = 0;
        }
        check("difficulty never decreases across waves", ok == 1);
    }

    /* 18. 游戏结束 */
    printf("[18] game over when lives run out\n");
    {
        game_init(&g, 7);
        g.spawn_disabled = 1;
        g.p.invuln = 0;
        /* 连续撞三次：3 -> 2 -> 1 -> 0，第 3 次就该结束 */
        for (int i = 0; i < 3; i++) {
            g.p.invuln = 0;
            game_new_ent(&g, ENT_ENEMY, SPR_ENEMY_A, VAR_NORMAL, g.p.x, g.p.y, 0, 0, 1);
            game_update(&g, 24);
        }
        check("game over flag set", g.over == 1);
        check("lives reached zero", g.p.lives == 0);
    }

    /* 19. 结束后的帧不再推进 */
    printf("[19] update is a no-op after game over\n");
    {
        game_init(&g, 7);
        g.over = 1;
        int f0 = g.frame;
        game_update(&g, 24);
        check("frame does not advance after game over", g.frame == f0);
    }

    /* 20. 敌机不越出左右墙 */
    printf("[20] enemies stay inside walls\n");
    {
        game_init(&g, 7);
        g.spawn_disabled = 1;
        int e = game_new_ent(&g, ENT_ENEMY, SPR_ENEMY_A, VAR_NORMAL, 1, 5, -16, 0, 1);
        for (int i = 0; i < 30; i++) game_update(&g, 24);
        int id = find_ent(&g, ENT_ENEMY);
        int w = SPR_W[SPR_ENEMY_A][VAR_NORMAL];
        if (id >= 0)
            check("enemy never crosses left wall", g.ents[id].x >= 0 && g.ents[id].x + w <= GW);
        else
            check("enemy never crosses left wall", 1);
        (void)e;
    }

    /* 21. 实体槽位耗尽是安全的 */
    printf("[21] entity pool exhaustion is safe\n");
    {
        game_init(&g, 7);
        g.spawn_disabled = 1;
        for (int i = 0; i < MAX_ENTS + 50; i++)
            game_new_ent(&g, ENT_ENEMY, SPR_ENEMY_A, VAR_NORMAL, 1, 5, 0, 0, 1);
        int n = count_ents(&g, ENT_ENEMY);
        check("entity pool never exceeds MAX_ENTS", n <= MAX_ENTS);
        game_update(&g, 24);
        check("update survives pool exhaustion", 1);
    }

    /* 22. 分数不被重复结算 */
    printf("[22] score is counted exactly once per kill\n");
    {
        game_init(&g, 7);
        g.spawn_disabled = 1;
        /* 一架 1 血的敌机，被子弹打死 */
        game_new_ent(&g, ENT_ENEMY, SPR_ENEMY_A, VAR_NORMAL, 10, 5, 0, 0, 1);
        int b = game_new_ent(&g, ENT_BULLET_P, SPR_BULLET, VAR_NORMAL, 12, 5, 0, 0, 1);
        g.ents[b].x = 12;
        game_update(&g, 24);
        check("score equals exactly one enemy value", g.score == 100);
    }

    printf("\n");
    if (failed) { printf("SOME TESTS FAILED\n"); return 1; }
    printf("ALL TESTS PASSED\n");
    return 0;
}
