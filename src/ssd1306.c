#include "ssd1306.h"

#include <string.h>

#include "font5x7.h"

/* Sequenza di init verificata contro Adafruit_SSD1306 per pannelli 128x64. */
static const uint8_t init_seq[] = {
    0xAE,             /* display off                                   */
    0xD5, 0x80,       /* clock divide ratio / oscillator frequency     */
    0xA8, 0x3F,       /* multiplex ratio = HEIGHT-1 = 63               */
    0xD3, 0x00,       /* display offset 0                              */
    0x40,             /* start line 0                                  */
    0x8D, 0x14,       /* charge pump ON (Vcc generato internamente)    */
    0x20, 0x00,       /* memory addressing mode = horizontal           */
    0xA1,             /* segment remap                                 */
    0xC8,             /* COM scan decrescente                          */
    0xDA, 0x12,       /* COM pins config per 128x64                    */
    0x81, 0xCF,       /* contrast                                      */
    0xD9, 0xF1,       /* precharge                                     */
    0xDB, 0x40,       /* VCOMH deselect level                          */
    0xA4,             /* resume from RAM                               */
    0xA6,             /* display normale (non invertito)               */
    0x2E,             /* deactivate scroll                             */
    0xAF,             /* display ON                                    */
};

static bool cmd(ssd1306_t *d, const uint8_t *c, size_t n)
{
    /* Control byte 0x00 = "seguono solo comandi". */
    uint8_t tmp[16];
    while (n) {
        size_t chunk = n > sizeof(tmp) - 1 ? sizeof(tmp) - 1 : n;
        tmp[0] = 0x00;
        memcpy(tmp + 1, c, chunk);
        if (i2c_write_blocking(d->i2c, d->addr, tmp, chunk + 1, false) < 0) {
            return false;
        }
        c += chunk;
        n -= chunk;
    }
    return true;
}

bool ssd1306_init(ssd1306_t *d, i2c_inst_t *i2c, uint8_t addr)
{
    d->i2c  = i2c;
    d->addr = addr;
    memset(d->buf, 0, sizeof(d->buf));

    if (!cmd(d, init_seq, sizeof(init_seq))) {
        return false;
    }
    ssd1306_show(d, NULL);
    return true;
}

void ssd1306_clear(ssd1306_t *d)
{
    memset(d->buf, 0, sizeof(d->buf));
}

void ssd1306_set_contrast(ssd1306_t *d, uint8_t contrast)
{
    uint8_t c[2] = { 0x81, contrast };
    cmd(d, c, sizeof(c));
}

void ssd1306_show(ssd1306_t *d, void (*yield)(void))
{
    /* Una pagina alla volta (128 byte, ~3 ms): fra una pagina e l'altra
     * chiamiamo `yield` cosi' il polling dei pulsanti non resta scoperto. */
    static uint8_t line[1 + OLED_WIDTH];

    for (int page = 0; page < SSD1306_PAGES; page++) {
        uint8_t addr_seq[] = {
            0x21, 0x00, OLED_WIDTH - 1,          /* column address range */
            0x22, (uint8_t)page, (uint8_t)page,  /* page address range   */
        };
        if (!cmd(d, addr_seq, sizeof(addr_seq))) {
            return;
        }
        line[0] = 0x40; /* control byte: seguono dati */
        memcpy(line + 1, d->buf + page * OLED_WIDTH, OLED_WIDTH);
        i2c_write_blocking(d->i2c, d->addr, line, sizeof(line), false);

        if (yield) {
            yield();
        }
    }
}

void ssd1306_pixel(ssd1306_t *d, int x, int y, bool on)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
        return;
    }
    uint8_t *p = &d->buf[(y / 8) * OLED_WIDTH + x];
    uint8_t  m = (uint8_t)(1u << (y & 7));
    if (on) {
        *p |= m;
    } else {
        *p &= (uint8_t)~m;
    }
}

void ssd1306_fill_rect(ssd1306_t *d, int x, int y, int w, int h, bool on)
{
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            ssd1306_pixel(d, i, j, on);
        }
    }
}

void ssd1306_hline(ssd1306_t *d, int x, int y, int w, bool on)
{
    for (int i = x; i < x + w; i++) {
        ssd1306_pixel(d, i, y, on);
    }
}

void ssd1306_invert_rect(ssd1306_t *d, int x, int y, int w, int h)
{
    for (int j = y; j < y + h; j++) {
        if (j < 0 || j >= OLED_HEIGHT) {
            continue;
        }
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= OLED_WIDTH) {
                continue;
            }
            d->buf[(j / 8) * OLED_WIDTH + i] ^= (uint8_t)(1u << (j & 7));
        }
    }
}

int ssd1306_char(ssd1306_t *d, int x, int y, char c, bool on)
{
    const uint8_t *g = &font5x7[(uint8_t)c * FONT5X7_W];

    for (int col = 0; col < FONT5X7_W; col++) {
        uint8_t bits = g[col];
        for (int row = 0; row < 8; row++) {
            if (bits & (1u << row)) {
                ssd1306_pixel(d, x + col, y + row, on);
            }
        }
    }
    return x + FONT5X7_ADVANCE;
}

int ssd1306_text(ssd1306_t *d, int x, int y, const char *s, bool on)
{
    while (*s) {
        if (x >= OLED_WIDTH) {
            break;
        }
        x = ssd1306_char(d, x, y, *s++, on);
    }
    return x;
}

int ssd1306_text_width(const char *s)
{
    int n = 0;
    while (s[n]) {
        n++;
    }
    return n * FONT5X7_ADVANCE;
}

int ssd1306_text_clip(ssd1306_t *d, int x, int y, const char *s, int max_w, bool on)
{
    int max_chars = max_w / FONT5X7_ADVANCE;
    int len       = 0;

    while (s[len]) {
        len++;
    }
    if (max_chars <= 0) {
        return x;
    }
    if (len <= max_chars) {
        return ssd1306_text(d, x, y, s, on);
    }
    /* Tronca lasciando spazio per ".." */
    int keep = max_chars - 2;
    if (keep < 1) {
        keep = 1;
    }
    for (int i = 0; i < keep; i++) {
        x = ssd1306_char(d, x, y, s[i], on);
    }
    x = ssd1306_char(d, x, y, '.', on);
    x = ssd1306_char(d, x, y, '.', on);
    return x;
}

void ssd1306_bitmap(ssd1306_t *d, int x, int y, const uint8_t *bmp,
                    int w, int h, int stride)
{
    for (int row = 0; row < h; row++) {
        const uint8_t *r = bmp + row * stride;
        for (int col = 0; col < w; col++) {
            if (r[col / 8] & (0x80u >> (col % 8))) {
                ssd1306_pixel(d, x + col, y + row, true);
            }
        }
    }
}
