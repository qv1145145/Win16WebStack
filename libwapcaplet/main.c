#include <windows.h>

int CALLBACK LibMain(HINSTANCE hinst, WORD wDataSeg, WORD cbHeapSize, LPSTR lpszCmdLine)
{
    if (cbHeapSize > 0)
        UnlockData(0);
    return 1;
}

int __export __far __pascal WEP(int nExitType)
{
    (void)nExitType;
    return 1;
}
