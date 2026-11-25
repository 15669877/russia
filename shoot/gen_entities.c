/* ============================================================
 * gen_entities.c —— 生成 entities.h（打飞机素材表，纯 C，不用 Python）
 *
 * 【为什么需要这个工具】
 * 打飞机的素材一旦手写进游戏代码，一定会出现这些经典 bug：
 *   - 同一个精灵的两行宽度不一样，画出来左边齐右边歪
 *   - 镜像图忘了改，玩家往左飞时飞机还在朝右
 *   - 敌机"受损形态"和"正常形态"两张图手滑画成一样
 *   - 爆炸动画 3 帧，帧 2 反而比帧 1 大
 *   - 波次表里的血量倍率写成 1000%，一出场就被秒杀
 *
 * 所以这里只手写每种精灵的【标准姿态】，
 * 其余派生形态（镜像 / 受损 / 爆炸三帧）全部由程序算出来，
 * 再由本工具统一输出成 entities.h —— 游戏代码、单元测试、模拟器共用同一份。
 *
 * 【派生规则（都是纯计算，没有第二份手写数据）】
 *   镜像 variant  : 每一行字符串左右反转
 *   受损 variant : (行号 + 列号) % 3 == 0 的实心格替换成 '*'
 *   爆炸 3 帧    : 以中心点为原点，按曼哈顿距离 <= 5 / 4 / 3 逐帧收缩
 *
 * 【波次表】
 * 波次难度也是算出来的，不是手填的：
 *   敌机数量 / 出现间隔 / 血量倍率 / 速度倍率 随波次单调上升，
 *   每 5 波出一次 BOSS。生成器会检查这几条单调性，违反就报错不写文件。
 *
 * 【用法】
 *   gcc gen_entities.c -o gen_entities.exe
 *   ./gen_entities.exe
 *   （会在当前目录生成 entities.h）
 *
 * 【数据结构】
 *   SPR_ART[s][v][row]   精灵 s 的变体 v 的第 row 行字符串（NULL 表示结束）
 *   SPR_W / SPR_H        对应变体的宽 / 高（由程序量出来，不手写）
 *   SHOOT_WAVES[w]       第 w 波的出怪表
 * ============================================================ */
#include <stdio.h>
#include <string.h>

/* ---------- 精灵编号 ---------- */
#define SPR_PLAYER    0
#define SPR_ENEMY_A   1
#define SPR_ENEMY_B   2
#define SPR_ENEMY_C   3
#define SPR_BOSS      4
#define SPR_BULLET    5
#define SPR_EBULLET   6
#define SPR_ITEM_F    7
#define SPR_ITEM_B    8
#define SPR_COUNT     9

/* ---------- 变体编号 ---------- */
#define VAR_NORMAL 0   /* 标准姿态 */
#define VAR_MIRROR 1   /* 左右镜像 */
#define VAR_HURT   2   /* 受损（掉漆） */
#define SPR_VARIANTS 3

#define SPR_MAXH   8    /* 精灵最大行数 */
#define SPR_MAXW  32    /* 精灵最大宽度（生成时的中间缓冲） */

#define EXPLO_FRAMES 3
#define EXPLO_MAXH   6

#define SHOOT_WAVE_MAX   20
#define SHOOT_WAVE_SLOTS 32

/* ============================================================
 * 一、手写的标准姿态（全部素材只在这里写一遍）
 * ============================================================ */

/* 玩家战机 5x3：机头、机翼、机身 */
static const char *BASE_ART[SPR_COUNT][SPR_MAXH] = {
    { "  ^  ", " /|\\ ", "/___\\", NULL },                     /* 0 PLAYER   5x3 */
    { "ooooo", "o___o", NULL, NULL },                         /* 1 ENEMY_A  5x2 */
    { "  v  ", " (o) ", "/|_|\\", NULL },                     /* 2 ENEMY_B  5x3 */
    { " ooooo ", "ooooooo", "\\_____/", "  ###  ", NULL },   /* 3 ENEMY_C  7x4 */
    { "  OOO OOO  ", " OOOOOOOOO ", "OO OOOO  OO",             /* 4 BOSS    11x6 */
      "OOO     OOO", " O       O ", "  OOO OOO  ", NULL },
    { "|", "|", NULL, NULL },                                 /* 5 BULLET   1x2 */
    { "o", "o", NULL, NULL },                                 /* 6 EBULLET  1x2 */
    { " F ", "FFF", " F ", NULL, NULL },                      /* 7 ITEM_F   3x3 */
    { " B ", "BBB", " B ", NULL, NULL },                      /* 8 ITEM_B   3x3 */
};

