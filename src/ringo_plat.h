/* RINGO R-470 - interface com a plataforma (Windows ou ESP32)
   Cada plataforma implementa estas funcoes. */
#ifndef RINGO_PLAT_H
#define RINGO_PLAT_H
#include <stdint.h>
/* lista um diretorio: chama cb para cada entrada (sem "." e ".."). dir "" = raiz/unidades */
int  plat_list(const char *dir, void (*cb)(const char *name, int isdir, void *u), void *u);
long plat_load(const char *path, uint8_t *buf, long max);
int  plat_save(const char *path, const uint8_t *buf, long len);
int  plat_del(const char *path);
char plat_sep(void);
const char *plat_home(void);          /* pasta inicial */
int  plat_isroot(const char *dir);    /* 1 = nao da para subir */
int  plat_cfg_read(char *buf, int max);
int  plat_cfg_write(const char *buf);
void plat_keyname(uint32_t raw, char *out, int max);  /* nome de uma tecla fisica */
uint32_t plat_default_raw(int rk);    /* tecla fisica padrao de cada tecla do Ringo */
void plat_quit(void);
#endif
