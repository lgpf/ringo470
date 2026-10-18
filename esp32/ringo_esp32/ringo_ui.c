/* RINGO R-470 - interface do emulador
   F1 ajuda, F5 configuracao, F6 tabela de teclas, F9 cartucho (estilo LIST.COM), F10 reset,
   Shift+F10 sai; fita K7 em arquivos .CAS.
   RetroLabs - JL Martins e Luiz Pacheco - 2026 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "ringo_ui.h"
#include "ringo_plat.h"
#include "ui_font.h"

#define COLS 64
#define ROWS 32          /* linhas de texto de 12 pixels */
#define CH 12
#ifdef ESP_PLATFORM
#define MAXENT 128
#endif
#ifndef MAXENT
#define MAXENT 256
#endif
#define NAMEL 48

enum { UI_NONE, UI_HELP, UI_KEYS, UI_CFG, UI_KEYMAP, UI_KEYWAIT, UI_FILES, UI_CASDIR, UI_CASPICK, UI_CONFIRM };
enum { KB_ABNT, KB_US, KB_PERS };

static ringo_t *M;
static uint8_t fb[UI_W*UI_H/8];
static uint8_t scr[RINGO_W*RINGO_H/8];
static int mode=UI_NONE, retmode=UI_NONE;
static char msg[COLS+1]; static int msgt=0;
static int hostshift=0;
static uint8_t *cartbuf=NULL; static long cartlen=0; static char cartname[NAMEL]="";

/* ------------------------------ configuracao ------------------------- */
static struct { int color, invert, kb; char casdir[256], bindir[256]; uint32_t map[RK_COUNT]; } cfg;
static const char *colname[]={"Verde","Branco","Cinza","Ambar"};
static const uint32_t colrgb[]={0x33FF66,0xF0F0F0,0xA8A8A8,0xFFB000};
static const char *kbname[]={"ABNT","US","Personalizado"};

static void cfg_default(void){int k;memset(&cfg,0,sizeof cfg);cfg.color=1;cfg.invert=0;cfg.kb=KB_ABNT;
  strcpy(cfg.casdir,plat_home());strcpy(cfg.bindir,plat_home());for(k=0;k<RK_COUNT;k++)cfg.map[k]=plat_default_raw(k);}
static char*cfgb(void){static char*p;if(!p)p=(char*)malloc(4096);return p;}
static void cfg_save(void){char*b=cfgb();int k,n;if(!b)return;
  n=sprintf(b,"cor=%d\ninverter=%d\nteclado=%d\npastacas=%s\npastabin=%s\nmapa=",cfg.color,cfg.invert,cfg.kb,cfg.casdir,cfg.bindir);
  for(k=0;k<RK_COUNT;k++)n+=sprintf(b+n,"%lu%s",(unsigned long)cfg.map[k],k<RK_COUNT-1?",":"\n");
  plat_cfg_write(b);}
static void cfg_load(void){char*b=cfgb();char *l,*nx;cfg_default();if(!b||plat_cfg_read(b,4095)<=0)return;
  for(l=b;l&&*l;l=nx){char *v;nx=strchr(l,'\n');if(nx)*nx++=0;v=strchr(l,'=');if(!v)continue;*v++=0;
    {size_t ln=strlen(v);while(ln&&(v[ln-1]=='\r'))v[--ln]=0;}
    if(!strcmp(l,"cor"))cfg.color=atoi(v)&3;else if(!strcmp(l,"inverter"))cfg.invert=atoi(v)?1:0;
    else if(!strcmp(l,"teclado")){cfg.kb=atoi(v);if(cfg.kb<0||cfg.kb>2)cfg.kb=0;}
    else if(!strcmp(l,"pastacas")){strncpy(cfg.casdir,v,255);}
    else if(!strcmp(l,"pastabin")){strncpy(cfg.bindir,v,255);}
    else if(!strcmp(l,"mapa")){int k=0;char *p=v;while(*p&&k<RK_COUNT){cfg.map[k++]=strtoul(p,&p,10);if(*p==',')p++;}}}}

/* ------------------------------ desenho ------------------------------ */
static void clr(void){memset(fb,0,sizeof fb);}
static void putch(int col,int row,int c,int inv){int l;const uint8_t *g;if(col<0||col>=COLS||row<0||row>=ROWS)return;
  if(c<32||c>126)c='?';
  g=&UIFONT[(c-32)*8];
  for(l=0;l<CH;l++){uint8_t v=(l>=2&&l<10)?g[l-2]:0;fb[(row*CH+l)*64+col]=inv?(uint8_t)~v:v;}}
static void text(int col,int row,const char*s,int inv){while(*s&&col<COLS)putch(col++,row,(unsigned char)*s++,inv);}
static void textc(int row,const char*s,int inv){text((COLS-(int)strlen(s))/2,row,s,inv);}
static void bar(int row,int inv){int c;for(c=0;c<COLS;c++)putch(c,row,' ',inv);}
static void title(const char*s){bar(0,1);textc(0,s,1);}
static void footer(const char*s){bar(ROWS-1,1);text(1,ROWS-1,s,1);}
static void hline(int row){int c;for(c=0;c<COLS;c++)putch(c,row,'-',0);}
static void showmsg(const char*s){strncpy(msg,s,COLS);msg[COLS]=0;msgt=180;}

