/*
AE_PLAYLISTS_STORE.H

(0.9.4) The playlists CREATE GAME and HOST A SERVER share (ae_playlists.h): the set held in memory, read once from
<save root>/private/playlists.txt (else today's playlist.txt, migrated), written whole after every change, with the in use
playlist exported as playlist.txt (a server's rotation, as before) so older builds and a copied rotation keep working.
A TEST RUN stages a made-up set with the DEV commands `playlists demo` and `playlists forget`: those are held in memory and
never written, the player's own files untouched.
*/

#ifndef AE_PLAYLISTS_STORE_H
#define AE_PLAYLISTS_STORE_H

#include "ae_playlists.h"

/* the set (read at the first call) */
struct ae_playlists *ae_playlists_store(void);
/* the set written (playlists.txt, then playlist.txt from the in use playlist): 1 when written; 0 when it was not (a file too big
to read is kept as it is, or a staged set is in memory) */
int ae_playlists_store_save(void);
/* a number that changes with every save and every staged set: HOST A SERVER reads the playlist again when it moved */
unsigned ae_playlists_store_revision(void);
/* playlists.txt is larger than AE_PLAYLISTS_FILE: kept as it is, edits are not saved */
int ae_playlists_store_locked(void);
/* a staged set is held (the files are not written) */
int ae_playlists_store_staged(void);
/* stage a made-up set: AE_PLAYLISTS_STAGE_DEMO (4V4 ARENA shuffle 7 games in use, FFA NIGHT random 6, SWAT TIME in turn 4),
AE_PLAYLISTS_STAGE_OLD (no playlists.txt and one old-style playlist.txt: the migration's scene) or AE_PLAYLISTS_STAGE_REAL
(the files again) */
enum { AE_PLAYLISTS_STAGE_REAL, AE_PLAYLISTS_STAGE_DEMO, AE_PLAYLISTS_STAGE_OLD };
void ae_playlists_store_stage(int kind);

/* the playlist HOST A SERVER's server runs: its settings from the in use playlist (rotation line, shuffle, map vote, vote_no_repeat);
the games, 0 when none is in use */
int ae_playlists_store_host(char *line, int size, struct ae_playlist_host *host, char *name, int name_size);

#endif
