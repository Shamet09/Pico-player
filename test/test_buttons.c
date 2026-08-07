/* test_buttons.c - unit test nativi per button_fsm.
 *
 * Verificano: reiezione dei glitch piu' corti del debounce, click singolo
 * immediato quando il doppio click e' disattivato, click ritardato e doppio
 * click quando e' attivo, hold con i tick alla cadenza attesa, accelerazione
 * esponenziale, indipendenza fra pulsanti e soglia di hold dedicata a MENU.
 */
#include <string.h>

#include "button_fsm.h"
#include "test_util.h"

#define LOG_MAX 512

static button_fsm_t fsm;
static bool         raw[BTN_COUNT];
static uint32_t     clk;

static struct {
    uint32_t    t;
    btn_event_t ev;
} log[LOG_MAX];
static int log_n;

static const char *ev_name(int type)
{
    switch (type) {
    case BTN_EV_CLICK:        return "CLICK";
    case BTN_EV_DOUBLE_CLICK: return "DOUBLE";
    case BTN_EV_HOLD_START:   return "HOLD_START";
    case BTN_EV_HOLD_TICK:    return "HOLD_TICK";
    case BTN_EV_HOLD_END:     return "HOLD_END";
    default:                  return "?";
    }
}

static void setup(uint32_t hold_menu_ms)
{
    btn_cfg_t cfg[BTN_COUNT];
    for (int i = 0; i < BTN_COUNT; i++) {
        cfg[i].debounce_ms  = 25;
        cfg[i].hold_ms      = 500;
        cfg[i].hold_tick_ms = 400;
        cfg[i].dclick_ms    = 350;
    }
    cfg[BTN_MENU].hold_ms = hold_menu_ms;

    memset(raw, 0, sizeof(raw));
    log_n = 0;
    clk   = 0;
    button_fsm_init(&fsm, cfg, clk);
}

static void step_at(uint32_t t)
{
    btn_event_t evs[8];
    int n = button_fsm_update(&fsm, t, raw, evs, 8);
    for (int i = 0; i < n && log_n < LOG_MAX; i++) {
        log[log_n].t  = t;
        log[log_n].ev = evs[i];
        log_n++;
    }
}

/* Avanza il tempo a passi di 1 ms fino a `target`, registrando gli eventi. */
static void run_to(uint32_t target)
{
    while (clk < target) {
        clk++;
        step_at(clk);
    }
}

/* Il cambio di livello viene campionato nello stesso istante in cui avviene,
 * cosi' i tempi attesi nei test sono quelli "veri" (istante del fronte +
 * debounce) e non dipendono dal passo del banco di prova. */
static void press(btn_id_t b)   { raw[b] = true;  step_at(clk); }
static void release(btn_id_t b) { raw[b] = false; step_at(clk); }

/* Conta gli eventi di un certo tipo su un certo pulsante. */
static int count_of(btn_id_t b, int type)
{
    int n = 0;
    for (int i = 0; i < log_n; i++) {
        if (log[i].ev.button == b && log[i].ev.type == type) {
            n++;
        }
    }
    return n;
}

/* Istante del k-esimo evento (0-based) di quel tipo, oppure -1. */
static long time_of(btn_id_t b, int type, int k)
{
    for (int i = 0; i < log_n; i++) {
        if (log[i].ev.button == b && log[i].ev.type == type) {
            if (k-- == 0) {
                return (long)log[i].t;
            }
        }
    }
    return -1;
}

static void dump(void)
{
    for (int i = 0; i < log_n; i++) {
        printf("     t=%-5u btn=%u %-10s repeat=%u\n",
               log[i].t, log[i].ev.button, ev_name(log[i].ev.type), log[i].ev.repeat);
    }
}

