#include "mascot.h"

#include "mascot_bitmaps.h"

void mascot_draw(ssd1306_t *d, int x, int y, mascot_frame_t frame)
{
    if (frame < 0 || frame >= MASCOT_FRAME_COUNT) {
        frame = MASCOT_IDLE;
    }
    ssd1306_bitmap(d, x, y, mascot_bitmaps[frame],
                   MASCOT_W, MASCOT_H, MASCOT_STRIDE);
}

int mascot_width(void)
{
    return MASCOT_W;
}

int mascot_height(void)
{
    return MASCOT_H;
}
