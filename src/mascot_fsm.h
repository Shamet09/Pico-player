/* mascot_fsm.h - logica temporale della mascotte ("Pico", il gattino).
 *
 * Modulo PURO: decide QUALE frame mostrare, non lo disegna.
 * Testabile con gcc nativo (test/test_mascot.c).
 *
 * Priorita' degli stati (dalla piu' alta):
 *   1. errore SD          -> MASCOT_SD_ERROR  (occhi a X, bocca preoccupata)
 *   2. splash "SD ok"     -> MASCOT_SD_OK     (~2 s dopo un mount riuscito)
 *   3. in pausa           -> MASCOT_PAUSED    (occhi socchiusi, statico)
 *   4. riproduzione       -> MASCOT_IDLE con MASCOT_BLINK periodico casuale
 */
#ifndef PP_MASCOT_FSM_H
#define PP_MASCOT_FSM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MASCOT_IDLE = 0,
    MASCOT_BLINK,
    MASCOT_PAUSED,
    MASCOT_SD_ERROR,
    MASCOT_SD_OK,
    MASCOT_FRAME_COUNT
} mascot_frame_t;

#define MASCOT_SPLASH_MS     2000   /* durata dello splash "SD ok"        */
#define MASCOT_BLINK_MS      180    /* durata di un blink                 */
#define MASCOT_BLINK_MIN_MS  4000   /* intervallo minimo fra due blink    */
#define MASCOT_BLINK_MAX_MS  9000   /* intervallo massimo fra due blink   */

typedef struct {
    uint32_t rng;            /* stato del generatore pseudo-casuale */
    uint32_t splash_until;   /* fine dello splash "SD ok"           */
    bool     splash_active;
    uint32_t next_blink_at;  /* quando iniziare il prossimo blink   */
    uint32_t blink_until;    /* fine del blink in corso             */
    bool     blinking;
    bool     prev_sd_ok;
    bool     started;
} mascot_fsm_t;

/* `seed` deve essere diverso ad ogni avvio se si vuole una sequenza di blink
 * differente (es. da time_us_32()). Con seed fisso il comportamento e'
 * deterministico, cosa su cui si appoggiano gli unit test. */
void mascot_fsm_init(mascot_fsm_t *m, uint32_t now_ms, uint32_t seed);

/* Segnala esplicitamente un mount riuscito (fa partire lo splash).
 * In alternativa lo splash parte da solo sul fronte di salita di `sd_ok`. */
void mascot_fsm_sd_mounted(mascot_fsm_t *m, uint32_t now_ms);

mascot_frame_t mascot_fsm_update(mascot_fsm_t *m, uint32_t now_ms,
                                 bool sd_ok, bool playing);

#ifdef __cplusplus
}
#endif

#endif /* PP_MASCOT_FSM_H */