/* ------------------------------ caminhos ----------------------------- */
static void pjoin(char*out,const char*dir,const char*name){char s=plat_sep();size_t n;
  if(!dir[0]){sprintf(out,"%s%c",name,s);return;}
  strcpy(out,dir);n=strlen(out);if(n&&out[n-1]!=s){out[n]=s;out[n+1]=0;}strcat(out,name);}
static void pparent(char*dir){char s=plat_sep();size_t n;char*p;if(plat_isroot(dir))return;
  n=strlen(dir);if(n&&dir[n-1]==s){if(n==3&&dir[1]==':'){dir[0]=0;return;}dir[--n]=0;}
  p=strrchr(dir,s);if(!p){dir[0]=0;return;}
  if(p==dir){dir[1]=0;return;}
  if(p==dir+2&&dir[1]==':'){p[1]=0;return;}
  *p=0;}
static int hasext(const char*n,const char*e){size_t a=strlen(n),b=strlen(e);size_t i;if(a<b)return 0;
  for(i=0;i<b;i++)if(tolower((unsigned char)n[a-b+i])!=tolower((unsigned char)e[i]))return 0;return 1;}

/* ------------------------------ listas de arquivos ------------------- */
typedef struct { char name[NAMEL]; char isdir; } ent_t;
typedef struct { ent_t e[MAXENT]; int n, sel, top; } list_t;
static list_t L1, L2;           /* L1: pastas/arquivos;  L2: fitas .CAS */
static char curdir[256];
static const char *lfilter; static int ldirs, lfiles; static list_t *lfill;
static void addcb(const char*name,int isdir,void*u){list_t*l=lfill;(void)u;
  if(l->n>=MAXENT)return;if(isdir&&!ldirs)return;if(!isdir&&(!lfiles||!hasext(name,lfilter)))return;
  strncpy(l->e[l->n].name,name,NAMEL-1);l->e[l->n].name[NAMEL-1]=0;l->e[l->n].isdir=(char)isdir;l->n++;}
static int entcmp(const void*a,const void*b){const ent_t*x=a,*y=b;if(x->isdir!=y->isdir)return y->isdir-x->isdir;
  {const char*p=x->name,*q=y->name;while(*p&&tolower((unsigned char)*p)==tolower((unsigned char)*q)){p++;q++;}return tolower((unsigned char)*p)-tolower((unsigned char)*q);}}
static void fill(list_t*l,const char*dir,int dirs,int files,const char*ext,int dotdot){
  l->n=0;l->sel=0;l->top=0;lfill=l;ldirs=dirs;lfiles=files;lfilter=ext;
  if(dotdot&&!plat_isroot(dir)&&dir[0]){strcpy(l->e[0].name,"..");l->e[0].isdir=1;l->n=1;}
  plat_list(dir,addcb,NULL);
  qsort(l->e+(l->n&&!strcmp(l->e[0].name,"..")?1:0),l->n-(l->n&&!strcmp(l->e[0].name,"..")?1:0),sizeof(ent_t),entcmp);}
static void ldraw(list_t*l,int row0,int rows,int col,int width,int focus,const char*empty){int i;
  if(l->sel<l->top)l->top=l->sel;if(l->sel>=l->top+rows)l->top=l->sel-rows+1;
  if(!l->n){text(col+1,row0,empty,0);return;}
  for(i=0;i<rows&&l->top+i<l->n;i++){char b[COLS+1];ent_t*e=&l->e[l->top+i];int inv=focus&&(l->top+i==l->sel);
    if(e->isdir==1)snprintf(b,sizeof b," [%s]",e->name);else snprintf(b,sizeof b," %s",e->name);
    {int k=(int)strlen(b);while(k<width&&k<COLS)b[k++]=' ';b[width<COLS?width:COLS]=0;}
    text(col,row0+i,b,inv);}}
static int lnav(list_t*l,int hk,int ch,int rows){
  switch(hk){case HK_UP:if(l->sel>0)l->sel--;return 1;case HK_DOWN:if(l->sel<l->n-1)l->sel++;return 1;
    case HK_PGUP:l->sel-=rows;if(l->sel<0)l->sel=0;return 1;case HK_PGDN:l->sel+=rows;if(l->sel>=l->n)l->sel=l->n-1;if(l->sel<0)l->sel=0;return 1;
    case HK_HOME:l->sel=0;return 1;case HK_END:l->sel=l->n?l->n-1:0;return 1;}
  if(ch>' '&&ch<127){int i,c=tolower(ch);for(i=1;i<=l->n;i++){int j=(l->sel+i)%l->n;if(tolower((unsigned char)l->e[j].name[0])==c){l->sel=j;return 1;}}}
  return 0;}

/* ------------------------------ cartucho ----------------------------- */
static void load_cart(const char*path,const char*name){long n,cap=262144;uint8_t*b=(uint8_t*)malloc(cap);
  if(!b){cap=131072;b=(uint8_t*)malloc(cap);}   /* (ESP32 sem PSRAM: fica em 128 KB) */
  if(!b){showmsg("Sem memoria para o cartucho");return;}
  n=plat_load(path,b,cap);
  if(n<=0||(n%4096)){free(b);showmsg("Arquivo invalido (precisa ter multiplos de 4 KB)");return;}
  ringo_set_cart(M,NULL,0);if(cartbuf)free(cartbuf);cartbuf=b;cartlen=n;
  strncpy(cartname,name,NAMEL-1);ringo_set_cart(M,cartbuf,cartlen);ringo_reset(M);ringo_keys_clear(M);
  {char t[COLS+1];snprintf(t,sizeof t,"Cartucho: %s (%ld KB)",name,n/1024);showmsg(t);}}
