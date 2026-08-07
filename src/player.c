#include "player.h"

#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/audio_i2s.h"
#include "pico/multicore.h"
#include "pico/time.h"

#include "ff.h"
#include "hw_config.h"
#include "mp3dec.h"
#include "spi.h"

#include "config.h"
#include "play_order.h"
#include "playlist.h"
#include "shared_state.h"

/* ------------------------------------------------------------------ */
/* Stato del motore audio                                              */
/* ------------------------------------------------------------------ */

/* Buffer PCM di una frame MP3: MAX_NGRAN * MAX_NSAMP * MAX_NCHAN = 2*576*2. */
static int16_t s_pcm[MAX_NGRAN * MAX_NSAMP * MAX_NCHAN];
static int     s_pcm_frames;   /* frame (coppie L/R) disponibili in s_pcm */
static int     s_pcm_taken;    /* frame gia' copiati verso l'I2S          */
static int     s_pcm_chans;

static uint8_t s_in[MP3_INBUF_SIZE];   /* MP3 grezzo letto da SD */
static int     s_in_len;               /* byte validi            */
static int     s_in_pos;               /* byte gia' consumati    */

static HMP3Decoder        s_dec;
static audio_buffer_pool_t *s_pool;
static audio_buffer_t      *s_out;      /* buffer I2S in compilazione */
static uint32_t             s_out_pos;  /* frame gia' scritti dentro  */
static uint32_t             s_i2s_rate; /* frequenza attualmente impostata sul PIO */
static int32_t              s_gain;     /* 0..256, applicato ai campioni */
static uint8_t              s_volume;

static struct {
    FIL      f;
    bool     open;
    uint32_t data_start;      /* offset del primo frame (dopo ID3v2) */
    uint32_t data_size;
    uint32_t bitrate;         /* bit/s del primo frame               */
    uint32_t samprate;
    int      channels;
    uint32_t duration_sec;    /* stima; per i VBR e' approssimata    */
    uint64_t samples_played;
    bool     eof;
} s_track;

static play_order_t s_order;
static int          s_playlist_index;
static bool         s_playing;
static bool         s_have_track;

/* ------------------------------------------------------------------ */
/* Utilita'                                                            */
/* ------------------------------------------------------------------ */

static void set_gain_from_volume(uint8_t vol)
{
    if (vol > VOLUME_MAX) {
        vol = VOLUME_MAX;
    }
    s_volume = vol;
    /* Curva quadratica: piu' vicina alla percezione dell'orecchio di una
     * rampa lineare, e resta un semplice shift a 8 bit nel loop audio. */
    s_gain = ((int32_t)vol * (int32_t)vol * 256) / (VOLUME_MAX * VOLUME_MAX);
}

/* Aggiorna il divisore del PIO per cambiare frequenza di campionamento senza
 * reinizializzare l'I2S. E' esattamente il calcolo che pico_audio_i2s fa
 * internamente in update_pio_frequency() (la funzione non e' esportata). */
static void i2s_set_rate(uint32_t rate)
{
    if (rate == 0 || rate == s_i2s_rate) {
        return;
    }
    uint32_t sys     = clock_get_hz(clk_sys);
    uint32_t divider = sys * 4 / rate;
    pio_sm_set_clkdiv_int_frac(pio0, AUDIO_PIO_SM,
                               (uint16_t)(divider >> 8u), (uint8_t)(divider & 0xffu));
    s_i2s_rate = rate;
}

static uint32_t track_elapsed_sec(void)
{
    if (!s_track.open || s_track.samprate == 0) {
        return 0;
    }
    return (uint32_t)(s_track.samples_played / s_track.samprate);
}

/* ------------------------------------------------------------------ */
/* Lettura del file                                                    */
/* ------------------------------------------------------------------ */

static void in_reset(void)
{
    s_in_len = 0;
    s_in_pos = 0;
}

static bool in_refill(void)
{
    UINT br = 0;

    if (!s_track.open) {
        return false;
    }
    /* compatta: sposta i byte non consumati all'inizio */
    if (s_in_pos > 0) {
        memmove(s_in, s_in + s_in_pos, (size_t)(s_in_len - s_in_pos));
        s_in_len -= s_in_pos;
        s_in_pos = 0;
    }
    int space = MP3_INBUF_SIZE - s_in_len;
    if (space <= 0) {
        return false;
    }
    int want = space > SD_READ_CHUNK ? SD_READ_CHUNK : space;
    if (f_read(&s_track.f, s_in + s_in_len, (UINT)want, &br) != FR_OK) {
        s_track.eof = true;
        return false;
    }
    if (br == 0) {
        s_track.eof = true;
        return false;
    }
    s_in_len += (int)br;
    return true;
}

