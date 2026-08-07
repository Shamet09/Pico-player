# Licenze del codice di terze parti

| Componente | Origine | Licenza | Dove sta |
|---|---|---|---|
| Pico SDK | `raspberrypi/pico-sdk` | BSD-3-Clause | dipendenza esterna (`PICO_SDK_PATH`) |
| pico-extras (`pico_audio`, `pico_audio_i2s`, `pico_util_buffer`) | `raspberrypi/pico-extras` | BSD-3-Clause | dipendenza esterna (`PICO_EXTRAS_PATH`) |
| FatFs (ff15) | ChaN, ridistribuito da `carlk3/no-OS-FatFS-SD-SPI-RPi-Pico` | licenza FatFs (BSD-like) — `lib/fatfs_spi/ff15/LICENSE.txt` | vendorizzato in `lib/fatfs_spi/` |
| Driver microSD SPI | `carlk3/no-OS-FatFS-SD-SPI-RPi-Pico` | Apache-2.0 — `lib/fatfs_spi/LICENSE` | vendorizzato in `lib/fatfs_spi/` |
| Decoder MP3 Helix | RealNetworks, ridistribuito da `pschatzmann/arduino-libhelix` | RPSL / RCSL — `lib/helix/RPSL.txt`, `lib/helix/RCSL.txt` | vendorizzato in `lib/helix/` |
| Font 5x7 "classic" | `adafruit/Adafruit-GFX-Library` (`glcdfont.c`) | BSD | vendorizzato in `src/font5x7.h` |

## Modifiche al codice di terze parti

Sono minime e tutte annotate nel file interessato:

1. **`lib/helix/assembly.h`** — aggiunta la macro `HELIX_PORTABLE_C` alla lista
   di piattaforme che usano la variante C portabile.
   *Perche':* il ramo `__GNUC__ && ARM` usa `smull` e operandi con barrel
   shifter (`eor %0,%2,%2,asr #31`), istruzioni ARMv5TE/DSP che il Cortex-M0+
   dell'RP2040 (ARMv6-M) non possiede. Definire `ARM` farebbe fallire la
   compilazione.

2. **`lib/helix/helix_memory.c`** — file nuovo, non presente nell'upstream.
   `buffers.c` include `utils/helix_memory.h`, che in arduino-libhelix e'
   implementato da un `.cpp` che tira dentro l'allocatore C++ e il logging del
   wrapper Arduino. Noi usiamo solo il decoder C puro, quindi
   `helix_malloc`/`helix_free` sono semplici wrapper su `malloc`/`free`.

3. **`src/font5x7.h`** — da `glcdfont.c`: rimossi gli `#include` per AVR/ESP e
   la macro `PROGMEM`, che su RP2040 non servono. I 1280 byte di dati
   (256 caratteri x 5 byte) sono identici all'originale.

4. **`lib/fatfs_spi/CMakeLists.txt`** — riscritto con percorsi assoluti
   (`CMAKE_CURRENT_LIST_DIR`) invece che relativi, ed escluso `ff_stdio.c`
   che non usiamo. I sorgenti C sono invariati, `ffconf.h` compreso
   (`FF_USE_LFN=3`, `FF_MAX_LFN=255`, `FF_FS_REENTRANT=0`).
