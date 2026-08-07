# PicoPlayer

Firmware in C (Pico SDK, non Arduino) per un lettore MP3 portatile basato su
Raspberry Pi Pico (RP2040): display OLED, microSD, DAC I2S, batteria 18650 e
quattro pulsanti.

Filosofia: **semplice, veloce, facile da usare, facile da mantenere.** Dove una
funzionalita' complicava l'interfaccia senza aggiungere valore vero, non e'
stata implementata.

> **Prima volta?** [GUIDA_DA_ZERO.docx](GUIDA_DA_ZERO.docx) e' una guida passo
> passo per principianti: da un PC appena formattato fino al Pico che suona,
> compresa la preparazione della microSD e cosa fare se qualcosa non va.
> Questo README e' invece la documentazione tecnica del progetto.

---

## 1. Hardware e piedinatura

Tutti i numeri qui sotto vivono in un solo posto nel codice: [src/config.h](src/config.h).

| Periferica | Segnale | Pin Pico | Note |
|---|---|---|---|
| OLED SSD1306 (I2C0) | SDA | GP0 | indirizzo 0x3C |
| OLED SSD1306 (I2C0) | SCL | GP1 | 400 kHz fast mode |
| microSD (SPI1 hw) | MISO | GP8 | |
| microSD (SPI1 hw) | CS | GP9 | |
| microSD (SPI1 hw) | SCK | GP10 | |
| microSD (SPI1 hw) | MOSI | GP11 | |
| DAC PCM5102 (I2S) | BCK | GP14 | deve essere adiacente a LCK |
| DAC PCM5102 (I2S) | LCK | GP15 | = BCK + 1, richiesto da `pico_audio_i2s` |
| DAC PCM5102 (I2S) | DIN | GP16 | |
| DAC PCM5102 | SCK (del DAC) | **GND** | modalita' clock interno, non e' un GPIO |
| Pulsante LEFT | | GP18 | usato |
| Pulsante RIGHT | | GP19 | usato |
| Pulsante UP | | GP20 | cablato ma non usato |
| Pulsante DOWN | | GP21 | cablato ma non usato |
| Pulsante OK | | GP22 | usato |
| Pulsante MENU | | GP26 | usato |
| Alimentazione | VSYS | pin 39 | da OUT+ del modulo IP5306 |
| Debug seriale (opz.) | UART0 TX / RX | GP12 / GP13 | 115200 8N1 |

I pulsanti sono attivi bassi con pull-up interno: un capo al GPIO, l'altro a GND.

**Perche' la seriale non e' su GP0/GP1** (dove il Pico SDK la mette di default):
quei pin sono occupati dall'I2C dell'OLED. E' stata spostata su GP12/GP13, che
il progetto non usa. La stdio su USB e' disattivata di proposito: richiederebbe
il submodule `tinyusb` del SDK, che cosi' non serve scaricare.

### Batteria

Senza un fuel gauge (es. MAX17048) **non e' possibile** leggere la percentuale
reale: il modulo IP5306 fornisce ~5 V regolati indipendentemente dallo stato
della cella, quindi anche misurare VSYS con l'ADC darebbe una lettura piatta
fino allo spegnimento. `battery_get_percent()` ritorna quindi `-1` e la UI
mostra `BAT --%`. La funzione e' isolata in [src/battery.c](src/battery.c):
aggiungendo un fuel gauge basta cambiare quel file.

---

## 2. Contenuto della microSD

```
/
├── Rock/            <- una cartella = una playlist
│   ├── 01 brano.mp3
│   └── 02 altro.mp3
├── Jazz/
│   └── ...
```

* Formato: FAT16 / FAT32 (exFAT non abilitato). Nomi lunghi supportati (UTF-8).
* Solo i file `.mp3` vengono elencati; file e cartelle nascosti o di sistema
  sono ignorati.
* Le liste sono ordinate alfabeticamente (case-insensitive), non nell'ordine
  in cui stanno sulla FAT.
* Limiti: 24 playlist, 200 brani per playlist (`PP_MAX_PLAYLISTS` /
  `PP_MAX_TRACKS` in `config.h`).
