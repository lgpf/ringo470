/* RINGO R-470 - nucleo do emulador. RetroLabs - JL Martins e Luiz Pacheco - 2026 */
#include "ringo_core.h"
#include <string.h>

#define FC 1
#define FN 2
#define FPV 4
#define FH 16
#define FZ 64
#define FS 128
static uint8_t SZ[256],SZP[256]; static int tabok=0;
static const uint8_t CYC[64]={4,10,7,6,4,4,7,4,4,11,7,6,4,4,7,4,8,10,7,6,4,4,7,4,12,11,7,6,4,4,7,4,7,10,16,6,4,4,7,4,7,11,16,6,4,4,7,4,7,10,13,6,11,11,10,4,7,11,13,6,4,4,7,4};
static void mktab(void){int i;for(i=0;i<256;i++){int s=(i&0x80)|(i?0:FZ)|(i&0x28),p=i;SZ[i]=s;p^=p>>4;p^=p>>2;p^=p>>1;SZP[i]=s|((p&1)?0:FPV);}tabok=1;}

/* ------------------------- memoria e portas ------------------------- */
uint8_t ringo_rd(ringo_t *m,uint16_t a){
  a&=0x7fff;
  if(a<0x2000) return m->rom[a];
  if(a<0x4000){
    if(m->cart){ if(a<0x3000){ long o=((long)m->bank<<12)|(a-0x2000); return o<m->cartlen? m->cart[o]:0xff; } return 0xff; }
    return m->rom[a&0x1fff];               /* sem cartucho: espelho da ROM (BASIC) */
  }
  return m->ram[a-0x4000];
}
static void wr(ringo_t *m,uint16_t a,uint8_t v){
  if(m->cartlen>4096 && a>=0x2000 && a<0x3000){ m->bank=v&63; return; }   /* latch do multijogos (6 bits: ate 256 KB) */
  a&=0x7fff; if(a>=0x4000) m->ram[a-0x4000]=v;
}
static uint8_t inp(ringo_t *m,uint16_t p){
  if(!(p&1)){ int r,hi=p>>8; uint8_t v=0x1f; for(r=0;r<8;r++) if(!(hi&(1<<r))) v&=~m->matrix[r]; return v|0x80; }
  return 0xff;
}
static void outp(ringo_t *m,uint16_t p,uint8_t v){ (void)v; if(!(p&1)) m->nmion=1; else if(!(p&2)) m->nmion=0; }

/* ------------------------------ Z80 --------------------------------- */
#define CPU (&m->cpu)
#define RD(a) ringo_rd(m,(uint16_t)(a))
#define WR(a,v) wr(m,(uint16_t)(a),(uint8_t)(v))
static uint16_t BC(z80_t*c){return c->b<<8|c->c;} static void sBC(z80_t*c,int v){c->b=v>>8&255;c->c=v&255;}
static uint16_t DE(z80_t*c){return c->d<<8|c->e;} static void sDE(z80_t*c,int v){c->d=v>>8&255;c->e=v&255;}
static uint16_t HL(z80_t*c){return c->h<<8|c->l;} static void sHL(z80_t*c,int v){c->h=v>>8&255;c->l=v&255;}
static void incR(z80_t*c){c->r=(c->r&0x80)|((c->r+1)&0x7f);}
static uint8_t fetchop(ringo_t*m){z80_t*c=CPU;uint16_t a=c->pc;uint8_t v=RD(a);if((a&0x8000)&&!(v&0x40))v=0;c->pc=a+1;incR(c);return v;}
static uint8_t f8(ringo_t*m){uint8_t v=RD(CPU->pc);CPU->pc++;return v;}
static int f8s(ringo_t*m){int v=f8(m);return v>127?v-256:v;}
static uint16_t f16(ringo_t*m){int l=f8(m);return l|f8(m)<<8;}
static uint16_t r16(ringo_t*m,int a){return RD(a&0xffff)|RD((a+1)&0xffff)<<8;}
static void w16(ringo_t*m,int a,int v){WR(a&0xffff,v&255);WR((a+1)&0xffff,v>>8&255);}
static void push(ringo_t*m,int v){CPU->sp-=2;w16(m,CPU->sp,v);}
static uint16_t pop(ringo_t*m){uint16_t v=r16(m,CPU->sp);CPU->sp+=2;return v;}
static int irq(ringo_t*m){z80_t*c=CPU;if(!c->iff1||c->eidelay)return 0;if(c->halted){c->halted=0;c->pc++;}
  c->iff1=c->iff2=0;incR(c);push(m,c->pc);if(c->im==2){c->pc=r16(m,(c->i<<8)|0xff);return 19;}c->pc=0x38;return 13;}
