#include <windows.h>
#include <stdio.h>
typedef int (*ST)(const wchar_t*);
int main(int c, char**v){ HMODULE h=LoadLibraryA("gotvr_selftest.dll"); if(!h){printf("load fail %lu\n",GetLastError());return 9;}
 ST f=(ST)GetProcAddress(h,"GoTVR_SelfTest"); int r=f(L"Z:\\home\\claude\\got\\bin\\GhostOfTsushima.exe"); printf("selftest=%d\n",r); return r; }
