/* buttons.h - glue fra i GPIO e la macchina a stati pura button_fsm.
 *
 * Gira su core0. Gli eventi prodotti vengono accodati in un piccolo ring
 * buffer, cosi' buttons_poll() puo' essere chiamata anche mentre si sta
 * scrivendo sul display (vedi il parametro `yield` di ssd1306_show).
 */
#ifndef PP_BUTTONS_H
#define PP_BUTTONS_H

#include <stdbool.h>

#include "button_fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

void buttons_init(void);

/* Campiona i GPIO e fa avanzare la macchina a stati. Va chiamata spesso
 * (almeno ogni ~10 ms) perche' il debounce e' a 25 ms. */
void buttons_poll(void);

/* Estrae il prossimo evento; ritorna false se la coda e' vuota. */
bool buttons_get_event(btn_event_t *ev);

void buttons_set_dclick(btn_id_t button, bool enabled);

#ifdef __cplusplus
}
#endif

#endif /* PP_BUTTONS_H */