* **Se non ci sono sottocartelle** ma ci sono `.mp3` nella root, la root viene
  trattata come un'unica playlist chiamata "SD Root". E' una comodita' in piu'
  rispetto alle specifiche: chi copia i file alla buona sente comunque qualcosa
  invece di trovarsi uno schermo vuoto.

---

## 3. Schermate e comandi

All'avvio la riproduzione parte **in pausa**: si preme OK per iniziare.

### Home

```
+--------------------------------+
| (mascotte)            BAT --%  |
|                       VOL  60  |
|--------------------------------|
| Nome della canzone             |
| 1:23 / 3:45              3/12  |
| [====================--------] |
|--------------------------------|
| Nome playlist          SHUFFLE |
+--------------------------------+
```

### Song Browse

Si apre da Home con MENU (pressione breve); MENU fa da interruttore fra le due.
Torna a Home da sola dopo 30 secondi di inattivita'.

```
+--------------------------------+
| PLAY                (mascotte) |
|--------------------------------|
|  Canzone precedente            |
| >CANZONE ATTUALE<              |   <- evidenziata
|  Canzone successiva            |
|--------------------------------|
| Nome playlist          SHUFFLE |
+--------------------------------+
```

Precedente e successiva sono quelle dell'**ordine di ascolto reale**: con lo
shuffle attivo non sono i vicini alfabetici.

### Playlist Switch

Si apre da Home tenendo premuto MENU (600 ms).

```
+--------------------------------+
| PLAYLIST            (mascotte) |
|--------------------------------|
|  Playlist precedente           |
| >PLAYLIST SELEZIONATA<         |
|  Playlist successiva           |
|--------------------------------|
| OK=scegli MENU=esc             |
+--------------------------------+
```

### Tabella dei comandi

| Pulsante | Home | Song Browse | Playlist Switch |
|---|---|---|---|
| LEFT/RIGHT (click) | Volume ±5 | Seek ±10 s | Playlist prec./succ. |
| LEFT/RIGHT (hold) | Volume, raddoppia ogni tick, tetto 25 | Seek, raddoppia ogni tick, tetto 60 s | — |
| LEFT/RIGHT (doppio click) | — (disattivato) | Traccia prec./succ. | — |
| OK (click) | Play/Pausa | Play/Pausa | Conferma playlist |
| OK (hold) | Toggle shuffle/lineare | — | — |
| MENU (click) | Apre Song Browse | Torna a Home | Annulla, torna a Home |
| MENU (hold) | Apre Playlist Switch | — | — |

Timing (in `config.h`): debounce 25 ms, soglia hold 500 ms (MENU 600 ms), tick
di hold 400 ms, finestra doppio click 350 ms, timeout Browse 30 s.

Il doppio click e' attivo **solo** per LEFT/RIGHT in Song Browse. Altrove il
click viene emesso subito al rilascio, senza aspettare un eventuale secondo
click: altrimenti il volume in Home risulterebbe percettibilmente in ritardo.

### La mascotte

Un gattino in pixel art (20x16). Cambia espressione secondo questa priorita':

1. **Errore SD** (assente o mount fallito) — occhi a X, bocca aperta. Batte tutto.
2. **Splash "SD ok"** — faccina felice `^.^` per ~2 s dopo un mount riuscito.
3. **In pausa** — occhi socchiusi, bocca chiusa, statica.
4. **In riproduzione** — neutra `o.o` con un blink di 180 ms a intervalli
   casuali fra 4 e 9 secondi.

Anteprima dei cinque frame: [tools/mascot_preview.png](tools/mascot_preview.png).
I bitmap non sono trascritti a mano: [tools/gen_mascot.py](tools/gen_mascot.py)
li definisce come pixel art leggibile e genera `src/mascot_bitmaps.h` piu' il
PNG di controllo. Per rigenerarli: `python3 tools/gen_mascot.py`.

---

## 4. Architettura software

```
core0  (UI)                          core1  (audio)
  pulsanti -> button_fsm                SD -> FatFs -> Helix MP3 -> I2S
  ui.c (Home/Browse/Playlist)
  ssd1306 + mascotte
        |                                     |
        +-------- FIFO multicore (comandi) ---+
        +-------- g_shared (critical_section) +
```

