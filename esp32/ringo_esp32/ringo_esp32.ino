/* =====================================================================
   RINGO R-470 - emulador para ESP32 (placa TTGO VGA32 v1.4)
   RetroLabs - JL Martins e Luiz Pacheco - 2026

   Precisa da biblioteca FabGL. Placa: "ESP32 Wrover Module" (com PSRAM),
   Partition Scheme "Huge APP". Video VGA 512x384, teclado PS/2 e cartao SD.
   No cartao: os .BIN dos cartuchos e as fitas .CAS (a pasta e escolhida no F5).
   ===================================================================== */
#include "fabgl.h"
extern "C" {
#include "ringo_core.h"
#include "ringo_ui.h"
#include "ringo_plat.h"
}
#include "rom_ringo.h"

fabgl::VGA2Controller DisplayController;
fabgl::PS2Controller  PS2Controller;
static ringo_t M;
static uint8_t *lastfb;                     /* copia do ultimo quadro mostrado */
static uint32_t curInk = 1, curPap = 1;

SET_LOOP_TASK_STACK_SIZE(32 * 1024);       /* SAVE/LOAD usam bastante pilha */

/* ----------------------------- plataforma ----------------------------- */
extern "C" {
const char *plat_home(void) { return "/SD"; }
void plat_quit(void) { esp_restart(); }

/* codigo fisico = scancode PS/2 (conjunto 2), + 0x100 para teclas E0 */
static const char *scname(uint32_t r) {
  switch (r) {
    case 0x1C: return "A"; case 0x32: return "B"; case 0x21: return "C"; case 0x23: return "D"; case 0x24: return "E";
    case 0x2B: return "F"; case 0x34: return "G"; case 0x33: return "H"; case 0x43: return "I"; case 0x3B: return "J";
    case 0x42: return "K"; case 0x4B: return "L"; case 0x3A: return "M"; case 0x31: return "N"; case 0x44: return "O";
    case 0x4D: return "P"; case 0x15: return "Q"; case 0x2D: return "R"; case 0x1B: return "S"; case 0x2C: return "T";
    case 0x3C: return "U"; case 0x2A: return "V"; case 0x1D: return "W"; case 0x22: return "X"; case 0x35: return "Y";
    case 0x1A: return "Z"; case 0x45: return "0"; case 0x16: return "1"; case 0x1E: return "2"; case 0x26: return "3";
    case 0x25: return "4"; case 0x2E: return "5"; case 0x36: return "6"; case 0x3D: return "7"; case 0x3E: return "8";
    case 0x46: return "9"; case 0x5A: return "Enter"; case 0x29: return "Espaco"; case 0x12: return "Shift esq";
    case 0x59: return "Shift dir"; case 0x66: return "Backspace"; case 0x0D: return "Tab"; case 0x58: return "Caps Lock";
    case 0x14: return "Ctrl"; case 0x11: return "Alt"; case 0x76: return "Esc"; case 0x49: return "Ponto";
    case 0x41: return "Virgula"; case 0x16B: return "Seta esq"; case 0x174: return "Seta dir"; case 0x175: return "Seta cima";
    case 0x172: return "Seta baixo"; case 0x170: return "Insert"; case 0x171: return "Delete"; case 0x16C: return "Home";
    case 0x169: return "End"; case 0x17D: return "PgUp"; case 0x17A: return "PgDn";
  }
  return nullptr;
}
void plat_keyname(uint32_t raw, char *out, int max) {
  const char *n = scname(raw);
  if (n) snprintf(out, max, "%s", n); else snprintf(out, max, "SC %03X", (unsigned)raw);
}
uint32_t plat_default_raw(int rk) {
  static const uint16_t T[RK_COUNT] = {
    0x12, 0x1A, 0x22, 0x21, 0x2A,          /* SHIFT Z X C V */
    0x1C, 0x1B, 0x23, 0x2B, 0x34,          /* A S D F G */
    0x15, 0x1D, 0x24, 0x2D, 0x2C,          /* Q W E R T */
    0x16, 0x1E, 0x26, 0x25, 0x2E,          /* 1 2 3 4 5 */
    0x45, 0x46, 0x3E, 0x3D, 0x36,          /* 0 9 8 7 6 */
    0x4D, 0x44, 0x43, 0x3C, 0x35,          /* P O I U Y */
    0x5A, 0x4B, 0x42, 0x3B, 0x33,          /* ENTER L K J H */
    0x29, 0x49, 0x3A, 0x31, 0x32,          /* ESPACO . M N B */
    0x66, 0x16B, 0x174, 0x175, 0x172,      /* DELETE e setas */
    0x0D, 0x58, 0x170, 0x16C };            /* FUNCTION GRAPHICS INVERSE-VIDEO EDIT */
  return (rk >= 0 && rk < RK_COUNT) ? T[rk] : 0;
}
}

