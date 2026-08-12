/* shared_state.h - stato condiviso fra i due core + protocollo dei comandi.
 *
 * Ripartizione (vedi README.md):
 *   core0 = interfaccia utente (pulsanti, OLED, mascotte)
 *   core1 = audio (SD + FatFs + decodifica MP3 + I2S)
 *
 * REGOLA D'ORO: solo core1 chiama funzioni FatFs (f_*) e il driver SD.
 * core0 non tocca mai la SD: legge solo questa struct.
 *
 * Tutti i campi sono protetti da una critical_section. Le sezioni critiche
 * sono cortissime (copie di pochi byte o di una stringa) per non disturbare
 * la decodifica su core1.
 */
#ifndef PP_SHARED_STATE_H
#define PP_SHARED_STATE_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/critical_section.h"

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SD_STATUS_UNKNOWN = 0,   /* mount non ancora tentato */
    SD_STATUS_OK,
    SD_STATUS_ERROR
} sd_status_t;

/* Comandi core0 -> core1, inviati sulla FIFO hardware multicore.
 * Due word per comando: tipo + parametro con segno. */
typedef enum {
    CMD_NONE = 0,
    CMD_PLAY_PAUSE,       /* param ignorato                              */
    CMD_NEXT_TRACK,       /* param ignorato                              */
    CMD_PREV_TRACK,       /* param ignorato                              */
    CMD_SEEK,             /* param = delta in secondi (con segno)        */
    CMD_SELECT_PLAYLIST,  /* param = indice playlist                     */
    CMD_SELECT_TRACK,     /* param = indice brano nella playlist         */
    CMD_SET_SHUFFLE       /* param = 0/1                                 */
} pp_cmd_t;

typedef struct {
    critical_section_t lock;

    /* --- scritti da core1, letti da core0 ------------------------------- */
    bool        ready;            /* core1 ha finito l'init                */
    sd_status_t sd_status;
    bool        playing;
    bool        shuffle;
    uint16_t    playlist_index;
    uint16_t    playlist_count;
    uint16_t    track_index;      /* indice nella playlist (non nell'ordine) */
    uint16_t    track_prev;       /* vicini nell'ORDINE DI ASCOLTO, gia'     */
    uint16_t    track_next;       /* risolti da core1 (contano lo shuffle)   */
    uint16_t    track_count;
    uint32_t    elapsed_sec;
    uint32_t    duration_sec;     /* stima: vedi README (VBR = approssimato) */

    char playlist_names[PP_MAX_PLAYLISTS][PP_NAME_LEN];
    char track_names[PP_MAX_TRACKS][PP_NAME_LEN];

    /* --- scritto da core0, letto da core1 ------------------------------- */
    uint8_t volume;               /* 0..100 */

    /* Modo test audio: deciso da core0 leggendo MENU all'accensione, PRIMA di
     * lanciare core1. Dopo il lancio nessuno lo scrive piu', quindi entrambi i
     * core possono leggerlo senza sincronizzazione. Vedi config.h. */
    bool selftest;
} shared_state_t;

extern shared_state_t g_shared;

void pp_shared_init(void);

/* Helper per core0: invia un comando a core1. */
void pp_send_cmd(pp_cmd_t cmd, int32_t param);

#ifdef __cplusplus
}
#endif

#endif /* PP_SHARED_STATE_H */
