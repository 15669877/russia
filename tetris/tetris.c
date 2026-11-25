/* ============================================================
 * tetris.c —— 极简俄罗斯方块（纯终端，无任何 UI 装饰）
 *
 * 模块划分（每块都能单独拆出来学）：
 *   1) 图形数据 cell()    —— 7 种方块 x 4 个旋转态，位掩码存储
 *   2) 碰撞检查 fits()    —— 唯一真理来源：能不能放这儿
 *   3) 操作 move/rotate   —— 左右、上下、旋转、软降、硬降
 *   4) 下坠 gravity()     —— 按 tick 计时自动下落，速度随分数变快
 *   5) 锁定 lock()        —— 落地写进棋盘
 *   6) 消行 clear_lines() —— 满行消除 + 计分
 *   7) 生成 new_piece()   —— 新方块 + 游戏结束判定
 *   8) 显示端 show()      —— 只重画变化的行，画面完全不闪
 *
 * 编译：
 *   Visual Studio：打开 tetris.sln，Ctrl+F5
 *   MinGW       ：gcc tetris.c -o tetris.exe
 *
 * 操作：←/→ 或 a/d 移动   ↑/w 旋转   ↓/s 软降   空格 硬降   q 退出
 * ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "key.h"
#include "render.h"

/* ---------- 1) 图形数据（见 shapes.h，由 gen_shapes.c 生成） ---------- */
#include "shapes.h"

#define W 10        /* 场地宽（格） */
#define HMAX 30     /* 场地高上限（格），实际高度由终端窗口决定 */

static int H = 20;  /* 实际场地高，main() 里按窗口高度算出来 */

/* ---------- 全局状态 ---------- */
static int board[HMAX][W];   /* 0 空，1..7 已锁定方块（值 = 形状 + 1） */
static int type, rot, x, y;    /* 活动方块：形状 / 旋转态 / 包围盒左上角 */
static int score, lines, over, tick, drop_interval;

/* 取图形在 (cx,cy) 处的格子是否实心。cx/cy 是包围盒内的相对坐标。 */
static int cell(int t, int r, int cx, int cy) {
    int w = SHAPE_SIZE[t][r][0];
    if (cy >= SHAPE_SIZE[t][r][1] || cx >= w) return 0;
    return (SHAPE[t][r][cy] >> (w - 1 - cx)) & 1;
}

/* ---------- 2) 碰撞检查：整个游戏唯一的判定入口 ----------
 * 每个实心格子都必须：x 在 [0,W) 内、y 未触底、目标位置没有已锁定方块。
 */
static int fits(int nt, int nr, int nx, int ny) {
    for (int cy = 0; cy < SHAPE_SIZE[nt][nr][1]; cy++)
        for (int cx = 0; cx < SHAPE_SIZE[nt][nr][0]; cx++) {
            if (!cell(nt, nr, cx, cy)) continue;
            int px = nx + cx, py = ny + cy;
            if (px < 0 || px >= W) return 0;         /* 左右墙 */
            if (py >= H)          return 0;         /* 地板 */
            if (py >= 0 && board[py][px]) return 0; /* 撞到已有方块 */
        }
    return 1;
}

/* 前置声明：下面几个函数互相调用 */
static void lock(void);
static void clear_lines(void);
static void new_piece(void);

/* ---------- 3) 操作 ---------- */
/* 返回 1 表示移动成功，0 表示被挡住 */
static int move(int dx, int dy) {
    if (fits(type, rot, x + dx, y + dy)) { x += dx; y += dy; return 1; }
    return 0;
}

/* 旋转：先让「新旧包围盒的中心」尽量重合（手感最自然），
 * 靠墙放不下时再依次尝试左右/上下的微调。 */
static void rotate(int d) {
    int nr = (rot + d + 4) % 4;
    int ow = SHAPE_SIZE[type][rot][0], oh = SHAPE_SIZE[type][rot][1];
    int nw = SHAPE_SIZE[type][nr][0], nh = SHAPE_SIZE[type][nr][1];

    int cx = x + (ow - 1) / 2;      /* 旧中心 */
    int cy = y + (oh - 1) / 2;
    int bx = cx - (nw - 1) / 2;      /* 新中心对齐后的位置 */
    int by = cy - (nh - 1) / 2;

    int cand[6][2] = { {bx,by}, {bx-1,by}, {bx+1,by},
                        {bx-2,by}, {bx+2,by}, {bx,by-1} };
    for (int i = 0; i < 6; i++)
        if (fits(type, nr, cand[i][0], cand[i][1])) {
            x = cand[i][0]; y = cand[i][1]; rot = nr; return;
        }
}

static void hard_drop(void) {
    int n = 0;
    while (fits(type, rot, x, y + 1)) { y++; n++; }
    score += n * 2;
    lock();
}

/* ---------- 5) 锁定：把活动方块写进棋盘 ---------- */
static void lock(void) {
    for (int cy = 0; cy < SHAPE_SIZE[type][rot][1]; cy++)
        for (int cx = 0; cx < SHAPE_SIZE[type][rot][0]; cx++)
            if (cell(type, rot, cx, cy)) {
                int px = x + cx, py = y + cy;
                if (py >= 0 && py < H && px >= 0 && px < W) board[py][px] = 1;
            }
    clear_lines();
    new_piece();
}

