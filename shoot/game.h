/* ============================================================
 * game.h —— 打飞机核心逻辑（纯逻辑，不含任何 I/O）
 *
 * 【为什么单独抽成头文件】
 * 俄罗斯方块项目里 selftest.c / sim.c 各自复制了一份游戏逻辑，
 * 于是「游戏改了、测试没改」这种漂移迟早发生。
 * 这里把逻辑抽出来，shoot.c / selftest.c / sim.c 三者共用同一份 ——
 * 这就是「单一真相源」：判定逻辑只有一份实现。
 *
 * 【坐标系】
 *   战场是一个 GW x GH 的字符网格。
 *   实体用「精灵包围盒左上角」(x, y) 定位，单位=字符。
 *   精灵 art 里空格是透明的，所以碰撞判定是逐「实心格」做的，
 *   不是粗略的矩形相交 —— 战机斜角不会误伤。
 *
 * 【速度精度】
 *   实体速度用 1/16 格/帧 的定点数（speed 字段）。
 *   累加器 acc 攒够 16 就整格移动一格，否则不动。
 *   这样「慢速的轰炸机」和「快速的子弹」能用同一套代码表达。
 *
 * 【随机数】
 *   用自带的 LCG 而不是 rand()，因为 sim.c 要跑几百局且必须可复现。
 *   种子存在 Game 里，每局重置。
 * ============================================================ */
#ifndef GAME_H
#define GAME_H

#include "entities.h"

/* ---------- 战场尺寸 ---------- */
#define GW      40        /* 战场宽（字符） */
#define GHMAX   30        /* 战场高上限，实际高度由终端窗口决定 */

/* ---------- 实体种类 ---------- */
#define ENT_ENEMY     0   /* 敌机 */
#define ENT_BULLET_P  1   /* 玩家子弹 */
#define ENT_BULLET_E  2   /* 敌方子弹 */
#define ENT_PICKUP    3   /* 道具 */
#define ENT_EXPLO     4   /* 爆炸特效 */

#define MAX_ENTS      96
#define MAX_BULLETS   48

/* ---------- 玩家状态 ---------- */
typedef struct {
    int x, y;        /* 包围盒左上角 */
    int lives;       /* 剩余命数 */
    int fire_lv;     /* 火力等级 0..2 */
    int bombs;       /* 炸弹数 */
    int cooldown;    /* 射击冷却 */
    int invuln;      /* 无敌剩余帧 */
} Player;

/* ---------- 通用实体 ---------- */
typedef struct {
    int active;
    int kind;        /* ENT_* */
    int x, y;
    int spr, var;    /* 精灵编号 / 变体 */
    int vx, vy;      /* 速度（1/16 格/帧） */
    int hp, maxhp;
    int acc_x, acc_y;/* 定点累加器 */
    int timer;       /* 通用计时器 */
    int score;       /* 击毁得分 */
    int shoot_cd;    /* 开火冷却 */
} Ent;

/* ---------- 游戏总状态 ---------- */
typedef struct {
    Player p;
    Ent ents[MAX_ENTS];
    int score;
    int wave;              /* 当前波次索引（0 起） */
    int slot;              /* 本波已出场到第几个 slot */
    int wave_timer;        /* 本波已过帧数 */
    int frame;
    int over;              /* 游戏结束标记 */
    int boss_hp, boss_max;
    int wave_cleared;      /* 波次清空提示剩余帧 */
    int spawn_disabled;    /* 测试钩子：置 1 则不出怪，用于隔离单点测试 */
    unsigned int rng;      /* 随机数状态 */
} Game;

/* ============================================================
 * 一、随机数（自带 LCG，可复现）
 * ============================================================ */
static inline unsigned int game_rand(Game *g) {
    g->rng = g->rng * 1103515245u + 12345u;
    return (g->rng >> 16) & 0x7fffu;
}

/* ============================================================
 * 二、精灵实心格判定
 * 碰撞的唯一依据。art 里空格透明 = 这一格不算实体。
 * ============================================================ */
