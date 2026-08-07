#include "mascot_fsm.h"

#include <string.h>

static inline bool time_reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

/* xorshift32: piccolo, deterministico, piu' che sufficiente per i blink. */
static uint32_t rng_next(mascot_fsm_t *m)
{
    uint32_t x = m->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    m->rng = x;
    return x;
}

static void schedule_next_blink(mascot_fsm_t *m, uint32_t from_ms)
{
    uint32_t span = MASCOT_BLINK_MAX_MS - MASCOT_BLINK_MIN_MS;
    m->next_blink_at = from_ms + MASCOT_BLINK_MIN_MS + (rng_next(m) % (span + 1));
}

void mascot_fsm_init(mascot_fsm_t *m, uint32_t now_ms, uint32_t seed)
{
    memset(m, 0, sizeof(*m));
    m->rng = seed ? seed : 0x2545F491u;  /* xorshift non tollera lo zero */
    m->started = true;
    schedule_next_blink(m, now_ms);
}

void mascot_fsm_sd_mounted(mascot_fsm_t *m, uint32_t now_ms)
{
    m->splash_active = true;
    m->splash_until  = now_ms + MASCOT_SPLASH_MS;
}

mascot_frame_t mascot_fsm_update(mascot_fsm_t *m, uint32_t now_ms,
                                 bool sd_ok, bool playing)
{
    /* Fronte di salita di sd_ok: mount riuscito -> splash. */
    if (sd_ok && !m->prev_sd_ok) {
        mascot_fsm_sd_mounted(m, now_ms);
    }
    m->prev_sd_ok = sd_ok;

    /* 1. errore SD: precedenza assoluta, e annulla lo splash. */
    if (!sd_ok) {
        m->splash_active = false;
        m->blinking      = false;
        schedule_next_blink(m, now_ms);
        return MASCOT_SD_ERROR;
    }

    /* 2. splash "SD ok" */
    if (m->splash_active) {
        if (!time_reached(now_ms, m->splash_until)) {
            /* durante lo splash i blink non scorrono */
            schedule_next_blink(m, now_ms);
            return MASCOT_SD_OK;
        }
        m->splash_active = false;
        schedule_next_blink(m, now_ms);
    }

    /* 3. in pausa: espressione statica, e il timer dei blink resta fermo
     *    (viene riprogrammato in continuazione) cosi' alla ripresa non parte
     *    subito un blink. */
    if (!playing) {
        m->blinking = false;
        schedule_next_blink(m, now_ms);
        return MASCOT_PAUSED;
    }

    /* 4. riproduzione attiva: neutro + blink casuale */
    if (m->blinking) {
        if (time_reached(now_ms, m->blink_until)) {
            m->blinking = false;
            schedule_next_blink(m, now_ms);
        } else {
            return MASCOT_BLINK;
        }
    } else if (time_reached(now_ms, m->next_blink_at)) {
        m->blinking    = true;
        m->blink_until = now_ms + MASCOT_BLINK_MS;
        return MASCOT_BLINK;
    }

    return MASCOT_IDLE;
}