static int nmi(ringo_t*m){z80_t*c=CPU;if(c->halted){c->halted=0;c->pc++;}c->iff2=c->iff1;c->iff1=0;incR(c);push(m,c->pc);c->pc=0x66;return 11;}
static int cond(z80_t*c,int y){int f=c->f;switch(y){case 0:return!(f&FZ);case 1:return!!(f&FZ);case 2:return!(f&FC);case 3:return!!(f&FC);case 4:return!(f&FPV);case 5:return!!(f&FPV);case 6:return!(f&FS);default:return!!(f&FS);}}
static void alu(z80_t*c,int y,int v){int a=c->a,r;switch(y){
  case 0:case 1:r=a+v+(y==1?c->f&FC:0);c->f=SZ[r&255]|((a^v^r)&FH)|(((a^~v)&(a^r)&0x80)?FPV:0)|(r>>8&1);c->a=r&255;break;
  case 2:case 3:case 7:r=a-v-(y==3?c->f&FC:0);c->f=FN|SZ[r&255]|((a^v^r)&FH)|(((a^v)&(a^r)&0x80)?FPV:0)|(r&0x100?FC:0);if(y!=7)c->a=r&255;break;
  case 4:c->a&=v;c->f=SZP[c->a]|FH;break;case 5:c->a^=v;c->f=SZP[c->a];break;default:c->a|=v;c->f=SZP[c->a];}}
static int inc8(z80_t*c,int v){int r=(v+1)&255;c->f=(c->f&FC)|SZ[r]|((v&15)==15?FH:0)|(v==0x7f?FPV:0);return r;}
static int dec8(z80_t*c,int v){int r=(v-1)&255;c->f=(c->f&FC)|FN|SZ[r]|((v&15)==0?FH:0)|(v==0x80?FPV:0);return r;}
static int add16(z80_t*c,int a,int b){int r=a+b;c->f=(c->f&(FS|FZ|FPV))|(((a^b^r)>>8)&FH)|(r>>16&1);return r&0xffff;}
static void adc16(z80_t*c,int v){int hl=HL(c),r=hl+v+(c->f&FC),q=r&0xffff;c->f=(q>>8&FS)|(q?0:FZ)|(((hl^v^r)>>8)&FH)|(((hl^~v)&(hl^r)&0x8000)?FPV:0)|(r>>16&1);sHL(c,q);}
static void sbc16(z80_t*c,int v){int hl=HL(c),r=hl-v-(c->f&FC),q=r&0xffff;c->f=FN|(q>>8&FS)|(q?0:FZ)|(((hl^v^r)>>8)&FH)|(((hl^v)&(hl^r)&0x8000)?FPV:0)|(r<0?FC:0);sHL(c,q);}
static int rot(z80_t*cp,int y,int v){int c,r;switch(y){case 0:c=v>>7;r=(v<<1|c)&255;break;case 1:c=v&1;r=v>>1|c<<7;break;case 2:c=v>>7;r=(v<<1|(cp->f&FC))&255;break;
  case 3:c=v&1;r=v>>1|(cp->f&FC)<<7;break;case 4:c=v>>7;r=(v<<1)&255;break;case 5:c=v&1;r=v>>1|(v&0x80);break;case 6:c=v>>7;r=(v<<1|1)&255;break;default:c=v&1;r=v>>1;}
  cp->f=SZP[r]|c;return r;}
static int getHLx(z80_t*c){return c->idx==0?HL(c):c->idx==1?c->ix:c->iy;}
static void setHLx(z80_t*c,int v){v&=0xffff;if(c->idx==0)sHL(c,v);else if(c->idx==1)c->ix=v;else c->iy=v;}
static int ea(ringo_t*m){z80_t*c=CPU;if(c->idx==0)return HL(c);if(c->ea_<0){c->ea_=(getHLx(c)+f8s(m))&0xffff;c->t_+=8;}return c->ea_;}
static int getR(ringo_t*m,int n,int plain){z80_t*c=CPU;switch(n){case 0:return c->b;case 1:return c->c;case 2:return c->d;case 3:return c->e;
  case 4:return(plain||!c->idx)?c->h:getHLx(c)>>8;case 5:return(plain||!c->idx)?c->l:getHLx(c)&255;case 6:return RD(ea(m));default:return c->a;}}
