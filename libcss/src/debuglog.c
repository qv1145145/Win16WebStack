/* debuglog.c - Win16 调试日志系统 */
#include "debuglog.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

/* ---- 初始化日志（清空旧文件） ---- */
void LOG_INIT(void)
{
#ifdef DEBUG_LOG_ENABLED
    HFILE hf;
    OFSTRUCT of;

    /* 打开文件（创建并截断），写入文件头 */
    hf = OpenFile( DEBUG_LOG_PATH, &of, OF_CREATE | OF_WRITE );
    if ( hf != HFILE_ERROR )
    {
        const char* header = "=== Libcss DLL Debug Log ===\r\n";
        _lwrite( hf, header, lstrlen( (LPCSTR)header ) );
        _lclose( hf );
    }
#endif
}

/* ---- 写入日志 ---- */
void LOG( const char* func, const char* fmt, ... )
{
#ifdef DEBUG_LOG_ENABLED
    char buf[512];
    char logbuf[600];
    va_list ap;
    HFILE hf;
    OFSTRUCT of;

    /* 格式化用户消息 */
    va_start( ap, fmt );
    wvsprintf( buf, fmt, ap );
    va_end( ap );

    /* 组装完整日志行：函数名 + 消息 */
    wsprintf( logbuf, "[%s] %s\r\n", func, buf );

    /* 追加写入文件 */
    hf = OpenFile( DEBUG_LOG_PATH, &of, OF_WRITE );
    if ( hf != HFILE_ERROR )
    {
        /* 移动到文件末尾 */
        _llseek( hf, 0, 2 );   /* SEEK_END = 2 */
        _lwrite( hf, logbuf, lstrlen( (LPCSTR)logbuf ) );
        _lclose( hf );
    }

#ifdef DEBUG_LOG_TO_MESSAGEBOX
    /* 同时弹窗（可选） */
    MessageBox( NULL, logbuf, "DEBUG", MB_OK );
#endif
#endif
}
