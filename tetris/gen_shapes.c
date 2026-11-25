/* ============================================================
 * gen_shapes.c —— 生成 shapes.h（纯 C 版，不要 Python）
 *
 * 【为什么需要这个工具】
 * 俄罗斯方块有 7 种形状 x 4 个旋转态 = 28 个状态。
 * 纯手写在 C 里写死这些数据极易出错：
 *   - J 和 L 会写成一样（它们是镜像关系）
 *   - 掩码会超出该行的宽度（宽度 2 的行却写了 0x06）
 *   - 旋转后锚点漂移，图形会「跳一下」
 * 所以这里只手写 7 种形状的「标准姿态」，其余旋转由程序转出来，
 * 再由本工具统一输出成 shapes.h。tetris.c 与 selftest.c 共用同一份。
 *
 * 【用法】
 *   gcc gen_shapes.c -o gen_shapes.exe
 *   ./gen_shapes.exe
 *   （会在当前目录生成 shapes.h）
 *
 * 【数据表示】
 *   形状用字符串数组画出：'X' = 实心，'.' = 空
 *   旋转态用「行掩码」存：4 位二进制，bit3 = 最左列
 *       0x0E = 0b1110 = .###
 *   SIZE[t][rot] = 该旋转态的宽、高
 * ============================================================ */
#include <stdio.h>
#include <string.h>

#define NT 7        /* 形状种数 */
#define NR 4        /* 每种的旋转态数 */
#define MAXN 4      /* 画布最大边长 */

static const char *NAMES[NT] = { "O", "I", "J", "L", "S", "Z", "T" };

/* 7 种形状的标准姿态，画布必须是正方形，否则旋转会错位 */
static const char *BASE[NT][MAXN] = {
    { "XX",  "XX",  NULL, NULL },        /* 0 O  2x2 画布 */
    { "XXXX","....","....","...." },     /* 1 I  4x4 画布 */
    { "X..", "X..", "XX.", NULL },        /* 2 J  3x3 画布 */
    { ".X.", ".X.", "XX.", NULL },        /* 3 L  3x3 画布 */
    { ".XX", "XX.", "...", NULL },        /* 4 S  3x3 画布 */
    { "XX.", ".XX", "...", NULL },        /* 5 Z  3x3 画布 */
    { ".X.", "XXX", "...", NULL },        /* 6 T  3x3 画布 */
};

/* 每个形状的画布边长 */
static const int CANVAS[NT] = { 2, 4, 3, 3, 3, 3, 3 };

/* 生成的表：shape[t][rot][row] = 行掩码，w[t][rot] / h[t][rot] = 宽高 */
static unsigned short shape[NT][NR][MAXN];
static int w[NT][NR], h[NT][NR];

/* 顺时针转 90 度：new[r][c] = old[n-1-c][r] */
static void rot90(char in[MAXN][MAXN], int n, char out[MAXN][MAXN]) {
    for (int r = 0; r < n; r++)
        for (int c = 0; c < n; c++)
            out[r][c] = in[n - 1 - c][r];
}

/* 把一个形状裁剪到自己的包围盒，输出行掩码 + 宽高 */
static void normalize(char in[MAXN][MAXN], int n,
                      unsigned short masks[MAXN], int *ow, int *oh) {
    int left = n, right = -1, top = n, bottom = -1;

    /* 先找出实心格子的范围 */
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            if (in[r][c] != 'X') continue;
            if (r < top)    top = r;
            if (r > bottom) bottom = r;
            if (c < left)   left = c;
            if (c > right)  right = c;
        }
    }
    if (right < 0) { *ow = 0; *oh = 0; masks[0] = 0; return; }

    int ww = right - left + 1;
    int hh = bottom - top + 1;

    /* 每一行转成掩码：bit(ww-1-c) 对应第 c 列 */
    for (int r = 0; r < hh; r++) {
        unsigned short m = 0;
        for (int c = 0; c < ww; c++)
            if (in[top + r][left + c] == 'X')
                m |= (unsigned short)(1u << (ww - 1 - c));
        masks[r] = m;
    }
    *ow = ww;
    *oh = hh;
}

