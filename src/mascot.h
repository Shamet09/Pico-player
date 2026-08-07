/* mascot.h - disegna la mascotte sull'OLED.
 * La LOGICA (quale frame, quando) sta in mascot_fsm.h, che e' pura e testata.
 */
#ifndef PP_MASCOT_H
#define PP_MASCOT_H

#include "mascot_fsm.h"
#include "ssd1306.h"

#ifdef __cplusplus
extern "C" {
#endif

void mascot_draw(ssd1306_t *d, int x, int y, mascot_frame_t frame);
int  mascot_width(void);
int  mascot_height(void);

#ifdef __cplusplus
}
#endif

#endif /* PP_MASCOT_H */
