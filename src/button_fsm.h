/* button_fsm.h - macchina a stati dei pulsanti.
 *
 * Modulo PURO: nessuna dipendenza da hardware, SDK o tempo reale.
 * Riceve in ingresso solo il livello grezzo dei pulsanti e un timestamp in
 * millisecondi, e produce eventi. Questo permette di testarlo con gcc nativo
 * (vedi test/test_buttons.c) senza toolchain ARM.
 *
 * Eventi generati:
 *   BTN_EV_CLICK        rilascio prima della soglia di hold
 *   BTN_EV_DOUBLE_CLICK secondo click entro la finestra (solo se abilitato)
 *   BTN_EV_HOLD_START   raggiunta la soglia di hold (one-shot)
 *   BTN_EV_HOLD_TICK    ripetizione durante l'hold; `repeat` parte da 1
 *   BTN_EV_HOLD_END     rilascio dopo un hold
 *
 * Nota: il primo BTN_EV_HOLD_TICK viene emesso nello stesso istante di
 * BTN_EV_HOLD_START, cosi' l'azione ripetuta (volume/seek) parte subito e non
 * dopo un ulteriore intervallo di tick.
 */
#ifndef PP_BUTTON_FSM_H
#define PP_BUTTON_FSM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BTN_LEFT = 0,
    BTN_RIGHT,
    BTN_OK,
    BTN_MENU,
    BTN_COUNT
} btn_id_t;

typedef enum {
    BTN_EV_CLICK = 0,
    BTN_EV_DOUBLE_CLICK,
    BTN_EV_HOLD_START,
    BTN_EV_HOLD_TICK,
    BTN_EV_HOLD_END
} btn_ev_type_t;

typedef struct {
    uint8_t  button;   /* btn_id_t     */
    uint8_t  type;     /* btn_ev_type_t */
    uint16_t repeat;   /* indice del tick di hold, 1-based; 0 negli altri eventi */
} btn_event_t;

typedef struct {
    uint32_t debounce_ms;
    uint32_t hold_ms;
    uint32_t hold_tick_ms;
    uint32_t dclick_ms;
} btn_cfg_t;

typedef struct {
    bool     level_raw;        /* ultimo livello campionato (true = premuto) */
    bool     level_stable;     /* livello dopo debounce                      */
    uint32_t raw_since;        /* quando level_raw e' cambiato l'ultima volta */
    uint32_t press_time;       /* istante della pressione stabile            */
    bool     hold_fired;       /* soglia di hold gia' superata               */
    uint32_t next_tick;        /* prossimo HOLD_TICK                          */
    uint16_t tick_n;           /* contatore dei tick di hold                  */
    bool     click_suppressed; /* il rilascio non deve generare un click      */
    bool     click_pending;    /* click in attesa di un eventuale secondo     */
    uint32_t click_deadline;   /* scadenza della finestra di doppio click     */
    bool     dclick_enabled;   /* doppio click attivo per questo pulsante     */
} btn_state_t;

typedef struct {
    btn_cfg_t   cfg[BTN_COUNT];
    btn_state_t st[BTN_COUNT];
} button_fsm_t;

/* Inizializza la macchina a stati. `cfg` e' un array di BTN_COUNT
 * configurazioni (una per pulsante, cosi' MENU puo' avere una soglia di hold
 * diversa). Tutti i pulsanti partono rilasciati e senza doppio click. */
void button_fsm_init(button_fsm_t *fsm, const btn_cfg_t cfg[BTN_COUNT], uint32_t now_ms);

/* Abilita/disabilita il doppio click per un pulsante.
 * Quando e' disabilitato, il click viene emesso SUBITO al rilascio. */
void button_fsm_set_dclick(button_fsm_t *fsm, btn_id_t button, bool enabled);

/* Fa avanzare la macchina a stati.
 * `pressed_raw` e' un array di BTN_COUNT livelli grezzi (true = premuto,
 * gia' normalizzato: i pulsanti sono attivi bassi sull'hardware).
 * Scrive fino a `max_out` eventi in `out` e ritorna quanti ne ha scritti. */
int button_fsm_update(button_fsm_t *fsm, uint32_t now_ms,
                      const bool pressed_raw[BTN_COUNT],
                      btn_event_t *out, int max_out);

/* Incremento esponenziale usato durante l'hold: raddoppia ad ogni tick e si
 * ferma a `cap`. `n` e' l'indice del tick (1-based). Funzione pura, testata. */
uint32_t button_hold_step(uint32_t base, uint32_t cap, uint16_t n);

#ifdef __cplusplus
}
#endif

#endif /* PP_BUTTON_FSM_H */
