#include <windows.h>
#include "css_malloc.h"

int CALLBACK LibMain(HINSTANCE hinst, WORD wDataSeg,
                     WORD cbHeapSize, LPSTR lpszCmdLine)
{
    if (cbHeapSize > 0)
        UnlockData(0);

    /* 初始化内存池：63 KB, 2048 控制块, 分割阈值 16 字节, 4 字节对齐 */
    if (!css_heap_init(63 * 1024, 2048, 16, 4))
        return 0;

    return 1;
}

int __export __far __pascal WEP(int nExitType)
{
    css_heap_destroy();
    return 1;
}
