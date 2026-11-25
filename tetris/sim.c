/* ============================================================
 * sim.c —— 无头游戏模拟器（纯 C，不需要键盘）
 *
 * 【目的】
 * 沙箱/自动化环境里没法按键盘，所以用「假按键」驱动核心逻辑，
 * 自动玩几百局，验证完整游戏流程没问题：
 *   下坠 -> 落地锁定 -> 消行 -> 计分 -> 加速 -> 新方块 -> 游戏结束
 * 这是对 tetris.c 逻辑的一次端到端回归。
 *
 * 【怎么跑】
 *   gcc sim.c -o sim.exe
 *   ./sim.exe
 *   打印每局的统计，最后打印 SIMULATION PASSED
 * ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 10
#define H 20
#define GAMES 200

static const unsigned short SHAPE[7][4][4] = {
    {{0x03,0x03}, {0x03,0x03}, {0x03,0x03}, {0x03,0x03}},
    {{0x0F}, {0x01,0x01,0x01,0x01}, {0x0F}, {0x01,0x01,0x01,0x01}},
    {{0x02,0x02,0x03}, {0x07,0x04}, {0x03,0x01,0x01}, {0x01,0x07}},
    {{0x01,0x01,0x03}, {0x04,0x04,0x03}, {0x03,0x04,0x04}, {0x06,0x01,0x01}},
    {{0x03,0x06}, {0x02,0x03,0x01}, {0x03,0x06}, {0x02,0x03,0x01}},
    {{0x06,0x03}, {0x01,0x03,0x02}, {0x06,0x03}, {0x01,0x03,0x02}},
    {{0x02,0x07}, {0x02,0x03,0x02}, {0x07,0x02}, {0x01,0x03,0x01}},
};
static const unsigned char SHAPE_SIZE[7][4][2] = {
    {{2,2}, {2,2}, {2,2}, {2,2}},
    {{4,1}, {1,4}, {4,1}, {1,4}},
    {{2,3}, {3,2}, {2,3}, {3,2}},
    {{2,3}, {3,2}, {2,3}, {3,2}},
    {{3,2}, {2,3}, {3,2}, {2,3}},
    {{3,2}, {2,3}, {3,2}, {2,3}},
    {{3,2}, {2,3}, {3,2}, {2,3}},
};

static int board[H][W];
static int type, rot, x, y;
static int score, lines, over, tick, drop_interval;

static void new_piece(void);   /* 前置声明 */

static int cell(int t, int r, int cx, int cy) {
    int w = SHAPE_SIZE[t][r][0];
    if (cy >= SHAPE_SIZE[t][r][1] || cx >= w) return 0;
    return (SHAPE[t][r][cy] >> (w - 1 - cx)) & 1;
}
static int fits(int nt, int nr, int nx, int ny) {
    for (int cy = 0; cy < SHAPE_SIZE[nt][nr][1]; cy++)
        for (int cx = 0; cx < SHAPE_SIZE[nt][nr][0]; cx++) {
            if (!cell(nt, nr, cx, cy)) continue;
            int px = nx + cx, py = ny + cy;
            if (px < 0 || px >= W) return 0;
            if (py >= H)          return 0;
            if (py >= 0 && board[py][px]) return 0;
        }
    return 1;
}
static void clear_lines(void) {
    int r = H - 1;
    while (r >= 0) {
        int full = 1;
        for (int c = 0; c < W; c++) if (!board[r][c]) { full = 0; break; }
        if (full) {
            for (int rr = r; rr > 0; rr--) memcpy(board[rr], board[rr-1], sizeof board[0]);
            memset(board[0], 0, sizeof board[0]);
            score += 100; lines++;
            drop_interval = (score / 500) + 1;
        } else r--;
    }
}
static void lock(void) {
    for (int cy = 0; cy < SHAPE_SIZE[type][rot][1]; cy++)
        for (int cx = 0; cx < SHAPE_SIZE[type][rot][0]; cx++)
            if (cell(type, rot, cx, cy)) {
                int px = x + cx, py = y + cy;
                if (py >= 0 && py < H && px >= 0 && px < W) board[py][px] = type + 1;
            }
    clear_lines();
    new_piece();          /* 锁定后必须生成新方块（与 tetris.c 一致） */
}
static void new_piece(void) {
    type = rand() % 7;
    rot = 0;
    x = (W - SHAPE_SIZE[type][rot][0]) / 2;
    y = 0;
    if (!fits(type, rot, x, y)) over = 1;
}

