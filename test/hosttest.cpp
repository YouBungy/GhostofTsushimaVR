#include <windows.h>
#include <stdio.h>
#include "../src/gotvr_shared.h"
int main(void){ wchar_t nm[96]; gvName(nm,96,GetCurrentProcessId(),NULL);
 HANDLE m=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(GvShared),nm); GvShared*S=(GvShared*)MapViewOfFile(m,FILE_MAP_ALL_ACCESS,0,0,sizeof(GvShared));
 memset(S,0,sizeof*S); S->magic=GV_MAGIC; S->version=GV_VERSION; S->gamePid=GetCurrentProcessId(); S->mode=2;
 wchar_t exe[MAX_PATH]; GetModuleFileNameW(NULL,exe,MAX_PATH); wcscpy(wcsrchr(exe,L'\\')+1,L"GoTVR\\GoTVRHost.exe"); wchar_t cmd[512]; swprintf(cmd,512,L"\"%ls\" %lu",exe,GetCurrentProcessId()); STARTUPINFOW si={sizeof si}; PROCESS_INFORMATION pi;
 if(!CreateProcessW(exe,cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi)){printf("spawn fail %lu\n",GetLastError());return 1;}
 DWORD w=WaitForSingleObject(pi.hProcess,20000); DWORD c=0; GetExitCodeProcess(pi.hProcess,&c);
 printf("host wait=%lu exit=%lu state=%ld msg=%s hostPid=%u\n",w,c,S->hostState,S->hostMsg,S->hostPid); return 0; }
