# Ringo R-470 para MS-DOS puro — RINGODOS.EXE

Roda direto no DOS, sem Windows nem DOSBox (PCs antigos e PDVs: Pentium, SiS550, Vortex86...).

## Requisitos
386 ou melhor, VGA, MS-DOS 5+ ou FreeDOS. O CWSDPMI (r7) vai embutido no EXE pelo `CWSDSTUB.EXE`: basta executar o `RINGODOS.EXE`, sem outros arquivos.

## Compilar (DJGPP)
- **Windows** com DJGPP: `make.bat` nesta pasta.
- **Linux (cruzado)**: `make` com o `i586-pc-msdosdjgpp-gcc`.

O `make.bat`/`Makefile` compila, converte com `exe2coff` e junta o `CWSDSTUB.EXE` na frente.

Os fontes têm nomes longos: compile no Windows ou por compilação cruzada. O executável segue o padrão 8.3.

## Como funciona
- Vídeo VGA modo 12h (640 × 480); a tela de 512 × 384 fica centralizada e só as linhas que mudaram são copiadas.
- Teclado pela interrupção 9 (scancodes, teclas estendidas E0); layouts ABNT2 e US.
- 60 quadros por segundo pelo `uclock()`.
- `Shift+F10` volta ao DOS em modo texto.

Manual completo: [`../docs/manuais/emuladores/Manual_Emulador_MS-DOS.pdf`](../docs/manuais/emuladores/Manual_Emulador_MS-DOS.pdf).
