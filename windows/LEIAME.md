# Ringo R-470 para Windows — RINGOW.EXE

Programa Win32 puro (sem DLLs extras), janela redimensionável em 4:3.

## Compilar
- **Windows**: instale o TDM-GCC ou o MSYS2 (`mingw-w64-x86_64-gcc`) e rode `make.bat` nesta pasta.
- **Linux (cruzado)**: `make` com o `x86_64-w64-mingw32-gcc`.

```
windres ringo.rc -O coff -o ringo_res.o
gcc -O2 -I..\src -o RINGOW.EXE ringo_win.c ..\src\ringo_ui.c ..\src\ringo_core.c ringo_res.o -mwindows -lgdi32 -lwinmm
```

## Usar
Rode `RINGOW.EXE`. O `RINGO.CFG` é criado ao lado do executável. F1 abre a ajuda.
Manual completo: [`../docs/manuais/emuladores/Manual_Emulador_Windows.pdf`](../docs/manuais/emuladores/Manual_Emulador_Windows.pdf).
