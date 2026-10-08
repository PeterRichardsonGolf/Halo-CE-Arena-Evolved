/*
ARENA_GAMETYPES.H

port: Halo CE: Arena Evolved, the modern-gameplay gametypes saved as
ordinary custom gametypes (arena_gametypes.c).
*/

#ifndef __ARENA_GAMETYPES_H
#define __ARENA_GAMETYPES_H
#pragma once

/* ---------- headers */

#include "cseries/cseries.h"

/* ---------- constants */

/* the sets of seeded gametypes (arena_gametype_names.c's table), in the
lists' order; the player's own and the stock ones are in none */
enum arena_gametype_set
{
	_arena_gametype_set_none,       /* not one of ours: stock CE, the player's own */
	_arena_gametype_set_ae,         /* AE casual (unmarked in names) */
	_arena_gametype_set_ae_comp,
	_arena_gametype_set_nhe,
	_arena_gametype_set_training,   /* AE TRAINING / PRACTICE: outside the sets */
	NUMBER_OF_ARENA_GAMETYPE_SETS
};

/* ---------- structures */

struct arena_gametype_info
{
	short set;                      /* enum arena_gametype_set */
	short order;                    /* place within its set (the table's order) */
	boolean team;                   /* TEAM, else FFA */
	boolean special;                /* AE casual's special team modes (SNIPERS, SWAT, ...) */
	short game_engine;              /* the mode: game_engine.h's engine (slayer, ctf, oddball, king, race) */
	char const *display_name;       /* "TEAM AE COMP SLAYER": ASCII, upper case, at most 31 characters */
	char const *description;        /* one or two sentences for the panel, or NULL */
};

struct game_variant;

/* ---------- prototypes/ARENA_GAMETYPE_NAMES.C */

/* TRUE, and *info, when stored_name (game_variant's 11-character name) is a
seeded gametype, by its name (letters' case aside); an old name of one
(an alias) is TRUE too, with its successor's set, order, team and engine,
its own stored name as display_name, no description, not special */
boolean arena_gametype_info(
	wchar_t const *stored_name,
	struct arena_gametype_info *info);
/* the name to show for any gametype: the table's display name, else the
stored name (an alias's too); size in characters, with the NUL */
void arena_gametype_display_name(
	wchar_t const *stored_name,
	wchar_t *display_name,
	short size);
/* whether stored_name is one of the table's gametypes on AE's rules (the
AE, AE COMP and TRAINING sets; not an old name, not Halo 1: NHE's set):
the one marker of AE's gametypes (game_engine.c's weapon set with a custom
loadout); a player's own gametype only by taking a seed's name */
boolean arena_gametype_has_ae_rules(
	wchar_t const *stored_name);
/* "AE", "AE COMP", "NHE", "TRAINING"; "" for none */
char const *arena_gametype_set_name(
	short set);
/* the table's gametypes (not the aliases), in its order */
short arena_gametype_catalog_count(
	void);
char const *arena_gametype_catalog_stored_name(
	short index);

/* ---------- prototypes/ARENA_GAMETYPES.C */

/* a player's own gametype's longer display name (an 'AEDN' block in its
file; a seeded gametype never has one). TRUE, and the name (size characters
with the NUL), when it has one */
boolean arena_gametype_own_display_name(
	long profile_index,
	wchar_t *display_name,
	short size);
/* the same by stored name (the network game's, a lobby's, the scoreboard's):
the saved gametype of that name, only when its file holds exactly the variant in play (a joiner's
own gametype of the same name but other rules is not the host's); remembered until the variant or a
gametype file changes */
boolean arena_gametype_own_display_name_for_stored_name(
	wchar_t const *stored_name,
	struct game_variant const *variant,
	wchar_t *display_name,
	short size);
/* debug.set_display_name: see arena_gametypes.c; once at the start */
void arena_gametypes_debug_set_display_name(
	void);
/* sets it (empty or NULL: takes it away); FALSE for a seeded gametype's
profile or a failed write. For the AE menus (MY GAME TYPES) */
boolean arena_gametype_set_own_display_name(
	long profile_index,
	wchar_t const *display_name);

/* the stored name a seeded gametype of this old name has now (its
migrations' renames followed: "AE PRO TS" -> "AE COMP TS"), else name */
char const *arena_gametypes_migrated_name(
	char const *name);

/* TRUE while that seeded gametype's file is still exactly as seeded
(FALSE: edited, not on disk, or not one seeded by this build) */
boolean arena_gametype_as_seeded(
	wchar_t const *stored_name);

/* TRUE when it wrote any */
boolean arena_gametypes_seed(
	void);
/* the custom gametypes among count gametypes in the lists' order */
void arena_gametypes_sort(
	word count,
	long *indices);

#endif // __ARENA_GAMETYPES_H