/* ------------------------------------------------------------------ */
/* Decodifica                                                          */
/* ------------------------------------------------------------------ */

/* Ritorna il numero di campioni prodotti (interleaved), 0 se questa chiamata
 * non ha prodotto audio, -1 se il brano e' finito. */
static int decode_frame(MP3FrameInfo *fi)
{
    for (int attempt = 0; attempt < 32; attempt++) {
        if ((s_in_len - s_in_pos) < (MP3_INBUF_SIZE / 2) && !s_track.eof) {
            in_refill();
        }
        int avail = s_in_len - s_in_pos;
        if (avail < 4) {
            return -1;                       /* fine brano */
        }

        int off = MP3FindSyncWord(s_in + s_in_pos, avail);
        if (off < 0) {
            /* Nessun sync in tutto il buffer: buttiamo via quasi tutto,
             * conservando 3 byte per un sync a cavallo del refill. */
            s_in_pos = (s_in_len > 3) ? (s_in_len - 3) : s_in_len;
            if (s_track.eof) {
                return -1;
            }
            continue;
        }

        int             sync_abs   = s_in_pos + off;
        unsigned char  *rp         = s_in + sync_abs;
        int             bytes_left = s_in_len - sync_abs;

        int err = MP3Decode(s_dec, &rp, &bytes_left, s_pcm, 0);

        /* `rp` e' l'unico riferimento affidabile: in alcuni percorsi d'errore
         * Helix avanza il puntatore senza aggiornare bytes_left, quindi la
         * posizione la ricalcoliamo sempre da qui. */
        int consumed_to = (int)(rp - s_in);

        if (err == ERR_MP3_NONE) {
            s_in_pos = consumed_to;
            MP3GetLastFrameInfo(s_dec, fi);
            return fi->outputSamps;
        }

        if (err == ERR_MP3_MAINDATA_UNDERFLOW) {
            /* Il "bit reservoir" non e' ancora valido: succede all'inizio del
             * file e subito dopo un seek. Helix ha comunque consumato il
             * frame e si risincronizza da solo entro un paio di frame. */
            s_in_pos = consumed_to;
            continue;
        }

        if (err == ERR_MP3_INDATA_UNDERFLOW) {
            /* Frame troncato: torniamo al sync e chiediamo altri dati. */
            s_in_pos = sync_abs;
            if (s_track.eof || !in_refill()) {
                return -1;
            }
            continue;
        }

        /* Frame corrotto: saltiamo un byte e cerchiamo il sync successivo. */
        s_in_pos = sync_abs + 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Gestione brani                                                      */
/* ------------------------------------------------------------------ */

static void close_track(void)
{
    if (s_track.open) {
        f_close(&s_track.f);
    }
    memset(&s_track, 0, sizeof(s_track));
    in_reset();
    s_pcm_frames = 0;
    s_pcm_taken  = 0;
    s_have_track = false;
}

/* Salta un eventuale tag ID3v2 in testa al file: senza questo passo
 * MP3FindSyncWord puo' trovare un falso sync dentro il tag. */
static uint32_t id3v2_size(void)
{
    if (s_in_len < 10) {
        return 0;
    }
    if (!(s_in[0] == 'I' && s_in[1] == 'D' && s_in[2] == '3')) {
        return 0;
    }
    uint32_t sz = ((uint32_t)(s_in[6] & 0x7F) << 21) |
                  ((uint32_t)(s_in[7] & 0x7F) << 14) |
                  ((uint32_t)(s_in[8] & 0x7F) << 7)  |
                  ((uint32_t)(s_in[9] & 0x7F));
    uint32_t total = 10 + sz;
    if (s_in[5] & 0x10) {
        total += 10;   /* footer presente */
    }
    return total;
}

static bool open_track(uint16_t track_idx)
{
    char path[2 * PP_NAME_LEN + 4];

    close_track();

    if (track_idx >= g_shared.track_count) {
        return false;
    }
    playlist_build_path(path, sizeof(path),
                        g_shared.playlist_names[s_playlist_index],
                        g_shared.track_names[track_idx]);

    if (f_open(&s_track.f, path, FA_READ) != FR_OK) {
        printf("apertura fallita: %s\n", path);
        return false;
    }
    s_track.open = true;
    in_reset();
    in_refill();

    uint32_t start = id3v2_size();
    uint32_t fsize = (uint32_t)f_size(&s_track.f);
    s_track.data_start = start;
    s_track.data_size  = (fsize > start) ? (fsize - start) : 0;

    if (start > 0) {
        f_lseek(&s_track.f, start);
        in_reset();
        s_track.eof = false;
        in_refill();
    }

    /* Valori di riserva, sovrascritti appena leggiamo il primo frame. */
    s_track.samprate = AUDIO_SAMPLE_FREQ;
    s_track.channels = 2;
    s_track.bitrate  = 128000;

    int off = MP3FindSyncWord(s_in + s_in_pos, s_in_len - s_in_pos);
    if (off >= 0) {
        s_in_pos += off;
        MP3FrameInfo fi;
        if (MP3GetNextFrameInfo(s_dec, &fi, s_in + s_in_pos) == ERR_MP3_NONE &&
            fi.samprate > 0 && fi.bitrate > 0) {
            s_track.samprate = (uint32_t)fi.samprate;
            s_track.channels = fi.nChans;
            s_track.bitrate  = (uint32_t)fi.bitrate;
        }
    }
    s_track.duration_sec = s_track.bitrate
        ? (uint32_t)(((uint64_t)s_track.data_size * 8) / s_track.bitrate)
        : 0;
    s_track.samples_played = 0;

    i2s_set_rate(s_track.samprate);
    s_have_track = true;
    return true;
}

/* Apre il brano corrente dell'ordine di riproduzione; se fallisce prova i
 * successivi (file corrotto o rimosso) invece di bloccarsi. */
static bool open_current_or_next(void)
{
    uint16_t n = g_shared.track_count;
    for (uint16_t i = 0; i < n; i++) {
        if (open_track(play_order_current(&s_order))) {
            return true;
        }
        play_order_next(&s_order);
    }
    return false;
}

static void load_playlist(int index)
{
    if (index < 0 || index >= (int)g_shared.playlist_count) {
        return;
    }
    s_playlist_index = index;

    /* Azzeriamo il contatore prima di riscrivere i nomi: core0 itera solo
     * fino a track_count, quindi non puo' leggere voci a meta' scrittura. */
    critical_section_enter_blocking(&g_shared.lock);
    g_shared.track_count = 0;
    critical_section_exit(&g_shared.lock);

    int n = playlist_scan_tracks(g_shared.playlist_names[index],
                                 g_shared.track_names, PP_MAX_TRACKS);

    bool shuffle = play_order_is_shuffle(&s_order);
    play_order_init(&s_order, (uint16_t)n, time_us_32() | 1u);
    play_order_set_shuffle(&s_order, shuffle);

    critical_section_enter_blocking(&g_shared.lock);
    g_shared.track_count    = (uint16_t)n;
    g_shared.playlist_index = (uint16_t)index;
    critical_section_exit(&g_shared.lock);

    if (n > 0) {
        open_current_or_next();
    } else {
        close_track();
    }
}

static void do_seek(int32_t delta_sec)
{
    if (!s_track.open || s_track.duration_sec == 0) {
        return;
    }
    int64_t target = (int64_t)track_elapsed_sec() + delta_sec;
    if (target < 0) {
        target = 0;
    }
    if (target >= (int64_t)s_track.duration_sec) {
        target = (int64_t)s_track.duration_sec - 1;
        if (target < 0) {
            target = 0;
        }
    }

    uint64_t off = (uint64_t)s_track.data_size * (uint64_t)target /
                   (uint64_t)s_track.duration_sec;
    f_lseek(&s_track.f, s_track.data_start + (FSIZE_t)off);

    in_reset();
    s_track.eof  = false;
    s_pcm_frames = 0;
    s_pcm_taken  = 0;
    in_refill();

    /* Ci riallineiamo al primo sync word utile. Il decoder non viene
     * reinizializzato: dopo un salto Helix segnala MAINDATA_UNDERFLOW per un
     * paio di frame e poi riparte da solo (vedi decode_frame). Evitiamo cosi'
     * una free/malloc da ~30 KB in piena riproduzione. */
    int o = MP3FindSyncWord(s_in, s_in_len);
    if (o > 0) {
        s_in_pos = o;
    }
    s_track.samples_played = (uint64_t)target * s_track.samprate;
}

static void next_track(void)
{
    play_order_next(&s_order);
    open_current_or_next();
}

static void prev_track(void)
{
    play_order_prev(&s_order);
    open_current_or_next();
}

static void set_playing(bool on)
{
    if (on && !s_have_track) {
        return;                       /* niente da suonare */
    }
    s_playing = on;
    audio_i2s_set_enabled(on);        /* pausa istantanea */
}

/* ------------------------------------------------------------------ */
/* Comandi da core0                                                    */
/* ------------------------------------------------------------------ */

static void handle_cmd(uint32_t cmd, int32_t param)
{
    switch (cmd) {
    case CMD_PLAY_PAUSE:
        set_playing(!s_playing);
        break;
    case CMD_NEXT_TRACK:
        next_track();
        break;
    case CMD_PREV_TRACK:
        prev_track();
        break;
    case CMD_SEEK:
        do_seek(param);
        break;
    case CMD_SELECT_PLAYLIST:
        load_playlist(param);
        break;
    case CMD_SELECT_TRACK:
        if (param >= 0 && param < (int32_t)g_shared.track_count) {
            play_order_goto_track(&s_order, (uint16_t)param);
            open_current_or_next();
        }
        break;
    case CMD_SET_SHUFFLE:
        play_order_set_shuffle(&s_order, param != 0);
        break;
    default:
        break;
    }
}

static void process_commands(void)
{
    while (multicore_fifo_rvalid()) {
        uint32_t cmd   = multicore_fifo_pop_blocking();
        int32_t  param = (int32_t)multicore_fifo_pop_blocking();
        handle_cmd(cmd, param);
    }
}

/* ------------------------------------------------------------------ */
/* Pubblicazione dello stato verso core0                               */
/* ------------------------------------------------------------------ */

static void publish(sd_status_t sd_status, bool ready)
{
    critical_section_enter_blocking(&g_shared.lock);
    g_shared.sd_status      = sd_status;
    g_shared.ready          = ready;
    g_shared.playing        = s_playing;
    g_shared.shuffle        = play_order_is_shuffle(&s_order);
    g_shared.playlist_index = (uint16_t)s_playlist_index;
    g_shared.track_index    = play_order_current(&s_order);
    g_shared.track_prev     = play_order_peek_prev(&s_order);
    g_shared.track_next     = play_order_peek_next(&s_order);
    g_shared.elapsed_sec    = track_elapsed_sec();
    g_shared.duration_sec   = s_track.duration_sec;
    uint8_t vol             = g_shared.volume;
    critical_section_exit(&g_shared.lock);

    if (vol != s_volume) {
        set_gain_from_volume(vol);
    }
}

/* ------------------------------------------------------------------ */
/* Uscita audio                                                        */
/* ------------------------------------------------------------------ */

/* Copia i campioni decodificati nel buffer I2S corrente. */
static void feed_i2s(void)
{
    if (!s_out) {
        s_out = take_audio_buffer(s_pool, false);   /* mai bloccante: dobbiamo
                                                     * restare reattivi ai
                                                     * comandi di core0 */
        if (!s_out) {
            return;
        }
        s_out_pos = 0;
    }

    int16_t *dst = (int16_t *)s_out->buffer->bytes;
    uint32_t cap = s_out->max_sample_count;

    while (s_pcm_taken < s_pcm_frames && s_out_pos < cap) {
        int32_t l, r;
        if (s_pcm_chans == 2) {
            l = s_pcm[s_pcm_taken * 2];
            r = s_pcm[s_pcm_taken * 2 + 1];
        } else {
            l = r = s_pcm[s_pcm_taken];    /* mono -> duplicato sui due canali */
        }
        if (s_gain != 256) {
            l = (l * s_gain) >> 8;
            r = (r * s_gain) >> 8;
        }
        dst[s_out_pos * 2]     = (int16_t)l;
        dst[s_out_pos * 2 + 1] = (int16_t)r;
        s_pcm_taken++;
        s_out_pos++;
    }

    if (s_out_pos >= cap) {
        s_out->sample_count = s_out_pos;
        give_audio_buffer(s_pool, s_out);
        s_out     = NULL;
        s_out_pos = 0;
    }
}

/* Consegna un buffer parzialmente pieno (fine brano). */
static void flush_i2s(void)
{
    if (s_out && s_out_pos > 0) {
        s_out->sample_count = s_out_pos;
        give_audio_buffer(s_pool, s_out);
        s_out     = NULL;
        s_out_pos = 0;
    }
}

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */

static bool audio_init(void)
{
    static audio_format_t audio_format = {
        .sample_freq   = AUDIO_SAMPLE_FREQ,
        .format        = AUDIO_BUFFER_FORMAT_PCM_S16,
        .channel_count = 2,
    };
    static audio_buffer_format_t producer_format = {
        .format        = &audio_format,
        .sample_stride = 4,          /* 2 canali x 16 bit */
    };

    s_pool = audio_new_producer_pool(&producer_format,
                                     AUDIO_BUFFER_COUNT, AUDIO_BUFFER_FRAMES);
    if (!s_pool) {
        return false;
    }

    audio_i2s_config_t cfg = {
        .data_pin       = PIN_I2S_DIN,
        .clock_pin_base = PIN_I2S_BCK,   /* LCK = BCK + 1, automatico */
        .dma_channel    = AUDIO_DMA_CHANNEL,
        .pio_sm         = AUDIO_PIO_SM,
    };
    if (!audio_i2s_setup(&audio_format, &cfg)) {
        return false;
    }
    if (!audio_i2s_connect(s_pool)) {
        return false;
    }
    s_i2s_rate = AUDIO_SAMPLE_FREQ;
    audio_i2s_set_enabled(false);    /* si parte in pausa */
    return true;
}

static void scan_playlists(void)
{
    critical_section_enter_blocking(&g_shared.lock);
    g_shared.playlist_count = 0;
    critical_section_exit(&g_shared.lock);

    int n = playlist_scan_playlists(g_shared.playlist_names, PP_MAX_PLAYLISTS);

    critical_section_enter_blocking(&g_shared.lock);
    g_shared.playlist_count = (uint16_t)n;
    critical_section_exit(&g_shared.lock);
}

/* ------------------------------------------------------------------ */
/* Entry point di core1                                                */
/* ------------------------------------------------------------------ */

void player_core1_main(void)
{
    play_order_init(&s_order, 0, 1);
    set_gain_from_volume(VOLUME_DEFAULT);

    /* 1. Audio per primo: cosi' si prende il canale DMA 0 e la sua IRQ
     *    (DMA_IRQ_0) prima che il driver SD si prenda i canali liberi. */
    if (!audio_init()) {
        printf("init audio fallito\n");
    }

    /* 2. Il driver SD va su DMA_IRQ_1, per non litigare con l'audio.
     *    Tutto l'init SD avviene QUI, su core1: il driver aspetta il
     *    completamento del DMA su un semaforo rilasciato dalla sua ISR, e
     *    irq_set_enabled() vale solo per il core che la chiama. */
    set_spi_dma_irq_channel(true, true);

    bool sd_ok = sd_init_driver() && playlist_mount();

    if (sd_ok) {
        scan_playlists();
    }

    s_dec = MP3InitDecoder();
    if (!s_dec) {
        printf("MP3InitDecoder fallito (memoria insufficiente)\n");
        sd_ok = false;
    }

    if (sd_ok && g_shared.playlist_count > 0) {
        load_playlist(0);
    }

    publish(sd_ok ? SD_STATUS_OK : SD_STATUS_ERROR, true);

    absolute_time_t next_publish = make_timeout_time_ms(50);

    for (;;) {
        process_commands();

        if (s_playing && s_have_track) {
            if (s_pcm_taken >= s_pcm_frames) {
                MP3FrameInfo fi;
                int samples = decode_frame(&fi);
                if (samples < 0) {
                    /* Fine brano: si passa al successivo, e in fondo alla
                     * playlist si ricomincia dal primo (loop semplice). */
                    flush_i2s();
                    next_track();
                    if (!s_have_track) {
                        set_playing(false);
                    }
                } else if (samples > 0) {
                    s_pcm_chans  = fi.nChans > 0 ? fi.nChans : 2;
                    s_pcm_frames = samples / s_pcm_chans;
                    s_pcm_taken  = 0;
                    s_track.samples_played += (uint64_t)s_pcm_frames;
                    if ((uint32_t)fi.samprate != s_i2s_rate) {
                        i2s_set_rate((uint32_t)fi.samprate);
                        s_track.samprate = (uint32_t)fi.samprate;
                    }
                }
            }
            if (s_pcm_taken < s_pcm_frames) {
                feed_i2s();
            }
        } else {
            /* In pausa non c'e' nulla da decodificare. busy_wait_us e non
             * sleep_ms: quest'ultima si appoggia all'alarm pool di default,
             * che vive su core0. */
            busy_wait_us(500);
        }

        if (time_reached(next_publish)) {
            publish(sd_ok ? SD_STATUS_OK : SD_STATUS_ERROR, true);
            next_publish = make_timeout_time_ms(50);
        }
    }
}
