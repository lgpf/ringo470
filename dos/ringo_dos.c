/* =====================================================================
   RINGO R-470 - emulador para MS-DOS puro (sem Windows nem DOSBox)
   RetroLabs - JL Martins e Luiz Pacheco - 2026

   Compilar com DJGPP (gcc para DOS, 32 bits com DPMI):
     gcc -O2 -o RINGODOS.EXE ringo_dos.c ringo_ui.c ringo_core.c
   Precisa de CWSDPMI.EXE na mesma pasta (ou outro servidor DPMI).
   Video VGA 640x480 (modo 12h): a tela de 512x384 fica centralizada.
   Teclado pela interrupcao 9 (scancodes); 60 quadros/s pelo uclock().
   ===================================================================== */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dir.h>
#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <pc.h>
#include <sys/movedata.h>
#include <time.h>
#include "ringo_core.h"
#include "ringo_ui.h"
#include "ringo_plat.h"
#include "rom_ringo.h"

static ringo_t M;
static int quitting=0;
static char exedir[260],cfgpath[280];

/* ----------------------------- plataforma ----------------------------- */
int plat_list(const char*dir,void(*cb)(const char*,int,void*),void*u){
  if(!dir[0]){int d;for(d='C';d<='Z';d++){char n[4]={(char)d,':','\\',0};if(access(n,D_OK)==0){n[2]=0;cb(n,1,u);}}return 1;}
  {char pat[300];struct ffblk f;size_t n;int r;
   strncpy(pat,dir,260);pat[259]=0;n=strlen(pat);if(n&&pat[n-1]!='\\')strcat(pat,"\\");strcat(pat,"*.*");
   r=findfirst(pat,&f,FA_DIREC|FA_RDONLY|FA_ARCH);if(r)return 0;
   while(!r){if(f.ff_name[0]!='.'&&!(f.ff_attrib&(FA_HIDDEN|FA_SYSTEM)))cb(f.ff_name,(f.ff_attrib&FA_DIREC)?1:0,u);r=findnext(&f);}
   return 1;}}
long plat_load(const char*path,uint8_t*buf,long max){FILE*f=fopen(path,"rb");long n;if(!f)return -1;n=(long)fread(buf,1,max,f);fclose(f);return n;}
int plat_save(const char*path,const uint8_t*buf,long len){FILE*f=fopen(path,"wb");if(!f)return 0;fwrite(buf,1,len,f);fclose(f);return 1;}
int plat_del(const char*path){return remove(path)==0;}
char plat_sep(void){return '\\';}
const char*plat_home(void){return exedir;}
int plat_isroot(const char*dir){return dir[0]==0;}
int plat_cfg_read(char*b,int max){FILE*f=fopen(cfgpath,"rb");int n;if(!f)return 0;n=(int)fread(b,1,max,f);b[n]=0;fclose(f);return n;}
int plat_cfg_write(const char*b){FILE*f=fopen(cfgpath,"wb");if(!f)return 0;fputs(b,f);fclose(f);return 1;}
void plat_quit(void){quitting=1;}

/* codigo fisico = scancode do PC (conjunto 1), + 0x100 nas teclas com prefixo E0 */
static const char *SCN[0x60]={0,"Esc","1","2","3","4","5","6","7","8","9","0","-","=","Backspace","Tab",
 "Q","W","E","R","T","Y","U","I","O","P","[","]","Enter","Ctrl","A","S",
 "D","F","G","H","J","K","L","Ç ;","~ '","` '","Shift esq","] \\","Z","X","C","V",
 "B","N","M",",",".","; /","Shift dir","* num","Alt","Espaco","Caps Lock","F1","F2","F3","F4","F5",
 "F6","F7","F8","F9","F10","Num Lock","Scroll","7 num","8 num","9 num","- num","4 num","5 num","6 num","+ num","1 num",
 "2 num","3 num","0 num",". num",0,0,"\\ |","F11","F12",0,0,0,0,0,0,0};
void plat_keyname(uint32_t raw,char*out,int max){unsigned s=raw&0xff;
  if(raw&0x100){const char*n=0;switch(s){case 0x48:n="Cima";break;case 0x50:n="Baixo";break;case 0x4B:n="Esquerda";break;case 0x4D:n="Direita";break;
      case 0x47:n="Home";break;case 0x4F:n="End";break;case 0x49:n="PgUp";break;case 0x51:n="PgDn";break;case 0x52:n="Insert";break;case 0x53:n="Delete";break;
      case 0x1C:n="Enter num";break;case 0x1D:n="Ctrl dir";break;case 0x38:n="Alt Gr";break;case 0x35:n="/ num";break;}
    if(n){snprintf(out,max,"%s",n);return;}}
  if(s==0x73){snprintf(out,max,"/ ?");return;}
  if(s<0x60&&SCN[s])snprintf(out,max,"%s",SCN[s]);else snprintf(out,max,"SC %03X",(unsigned)raw);}