static void setR(ringo_t*m,int n,int v,int plain){z80_t*c=CPU;v&=255;switch(n){case 0:c->b=v;break;case 1:c->c=v;break;case 2:c->d=v;break;case 3:c->e=v;break;
  case 4:if(plain||!c->idx)c->h=v;else setHLx(c,(getHLx(c)&255)|v<<8);break;case 5:if(plain||!c->idx)c->l=v;else setHLx(c,(getHLx(c)&0xff00)|v);break;
  case 6:WR(ea(m),v);break;default:c->a=v;}}
static int getRP(z80_t*c,int p){return p==0?BC(c):p==1?DE(c):p==2?getHLx(c):c->sp;}
static void setRP(z80_t*c,int p,int v){v&=0xffff;if(p==0)sBC(c,v);else if(p==1)sDE(c,v);else if(p==2)setHLx(c,v);else c->sp=v;}

static int op_main(ringo_t*m,int op){z80_t*c=CPU;int x=op>>6,y=op>>3&7,z=op&7,p=y>>1,q=y&1;
  int t=x==0?CYC[op]:x==1?(z==6||y==6?7:4):x==2?(z==6?7:4):0;
  if(x==0){switch(z){
    case 0:if(y==1){int a=c->a,f=c->f;c->a=c->a_;c->f=c->f_;c->a_=a;c->f_=f;}
      else if(y==2){int d=f8s(m);c->b--;if(c->b){c->pc+=d;t=13;}}
      else if(y>=3){int d=f8s(m);if(y==3||cond(c,y-4)){c->pc+=d;t=12;}else t=7;}
      break;
    case 1:if(q==0)setRP(c,p,f16(m));else setHLx(c,add16(c,getHLx(c),getRP(c,p)));break;
    case 2:switch(y){case 0:WR(BC(c),c->a);break;case 1:c->a=RD(BC(c));break;case 2:WR(DE(c),c->a);break;case 3:c->a=RD(DE(c));break;
      case 4:w16(m,f16(m),getHLx(c));break;case 5:setHLx(c,r16(m,f16(m)));break;case 6:WR(f16(m),c->a);break;default:c->a=RD(f16(m));}break;
    case 3:setRP(c,p,getRP(c,p)+(q?-1:1));break;
    case 4:if(y==6){int a=ea(m);WR(a,inc8(c,RD(a)));t=11;}else setR(m,y,inc8(c,getR(m,y,0)),0);break;
    case 5:if(y==6){int a=ea(m);WR(a,dec8(c,RD(a)));t=11;}else setR(m,y,dec8(c,getR(m,y,0)),0);break;
    case 6:if(y==6){int a=ea(m);WR(a,f8(m));}else setR(m,y,f8(m),0);break;
    default:{int P=c->f&(FS|FZ|FPV),a=c->a,cc;switch(y){
      case 0:cc=a>>7;c->a=(a<<1|cc)&255;c->f=P|cc;break;case 1:cc=a&1;c->a=a>>1|cc<<7;c->f=P|cc;break;
      case 2:cc=a>>7;c->a=(a<<1|(c->f&FC))&255;c->f=P|cc;break;case 3:cc=a&1;c->a=a>>1|(c->f&FC)<<7;c->f=P|cc;break;
      case 4:{int cf=c->f&FC,h=c->f&FH,n=c->f&FN,cr=0,r,hf;if(h||(a&15)>9)cr|=6;if(cf||a>0x99){cr|=0x60;cf=1;}
        r=(n?a-cr:a+cr)&255;hf=n?((h&&(a&15)<6)?FH:0):((a&15)>9?FH:0);c->a=r;c->f=SZP[r]|hf|n|(cf?FC:0);break;}
      case 5:c->a^=255;c->f|=FH|FN;break;case 6:c->f=P|FC;break;default:c->f=P|((c->f&FC)?FH:FC);}}}
    return t;}
  if(x==1){if(op==0x76){c->halted=1;c->pc--;return 4;}
    if(c->idx&&(y==6||z==6))setR(m,y,getR(m,z,1),1);else setR(m,y,getR(m,z,0),0);return t;}
  if(x==2){alu(c,y,getR(m,z,0));return t;}
  switch(z){
    case 0:if(cond(c,y)){c->pc=pop(m);return 11;}return 5;
    case 1:if(q==0){int v=pop(m);if(p==3){c->a=v>>8;c->f=v&255;}else setRP(c,p,v);return 10;}
      switch(p){case 0:c->pc=pop(m);return 10;
        case 1:{uint8_t t2;t2=c->b;c->b=c->b_;c->b_=t2;t2=c->c;c->c=c->c_;c->c_=t2;t2=c->d;c->d=c->d_;c->d_=t2;t2=c->e;c->e=c->e_;c->e_=t2;t2=c->h;c->h=c->h_;c->h_=t2;t2=c->l;c->l=c->l_;c->l_=t2;return 4;}
        case 2:c->pc=getHLx(c);return 4;default:c->sp=getHLx(c);return 6;}
    case 2:{int a=f16(m);if(cond(c,y))c->pc=a;return 10;}
    case 3:switch(y){case 0:c->pc=f16(m);return 10;case 2:{int n=f8(m);outp(m,n|c->a<<8,c->a);return 11;}
      case 3:{int n=f8(m);c->a=inp(m,n|c->a<<8);return 11;}
      case 4:{int v=r16(m,c->sp);w16(m,c->sp,getHLx(c));setHLx(c,v);return 19;}
      case 5:{int v=DE(c);sDE(c,HL(c));sHL(c,v);return 4;}
      case 6:c->iff1=c->iff2=0;return 4;default:c->iff1=c->iff2=1;c->eidelay=1;return 4;}
    case 4:{int a=f16(m);if(cond(c,y)){push(m,c->pc);c->pc=a;return 17;}return 10;}
    case 5:if(q==0){push(m,p==3?(c->a<<8|c->f):getRP(c,p));return 11;}{int a=f16(m);push(m,c->pc);c->pc=a;return 17;}
    case 6:alu(c,y,f8(m));return 7;
    default:push(m,c->pc);c->pc=y*8;return 11;}}
