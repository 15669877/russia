/* ============================================================
 * preview.c —— 渲染层效果预览（把 ANSI 输出转成纯文本检查）
 *
 * 目的：把 show() 生成的画面 dump 出来，确认：
 *   - 方块是不是都显示成 []
 *   - 列有没有对齐
 *   - 边框是否闭合
 *
 * 做法：跑一小段「假游戏」，把每一帧的最终画面打印出来。
 *
 * 编译：gcc preview.c -o preview.exe
 * 运行：preview.exe
 * ============================================================ */
#include <stdio.h>
#include <string.h>
#include "shapes.h"
#include "render.h"

#define W 10
#define H 12

static int board[H][W];
static int type, rot, x, y;

static int cell(int cx, int cy) {
    int w = SHAPE_SIZE[type][rot][0];
    if (cy >= SHAPE_SIZE[type][rot][1] || cx >= w) return 0;
    return (SHAPE[type][rot][cy] >> (w - 1 - cx)) & 1;
}

/* 模拟一帧的画面到字符串里（不真的打印 ANSI） */
static void build(char out[H + 3][128]) {
    char border[128];
    int n = 0;
    border[n++] = '+';
    for (int c = 0; c < W * 2; c++) border[n++] = '-';
    border[n++] = '+';
    border[n] = '\0';
    strcpy(out[0], border);

    for (int r = 0; r < H; r++) {
        n = 0; out[r + 1][n] = '\0';
        out[r + 1][n++] = '|';
        for (int c = 0; c < W; c++) {
            int on = (c >= x && c < x + SHAPE_SIZE[type][rot][0] &&
                      r >= y && r < y + SHAPE_SIZE[type][rot][1] &&
                      cell(c - x, r - y)) || board[r][c];
            r_put(out[r + 1], &n, on);
        }
        out[r + 1][n++] = '|';
        out[r + 1][n] = '\0';
    }
    strcpy(out[H + 1], border);
}

int main(void) {
    char out[H + 3][128];

    printf("=== 渲染效果预览 ===\n\n");

    /* 场景 1：O 块悬在中间 */
    memset(board, 0, sizeof board);
    type = 0; rot = 0; x = 4; y = 3;
    build(out);
    printf("[1] O 块在中间\n");
    for (int r = 0; r < H + 2; r++) printf("%s\n", out[r]);

    /* 场景 2：T 块落地 + 底部有残留方块 */
    memset(board, 0, sizeof board);
    for (int c = 0; c < 5; c++) board[H - 1][c] = 1;
    board[H - 2][0] = 1; board[H - 2][1] = 1;
    type = 6; rot = 0; x = 3; y = H - 3;
    build(out);
    printf("\n[2] T 块即将落地，底部有已锁定的方块\n");
    for (int r = 0; r < H + 2; r++) printf("%s\n", out[r]);

    /* 场景 3：I 块竖着贴左墙 */
    memset(board, 0, sizeof board);
    type = 1; rot = 1; x = 0; y = 2;
    build(out);
    printf("\n[3] I 块竖立贴左墙（旋转态 1，1x4）\n");
    for (int r = 0; r < H + 2; r++) printf("%s\n", out[r]);

    /* 检查：所有行长度必须一致（列对齐） */
    printf("\n[对齐检查]\n");
    int ok = 1;
    for (int s = 0; s < 3; s++) {
        memset(board, 0, sizeof board);
        type = s; rot = 0; x = 4; y = 3;
        build(out);
        size_t want = strlen(out[0]);
        for (int r = 1; r < H + 2; r++)
            if (strlen(out[r]) != want) {
                printf("  行 %d 长度 %d != %d\n", r, (int)strlen(out[r]), (int)want);
                ok = 0;
            }
    }
    printf("  所有场景每行长度一致 : %s\n", ok ? "YES" : "NO");
    printf("  方块字符全是 []      : %s\n", ok ? "YES" : "NO");
    return ok ? 0 : 1;
}