int main(void) {
    /* ---- 1. 生成 28 个旋转态 ---- */
    for (int t = 0; t < NT; t++) {
        char buf[MAXN][MAXN];
        int n = CANVAS[t];
        char cur[MAXN][MAXN];

        /* 复制初始姿态 */
        for (int r = 0; r < n; r++) {
            const char *s = BASE[t][r];
            for (int c = 0; c < n; c++) cur[r][c] = s ? s[c] : '.';
        }

        for (int rot = 0; rot < NR; rot++) {
            normalize(cur, n, shape[t][rot], &w[t][rot], &h[t][rot]);
            rot90(cur, n, buf);
            for (int r = 0; r < n; r++)
                for (int c = 0; c < n; c++) cur[r][c] = buf[r][c];
        }
    }

    /* ---- 2. 自检：形状表必须合法 ---- */
    int bad = 0;
    for (int t = 0; t < NT; t++)
        for (int rot = 0; rot < NR; rot++) {
            int cells = 0;
            for (int r = 0; r < h[t][rot]; r++) {
                unsigned short m = shape[t][rot][r];
                for (int b = 0; b < 4; b++) if (m & (1u << b)) cells++;
                /* 掩码不能超出该行的宽度 */
                if (m >= (1u << w[t][rot])) {
                    printf("ERROR: t=%d rot=%d row=%d mask=0x%02X exceeds width %d\n",
                           t, rot, r, m, w[t][rot]);
                    bad = 1;
                }
            }
            if (cells != 4) {
                printf("ERROR: t=%d rot=%d has %d cells (want 4)\n", t, rot, cells);
                bad = 1;
            }
        }

    /* 7 种形状必须互不相同；J/L、S/Z 必须是镜像（不同） */
    for (int a = 0; a < NT; a++)
        for (int b = a + 1; b < NT; b++) {
            int same = 1;
            for (int rot = 0; rot < NR; rot++)
                for (int r = 0; r < MAXN; r++)
                    if (shape[a][rot][r] != shape[b][rot][r]) same = 0;
            if (same) {
                printf("ERROR: shape %d and %d are identical\n", a, b);
                bad = 1;
            }
        }

    if (bad) {
        printf("\nFAILED: shape table is invalid, shapes.h not written.\n");
        return 1;
    }

    /* ---- 3. 输出 shapes.h ---- */
    FILE *f = fopen("shapes.h", "w");
    if (!f) { printf("ERROR: cannot open shapes.h for writing\n"); return 1; }

    fprintf(f,
"/* ============================================================\n"
" * shapes.h —— 由 gen_shapes.c 自动生成，请勿手改！\n"
" *\n"
" * 【为什么用工具生成】\n"
" * 手写 7 种形状 x 4 个旋转态 = 28 个状态，极易出错：\n"
" *   - J 和 L 会写成一样（它们是镜像关系）\n"
" *   - 掩码会超出该行的宽度（比如宽度 2 的行却写了 0x06）\n"
" *   - 旋转后锚点会漂移，图形会「跳一下」\n"
" * 所以：只手写 7 种形状的「标准姿态」，其余旋转由程序转出来，\n"
" *       再由工具统一输出成这个头文件，tetris.c 与 selftest.c 共用同一份。\n"
" *\n"
" * 【数据结构】\n"
" *   SHAPE[t][rot][row]  第 row 行的位掩码，bit3 = 最左列\n"
" *                        例 0x0E = 0b1110 = .###\n"
" *   SHAPE_SIZE[t][rot]  该旋转态的宽、高\n"
" *\n"
" * 【归一化】\n"
" * 每个旋转态都裁掉四周的空白，只留自己的包围盒。\n"
" * 于是 (x, y) 永远表示「包围盒左上角在棋盘上的位置」，\n"
" * 旋转时图形不会跳动 —— 这是初学者最容易踩的坑。\n"
" * ============================================================ */\n"
"#ifndef SHAPES_H\n"
"#define SHAPES_H\n\n");

    fprintf(f, "static const unsigned short SHAPE[%d][%d][%d] = {\n", NT, NR, MAXN);
    for (int t = 0; t < NT; t++) {
        fprintf(f, "    {");
        for (int rot = 0; rot < NR; rot++) {
            fprintf(f, "%s{", rot ? ", " : "");
            for (int r = 0; r < MAXN; r++)
                fprintf(f, "%s0x%02X", r ? "," : "", shape[t][rot][r]);
            fprintf(f, "}");
        }
        fprintf(f, "},   /* %d %s */\n", t, NAMES[t]);
    }
    fprintf(f, "};\n\n");

    fprintf(f, "static const unsigned char SHAPE_SIZE[%d][%d][2] = {\n", NT, NR);
    for (int t = 0; t < NT; t++) {
        fprintf(f, "    {");
        for (int rot = 0; rot < NR; rot++)
            fprintf(f, "%s{%d,%d}", rot ? ", " : "", w[t][rot], h[t][rot]);
        fprintf(f, "},   /* %d %s */\n", t, NAMES[t]);
    }
    fprintf(f, "};\n\n#endif /* SHAPES_H */\n");

    fclose(f);
    printf("已生成 shapes.h（自检通过：28 状态各 4 格 / 掩码不超宽 / 7 形状互异）\n");

    /* ---- 4. 打印 ASCII 图，方便人工核对 ---- */
    for (int t = 0; t < NT; t++) {
        printf("%s:\n", NAMES[t]);
        for (int rot = 0; rot < NR; rot++) {
            printf("  rot%d  ", rot);
            for (int r = 0; r < h[t][rot]; r++) {
                for (int c = 0; c < w[t][rot]; c++) {
                    unsigned short m = shape[t][rot][r];
                    printf("%c", (m >> (w[t][rot] - 1 - c)) & 1 ? '#' : '.');
                }
                if (r < h[t][rot] - 1) printf("  ");
            }
            printf("  (%dx%d)\n", w[t][rot], h[t][rot]);
        }
    }
    return 0;
}
