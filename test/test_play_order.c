/* test_play_order.c - unit test nativi per play_order.
 *
 * Verificano: ordine lineare con wrap, validita' della permutazione shuffle,
 * conservazione del brano corrente quando si attiva/disattiva lo shuffle,
 * coerenza di peek_next/peek_prev e casi limite (0 e 1 brano).
 */
#include <string.h>

#include "play_order.h"
#include "test_util.h"

/* Controlla che order[] sia una permutazione di 0..count-1. */
static int is_permutation(const play_order_t *p)
{
    static char seen[PP_MAX_TRACKS];
    memset(seen, 0, sizeof(seen));
    for (uint16_t i = 0; i < p->count; i++) {
        uint16_t v = p->order[i];
        if (v >= p->count || seen[v]) {
            return 0;
        }
        seen[v] = 1;
    }
    return 1;
}

int main(void)
{
    play_order_t p;

    /* ---------------------------------------------------------------- */
    SECTION("ordine lineare e wrap a fine playlist");
    play_order_init(&p, 5, 1);
    CHECK_EQ(play_order_current(&p), 0);
    CHECK_EQ(play_order_next(&p), 1);
    CHECK_EQ(play_order_next(&p), 2);
    CHECK_EQ(play_order_next(&p), 3);
    CHECK_EQ(play_order_next(&p), 4);
    CHECK_EQ(play_order_next(&p), 0);   /* loop semplice */
    CHECK_EQ(play_order_prev(&p), 4);
    CHECK_EQ(play_order_prev(&p), 3);
    CHECK(!play_order_is_shuffle(&p));

    /* ---------------------------------------------------------------- */
    SECTION("peek_next / peek_prev non spostano la posizione");
    play_order_init(&p, 5, 1);
    play_order_next(&p);                /* siamo sul brano 1 */
    CHECK_EQ(play_order_current(&p), 1);
    CHECK_EQ(play_order_peek_next(&p), 2);
    CHECK_EQ(play_order_peek_prev(&p), 0);
    CHECK_EQ(play_order_current(&p), 1); /* invariato */

    /* ---------------------------------------------------------------- */
    SECTION("shuffle: permutazione valida e brano corrente in testa");
    play_order_init(&p, 20, 0xC0FFEE);
    for (int i = 0; i < 7; i++) {
        play_order_next(&p);            /* siamo sul brano 7 */
    }
    CHECK_EQ(play_order_current(&p), 7);
    play_order_set_shuffle(&p, true);
    CHECK(play_order_is_shuffle(&p));
    CHECK(is_permutation(&p));
    CHECK_EQ(play_order_current(&p), 7);  /* non si interrompe l'ascolto */
    CHECK_EQ(p.pos, 0);

    /* ---------------------------------------------------------------- */
    SECTION("shuffle: si passa comunque per tutti i brani una volta sola");
    {
        static char seen[PP_MAX_TRACKS];
        memset(seen, 0, sizeof(seen));
        seen[play_order_current(&p)] = 1;
        int dup = 0;
        for (int i = 1; i < 20; i++) {
            uint16_t t = play_order_next(&p);
            if (seen[t]) {
                dup++;
            }
            seen[t] = 1;
        }
        CHECK_EQ(dup, 0);
        CHECK_EQ(play_order_next(&p), play_order_current(&p)); /* wrap */
    }

    /* ---------------------------------------------------------------- */
    SECTION("shuffle off: torna lineare, posizionato sul brano corrente");
    play_order_init(&p, 20, 0xABCD);
    for (int i = 0; i < 11; i++) {
        play_order_next(&p);
    }
    CHECK_EQ(play_order_current(&p), 11);
    play_order_set_shuffle(&p, true);
    CHECK_EQ(play_order_current(&p), 11);
    play_order_set_shuffle(&p, false);
    CHECK(!play_order_is_shuffle(&p));
    CHECK_EQ(play_order_current(&p), 11);
    CHECK_EQ(p.pos, 11);
    CHECK_EQ(play_order_next(&p), 12);    /* di nuovo alfabetico */
    CHECK_EQ(play_order_next(&p), 13);

    /* ---------------------------------------------------------------- */
    SECTION("shuffle a meta' brano da una posizione qualsiasi");
    for (uint16_t start = 0; start < 20; start++) {
        play_order_init(&p, 20, 1234u + start);
        play_order_goto_track(&p, start);
        play_order_set_shuffle(&p, true);
        if (play_order_current(&p) != start || !is_permutation(&p)) {
            printf("     fallito con start=%u\n", start);
            CHECK_EQ(play_order_current(&p), start);
            break;
        }
    }
    CHECK(1);   /* arrivati qui, tutte le 20 posizioni sono andate bene */

    /* ---------------------------------------------------------------- */
    SECTION("goto_track");
    play_order_init(&p, 10, 5);
    play_order_goto_track(&p, 7);
    CHECK_EQ(play_order_current(&p), 7);
    CHECK_EQ(play_order_next(&p), 8);

    /* ---------------------------------------------------------------- */
    SECTION("cambio playlist: set_count riparte da zero");
    play_order_init(&p, 10, 5);
    play_order_set_shuffle(&p, true);
    play_order_set_count(&p, 3);
    CHECK_EQ(p.count, 3);
    CHECK(is_permutation(&p));
    CHECK(play_order_is_shuffle(&p));   /* la modalita' resta */
    play_order_set_shuffle(&p, false);
    play_order_set_count(&p, 4);
    CHECK_EQ(play_order_current(&p), 0);

    /* ---------------------------------------------------------------- */
    SECTION("casi limite: 0 e 1 brano");
    play_order_init(&p, 0, 7);
    CHECK_EQ(play_order_current(&p), 0);
    CHECK_EQ(play_order_next(&p), 0);
    CHECK_EQ(play_order_prev(&p), 0);
    play_order_set_shuffle(&p, true);   /* non deve esplodere */
    CHECK_EQ(play_order_current(&p), 0);

    play_order_init(&p, 1, 7);
    CHECK_EQ(play_order_current(&p), 0);
    CHECK_EQ(play_order_next(&p), 0);
    CHECK_EQ(play_order_prev(&p), 0);
    CHECK_EQ(play_order_peek_next(&p), 0);
    play_order_set_shuffle(&p, true);
    CHECK_EQ(play_order_current(&p), 0);

    /* ---------------------------------------------------------------- */
    SECTION("il numero di brani viene limitato a PP_MAX_TRACKS");
    play_order_init(&p, PP_MAX_TRACKS + 50, 3);
    CHECK_EQ(p.count, PP_MAX_TRACKS);
    CHECK(is_permutation(&p));

    /* ---------------------------------------------------------------- */
    SECTION("due shuffle successivi danno ordini diversi");
    {
        play_order_t a, b;
        play_order_init(&a, 50, 11111);
        play_order_init(&b, 50, 99999);
        play_order_set_shuffle(&a, true);
        play_order_set_shuffle(&b, true);
        int same = 1;
        for (uint16_t i = 0; i < 50; i++) {
            if (a.order[i] != b.order[i]) {
                same = 0;
                break;
            }
        }
        CHECK(!same);
    }

    return t_summary("test_play_order");
}