* **core0** non tocca mai la microSD. **Solo core1** chiama funzioni `f_*`:
  FatFs e' configurato con `FF_FS_REENTRANT=0` e non e' thread-safe.
* I comandi vanno da core0 a core1 sulla FIFO hardware (2 word: tipo +
  parametro con segno): `PLAY_PAUSE`, `NEXT_TRACK`, `PREV_TRACK`, `SEEK`,
  `SELECT_PLAYLIST`, `SELECT_TRACK`, `SET_SHUFFLE`.
* Lo stato torna indietro in `g_shared` ([src/shared_state.h](src/shared_state.h)),
  protetto da una `critical_section_t`. Le sezioni critiche sono cortissime
  (poche centinaia di byte) per non disturbare la decodifica.
* Il volume e' l'unico campo scritto da core0 e letto da core1.

### Assegnazione delle risorse condivise

Sono i punti in cui due librerie diverse potevano litigare in silenzio:

| Risorsa | Assegnata a | Come |
|---|---|---|
| DMA canale 0 | audio I2S | `AUDIO_DMA_CHANNEL` in `config.h` |
| DMA canali 1-2 | driver SD | `dma_claim_unused_channel()`, dopo l'audio |
| `DMA_IRQ_0` | audio I2S | default di `pico_audio_i2s` |
| `DMA_IRQ_1` | driver SD | `set_spi_dma_irq_channel(true, true)` |
| PIO0 SM0 | audio I2S | `AUDIO_PIO_SM` |
| I2C0 | OLED | core0 |
| SPI1 | microSD | core1 |

L'audio viene inizializzato **prima** del driver SD, cosi' si prende il canale
DMA 0 prima che l'SD prenda i canali liberi. Ed entrambi i driver installano la
loro ISR **su core1**, perche' `irq_set_enabled()` agisce solo sul core che la
chiama e il driver SD aspetta il completamento del DMA su un semaforo che viene
rilasciato dalla sua ISR.

### Decodifica, durata e seek

* Il tag ID3v2 in testa al file viene saltato prima di cercare il primo sync
  word, altrimenti `MP3FindSyncWord` puo' agganciarsi a un falso sync dentro il
  tag.
* **Durata**: stimata dal bitrate del primo frame e dalla dimensione del file
  (`durata ≈ byte * 8 / bitrate`). Per i file **VBR e' solo un'approssimazione**:
  per un valore esatto servirebbe un indice, che per un lettore hobbistico non
  vale la complessita'.
* **Seek**: offset in byte proporzionale (`target/durata * dimensione`),
  `f_lseek`, poi riallineamento al primo `MP3FindSyncWord`. Sui VBR il salto
  e' quindi approssimativo quanto la durata.
* **Frequenza di campionamento**: viene letta da ogni frame e applicata al
  divisore del PIO, quindi funzionano anche file a 32/48 kHz e MPEG2/2.5. I
  file mono vengono duplicati sui due canali.
* **Volume**: guadagno quadratico (piu' vicino alla percezione dell'orecchio),
  applicato con uno shift a 8 bit nel loop di copia. Nessuna divisione per
  campione.
* **Fine playlist**: si riparte automaticamente dal primo brano (loop semplice).
* **Shuffle**: permutazione Fisher-Yates. Attivandolo a meta' riproduzione la
  canzone corrente resta in prima posizione, cosi' non si interrompe quello che
  si sta ascoltando. Disattivandolo si torna all'ordine alfabetico,
  riposizionati sulla canzone corrente.

### Occupazione di memoria

Dal `.elf` compilato: **136,5 KB di flash**, **36,6 KB di .bss**. A runtime si
aggiungono circa 30 KB di heap per i buffer del decoder Helix e circa 20 KB per
i buffer audio, allocati una sola volta all'avvio: **durante la riproduzione non
c'e' nessuna allocazione dinamica.** Totale ~87 KB dei 264 KB dell'RP2040.

