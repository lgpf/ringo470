@echo off
rem RINGOW.EXE - emulador do Ringo R-470 para Windows (MinGW, TDM-GCC ou MSYS2)
windres ringo.rc -O coff -o ringo_res.o
gcc -O2 -I..\src -o RINGOW.EXE ringo_win.c ..\src\ringo_ui.c ..\src\ringo_core.c ringo_res.o -mwindows -lgdi32 -lwinmm
if errorlevel 1 goto erro
echo Pronto: RINGOW.EXE
goto fim
:erro
echo Erro na compilacao.
:fim
