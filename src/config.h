/* config.h - TUTTI i pin e le costanti di progetto in un solo posto.
 *
 * PicoPlayer - lettore MP3 portatile su Raspberry Pi Pico (RP2040).
 * Vedi README.md per la mappatura completa dell'hardware.
 */
#ifndef PP_CONFIG_H
#define PP_CONFIG_H

/* ------------------------------------------------------------------ */
/* Pin                                                                 */
/* ------------------------------------------------------------------ */

/* OLED SSD1306 128x64 su I2C0 */
#define PIN_I2C_SDA          0
#define PIN_I2C_SCL          1
#define OLED_I2C_INST        i2c0
#define OLED_I2C_ADDR        0x3C     /* alcuni moduli usano 0x3D */
#define OLED_I2C_BAUD        400000   /* fast mode */
#define OLED_WIDTH           128
#define OLED_HEIGHT          64

/* microSD su SPI1 (hardware) */
#define PIN_SD_MISO          8
#define PIN_SD_CS            9
#define PIN_SD_SCK           10
#define PIN_SD_MOSI          11
/* 6,25 MHz invece dei 12,5 MHz iniziali. Su cavetti dupont e breadboard
 * 12,5 MHz sono al limite: il CRC del driver intercetta il disturbo e la
 * lettura fallisce, in modo apparentemente casuale. Di banda ne avanza
 * comunque tantissimo: un MP3 a 320 kbps vuole 40 KB/s, qui ce ne sono
 * centinaia. Se la SD dovesse dare ancora problemi, scendere a 4000*1000. */
#define SD_BAUD_RATE         (6250 * 1000)

/* Recupero dagli errori di I/O della SD (vedi player.c).
 * SD_IO_RETRIES  : riletture dello stesso blocco prima di dichiarare guasto.
 * SD_RECOVER_MS  : intervallo fra due tentativi di rimontare la scheda.
 * SD_OPEN_TRIES  : quante tracce provare ad aprire prima di arrendersi. Con la
 *                  SD guasta ogni f_open puo' costare fino a un secondo di
 *                  timeout: provarle tutte e 200 bloccherebbe il player per
 *                  minuti, con lo schermo fermo. */
#define SD_IO_RETRIES        3
#define SD_RECOVER_MS        2000
#define SD_OPEN_TRIES        8

/* DAC PCM5102 su I2S (PIO).
 * LCK deve essere BCK+1: e' un requisito di pico_audio_i2s
 * (clock_pin_base = BCK, clock_pin_base+1 = LCK).
 * Il pin SCK del DAC va a GND (clock interno), non e' un GPIO. */
#define PIN_I2S_BCK          14
#define PIN_I2S_LCK          (PIN_I2S_BCK + 1)   /* 15, implicito */
#define PIN_I2S_DIN          16

/* Pulsanti (attivi bassi, pull-up interno).
 * UP e DOWN sono cablati ma non usati dal firmware. */
#define PIN_BTN_LEFT         18
#define PIN_BTN_RIGHT        19
#define PIN_BTN_UP           20   /* cablato, non usato */
#define PIN_BTN_DOWN         21   /* cablato, non usato */
#define PIN_BTN_OK           22
#define PIN_BTN_MENU         26

/* ------------------------------------------------------------------ */
/* Timing pulsanti (millisecondi)                                      */
/* ------------------------------------------------------------------ */
#define BTN_DEBOUNCE_MS      25
#define BTN_HOLD_MS          500    /* soglia hold di default */
#define BTN_HOLD_MENU_MS     600    /* soglia dedicata per MENU */
#define BTN_HOLD_TICK_MS     400
#define BTN_DCLICK_MS        350    /* solo LEFT/RIGHT in Song Browse */

#define UI_BROWSE_TIMEOUT_MS 30000  /* Song Browse -> Home per inattivita' */

/* ------------------------------------------------------------------ */
/* Clock di sistema                                                    */
/* ------------------------------------------------------------------ */
/* 0 = clock di fabbrica (125 MHz), che e' il default: nessun overclock,
 * massima probabilita' che funzioni al primo colpo.
 *
 * La decodifica MP3 gira sul Cortex-M0+ senza istruzioni DSP: a 125 MHz un
 * file 44.1 kHz stereo fino a ~192 kbps sta comodamente dentro il tempo
 * reale su core1. Se con file a bitrate alto (256/320 kbps) si sentissero
 * scatti, mettere qui 200000 (200 MHz): e' un overclock ampiamente usato sui
 * progetti audio RP2040. Il valore va impostato PRIMA di inizializzare
 * l'I2S, cosa che main.c fa gia'. */
#define PP_SYS_CLOCK_KHZ     0

/* ------------------------------------------------------------------ */
/* Audio                                                               */
/* ------------------------------------------------------------------ */
#define AUDIO_SAMPLE_FREQ    44100   /* frequenza iniziale del clock I2S */
#define AUDIO_BUFFER_FRAMES  1152    /* frame (coppie L/R) per buffer     */
#define AUDIO_BUFFER_COUNT   4       /* ~104 ms di coda a 44.1 kHz        */
#define AUDIO_DMA_CHANNEL    0       /* canale DMA riservato all'I2S      */
#define AUDIO_PIO_SM         0

/* ------------------------------------------------------------------ */
/* Modo test audio                                                     */
/* ------------------------------------------------------------------ */
/* Tenendo premuto MENU all'accensione il firmware NON tocca la microSD e non
 * decodifica nulla: manda all'I2S un tono continuo generato al volo. Serve a
 * separare due guasti che dall'esterno si assomigliano:
 *
 *   tono si sente  -> I2S, PIO, DMA e DAC funzionano: il problema sta a monte
 *                     (SD, decodifica, catena dei buffer).
 *   tono NON si sente -> il problema e' a valle del Pico: cablaggio BCK/LCK/DIN,
 *                     SCK del DAC non a massa, alimentazione, oppure il pin
 *                     XSMT del PCM5102 tenuto basso (= soft mute). */
#define PP_SELFTEST_TONE_HZ  440

/* Volume: 0..100, passo base 5, con accelerazione durante l'hold. */
#define VOLUME_DEFAULT       60
#define VOLUME_STEP          5
#define VOLUME_STEP_CAP      25
#define VOLUME_MAX           100

/* Seek: passo base 10 s, con accelerazione durante l'hold. */
#define SEEK_STEP_SEC        10
#define SEEK_STEP_CAP_SEC    60

/* ------------------------------------------------------------------ */
/* Limiti (regolabili)                                                 */
/* ------------------------------------------------------------------ */
#define PP_MAX_PLAYLISTS     24
#define PP_MAX_TRACKS        200
#define PP_NAME_LEN          64    /* nome file/cartella, UTF-8, con NUL */

/* Dimensione del buffer di lettura MP3 grezzo.
 * MAINBUF_SIZE di Helix e' 1940: teniamo abbondantemente sopra. */
#define MP3_INBUF_SIZE       8192
/* Blocco letto da SD ad ogni refill (multiplo di 512). */
#define SD_READ_CHUNK        4096

#endif /* PP_CONFIG_H */