/* ----------------------------- teclado PS/2 --------------------------- */
/* caracteres de cada scancode: [0] sem shift, [1] com shift (layout US e ABNT2) */
struct K2 { uint8_t sc; char us0, us1, ab0, ab1; };
static const K2 KC[] = {
  {0x16,'1','!','1','!'},{0x1E,'2','@','2','@'},{0x26,'3','#','3','#'},{0x25,'4','$','4','$'},{0x2E,'5','%','5','%'},
  {0x36,'6','^','6',0},  {0x3D,'7','&','7','&'},{0x3E,'8','*','8','*'},{0x46,'9','(','9','('},{0x45,'0',')','0',')'},
  {0x4E,'-','_','-','_'},{0x55,'=','+','=','+'},{0x0E,'`','~','\'','"'},{0x54,'[','{',0,'`'}, {0x5B,']','}','[','{'},
  {0x5D,'\\','|',']','}'},{0x4C,';',':',0,0},  {0x52,'\'','"','~','^'},{0x41,',','<',',','<'},{0x49,'.','>','.','>'},
  {0x4A,'/','?',';',':'},{0x51,0,0,'/','?'},   {0x61,0,0,'\\','|'},{0x29,' ',' ',' ',' '},
  {0x7C,'*','*','*','*'},{0x7B,'-','-','-','-'},{0x79,'+','+','+','+'},{0x71,'.','.',',',','}};
static int hostShift = 0;
static int sc2hk(uint32_t r) {
  switch (r) {
    case 0x05: return HK_F1; case 0x06: return HK_F2; case 0x04: return HK_F3; case 0x0C: return HK_F4;
    case 0x03: return HK_F5; case 0x0B: return HK_F6; case 0x83: return HK_F7; case 0x0A: return HK_F8;
    case 0x01: return HK_F9; case 0x09: return HK_F10; case 0x78: return HK_F11; case 0x07: return HK_F12;
    case 0x76: return HK_ESC; case 0x5A: case 0x15A: return HK_ENTER; case 0x66: return HK_BACK; case 0x0D: return HK_TAB;
    case 0x29: return HK_SPACE; case 0x12: return HK_LSHIFT; case 0x59: return HK_RSHIFT; case 0x14: case 0x114: return HK_CTRL;
    case 0x11: case 0x111: return HK_ALT; case 0x58: return HK_CAPS; case 0x170: return HK_INS; case 0x171: return HK_DEL;
    case 0x16C: return HK_HOME; case 0x169: return HK_END; case 0x17D: return HK_PGUP; case 0x17A: return HK_PGDN;
    case 0x175: return HK_UP; case 0x172: return HK_DOWN; case 0x16B: return HK_LEFT; case 0x174: return HK_RIGHT;
  }
  static const struct { uint8_t sc; char c; } A[] = {
    {0x1C,'A'},{0x32,'B'},{0x21,'C'},{0x23,'D'},{0x24,'E'},{0x2B,'F'},{0x34,'G'},{0x33,'H'},{0x43,'I'},{0x3B,'J'},
    {0x42,'K'},{0x4B,'L'},{0x3A,'M'},{0x31,'N'},{0x44,'O'},{0x4D,'P'},{0x15,'Q'},{0x2D,'R'},{0x1B,'S'},{0x2C,'T'},
    {0x3C,'U'},{0x2A,'V'},{0x1D,'W'},{0x22,'X'},{0x35,'Y'},{0x1A,'Z'},{0x45,'0'},{0x16,'1'},{0x1E,'2'},{0x26,'3'},
    {0x25,'4'},{0x2E,'5'},{0x36,'6'},{0x3D,'7'},{0x3E,'8'},{0x46,'9'}};
  for (auto &a : A) if (a.sc == r) return a.c;
  return HK_OTHER;
}
static int sc2ch(uint32_t r, int hk) {
  if (hk >= 'A' && hk <= 'Z') return hk + 32;
  if (r & 0x100) { if (r == 0x14A) return '/'; return 0; }
  int abnt = (ui_kblayout() == 0);
  for (auto &k : KC) if (k.sc == r) return abnt ? (hostShift ? k.ab1 : k.ab0) : (hostShift ? k.us1 : k.us0);
  return 0;
}
static void pollKeyboard() {
  static int e0 = 0, f0 = 0;
  auto kb = PS2Controller.keyboard();
  if (!kb) return;
  while (kb->scancodeAvailable()) {
    int sc = kb->getNextScancode(0);
    if (sc < 0) break;
    if (sc == 0xE0) { e0 = 1; continue; }
    if (sc == 0xF0) { f0 = 1; continue; }
    if (sc == 0xE1 || sc == 0xAA || sc == 0xFA) { e0 = f0 = 0; continue; }
    uint32_t raw = (uint32_t)sc | (e0 ? 0x100 : 0);
    if (raw == 0x112) { e0 = f0 = 0; continue; }       /* shift falso do PrintScreen */
    int down = !f0;
    e0 = f0 = 0;
    int hk = sc2hk(raw);
    if (hk == HK_LSHIFT || hk == HK_RSHIFT) hostShift = down;
    int ch = down ? sc2ch(raw, hk) : 0;
    if (hk >= '0' && hk <= '9' && !hostShift) ch = hk;
    ui_key(hk, ch, raw, down);
  }
}

