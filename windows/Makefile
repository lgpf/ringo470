# Compilacao cruzada no Linux:  make   (precisa de x86_64-w64-mingw32-gcc)
CC = x86_64-w64-mingw32-gcc
WINDRES = x86_64-w64-mingw32-windres
RINGOW.EXE: ringo_win.c ../src/ringo_ui.c ../src/ringo_core.c ringo_res.o
	$(CC) -O2 -I../src -o $@ $^ -mwindows -lgdi32 -lwinmm -static
ringo_res.o: ringo.rc ringo.ico
	$(WINDRES) ringo.rc -O coff -o $@
clean:
	rm -f RINGOW.EXE ringo_res.o