static int op_cb(ringo_t*m){z80_t*c=CPU;int a=0,op,x,y,z,mem,v,r;
  if(c->idx){a=(getHLx(c)+f8s(m))&0xffff;op=f8(m);}else op=fetchop(m);
  x=op>>6;y=op>>3&7;z=op&7;mem=c->idx||z==6;if(!c->idx)a=HL(c);
  v=mem?RD(a):getR(m,z,1);
  if(x==1){c->f=(c->f&FC)|FH|((v>>y&1)?0:FZ|FPV)|(y==7&&(v&0x80)?FS:0);return mem?(c->idx?20:12):8;}
  r=x==0?rot(c,y,v):x==2?v&~(1<<y):v|(1<<y);
  if(mem){WR(a,r);if(c->idx&&z!=6)setR(m,z,r,1);}else setR(m,z,r,1);
  return mem?(c->idx?23:15):8;}
static int op_ed(ringo_t*m,int op){z80_t*c=CPU;int x=op>>6,y=op>>3&7,z=op&7,p=y>>1,q=y&1;
  if(x==1){switch(z){
    case 0:{int v=inp(m,BC(c));if(y!=6)setR(m,y,v,1);c->f=(c->f&FC)|SZP[v];return 12;}
    case 1:outp(m,BC(c),y==6?0:getR(m,y,1));return 12;
    case 2:if(q==0)sbc16(c,getRP(c,p));else adc16(c,getRP(c,p));return 15;
    case 3:{int a=f16(m);if(q==0)w16(m,a,getRP(c,p));else setRP(c,p,r16(m,a));return 20;}
    case 4:{int v=c->a;c->a=0;alu(c,2,v);return 8;}
    case 5:c->pc=pop(m);c->iff1=c->iff2;return 14;
    case 6:{static const uint8_t I[8]={0,0,1,2,0,0,1,2};c->im=I[y];return 8;}
    default:switch(y){case 0:c->i=c->a;return 9;case 1:c->r=c->a;return 9;
      case 2:c->a=c->i;c->f=(c->f&FC)|SZ[c->a]|(c->iff2?FPV:0);return 9;
      case 3:c->a=c->r;c->f=(c->f&FC)|SZ[c->a]|(c->iff2?FPV:0);return 9;
      case 4:{int mm=RD(HL(c));WR(HL(c),((c->a<<4)|(mm>>4))&255);c->a=(c->a&0xf0)|(mm&15);c->f=(c->f&FC)|SZP[c->a];return 18;}
      case 5:{int mm=RD(HL(c));WR(HL(c),((mm<<4)|(c->a&15))&255);c->a=(c->a&0xf0)|(mm>>4);c->f=(c->f&FC)|SZP[c->a];return 18;}
      default:return 8;}}}
  if(x==2&&y>=4&&z<=3){int inc=(y&1)?-1:1,rep=y>=6;
    if(z==0){WR(DE(c),RD(HL(c)));sHL(c,HL(c)+inc);sDE(c,DE(c)+inc);sBC(c,BC(c)-1);c->f=(c->f&(FS|FZ|FC))|(BC(c)?FPV:0);
      if(rep&&BC(c)){c->pc-=2;return 21;}return 16;}
    if(z==1){int v=RD(HL(c)),r=(c->a-v)&255;sHL(c,HL(c)+inc);sBC(c,BC(c)-1);
      c->f=(c->f&FC)|FN|(SZ[r]&(FS|FZ))|((c->a^v^r)&FH)|(BC(c)?FPV:0);if(rep&&BC(c)&&r){c->pc-=2;return 21;}return 16;}
    if(z==2){WR(HL(c),inp(m,BC(c)));sHL(c,HL(c)+inc);c->b--;c->f=FN|(c->b?0:FZ);if(rep&&c->b){c->pc-=2;return 21;}return 16;}
    c->b--;outp(m,BC(c),RD(HL(c)));sHL(c,HL(c)+inc);c->f=FN|(c->b?0:FZ);if(rep&&c->b){c->pc-=2;return 21;}return 16;}
  return 8;}