/* 爆炸的画布尺寸参照（实际图形由曼哈顿距离算法生成，不直接用这张表） */
static const char *EXPLO_BASE[EXPLO_MAXH] = {
    "  ***  ",
    " ****** ",
    "*******",
    " ****** ",
    "  ***  ",
    NULL
};

/* ============================================================
 * 二、生成用的中间缓冲
 * ============================================================ */
static char art[SPR_COUNT][SPR_VARIANTS][SPR_MAXH][SPR_MAXW];
static int  aw  [SPR_COUNT][SPR_VARIANTS];
static int  ah  [SPR_COUNT][SPR_VARIANTS];

static char ex_art[EXPLO_FRAMES][EXPLO_MAXH][SPR_MAXW];
static int  ex_w[EXPLO_FRAMES], ex_h[EXPLO_FRAMES];

/* 敌机属性（唯一真相源，游戏代码只查表）
 * 索引和 SPR_* 一一对应；非敌机精灵填 0。
 *   HP      基础血量
 *   SPEED   下落速度，1/16 格/帧
 *   SCORE   击毁得分
 *   SHOOTS  是否开火
 *   DRIFT   横向漂移，1/16 格/帧 */
static const int ENEMY_HP[SPR_COUNT]    = { 0, 1, 3, 6, 40, 0, 0, 0, 0 };
static const int ENEMY_SPEED[SPR_COUNT] = { 0, 6, 4, 3,  1, 0, 0, 0, 0 };
static const int ENEMY_SCORE[SPR_COUNT] = { 0, 100, 300, 500, 5000, 0, 0, 0, 0 };
static const int ENEMY_SHOOTS[SPR_COUNT]= { 0, 0, 1, 1, 1, 0, 0, 0, 0 };
static const int ENEMY_DRIFT[SPR_COUNT]= { 0, 0, 3, 0, 2, 0, 0, 0, 0 };

/* 波次表 */
typedef struct { unsigned short delay; unsigned char type; } Spawn;
typedef struct {
    int  count;        /* 本波敌人总数 */
    int  interval;     /* 出怪间隔（帧） */
    int  hp_mul;       /* 血量倍率（百分比） */
    int  spd_mul;      /* 速度倍率（百分比） */
    int  boss;         /* 1 = 本波有 BOSS */
    int  boss_hp;      /* BOSS 血量 */
    Spawn slots[SHOOT_WAVE_SLOTS];
} Wave;

static Wave waves[SHOOT_WAVE_MAX];

/* ============================================================
 * 三、派生算法
 * ============================================================ */

/* ---------- 转义 ----------
 * 素材里含有 '\' 和 '"' 这类字符，直接写进 C 字符串字面量会被当成转义序列：
 *   "/___\"  ->  "\___" 里的 \ 被吃掉，后面字符串提前结束，整份头文件编译不过。
 * 所以输出到 .h 时必须转义。
 * 这里做「转义 -> 反转义」的往返自检，保证写出去的和读回来的完全一致。
 */
#define ESC_MAX (SPR_MAXW * 4 + 8)

/* 把 src 转义后写进 dst，返回写入长度（不含结尾 '\0'） */
static int esc_write(char *dst, const char *src) {
    int n = 0;
    for (const char *p = src; *p; p++) {
        if (*p == '\\' || *p == '"') { dst[n++] = '\\'; dst[n++] = *p; }
        else dst[n++] = *p;
    }
    dst[n] = '\0';
    return n;
}

/* 把转义后的串解回来（用于往返自检） */
static int esc_read(char *dst, const char *src) {
    int n = 0;
    for (const char *p = src; *p; p++) {
        if (*p == '\\' && (*(p + 1) == '\\' || *(p + 1) == '"')) {
            dst[n++] = *(++p);
        } else {
            dst[n++] = *p;
        }
    }
    dst[n] = '\0';
    return n;
}