/* ----------------------------- video ---------------------------------- */
/* Copia o quadro do emulador (1 bit por pixel, MSB a esquerda) direto para o
   framebuffer do VGA2Controller, que usa o mesmo formato. As cores ficam so na
   paleta: indice 0 = fundo, 1 = tinta (inverter = so trocar a paleta). */
static void setColors(uint32_t ink, uint32_t pap) {
  DisplayController.setPaletteItem(0, RGB888(pap >> 16, (pap >> 8) & 255, pap & 255));
  DisplayController.setPaletteItem(1, RGB888(ink >> 16, (ink >> 8) & 255, ink & 255));
  curInk = ink; curPap = pap;
}
static void present() {
  uint32_t ink, pap;
  ui_colors(&ink, &pap);
  if (ink != curInk || pap != curPap) setColors(ink, pap);
  const uint8_t *f = ui_fb();
  for (int y = 0; y < UI_H; y++) {
    const uint8_t *src = f + y * (UI_W / 8);
    uint8_t *old = lastfb + y * (UI_W / 8);
    if (memcmp(src, old, UI_W / 8) == 0) continue;   /* linha nao mudou */
    memcpy(old, src, UI_W / 8);
    memcpy(DisplayController.getScanline(y), src, UI_W / 8);
  }
}

/* ----------------------------- programa ------------------------------- */
void setup() {
  Serial.begin(115200);
  PS2Controller.begin(PS2Preset::KeyboardPort0, KbdMode::NoVirtualKeys);
  DisplayController.begin();
  DisplayController.setResolution(VGA_512x384_60Hz);
  lastfb = (uint8_t *)malloc(UI_W * UI_H / 8);
  memset(lastfb, 0xAA, UI_W * UI_H / 8);   /* forca o primeiro desenho */
  fabgl::FileBrowser::mountSDCard(false, "/SD");
  ringo_init(&M, RINGO_ROM);
  ui_init(&M);
  setColors(0x000000, 0xF0F0F0);
}

void loop() {
  static uint32_t next = 0;
  uint32_t now = micros();
  if ((int32_t)(now - next) < 0) { pollKeyboard(); return; }
  next += 16667;                            /* 60 quadros por segundo */
  if ((int32_t)(now - next) > 100000) next = now;
  pollKeyboard();
  ui_frame();
  present();
}
