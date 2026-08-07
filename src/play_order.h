/* play_order.h - ordine di riproduzione (lineare o shuffle).
 *
 * Modulo PURO: nessuna dipendenza da SD, audio o SDK. Testabile con gcc
 * nativo (test/test_play_order.c).
 *
 * Regole (dal documento di progetto):
 *  - attivando lo shuffle a meta' riproduzione, la canzone corrente resta in
 *    prima posizione: non si interrompe bruscamente quello che si ascolta;
 *  - disattivandolo si torna all'ordine lineare, riposizionati sulla canzone
 *    corrente;
 *  - a fine playlist si riparte dalla prima (loop semplice).
 */
#ifndef PP_PLAY_ORDER_H
#define PP_PLAY_ORDER_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t order[PP_MAX_TRACKS]; /* order[pos] = indice del brano */
    uint16_t count;                /* numero di brani               */
    uint16_t pos;                  /* posizione corrente in order[] */
    bool     shuffle;
    uint32_t rng;
} play_order_t;

/* Inizializza in ordine lineare, posizionato sul primo brano. */
void     play_order_init(play_order_t *p, uint16_t count, uint32_t seed);

/* Cambia il numero di brani (es. cambio playlist): riparte lineare da 0,
 * mantenendo la modalita' shuffle corrente. */
void     play_order_set_count(play_order_t *p, uint16_t count);

uint16_t play_order_current(const play_order_t *p);
uint16_t play_order_next(play_order_t *p);   /* avanza con wrap; ritorna il brano */
uint16_t play_order_prev(play_order_t *p);   /* indietro con wrap                 */

/* Guardano avanti/indietro SENZA spostare la posizione: servono a Song Browse
 * per mostrare i vicini nell'ordine di ascolto reale (che con lo shuffle
 * attivo non e' quello alfabetico). */
uint16_t play_order_peek_next(const play_order_t *p);
uint16_t play_order_peek_prev(const play_order_t *p);

/* Posiziona l'ordine sul brano indicato (es. scelta manuale da Song Browse). */
void     play_order_goto_track(play_order_t *p, uint16_t track);

/* Attiva/disattiva lo shuffle preservando il brano corrente. */
void     play_order_set_shuffle(play_order_t *p, bool on);
bool     play_order_is_shuffle(const play_order_t *p);

#ifdef __cplusplus
}
#endif

#endif /* PP_PLAY_ORDER_H */
