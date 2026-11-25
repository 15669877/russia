/* key.h —— 极简跨平台按键读取（非阻塞）
 * 用法: key_hit() 判断是否有键, key_get() 取一个键。
 *   Windows: _kbhit()/_getch()  (conio.h)
 *   POSIX  : select() + termios,读到 ESC 时再等一个字节判断方向键
 * 返回值统一为: 'L'(左) 'R'(右) 'U'(上) 'D'(下) ' '(空格) 'Q'(退出) 以及普通字符
 *
 * 注意：Windows 的 _getch() 对方向键返回 0 + 第二个字节
 *       ('H'上 'P'下 'M'左 'K'右)，必须在这里翻译成 'U'/'D'/'L'/'R'，
 *       否则游戏里按方向键没反应。
 */
#ifndef KEY_H
#define KEY_H

#ifdef _WIN32
#include <conio.h>
static inline int key_hit(void) { return _kbhit(); }
static inline int key_get(void) {
    int c = _getch();
    if (c == 0 || c == 0xE0) {          /* 扩展键：方向键 / 功能键 */
        int c2 = _getch();
        switch (c2) {
            case 'H': return 'U';       /* 上 */
            case 'P': return 'D';       /* 下 */
            case 'K': return 'L';       /* 左 */
            case 'M': return 'R';       /* 右 */
            default:  return 0;
        }
    }
    if (c == 27) return 'Q';            /* ESC */
    return c;
}
#else
#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>

static inline void key_init(void) {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}
static inline int key_hit(void) {
    fd_set fds;
    struct timeval tv = {0, 0};
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}
static inline int key_get(void) {
    int c = getchar();
    if (c == 27) {            /* ESC [ A/B/C/D */
        if (getchar() != '[') return 'Q';
        switch (getchar()) {
            case 'A': return 'U';
            case 'B': return 'D';
            case 'C': return 'R';
            case 'D': return 'L';
            default:  return 0;
        }
    }
    return c;
}
#endif

/* 终端窗口高度（行数）。用来让棋盘高度自适应窗口，
 * 打印行数超出窗口就会触发终端滚动，滚动看起来就像画面在跳。
 * 拿不到时返回 0，调用方自己决定用默认高度。 */
#ifdef _WIN32
#include <windows.h>
static inline int term_rows(void) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
        return info.srWindow.Bottom - info.srWindow.Top + 1;
    return 0;
}
#else
#include <sys/ioctl.h>
static inline int term_rows(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0)
        return ws.ws_row;
    return 0;
}
#endif

/* 跨平台毫秒级延时，用于动画节奏（纯终端不需要精确） */
#ifdef _WIN32
static inline void key_delay(int ms) { Sleep(ms); }
#else
static inline void key_delay(int ms) { usleep(ms * 1000); }
#endif

#endif /* KEY_H */