static int step(ringo_t*m){z80_t*c=CPU;int op;c->eidelay=0;if(c->halted){incR(c);return 4;}
  op=fetchop(m);c->idx=0;c->ea_=-1;c->t_=0;
  while(op==0xDD||op==0xFD){c->idx=op==0xDD?1:2;c->t_+=4;op=fetchop(m);}
  if(op==0xCB)return c->t_+op_cb(m);
  if(op==0xED){c->idx=0;return c->t_+op_ed(m,fetchop(m));}
  return c->t_+op_main(m,op);}

/* ------------------------------ fita -------------------------------- */
/* As rotinas de SAVE/LOAD da ROM do Ringo ficam nos mesmos enderecos do ZX81.
   SAVE: em 02FCh, HL aponta o nome (ultimo caractere com bit 7).
   LOAD: em 0347h, DE aponta o nome e C = tamanho-1 (FFh = LOAD "").
   O conteudo gravado e a RAM de 4009h ate E_LINE (como um arquivo .P). */
static const char ZXC[64]=" ##########\"#$:?()><=+-*/;,.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static void zxname(ringo_t*m,uint16_t a,int len,char*out){int i,n=0;
  for(i=0;i<len&&n<30;i++){uint8_t ch=RD(a+i);char cc=ZXC[ch&63];if(cc=='#')cc='_';out[n++]=cc;if(ch&0x80)break;}out[n]=0;}
static void tape_finish(ringo_t*m){push(m,0);CPU->pc=0x0206;}  /* fim normal: SLOW/FAST e volta */
static int tape_traps(ringo_t*m){z80_t*c=CPU;
  if(c->pc==0x02FC){                      /* SAVE */
    int i,len; uint16_t eline=m->ram[0x14]|m->ram[0x15]<<8; char nm[32]; uint8_t buf[0x4000];
    zxname(m,HL(c),64,nm);
    len=eline-0x4009; if(len<0||len>0x3ff7)len=0;
    for(i=0;i<len;i++)buf[i]=RD(0x4009+i);
    if(m->cb_save)m->cb_save(m,nm,buf,len);
    tape_finish(m);return 1;}
  if(c->pc==0x0347){                      /* LOAD */
    if(c->c==0xff)m->tapename[0]=0;else zxname(m,DE(c),c->c+1,m->tapename);
    m->tape=TAPE_WAIT_LOAD;
    if(m->cb_load&&m->cb_load(m,m->tapename))return 1;   /* ja carregou */
    return 1;}                            /* fica esperando o seletor */
  return 0;}