/* 把字符串写进定长缓冲（补 '\0'） */
static void copy_str(char *dst, const char *src) {
    strncpy(dst, src, SPR_MAXW - 1);
    dst[SPR_MAXW - 1] = '\0';
}

/* 镜像：把一行左右反转 */
static void mirror_row(char *dst, const char *src) {
    int n = (int)strlen(src);
    for (int i = 0; i < n; i++) dst[i] = src[n - 1 - i];
    dst[n] = '\0';
}

/* 受损：按 (行+列)%3 把实心格打成 '*'，空格保持透明 */
static void hurt_row(char *dst, const char *src, int row) {
    int n = (int)strlen(src);
    for (int i = 0; i < n; i++) {
        if (src[i] == ' ') dst[i] = ' ';
        else if ((row + i) % 3 == 0) dst[i] = '*';
        else dst[i] = src[i];
    }
    dst[n] = '\0';
}

/* ============================================================
 * 四、生成精灵表
 * ============================================================ */
static int build_sprites(void) {
    int bad = 0;

    for (int s = 0; s < SPR_COUNT; s++) {
        /* --- 量出标准姿态的宽高，并检查每行是否等宽 --- */
        int rows = 0;
        while (rows < SPR_MAXH && BASE_ART[s][rows]) rows++;
        if (rows == 0) {
            printf("ERROR: sprite %d has no art\n", s);
            bad = 1;
            continue;
        }
        int w = (int)strlen(BASE_ART[s][0]);
        if (w <= 0 || w >= SPR_MAXW) {
            printf("ERROR: sprite %d has bad width %d\n", s, w);
            bad = 1;
            continue;
        }
        for (int r = 1; r < rows; r++) {
            int lw = (int)strlen(BASE_ART[s][r]);
            if (lw != w) {
                printf("ERROR: sprite %d row %d width %d != %d (art is ragged)\n",
                       s, r, lw, w);
                bad = 1;
            }
        }

        /* --- variant 0：标准姿态 --- */
        aw[s][VAR_NORMAL] = w;
        ah[s][VAR_NORMAL] = rows;
        for (int r = 0; r < rows; r++)
            copy_str(art[s][VAR_NORMAL][r], BASE_ART[s][r]);

        /* --- variant 1：镜像（程序反转，不手写第二份） --- */
        aw[s][VAR_MIRROR] = w;
        ah[s][VAR_MIRROR] = rows;
        for (int r = 0; r < rows; r++)
            mirror_row(art[s][VAR_MIRROR][r], art[s][VAR_NORMAL][r]);

        /* --- variant 2：受损（程序打点，不手写第三份） --- */
        aw[s][VAR_HURT] = w;
        ah[s][VAR_HURT] = rows;
        for (int r = 0; r < rows; r++)
            hurt_row(art[s][VAR_HURT][r], art[s][VAR_NORMAL][r], r);
    }

    /* --- 爆炸 3 帧 ---
     * 做法：以中心为原点，用菱形 |dx|+|dy| <= r 决定哪些格子保留，
     *       半径逐帧递减（3 -> 2 -> 1）。
     *       这样得到的是「实心菱形 -> 小菱形 -> 十字」的清晰收缩，
     *       而不是「圆角矩形掉了4个角」这种肉眼看不出的变化。
     * 半径差取得大一些，动画才有「炸开又收拢」的观感。 */
    {
        int rows = 0;
        while (rows < EXPLO_MAXH && EXPLO_BASE[rows]) rows++;
        int w = (int)strlen(EXPLO_BASE[0]);
        int cx = w / 2, cy = rows / 2;

        for (int f = 0; f < EXPLO_FRAMES; f++) {
            int radius = 3 - f;                 /* 3 -> 2 -> 1 */
            int cnt = 0;
            for (int r = 0; r < rows; r++) {
                for (int c = 0; c < w; c++) {
                    int d = (c > cx ? c - cx : cx - c) +
                            (r > cy ? r - cy : cy - r);
                    int keep = (d <= radius);
                    /* 字符随帧变化，形成「亮 -> 暗 -> 残烬」的层次 */
                    char ch = keep ? (f == 0 ? '*' : (f == 1 ? '+' : '.')) : ' ';
                    ex_art[f][r][c] = ch;
                    if (keep) cnt++;
                }
                ex_art[f][r][w] = '\0';
            }
            ex_w[f] = w;
            ex_h[f] = rows;
            if (cnt == 0) {
                printf("ERROR: explosion frame %d is empty\n", f);
                bad = 1;
            }
        }
        /* 帧必须逐帧变小，否则播放出来是「炸开又缩回」 */
        for (int f = 1; f < EXPLO_FRAMES; f++) {
            int a = 0, b = 0;
            for (int r = 0; r < ex_h[f - 1]; r++)
                for (int c = 0; c < ex_w[f - 1]; c++)
                    if (ex_art[f - 1][r][c] != ' ') a++;
            for (int r = 0; r < ex_h[f]; r++)
                for (int c = 0; c < ex_w[f]; c++)
                    if (ex_art[f][r][c] != ' ') b++;
            if (b > a) {
                printf("ERROR: explosion frame %d is not smaller than frame %d\n",
                       f, f - 1);
                bad = 1;
            }
        }
    }

    return bad;
}

