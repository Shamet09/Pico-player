/* ui.h - macchina a stati delle schermate (Home / Song Browse / Playlist
 * Switch) e disegno. Gira su core0.
 */
#ifndef PP_UI_H
#define PP_UI_H

#include "ssd1306.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_HOME = 0,
    UI_BROWSE,
    UI_PLAYLIST
} ui_screen_t;

void        ui_init(ssd1306_t *display);

/* Consuma gli eventi dei pulsanti in coda e aggiorna lo stato. */
void        ui_handle_events(void);

/* Ridisegna il framebuffer e lo manda al display. */
void        ui_render(void);

ui_screen_t ui_screen(void);

#ifdef __cplusplus
}
#endif

#endif /* PP_UI_H */