static inline int spr_solid(int spr, int var, int cx, int cy) {
    int h = SPR_H[spr][var];
    if (cy < 0 || cy >= h) return 0;
    const char *row = SPR_ART[spr][var][cy];
    if (!row) return 0;
    int w = SPR_W[spr][var];
    if (cx < 0 || cx >= w) return 0;
    return row[cx] != ' ';
}

/* 两个精灵是否重叠（实心格级精确判定）
 * 先用包围盒快速排除，再逐格比对。 */
static inline int spr_overlap(int s1, int v1, int x1, int y1,
                              int s2, int v2, int x2, int y2) {
    int w1 = SPR_W[s1][v1], h1 = SPR_H[s1][v1];
    int w2 = SPR_W[s2][v2], h2 = SPR_H[s2][v2];

    /* 快速排除：包围盒都不相交，直接返回 */
    if (x1 + w1 <= x2 || x2 + w2 <= x1) return 0;
    if (y1 + h1 <= y2 || y2 + h2 <= y1) return 0;

    for (int cy = 0; cy < h1; cy++) {
        for (int cx = 0; cx < w1; cx++) {
            if (!spr_solid(s1, v1, cx, cy)) continue;
            int px = x1 + cx, py = y1 + cy;
            for (int dy = 0; dy < h2; dy++) {
                for (int dx = 0; dx < w2; dx++) {
                    if (!spr_solid(s2, v2, dx, dy)) continue;
                    if (px == x2 + dx && py == y2 + dy) return 1;
                }
            }
        }
    }
    return 0;
}

/* ============================================================
 * 三、实体管理
 * ============================================================ */
static inline void game_clear_ents(Game *g) {
    for (int i = 0; i < MAX_ENTS; i++) g->ents[i].active = 0;
}

/* 申请一个实体槽位；满了返回 -1 */
static inline int game_new_ent(Game *g, int kind, int spr, int var,
                               int x, int y, int vx, int vy, int hp) {
    int slot = -1;
    for (int i = 0; i < MAX_ENTS; i++) {
        if (!g->ents[i].active) { slot = i; break; }
    }
    if (slot < 0) return -1;

    Ent *e = &g->ents[slot];
    e->active = 1;
    e->kind  = kind;
    e->spr   = spr;
    e->var   = var;
    e->x     = x;
    e->y     = y;
    e->vx    = vx;
    e->vy    = vy;
    e->hp    = hp;
    e->maxhp = hp;
    e->acc_x = 0;
    e->acc_y = 0;
    e->timer = 0;
    /* 分值由精灵类型唯一决定（查表），不靠调用方记得填 —— 
     * 否则「手工造一个敌机」时会漏填，击毁就不得分。 */
    e->score = (kind == ENT_ENEMY) ? SPR_SCORE[spr] : 0;
    e->shoot_cd = 0;
    return slot;
}

/* ============================================================
 * 四、玩家操作
 * ============================================================ */
static inline void game_kill_enemy(Game *g, Ent *e);

static inline void game_init(Game *g, unsigned int seed) {
    game_clear_ents(g);
    g->score = 0;
    g->wave = 0;
    g->slot = 0;
    g->wave_timer = 0;
    g->frame = 0;
    g->over = 0;
    g->boss_hp = g->boss_max = 0;
    g->wave_cleared = 0;
    g->spawn_disabled = 0;
    g->rng = seed ? seed : 1u;

    g->p.x = GW / 2 - SPR_W[SPR_PLAYER][VAR_NORMAL] / 2;
    g->p.y = GHMAX - SPR_H[SPR_PLAYER][VAR_NORMAL] - 1;
    g->p.lives = 3;
    g->p.fire_lv = 0;
    g->p.bombs = 1;
    g->p.cooldown = 0;
    g->p.invuln = 60;      /* 开局给一点无敌时间 */
}

/* 玩家左右移动（整格，整数坐标最跟手） */
static inline void game_move(Game *g, int dx) {
    if (g->over) return;
    int w = SPR_W[SPR_PLAYER][VAR_NORMAL];
    int nx = g->p.x + dx;
    if (nx < 0) nx = 0;
    if (nx + w > GW) nx = GW - w;
    g->p.x = nx;
}

