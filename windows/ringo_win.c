/* =====================================================================
   RINGO R-470 - emulador para Windows (Win32 puro, sem bibliotecas)
   RetroLabs - JL Martins e Luiz Pacheco - 2026

   Compilar (MinGW):
     gcc -O2 -o RINGOW.EXE ringo_win.c ringo_ui.c ringo_core.c -mwindows -lgdi32 -lwinmm
   ===================================================================== */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include "ringo_core.h"
#include "ringo_ui.h"
#include "ringo_plat.h"
#include "rom_ringo.h"

static ringo_t M;
static HWND hwnd;
static int quitting=0;
static char exedir[MAX_PATH];
static char cfgpath[MAX_PATH];

/* ----------------------------- plataforma ----------------------------- */
int plat_list(const char*dir,void(*cb)(const char*,int,void*),void*u){
  if(!dir[0]){DWORD d=GetLogicalDrives();int i;for(i=0;i<26;i++)if(d&(1u<<i)){char n[4]={(char)('A'+i),':',0,0};cb(n,1,u);}return 1;}
  {char pat[MAX_PATH+4];WIN32_FIND_DATAA fd;HANDLE h;size_t n;
   strncpy(pat,dir,MAX_PATH);pat[MAX_PATH-1]=0;n=strlen(pat);if(n&&pat[n-1]!='\\')strcat(pat,"\\");strcat(pat,"*");
   h=FindFirstFileA(pat,&fd);if(h==INVALID_HANDLE_VALUE)return 0;
   do{if(fd.cFileName[0]=='.')continue;if(fd.dwFileAttributes&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM))continue;
     cb(fd.cFileName,(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)?1:0,u);}while(FindNextFileA(h,&fd));
   FindClose(h);return 1;}}
long plat_load(const char*path,uint8_t*buf,long max){FILE*f=fopen(path,"rb");long n;if(!f)return -1;n=(long)fread(buf,1,max,f);fclose(f);return n;}
int plat_save(const char*path,const uint8_t*buf,long len){FILE*f=fopen(path,"wb");if(!f)return 0;fwrite(buf,1,len,f);fclose(f);return 1;}
int plat_del(const char*path){return DeleteFileA(path)?1:0;}
char plat_sep(void){return '\\';}
const char*plat_home(void){return exedir;}
int plat_isroot(const char*dir){return dir[0]==0;}
int plat_cfg_read(char*b,int max){FILE*f=fopen(cfgpath,"rb");int n;if(!f)return 0;n=(int)fread(b,1,max,f);b[n]=0;fclose(f);return n;}
int plat_cfg_write(const char*b){FILE*f=fopen(cfgpath,"wb");if(!f)return 0;fputs(b,f);fclose(f);return 1;}
void plat_quit(void){quitting=1;}
/* codigo fisico = scancode do PC (+ 0x100 para teclas estendidas) */
void plat_keyname(uint32_t raw,char*out,int max){LONG lp=(LONG)(((raw&0xff)<<16)|((raw&0x100)?(1<<24):0));
  if(!GetKeyNameTextA(lp,out,max))snprintf(out,max,"SC %03X",(unsigned)raw);}
static uint32_t vksc(UINT vk,int ext){UINT s=MapVirtualKeyA(vk,0);return s?(s|(ext?0x100:0)):0;}
uint32_t plat_default_raw(int rk){
  static const char L[]="\0ZXCVASDFGQWERT1234509876POIUY";  /* ate RK_Y */
  if(rk>RK_SHIFT&&rk<=RK_Y)return vksc((UINT)(unsigned char)L[rk],0);
  switch(rk){case RK_SHIFT:return vksc(VK_LSHIFT,0);case RK_ENTER:return vksc(VK_RETURN,0);
    case RK_L:return vksc('L',0);case RK_K:return vksc('K',0);case RK_J:return vksc('J',0);case RK_H:return vksc('H',0);
    case RK_SPACE:return vksc(VK_SPACE,0);case RK_DOT:return vksc(VK_OEM_PERIOD,0);case RK_M:return vksc('M',0);
    case RK_N:return vksc('N',0);case RK_B:return vksc('B',0);case RK_DELETE:return vksc(VK_BACK,0);
    case RK_LEFT:return vksc(VK_LEFT,1);case RK_RIGHT:return vksc(VK_RIGHT,1);case RK_UP:return vksc(VK_UP,1);case RK_DOWN:return vksc(VK_DOWN,1);
    case RK_FUNCTION:return vksc(VK_TAB,0);case RK_GRAPHICS:return vksc(VK_CAPITAL,0);case RK_INVVIDEO:return vksc(VK_INSERT,1);
    case RK_EDIT:return vksc(VK_HOME,1);}
  return 0;}