static void eject(void){ringo_set_cart(M,NULL,0);if(cartbuf)free(cartbuf);cartbuf=NULL;cartlen=0;cartname[0]=0;ringo_reset(M);ringo_keys_clear(M);showmsg("Sem cartucho: BASIC do Ringo");}

/* ------------------------------ fita (.CAS) -------------------------- */
static int find_cas(const char*name,char*path){int i;fill(&L2,cfg.casdir,0,1,".cas",0);
  for(i=0;i<L2.n;i++){char b[NAMEL];size_t k=strlen(L2.e[i].name);if(k<4)continue;strncpy(b,L2.e[i].name,NAMEL-1);b[k-4]=0;
    {const char*p=b,*q=name;while(*p&&tolower((unsigned char)*p)==tolower((unsigned char)*q)){p++;q++;}if(!*p&&!*q){pjoin(path,cfg.casdir,L2.e[i].name);return 1;}}}
  return 0;}
static void open_caspick(void);
static int cb_save(ringo_t*m,const char*name,const uint8_t*d,int len){char path[320],fn[NAMEL],t[COLS+1];(void)m;
  snprintf(fn,sizeof fn,"%s.cas",name[0]?name:"SEMNOME");pjoin(path,cfg.casdir,fn);
  if(plat_save(path,d,len))snprintf(t,sizeof t,"SAVE: %s gravado (%d bytes)",fn,len);else snprintf(t,sizeof t,"SAVE: erro gravando %s",fn);
  showmsg(t);return 1;}
static uint8_t*tapebuf;
static int load_cas_path(const char*path,const char*shown){long n;if(!tapebuf)tapebuf=(uint8_t*)malloc(0x4000);n=tapebuf?plat_load(path,tapebuf,0x4000):-1;char t[COLS+1];
  if(n<=0){snprintf(t,sizeof t,"LOAD: erro lendo %s",shown);showmsg(t);return 0;}
  ringo_load_data(M,tapebuf,(int)n);snprintf(t,sizeof t,"LOAD: %s carregado",shown);showmsg(t);return 1;}
static int cb_load(ringo_t*m,const char*name){char path[320];(void)m;
  if(name[0]&&find_cas(name,path))return load_cas_path(path,name);
  open_caspick();return 0;}

/* ------------------------------ teclado do emulador ------------------ */
#define MAXACT 16
static struct { uint32_t raw; int rk[4]; int n; } act[MAXACT];
static int rk_letter(int c){static const int T[26]={RK_A,RK_B,RK_C,RK_D,RK_E,RK_F,RK_G,RK_H,RK_I,RK_J,RK_K,RK_L,RK_M,RK_N,RK_O,RK_P,RK_Q,RK_R,RK_S,RK_T,RK_U,RK_V,RK_W,RK_X,RK_Y,RK_Z};return T[c-'A'];}
static int rk_digit(int c){static const int T[10]={RK_0,RK_1,RK_2,RK_3,RK_4,RK_5,RK_6,RK_7,RK_8,RK_9};return T[c-'0'];}
/* simbolos: caractere do PC -> tecla do Ringo (com SHIFT, salvo o ponto) */
static int sym(int ch,int*shift){*shift=1;switch(ch){
  case '?':return RK_Z;case '(':return RK_X;case ')':return RK_C;case '<':return RK_V;case '*':return RK_G;
  case '"':return RK_P;case '$':return RK_O;case '=':return RK_L;case '+':return RK_K;case '-':return RK_J;
  case '/':return RK_H;case ',':return RK_DOT;case ';':return RK_M;case ':':return RK_N;case '>':return RK_B;
  case '.':*shift=0;return RK_DOT;}return -1;}
static void releaseall(void){int i;for(i=0;i<MAXACT;i++)act[i].n=0;ringo_keys_clear(M);}
static void emu_key(int hk,int ch,uint32_t raw,int down){int i,rk=-1,extra=0,slot=-1;
  for(i=0;i<MAXACT;i++)if(act[i].n&&act[i].raw==raw){slot=i;break;}
  if(!down){if(slot>=0){int k;for(k=0;k<act[slot].n;k++)ringo_key(M,act[slot].rk[k],0);act[slot].n=0;}return;}
  if(slot>=0)return;                                   /* repeticao automatica */
  for(i=0;i<MAXACT;i++)if(!act[i].n){slot=i;break;}if(slot<0)return;
  act[slot].raw=raw;
  if(cfg.kb==KB_PERS){int k;for(k=0;k<RK_COUNT&&act[slot].n<4;k++)if(cfg.map[k]&&cfg.map[k]==raw)act[slot].rk[act[slot].n++]=k;}
  else{
    if(hk==HK_LSHIFT||hk==HK_RSHIFT)rk=RK_SHIFT;
    else if(hk>='A'&&hk<='Z')rk=rk_letter(hk);
    else if(hk>='0'&&hk<='9'&&!hostshift)rk=rk_digit(hk);
    else switch(hk){case HK_ENTER:rk=RK_ENTER;break;case HK_SPACE:case HK_ESC:rk=RK_SPACE;break;
      case HK_BACK:case HK_DEL:rk=RK_DELETE;break;case HK_LEFT:rk=RK_LEFT;break;case HK_RIGHT:rk=RK_RIGHT;break;
      case HK_UP:rk=RK_UP;break;case HK_DOWN:rk=RK_DOWN;break;case HK_TAB:rk=RK_FUNCTION;break;
      case HK_CAPS:rk=RK_GRAPHICS;break;case HK_INS:rk=RK_INVVIDEO;break;case HK_HOME:rk=RK_EDIT;break;
      default:if(ch)rk=sym(ch,&extra);}
    if(rk>=0){act[slot].rk[act[slot].n++]=rk;if(extra)act[slot].rk[act[slot].n++]=RK_SHIFT;}}
  for(i=0;i<act[slot].n;i++)ringo_key(M,act[slot].rk[i],1);}