int main(void)
{
    /* ---------------------------------------------------------------- */
    SECTION("glitch piu' corto del debounce: nessun evento");
    setup(600);
    run_to(100);
    press(BTN_LEFT);
    run_to(110);            /* premuto solo 10 ms < 25 ms di debounce */
    release(BTN_LEFT);
    run_to(2000);
    CHECK_EQ(log_n, 0);

    /* ---------------------------------------------------------------- */
    SECTION("glitch ripetuti (rimbalzo del contatto) non generano click");
    setup(600);
    run_to(100);
    for (int i = 0; i < 6; i++) {   /* 6 rimbalzi da 8 ms */
        press(BTN_OK);
        run_to(clk + 8);
        release(BTN_OK);
        run_to(clk + 8);
    }
    run_to(3000);
    CHECK_EQ(log_n, 0);

    /* ---------------------------------------------------------------- */
    SECTION("click singolo senza doppio click: nessun ritardo");
    setup(600);
    run_to(100);
    press(BTN_LEFT);
    run_to(200);
    release(BTN_LEFT);
    run_to(1000);
    CHECK_EQ(count_of(BTN_LEFT, BTN_EV_CLICK), 1);
    /* rilascio a 200 -> stabile a 225 -> click subito */
    CHECK_EQ(time_of(BTN_LEFT, BTN_EV_CLICK, 0), 225);
    CHECK_EQ(count_of(BTN_LEFT, BTN_EV_DOUBLE_CLICK), 0);

    /* ---------------------------------------------------------------- */
    SECTION("click singolo con doppio click attivo: ritardato di 350 ms");
    setup(600);
    button_fsm_set_dclick(&fsm, BTN_LEFT, true);
    run_to(100);
    press(BTN_LEFT);
    run_to(200);
    release(BTN_LEFT);
    run_to(500);
    CHECK_EQ(count_of(BTN_LEFT, BTN_EV_CLICK), 0);   /* non ancora */
    run_to(1000);
    CHECK_EQ(count_of(BTN_LEFT, BTN_EV_CLICK), 1);
    CHECK_EQ(time_of(BTN_LEFT, BTN_EV_CLICK, 0), 225 + 350);

    /* ---------------------------------------------------------------- */
    SECTION("doppio click");
    setup(600);
    button_fsm_set_dclick(&fsm, BTN_RIGHT, true);
    run_to(100);
    press(BTN_RIGHT);
    run_to(180);
    release(BTN_RIGHT);      /* stabile a 205 -> finestra fino a 555 */
    run_to(300);
    press(BTN_RIGHT);        /* stabile a 325: dentro la finestra     */
    run_to(400);
    release(BTN_RIGHT);
    run_to(1500);
    CHECK_EQ(count_of(BTN_RIGHT, BTN_EV_DOUBLE_CLICK), 1);
    CHECK_EQ(count_of(BTN_RIGHT, BTN_EV_CLICK), 0);  /* niente click extra */
    CHECK_EQ(time_of(BTN_RIGHT, BTN_EV_DOUBLE_CLICK, 0), 325);

    /* ---------------------------------------------------------------- */
    SECTION("secondo click fuori finestra: due click singoli");
    setup(600);
    button_fsm_set_dclick(&fsm, BTN_RIGHT, true);
    run_to(100);
    press(BTN_RIGHT);
    run_to(180);
    release(BTN_RIGHT);      /* stabile a 205, finestra fino a 555 */
    run_to(700);             /* il primo click e' gia' scattato a 555 */
    press(BTN_RIGHT);
    run_to(780);
    release(BTN_RIGHT);
    run_to(1600);
    CHECK_EQ(count_of(BTN_RIGHT, BTN_EV_CLICK), 2);
    CHECK_EQ(count_of(BTN_RIGHT, BTN_EV_DOUBLE_CLICK), 0);

    /* ---------------------------------------------------------------- */
    SECTION("hold: HOLD_START e tick a 400 ms");
    setup(600);
    run_to(100);
    press(BTN_LEFT);
    run_to(2000);
    /* stabile a 125, soglia 500 -> primo hold a 625 */
    CHECK_EQ(count_of(BTN_LEFT, BTN_EV_HOLD_START), 1);
    CHECK_EQ(time_of(BTN_LEFT, BTN_EV_HOLD_START, 0), 625);
    CHECK_EQ(time_of(BTN_LEFT, BTN_EV_HOLD_TICK, 0), 625);   /* subito */
    CHECK_EQ(time_of(BTN_LEFT, BTN_EV_HOLD_TICK, 1), 1025);
    CHECK_EQ(time_of(BTN_LEFT, BTN_EV_HOLD_TICK, 2), 1425);
    CHECK_EQ(time_of(BTN_LEFT, BTN_EV_HOLD_TICK, 3), 1825);
    CHECK_EQ(log[1].ev.repeat, 1);   /* il primo tick ha repeat = 1 */

    /* ---------------------------------------------------------------- */
    SECTION("hold: al rilascio niente click, solo HOLD_END");
    release(BTN_LEFT);
    run_to(3000);
    CHECK_EQ(count_of(BTN_LEFT, BTN_EV_CLICK), 0);
    CHECK_EQ(count_of(BTN_LEFT, BTN_EV_HOLD_END), 1);

    /* ---------------------------------------------------------------- */
    SECTION("MENU ha una soglia di hold dedicata (600 ms)");
    setup(600);
    run_to(100);
    press(BTN_MENU);
    run_to(700);
    CHECK_EQ(count_of(BTN_MENU, BTN_EV_HOLD_START), 0);  /* 125+600 = 725 */
    run_to(800);
    CHECK_EQ(count_of(BTN_MENU, BTN_EV_HOLD_START), 1);
    CHECK_EQ(time_of(BTN_MENU, BTN_EV_HOLD_START, 0), 725);

    /* ---------------------------------------------------------------- */
    SECTION("MENU sotto la soglia di hold: click semplice");
    setup(600);
    run_to(100);
    press(BTN_MENU);
    run_to(400);            /* 275 ms premuto: sotto i 600 */
    release(BTN_MENU);
    run_to(1200);
    CHECK_EQ(count_of(BTN_MENU, BTN_EV_CLICK), 1);
    CHECK_EQ(count_of(BTN_MENU, BTN_EV_HOLD_START), 0);

    /* ---------------------------------------------------------------- */
    SECTION("i pulsanti sono indipendenti");
    setup(600);
    run_to(100);
    press(BTN_LEFT);
    run_to(200);
    release(BTN_LEFT);
    run_to(300);
    CHECK_EQ(count_of(BTN_LEFT, BTN_EV_CLICK), 1);
    CHECK_EQ(count_of(BTN_RIGHT, BTN_EV_CLICK), 0);
    CHECK_EQ(count_of(BTN_OK, BTN_EV_CLICK), 0);
    CHECK_EQ(count_of(BTN_MENU, BTN_EV_CLICK), 0);

    /* ---------------------------------------------------------------- */
    SECTION("un hold annulla il click in attesa (nessun click fantasma)");
    setup(600);
    button_fsm_set_dclick(&fsm, BTN_LEFT, true);
    run_to(100);
    press(BTN_LEFT);
    run_to(180);
    release(BTN_LEFT);       /* click pendente fino a 555 */
    run_to(300);
    press(BTN_LEFT);         /* secondo click -> DOUBLE a 325 */
    run_to(1500);            /* poi resta premuto -> hold      */
    CHECK_EQ(count_of(BTN_LEFT, BTN_EV_DOUBLE_CLICK), 1);
    CHECK_EQ(count_of(BTN_LEFT, BTN_EV_CLICK), 0);
    CHECK(count_of(BTN_LEFT, BTN_EV_HOLD_START) == 1);

    /* ---------------------------------------------------------------- */
    SECTION("polling lento (33 ms): l'evento arriva comunque una volta sola");
    {
        btn_cfg_t cfg[BTN_COUNT];
        for (int i = 0; i < BTN_COUNT; i++) {
            cfg[i].debounce_ms  = 25;
            cfg[i].hold_ms      = 500;
            cfg[i].hold_tick_ms = 400;
            cfg[i].dclick_ms    = 350;
        }
        memset(raw, 0, sizeof(raw));
        log_n = 0;
        button_fsm_init(&fsm, cfg, 0);

        btn_event_t evs[8];
        raw[BTN_OK] = true;
        for (uint32_t t = 33; t <= 2000; t += 33) {   /* campionamento rado */
            int n = button_fsm_update(&fsm, t, raw, evs, 8);
            for (int i = 0; i < n && log_n < LOG_MAX; i++) {
                log[log_n].t = t;
                log[log_n].ev = evs[i];
                log_n++;
            }
        }
        /* Da 0 a 2000 ms: hold a ~533, poi tick ogni 400 ms.
         * Con campionamento a 33 ms i tick restano 4 (nessuna raffica). */
        CHECK_EQ(count_of(BTN_OK, BTN_EV_HOLD_START), 1);
        CHECK_EQ(count_of(BTN_OK, BTN_EV_HOLD_TICK), 4);
    }

    /* ---------------------------------------------------------------- */
    SECTION("button_hold_step: accelerazione esponenziale con tetto");
    CHECK_EQ(button_hold_step(5, 25, 1), 5);
    CHECK_EQ(button_hold_step(5, 25, 2), 10);
    CHECK_EQ(button_hold_step(5, 25, 3), 20);
    CHECK_EQ(button_hold_step(5, 25, 4), 25);   /* tetto */
    CHECK_EQ(button_hold_step(5, 25, 50), 25);
    CHECK_EQ(button_hold_step(10, 60, 1), 10);
    CHECK_EQ(button_hold_step(10, 60, 2), 20);
    CHECK_EQ(button_hold_step(10, 60, 3), 40);
    CHECK_EQ(button_hold_step(10, 60, 4), 60);
    CHECK_EQ(button_hold_step(10, 60, 1000), 60);
    CHECK_EQ(button_hold_step(5, 25, 0), 0);

    if (t_failed) {
        printf("\nlog dell'ultimo scenario:\n");
        dump();
    }
    return t_summary("test_buttons");
}