/* ----------------------------- teclado -------------------------------- */
static int vk2hk(WPARAM vk,int scan){
  if(vk>='A'&&vk<='Z')return (int)vk;if(vk>='0'&&vk<='9')return (int)vk;
  if(vk>=VK_F1&&vk<=VK_F12)return HK_F1+(int)(vk-VK_F1);
  switch(vk){case VK_ESCAPE:return HK_ESC;case VK_RETURN:return HK_ENTER;case VK_BACK:return HK_BACK;case VK_TAB:return HK_TAB;
    case VK_SPACE:return HK_SPACE;case VK_SHIFT:return scan==0x36?HK_RSHIFT:HK_LSHIFT;case VK_CONTROL:return HK_CTRL;case VK_MENU:return HK_ALT;
    case VK_CAPITAL:return HK_CAPS;case VK_INSERT:return HK_INS;case VK_DELETE:return HK_DEL;case VK_HOME:return HK_HOME;case VK_END:return HK_END;
    case VK_PRIOR:return HK_PGUP;case VK_NEXT:return HK_PGDN;case VK_UP:return HK_UP;case VK_DOWN:return HK_DOWN;case VK_LEFT:return HK_LEFT;case VK_RIGHT:return HK_RIGHT;}
  return HK_OTHER;}
static void keyev(WPARAM vk,LPARAM lp,int down){int scan=(int)((lp>>16)&0xff),ext=(int)((lp>>24)&1);uint32_t raw=(uint32_t)scan|(ext?0x100:0);
  int hk=vk2hk(vk,scan),ch=0;
  if(down&&hk==HK_OTHER){BYTE ks[256];WCHAR buf[4];int n;GetKeyboardState(ks);n=ToUnicode((UINT)vk,(UINT)scan,ks,buf,4,0);
    if(n==1&&buf[0]>=32&&buf[0]<127)ch=buf[0];else if(n<0)ToUnicode((UINT)vk,(UINT)scan,ks,buf,4,0);}
  else if(down&&hk>='A'&&hk<='Z')ch=hk+32;else if(down&&hk>='0'&&hk<='9'){BYTE ks[256];WCHAR buf[4];GetKeyboardState(ks);
    if(ToUnicode((UINT)vk,(UINT)scan,ks,buf,4,0)==1&&buf[0]>=32&&buf[0]<127)ch=buf[0];}
  if(!raw)raw=(uint32_t)vk|0x200;
  ui_key(hk,ch,raw,down);}