/* 玩家开火。火力等级越高，子弹越多、冷却越短。 */
static inline void game_fire(Game *g) {
    if (g->over) return;
    if (g->p.cooldown > 0) return;

    int cx = g->p.x + SPR_W[SPR_PLAYER][VAR_NORMAL] / 2;
    int top = g->p.y - 1;

    /* 等级 0：1 颗；等级 1：2 颗（并行）；等级 2：3 颗（散射）
     * 注意 vy 的符号：负数向上（y 递减）。敌机是 vy 正数向下。 */
    int lv = g->p.fire_lv;
    int shots = lv + 1;
    for (int i = 0; i < shots; i++) {
        int bx = cx + (i - (shots - 1) / 2) * 2;
        if (bx < 0 || bx >= GW) continue;
        game_new_ent(g, ENT_BULLET_P, SPR_BULLET, VAR_NORMAL,
                     bx, top, 0, -16, 1);
    }
    g->p.cooldown = (lv >= 2) ? 7 : (lv >= 1) ? 9 : 12;
}

/* 玩家放炸弹：清屏所有敌弹，对全体敌机造成伤害 */
static inline void game_bomb(Game *g) {
    if (g->over) return;
    if (g->p.bombs <= 0) return;
    g->p.bombs--;

    /* 清掉所有敌方子弹 */
    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &g->ents[i];
        if (e->active && e->kind == ENT_BULLET_E) e->active = 0;
    }
    /* 对全体敌机造成 4 点伤害 —— 走同一个击毁函数，
     * 这样爆炸特效、分数、道具掉落的行为和被子弹打死完全一致。 */
    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &g->ents[i];
        if (e->active && e->kind == ENT_ENEMY) {
            e->hp -= 4;
            if (e->hp <= 0) game_kill_enemy(g, e);
        }
    }
    g->p.invuln = 45;
}

/* ============================================================
 * 五、敌机出怪
 * ============================================================ */
static inline int game_spawn_slot(Game *g, int slot) {
    const ShootWave *w = &SHOOT_WAVES[g->wave];
    if (slot >= w->count) return -1;

    int type = w->slots[slot].type;
    int spr  = type;
    int sw = SPR_W[type][VAR_NORMAL];
    int sh = SPR_H[type][VAR_NORMAL];

    /* 敌机属性全部查表（entities.h 由生成器产出），这里不记数值。
     * BOSS 的血量由波次表单独指定（boss_hp），随波次成长。 */
    int hp   = (type == SPR_BOSS) ? w->boss_hp : SPR_HP[type];
    int vy   = SPR_SPEED[type];
    int vx   = SPR_DRIFT[type];
    int shoot= SPR_SHOOTS[type];

    /* 按波次缩放 */
    hp   = hp   * w->hp_mul  / 100;
    vy   = vy   * w->spd_mul / 100;
    if (vy < 1) vy = 1;

    /* 水平位置随机，但别贴边 */
    int x = (int)(game_rand(g) % (unsigned)(GW - sw));
    if (x < 0) x = 0;
    if (x + sw > GW) x = GW - sw;

    /* BOSS 从顶部正中间进场 */
    if (type == SPR_BOSS) {
        x = (GW - sw) / 2;
        g->boss_hp = g->boss_max = hp;
    }

    int id = game_new_ent(g, ENT_ENEMY, spr, VAR_NORMAL, x, -sh,
                          vx, vy, hp);
    if (id < 0) return -1;
    /* score 由 game_new_ent 查表填好，这里不用再设 */
    g->ents[id].shoot_cd = shoot ? (int)(game_rand(g) % 80u) + 40 : 0;
    return id;
}

/* ============================================================
 * 六、敌机被击毁 → 爆炸 + 得分 + 可能掉落道具
 * ============================================================ */
