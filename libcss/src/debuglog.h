/* debuglog.h - Win16 调试日志系统 */

#ifndef DEBUGLOG_H
#define DEBUGLOG_H

/* ---- 指针宏 ---- */
#define FARPTR_SEG(p)  ((unsigned)((unsigned long)(p) >> 16))
#define FARPTR_OFF(p)  ((unsigned)((unsigned long)(p) & 0xFFFF))

/* ---- 调试开关 ---- */
#define DEBUG_LOG_ENABLED         /* 注释此行可关闭所有调试输出 */
// #define DEBUG_LOG_TO_MESSAGEBOX   /* 注释此行可关闭 MessageBox 弹窗 */

/* ---- 日志文件路径 ---- */
#define DEBUG_LOG_PATH  "D:\\DEBUG.LOG"

/* ---- 初始化日志（清空旧文件） ---- */
void LOG_INIT(void);

/* ---- 写入日志 ---- */
void LOG( const char* func, const char* fmt, ... );

#endif /* DEBUGLOG_H */