/* ----------------------------- video ---------------------------------- */
static struct { BITMAPINFOHEADER h; RGBQUAD pal[2]; } bmi;
static void paint(HDC dc){RECT r;int cw,chh,w,h,x,y;uint32_t ink,paper;const uint8_t*fb=ui_fb();
  GetClientRect(hwnd,&r);cw=r.right;chh=r.bottom;
  w=cw;h=w*3/4;if(h>chh){h=chh;w=h*4/3;}x=(cw-w)/2;y=(chh-h)/2;
  ui_colors(&ink,&paper);
  bmi.pal[0].rgbRed=(BYTE)(paper>>16);bmi.pal[0].rgbGreen=(BYTE)(paper>>8);bmi.pal[0].rgbBlue=(BYTE)paper;
  bmi.pal[1].rgbRed=(BYTE)(ink>>16);bmi.pal[1].rgbGreen=(BYTE)(ink>>8);bmi.pal[1].rgbBlue=(BYTE)ink;
  SetStretchBltMode(dc,COLORONCOLOR);
  StretchDIBits(dc,x,y,w,h,0,0,UI_W,UI_H,fb,(BITMAPINFO*)&bmi,DIB_RGB_COLORS,SRCCOPY);
  if(x>0){RECT a={0,0,x,chh},b={x+w,0,cw,chh};FillRect(dc,&a,(HBRUSH)GetStockObject(BLACK_BRUSH));FillRect(dc,&b,(HBRUSH)GetStockObject(BLACK_BRUSH));}
  if(y>0){RECT a={0,0,cw,y},b={0,y+h,cw,chh};FillRect(dc,&a,(HBRUSH)GetStockObject(BLACK_BRUSH));FillRect(dc,&b,(HBRUSH)GetStockObject(BLACK_BRUSH));}}

static LRESULT CALLBACK wndproc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
  switch(msg){
    case WM_KEYDOWN:case WM_SYSKEYDOWN:if(wp==VK_F4&&(GetKeyState(VK_MENU)&0x8000))break;keyev(wp,lp,1);return 0;
    case WM_KEYUP:case WM_SYSKEYUP:keyev(wp,lp,0);return 0;
    case WM_SYSCHAR:case WM_CHAR:return 0;
    case WM_KILLFOCUS:ui_focus_lost();return 0;
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);paint(dc);EndPaint(h,&ps);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_CLOSE:quitting=1;return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;}
  return DefWindowProcA(h,msg,wp,lp);}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cl,int show){WNDCLASSA wc;RECT r={0,0,1024,768};MSG msg;
  LARGE_INTEGER freq,t0,now;double next;char*p;(void)hp;(void)cl;
  GetModuleFileNameA(NULL,exedir,MAX_PATH);p=strrchr(exedir,'\\');if(p)p[0]=0;
  snprintf(cfgpath,sizeof cfgpath,"%s\\RINGO.CFG",exedir);
  memset(&bmi,0,sizeof bmi);bmi.h.biSize=sizeof(BITMAPINFOHEADER);bmi.h.biWidth=UI_W;bmi.h.biHeight=-UI_H;
  bmi.h.biPlanes=1;bmi.h.biBitCount=1;bmi.h.biCompression=BI_RGB;bmi.h.biClrUsed=2;
  memset(&wc,0,sizeof wc);wc.lpfnWndProc=wndproc;wc.hInstance=hi;wc.hCursor=LoadCursor(NULL,IDC_ARROW);
  wc.hIcon=LoadIcon(GetModuleHandle(NULL),MAKEINTRESOURCE(1));wc.lpszClassName="RingoR470";RegisterClassA(&wc);
  AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
  hwnd=CreateWindowA("RingoR470","Ringo R-470 - RetroLabs   (F1 ajuda)",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,
    r.right-r.left,r.bottom-r.top,NULL,NULL,hi,NULL);
  ShowWindow(hwnd,show);
  ringo_init(&M,RINGO_ROM);ui_init(&M);
  timeBeginPeriod(1);QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&t0);next=0;
  while(!quitting){double el;
    while(PeekMessageA(&msg,NULL,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)quitting=1;TranslateMessage(&msg);DispatchMessageA(&msg);}
    QueryPerformanceCounter(&now);el=(double)(now.QuadPart-t0.QuadPart)*1000.0/(double)freq.QuadPart;
    if(el<next){DWORD w=(DWORD)(next-el);if(w>1)Sleep(w-1);continue;}
    next+=1000.0/60.0;if(el-next>100)next=el;         /* nao tenta recuperar atrasos grandes */
    ui_frame();{HDC dc=GetDC(hwnd);paint(dc);ReleaseDC(hwnd,dc);}}
  timeEndPeriod(1);DestroyWindow(hwnd);return 0;}