void ringo_load_data(ringo_t*m,const uint8_t*d,int len){int i;
  if(len>0x3ff7)len=0x3ff7;
  for(i=0;i<len;i++)wr(m,0x4009+i,d[i]);
  m->tape=TAPE_IDLE;tape_finish(m);}
void ringo_load_cancel(ringo_t*m){m->tape=TAPE_IDLE;CPU->pc=0x03A6;}   /* relatorio D (BREAK) */

/* ------------------------------ maquina ----------------------------- */
void ringo_init(ringo_t*m,const uint8_t*rom){memset(m,0,sizeof(*m));if(!tabok)mktab();memcpy(m->rom,rom,8192);ringo_reset(m);}
void ringo_reset(ringo_t*m){z80_t*c=CPU;memset(c,0,sizeof(*c));c->a=c->f=0xff;c->ix=c->iy=0xffff;c->sp=0xffff;
  m->nmion=0;m->nmiacc=0;m->bank=0;m->tape=TAPE_IDLE;memset(m->ram,0,sizeof(m->ram));}
void ringo_set_cart(ringo_t*m,uint8_t*img,long len){m->cart=img;m->cartlen=img?len:0;m->bank=0;}
void ringo_frame(ringo_t*m){long tt=0;z80_t*c=CPU;
  while(tt<54166){int t;
    if(m->tape==TAPE_WAIT_LOAD)return;    /* parado esperando a fita */
    if(c->pc<0x2000&&(c->pc==0x02FC||c->pc==0x0347)&&tape_traps(m))continue;
    t=step(m);tt+=t;
    if(m->nmion){m->nmiacc+=t;if(m->nmiacc>=207){m->nmiacc-=207;tt+=nmi(m);}}
    if(c->iff1&&!c->eidelay&&!(c->r&0x40))tt+=irq(m);}}

/* tela: le o D_FILE (24 linhas de ate 32 caracteres) e gera 256x192 em
   1 bit por pixel (32 bytes por linha, bit 7 = pixel da esquerda, 1 = tinta) */
void ringo_render(ringo_t*m,uint8_t*pix){int x,y,l;uint16_t p=(m->ram[0x0C]|m->ram[0x0D]<<8)&0x7fff;int I=m->cpu.i&0xfe;
  memset(pix,0,RINGO_W*RINGO_H/8);
  if(p<0x4000)return;
  p++;
  for(y=0;y<24;y++){for(x=0;x<33;x++){uint8_t ch;if(p>=0x8000)return;ch=RD(p++);if(ch==0x76)break;
      if(x<32)for(l=0;l<8;l++){uint8_t b=RD((I<<8)|((ch&63)<<3)|l);if(ch&0x80)b^=0xff;pix[(y*8+l)*32+x]=b;}}
    if(x>=33){while(p<0x8000&&RD(p++)!=0x76);}}}

/* ------------------------------ teclado ----------------------------- */
static void upd_matrix(ringo_t*m){int k,shift=0;memset(m->matrix,0,8);
  for(k=0;k<40;k++)if(m->keycount[k])m->matrix[k/5]|=1<<(k%5);
#define COMBO(rk,key) if(m->keycount[rk]){shift=1;m->matrix[(key)/5]|=1<<((key)%5);}
  COMBO(RK_DELETE,RK_0) COMBO(RK_LEFT,RK_5) COMBO(RK_DOWN,RK_6) COMBO(RK_UP,RK_7) COMBO(RK_RIGHT,RK_8)
  COMBO(RK_FUNCTION,RK_ENTER) COMBO(RK_GRAPHICS,RK_9) COMBO(RK_EDIT,RK_1)
  if(shift)m->matrix[0]|=1;}
void ringo_key(ringo_t*m,int rk,int down){
  if(rk<0||rk>=RK_COUNT)return;
  if(rk==RK_INVVIDEO){if(down&&!m->keycount[rk])m->invert^=1;}
  if(down)m->keycount[rk]++;else if(m->keycount[rk])m->keycount[rk]--;
  upd_matrix(m);}
void ringo_keys_clear(ringo_t*m){memset(m->keycount,0,sizeof(m->keycount));upd_matrix(m);}