static inline void game_kill_enemy(Game *g, Ent *e) {
    /* 爆炸特效：用爆炸动画的尺寸，占 3 帧 */
    game_new_ent(g, ENT_EXPLO, SPR_ENEMY_A, VAR_NORMAL, e->x, e->y, 0, 0, 1);

    g->score += e->score;

    /* BOSS 血条更新 */
    if (e->spr == SPR_BOSS) g->boss_hp = 0;

    /* 掉落道具：小概率 */
    unsigned int r = game_rand(g) % 100u;
    if (r < 18) {
        int is_fire = (game_rand(g) % 2u) == 0;
        int px = e->x + SPR_W[e->spr][e->var] / 2 - 1;
        int py = e->y + SPR_H[e->spr][e->var] / 2 - 1;
        if (px < 0) px = 0;
        if (px >= GW) px = GW - 1;
        game_new_ent(g, ENT_PICKUP,
                     is_fire ? SPR_ITEM_F : SPR_ITEM_B, VAR_NORMAL,
                     px, py, 0, 4, 1);
    }

    e->active = 0;
}

/* ============================================================
 * 七、玩家被击中 → 掉一条命 + 短暂无敌
 * ============================================================ */
static inline void game_hit_player(Game *g) {
    if (g->p.invuln > 0) return;
    g->p.lives--;
    g->p.invuln = 90;          /* 掉命后 1.5 秒无敌 */
    g->p.fire_lv = 0;          /* 火力等级归零 */
    /* 命数用完即结束。lives 从 3 递减，掉到 0 时已经没命了，
     * 所以判 <= 0 而不是 < 0 —— 否则玩家会白挨一下才结束。 */
    if (g->p.lives <= 0) {
        g->p.lives = 0;
        g->over = 1;
    }
}

/* ============================================================
 * 八、推进一帧（游戏的心脏）
 * ============================================================ */