/* ------------------------------ ajuda (F1) --------------------------- */
static const char *HELP[][40]={
{"EMULADOR DO RINGO R-470",
 "",
 " F1        esta ajuda (PgUp/PgDn ou setas trocam a pagina)",
 " F5        configuracao: cores, teclado, pasta das fitas",
 " F6        tabela de todas as teclas do Ringo e seus comandos",
 " F9        carregar cartucho (.BIN de 4 KB ou multijogos)",
 " F10       reset do Ringo",
 " Shift+F10 sair do emulador",
 " Esc       BREAK (interrompe o programa em BASIC)",
 "",
 "TECLAS ESPECIAIS DO RINGO (teclado ABNT ou US)",
 "",
 " Backspace/Delete  DELETE (apaga)",
 " Setas             cursores",
 " Tab               FUNCTION (cursor F: funcoes em azul)",
 " Caps Lock         GRAPHICS (cursor G: simbolos graficos)",
 " Insert            INVERSE VIDEO (inverte frente/fundo)",
 " Home              EDIT (traz a linha para edicao)",
 " Shift             SHIFT",
 "",
 "Os simbolos do PC ( \" $ ( ) + - * / = < > , ; : ? ) viram",
 "a tecla certa do Ringo automaticamente.",
 "",
 "No modo Personalizado cada tecla do Ringo usa a tecla do",
 "PC escolhida no editor de mapa (F5).",0},
{"O TECLADO DO RINGO",
 "",
 "O Ringo e um computador estilo ZX81: os comandos do BASIC",
 "sao digitados com uma tecla so. O cursor mostra o modo:",
 "",
 " K  comando: a tecla digita a palavra impressa embaixo",
 "    dela (ex.: P = PRINT, R = RUN, L = LIST, S = SAVE,",
 "    A = LOAD, M = NEW)",
 " L  letra: a tecla digita a letra",
 " F  funcao (FUNCTION/Tab): palavras em azul (SIN, CODE...)",
 " G  graficos (GRAPHICS/Caps Lock): caracteres invertidos",
 "    e, com SHIFT, os simbolos semi-graficos",
 "",
 "SHIFT + tecla digita o que esta impresso em cima dela",
 "(simbolos, aspas, palavras como AND, THEN, STOP...).",
 "",
 "Para parar um programa: Esc (BREAK).",0},
{"FITA K7 (ARQUIVOS .CAS)",
 "",
 "O SAVE e o LOAD do Ringo gravam e leem arquivos .CAS",
 "na pasta das fitas (escolhida no F5).",
 "",
 " SAVE \"JOGO\"   grava JOGO.CAS na pasta das fitas",
 " LOAD \"JOGO\"   le JOGO.CAS",
 " LOAD \"\"       abre o seletor para escolher a fita",
 "",
 "No seletor: setas escolhem, ENTER carrega,",
 "Esc cancela (o Ringo recebe um BREAK).",
 "",
 "Como no gravador de verdade, um SAVE dentro do programa",
 "faz o programa continuar sozinho depois do LOAD.",
 "",
 "O arquivo .CAS guarda a memoria a partir de 16393 (4009h)",
 "ate o fim do programa, no mesmo formato de um arquivo .P",
 "do ZX81.",0},
{"CARTUCHOS (F9)",
 "",
 "O F9 abre a lista de arquivos (estilo LIST.COM):",
 "",
 " setas, PgUp, PgDn, Home, End   andam pela lista",
 " letra                         pula para o nome",
 " ENTER                         entra na pasta / carrega",
 " Backspace                     volta uma pasta",
 " Esc                           cancela",
 "",
 "A primeira linha, <SEM CARTUCHO>, volta ao BASIC.",
 "",
 "Arquivos de 4 KB sao cartuchos comuns (entrada em 241Ch).",
 "Arquivos maiores (ate 256 KB) sao o cartucho multijogos:",
 "o banco 0 e o menu e cada banco de 4 KB e um jogo.",
 "",
 "Ao carregar, o Ringo e reiniciado com o cartucho.",
 "",
 "",
 "RetroLabs - JL Martins e Luiz Pacheco - 2026",0}};
