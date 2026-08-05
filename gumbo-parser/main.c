/*
 * gumbo-parser Win16 DLL 入口模块
 * 编译器: Open Watcom 2.0
 */

#include <windows.h>

/* 所有导出函数均使用 far pascal 调用约定，并标记为 __export */
#define WIN16_API __far __pascal __export

/*--------------------------------------------------------------------
 * LibMain : DLL 初始化入口，由 Windows 在加载 DLL 时调用
 * 成功返回 1，否则返回 0
 *------------------------------------------------------------------*/
int WIN16_API LibMain(HINSTANCE hinst, WORD wDataSeg, WORD cbHeapSize,
                      LPSTR lpszCmdLine)
{
    /* 如果 DLL 指定了局部堆，解锁数据段以允许堆分配 */
    if (cbHeapSize > 0)
        UnlockData(0);

    /* 此处可添加额外的初始化操作，例如分配全局资源、初始化库状态等 */
    (void)hinst;
    (void)wDataSeg;
    (void)lpszCmdLine;

    return 1;   /* 初始化成功 */
}

/*--------------------------------------------------------------------
 * WEP : Windows Exit Procedure，在 DLL 卸载时执行清理（可选）
 * 参数 nExitType 为 WEP_FREE_DLL 或 WEP_SYSTEM_EXIT
 *------------------------------------------------------------------*/
int WIN16_API WEP(int nExitType)
{
    /* 在此释放资源，如不需要可留空 */
    (void)nExitType;
    return 1;   /* 成功 */
}
