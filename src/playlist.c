#include "playlist.h"

#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "hw_config.h"

static FATFS  s_fs;
static bool   s_mounted;

/* Confronto case-insensitive ASCII, sufficiente per ordinare i nomi file. */
static int ci_cmp(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - 'a' + 'A');
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - 'a' + 'A');
        if (ca != cb) {
            return (int)(unsigned char)ca - (int)(unsigned char)cb;
        }
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static bool has_mp3_ext(const char *name)
{
    size_t n = strlen(name);
    if (n < 5) {              /* almeno "x.mp3" */
        return false;
    }
    return ci_cmp(name + n - 4, ".mp3") == 0;
}

static bool is_skippable(const FILINFO *fi)
{
    if (fi->fname[0] == '.') {
        return true;                       /* nascosti e "." / ".." */
    }
    if (fi->fattrib & (AM_HID | AM_SYS)) {
        return true;
    }
    return false;
}

/* Insertion sort: con al massimo 200 nomi e' istantaneo e non serve memoria
 * extra (cosa che conta su un MCU). */
static void sort_names(char names[][PP_NAME_LEN], int n)
{
    char tmp[PP_NAME_LEN];
    for (int i = 1; i < n; i++) {
        memcpy(tmp, names[i], PP_NAME_LEN);
        int j = i - 1;
        while (j >= 0 && ci_cmp(names[j], tmp) > 0) {
            memcpy(names[j + 1], names[j], PP_NAME_LEN);
            j--;
        }
        memcpy(names[j + 1], tmp, PP_NAME_LEN);
    }
}

bool playlist_mount(void)
{
    if (s_mounted) {
        return true;
    }
    sd_card_t *sd = sd_get_by_num(0);
    if (!sd) {
        return false;
    }
    FRESULT fr = f_mount(&s_fs, sd->pcName, 1 /* mount immediato */);
    if (fr != FR_OK) {
        printf("f_mount fallito: %d\n", fr);
        return false;
    }
    s_mounted = true;
    return true;
}

void playlist_unmount(void)
{
    if (!s_mounted) {
        return;
    }
    sd_card_t *sd = sd_get_by_num(0);
    if (sd) {
        f_mount(NULL, sd->pcName, 0);
    }
    s_mounted = false;
}

static int scan_dir(const char *path, bool want_dirs,
                    char names[][PP_NAME_LEN], int max)
{
    DIR     dir;
    FILINFO fi;
    int     n = 0;

    if (f_opendir(&dir, path) != FR_OK) {
        return 0;
    }
    for (;;) {
        if (f_readdir(&dir, &fi) != FR_OK) {
            break;
        }
        if (fi.fname[0] == 0) {
            break;                       /* fine cartella */
        }
        if (is_skippable(&fi)) {
            continue;
        }
        bool is_dir = (fi.fattrib & AM_DIR) != 0;
        if (want_dirs != is_dir) {
            continue;
        }
        if (!want_dirs && !has_mp3_ext(fi.fname)) {
            continue;
        }
        if (n >= max) {
            printf("attenzione: piu' di %d voci in %s, le altre sono ignorate\n",
                   max, path);
            break;
        }
        strncpy(names[n], fi.fname, PP_NAME_LEN - 1);
        names[n][PP_NAME_LEN - 1] = '\0';
        n++;
    }
    f_closedir(&dir);

    sort_names(names, n);
    return n;
}

int playlist_scan_playlists(char names[][PP_NAME_LEN], int max)
{
    int n = scan_dir("/", true, names, max);
    if (n > 0) {
        return n;
    }

    /* Nessuna sottocartella: se ci sono .mp3 nella root li trattiamo come
     * un'unica playlist, cosi' chi copia i file "alla buona" sente comunque
     * qualcosa. Il nome vuoto identifica la root. */
    char probe[1][PP_NAME_LEN];
    if (scan_dir("/", false, probe, 1) > 0) {
        names[0][0] = '\0';
        return 1;
    }
    return 0;
}

int playlist_scan_tracks(const char *playlist, char names[][PP_NAME_LEN], int max)
{
    char path[PP_NAME_LEN + 2];

    if (!playlist || playlist[0] == '\0') {
        strcpy(path, "/");
    } else {
        snprintf(path, sizeof(path), "/%s", playlist);
    }
    return scan_dir(path, false, names, max);
}

void playlist_build_path(char *out, size_t out_len,
                         const char *playlist, const char *track)
{
    if (!playlist || playlist[0] == '\0') {
        snprintf(out, out_len, "/%s", track);
    } else {
        snprintf(out, out_len, "/%s/%s", playlist, track);
    }
}

const char *playlist_display_name(const char *playlist)
{
    if (!playlist || playlist[0] == '\0') {
        return "SD Root";
    }
    return playlist;
}
