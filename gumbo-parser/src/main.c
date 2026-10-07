/*
 * gumbo-parser Win16 DLL Èë¿ÚÄ£¿é
 * ±àÒëÆ÷: Open Watcom 2.0
 */

#include <windows.h>

int CALLBACK LibMain(HINSTANCE hinst, WORD wDataSeg, WORD cbHeapSize,
                      LPSTR lpszCmdLine)
{
    if (cbHeapSize > 0)
        UnlockData(0);

    (void)hinst;
    (void)wDataSeg;
    (void)lpszCmdLine;
	
    return 1;
}

int CALLBACK WEP(int nExitType)
{
    (void)nExitType;
    return 1;
}
