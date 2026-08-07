#include "battery.h"

void battery_init(void)
{
    /* Niente da fare finche' non c'e' un fuel gauge sul bus I2C.
     * Con un MAX17048 qui andrebbe l'init del chip. */
}

int battery_get_percent(void)
{
    /* -1 = sconosciuta. La UI mostra "--%".
     *
     * Nota: leggere VSYS con l'ADC (GP29/ADC3) su questo hardware NON aiuta:
     * il modulo IP5306 fornisce ~5 V regolati indipendentemente dallo stato
     * della cella, quindi la lettura sarebbe piatta fino allo spegnimento. */
    return -1;
}
