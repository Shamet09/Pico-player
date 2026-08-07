/* playlist.h - scansione della microSD: playlist (cartelle) e brani (.mp3).
 *
 * ATTENZIONE: tutte queste funzioni chiamano FatFs. FF_FS_REENTRANT e' 0,
 * quindi devono essere invocate SOLO da core1 (vedi shared_state.h).
 */
#ifndef PP_PLAYLIST_H
#define PP_PLAYLIST_H

#include <stdbool.h>
#include <stddef.h>

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Monta la scheda. Ritorna false se assente o non montabile. */
bool playlist_mount(void);
void playlist_unmount(void);

/* Elenca le sottocartelle della root, in ordine alfabetico.
 * Se non ci sono sottocartelle utilizzabili ma ci sono .mp3 nella root,
 * ritorna una sola playlist con nome vuoto (= root della SD). */
int playlist_scan_playlists(char names[][PP_NAME_LEN], int max);

/* Elenca i file .mp3 di una playlist, in ordine alfabetico.
 * `playlist` vuoto significa "root della SD". */
int playlist_scan_tracks(const char *playlist, char names[][PP_NAME_LEN], int max);

/* Compone il path completo del brano ("/playlist/brano.mp3"). */
void playlist_build_path(char *out, size_t out_len,
                         const char *playlist, const char *track);

/* Nome da mostrare per una playlist (gestisce il caso "root"). */
const char *playlist_display_name(const char *playlist);

#ifdef __cplusplus
}
#endif

#endif /* PP_PLAYLIST_H */