#define NHELP 4
static int hpage=0;
static void draw_help(void){int i;char t[32];clr();title(HELP[hpage][0]);
  for(i=1;HELP[hpage][i];i++)text(2,1+i,HELP[hpage][i],0);
  snprintf(t,sizeof t,"Pagina %d de %d",hpage+1,NHELP);text(COLS-2-(int)strlen(t),ROWS-2,t,0);
  footer("PgUp/PgDn ou setas: paginas    Esc: volta");}


/* ------------------------------ teclas do Ringo (F6) ----------------- */
/* tirado das tabelas da ROM do Ringo: tecla, sozinha (comando no cursor K
   ou a propria letra/numero no cursor L), com SHIFT, e no modo FUNCTION */
static const char *KT[][4]={
{"1","1","EDIT","-"},
{"2","2","\"\"","-"},
{"3","3","LPRINT","-"},
{"4","4","LLIST","-"},
{"5","5","CUR.ESQ","-"},
{"6","6","CUR.BAIXO","-"},
{"7","7","CUR.CIMA","-"},
{"8","8","CUR.DIR","-"},
{"9","9","GRAPHICS","-"},
{"0","0","DELETE","-"},
{"Q","COPY","OR","SIN"},
{"W","IF","THEN","ASN"},
{"E","PAUSE","SLOW","COS"},
{"R","RUN","FAST","ACS"},
{"T","REM","TO","TAN"},
{"Y","PLOT","<=","ATN"},
{"U","UNPLOT","<>","USR"},
{"I","INPUT",">=","INKEY$"},
{"O","RAND","$","RND"},
{"P","PRINT","\"","PI"},
{"A","LOAD","AND","-"},
{"S","SAVE","STOP","STR$"},
{"D","DIM","STEP","CHR$"},
{"F","FOR","**","SQR"},
{"G","GOTO","*","ABS"},
{"H","GOSUB","/","INT"},
{"J","RETURN","-","SGN"},
{"K","LET","+","EXP"},
{"L","LIST","=","LN"},
{"ENTER","-","FUNCTION","-"},
{"Z","CLEAR","?","-"},
{"X","CLS","(","LEN"},
{"C","CONT",")","CODE"},
{"V","SCROLL","<","VAL"},
{"B","POKE",">","PEEK"},
{"N","NEXT",":","NOT"},
{"M","NEW",";","AT"},
{".",".",",","TAB"},
{"ESPACO","-","ESPACO","-"}};
#define NKT ((int)(sizeof KT/sizeof KT[0]))
static void draw_keys(void){int i;char b[COLS+1];clr();title("TECLAS DO RINGO E SEUS COMANDOS");
  for(i=0;i<2;i++){int x=i*33;
    text(x,2,"TECLA SOZINHA SHIFT     FUNCAO ",1);}
  for(i=0;i<NKT;i++){int x=(i/20)*33,y=3+(i%20);
    snprintf(b,sizeof b,"%-5s %-7s %-9s %-7s",KT[i][0],KT[i][1],KT[i][2],KT[i][3]);text(x,y,b,0);}
  text(1,24,"SOZINHA: no cursor K digita o comando; no cursor L, a letra.",0);
  text(1,25,"SHIFT: o que vem impresso em cima da tecla (Shift do PC).",0);
  text(1,26,"FUNCTION: SHIFT+ENTER (ou Tab no PC) e depois a tecla.",0);
  text(1,27,"GRAPHICS: SHIFT+9 (ou Caps Lock): letras viram inversas e,",0);
  text(1,28,"com SHIFT, os simbolos semi-graficos.  BREAK: Esc no PC.",0);
  footer("F6 ou Esc: volta");}

/* ------------------------------ configuracao (F5) -------------------- */
static int csel=0;
#define NCFG 6
static void draw_cfg(void){char b[COLS+1];int i;clr();title("CONFIGURACAO DO EMULADOR");
  for(i=0;i<NCFG;i++){int r=3+i*3;
    switch(i){case 0:snprintf(b,sizeof b,"Cor do fundo ........ < %s >",colname[cfg.color]);break;
      case 1:snprintf(b,sizeof b,"Inverter frente/fundo < %s >",cfg.invert?"Sim":"Nao");break;
      case 2:snprintf(b,sizeof b,"Teclado ............. < %s >",kbname[cfg.kb]);break;
      case 3:snprintf(b,sizeof b,"Editar mapa do teclado personalizado...");break;
      case 4:snprintf(b,sizeof b,"Pasta das fitas (CAS):");break;
      default:snprintf(b,sizeof b,"Salvar e voltar");}
    text(4,r,b,i==csel);
    if(i==4){char p[COLS-6];strncpy(p,cfg.casdir[0]?cfg.casdir:"(raiz)",sizeof p-1);p[sizeof p-1]=0;text(6,r+1,p,0);}}
  text(4,22,"Padrao: frente preta e fundo na cor escolhida.",0);
  text(4,23,"\"Inverter\" troca: frente na cor e fundo preto.",0);
  text(4,24,"A tecla INVERSE VIDEO do Ringo (Insert) tambem inverte.",0);
  {int y=27,x;uint32_t a,b2;(void)a;(void)b2;text(4,y,"Exemplo:",0);
   for(x=0;x<20;x++)putch(14+x,y," RINGO R-470  2026 "[x%19],cfg.invert);}
  footer("Setas: escolhe/muda    ENTER: abre    Esc: salva e volta");}