/* ============================================================
 * 五、生成波次表
 * ============================================================ */
static int build_waves(void) {
    int bad = 0;

    for (int w = 0; w < SHOOT_WAVE_MAX; w++) {
        Wave *wv = &waves[w];

        /* 本波敌人总数：严格单调递增（自检会验证）。
         * 注意必须先定总数、再分配类型，否则会出现
         * 「BOSS 波挤掉常规敌机 → 下一波总数反而变少」的反常。 */
        int total = 4 + w;

        /* 分配到各机型：越强的机型越晚解锁，占位也越少 */
        int nB = (w >= 1) ? w : 0;            if (nB > 8) nB = 8;   /* 战斗机 */
        int nC = (w >= 3) ? (w - 3) / 2 : 0;  if (nC > 4) nC = 4;   /* 轰炸机 */
        wv->boss = ((w + 1) % 5 == 0) ? 1 : 0;                   /* 每 5 波一 BOSS */

        /* 小飞机兜底，至少 1 架 */
        int nA = total - nB - nC - wv->boss;
        if (nA < 1) nA = 1;

        wv->interval = 40 - w * 2; if (wv->interval < 12) wv->interval = 12;
        wv->hp_mul   = 100 + w * 10;
        wv->spd_mul  = 100 + w * 5;
        wv->boss_hp  = 40 + w * 25;

        int n = 0;
        /* 小飞机先来，间隔均匀 */
        for (int i = 0; i < nA; i++) {
            wv->slots[n].type  = SPR_ENEMY_A;
            wv->slots[n].delay = (unsigned short)(i * wv->interval);
            n++;
        }
        /* 战斗机稍晚入场，间隔更大 */
        for (int i = 0; i < nB; i++) {
            wv->slots[n].type  = SPR_ENEMY_B;
            wv->slots[n].delay = (unsigned short)(20 + i * (wv->interval + 10));
            n++;
        }
        /* 轰炸机最后压上来 */
        for (int i = 0; i < nC; i++) {
            wv->slots[n].type  = SPR_ENEMY_C;
            wv->slots[n].delay = (unsigned short)(40 + i * (wv->interval + 20));
            n++;
        }
        /* BOSS 压在最后 */
        if (wv->boss) {
            wv->slots[n].type  = SPR_BOSS;
            wv->slots[n].delay = (unsigned short)(60 + n * wv->interval);
            n++;
        }
        wv->count = n;

        if (n > SHOOT_WAVE_SLOTS) {
            printf("ERROR: wave %d has %d slots (max %d)\n",
                   w, n, SHOOT_WAVE_SLOTS);
            bad = 1;
        }
        if (nA < 1) {
            printf("ERROR: wave %d has no scout planes\n", w);
            bad = 1;
        }

        /* 出怪必须按时间排好序，游戏里直接顺序取就行 */
        for (int i = 1; i < n; i++) {
            Spawn key = wv->slots[i];
            int j = i - 1;
            while (j >= 0 && wv->slots[j].delay > key.delay) {
                wv->slots[j + 1] = wv->slots[j];
                j--;
            }
            wv->slots[j + 1] = key;
        }
    }

    /* 难度必须单调上升：血量倍率、速度倍率、敌机数量都不能往回掉 */
    for (int w = 1; w < SHOOT_WAVE_MAX; w++) {
        if (waves[w].hp_mul <= waves[w - 1].hp_mul) {
            printf("ERROR: wave %d hp_mul not increasing\n", w);
            bad = 1;
        }
        if (waves[w].spd_mul < waves[w - 1].spd_mul) {
            printf("ERROR: wave %d spd_mul decreasing\n", w);
            bad = 1;
        }
        if (waves[w].count < waves[w - 1].count) {
            printf("ERROR: wave %d count decreasing\n", w);
            bad = 1;
        }
        if (waves[w].interval > waves[w - 1].interval) {
            printf("ERROR: wave %d interval increasing\n", w);
            bad = 1;
        }
    }

    return bad;
}

