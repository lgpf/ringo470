# Compilacao cruzada no Linux:  make   (precisa do DJGPP: i586-pc-msdosdjgpp-gcc e exe2coff)
# Gera um RINGODOS.EXE autossuficiente, com o CWSDPMI embutido (CWSDSTUB.EXE).
CC = i586-pc-msdosdjgpp-gcc
EXE2COFF = exe2coff
RINGODOS.EXE: ringo_dos.c ../src/ringo_ui.c ../src/ringo_core.c CWSDSTUB.EXE
	$(CC) -O2 -I../src -o R.EXE ringo_dos.c ../src/ringo_ui.c ../src/ringo_core.c
	$(EXE2COFF) R.EXE
	cat CWSDSTUB.EXE R > $@
	rm -f R R.EXE
clean:
	rm -f RINGODOS.EXE R R.EXE R.exe