static void open_keymap(void);
static void open_casdir(int pick);
static void cfg_key(int hk){
  switch(hk){case HK_UP:csel=(csel+NCFG-1)%NCFG;break;case HK_DOWN:csel=(csel+1)%NCFG;break;
    case HK_LEFT:case HK_RIGHT:{int d=hk==HK_RIGHT?1:-1;if(csel==0)cfg.color=(cfg.color+d+4)%4;else if(csel==1)cfg.invert^=1;else if(csel==2){cfg.kb=(cfg.kb+d+3)%3;releaseall();}}break;
    case HK_ENTER:if(csel==3)open_keymap();else if(csel==4)open_casdir(0);else if(csel==5){cfg_save();mode=UI_NONE;}
      else if(csel==1)cfg.invert^=1;else if(csel==0)cfg.color=(cfg.color+1)%4;else if(csel==2){cfg.kb=(cfg.kb+1)%3;releaseall();}break;
    case HK_ESC:cfg_save();mode=UI_NONE;break;}}

/* ------------------------------ editor do mapa ----------------------- */
typedef struct { int rk,row,col; const char*lab; } kpos_t;
static const kpos_t KP[]={
 {RK_1,0,0,"1"},{RK_2,0,1,"2"},{RK_3,0,2,"3"},{RK_4,0,3,"4"},{RK_5,0,4,"5"},{RK_6,0,5,"6"},{RK_7,0,6,"7"},{RK_8,0,7,"8"},{RK_9,0,8,"9"},{RK_0,0,9,"0"},{RK_DELETE,0,10,"DELETE"},{RK_RIGHT,0,12,"->"},
 {RK_Q,1,0,"Q"},{RK_W,1,1,"W"},{RK_E,1,2,"E"},{RK_R,1,3,"R"},{RK_T,1,4,"T"},{RK_Y,1,5,"Y"},{RK_U,1,6,"U"},{RK_I,1,7,"I"},{RK_O,1,8,"O"},{RK_P,1,9,"P"},{RK_ENTER,1,10,"ENTER"},{RK_LEFT,1,12,"<-"},
 {RK_FUNCTION,2,0,"FUNC"},{RK_A,2,1,"A"},{RK_S,2,2,"S"},{RK_D,2,3,"D"},{RK_F,2,4,"F"},{RK_G,2,5,"G"},{RK_H,2,6,"H"},{RK_J,2,7,"J"},{RK_K,2,8,"K"},{RK_L,2,9,"L"},{RK_INVVIDEO,2,10,"INV.V"},{RK_UP,2,12,"^"},
 {RK_SHIFT,3,0,"SHIFT"},{RK_Z,3,1,"Z"},{RK_X,3,2,"X"},{RK_C,3,3,"C"},{RK_V,3,4,"V"},{RK_B,3,5,"B"},{RK_N,3,6,"N"},{RK_M,3,7,"M"},{RK_DOT,3,8,"."},{RK_GRAPHICS,3,9,"GRAPH"},{RK_DOWN,3,12,"v"},
 {RK_SPACE,4,3,"ESPACO / BREAK"},{RK_EDIT,4,9,"EDIT"}};
#define NKP ((int)(sizeof KP/sizeof KP[0]))
static int ksel=0;
static int kx(const kpos_t*k){int c=k->col;switch(k->row){
  case 0:return c<=9?3+c*4:(c==10?43:52);
  case 1:return c<=9?5+c*4:(c==10?45:53);
  case 2:return c==0?1:(c<=9?7+(c-1)*4:(c==10?46:55));
  case 3:return c==0?1:(c<=8?8+(c-1)*4:(c==9?42:55));
  default:return c==3?14:42;}}
static void draw_keymap(void){int i;char b[COLS+1],kn[40];clr();title("MAPA DO TECLADO PERSONALIZADO");
  text(2,3,"Escolha a tecla do Ringo e aperte ENTER; depois aperte a",0);
  text(2,4,"tecla do PC que vai fazer o papel dela.",0);
  for(i=0;i<NKP;i++){const kpos_t*k=&KP[i];int y=6+k->row*3,x=kx(k);
    snprintf(b,sizeof b,"%s",k->lab);
    if(strlen(k->lab)==1||!strcmp(k->lab,"->")||!strcmp(k->lab,"<-")){putch(x-1,y,'[',0);text(x,y,b,i==ksel);putch(x+(int)strlen(b),y,']',0);}
    else text(x,y,b,i==ksel);
    if(cfg.map[k->rk]==0)putch(x,y+1,'.',0);}
  {const kpos_t*k=&KP[ksel];plat_keyname(cfg.map[k->rk],kn,sizeof kn);
   snprintf(b,sizeof b,"Tecla do Ringo: %-15s  Tecla do PC: %s",k->lab,cfg.map[k->rk]?kn:"(nenhuma)");text(2,22,b,0);}
  if(mode==UI_KEYWAIT){bar(24,1);textc(24,"Aperte agora a tecla do PC (Esc cancela)",1);}
  text(2,27,"Este mapa so vale com o teclado \"Personalizado\" (F5).",0);
  text(2,28,"Teclas F1, F5, F6, F9 e F10 ficam sempre com o emulador.",0);
  footer("Setas ENTER:define Del:limpa F3:padrao Esc:volta");}
