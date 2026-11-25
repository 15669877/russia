/* ============================================================
 * selftest.c —— 核心逻辑自测（不需要键盘，命令行直接跑）
 * 目的：在没有交互终端的环境里验证 tetris.c 的判定逻辑是否正确。
 *       测：碰撞、锁墙、消行、计分、旋转、满盘检测、形状表完整性。
 *
 * 编译：gcc selftest.c -o selftest.exe
 * 运行：selftest.exe      全部通过打印 ALL TESTS PASSED
 * ============================================================ */
#include <stdio.h>
#include <string.h>

#define W 10
#define H 20

static int board[H][W];
static int type, rot, x, y;
static int score, lines;

/* 与 tetris.c 共用同一份形状表（由 gen_shapes.py 生成） */
#include "shapes.h"

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

static int move(int dx, int dy) {
    if (fits(type, rot, x + dx, y + dy)) { x += dx; y += dy; return 1; }
    return 0;
}

static void rotate(int d) {
    int nr = (rot + d + 4) % 4;
    int ow = SHAPE_SIZE[type][rot][0], oh = SHAPE_SIZE[type][rot][1];
    int nw = SHAPE_SIZE[type][nr][0], nh = SHAPE_SIZE[type][nr][1];
    int cx = x + (ow - 1) / 2, cy = y + (oh - 1) / 2;
    int bx = cx - (nw - 1) / 2, by = cy - (nh - 1) / 2;
    int cand[6][2] = { {bx,by}, {bx-1,by}, {bx+1,by},
                        {bx-2,by}, {bx+2,by}, {bx,by-1} };
    for (int i = 0; i < 6; i++)
        if (fits(type, nr, cand[i][0], cand[i][1])) {
            x = cand[i][0]; y = cand[i][1]; rot = nr; return;
        }
}

static void lock(void) {
    for (int cy = 0; cy < SHAPE_SIZE[type][rot][1]; cy++)
        for (int cx = 0; cx < SHAPE_SIZE[type][rot][0]; cx++)
            if (cell(type, rot, cx, cy)) {
                int px = x + cx, py = y + cy;
                if (py >= 0 && py < H && px >= 0 && px < W) board[py][px] = type + 1;
            }
}

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
        } else {
            r--;
        }
    }
}

/* ---- 测试辅助 ---- */
static int failed = 0;
static void check(const char *name, int cond) {
    if (cond) printf("  [PASS] %s\n", name);
    else { printf("  [FAIL] %s\n", name); failed = 1; }
}
static void reset(void) {
    memset(board, 0, sizeof board);
    type = 0; rot = 0; x = 3; y = 0;
    score = 0; lines = 0;
}
static int piece_bottom(void) {
    int hi = -1;
    for (int cy = 0; cy < SHAPE_SIZE[type][rot][1]; cy++)
        for (int cx = 0; cx < SHAPE_SIZE[type][rot][0]; cx++)
            if (cell(type, rot, cx, cy) && y + cy > hi) hi = y + cy;
    return hi;
}
static int piece_left(void) {
    int lo = 99;
    for (int cy = 0; cy < SHAPE_SIZE[type][rot][1]; cy++)
        for (int cx = 0; cx < SHAPE_SIZE[type][rot][0]; cx++)
            if (cell(type, rot, cx, cy) && x + cx < lo) lo = x + cx;
    return lo;
}
static int piece_right(void) {
    int hi = -1;
    for (int cy = 0; cy < SHAPE_SIZE[type][rot][1]; cy++)
        for (int cx = 0; cx < SHAPE_SIZE[type][rot][0]; cx++)
            if (cell(type, rot, cx, cy) && x + cx > hi) hi = x + cx;
    return hi;
}
static int within_wall(void) {
    for (int cy = 0; cy < SHAPE_SIZE[type][rot][1]; cy++)
        for (int cx = 0; cx < SHAPE_SIZE[type][rot][0]; cx++)
            if (cell(type, rot, cx, cy)) {
                int px = x + cx;
                if (px < 0 || px >= W) return 0;
            }
    return 1;
}

