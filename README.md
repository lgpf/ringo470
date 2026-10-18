# Ringo R-470 — emulador

**RetroLabs — JL Martins e Luiz Pacheco — 2026**

Emulador do computador brasileiro **Ringo R-470** (clone do ZX81 com Z80 a 3,25 MHz) para **Windows**, **MS-DOS puro** e **ESP32 (TTGO VGA32 v1.4)**.

## Download

Os executáveis prontos estão nas [Releases](https://github.com/lgpf/ringo470/releases):

| Versão | Arquivo | Observação |
|---|---|---|
| Windows | `RINGOW.EXE` | Win32 64 bits, sem DLLs extras |
| MS-DOS | `RINGODOS.EXE` | MS-DOS 5.0+ ou FreeDOS, 386+ com VGA; CWSDPMI embutido, roda sozinho |
| ESP32 | `bootloader.bin`, `partitions.bin`, `boot_app0.bin`, `RINGO.bin` (ou `RINGO_COMPLETO.bin`) | TTGO VGA32 v1.4 com PSRAM; endereços abaixo |

### Gravação no ESP32 (ESP32 Download Tool, chip ESP32)

```
bootloader.bin   @ 0x1000
partitions.bin   @ 0x8000
boot_app0.bin    @ 0xE000
RINGO.bin        @ 0x10000
SPI SPEED 40MHz   SPI MODE DIO   FLASH SIZE 32Mbit
```

Ou um arquivo só: `RINGO_COMPLETO.bin @ 0x0` (`esptool.py --chip esp32 write_flash 0x0 RINGO_COMPLETO.bin`).
Se a placa já tem o DGT-100 ou o CP-500 M80 desta série, basta gravar o `RINGO.bin @ 0x10000`.

## Conteúdo

| Pasta | O que tem |
|---|---|
| [`src/`](src) | Núcleo comum: CPU Z80, máquina Ringo, interface (ajuda, configuração, arquivos, fita) e a ROM |
| [`windows/`](windows) | Emulador para Windows (`RINGOW.EXE`, Win32 puro, com ícone) |
| [`dos/`](dos) | Emulador para MS-DOS puro (`RINGODOS.EXE`, DJGPP + CWSDPMI embutido, VGA) |
| [`esp32/`](esp32) | Emulador para a TTGO VGA32 v1.4 (Arduino + FabGL; VGA, PS/2, cartão SD) |
| [`docs/manuais/emuladores/`](docs/manuais/emuladores) | Manuais em PDF |

## Uso

| Tecla | Função |
|---|---|
| F1 | Ajuda (4 páginas) |
| F5 | Configuração: cor, inverter frente/fundo, teclado ABNT/US/personalizado, pasta das fitas |
| F6 | Tabela de todas as teclas do Ringo e seus comandos |
| F9 | Carregar cartucho `.BIN` (4 KB ou multijogos até 256 KB) |
| F10 | Reset — **Shift+F10** sai (no ESP32, reinicia a placa) |
| Esc | BREAK |

Fita K7: `SAVE "NOME"` e `LOAD "NOME"` gravam e leem arquivos `.CAS` (formato `.P` do ZX81).

## Compilar

| Versão | Ferramenta | Comando |
|---|---|---|
| Windows | MinGW / TDM-GCC / MSYS2 | `windows\make.bat` (ou `make` no Linux com `x86_64-w64-mingw32-gcc`) |
| MS-DOS | DJGPP | `dos\make.bat` (ou `make` com `i586-pc-msdosdjgpp-gcc`); o EXE sai com o CWSDPMI embutido |
| ESP32 | Arduino IDE + core ESP32 2.0.x + FabGL 1.0.9 | rodar `esp32/atualiza_fontes`, abrir `esp32/ringo_esp32/ringo_esp32.ino`; placa *ESP32 Wrover Module* (PSRAM), *Huge APP* |

Detalhes em [`windows/LEIAME.md`](windows/LEIAME.md), [`dos/LEIAME.md`](dos/LEIAME.md), [`esp32/LEIAME.md`](esp32/LEIAME.md) e nos manuais:
[Windows](docs/manuais/emuladores/Manual_Emulador_Windows.pdf) ·
[MS-DOS](docs/manuais/emuladores/Manual_Emulador_MS-DOS.pdf) ·
[ESP32](docs/manuais/emuladores/Manual_Emulador_ESP32.pdf).

## Sobre a ROM

A ROM do Ringo R-470 está em `src/rom_ringo.h`. Ela pertence aos detentores dos direitos do Ringo; confira se pode distribuí-la antes de publicar o repositório.

## Créditos

RetroLabs — JL Martins e Luiz Pacheco — 2026.