static void km_move(int dr,int dc){const kpos_t*c=&KP[ksel];int i,best=-1,bd=999;
  if(dc){for(i=0;i<NKP;i++)if(KP[i].row==c->row&&((dc>0&&KP[i].col>c->col)||(dc<0&&KP[i].col<c->col))){int d=abs(KP[i].col-c->col);if(d<bd){bd=d;best=i;}}}
  else{int r=c->row+dr;for(i=0;i<NKP;i++)if(KP[i].row==r){int d=abs(kx(&KP[i])-kx(c));if(d<bd){bd=d;best=i;}}}
  if(best>=0)ksel=best;}
static void keymap_key(int hk){switch(hk){case HK_LEFT:km_move(0,-1);break;case HK_RIGHT:km_move(0,1);break;
  case HK_UP:km_move(-1,0);break;case HK_DOWN:km_move(1,0);break;case HK_ENTER:mode=UI_KEYWAIT;break;
  case HK_DEL:cfg.map[KP[ksel].rk]=0;break;case HK_F3:{int k;for(k=0;k<RK_COUNT;k++)cfg.map[k]=plat_default_raw(k);}break;
  case HK_ESC:mode=UI_CFG;break;}}
static void open_keymap(void){mode=UI_KEYMAP;}

/* ------------------------------ seletor de cartucho (F9) ------------- */
#define FROWS 26
static void files_fill(void){fill(&L1,curdir,1,1,".bin",1);
  if(L1.n<MAXENT){memmove(&L1.e[1],&L1.e[0],sizeof(ent_t)*L1.n);strcpy(L1.e[0].name,"<SEM CARTUCHO - BASIC>");L1.e[0].isdir=2;L1.n++;}}
static void draw_files(void){char b[COLS+1];clr();title("CARREGAR CARTUCHO (.BIN)");
  snprintf(b,sizeof b,"%s",curdir[0]?curdir:"(unidades)");text(1,2,b,0);hline(3);
  ldraw(&L1,4,FROWS,0,COLS,1,"(vazio)");
  footer("ENTER: carrega/entra  Backspace: volta  letra: pula  Esc: sai");}
static void files_key(int hk,int ch){ent_t*e;if(lnav(&L1,hk,ch,FROWS))return;
  switch(hk){case HK_ESC:mode=UI_NONE;break;
    case HK_BACK:if(!plat_isroot(curdir)&&curdir[0]){pparent(curdir);files_fill();}break;
    case HK_ENTER:if(!L1.n)break;e=&L1.e[L1.sel];
      if(e->isdir==2){eject();mode=UI_NONE;break;}
      if(e->isdir){if(!strcmp(e->name,".."))pparent(curdir);else{char p[256];pjoin(p,curdir,e->name);strncpy(curdir,p,255);}files_fill();}
      else{char p[320];pjoin(p,curdir,e->name);strcpy(cfg.bindir,curdir);cfg_save();load_cart(p,e->name);mode=UI_NONE;}break;}}

/* ------------------------------ pasta das fitas / seletor de fita ---- */
static int focus2=0, picking=0; static char confirmname[NAMEL];
#define TROWS 11
#define BROWS 11
static void casdir_fill(void){fill(&L1,curdir,1,0,"",1);fill(&L2,curdir,0,1,".cas",0);}
static void draw_casdir(void){char b[COLS+1];clr();
  title(picking?"LOAD: ESCOLHA A FITA (.CAS)":"PASTA DAS FITAS (.CAS)");
  snprintf(b,sizeof b,"%s",curdir[0]?curdir:"(unidades)");text(1,2,b,0);hline(3);
  text(0,4,"PASTAS",!focus2);ldraw(&L1,5,TROWS,0,COLS,!focus2,"(sem pastas)");
  hline(16);snprintf(b,sizeof b,"FITAS NESTA PASTA (%d)",L2.n);text(0,17,b,focus2);
  ldraw(&L2,18,BROWS,0,COLS,focus2,"(nenhum arquivo .CAS)");
  if(mode==UI_CONFIRM){bar(29,1);snprintf(b,sizeof b,"Apagar %s ? (S/N)",confirmname);textc(29,b,1);}
  if(picking)footer("Tab: troca  ENTER: entra/carrega  Del: apaga  Esc: cancela");
  else footer("Tab: troca  ENTER: entra  F2: usa esta pasta  Del: apaga  Esc");}
static void open_casdir(int pick){picking=pick;focus2=pick?1:0;strncpy(curdir,cfg.casdir,255);casdir_fill();mode=pick?UI_CASPICK:UI_CASDIR;}
static void open_caspick(void){releaseall();open_casdir(1);}
static void casdir_key(int hk,int ch){list_t*l=focus2?&L2:&L1;
  if(hk==HK_TAB){focus2^=1;return;}
  if(lnav(l,hk,ch,focus2?BROWS:TROWS))return;
  switch(hk){
    case HK_ESC:if(picking){ringo_load_cancel(M);showmsg("LOAD cancelado");mode=UI_NONE;}else mode=UI_CFG;break;
    case HK_F2:if(!picking){strncpy(cfg.casdir,curdir,255);cfg_save();showmsg("Pasta das fitas escolhida");mode=UI_CFG;}break;
    case HK_BACK:if(!plat_isroot(curdir)&&curdir[0]){pparent(curdir);casdir_fill();}break;
    case HK_DEL:if(focus2&&L2.n){strncpy(confirmname,L2.e[L2.sel].name,NAMEL-1);retmode=mode;mode=UI_CONFIRM;}break;
    case HK_ENTER:
      if(!focus2){ent_t*e;if(!L1.n)break;e=&L1.e[L1.sel];
        if(!strcmp(e->name,".."))pparent(curdir);else{char p[256];pjoin(p,curdir,e->name);strncpy(curdir,p,255);}casdir_fill();}
      else if(picking&&L2.n){char p[320];pjoin(p,curdir,L2.e[L2.sel].name);mode=UI_NONE;
        if(!load_cas_path(p,L2.e[L2.sel].name))ringo_load_cancel(M);}
      break;}}
