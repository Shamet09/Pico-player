/* ssd1306.h - driver minimale per OLED SSD1306 128x64 su I2C.
 *
 * Framebuffer in RAM (1024 byte) + flush a pagine. Il flush accetta una
 * callback `yield` chiamata fra una pagina e l'altra: serve per continuare a
 * campionare i pulsanti mentre si scrive sul display (un frame intero a
 * 400 kHz dura circa 23 ms, troppo per lasciare il debounce senza campioni).
 */
#ifndef PP_SSD1306_H
#define PP_SSD1306_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/i2c.h"

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_PAGES (OLED_HEIGHT / 8)

typedef struct {
    i2c_inst_t *i2c;
    uint8_t     addr;
    uint8_t     buf[OLED_WIDTH * SSD1306_PAGES];
} ssd1306_t;

/* Inizializza il pannello. Ritorna false se il display non risponde
 * sull'indirizzo I2C configurato. */
bool ssd1306_init(ssd1306_t *d, i2c_inst_t *i2c, uint8_t addr);

void ssd1306_clear(ssd1306_t *d);
void ssd1306_show(ssd1306_t *d, void (*yield)(void));
void ssd1306_set_contrast(ssd1306_t *d, uint8_t contrast);

void ssd1306_pixel(ssd1306_t *d, int x, int y, bool on);
void ssd1306_fill_rect(ssd1306_t *d, int x, int y, int w, int h, bool on);
void ssd1306_hline(ssd1306_t *d, int x, int y, int w, bool on);
void ssd1306_invert_rect(ssd1306_t *d, int x, int y, int w, int h);

/* Testo con font 5x7. Ritorna la x dopo l'ultimo carattere disegnato. */
int  ssd1306_char(ssd1306_t *d, int x, int y, char c, bool on);
int  ssd1306_text(ssd1306_t *d, int x, int y, const char *s, bool on);
/* Come ssd1306_text ma tronca a `max_w` pixel aggiungendo ".." se serve. */
int  ssd1306_text_clip(ssd1306_t *d, int x, int y, const char *s, int max_w, bool on);
int  ssd1306_text_width(const char *s);

/* Bitmap 1 bpp, MSB a sinistra, `stride` byte per riga. */
void ssd1306_bitmap(ssd1306_t *d, int x, int y, const uint8_t *bmp,
                    int w, int h, int stride);

#ifdef __cplusplus
}
#endif

#endif /* PP_SSD1306_H */
