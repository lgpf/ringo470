/* RINGO R-470 - interface do emulador (ajuda, configuracao, arquivos, fita)
   RetroLabs - JL Martins e Luiz Pacheco - 2026 */
#ifndef RINGO_UI_H
#define RINGO_UI_H
#include <stdint.h>
#include "ringo_core.h"

#define UI_W 512
#define UI_H 384
/* teclas "com nome" vindas da plataforma (o resto: 'A'..'Z', '0'..'9' ou HK_OTHER) */
enum { HK_NONE=0, HK_OTHER=1,
  HK_F1=0x100,HK_F2,HK_F3,HK_F4,HK_F5,HK_F6,HK_F7,HK_F8,HK_F9,HK_F10,HK_F11,HK_F12,
  HK_ESC,HK_ENTER,HK_BACK,HK_TAB,HK_SPACE,HK_LSHIFT,HK_RSHIFT,HK_CTRL,HK_ALT,HK_CAPS,
  HK_INS,HK_DEL,HK_HOME,HK_END,HK_PGUP,HK_PGDN,HK_UP,HK_DOWN,HK_LEFT,HK_RIGHT };

void ui_init(ringo_t *m);
/* evento de tecla: hk = tecla com nome; ch = caractere digitado (0 se nenhum); raw = codigo fisico */
void ui_key(int hk, int ch, uint32_t raw, int down);
void ui_frame(void);                   /* roda 1/60 s e monta a tela */
const uint8_t *ui_fb(void);            /* 512x384, 1 bit por pixel (64 bytes por linha) */
void ui_colors(uint32_t *ink, uint32_t *paper);   /* cores RGB 0xRRGGBB */
void ui_focus_lost(void);              /* solta todas as teclas */
int  ui_kblayout(void);                /* 0 ABNT, 1 US, 2 personalizado */
#endif
