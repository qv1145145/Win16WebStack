#include <windows.h>
#include "src\libunicode-table.h"
#include "src\debuglog.h"

int CALLBACK LibMain(HINSTANCE hinst, WORD wDataSeg, WORD cbHeapSize, LPSTR lpszCmdLine)
{
    (void)wDataSeg; (void)hinst; (void)lpszCmdLine;

    LOG_INIT();

    if (cbHeapSize > 0)
        UnlockData(0);

    /* 初始化 Unicode 表（如果分配失败，清理已初始化的资源） */
    if (!unicode_alloc_tables())
        return 0;

    return 1;
}

int __export __far __pascal WEP(int nExitType)
{
    (void)nExitType;

    /* 按相反顺序销毁 */
    unicode_free_tables();

    return 1;
}