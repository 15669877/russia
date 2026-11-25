/* ============================================================
 * step1_display.c —— 第 1 步：显示界面 / 把方块画出来
 *
 * 【这一步只解决一件事】
 *   怎么在终端里「画」出一个方块。
 *
 * 【要理解的知识点】
 *   1) 图形 = 数据。7 种方块用一张表存起来（shapes.h），
 *      每一行用一个「位掩码」表示：4 位二进制，bit3 = 最左列。
 *          0x0E = 0b1110 = .###   左边空一格，右边三格实心
 *      打印时逐位判断该画方块还是画空格。
 *   2) 循环嵌套：外层管行，内层管列。
 *   3) 用「包围盒」当锚点：每个旋转态都有自己的宽 w 高 h，
 *      所以 (x,y) 永远指图形左上角，旋转时不会跳动。
 *   4) 一个格子画成两个字符（"[]"）。终端字符是瘦高比例，
 *      一格只画一个字符的话整个图形会被拉长，一格两个字符才是正方形。
 *
 * 【怎么跑】
 *   Visual Studio：打开 tetris.sln → 右键 step1_display → 设为启动项目 → Ctrl+F5
 *   运行后输入 0~6 选形状，再输入 0~3 选旋转态，输入 q 退出。
 * ============================================================ */
#include <stdio.h>
#include "shapes.h"
#include "render.h"

/* 把某个形状的某个旋转态画出来 */
static void draw_shape(int t, int rot) {
    int w = SHAPE_SIZE[t][rot][0];       /* 宽 */
    int h = SHAPE_SIZE[t][rot][1];       /* 高 */

    printf("shape %d, rot %d  (%dx%d)\n", t, rot, w, h);
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            /* 取这一行掩码的第 c 位：bit(w-1-c) */
            int bit = (SHAPE[t][rot][r] >> (w - 1 - c)) & 1;
            printf("%s", bit ? BLK : EMP);
        }
        printf("\n");
    }
    printf("----\n");
}

int main(void) {
    int t, rot;

    printf("Pick a shape (0~6), or q to quit.\n");
    printf("  0=O  1=I  2=J  3=L  4=S  5=Z  6=T\n\n");

    while (1) {
        printf("shape (0~6): ");
        if (scanf("%d", &t) != 1) {        /* 输入非数字（如 q）就退出 */
            int c;                        /* 清掉输入缓冲 */
            while ((c = getchar()) != '\n' && c != EOF) { }
            printf("Bye.\n");
            break;
        }
        if (t < 0 || t > 6) { printf("Only 0~6 supported.\n"); continue; }

        printf("rotation (0~3): ");
        if (scanf("%d", &rot) != 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF) { }
            printf("Bye.\n");
            break;
        }
        if (rot < 0 || rot > 3) rot = 0;

        draw_shape(t, rot);
    }
    return 0;
}
