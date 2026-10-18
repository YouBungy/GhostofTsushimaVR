#include <windows.h>
#include <stdio.h>
typedef DWORD (WINAPI *GS)(DWORD, void*);
int main(void){
  wchar_t p[MAX_PATH]; GetModuleFileNameW(NULL,p,MAX_PATH); wchar_t*s=wcsrchr(p,L'\\'); wcscpy(s+1,L"xinput1_4.dll");
  HMODULE h=LoadLibraryW(p); printf("load=%p err=%lu\n",(void*)h,GetLastError());
  if(!h) return 1;
  GS g=(GS)GetProcAddress(h,"XInputGetState"); unsigned char st[16]={0};
  printf("XInputGetState(0)=%lu (1167 = no controller is fine)\n", g?g(0,st):999);
  printf("ord100=%p ord108=%p\n",(void*)GetProcAddress(h,(LPCSTR)100),(void*)GetProcAddress(h,(LPCSTR)108));
  Sleep(2500); return 0;
}
