/* test_mascot.c - unit test nativi per mascot_fsm.
 *
 * Verificano: priorita' dell'errore SD su tutto, durata dello splash "SD ok",
 * espressione statica in pausa, blink periodico entro l'intervallo dichiarato
 * e determinismo a parita' di seed.
 */
#include "mascot_fsm.h"
#include "test_util.h"

static const char *frame_name(mascot_frame_t f)
{
    switch (f) {
    case MASCOT_IDLE:     return "IDLE";
    case MASCOT_BLINK:    return "BLINK";
    case MASCOT_PAUSED:   return "PAUSED";
    case MASCOT_SD_ERROR: return "SD_ERROR";
    case MASCOT_SD_OK:    return "SD_OK";
    default:              return "?";
    }
}

int main(void)
{
    mascot_fsm_t m;

    /* ---------------------------------------------------------------- */
    SECTION("mount riuscito -> splash SD_OK per ~2 s, poi IDLE");
    mascot_fsm_init(&m, 0, 12345);
    CHECK_EQ(mascot_fsm_update(&m, 0, true, true), MASCOT_SD_OK);
    CHECK_EQ(mascot_fsm_update(&m, 1000, true, true), MASCOT_SD_OK);
    CHECK_EQ(mascot_fsm_update(&m, 1999, true, true), MASCOT_SD_OK);
    CHECK_EQ(mascot_fsm_update(&m, 2000, true, true), MASCOT_IDLE);
    CHECK_EQ(mascot_fsm_update(&m, 2500, true, true), MASCOT_IDLE);

    /* ---------------------------------------------------------------- */
    SECTION("errore SD: precedenza assoluta, anche durante lo splash");
    mascot_fsm_init(&m, 0, 12345);
    CHECK_EQ(mascot_fsm_update(&m, 0, true, true), MASCOT_SD_OK);
    CHECK_EQ(mascot_fsm_update(&m, 500, false, true), MASCOT_SD_ERROR);
    CHECK_EQ(mascot_fsm_update(&m, 800, false, false), MASCOT_SD_ERROR);
    CHECK_EQ(mascot_fsm_update(&m, 20000, false, true), MASCOT_SD_ERROR);

    /* ---------------------------------------------------------------- */
    SECTION("la SD che torna disponibile rifa' partire lo splash");
    CHECK_EQ(mascot_fsm_update(&m, 20500, true, true), MASCOT_SD_OK);
    CHECK_EQ(mascot_fsm_update(&m, 22499, true, true), MASCOT_SD_OK);
    CHECK_EQ(mascot_fsm_update(&m, 22500, true, true), MASCOT_IDLE);

    /* ---------------------------------------------------------------- */
    SECTION("in pausa: espressione statica PAUSED");
    mascot_fsm_init(&m, 0, 999);
    mascot_fsm_update(&m, 0, true, false);          /* fa partire lo splash */
    CHECK_EQ(mascot_fsm_update(&m, 2000, true, false), MASCOT_PAUSED);
    {
        int non_paused = 0;
        for (uint32_t t = 2000; t < 60000; t += 50) {
            if (mascot_fsm_update(&m, t, true, false) != MASCOT_PAUSED) {
                non_paused++;
            }
        }
        CHECK_EQ(non_paused, 0);   /* mai un blink mentre e' in pausa */
    }

    /* ---------------------------------------------------------------- */
    SECTION("riproduzione: blink periodico entro 4-9 s, durata ~180 ms");
    mascot_fsm_init(&m, 0, 0xBEEF);
    mascot_fsm_update(&m, 0, true, true);
    {
        uint32_t last_blink_end = 2000;   /* fine dello splash */
        int      blinks = 0;
        int      interval_ok = 1, duration_ok = 1;
        int      in_blink = 0;
        uint32_t blink_start = 0;

        for (uint32_t t = 2000; t < 120000; t += 10) {
            mascot_frame_t f = mascot_fsm_update(&m, t, true, true);
            if (f == MASCOT_BLINK && !in_blink) {
                in_blink    = 1;
                blink_start = t;
                uint32_t gap = t - last_blink_end;
                /* +-10 ms di tolleranza per il passo di campionamento */
                if (gap < MASCOT_BLINK_MIN_MS - 10 || gap > MASCOT_BLINK_MAX_MS + 10) {
                    interval_ok = 0;
                    printf("     intervallo fuori range: %u ms\n", gap);
                }
            } else if (f != MASCOT_BLINK && in_blink) {
                in_blink = 0;
                blinks++;
                uint32_t dur = t - blink_start;
                if (dur < MASCOT_BLINK_MS - 10 || dur > MASCOT_BLINK_MS + 20) {
                    duration_ok = 0;
                    printf("     durata blink fuori range: %u ms\n", dur);
                }
                last_blink_end = t;
            }
        }
        CHECK(blinks > 10);            /* ~118 s / max 9.2 s -> almeno 12 */
        CHECK(interval_ok);
        CHECK(duration_ok);
    }

    /* ---------------------------------------------------------------- */
    SECTION("a parita' di seed la sequenza e' identica (test riproducibili)");
    {
        mascot_fsm_t a, b;
        mascot_fsm_init(&a, 0, 4242);
        mascot_fsm_init(&b, 0, 4242);
        int diff = 0;
        for (uint32_t t = 0; t < 60000; t += 25) {
            if (mascot_fsm_update(&a, t, true, true) !=
                mascot_fsm_update(&b, t, true, true)) {
                diff++;
            }
        }
        CHECK_EQ(diff, 0);
    }

    /* ---------------------------------------------------------------- */
    SECTION("semi diversi -> sequenze di blink diverse");
    {
        mascot_fsm_t a, b;
        mascot_fsm_init(&a, 0, 1);
        mascot_fsm_init(&b, 0, 7777777);
        int diff = 0;
        for (uint32_t t = 0; t < 60000; t += 25) {
            if (mascot_fsm_update(&a, t, true, true) !=
                mascot_fsm_update(&b, t, true, true)) {
                diff++;
            }
        }
        CHECK(diff > 0);
    }

    /* ---------------------------------------------------------------- */
    SECTION("dopo una pausa lunga il blink non scatta subito alla ripresa");
    mascot_fsm_init(&m, 0, 31337);
    mascot_fsm_update(&m, 0, true, true);
    for (uint32_t t = 2000; t < 40000; t += 50) {
        mascot_fsm_update(&m, t, true, false);   /* in pausa a lungo */
    }
    CHECK_EQ(mascot_fsm_update(&m, 40000, true, true), MASCOT_IDLE);
    CHECK_EQ(mascot_fsm_update(&m, 40100, true, true), MASCOT_IDLE);
    {
        /* il primo blink dopo la ripresa deve stare comunque nell'intervallo */
        long first = -1;
        for (uint32_t t = 40000; t < 55000; t += 10) {
            if (mascot_fsm_update(&m, t, true, true) == MASCOT_BLINK) {
                first = (long)(t - 40000);
                break;
            }
        }
        printf("     primo blink dopo la ripresa: %ld ms\n", first);
        CHECK(first >= (long)MASCOT_BLINK_MIN_MS - 60);
        CHECK(first <= (long)MASCOT_BLINK_MAX_MS + 60);
    }

    (void)frame_name;
    return t_summary("test_mascot");
}
