/*
AE_GLUE_GAMETYPE_PICKER.H

CHANGE GAMETYPE's glue (ae_glue_gametype_picker.c): the model's gametypes (ae_gametype_picker.h) from the saved
gametypes the game lists (ae_setup_gametypes), their folders and names from the names table (arena_gametype_names.c:
the playlist categories; a built-in one by its variant's system default bit is STOCK, any other one not in the table
MY GAMETYPES), each one's start weapon and the panel's facts from its file. RECENT's games (CHANGE GAMETYPE's RECENT:
their gametypes), FAVOURITES and the player's own folders are saved (ae_glue_picker_stores.h), by id: AE's seeded name or a built-in's, saved:<stored name>
for the player's own. Menus' thread only. Include after cseries.h.

Log: "ae change gametype: gametypes N (pairs P), current '<stored>'" as they load.
*/

#ifndef __AE_GLUE_GAMETYPE_PICKER_H
#define __AE_GLUE_GAMETYPE_PICKER_H

#include "ae_gametype_picker.h"

/* the gametypes the game lists now, finished (ae_gtp_finish), with FAVOURITES and the player's folders, and (recent
non-zero) RECENT's gametypes; *current: the open edit's gametype (CREATE GAME's), -1 for none */
void ae_gtp_load(struct ae_gtp *gtp, short *current, int recent);
/* a gametype's id (the folders' and RECENT's), "" for none */
void ae_gtp_id(struct ae_gtp const *gtp, short gametype, char *id, int size);
/* the gametype of an id, -1 none */
short ae_gtp_find_id(struct ae_gtp const *gtp, const char *id);
/* the player's folders (the store's) on the gametypes again: FAVOURITES, own_names and each one's own bits */
void ae_gtp_folders_apply(struct ae_gtp *gtp);
/* RECENT's gametypes again (ae_recent_games_by: the gametypes of the games played, newest first, each once), those
that are here */
void ae_gtp_recent_apply(struct ae_gtp *gtp);
/* a gametype's pair put in or taken out of one of the player's folders (FAVOURITES 0), saved: 1 now in it, 0 now out,
-1 refused. Log "ae change gametype: '<id>' in|out of <folder>" */
int ae_gtp_folder_toggle(struct ae_gtp *gtp, short gametype, short folder);
/* the host started a game from CREATE GAME on a map (its list name): the edit's gametype and the map first in RECENT */
void ae_gtp_record_game(const char *map);
/* whether a gametype may be deleted: one of MY GAMETYPES (the player's own: not a seeded AE / NHE one, which the next
start seeds again, nor a built-in) with a saved file; deleted: its saved gametype and its gametype file
(playlist_profile_delete, gametype_files_profile_deleted), 1 when done. Log "ae change gametype: deleted '<stored>'" */
int ae_gtp_can_delete(struct ae_gtp const *gtp, short gametype);
int ae_gtp_delete(struct ae_gtp const *gtp, short gametype);
/* the groups of CHANGE MAP's maps PLAYS WELL ON prefers (NHE, COMMUNITY COMPETITIVE: the comp maps), AE_SETUP_MAP_GROUP_WORDS
words */
void ae_gtp_preferred_map_groups(struct ae_setup_maps const *maps, unsigned int *groups);

#endif
