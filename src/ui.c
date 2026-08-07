#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "pico/time.h"

#include "battery.h"
#include "buttons.h"
#include "config.h"
#include "mascot.h"
#include "mascot_fsm.h"
#include "playlist.h"
#include "shared_state.h"

/* ------------------------------------------------------------------ */
/* Stato della UI                                                      */
/* ------------------------------------------------------------------ */

static ssd1306_t   *s_d;
static ui_screen_t  s_screen;
static uint32_t     s_last_input_ms;
static uint16_t     s_pl_sel;          /* selezione in Playlist Switch */
static mascot_fsm_t s_mascot;

/* Copia locale dello stato condiviso, presa una volta per frame. */
typedef struct {
    bool        ready;
    bool        playing;
    bool        shuffle;
    sd_status_t sd;
    uint16_t    pl_index, pl_count;
    uint16_t    tr_index, tr_prev, tr_next, tr_count;
    uint32_t    elapsed, duration;
    uint8_t     volume;
    char        track[PP_NAME_LEN];
    char        track_prev[PP_NAME_LEN];
    char        track_next[PP_NAME_LEN];
    char        playlist[PP_NAME_LEN];
} ui_snap_t;

static ui_snap_t s_snap;

/* ------------------------------------------------------------------ */
/* Helper                                                              */
/* ------------------------------------------------------------------ */

static uint32_t now_ms(void)
{
    return to_ms_since_boot(get_absolute_time());
}

/* Copia un nome file togliendo l'estensione .mp3, che sullo schermo e' solo
 * rumore. */
