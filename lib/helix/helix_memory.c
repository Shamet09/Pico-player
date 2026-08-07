/* helix_memory.c - implementazione minimale di helix_malloc/helix_free.
 *
 * Il decoder Helix alloca i suoi buffer una volta sola, in MP3InitDecoder()
 * (circa 30 KB in totale). Sul RP2040 la heap della newlib va benissimo:
 * l'Allocator C++ dell'upstream faceva esattamente la stessa cosa su
 * piattaforme senza PSRAM.
 *
 * In PicoPlayer queste funzioni vengono chiamate una sola volta all'avvio di
 * core1: durante la riproduzione non c'e' nessuna allocazione dinamica.
 */
#include <stdlib.h>

#include "utils/helix_memory.h"

void *helix_malloc(int size)
{
    if (size <= 0) {
        return NULL;
    }
    return malloc((size_t)size);
}

void helix_free(void *ptr)
{
    free(ptr);
}