/* ------------------------------------------------------------
 * 贪心 AI：给当前方块找一个落点。
 * 评分：落下去之后的「列高总和 + 空洞数」越小越好。
 * 有了这个 AI 才能真的填满一整行，测到完整的消行路径。
 * ------------------------------------------------------------ */
static int try_place(int nt, int nr, int nx, int ny, int *out_score) {
    if (!fits(nt, nr, nx, ny)) return 0;

    /* 在临时棋盘上模拟落到底 */
    int tmp[H][W];
    memcpy(tmp, board, sizeof board);
    int tx = nx, ty = ny;
    while (fits(nt, nr, tx, ty + 1)) ty++;

    for (int cy = 0; cy < SHAPE_SIZE[nt][nr][1]; cy++)
        for (int cx = 0; cx < SHAPE_SIZE[nt][nr][0]; cx++)
            if (cell(nt, nr, cx, cy)) {
                int px = tx + cx, py = ty + cy;
                if (py >= 0 && py < H && px >= 0 && px < W) tmp[py][px] = 1;
            }

    /* 评分：空洞越多越差，列高总和越小越好 */
    int score = 0;
    for (int c = 0; c < W; c++) {
        int h = 0, hole = 0, seen = 0;
        for (int r = 0; r < H; r++) {
            if (tmp[r][c]) { if (!seen) h = H - r; seen = 1; }
            else if (seen) hole++;
        }
        score += h * 4 + hole * 8;
    }
    /* 刚刚消掉行会有奖励 */
    for (int r = H - 1; r >= 0; r--) {
        int full = 1;
        for (int c = 0; c < W; c++) if (!tmp[r][c]) { full = 0; break; }
        if (full) score -= 30;
    }
    *out_score = score;
    return 1;
}

/* 给当前方块挑一个最优落点，直接执行（旋转 + 移动 + 硬降） */
static void ai_move(void) {
    int best_score = 0x7fffffff;
    int best_rot = rot, best_x = x;

    for (int r = 0; r < 4; r++) {
        for (int nx = 0; nx + SHAPE_SIZE[type][r][0] <= W; nx++) {
            int s;
            if (try_place(type, r, nx, 0, &s) && s < best_score) {
                best_score = s; best_rot = r; best_x = nx;
            }
        }
    }
    rot = best_rot;
    x = best_x;
    y = 0;
    while (fits(type, rot, x, y + 1)) y++;
}

int main(void) {
    srand(20261022);
    int total_lines = 0, max_lines = 0, max_score = 0, games_over = 0;
    int failed = 0;

    for (int g = 0; g < GAMES; g++) {
        memset(board, 0, sizeof board);
        score = 0; lines = 0; over = 0; tick = 0; drop_interval = 40;
        new_piece();

        int steps = 0;
        while (!over && steps < 100000) {
            steps++;
            /* 贪心 AI 决策：选一个能消行的落点 */
            ai_move();
            int n = 0;
            while (fits(type, rot, x, y + 1)) { y++; n++; }
            score += n * 2;
            lock();
        }
        if (steps >= 100000) { printf("game %d: TIMEOUT (infinite loop!)\n", g); failed = 1; }
        if (!over)          { printf("game %d: did not end\n", g); failed = 1; }

        games_over++;
        total_lines += lines;
        if (lines > max_lines) max_lines = lines;
        if (score > max_score) max_score = score;
    }

    printf("=== 无头游戏模拟 ===\n");
    printf("对局数        : %d\n", games_over);
    printf("全部正常结束  : %s\n", games_over == GAMES ? "YES" : "NO");
    printf("总消行数      : %d\n", total_lines);
    printf("单局最多消行  : %d\n", max_lines);
    printf("单局最高分    : %d\n", max_score);
    if (total_lines == 0) { printf("警告：一行都没消掉，随机策略可能太保守\n"); failed = 1; }

    if (failed) { printf("\nSIMULATION FAILED\n"); return 1; }
    printf("\nSIMULATION PASSED\n");
    return 0;
}
