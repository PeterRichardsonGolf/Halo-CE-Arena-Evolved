/*
AE_GLUE_PICKER_STORES.H

The pickers' saved data (plan .superpowers/sdd/2026-10-27/menus-p2-pickers.md, section 1): the player's folders of maps
and of gametypes (ae_picker_folders.h) and RECENT's games (ae_recent_games.h), one set a PC, in the save root's private
folder (ae_private_files.h): map-folders.txt, gametype-folders.txt, recent-games.txt. Each is read the first time it
is asked for and kept for the session; a change is written at once. A file the reader refuses is kept aside as
<file>.bad and the store starts afresh. Menus' thread only.

Log: "ae pickers: <file>: N folders, M entries" / "ae pickers: <file>: none yet" / "ae pickers: <file> refused: kept
as <file>.bad, a fresh one started" as they are read, "ae pickers: <file> written" or "... could not be written",
"ae pickers: recent game '<map>' | '<gametype>'".
*/

#ifndef __AE_GLUE_PICKER_STORES_H
#define __AE_GLUE_PICKER_STORES_H

#include "ae_picker_folders.h"
#include "ae_recent_games.h"

/* a kind's folders (AE_PF_MAPS, AE_PF_GAMETYPES), read once */
struct ae_picker_folders *ae_picker_store(int kind);
/* a kind's folders written (after any change); 1 when written */
int ae_picker_store_save(int kind);
/* RECENT's games, read once */
struct ae_recent_games *ae_recent_store(void);
/* RECENT's folder folded in a picker (by: AE_RECENT_BY_MAP, AE_RECENT_BY_GAMETYPE; 0 open, the default) and the
choice written, per picker, with RECENT's games */
int ae_recent_store_folded(int by);
void ae_recent_store_set_folded(int by, int folded);
/* a game the host started from CREATE GAME (its map's list name, its gametype's id): first in RECENT, written */
void ae_recent_store_record(const char *map, const char *gametype);

#endif
