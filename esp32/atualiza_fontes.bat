@echo off
rem A Arduino IDE so compila arquivos dentro da pasta do sketch: copia os fontes comuns de ..\src
for %%f in (ringo_core.c ringo_core.h ringo_ui.c ringo_ui.h ringo_plat.h rom_ringo.h ui_font.h) do copy /y ..\src\%%f ringo_esp32\ >nul
echo fontes de src copiados para ringo_esp32