Lo **stack di core1 e' portato a 4 KB** (il default del SDK e' 2 KB). Misurando
i frame con `-fstack-usage`, il percorso piu' profondo e' la scansione della SD
(`player_core1_main` 144 B + `playlist_scan_tracks` 88 B + `scan_dir` 488 B, che
contiene un `FILINFO` da 288 B, piu' la catena FatFs/driver SD) e sfiora i
1400 byte, a cui puo' annidarsi sopra la ISR del DMA audio. Con 2 KB restavano
circa 450 byte di margine: troppo pochi per una cosa che, se va storta, si
manifesta come un crash difficile da diagnosticare.

---

## 5. Compilare

### Prerequisiti (Ubuntu/Debian, anche in WSL)

```bash
sudo apt-get install cmake ninja-build build-essential \
     gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib \
     python3-pil
```

### Dipendenze esterne

Solo due: Pico SDK e pico-extras. Tutto il resto e' gia' in `lib/`.

```bash
./tools/setup_deps.sh          # clona in /opt/pico
export PICO_SDK_PATH=/opt/pico/pico-sdk
export PICO_EXTRAS_PATH=/opt/pico/pico-extras
```

Di pico-extras servono solo tre moduli, aggiunti singolarmente con
`add_subdirectory()`: `pico_util_buffer`, `pico_audio`, `pico_audio_i2s`.

### Build

```bash
./tools/build.sh              # -> build/picoplayer.uf2
```

Il default e' `Release`: senza ottimizzazione la decodifica MP3 non sta dietro
al tempo reale sul Cortex-M0+.

### Flash

Tenere premuto BOOTSEL, collegare l'USB, copiare `build/picoplayer.uf2` sulla
chiavetta `RPI-RP2` che compare.

### Test

```bash
./test/run_tests.sh           # gcc di sistema, niente toolchain ARM
```

### Editor: far sparire gli #include sottolineati in rosso

Gli include di `pico/*`, `hardware/*`, `ff.h` e `mp3dec.h` non si risolvono da
soli in un editor: i primi due stanno fuori dal repository (SDK e pico-extras),
gli altri in sottocartelle di `lib/` note solo a CMake, e alcuni header
(`pico/config_autogen.h`, `pico/version.h`) **non esistono affatto** finche' non
si compila, perche' li genera CMake nella directory di build.

La build produce quindi un `compile_commands.json`, che `tools/build.sh` copia
nella radice del progetto: e' da li' che l'editor ricava gli include path veri
invece di indovinarli.

```bash
./tools/build.sh        # genera/aggiorna compile_commands.json
```

`.vscode/c_cpp_properties.json` e' gia' configurato per leggerlo. Va usato
**aprendo la cartella in modalita' WSL** (estensione *WSL* di Microsoft, comando
"Connect to WSL"): i percorsi dentro `compile_commands.json` sono percorsi
Linux, quindi un VS Code che gira su Windows non li segue.

C'e' anche una configurazione di ripiego `PicoPlayer (Windows, ripiego)` che usa
percorsi `\\wsl.localhost\...`: funziona senza modalita' WSL, ma **contiene il
nome utente e il nome della distro**, quindi va adattata a mano su un'altra
macchina. Si sceglie dalla barra di stato in basso a destra di VS Code.

`compile_commands.json` e' in `.gitignore`: contiene percorsi assoluti della
macchina su cui e' stato compilato, va rigenerato in locale.

---

## 6. Struttura dei file

```
CMakeLists.txt
pico_sdk_import.cmake
hw_config.c                 config SPI/pin per la libreria SD
src/
  config.h                  TUTTI i pin e le costanti
  shared_state.h/.c         struct condivisa + protocollo comandi
  button_fsm.h/.c           macchina a stati pulsanti     (puro, testato)
  buttons.h/.c              glue GPIO -> button_fsm
  mascot_fsm.h/.c           logica mascotte               (puro, testato)
  mascot.h/.c               disegna la mascotte
  mascot_bitmaps.h          generato da tools/gen_mascot.py
  play_order.h/.c           shuffle/lineare               (puro, testato)
  ssd1306.h/.c              driver OLED
  font5x7.h                 vendorizzato da Adafruit-GFX
  playlist.h/.c             scansione SD (FatFs)
  player.h/.c               motore audio, gira su core1
  ui.h/.c                   schermate Home/Browse/PlaylistSwitch
  battery.h/.c              stub della percentuale batteria
  main.c                    init, lancio core1, loop core0
lib/
  helix/                    decoder MP3 Helix
  fatfs_spi/                FatFs ff15 + driver microSD SPI
  LICENSES.md               licenze e modifiche al codice di terze parti
test/
  test_buttons.c  test_mascot.c  test_play_order.c  run_tests.sh
tools/
  setup_deps.sh  build.sh  gen_mascot.py  mascot_preview.png
```