static void copy_title(char *dst, size_t dst_len, const char *src)
{
    size_t n = strlen(src);
    if (n >= 4) {
        const char *ext = src + n - 4;
        if ((ext[0] == '.') &&
            (ext[1] == 'm' || ext[1] == 'M') &&
            (ext[2] == 'p' || ext[2] == 'P') &&
            (ext[3] == '3')) {
            n -= 4;
        }
    }
    if (n >= dst_len) {
        n = dst_len - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void snapshot(void)
{
    critical_section_enter_blocking(&g_shared.lock);

    s_snap.ready    = g_shared.ready;
    s_snap.playing  = g_shared.playing;
    s_snap.shuffle  = g_shared.shuffle;
    s_snap.sd       = g_shared.sd_status;
    s_snap.pl_index = g_shared.playlist_index;
    s_snap.pl_count = g_shared.playlist_count;
    s_snap.tr_index = g_shared.track_index;
    s_snap.tr_prev  = g_shared.track_prev;
    s_snap.tr_next  = g_shared.track_next;
    s_snap.tr_count = g_shared.track_count;
    s_snap.elapsed  = g_shared.elapsed_sec;
    s_snap.duration = g_shared.duration_sec;
    s_snap.volume   = g_shared.volume;

    s_snap.track[0] = s_snap.track_prev[0] = s_snap.track_next[0] = '\0';
    if (s_snap.tr_count > 0) {
        if (s_snap.tr_index < s_snap.tr_count) {
            copy_title(s_snap.track, sizeof(s_snap.track),
                       g_shared.track_names[s_snap.tr_index]);
        }
        if (s_snap.tr_prev < s_snap.tr_count) {
            copy_title(s_snap.track_prev, sizeof(s_snap.track_prev),
                       g_shared.track_names[s_snap.tr_prev]);
        }
        if (s_snap.tr_next < s_snap.tr_count) {
            copy_title(s_snap.track_next, sizeof(s_snap.track_next),
                       g_shared.track_names[s_snap.tr_next]);
        }
    }

    s_snap.playlist[0] = '\0';
    if (s_snap.pl_index < s_snap.pl_count) {
        strncpy(s_snap.playlist, g_shared.playlist_names[s_snap.pl_index],
                PP_NAME_LEN - 1);
        s_snap.playlist[PP_NAME_LEN - 1] = '\0';
    }

    critical_section_exit(&g_shared.lock);
}

static void set_volume(int delta)
{
    int v = (int)s_snap.volume + delta;
    if (v < 0) {
        v = 0;
    }
    if (v > VOLUME_MAX) {
        v = VOLUME_MAX;
    }
    critical_section_enter_blocking(&g_shared.lock);
    g_shared.volume = (uint8_t)v;
    critical_section_exit(&g_shared.lock);
    s_snap.volume = (uint8_t)v;
}

static void set_screen(ui_screen_t s)
{
    s_screen        = s;
    s_last_input_ms = now_ms();

    /* Il doppio click serve solo a LEFT/RIGHT in Song Browse. Altrove va
     * disattivato, altrimenti ogni click singolo verrebbe ritardato di
     * BTN_DCLICK_MS e il volume in Home sembrerebbe "in ritardo". */
    bool dclick = (s == UI_BROWSE);
    buttons_set_dclick(BTN_LEFT, dclick);
    buttons_set_dclick(BTN_RIGHT, dclick);
}

static void fmt_time(char *out, size_t n, uint32_t sec)
{
    snprintf(out, n, "%lu:%02lu",
             (unsigned long)(sec / 60), (unsigned long)(sec % 60));
}

/* ------------------------------------------------------------------ */
/* Gestione eventi                                                     */
/* ------------------------------------------------------------------ */

static void on_home(const btn_event_t *ev)
{
    switch (ev->type) {
    case BTN_EV_CLICK:
        if (ev->button == BTN_LEFT) {
            set_volume(-VOLUME_STEP);
        } else if (ev->button == BTN_RIGHT) {
            set_volume(+VOLUME_STEP);
        } else if (ev->button == BTN_OK) {
            pp_send_cmd(CMD_PLAY_PAUSE, 0);
        } else if (ev->button == BTN_MENU) {
            set_screen(UI_BROWSE);
        }
        break;

    case BTN_EV_HOLD_START:
        if (ev->button == BTN_OK) {
            pp_send_cmd(CMD_SET_SHUFFLE, s_snap.shuffle ? 0 : 1);
        } else if (ev->button == BTN_MENU) {
            s_pl_sel = s_snap.pl_index;
            set_screen(UI_PLAYLIST);
        }
        break;

    case BTN_EV_HOLD_TICK: {
        int step = (int)button_hold_step(VOLUME_STEP, VOLUME_STEP_CAP, ev->repeat);
        if (ev->button == BTN_LEFT) {
            set_volume(-step);
        } else if (ev->button == BTN_RIGHT) {
            set_volume(+step);
        }
        break;
    }

    default:
        break;
    }
}

static void on_browse(const btn_event_t *ev)
{
    switch (ev->type) {
    case BTN_EV_CLICK:
        if (ev->button == BTN_LEFT) {
            pp_send_cmd(CMD_SEEK, -SEEK_STEP_SEC);
        } else if (ev->button == BTN_RIGHT) {
            pp_send_cmd(CMD_SEEK, +SEEK_STEP_SEC);
        } else if (ev->button == BTN_OK) {
            pp_send_cmd(CMD_PLAY_PAUSE, 0);
        } else if (ev->button == BTN_MENU) {
            set_screen(UI_HOME);
        }
        break;

    case BTN_EV_DOUBLE_CLICK:
        if (ev->button == BTN_LEFT) {
            pp_send_cmd(CMD_PREV_TRACK, 0);
        } else if (ev->button == BTN_RIGHT) {
            pp_send_cmd(CMD_NEXT_TRACK, 0);
        }
        break;

    case BTN_EV_HOLD_TICK: {
        int step = (int)button_hold_step(SEEK_STEP_SEC, SEEK_STEP_CAP_SEC, ev->repeat);
        if (ev->button == BTN_LEFT) {
            pp_send_cmd(CMD_SEEK, -step);
        } else if (ev->button == BTN_RIGHT) {
            pp_send_cmd(CMD_SEEK, +step);
        }
        break;
    }

    default:
        break;
    }
}

static void on_playlist(const btn_event_t *ev)
{
    if (ev->type != BTN_EV_CLICK) {
        return;   /* in questa schermata l'hold non fa nulla */
    }
    if (s_snap.pl_count == 0) {
        if (ev->button == BTN_MENU || ev->button == BTN_OK) {
            set_screen(UI_HOME);
        }
        return;
    }

    switch (ev->button) {
    case BTN_LEFT:
        s_pl_sel = (uint16_t)((s_pl_sel + s_snap.pl_count - 1) % s_snap.pl_count);
        break;
    case BTN_RIGHT:
        s_pl_sel = (uint16_t)((s_pl_sel + 1) % s_snap.pl_count);
        break;
    case BTN_OK:
        pp_send_cmd(CMD_SELECT_PLAYLIST, s_pl_sel);
        set_screen(UI_HOME);
        break;
    case BTN_MENU:
        set_screen(UI_HOME);   /* annulla senza cambiare nulla */
        break;
    default:
        break;
    }
}

void ui_handle_events(void)
{
    btn_event_t ev;

    snapshot();

    while (buttons_get_event(&ev)) {
        s_last_input_ms = now_ms();

        switch (s_screen) {
        case UI_HOME:     on_home(&ev);     break;
        case UI_BROWSE:   on_browse(&ev);   break;
        case UI_PLAYLIST: on_playlist(&ev); break;
        }
    }

    if (s_screen == UI_BROWSE &&
        (now_ms() - s_last_input_ms) >= UI_BROWSE_TIMEOUT_MS) {
        set_screen(UI_HOME);
    }
}

/* ------------------------------------------------------------------ */
/* Disegno                                                             */
/* ------------------------------------------------------------------ */

static void draw_header_right(int y, const char *s)
{
    int w = ssd1306_text_width(s);
    ssd1306_text(s_d, OLED_WIDTH - w, y, s, true);
}

static void draw_mode_tag(int y)
{
    const char *tag = s_snap.shuffle ? "SHUFFLE" : "LINEAR";
    int w = ssd1306_text_width(tag);
    ssd1306_text(s_d, OLED_WIDTH - w, y, tag, true);
}

static void draw_home(mascot_frame_t frame)
{
    char buf[24];

    mascot_draw(s_d, 0, 0, frame);

    int pct = battery_get_percent();
    if (pct < 0) {
        snprintf(buf, sizeof(buf), "BAT --%%");
    } else {
        snprintf(buf, sizeof(buf), "BAT %3d%%", pct);
    }
    draw_header_right(0, buf);

    snprintf(buf, sizeof(buf), "VOL %3d", s_snap.volume);
    draw_header_right(9, buf);

    ssd1306_hline(s_d, 0, 18, OLED_WIDTH, true);

    if (s_snap.sd == SD_STATUS_ERROR) {
        ssd1306_text(s_d, 0, 24, "SD assente o", true);
        ssd1306_text(s_d, 0, 34, "non leggibile", true);
    } else if (!s_snap.ready) {
        ssd1306_text(s_d, 0, 24, "Lettura SD...", true);
    } else if (s_snap.tr_count == 0) {
        ssd1306_text(s_d, 0, 24, "Nessun .mp3", true);
    } else {
        ssd1306_text_clip(s_d, 0, 22, s_snap.track, OLED_WIDTH, true);

        char t1[12], t2[12];
        fmt_time(t1, sizeof(t1), s_snap.elapsed);
        fmt_time(t2, sizeof(t2), s_snap.duration);
        snprintf(buf, sizeof(buf), "%s / %s", t1, t2);
        ssd1306_text(s_d, 0, 33, buf, true);

        snprintf(buf, sizeof(buf), "%u/%u",
                 (unsigned)(s_snap.tr_index + 1), (unsigned)s_snap.tr_count);
        draw_header_right(33, buf);

        /* barra di avanzamento */
        ssd1306_fill_rect(s_d, 0, 43, OLED_WIDTH, 1, true);
        if (s_snap.duration > 0) {
            uint32_t w = (uint32_t)OLED_WIDTH * s_snap.elapsed / s_snap.duration;
            if (w > OLED_WIDTH) {
                w = OLED_WIDTH;
            }
            ssd1306_fill_rect(s_d, 0, 41, (int)w, 5, true);
        }
    }

    ssd1306_hline(s_d, 0, 52, OLED_WIDTH, true);
    ssd1306_text_clip(s_d, 0, 56, playlist_display_name(s_snap.playlist),
                      OLED_WIDTH - ssd1306_text_width("SHUFFLE") - 4, true);
    draw_mode_tag(56);
}

static void draw_list_screen(const char *title, mascot_frame_t frame,
                             const char *prev, const char *cur, const char *next,
                             const char *footer_left, bool footer_mode_tag)
{
    mascot_draw(s_d, OLED_WIDTH - mascot_width(), 0, frame);
    ssd1306_text(s_d, 0, 4, title, true);
    ssd1306_hline(s_d, 0, 18, OLED_WIDTH, true);

    int list_w = OLED_WIDTH - 4;
    ssd1306_text_clip(s_d, 2, 21, prev, list_w, true);

    ssd1306_text_clip(s_d, 2, 32, cur, list_w, true);
    ssd1306_invert_rect(s_d, 0, 30, OLED_WIDTH, 11);

    ssd1306_text_clip(s_d, 2, 43, next, list_w, true);

    ssd1306_hline(s_d, 0, 52, OLED_WIDTH, true);
    if (footer_mode_tag) {
        ssd1306_text_clip(s_d, 0, 56, footer_left,
                          OLED_WIDTH - ssd1306_text_width("SHUFFLE") - 4, true);
        draw_mode_tag(56);
    } else {
        ssd1306_text_clip(s_d, 0, 56, footer_left, OLED_WIDTH, true);
    }
}

static void draw_browse(mascot_frame_t frame)
{
    const char *prev = s_snap.tr_count > 1 ? s_snap.track_prev : "";
    const char *next = s_snap.tr_count > 1 ? s_snap.track_next : "";
    const char *cur  = s_snap.tr_count > 0 ? s_snap.track : "Nessun .mp3";

    draw_list_screen(s_snap.playing ? "PLAY" : "PAUSA", frame,
                     prev, cur, next,
                     playlist_display_name(s_snap.playlist), true);
}

static void draw_playlist(mascot_frame_t frame)
{
    char prev[PP_NAME_LEN], cur[PP_NAME_LEN], next[PP_NAME_LEN];

    prev[0] = cur[0] = next[0] = '\0';

    if (s_snap.pl_count > 0) {
        uint16_t p = (uint16_t)((s_pl_sel + s_snap.pl_count - 1) % s_snap.pl_count);
        uint16_t n = (uint16_t)((s_pl_sel + 1) % s_snap.pl_count);

        critical_section_enter_blocking(&g_shared.lock);
        strncpy(cur,  g_shared.playlist_names[s_pl_sel], PP_NAME_LEN - 1);
        strncpy(prev, g_shared.playlist_names[p], PP_NAME_LEN - 1);
        strncpy(next, g_shared.playlist_names[n], PP_NAME_LEN - 1);
        critical_section_exit(&g_shared.lock);
        cur[PP_NAME_LEN - 1] = prev[PP_NAME_LEN - 1] = next[PP_NAME_LEN - 1] = '\0';

        if (s_snap.pl_count == 1) {
            prev[0] = next[0] = '\0';
        }
    } else {
        strcpy(cur, "Nessuna playlist");
    }

    draw_list_screen("PLAYLIST", frame,
                     s_snap.pl_count > 0 ? playlist_display_name(prev) : "",
                     s_snap.pl_count > 0 ? playlist_display_name(cur) : cur,
                     s_snap.pl_count > 0 ? playlist_display_name(next) : "",
                     "OK=scegli MENU=esc", false);
}

void ui_render(void)
{
    snapshot();

    /* Finche' core1 non ha finito l'init non sappiamo ancora se la SD c'e':
     * non facciamo girare la macchina a stati, cosi' non mostriamo ne' la
     * faccia di errore ne' lo splash "SD ok" prima del tempo. */
    mascot_frame_t frame;
    if (!s_snap.ready) {
        frame = MASCOT_IDLE;
    } else {
        frame = mascot_fsm_update(&s_mascot, now_ms(),
                                  s_snap.sd == SD_STATUS_OK,
                                  s_snap.playing);
    }

    ssd1306_clear(s_d);
    switch (s_screen) {
    case UI_HOME:     draw_home(frame);     break;
    case UI_BROWSE:   draw_browse(frame);   break;
    case UI_PLAYLIST: draw_playlist(frame); break;
    }
    /* buttons_poll fra una pagina e l'altra: il flush completo dura ~23 ms e
     * lasciare il debounce senza campioni cosi' a lungo farebbe perdere le
     * pressioni piu' rapide. */
    ssd1306_show(s_d, buttons_poll);
}

ui_screen_t ui_screen(void)
{
    return s_screen;
}

void ui_init(ssd1306_t *display)
{
    s_d = display;
    mascot_fsm_init(&s_mascot, now_ms(), time_us_32() | 1u);
    set_screen(UI_HOME);
}
