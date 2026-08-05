/* win16_compat.h - Win16 compatibility stubs for QuickJS */

#ifndef WIN16_COMPAT_H
#define WIN16_COMPAT_H

#ifdef _WIN16

#include <windows.h>

#ifndef SHORT
typedef short SHORT;
#endif

/* 控制台相关结构体定义 */
typedef struct _COORD {
    SHORT X;
    SHORT Y;
} COORD;

typedef struct _SMALL_RECT {
    SHORT Left;
    SHORT Top;
    SHORT Right;
    SHORT Bottom;
} SMALL_RECT;

typedef struct _CONSOLE_SCREEN_BUFFER_INFO {
    COORD      dwSize;
    COORD      dwCursorPosition;
    WORD       wAttributes;
    SMALL_RECT srWindow;
    COORD      dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO;

/* 常量定义 */
#define MAXIMUM_WAIT_OBJECTS           1

#define ENABLE_WINDOW_INPUT            0
#define ENABLE_PROCESSED_OUTPUT        0
#define ENABLE_WRAP_AT_EOL_OUTPUT      0
#define __ENABLE_VIRTUAL_TERMINAL_INPUT     0
#define __ENABLE_VIRTUAL_TERMINAL_PROCESSING 0

#define WAIT_OBJECT_0   0
#define WAIT_TIMEOUT    0x00000102L
#define WAIT_FAILED     0xFFFFFFFF

/* ---------- 函数桩 ---------- */

/* Sleep 模拟（忙等） */
static __inline void Win16_Sleep(DWORD dwMilliseconds) {
    DWORD start = GetCurrentTime();
    while (GetCurrentTime() - start < dwMilliseconds)
        ;
}
#define Sleep(dw)  Win16_Sleep(dw)

/* 控制台 API 桩 */
static __inline BOOL Win16_GetConsoleScreenBufferInfo(HANDLE h,
                                                      CONSOLE_SCREEN_BUFFER_INFO *info) {
    (void)h;
    memset(info, 0, sizeof(*info));
    info->dwSize.X = 80;
    info->dwSize.Y = 25;
    info->srWindow.Right  = 79;
    info->srWindow.Bottom = 24;
    return TRUE;
}
#define GetConsoleScreenBufferInfo  Win16_GetConsoleScreenBufferInfo

static __inline BOOL Win16_SetConsoleMode(HANDLE h, DWORD mode) {
    (void)h; (void)mode;
    return TRUE;
}
#define SetConsoleMode  Win16_SetConsoleMode



/* WaitForMultipleObjects 简单返回超时（避免阻塞） */
static __inline DWORD Win16_WaitForMultipleObjects(DWORD count,
                                                   const HANDLE *handles,
                                                   BOOL wait_all,
                                                   DWORD timeout) {
    (void)handles; (void)wait_all; (void)timeout;
    if (count == 0) return WAIT_FAILED;
    /* 总是返回超时，让事件循环继续轮询 */
    return WAIT_TIMEOUT;
}
#define WaitForMultipleObjects  Win16_WaitForMultipleObjects

static __inline FILE *Win16_popen(const char *command, const char *type) {
    (void)command; (void)type;
    return NULL;   // 模拟失败
}
#define popen  Win16_popen

static __inline int Win16_pclose(FILE *stream) {
    (void)stream;
    return -1;
}
#define pclose Win16_pclose

#endif /* _WIN16 */

#endif /* WIN16_COMPAT_H */
