/* player.h - motore audio: SD -> decodifica MP3 (Helix) -> I2S.
 *
 * Gira interamente su core1. core0 comunica solo tramite la FIFO multicore
 * (pp_send_cmd) e legge lo stato da g_shared.
 */
#ifndef PP_PLAYER_H
#define PP_PLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Entry point di core1. Non ritorna mai. */
void player_core1_main(void);

#ifdef __cplusplus
}
#endif

#endif /* PP_PLAYER_H */