static inline void game_update(Game *g, int GH) {
    if (g->over) return;
    g->frame++;

    /* ---- 冷却与无敌计时 ---- */
    if (g->p.cooldown > 0) g->p.cooldown--;
    if (g->p.invuln > 0)   g->p.invuln--;

    /* ---- 波次推进：按 slots[].delay 逐个出场 ----
     * spawn_disabled 是给单元测试用的钩子：置 1 就只推进逻辑不出怪，
     * 这样单点测试（比如「子弹能不能打死敌机」）不会被随机出怪干扰。 */
    const ShootWave *w = &SHOOT_WAVES[g->wave];
    g->wave_timer++;
    if (!g->spawn_disabled) {
        while (g->slot < w->count && g->wave_timer >= w->slots[g->slot].delay) {
            game_spawn_slot(g, g->slot);
            g->slot++;
        }
    }

    /* ---- 实体运动 + 行为 ---- */
    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &g->ents[i];
        if (!e->active) continue;

        e->timer++;

        /* 定点速度积分 */
        e->acc_x += e->vx;
        e->acc_y += e->vy;
        while (e->acc_x >= 16) { e->acc_x -= 16; e->x++; }
        while (e->acc_x <= -16) { e->acc_x += 16; e->x--; }
        while (e->acc_y >= 16) { e->acc_y -= 16; e->y++; }
        while (e->acc_y <= -16) { e->acc_y += 16; e->y--; }

        int sw = SPR_W[e->spr][e->var];
        int sh = SPR_H[e->spr][e->var];

        /* 左右撞墙反弹（敌机不会飞出战场） */
        if (e->kind == ENT_ENEMY) {
            if (e->x <= 0 && e->vx < 0) e->vx = -e->vx;
            if (e->x + sw >= GW && e->vx > 0) e->vx = -e->vx;

            /* BOSS 到顶后横向巡航 */
            if (e->spr == SPR_BOSS && e->y >= 0) {
                if (e->x <= 0) e->vx = 2;
                if (e->x + sw >= GW) e->vx = -2;
            }

            /* 敌机开火 */
            if (e->shoot_cd > 0) e->shoot_cd--;
            if (e->shoot_cd == 0 && e->y > 0 && e->y < GH - 4) {
                if (e->spr == SPR_BOSS) {
                    /* BOSS 散弹：3 发 */
                    int bx = e->x + sw / 2;
                    game_new_ent(g, ENT_BULLET_E, SPR_EBULLET, VAR_NORMAL, bx - 1, e->y + sh, 0, 8, 1);
                    game_new_ent(g, ENT_BULLET_E, SPR_EBULLET, VAR_NORMAL, bx,     e->y + sh, 0, 8, 1);
                    game_new_ent(g, ENT_BULLET_E, SPR_EBULLET, VAR_NORMAL, bx + 1, e->y + sh, 0, 8, 1);
                } else {
                    int bx = e->x + sw / 2;
                    game_new_ent(g, ENT_BULLET_E, SPR_EBULLET, VAR_NORMAL, bx, e->y + sh, 0, 8, 1);
                }
                e->shoot_cd = (e->spr == SPR_BOSS) ? 45 : 110;
            }
        }

        /* 出界清理 */
        if (e->y > GH + 2 || e->y < -sh - 20 || e->x < -4 || e->x > GW + 4) {
            if (e->kind == ENT_ENEMY) {
                /* 敌机飞走了不算被击毁，不给分 */
                e->active = 0;
            } else if (e->kind != ENT_EXPLO) {
                e->active = 0;
            }
        }
    }

    /* ---- 碰撞判定 ---- */
    for (int i = 0; i < MAX_ENTS; i++) {
        Ent *e = &g->ents[i];
        if (!e->active) continue;

        if (e->kind == ENT_BULLET_P) {
            /* 玩家子弹 -> 敌机 */
            for (int j = 0; j < MAX_ENTS; j++) {
                Ent *t = &g->ents[j];
                if (!t->active || t->kind != ENT_ENEMY) continue;
                if (!spr_overlap(e->spr, e->var, e->x, e->y,
                                 t->spr, t->var, t->x, t->y)) continue;

                e->active = 0;
                t->hp--;
                /* 血量掉到一半以下换「受损」形态 */
                if (t->hp * 2 <= t->maxhp) t->var = VAR_HURT;
                if (t->hp <= 0) {
                    /* 分数由 game_kill_enemy 统一结算，这里不要重复加 */
                    game_kill_enemy(g, t);
                }
                break;
            }
        }
        else if (e->kind == ENT_BULLET_E) {
            /* 敌方子弹 -> 玩家 */
            if (spr_overlap(e->spr, e->var, e->x, e->y,
                            SPR_PLAYER, VAR_NORMAL, g->p.x, g->p.y)) {
                e->active = 0;
                game_hit_player(g);
            }
        }
        else if (e->kind == ENT_ENEMY) {
            /* 敌机 -> 玩家 */
            if (spr_overlap(e->spr, e->var, e->x, e->y,
                            SPR_PLAYER, VAR_NORMAL, g->p.x, g->p.y)) {
                game_hit_player(g);
                g->score += e->score;    /* 撞毁也有分 */
                e->active = 0;
            }
        }
        else if (e->kind == ENT_PICKUP) {
            /* 道具 -> 玩家 */
            if (spr_overlap(e->spr, e->var, e->x, e->y,
                            SPR_PLAYER, VAR_NORMAL, g->p.x, g->p.y)) {
                if (e->spr == SPR_ITEM_F) {
                    if (g->p.fire_lv < 2) g->p.fire_lv++;
                } else {
                    if (g->p.bombs < 5) g->p.bombs++;
                }
                e->active = 0;
            }
        }
        else if (e->kind == ENT_EXPLO) {
            /* 爆炸特效播完就消失 */
            if (e->timer >= 6) e->active = 0;
        }
    }

    /* ---- 波次结束判定 ---- */
    if (!g->spawn_disabled && g->slot >= w->count) {
        int any_enemy = 0;
        for (int i = 0; i < MAX_ENTS; i++)
            if (g->ents[i].active && g->ents[i].kind == ENT_ENEMY)
                { any_enemy = 1; break; }
        if (!any_enemy) {
            if (g->wave_cleared == 0) g->wave_cleared = 90;
            else {
                g->wave_cleared--;
                if (g->wave_cleared <= 0) {
                    g->wave++;
                    if (g->wave >= SHOOT_WAVE_MAX) g->wave = SHOOT_WAVE_MAX - 1;
                    g->slot = 0;
                    g->wave_timer = 0;
                    g->wave_cleared = 0;
                }
            }
        }
    }
}

#endif /* GAME_H */
