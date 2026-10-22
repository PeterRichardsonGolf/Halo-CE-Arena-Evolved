/*
AE_GLUE_GAMETYPE_PICKER.H

CHANGE GAMETYPE's glue (ae_glue_gametype_picker.c): the model's gametypes (ae_gametype_picker.h) from the saved
gametypes the game lists (ae_setup_gametypes), their folders and names from the names table (arena_gametype_names.c:
the playlist categories; a built-in one by its variant's system default bit is STOCK, any other one not in the table
MY GAMETYPES), each one's start weapon and the panel's facts from its file. RECENT and FAVOURITES are this session's
(by stored name: not saved yet). Menus' thread only. Include after cseries.h.

Log: "ae change gametype: gametypes N (pairs P), current '<stored>'" as they load.
*/

#ifndef __AE_GLUE_GAMETYPE_PICKER_H
#define __AE_GLUE_GAMETYPE_PICKER_H

#include "ae_gametype_picker.h"

/* the gametypes the game lists now, finished (ae_gtp_finish), with this session's RECENT and FAVOURITES; *current:
the open edit's gametype (CREATE GAME's), -1 for none */
void ae_gtp_load(struct ae_gtp *gtp, short *current);
/* a gametype picked: first in this session's RECENT */
void ae_gtp_remember_pick(struct ae_gtp *gtp, short gametype);
/* FAVOURITE toggled on a gametype's pair, kept for the session */
void ae_gtp_remember_favourite(struct ae_gtp *gtp, short gametype);
/* the groups of CHANGE MAP's maps PLAYS WELL ON prefers (its COMP folder) */
unsigned int ae_gtp_preferred_map_groups(struct ae_setup_maps const *maps);

#endif
