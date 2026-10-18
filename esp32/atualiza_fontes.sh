#!/bin/sh
# A Arduino IDE so compila arquivos dentro da pasta do sketch: copia os fontes comuns de ../src
cd "$(dirname "$0")"
for f in ringo_core.c ringo_core.h ringo_ui.c ringo_ui.h ringo_plat.h rom_ringo.h ui_font.h; do cp ../src/$f ringo_esp32/; done
echo "fontes de src/ copiados para ringo_esp32/"