---

## 7. Cosa e' verificato e cosa no

### Verificato automaticamente

* **Build ARM completa** da pulito con `arm-none-eabi-gcc` 14.2.1 + Pico SDK
  2.3.0, `-O3 -DNDEBUG`, **zero warning** sui sorgenti del progetto. Prodotto
  un `.uf2` valido (265 KB, magic e family id RP2040 verificati).
* **Occupazione di stack misurata** con `-fstack-usage` sul percorso piu'
  profondo di core1, che ha portato ad alzare `PICO_CORE1_STACK_SIZE` a 4 KB.
* **116 controlli** di unit test nativi, tutti verdi:
  * `test_buttons` (47): reiezione dei glitch sotto la soglia di debounce,
    rimbalzi ripetuti del contatto, click singolo immediato senza doppio click,
    click ritardato di 350 ms con doppio click attivo, doppio click, secondo
    click fuori finestra, hold con tick a 400 ms, hold che non genera click al
    rilascio, soglia dedicata di MENU, indipendenza fra pulsanti, nessun click
    fantasma dopo un hold, comportamento con polling rado (33 ms),
    accelerazione esponenziale con tetto.
  * `test_mascot` (23): priorita' dell'errore SD, durata dello splash,
    espressione statica in pausa, intervallo e durata dei blink, determinismo a
    parita' di seed, nessun blink immediato alla ripresa dopo una pausa lunga.
  * `test_play_order` (46): ordine lineare con wrap, permutazione shuffle
    valida, conservazione del brano corrente attivando/disattivando lo shuffle
    da ognuna delle 20 posizioni, `peek_next`/`peek_prev`, cambio playlist,
    casi limite 0 e 1 brano, limite `PP_MAX_TRACKS`.
* **API delle librerie verificate sugli header realmente scaricati**, non a
  memoria: `audio_i2s_config_t`, `audio_new_producer_pool`, `audio_buffer_t`,
  `mp3dec.h`, `spi_t`/`sd_card_t`, `ffconf.h`. Da questa verifica sono usciti
  quattro problemi reali che sarebbero stati bug al primo avvio, elencati nella
  sezione 8.
* **Un solo core chiama FatFs**: verificato meccanicamente che nessun `f_*`
  compaia fuori da `playlist.c` e `player.c`, che girano solo su core1.

### Richiede necessariamente prova sull'hardware reale

Nessuna di queste cose e' verificabile senza il dispositivo montato:

* **Corrispondenza dei pin con il cablaggio fisico** — il firmware usa la
  piedinatura del documento di progetto, ma va confrontata con la scheda vera.
* **Indirizzo I2C del display**: assunto 0x3C. Alcuni moduli usano 0x3D: in quel
  caso cambiare `OLED_I2C_ADDR` in `config.h`. Se il display resta nero e sulla
  seriale compare "OLED non risponde", e' quasi certamente questo.
* **Qualita' audio e assenza di scatti.** La decodifica MP3 sul Cortex-M0+ e'
  la parte piu' esigente: a 125 MHz un 44.1 kHz stereo fino a ~192 kbps deve
  stare comodo su core1, ma solo la prova reale lo conferma. Se con file a
  256/320 kbps si sentono scatti, impostare `PP_SYS_CLOCK_KHZ` a `200000` in
  `config.h`.
* **Timing dei pulsanti come lo percepisce il dito.** Le soglie sono verificate
  matematicamente dagli unit test, ma se l'hold sembra troppo pronto o troppo
  lento sono quattro costanti in `config.h`.
* **Lettura della microSD**: velocita' SPI (12,5 MHz), qualita' del cablaggio e
  della scheda. Se il mount fallisce, provare ad abbassare `SD_BAUD_RATE`.
* **Corrispondenza L/R del DAC** e livello di uscita.
* **Autonomia della batteria** e comportamento del modulo IP5306 sotto carico.

