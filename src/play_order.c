#include "play_order.h"

#include <string.h>

static uint32_t rng_next(play_order_t *p)
{
    uint32_t x = p->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    p->rng = x;
    return x;
}

static void make_linear(play_order_t *p)
{
    for (uint16_t i = 0; i < p->count; i++) {
        p->order[i] = i;
    }
}

/* Fisher-Yates su tutto l'array. */
static void shuffle_all(play_order_t *p)
{
    if (p->count < 2) {
        return;
    }
    for (uint16_t i = (uint16_t)(p->count - 1); i > 0; i--) {
        uint16_t j = (uint16_t)(rng_next(p) % (uint32_t)(i + 1));
        uint16_t t = p->order[i];
        p->order[i] = p->order[j];
        p->order[j] = t;
    }
}

/* Porta `track` in posizione 0 scambiandolo con chi ci sta. */
static void move_track_to_front(play_order_t *p, uint16_t track)
{
    for (uint16_t i = 0; i < p->count; i++) {
        if (p->order[i] == track) {
            uint16_t t = p->order[0];
            p->order[0] = p->order[i];
            p->order[i] = t;
            return;
        }
    }
}

void play_order_init(play_order_t *p, uint16_t count, uint32_t seed)
{
    memset(p, 0, sizeof(*p));
    p->rng   = seed ? seed : 0x9E3779B9u;
    p->count = count > PP_MAX_TRACKS ? PP_MAX_TRACKS : count;
    p->pos   = 0;
    p->shuffle = false;
    make_linear(p);
}

void play_order_set_count(play_order_t *p, uint16_t count)
{
    p->count = count > PP_MAX_TRACKS ? PP_MAX_TRACKS : count;
    p->pos   = 0;
    make_linear(p);
    if (p->shuffle) {
        shuffle_all(p);
    }
}

uint16_t play_order_current(const play_order_t *p)
{
    if (p->count == 0) {
        return 0;
    }
    return p->order[p->pos];
}

uint16_t play_order_next(play_order_t *p)
{
    if (p->count == 0) {
        return 0;
    }
    p->pos = (uint16_t)((p->pos + 1) % p->count);
    return p->order[p->pos];
}

uint16_t play_order_prev(play_order_t *p)
{
    if (p->count == 0) {
        return 0;
    }
    p->pos = (uint16_t)((p->pos + p->count - 1) % p->count);
    return p->order[p->pos];
}

uint16_t play_order_peek_next(const play_order_t *p)
{
    if (p->count == 0) {
        return 0;
    }
    return p->order[(p->pos + 1) % p->count];
}

uint16_t play_order_peek_prev(const play_order_t *p)
{
    if (p->count == 0) {
        return 0;
    }
    return p->order[(p->pos + p->count - 1) % p->count];
}

void play_order_goto_track(play_order_t *p, uint16_t track)
{
    for (uint16_t i = 0; i < p->count; i++) {
        if (p->order[i] == track) {
            p->pos = i;
            return;
        }
    }
}

void play_order_set_shuffle(play_order_t *p, bool on)
{
    if (p->count == 0) {
        p->shuffle = on;
        return;
    }

    uint16_t current = p->order[p->pos];

    if (on) {
        make_linear(p);
        shuffle_all(p);
        move_track_to_front(p, current); /* non interrompiamo l'ascolto */
        p->pos = 0;
    } else {
        make_linear(p);
        p->pos = current;                /* ordine lineare: pos == indice brano */
    }
    p->shuffle = on;
}

bool play_order_is_shuffle(const play_order_t *p)
{
    return p->shuffle;
}