/* ============================================================
 * 六、输出 entities.h
 * ============================================================ */
static void write_sprites(FILE *f) {
    char esc[ESC_MAX];

    fprintf(f,
"#define SPR_PLAYER    0\n"
"#define SPR_ENEMY_A   1\n"
"#define SPR_ENEMY_B   2\n"
"#define SPR_ENEMY_C   3\n"
"#define SPR_BOSS      4\n"
"#define SPR_BULLET    5\n"
"#define SPR_EBULLET   6\n"
"#define SPR_ITEM_F    7\n"
"#define SPR_ITEM_B    8\n"
"#define SPR_COUNT     %d\n\n", SPR_COUNT);

    fprintf(f,
"#define VAR_NORMAL 0   /* 标准姿态 */\n"
"#define VAR_MIRROR 1   /* 左右镜像（程序反转得到） */\n"
"#define VAR_HURT   2   /* 受损形态（程序打点得到） */\n"
"#define SPR_VARIANTS %d\n"
"#define SPR_MAXH     %d\n\n", SPR_VARIANTS, SPR_MAXH);

    fprintf(f, "static const char *const SPR_ART[SPR_COUNT][SPR_VARIANTS][SPR_MAXH] = {\n");
    for (int s = 0; s < SPR_COUNT; s++) {
        fprintf(f, "    {   /* %d */\n", s);
        for (int v = 0; v < SPR_VARIANTS; v++) {
            fprintf(f, "        {   /* var %d */\n", v);
            for (int r = 0; r < SPR_MAXH; r++) {
                if (r >= ah[s][v]) { fprintf(f, "            NULL,\n"); continue; }
                esc_write(esc, art[s][v][r]);
                fprintf(f, "            \"%s\",\n", esc);
            }
            fprintf(f, "        },\n");
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "};\n\n");

    fprintf(f, "static const int SPR_W[SPR_COUNT][SPR_VARIANTS] = {\n");
    for (int s = 0; s < SPR_COUNT; s++) {
        fprintf(f, "    {");
        for (int v = 0; v < SPR_VARIANTS; v++)
            fprintf(f, "%s%d", v ? ", " : "", aw[s][v]);
        fprintf(f, " },\n");
    }
    fprintf(f, "};\n\n");

    fprintf(f, "static const int SPR_H[SPR_COUNT][SPR_VARIANTS] = {\n");
    for (int s = 0; s < SPR_COUNT; s++) {
        fprintf(f, "    {");
        for (int v = 0; v < SPR_VARIANTS; v++)
            fprintf(f, "%s%d", v ? ", " : "", ah[s][v]);
        fprintf(f, " },\n");
    }
    fprintf(f, "};\n\n");

    /* ---- 敌机属性表 ----
     * 分值、血量、速度、开火这些「数值型事实」也放在生成器里，
     * 避免散落在游戏代码的 switch 里被漏抄或抄错。
     * 游戏代码只查表，不再自己记数值 —— 分值由精灵类型唯一决定。 */
    fprintf(f,
"/* 敌机属性表（由生成器统一产出，游戏代码只查表）：\n"
" *   HP      基础血量（再乘波次的 hp_mul）\n"
" *   SPEED   基础下落速度，单位 1/16 格/帧（再乘波次的 spd_mul）\n"
" *   SCORE   击毁得分\n"
" *   SHOOTS  是否开火：0 不开火，1 开火\n"
" *   DRIFT   横向漂移速度，单位 1/16 格/帧\n"
" * 玩家 / 子弹 / 道具这些非敌机精灵填 0。 */\n"
"static const int SPR_HP[SPR_COUNT]    = {");
    for (int s = 0; s < SPR_COUNT; s++) fprintf(f, "%s%d", s ? ", " : "", ENEMY_HP[s]);
    fprintf(f, "};\n\n");

    fprintf(f, "static const int SPR_SPEED[SPR_COUNT] = {");
    for (int s = 0; s < SPR_COUNT; s++) fprintf(f, "%s%d", s ? ", " : "", ENEMY_SPEED[s]);
    fprintf(f, "};\n\n");

    fprintf(f, "static const int SPR_SCORE[SPR_COUNT] = {");
    for (int s = 0; s < SPR_COUNT; s++) fprintf(f, "%s%d", s ? ", " : "", ENEMY_SCORE[s]);
    fprintf(f, "};\n\n");

    fprintf(f, "static const int SPR_SHOOTS[SPR_COUNT] = {");
    for (int s = 0; s < SPR_COUNT; s++) fprintf(f, "%s%d", s ? ", " : "", ENEMY_SHOOTS[s]);
    fprintf(f, "};\n\n");

    fprintf(f, "static const int SPR_DRIFT[SPR_COUNT] = {");
    for (int s = 0; s < SPR_COUNT; s++) fprintf(f, "%s%d", s ? ", " : "", ENEMY_DRIFT[s]);
    fprintf(f, "};\n\n");

}

/* 转义往返自检：写出去的字符串解回来必须和原始素材一模一样。
 * 这一步能在写文件之前就抓出「素材含反斜杠却没转义」这类致命问题。 */
static int check_escape_roundtrip(void) {
    char esc[ESC_MAX], back[ESC_MAX];
    int bad = 0;

    for (int s = 0; s < SPR_COUNT; s++)
        for (int v = 0; v < SPR_VARIANTS; v++)
            for (int r = 0; r < ah[s][v]; r++) {
                esc_write(esc, art[s][v][r]);
                esc_read(back, esc);
                if (strcmp(back, art[s][v][r]) != 0) {
                    printf("ERROR: escape round-trip failed at sprite %d var %d row %d\n",
                           s, v, r);
                    bad = 1;
                }
            }

    for (int fr = 0; fr < EXPLO_FRAMES; fr++)
        for (int r = 0; r < ex_h[fr]; r++) {
            esc_write(esc, ex_art[fr][r]);
            esc_read(back, esc);
            if (strcmp(back, ex_art[fr][r]) != 0) {
                printf("ERROR: escape round-trip failed at explosion frame %d row %d\n",
                       fr, r);
                bad = 1;
            }
        }

    return bad;
}

static void write_explosion(FILE *f) {
    char esc[ESC_MAX];

    fprintf(f, "#define EXPLO_FRAMES %d\n"
               "#define EXPLO_MAXH   %d\n\n", EXPLO_FRAMES, EXPLO_MAXH);

    fprintf(f, "static const char *const EXPLO_ART[EXPLO_FRAMES][EXPLO_MAXH] = {\n");
    for (int fr = 0; fr < EXPLO_FRAMES; fr++) {
        fprintf(f, "    {   /* frame %d */\n", fr);
        for (int r = 0; r < EXPLO_MAXH; r++) {
            if (r >= ex_h[fr]) { fprintf(f, "        NULL,\n"); continue; }
            esc_write(esc, ex_art[fr][r]);
            fprintf(f, "        \"%s\",\n", esc);
        }
        fprintf(f, "    },\n");
    }
    fprintf(f, "};\n\n");

    fprintf(f, "static const int EXPLO_W[EXPLO_FRAMES] = {");
    for (int i = 0; i < EXPLO_FRAMES; i++) fprintf(f, "%s%d", i ? ", " : "", ex_w[i]);
    fprintf(f, "};\n\n");

    fprintf(f, "static const int EXPLO_H[EXPLO_FRAMES] = {");
    for (int i = 0; i < EXPLO_FRAMES; i++) fprintf(f, "%s%d", i ? ", " : "", ex_h[i]);
    fprintf(f, "};\n\n");
}

static void write_waves(FILE *f) {
    fprintf(f,
"#define SHOOT_WAVE_MAX   %d\n"
"#define SHOOT_WAVE_SLOTS %d\n\n", SHOOT_WAVE_MAX, SHOOT_WAVE_SLOTS);

    fprintf(f,
"typedef struct {\n"
"    unsigned short delay;   /* 相对本波开始的帧号 */\n"
"    unsigned char  type;    /* 敌机精灵编号 */\n"
"} ShootSpawn;\n\n");

    fprintf(f,
"typedef struct {\n"
"    int  count;             /* 本波敌人总数 */\n"
"    int  interval;          /* 出怪间隔（帧） */\n"
"    int  hp_mul;            /* 血量倍率（百分比） */\n"
"    int  spd_mul;           /* 速度倍率（百分比） */\n"
"    int  boss;              /* 1 = 本波有 BOSS */\n"
"    int  boss_hp;           /* BOSS 血量 */\n"
"    ShootSpawn slots[SHOOT_WAVE_SLOTS];\n"
"} ShootWave;\n\n");

    fprintf(f, "static const ShootWave SHOOT_WAVES[SHOOT_WAVE_MAX] = {\n");
    for (int w = 0; w < SHOOT_WAVE_MAX; w++) {
        Wave *wv = &waves[w];
        fprintf(f, "    { %d, %d, %d, %d, %d, %d, {",   /* count..boss_hp */
                wv->count, wv->interval, wv->hp_mul, wv->spd_mul,
                wv->boss, wv->boss_hp);
        for (int i = 0; i < SHOOT_WAVE_SLOTS; i++) {
            if (i >= wv->count) break;
            if (i) fprintf(f, ", ");
            fprintf(f, "{ %d, %d }", wv->slots[i].delay, wv->slots[i].type);
        }
        fprintf(f, " } },\n");
    }
    fprintf(f, "};\n\n");
}

/* ============================================================
 * 七、main
 * ============================================================ */
int main(void) {
    printf("=== 生成 entities.h ===\n\n");

    int bad = 0;
    bad |= build_sprites();
    bad |= build_waves();
    bad |= check_escape_roundtrip();

    /* ---- 自检：精灵表必须合法 ---- */
    for (int s = 0; s < SPR_COUNT; s++) {
        for (int v = 0; v < SPR_VARIANTS; v++) {
            if (aw[s][v] <= 0 || ah[s][v] <= 0) {
                printf("ERROR: sprite %d var %d has empty bounding box\n", s, v);
                bad = 1;
            }
            /* 镜像后的宽度必须和原来一致 */
            if (v == VAR_MIRROR && aw[s][v] != aw[s][VAR_NORMAL]) {
                printf("ERROR: sprite %d mirror width mismatch\n", s);
                bad = 1;
            }
            /* 受损形态不能和标准形态完全一样 */
            if (v == VAR_HURT) {
                int same = 1;
                for (int r = 0; r < ah[s][v]; r++)
                    if (strcmp(art[s][v][r], art[s][VAR_NORMAL][r]) != 0) same = 0;
                if (same && aw[s][v] > 1) {
                    printf("ERROR: sprite %d hurt variant identical to normal\n", s);
                    bad = 1;
                }
            }
        }
    }

    /* ---- 自检：敌机属性必须合法 ---- */
    for (int s = SPR_ENEMY_A; s <= SPR_BOSS; s++) {
        if (ENEMY_HP[s] <= 0) {
            printf("ERROR: enemy sprite %d has hp %d (must be > 0)\n", s, ENEMY_HP[s]);
            bad = 1;
        }
        if (ENEMY_SPEED[s] <= 0) {
            printf("ERROR: enemy sprite %d has speed %d (must be > 0)\n", s, ENEMY_SPEED[s]);
            bad = 1;
        }
        if (ENEMY_SCORE[s] <= 0) {
            printf("ERROR: enemy sprite %d has score %d (must be > 0)\n", s, ENEMY_SCORE[s]);
            bad = 1;
        }
    }
    /* 越强的敌机应该血更厚、值更多分 */
    if (!(ENEMY_HP[SPR_BOSS] > ENEMY_HP[SPR_ENEMY_C] &&
          ENEMY_HP[SPR_ENEMY_C] > ENEMY_HP[SPR_ENEMY_B] &&
          ENEMY_HP[SPR_ENEMY_B] > ENEMY_HP[SPR_ENEMY_A])) {
        printf("ERROR: enemy hp is not strictly increasing with strength\n");
        bad = 1;
    }
    if (!(ENEMY_SCORE[SPR_BOSS] > ENEMY_SCORE[SPR_ENEMY_C] &&
          ENEMY_SCORE[SPR_ENEMY_C] > ENEMY_SCORE[SPR_ENEMY_B] &&
          ENEMY_SCORE[SPR_ENEMY_B] > ENEMY_SCORE[SPR_ENEMY_A])) {
        printf("ERROR: enemy score is not strictly increasing with strength\n");
        bad = 1;
    }

    if (bad) {
        printf("\nFAILED: entity table is invalid, entities.h not written.\n");
        return 1;
    }

    /* ---- 输出 entities.h ---- */
    FILE *f = fopen("entities.h", "w");
    if (!f) { printf("ERROR: cannot open entities.h for writing\n"); return 1; }

    fprintf(f,
"/* ============================================================\n"
" * entities.h —— 由 gen_entities.c 自动生成，请勿手改！\n"
" *\n"
" * 【为什么用工具生成】\n"
" * 手写敌机素材极易出错：\n"
" *   - 同一精灵两行宽度不一致，画出来歪掉\n"
" *   - 镜像图忘了改，往左飞时飞机还朝右\n"
" *   - 爆炸动画第 3 帧比第 2 帧还大\n"
" *   - 波次表血量倍率写成 1000%%，一出场就被秒杀\n"
" * 所以：素材只写【标准姿态】一份，镜像 / 受损 / 爆炸帧全部由程序派生，\n"
" *       波次难度也由公式算出并检查单调性。\n"
" *\n"
" * 【数据结构】\n"
" *   SPR_ART[s][v][row]  精灵 s 的变体 v 的第 row 行；NULL 表示行结束\n"
" *   SPR_W / SPR_H       对应变体的宽 / 高（程序量出，不手写）\n"
" *   EXPLO_ART[f][row]   爆炸第 f 帧\n"
" *   SHOOT_WAVES[w]      第 w 波的出怪表（已按时间排好序）\n"
" * ============================================================ */\n"
"#ifndef ENTITIES_H\n"
"#define ENTITIES_H\n\n");

    write_sprites(f);
    write_explosion(f);
    write_waves(f);

    fprintf(f, "#endif /* ENTITIES_H */\n");
    fclose(f);

    printf("已生成 entities.h\n");
    printf("自检通过：%d 精灵 x %d 变体 / 爆炸 %d 帧逐帧收缩 / 波次难度单调\n\n",
           SPR_COUNT, SPR_VARIANTS, EXPLO_FRAMES);

    /* ---- 打印 ASCII 图，方便人工核对 ---- */
    for (int s = 0; s < SPR_COUNT; s++) {
        printf("sprite %d (normal / mirror / hurt):\n", s);
        for (int v = 0; v < SPR_VARIANTS; v++) {
            printf("  var %d\n", v);
            for (int r = 0; r < ah[s][v]; r++)
                printf("    |%s|\n", art[s][v][r]);
        }
    }

    printf("\nexplosion frames:\n");
    for (int fr = 0; fr < EXPLO_FRAMES; fr++) {
        printf("  frame %d\n", fr);
        for (int r = 0; r < ex_h[fr]; r++)
            printf("    |%s|\n", ex_art[fr][r]);
    }

    printf("\nwave table:\n");
    for (int w = 0; w < SHOOT_WAVE_MAX; w++) {
        Wave *wv = &waves[w];
        printf("  wave %2d: count %2d  interval %2d  hp %3d%%  spd %3d%%  boss %d (hp %d)\n",
               w + 1, wv->count, wv->interval, wv->hp_mul, wv->spd_mul,
               wv->boss, wv->boss_hp);
    }

    return 0;
}