static void confirm_key(int ch){if(ch=='s'||ch=='S'){char p[320];pjoin(p,curdir,confirmname);
    if(plat_del(p))showmsg("Arquivo apagado");else showmsg("Nao consegui apagar");casdir_fill();}
  mode=retmode;}

/* ------------------------------ entrada ------------------------------ */
static void openui(int m2){releaseall();mode=m2;}
void ui_key(int hk,int ch,uint32_t raw,int down){
  if(hk==HK_LSHIFT||hk==HK_RSHIFT)hostshift=down;
  if(mode==UI_KEYWAIT){if(down){if(hk==HK_ESC)mode=UI_KEYMAP;else if(hk<HK_F1||hk>HK_F12||(hk!=HK_F1&&hk!=HK_F5&&hk!=HK_F6&&hk!=HK_F9&&hk!=HK_F10)){cfg.map[KP[ksel].rk]=raw;mode=UI_KEYMAP;km_move(0,1);}}return;}
  if(down&&hk==HK_F10&&hostshift){cfg_save();plat_quit();return;}
  if(mode!=UI_NONE){if(!down)return;
    switch(mode){case UI_KEYS:if(hk==HK_ESC||hk==HK_F6)mode=UI_NONE;break;
      case UI_HELP:if(hk==HK_ESC||hk==HK_F1)mode=UI_NONE;else if(hk==HK_PGDN||hk==HK_DOWN||hk==HK_RIGHT||hk==HK_ENTER)hpage=(hpage+1)%NHELP;else if(hk==HK_PGUP||hk==HK_UP||hk==HK_LEFT)hpage=(hpage+NHELP-1)%NHELP;break;
      case UI_CFG:cfg_key(hk);break;case UI_KEYMAP:keymap_key(hk);break;case UI_FILES:files_key(hk,ch);break;
      case UI_CASDIR:case UI_CASPICK:casdir_key(hk,ch);break;case UI_CONFIRM:confirm_key(ch);break;}
    return;}
  if(down)switch(hk){case HK_F1:openui(UI_HELP);return;case HK_F6:openui(UI_KEYS);return;case HK_F5:csel=0;openui(UI_CFG);return;
    case HK_F9:strncpy(curdir,cfg.bindir,255);files_fill();openui(UI_FILES);return;
    case HK_F10:releaseall();ringo_reset(M);showmsg("Reset");return;}
  if(hk>=HK_F1&&hk<=HK_F12)return;
  if(M->tape==TAPE_WAIT_LOAD)return;
  emu_key(hk,ch,raw,down);}
void ui_focus_lost(void){releaseall();hostshift=0;}

/* ------------------------------ quadro ------------------------------- */
static void draw_emu(void){int y,x;ringo_render(M,scr);
  for(y=0;y<RINGO_H;y++){uint8_t*d0=&fb[(y*2)*64],*d1=&fb[(y*2+1)*64];const uint8_t*s=&scr[y*32];
    for(x=0;x<32;x++){uint8_t b=s[x];uint16_t w=0;int k;for(k=0;k<8;k++)if(b&(0x80>>k))w|=(uint16_t)(0xC000>>(k*2));
      d0[x*2]=d1[x*2]=(uint8_t)(w>>8);d0[x*2+1]=d1[x*2+1]=(uint8_t)w;}}
  if(msgt>0){msgt--;bar(ROWS-1,1);text(1,ROWS-1,msg,1);}}
void ui_frame(void){
  if(mode==UI_NONE&&M->tape!=TAPE_WAIT_LOAD)ringo_frame(M);
  switch(mode){case UI_NONE:draw_emu();break;case UI_HELP:draw_help();break;case UI_KEYS:draw_keys();break;case UI_CFG:draw_cfg();break;
    case UI_KEYMAP:case UI_KEYWAIT:draw_keymap();break;case UI_FILES:draw_files();break;
    case UI_CASDIR:case UI_CASPICK:draw_casdir();break;case UI_CONFIRM:draw_casdir();break;}}
const uint8_t*ui_fb(void){return fb;}
void ui_colors(uint32_t*ink,uint32_t*paper){int inv=cfg.invert^(mode==UI_NONE?M->invert:0);
  if(!inv){*ink=0x000000;*paper=colrgb[cfg.color];}else{*ink=colrgb[cfg.color];*paper=0x000000;}}
int ui_kblayout(void){return cfg.kb;}
void ui_init(ringo_t*m){M=m;cfg_load();M->cb_save=cb_save;M->cb_load=cb_load;showmsg("Ringo R-470 - F1 ajuda");}