uint32_t plat_default_raw(int rk){
  static const uint8_t T[RK_COUNT]={0x2A,0x2C,0x2D,0x2E,0x2F, 0x1E,0x1F,0x20,0x21,0x22, 0x10,0x11,0x12,0x13,0x14,
    0x02,0x03,0x04,0x05,0x06, 0x0B,0x0A,0x09,0x08,0x07, 0x19,0x18,0x17,0x16,0x15, 0x1C,0x26,0x25,0x24,0x23,
    0x39,0x34,0x32,0x31,0x30, 0x0E,0x4B,0x4D,0x48,0x50, 0x0F,0x3A,0x52,0x47};
  uint32_t r;if(rk<0||rk>=RK_COUNT)return 0;r=T[rk];
  if(rk==RK_LEFT||rk==RK_RIGHT||rk==RK_UP||rk==RK_DOWN||rk==RK_INVVIDEO||rk==RK_EDIT)r|=0x100;
  return r;}

/* ----------------------------- teclado (INT 9) ------------------------ */
#define QN 64
static volatile uint16_t kq[QN];static volatile int kqh=0,kqt=0;static volatile int kext=0;
static _go32_dpmi_seginfo oldkb,newkb;
static void kbint(void){uint8_t s=inportb(0x60);
  if(s==0xE0)kext=1;else if(s==0xE1)kext=0;
  else{int n=(kqh+1)%QN;if(n!=kqt){kq[kqh]=(uint16_t)(s|(kext?0x100:0));kqh=n;}kext=0;}
  outportb(0x20,0x20);}
static void kbint_end(void){}
static void kb_install(void){
  _go32_dpmi_lock_code(kbint,(char*)kbint_end-(char*)kbint);
  _go32_dpmi_lock_data((void*)kq,sizeof kq);_go32_dpmi_lock_data((void*)&kqh,sizeof kqh);
  _go32_dpmi_lock_data((void*)&kqt,sizeof kqt);_go32_dpmi_lock_data((void*)&kext,sizeof kext);
  _go32_dpmi_get_protected_mode_interrupt_vector(9,&oldkb);
  newkb.pm_offset=(unsigned long)kbint;newkb.pm_selector=_go32_my_cs();
  _go32_dpmi_allocate_iret_wrapper(&newkb);_go32_dpmi_set_protected_mode_interrupt_vector(9,&newkb);}
static void kb_remove(void){_go32_dpmi_set_protected_mode_interrupt_vector(9,&oldkb);_go32_dpmi_free_iret_wrapper(&newkb);}

/* caracteres digitados: US e ABNT2 (sem e com Shift) */
static const char US0[0x60]="\0\0331234567890-=\b\tqwertyuiop[]\r\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
static const char US1[0x60]="\0\033!@#$%^&*()_+\b\tQWERTYUIOP{}\r\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";
static const char AB0[0x60]="\0\0331234567890-=\b\tqwertyuiop'[\r\0asdfghjkl\0~'\0]zxcvbnm,.;\0*\0 ";
static const char AB1[0x60]="\0\033!@#$%\0&*()_+\b\tQWERTYUIOP`{\r\0ASDFGHJKL\0^\"\0}ZXCVBNM<>:\0*\0 ";
static int shl=0,shr=0;
static int sc2hk(unsigned s,int ext){
  if(ext){switch(s){case 0x48:return HK_UP;case 0x50:return HK_DOWN;case 0x4B:return HK_LEFT;case 0x4D:return HK_RIGHT;
      case 0x47:return HK_HOME;case 0x4F:return HK_END;case 0x49:return HK_PGUP;case 0x51:return HK_PGDN;case 0x52:return HK_INS;case 0x53:return HK_DEL;
      case 0x1C:return HK_ENTER;case 0x1D:return HK_CTRL;case 0x38:return HK_ALT;}return HK_OTHER;}
  if(s>=0x3B&&s<=0x44)return HK_F1+(int)(s-0x3B);
  switch(s){case 0x57:return HK_F11;case 0x58:return HK_F12;case 0x01:return HK_ESC;case 0x1C:return HK_ENTER;case 0x0E:return HK_BACK;
    case 0x0F:return HK_TAB;case 0x39:return HK_SPACE;case 0x2A:return HK_LSHIFT;case 0x36:return HK_RSHIFT;case 0x1D:return HK_CTRL;
    case 0x38:return HK_ALT;case 0x3A:return HK_CAPS;
    case 0x48:return HK_UP;case 0x50:return HK_DOWN;case 0x4B:return HK_LEFT;case 0x4D:return HK_RIGHT;   /* teclado numerico */
    case 0x47:return HK_HOME;case 0x4F:return HK_END;case 0x49:return HK_PGUP;case 0x51:return HK_PGDN;case 0x52:return HK_INS;case 0x53:return HK_DEL;}
  if(s<0x40){char c=US0[s];if(c>='a'&&c<='z')return c-32;if(c>='0'&&c<='9')return c;}
  return HK_OTHER;}
