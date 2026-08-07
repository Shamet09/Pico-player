#include "buttons.h"

#include "hardware/gpio.h"
#include "pico/time.h"

#include "config.h"

#define BTN_QUEUE_LEN 16

static const uint8_t btn_pins[BTN_COUNT] = {
    [BTN_LEFT]  = PIN_BTN_LEFT,
    [BTN_RIGHT] = PIN_BTN_RIGHT,
    [BTN_OK]    = PIN_BTN_OK,
    [BTN_MENU]  = PIN_BTN_MENU,
};

static button_fsm_t fsm;
static btn_event_t  queue[BTN_QUEUE_LEN];
static uint8_t      q_head, q_tail;

static void queue_push(const btn_event_t *ev)
{
    uint8_t next = (uint8_t)((q_head + 1) % BTN_QUEUE_LEN);
    if (next == q_tail) {
        return; /* coda piena: scartiamo (non dovrebbe mai succedere) */
    }
    queue[q_head] = *ev;
    q_head = next;
}

void buttons_init(void)
{
    btn_cfg_t cfg[BTN_COUNT];

    for (int i = 0; i < BTN_COUNT; i++) {
        cfg[i].debounce_ms  = BTN_DEBOUNCE_MS;
        cfg[i].hold_ms      = BTN_HOLD_MS;
        cfg[i].hold_tick_ms = BTN_HOLD_TICK_MS;
        cfg[i].dclick_ms    = BTN_DCLICK_MS;
    }
    /* MENU ha una soglia di hold dedicata e piu' lunga: serve a distinguere
     * "apri Song Browse" da "apri Playlist Switch". */
    cfg[BTN_MENU].hold_ms = BTN_HOLD_MENU_MS;

    for (int i = 0; i < BTN_COUNT; i++) {
        gpio_init(btn_pins[i]);
        gpio_set_dir(btn_pins[i], GPIO_IN);
        gpio_pull_up(btn_pins[i]);
    }
    /* UP e DOWN sono cablati ma non usati: li mettiamo comunque in input con
     * pull-up per non lasciarli flottanti. */
    gpio_init(PIN_BTN_UP);
    gpio_set_dir(PIN_BTN_UP, GPIO_IN);
    gpio_pull_up(PIN_BTN_UP);
    gpio_init(PIN_BTN_DOWN);
    gpio_set_dir(PIN_BTN_DOWN, GPIO_IN);
    gpio_pull_up(PIN_BTN_DOWN);

    q_head = q_tail = 0;
    button_fsm_init(&fsm, cfg, to_ms_since_boot(get_absolute_time()));
}

void buttons_poll(void)
{
    bool        raw[BTN_COUNT];
    btn_event_t evs[8];

    for (int i = 0; i < BTN_COUNT; i++) {
        raw[i] = !gpio_get(btn_pins[i]);  /* attivi bassi */
    }

    uint32_t now = to_ms_since_boot(get_absolute_time());
    int n = button_fsm_update(&fsm, now, raw, evs, (int)(sizeof(evs) / sizeof(evs[0])));
    for (int i = 0; i < n; i++) {
        queue_push(&evs[i]);
    }
}

bool buttons_get_event(btn_event_t *ev)
{
    if (q_tail == q_head) {
        return false;
    }
    *ev = queue[q_tail];
    q_tail = (uint8_t)((q_tail + 1) % BTN_QUEUE_LEN);
    return true;
}

void buttons_set_dclick(btn_id_t button, bool enabled)
{
    button_fsm_set_dclick(&fsm, button, enabled);
}
