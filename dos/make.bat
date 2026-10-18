@echo off
rem RINGODOS.EXE - emulador do Ringo R-470 para MS-DOS (DJGPP), com o CWSDPMI embutido
gcc -O2 -I..\src -o R.EXE ringo_dos.c ..\src\ringo_ui.c ..\src\ringo_core.c
if errorlevel 1 goto erro
exe2coff R.EXE
copy /b CWSDSTUB.EXE+R RINGODOS.EXE >nul
del R
del R.EXE
echo Pronto: RINGODOS.EXE (roda sozinho, sem CWSDPMI.EXE)
goto fim
:erro
echo Erro na compilacao.
:fim
