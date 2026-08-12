/* main.c - init dell'hardware, lancio di core1, loop principale di core0.
 *
 * core0: pulsanti, OLED, mascotte.
 * core1: SD + decodifica MP3 + I2S (vedi player.c).
 */
#include <stdio.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#include "battery.h"
#include "buttons.h"
#include "config.h"
#include "player.h"
#include "shared_state.h"
#include "ssd1306.h"
#include "ui.h"

#define UI_FRAME_MS 66   /* ~15 fps: piu' che sufficiente e lascia banda I2C */

static ssd1306_t s_oled;

static void i2c_setup(void)
{
    i2c_init(OLED_I2C_INST, OLED_I2C_BAUD);
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    /* I moduli SSD1306 hanno gia' i pull-up a bordo; questi sono solo una
     * rete di sicurezza se si cabla il pannello nudo. */
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);
}

int main(void)
{
#if PP_SYS_CLOCK_KHZ
    /* Prima di stdio: il baud rate della UART dipende da clk_peri. */
    set_sys_clock_khz(PP_SYS_CLOCK_KHZ, true);
#endif

    stdio_init_all();
    printf("\nPicoPlayer\n");
    printf("clk_sys %lu Hz, SD %lu Hz\n",
           (unsigned long)clock_get_hz(clk_sys), (unsigned long)SD_BAUD_RATE);

    pp_shared_init();

    i2c_setup();
    if (!ssd1306_init(&s_oled, OLED_I2C_INST, OLED_I2C_ADDR)) {
        printf("OLED non risponde all'indirizzo 0x%02X\n", OLED_I2C_ADDR);
    }

    buttons_init();
    battery_init();
    ui_init(&s_oled);

    /* MENU tenuto premuto all'accensione = modo test audio: core1 salta SD e
     * decodifica e manda all'I2S un tono fisso (vedi config.h).
     * I pull-up appena attivati da buttons_init() hanno bisogno di qualche
     * millisecondo per portare a livello alto un pin non premuto. */
    sleep_ms(10);
    g_shared.selftest = !gpio_get(PIN_BTN_MENU);
    if (g_shared.selftest) {
        printf("MENU premuto all'avvio -> modo test audio\n");
    }

    /* Da qui in poi core1 e' l'unico a toccare la SD e FatFs. */
    multicore_launch_core1(player_core1_main);

    absolute_time_t next_frame = make_timeout_time_ms(UI_FRAME_MS);

    for (;;) {
        buttons_poll();
        ui_handle_events();

        if (time_reached(next_frame)) {
            ui_render();
            next_frame = make_timeout_time_ms(UI_FRAME_MS);
        } else {
            sleep_ms(2);   /* il debounce e' a 25 ms: campionare ogni 2 ms
                            * lascia molto margine */
        }
    }
}