---

## 8. Scelte tecniche e scostamenti dal documento di progetto

Il documento chiedeva di seguirlo alla lettera, salvo verificare gli header
reali quando qualcosa non combaciava. La verifica ha fatto emergere quattro
punti che, presi alla lettera, avrebbero prodotto un firmware che non funziona
al primo tentativo. Sono elencati qui in modo che la scelta sia rivedibile.

**1. Init della SD e scansione iniziale su core1, non su core0.**
Il documento diceva di fare la scansione delle playlist su core0 prima di
lanciare core1. Ma il driver SD aspetta il completamento del DMA su un semaforo
rilasciato dalla sua ISR, e `irq_set_enabled()` agisce solo sul core che la
chiama: inizializzando su core0 e leggendo poi da core1, ogni lettura
dipenderebbe da una ISR che gira sull'altro core. Facendo tutto su core1 la
regola d'oro ("un solo core tocca FatFs") vale in modo ancora piu' stretto:
core0 non chiama `f_*` mai, nemmeno all'avvio. Nel frattempo core0 mostra
"Lettura SD..." (poche centinaia di ms).

**2. Il decoder Helix non viene reinizializzato dopo un seek.**
Il documento suggeriva `MP3FreeDecoder` + `MP3InitDecoder` dopo un salto.
Leggendo `mp3dec.c` si vede che dopo una discontinuita' `MP3Decode` ritorna
`ERR_MP3_MAINDATA_UNDERFLOW` **avanzando comunque il puntatore di input**: si
riallinea da solo in un paio di frame. Reinizializzare significherebbe una
free+malloc da ~30 KB in piena riproduzione. Il risultato udibile e' lo stesso,
il rischio no.

**3. Il puntatore di input, non `bytesLeft`, e' la fonte di verita'.**
In `MP3Decode`, sul percorso d'errore `ERR_MP3_INVALID_SIDEINFO`, `*inbuf`
viene avanzato ma `*bytesLeft` no: i due restano disallineati di 4 byte. Il
codice ricalcola quindi la posizione da `rp` dopo ogni chiamata, invece di
fidarsi di `bytesLeft`. Senza questo accorgimento si legge oltre la fine del
buffer su file con frame corrotti.

**4. Serve anche `pico_util_buffer` da pico-extras.**
Il documento parlava di aggiungere `pico_audio` e `pico_audio_i2s`. In realta'
`pico_audio_headers` dipende da `pico_util_buffer`, che sta anch'esso in
pico-extras: senza quel terzo `add_subdirectory()` il link fallisce.

Inoltre:

**5. Il flush dell'OLED e' spezzato in pagine.** Un frame intero a 400 kHz dura
~23 ms: lasciare il debounce senza campioni cosi' a lungo fa perdere le
pressioni piu' rapide. `ssd1306_show()` accetta una callback chiamata fra una
pagina e l'altra, e la UI le passa `buttons_poll`.

**6. Il decoder Helix e' compilato in C portabile, non in assembly ARM.** Il
ramo `__GNUC__ && ARM` di `assembly.h` usa `smull` e operandi con barrel
shifter: istruzioni ARMv5TE/DSP che il Cortex-M0+ non ha. Dettagli in
[lib/LICENSES.md](lib/LICENSES.md).

**7. `Song Browse` mostra i vicini nell'ordine di ascolto**, non in ordine
alfabetico, cosi' con lo shuffle attivo "successiva" e' davvero la prossima che
si sentira'.

**8. Fallback sulla root della SD** quando non ci sono sottocartelle (sezione 2).

### Assunzioni ereditate dal documento, da confermare

* Indirizzo I2C del display: 0x3C.
* All'avvio si parte in pausa; l'utente preme OK.
* Percentuale batteria non disponibile con l'hardware attuale.
* Il menu piu' ampio previsto in `Architecture.md`
  (Favorites/History/Settings/About) **non** e' implementato: solo le tre
  schermate descritte sopra.
* Nessuna funzione "repeat" dedicata: a fine playlist si riparte dal primo brano.
* Il titolo mostrato e' il **nome del file** senza estensione, non il tag ID3.