static void kb_poll(void){
  while(kqt!=kqh){uint16_t v=kq[kqt];int down,ext,hk,ch=0;unsigned s;kqt=(kqt+1)%QN;
    down=!(v&0x80);ext=(v&0x100)!=0;s=v&0x7f;
    if(!ext&&s==0x2A)shl=down;if(!ext&&s==0x36)shr=down;
    hk=sc2hk(s,ext);
    if(down&&!ext&&s<0x40&&s!=0x01&&s!=0x0E&&s!=0x0F&&s!=0x1C){int sh=shl||shr,ab=(ui_kblayout()==0);
      ch=(unsigned char)(ab?(sh?AB1[s]:AB0[s]):(sh?US1[s]:US0[s]));if(ch<32||ch>126)ch=0;}
    if(down&&!ext&&s==0x73){int sh=shl||shr;ch=sh?'?':'/';}            /* tecla / ? do ABNT2 */
    if(down&&!ext&&s==0x27&&ui_kblayout()==0)ch=0;                     /* Ç */
    ui_key(hk,ch,(uint32_t)s|(ext?0x100:0),down);}}

/* ----------------------------- video VGA modo 12h --------------------- */
static uint8_t last[UI_H*(UI_W/8)];static int full=1;static uint32_t lastink=1,lastpaper=1;
static void setmode(int m){__dpmi_regs r;memset(&r,0,sizeof r);r.x.ax=(unsigned short)m;__dpmi_int(0x10,&r);}
static void dac(int i,uint32_t c){outportb(0x3C8,(unsigned char)i);outportb(0x3C9,(unsigned char)((c>>18)&63));
  outportb(0x3C9,(unsigned char)((c>>10)&63));outportb(0x3C9,(unsigned char)((c>>2)&63));}
static void vga_init(void){int i;setmode(0x12);
  for(i=0;i<16;i++){inportb(0x3DA);outportb(0x3C0,(unsigned char)i);outportb(0x3C0,(unsigned char)(i?63:0));}  /* 0 = fundo, resto = frente */
  inportb(0x3DA);outportb(0x3C0,0x20);
  outportb(0x3C4,2);outportb(0x3C5,0x0F);            /* escreve nos 4 planos */
  outportb(0x3CE,3);outportb(0x3CF,0);outportb(0x3CE,5);outportb(0x3CF,0);outportb(0x3CE,8);outportb(0x3CF,0xFF);
  {static uint8_t z[80];for(i=0;i<480;i++)dosmemput(z,80,0xA0000+i*80);}
  full=1;}
static void vga_show(void){const uint8_t*fb=ui_fb();uint32_t ink,paper;int y;
  ui_colors(&ink,&paper);
  if(ink!=lastink||paper!=lastpaper){dac(0,paper);dac(63,ink);lastink=ink;lastpaper=paper;}
  for(y=0;y<UI_H;y++){const uint8_t*row=fb+y*(UI_W/8);
    if(full||memcmp(row,last+y*(UI_W/8),UI_W/8)){dosmemput(row,UI_W/8,0xA0000+(y+48)*80+8);memcpy(last+y*(UI_W/8),row,UI_W/8);}}
  full=0;}

int main(int argc,char**argv){uclock_t t0,per;char*p;(void)argc;
  strncpy(exedir,argv[0],250);exedir[250]=0;for(p=exedir;*p;p++)if(*p=='/')*p='\\';
  p=strrchr(exedir,'\\');if(p)*p=0;else strcpy(exedir,".");
  snprintf(cfgpath,sizeof cfgpath,"%s\\RINGO.CFG",exedir);
  ringo_init(&M,RINGO_ROM);ui_init(&M);
  vga_init();kb_install();
  per=UCLOCKS_PER_SEC/60;t0=uclock();
  while(!quitting){uclock_t now;
    kb_poll();
    now=uclock();if(now-t0<per)continue;
    t0+=per;if(now-t0>per*6)t0=now;                 /* nao tenta recuperar atrasos grandes */
    ui_frame();vga_show();}
  kb_remove();setmode(3);
  printf("Ringo R-470 - RetroLabs - JL Martins e Luiz Pacheco - 2026\n");
  return 0;}