/* ---------- 6) 消行：从底部往上找满行，整体下移 ----------
 * 关键：消掉第 r 行后，原本 r-1 行的内容会落到 r 行，
 *       所以必须「重新检查同一行」，不能直接跳过。
 */
static void clear_lines(void) {
    int r = H - 1;
    while (r >= 0) {
        int full = 1;
        for (int c = 0; c < W; c++)
            if (!board[r][c]) { full = 0; break; }
        if (full) {
            for (int rr = r; rr > 0; rr--)
                memcpy(board[rr], board[rr - 1], sizeof board[0]);
            memset(board[0], 0, sizeof board[0]);
            score += 100;
            lines++;
            drop_interval = (score / 500) + 1;   /* 每消 500 分下坠加快一级 */
            /* r 不变：继续检查这一行（内容已下移） */
        } else {
            r--;
        }
    }
}

/* ---------- 7) 生成新方块 + 游戏结束判定 ---------- */
static void new_piece(void) {
    type = rand() % 7;
    rot  = 0;
    x    = (W - SHAPE_SIZE[type][rot][0]) / 2;   /* 水平居中 */
    y    = 0;
    if (!fits(type, rot, x, y)) over = 1;   /* 顶部放不下 → 结束 */
}

/* ---------- 8) 显示端 ----------
 * 只重画「和上一帧不同」的行，没变的行一个字符都不输出。
 * 所以画面不会闪，也不会整屏抖动。
 */
static void show(void) {
    static int stamp[4][4];
    char line[R_MAXCOLS];
    char border[R_MAXCOLS];
    int n;

    /* 把活动方块临时盖到棋盘上，显示完再擦掉（这样就不会残影） */
    for (int cy = 0; cy < 4; cy++)
        for (int cx = 0; cx < 4; cx++) {
            stamp[cy][cx] = cell(type, rot, cx, cy) &&
                            y + cy >= 0 && y + cy < H && x + cx >= 0 && x + cx < W;
            if (stamp[cy][cx]) board[y + cy][x + cx] = 1;
        }

    r_frame_start();

    /* 上下边框 */
    n = 0; border[n] = '\0';
    border[n++] = '+';
    for (int c = 0; c < W * 2; c++) border[n++] = '-';
    border[n++] = '+';
    border[n] = '\0';
    r_line(0, border);

    /* 场地：每一格 2 个字符宽，实心是 []，空是空格 */
    for (int r = 0; r < H; r++) {
        n = 0; line[n] = '\0';
        line[n++] = '|';
        for (int c = 0; c < W; c++)
            r_put(line, &n, board[r][c] ? 1 : 0);
        line[n++] = '|';
        line[n] = '\0';
        r_line(r + 1, line);
    }

    r_line(H + 1, border);       /* 底边框 */

    /* 状态行 */
    snprintf(line, sizeof line, "score %d    lines %d%s",
             score, lines, over ? "    GAME OVER" : "");
    r_line(H + 3, line);

    /* 擦掉活动方块 */
    for (int cy = 0; cy < 4; cy++)
        for (int cx = 0; cx < 4; cx++)
            if (stamp[cy][cx]) board[y + cy][x + cx] = 0;
}

/* ---------- 4) 下坠计时 ---------- */
static void gravity(void) {
    if (++tick < drop_interval) return;
    tick = 0;
    if (!move(0, 1)) lock();     /* 落不下去 → 锁定并换块 */
}

int main(void) {
#ifndef _WIN32
    key_init();
#endif
    /* 按终端窗口高度定棋盘高度：上下边框 2 行 + 状态行留 3 行。
     * 打印行数一旦超过窗口高度，终端就会滚动 —— 滚动看起来就是画面在跳。 */
    {
        int rows = term_rows();
        if (rows > 0) {
            H = rows - 6;
            if (H > HMAX) H = HMAX;
            if (H < 10)   H = 10;
        }
    }
    memset(board, 0, sizeof board);
    score = 0; lines = 0; over = 0; tick = 0; drop_interval = 40;
    srand((unsigned)time(NULL));
    new_piece();

    r_enter();               /* 进备用屏 + 藏光标：退出后终端原样恢复 */

    while (!over) {
        show();

        if (key_hit()) {
            switch (key_get()) {
                case 'L': case 'a': move(-1, 0); break;
                case 'R': case 'd': move( 1, 0); break;
                case 'D': case 's': if (move(0, 1)) score++; else lock(); break;
                case 'U': case 'w': rotate(1); break;
                case ' ': hard_drop(); break;
                case 'Q': case 'q': goto done;
                default: break;
            }
        } else {
            gravity();          /* 空闲时自动下落 */
        }
        key_delay(16);          /* 约 60 帧上限，够流畅也不刷屏 */
    }
    show();

done:
    r_leave();                /* 恢复光标 + 切回原屏幕 */
    return 0;
}