int main(void) {
    printf("=== tetris core logic self-test ===\n\n");

    /* 1. 形状表完整性 */
    printf("[1] SHAPE table integrity\n");
    {
        int bad = 0;
        for (int t = 0; t < 7; t++)
            for (int r = 0; r < 4; r++) {
                int n = 0;
                for (int cy = 0; cy < SHAPE_SIZE[t][r][1]; cy++)
                    for (int cx = 0; cx < SHAPE_SIZE[t][r][0]; cx++) n += cell(t, r, cx, cy);
                if (n != 4) bad = 1;
            }
        check("every shape state has exactly 4 cells", bad == 0);

        int osame = 1;
        for (int r = 1; r < 4; r++)
            for (int cy = 0; cy < 4; cy++)
                if (SHAPE[0][0][cy] != SHAPE[0][r][cy]) osame = 0;
        check("O piece is 2x2 in all rotations", osame == 1 &&
              SHAPE_SIZE[0][0][0] == 2 && SHAPE_SIZE[0][0][1] == 2);

        check("I piece alternates 4x1 and 1x4",
              SHAPE_SIZE[1][0][0] == 4 && SHAPE_SIZE[1][0][1] == 1 &&
              SHAPE_SIZE[1][1][0] == 1 && SHAPE_SIZE[1][1][1] == 4);

        int jl = 0, sz = 0;
        for (int r = 0; r < 4; r++) {
            for (int cy = 0; cy < 4; cy++) {
                if (SHAPE[2][r][cy] != SHAPE[3][r][cy]) jl = 1;
                if (SHAPE[4][r][cy] != SHAPE[5][r][cy]) sz = 1;
            }
        }
        check("J and L are different shapes", jl == 1);
        check("S and Z are different shapes", sz == 1);
    }

    /* 2. 碰撞：左右墙 */
    printf("[2] fits() wall check\n");
    reset();
    check("center fits",     fits(type, rot, 3, 0) == 1);
    check("far left blocked",  fits(type, rot, -3, 0) == 0);
    check("far right blocked", fits(type, rot, W, 0) == 0);
    {
        int ok = 1, guard = 0;
        type = 0; rot = 0; x = 3; y = 0;
        while (move(-1, 0)) { if (!within_wall()) ok = 0; if (++guard > 20) break; }
        if (!within_wall()) ok = 0;
        check("sliding left never leaves wall", ok == 1);
    }

    /* 3. 碰撞：地板 */
    printf("[3] fits() floor check\n");
    reset();
    type = 0; rot = 0; x = 3; y = H - 2;      /* O 块高 2 行 */
    check("O at H-2 fits",    fits(type, rot, 3, H - 2) == 1);
    check("O at H-1 blocked", fits(type, rot, 3, H - 1) == 0);
    check("y=H blocked",      fits(type, rot, 3, H) == 0);

    /* 4. 碰撞：撞到已有方块 */
    printf("[4] fits() occupied cell\n");
    reset();
    type = 0; rot = 0; x = 3;
    board[H - 1][4] = 1;                      /* O 块占第 3、4 列 */
    check("blocked by filled cell", fits(type, rot, 3, H - 2) == 0);
    check("empty area still fits", fits(type, rot, 6, H - 2) == 1);

    /* 5. move() 不变量 */
    printf("[5] move() respects wall\n");
    {
        int ok = 1;
        for (int t = 0; t < 7; t++) {
            type = t; rot = 0; x = 3; y = 5;
            for (int i = 0; i < 30; i++) { move(-1, 0); if (!within_wall()) ok = 0; }
            for (int i = 0; i < 30; i++) { move(1, 0);  if (!within_wall()) ok = 0; }
        }
        check("all 7 shapes stay in wall while moving", ok == 1);
    }

    /* 6. 下坠到底自动停止 */
    printf("[6] gravity stops at floor\n");
    reset();
    type = 0; rot = 0; x = 3; y = 0;
    int guard = 0;
    while (fits(type, rot, x, y + 1)) { y++; if (++guard > 200) break; }
    check("reached bottom row H-1", piece_bottom() == H - 1);
    check("cannot go further",      fits(type, rot, x, y + 1) == 0);

    /* 7. 上方有方块时，新方块停在它上面 */
    printf("[7] stacked piece blocks fall\n");
    reset();
    type = 0; rot = 0; x = 3; y = H - 2;
    lock();
    type = 0; rot = 0; x = 3; y = 0;
    guard = 0;
    while (fits(type, rot, x, y + 1)) { y++; if (++guard > 200) break; }
    check("second piece stops above first", piece_bottom() == H - 3);

    /* 8. 锁定写进棋盘 */
    printf("[8] lock() writes into board\n");
    reset();
    type = 0; rot = 0; x = 3; y = H - 2;
    lock();
    check("exactly 4 cells filled",
          board[H-2][3]+board[H-2][4]+board[H-1][3]+board[H-1][4] == 4);

    /* 9. 消一行 */
    printf("[9] clear_lines() removes one row\n");
    reset();
    for (int c = 0; c < W; c++) board[H - 1][c] = 1;
    lines = 0; score = 0;
    clear_lines();
    check("one line cleared", lines == 1);
    check("score = 100", score == 100);
    check("bottom row empty now", board[H - 1][0] == 0);

    /* 10. 一次消多行（旧代码 bug：一次只消一行） */
    printf("[10] clear_lines() removes multiple rows at once\n");
    reset();
    for (int c = 0; c < W; c++) { board[H - 1][c] = 1; board[H - 2][c] = 2; }
    lines = 0; score = 0;
    clear_lines();
    check("two lines cleared", lines == 2);
    check("score = 200", score == 200);

    /* 11. 消三行 + 上方内容下移 */
    printf("[11] clear_lines() shifts rows above down\n");
    reset();
    for (int c = 0; c < W; c++) { board[H-1][c]=1; board[H-2][c]=1; board[H-3][c]=1; }
    board[H-4][0] = 9;
    lines = 0; score = 0;
    clear_lines();
    check("three lines cleared", lines == 3);
    check("cell above moved down", board[H-1][0] == 9);

    /* 12. 旋转 */
    printf("[12] rotate() works\n");
    reset();
    type = 6; rot = 0; x = 3; y = 5;
    rotate(1);
    check("rot changed", rot == 1);
    check("still fits",   fits(type, rot, x, y) == 1);
    rotate(1); rotate(1); rotate(1);
    check("back to original", rot == 0);

    /* 13. 靠墙旋转不越界（这正是用户第 4 步要的「墙 + 检测」） */
    printf("[13] rotate() near wall stays in bounds\n");
    {
        int ok = 1;
        for (int t = 0; t < 7; t++) {
            type = t; rot = 0; x = 0; y = 5;
            rotate(1);
            if (piece_left() < 0 || piece_right() >= W) {
                printf("    shape %d rotated out of wall\n", t);
                ok = 0;
            }
        }
        check("all 7 shapes stay inside wall when rotated at x=0", ok == 1);
    }

    /* 14. 满盘 = 游戏结束 */
    printf("[14] top-out detection\n");
    reset();
    for (int r = 0; r < H; r++)
        for (int c = 0; c < W; c++) board[r][c] = 1;
    check("cannot spawn on full board", fits(0, 0, 3, 0) == 0);

    /* 15. 硬降距离计分 */
    printf("[15] hard drop distance\n");
    reset();
    type = 0; rot = 0; x = 3; y = 0;
    int n = 0;
    while (fits(type, rot, x, y + 1)) { y++; n++; }
    check("dropped H-2 cells", n == H - 2);

    /* 16. 7 种形状都能下坠到底并正常锁定 */
    printf("[16] every shape falls and locks correctly\n");
    {
        int allok = 1;
        for (int t = 0; t < 7; t++) {
            reset();
            type = t; rot = 0; x = 3; y = 0;
            guard = 0;
            while (fits(type, rot, x, y + 1)) { y++; if (++guard > 200) break; }
            if (piece_bottom() != H - 1) { allok = 0; printf("    shape %d not at bottom\n", t); }
            lock();
            int cnt = 0;
            for (int r = 0; r < H; r++) for (int c = 0; c < W; c++) if (board[r][c]) cnt++;
            if (cnt != 4) { allok = 0; printf("    shape %d locked %d cells\n", t, cnt); }
        }
        check("all shapes land at bottom with 4 cells", allok == 1);
    }

    /* 17. 4 个旋转态都能正常放置（不会因为锚点算错而非法） */
    printf("[17] all 28 rotation states are placeable\n");
    {
        int allok = 1;
        for (int t = 0; t < 7; t++)
            for (int r = 0; r < 4; r++) {
                reset();
                type = t; rot = r; x = 3; y = 5;
                if (!fits(type, rot, x, y)) { allok = 0; printf("    t=%d r=%d not placeable\n", t, r); }
            }
        check("every rotation state fits on empty board", allok == 1);
    }

    printf("\n");
    if (failed) { printf("SOME TESTS FAILED\n"); return 1; }
    printf("ALL TESTS PASSED\n");
    return 0;
}
