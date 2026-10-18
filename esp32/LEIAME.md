# Ringo R-470 para ESP32 — placa TTGO VGA32 v1.4

VGA 512 × 384, teclado PS/2 (ABNT2 ou US) e cartão SD.

## Compilar
1. Arduino IDE com o suporte a ESP32 e a biblioteca **FabGL**.
2. Rode `atualiza_fontes.sh` (ou `atualiza_fontes.bat`): copia os fontes comuns de `../src` para `ringo_esp32/` (a Arduino IDE só compila arquivos dentro da pasta do sketch).
3. Abra `ringo_esp32/ringo_esp32.ino`.
4. Placa **ESP32 Wrover Module**, PSRAM habilitada, Partition Scheme **Huge APP**. Compile e grave.

## Cartão SD
A raiz do cartão é a pasta inicial do F9 e das fitas; o `RINGO.CFG` fica na raiz. Cartuchos de 256 KB usam a PSRAM (sem PSRAM, carrega os primeiros 128 KB).

Manual completo: [`../docs/manuais/emuladores/Manual_Emulador_ESP32.pdf`](../docs/manuais/emuladores/Manual_Emulador_ESP32.pdf).
