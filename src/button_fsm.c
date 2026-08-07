#include "button_fsm.h"

#include <string.h>

/* Confronto di tempi robusto al wrap-around dei 32 bit. */
static inline bool time_reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

static inline void emit(btn_event_t *out, int max_out, int *n,
                        uint8_t button, uint8_t type, uint16_t repeat)
{
    if (*n >= max_out) {
        return; /* buffer pieno: l'evento viene scartato */
    }
    out[*n].button = button;
    out[*n].type   = type;
    out[*n].repeat = repeat;
    (*n)++;
}

void button_fsm_init(button_fsm_t *fsm, const btn_cfg_t cfg[BTN_COUNT], uint32_t now_ms)
{
    memset(fsm, 0, sizeof(*fsm));
    for (int i = 0; i < BTN_COUNT; i++) {
        fsm->cfg[i] = cfg[i];
        fsm->st[i].raw_since = now_ms;
    }
}

void button_fsm_set_dclick(button_fsm_t *fsm, btn_id_t button, bool enabled)
{
    if ((int)button < 0 || (int)button >= BTN_COUNT) {
        return;
    }
    fsm->st[button].dclick_enabled = enabled;
}

int button_fsm_update(button_fsm_t *fsm, uint32_t now_ms,
                      const bool pressed_raw[BTN_COUNT],
                      btn_event_t *out, int max_out)
{
    int n = 0;

    for (int b = 0; b < BTN_COUNT; b++) {
        btn_state_t *st  = &fsm->st[b];
        const btn_cfg_t *cf = &fsm->cfg[b];

        /* --- 1. debounce -------------------------------------------------- */
        if (pressed_raw[b] != st->level_raw) {
            st->level_raw  = pressed_raw[b];
            st->raw_since  = now_ms;
        }

        if (st->level_raw != st->level_stable &&
            (now_ms - st->raw_since) >= cf->debounce_ms) {

            st->level_stable = st->level_raw;

            if (st->level_stable) {
                /* --- fronte di pressione ------------------------------- */
                if (st->click_pending) {
                    /* secondo click entro la finestra: doppio click subito,
                     * e il rilascio successivo non deve generare nulla. */
                    emit(out, max_out, &n, (uint8_t)b, BTN_EV_DOUBLE_CLICK, 0);
                    st->click_pending    = false;
                    st->click_suppressed = true;
                } else {
                    st->click_suppressed = false;
                }
                st->press_time = now_ms;
                st->hold_fired = false;
                st->tick_n     = 0;
            } else {
                /* --- fronte di rilascio -------------------------------- */
                if (st->hold_fired) {
                    emit(out, max_out, &n, (uint8_t)b, BTN_EV_HOLD_END, 0);
                } else if (!st->click_suppressed) {
                    if (st->dclick_enabled) {
                        st->click_pending  = true;
                        st->click_deadline = now_ms + cf->dclick_ms;
                    } else {
                        emit(out, max_out, &n, (uint8_t)b, BTN_EV_CLICK, 0);
                    }
                }
                st->hold_fired       = false;
                st->click_suppressed = false;
            }
        }

        /* --- 2. hold ------------------------------------------------------ */
        if (st->level_stable) {
            if (!st->hold_fired) {
                if (time_reached(now_ms, st->press_time + cf->hold_ms)) {
                    st->hold_fired = true;
                    /* un hold annulla un eventuale click in attesa: l'utente
                     * sta chiaramente facendo un'altra cosa. */
                    st->click_pending = false;
                    emit(out, max_out, &n, (uint8_t)b, BTN_EV_HOLD_START, 0);
                    st->tick_n = 1;
                    emit(out, max_out, &n, (uint8_t)b, BTN_EV_HOLD_TICK, st->tick_n);
                    st->next_tick = st->press_time + cf->hold_ms + cf->hold_tick_ms;
                }
            } else if (time_reached(now_ms, st->next_tick)) {
                /* Un solo tick per chiamata: se siamo in ritardo (loop lento)
                 * risincronizziamo invece di emettere una raffica. */
                st->tick_n++;
                emit(out, max_out, &n, (uint8_t)b, BTN_EV_HOLD_TICK, st->tick_n);
                st->next_tick += cf->hold_tick_ms;
                if (time_reached(now_ms, st->next_tick)) {
                    st->next_tick = now_ms + cf->hold_tick_ms;
                }
            }
        }

        /* --- 3. scadenza della finestra di doppio click -------------------- */
        if (st->click_pending && time_reached(now_ms, st->click_deadline)) {
            emit(out, max_out, &n, (uint8_t)b, BTN_EV_CLICK, 0);
            st->click_pending = false;
        }
    }

    return n;
}

uint32_t button_hold_step(uint32_t base, uint32_t cap, uint16_t n)
{
    uint32_t step = base;

    if (n == 0) {
        return 0;
    }
    /* raddoppia n-1 volte, fermandosi al cap (niente overflow) */
    for (uint16_t i = 1; i < n; i++) {
        if (step >= cap) {
            break;
        }
        step <<= 1;
    }
    return step > cap ? cap : step;
}
