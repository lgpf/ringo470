/* ===================================================================
   RINGO R-470 - nucleo do emulador (Z80 + maquina estilo ZX81)
   RetroLabs - JL Martins e Luiz Pacheco - 2026
   Portavel: usado igual no Windows e no ESP32.
   =================================================================== */
#ifndef RINGO_CORE_H
#define RINGO_CORE_H
#include <stdint.h>

#define RINGO_W 256
#define RINGO_H 192

/* teclas do Ringo: 0..39 = matriz (linha*5 + bit), 40.. = teclas extras */
enum {
  RK_SHIFT=0, RK_Z, RK_X, RK_C, RK_V,
  RK_A, RK_S, RK_D, RK_F, RK_G,
  RK_Q, RK_W, RK_E, RK_R, RK_T,
  RK_1, RK_2, RK_3, RK_4, RK_5,
  RK_0, RK_9, RK_8, RK_7, RK_6,
  RK_P, RK_O, RK_I, RK_U, RK_Y,
  RK_ENTER, RK_L, RK_K, RK_J, RK_H,
  RK_SPACE, RK_DOT, RK_M, RK_N, RK_B,
  RK_DELETE, RK_LEFT, RK_RIGHT, RK_UP, RK_DOWN,
  RK_FUNCTION, RK_GRAPHICS, RK_INVVIDEO, RK_EDIT,
  RK_COUNT
};

typedef struct {
  uint8_t a,f,b,c,d,e,h,l, a_,f_,b_,c_,d_,e_,h_,l_;
  uint16_t ix,iy,sp,pc;
  uint8_t i,r,iff1,iff2,im,halted,eidelay;
  int idx, ea_; int t_;
} z80_t;

/* estados da fita */
enum { TAPE_IDLE=0, TAPE_WAIT_LOAD };

typedef struct ringo_s {
  z80_t cpu;
  uint8_t rom[8192];
  uint8_t ram[0x4000];           /* 4000h-7FFFh (16 KB) */
  uint8_t *cart; long cartlen;   /* cartucho 4 KB, ou imagem multijogos (n x 4 KB) */
  int bank;
  uint8_t keycount[RK_COUNT];    /* teclas apertadas (contagem por origem) */
  uint8_t matrix[8];
  int nmion, nmiacc;
  int invert;                    /* tecla INVERSE VIDEO */
  /* fita */
  int tape;
  char tapename[32];
  int (*cb_save)(struct ringo_s *m,const char *name,const uint8_t *data,int len);
  int (*cb_load)(struct ringo_s *m,const char *name); /* 1 = carregou, 0 = abrir seletor */
  void *user;
} ringo_t;

void ringo_init(ringo_t *m, const uint8_t *rom);
void ringo_reset(ringo_t *m);
void ringo_set_cart(ringo_t *m, uint8_t *img, long len);  /* NULL = sem cartucho */
void ringo_frame(ringo_t *m);                              /* 1/60 s */
void ringo_render(ringo_t *m, uint8_t *pix);               /* 256x192, 1 bit por pixel (6144 bytes) */
void ringo_key(ringo_t *m, int rk, int down);
void ringo_keys_clear(ringo_t *m);
uint8_t ringo_rd(ringo_t *m, uint16_t a);
/* fita: completar um LOAD que ficou esperando o seletor */
void ringo_load_data(ringo_t *m, const uint8_t *data, int len);
void ringo_load_cancel(ringo_t *m);
#endif
