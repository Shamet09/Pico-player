/* battery.h - stato della batteria.
 *
 * L'hardware attuale (18650 + modulo TP4056/IP5306) non espone la tensione di
 * cella: VSYS arriva dall'uscita regolata del modulo, che sta a ~5 V finche'
 * c'e' carica e poi crolla. Senza un fuel gauge (es. MAX17048) NON e'
 * possibile stimare la percentuale reale.
 *
 * Questa funzione e' quindi uno stub isolato: quando si aggiungera' un fuel
 * gauge bastera' cambiare battery.c, senza toccare il resto del firmware.
 */
#ifndef PP_BATTERY_H
#define PP_BATTERY_H

#ifdef __cplusplus
extern "C" {
#endif

void battery_init(void);

/* Ritorna 0..100, oppure -1 se la percentuale non e' disponibile. */
int battery_get_percent(void);

#ifdef __cplusplus
}
#endif

#endif /* PP_BATTERY_H */
