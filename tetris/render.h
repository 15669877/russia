/* ============================================================
 * render.h —— 终端渲染层（纯 C，跨平台）
 *
 * 【解决三个问题】
 *
 * 1) 画面一闪一闪地闪
 *    常见写法是每帧 printf("\033[H\033[2J") —— 整屏清掉再重画。
 *    终端要处理整屏擦除 + 整屏重绘，眼睛就看到画面在抖。
 *
 *    这里的做法：把每一行的内容缓存下来，每帧只输出「和上一帧不同」的行，
 *    用绝对定位 \033[row;1H 直接跳到那一行重写，其余行一个字符都不动。
 *    没变化 = 屏幕上什么都不发生 = 完全不闪。
 *
 * 2) 内容滚动（也会看起来像在跳）
 *    打印的行数超过终端高度时，终端会往上滚，
 *    这时 \033[row;1H 的绝对定位就全错位了。
 *    解决：用「备用屏幕缓冲区」——游戏期间画在独立缓冲区里，
 *    退出时恢复原终端，用户原来的窗口内容一点没动。
 *
 * 3) 光标在画面里一闪一闪
 *    游戏时把光标藏起来，退出时再放出来。
 *
 * 【符号统一】
 *    方块一律用 BLK "[]"（两个字符宽），空格用 EMP "  "（同样两个字符宽），
 *    这样一列占两格，字符画对齐不会歪。
 *    边界只用最朴素的 | - + 三种 ASCII。
 *
 * 【用法】
 *    r_enter();                游戏开始时调用一次
 *    r_frame_start();          每帧开头调用
 *    r_line(0, "+--------+");   写第 0 行（0 起算）
 *    r_leave();                游戏结束时调用一次
 * ============================================================ */
#ifndef RENDER_H
#define RENDER_H

#include <stdio.h>
#include <string.h>

/* 方块与空格，都占 2 个字符宽，保证列对齐 */
#define BLK "[]"
#define EMP "  "

#define R_MAXROWS 64
#define R_MAXCOLS 256

static char r_cur[R_MAXROWS][R_MAXCOLS];   /* 上一帧每行的内容 */
static int  r_ready   = 0;                 /* 缓存是否已建立 */
static int  r_entered = 0;                 /* 是否已进入备用屏幕 */

/* 把一个格子拼进字符串：满格用 BLK，空格用 EMP */
static inline void r_put(char *buf, int *n, int filled) {
    const char *g = filled ? BLK : EMP;
    buf[(*n)++] = g[0];
    buf[(*n)++] = g[1];
    buf[*n] = '\0';
}

/* 进入备用屏幕 + 隐藏光标（游戏开始时调用一次） */
static inline void r_enter(void) {
    if (r_entered) return;
    printf("\033[?1049h");   /* 切到备用屏幕缓冲区 */
    printf("\033[?25l");     /* 隐藏光标 */
    printf("\033[2J\033[H");  /* 清一次备用屏 */
    fflush(stdout);
    r_entered = 1;
    r_ready = 0;
}

/* 离开备用屏幕 + 恢复光标（游戏结束时调用一次） */
static inline void r_leave(void) {
    if (!r_entered) return;
    printf("\033[?25h");     /* 光标放回来 */
    printf("\033[?1049l");   /* 切回原屏幕 */
    fflush(stdout);
    r_entered = 0;
    r_ready = 0;
}

/* 每帧开头调用。注意：这里不再清屏（清屏正是闪烁的元凶） */
static inline void r_frame_start(void) {
    if (!r_entered) r_enter();
}

/* 写第 row 行（0 起算）。内容没变就什么都不输出。 */
static inline void r_line(int row, const char *s) {
    if (row < 0 || row >= R_MAXROWS) return;
    if (r_ready && strcmp(r_cur[row], s) == 0) return;   /* 没变，不动它 */

    printf("\033[%d;1H%s\033[K", row + 1, s);   /* 定位 + 写入 + 清掉行尾残留 */

    /* 缓存这一行（带长度检查，不会截断） */
    size_t len = strlen(s);
    if (len > R_MAXCOLS - 1) len = R_MAXCOLS - 1;
    memcpy(r_cur[row], s, len);
    r_cur[row][len] = '\0';
    r_ready = 1;
}

/* 重新开始：清缓存，下一帧所有行都会重画 */
static inline void r_reset(void) {
    memset(r_cur, 0, sizeof r_cur);
    r_ready = 0;
}

#endif /* RENDER_H */
