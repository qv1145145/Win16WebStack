#include <windows.h>

int CALLBACK LibMain(HINSTANCE hinst, WORD wDataSeg,
                     WORD cbHeapSize, LPSTR lpszCmdLine)
{
    (void)hinst; (void)wDataSeg;
    (void)cbHeapSize; (void)lpszCmdLine;
    if (cbHeapSize > 0)
        UnlockData(0);

    return 1;
}

int __export __far __pascal WEP(int nExitType)
{
    (void)nExitType;
    return 1;
}
