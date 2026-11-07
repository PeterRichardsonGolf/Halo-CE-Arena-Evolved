/*
AE_RECENT_GAMES.H

RECENT on CHANGE MAP and CHANGE GAMETYPE (ae_recent_games.c; plan .superpowers/sdd/2026-10-27/menus-p2-pickers.md,
section 1): the games the host started from CREATE GAME, each a map and a gametype, newest first. A game played again
moves to the top; 30 are kept. RECENT is each picker's own (owner 2026-10-28): CHANGE MAP's lists the maps played,
CHANGE GAMETYPE's the gametypes, newest first, each once, the first 8 still there; a row sets only its own half. Kept
in a text file in the save root's private folder (the glue's: ae_glue_picker_stores.c). Pure C, no engine includes
(unit test: port/linux/tests/ae_recent_games_test.c, under the sanitizers).

The ids are the folders' (ae_picker_folders.h): a map's list name, a gametype's seeded or built-in name or
saved:<stored name>. The file, newest first:

	# AE recent games
	game = bloodgulch | AE BTB CTF
	game = hotbox | AE 2V2 SLAY

A line of no known kind refuses the file whole (the glue keeps it aside as <file>.bad); a line with a bad id is
dropped.
*/

#ifndef __AE_RECENT_GAMES_H
#define __AE_RECENT_GAMES_H

enum { AE_RECENT_KEPT = 30, AE_RECENT_SHOWN = 8, AE_RECENT_ID = 64,
	AE_RECENT_TEXT = AE_RECENT_KEPT * (2 * AE_RECENT_ID + 12) + 64 };
/* a picker's RECENT: by the games' maps (CHANGE MAP) or by their gametypes (CHANGE GAMETYPE) */
enum ae_recent_by { AE_RECENT_BY_MAP, AE_RECENT_BY_GAMETYPE };

struct ae_recent_game { char map[AE_RECENT_ID], gametype[AE_RECENT_ID]; };
struct ae_recent_games
{
	short count;
	struct ae_recent_game games[AE_RECENT_KEPT];
	/* RECENT's folder folded in the maps picker / the gametype picker (0: open, the default) */
	short maps_folded, gametypes_folded;
};

void ae_recent_games_init(struct ae_recent_games *recent);
/* a file's text read (the list made empty first): 1 taken, 0 refused (the list is then empty) */
int ae_recent_games_read(struct ae_recent_games *recent, const char *text);
/* the file's text into buffer: its length, or -1 when it does not fit */
long ae_recent_games_write(struct ae_recent_games const *recent, char *buffer, long size);
/* a game started: first, its older entry dropped, the 31st gone; 0 for a bad id */
int ae_recent_games_add(struct ae_recent_games *recent, const char *map, const char *gametype);
/* a picker's RECENT (by: AE_RECENT_BY_*): its maps or its gametypes, newest first, each once (a map or gametype
played again later down the list is not shown again), up to maximum (at most AE_RECENT_SHOWN) of those that exist
(exists: the caller's, 1 when that map or gametype is there; NULL: all). Each one's newest game's index into games
(its map or gametype the row's); returns the count */
short ae_recent_games_by(struct ae_recent_games const *recent, int by, int (*exists)(const char *id, void *context),
	void *context, short *indices, short maximum);

/* RECENT's folded pickers alone, in a file of their own (recent-folds.txt): recent-games.txt keeps only its games, as
0.5.0 and older read it (they set aside a file with any other line); ae_recent_games_read reads either */
long ae_recent_folds_write(struct ae_recent_games const *recent, char *buffer, long size);

#endif
