/* helix_memory.h - allocatore usato da buffers.c del decoder Helix.
 *
 * Nell'upstream (pschatzmann/arduino-libhelix) questo header e' implementato
 * da utils/helix_memory.cpp, che tira dentro l'Allocator C++ e il logging del
 * wrapper Arduino. Noi usiamo solo il decoder C puro, quindi forniamo
 * l'equivalente minimo in helix_memory.c (malloc/free della newlib).
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void *helix_malloc(int size);
void  helix_free(void *ptr);

#ifdef __cplusplus
}
#endif
