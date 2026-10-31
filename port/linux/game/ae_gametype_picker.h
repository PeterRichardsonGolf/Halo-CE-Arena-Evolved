/*
AE_GAMETYPE_PICKER.H

CHANGE GAMETYPE's model (ae_gametype_picker.c; the v2 decisions of 2026-10-22): the saved gametypes in folders that open in place, as CHANGE
MAP's maps, filtered by mode (LB / RB) and searched by name (X). Pure C, no engine includes (unit test:
port/linux/tests/ae_gametype_picker_test.c). The glue (ae_glue_gametype_picker.c) fills the gametypes from the saved
gametype files and the names table (arena_gametype_names.c).

- Folders: RECENT (the gametypes of the games last played, each once: the glue's), always open; FAVOURITES and the player's own
  folders (A-Z, listed with nothing in them: an empty one's line under it when open); then MY GAMETYPES, AE FFA,
  AE 2V2, AE 4V4, AE BTB, AE OBJECTIVE, AE ACTION SACK, AE PRACTICE, NHE, OPENCE, STOCK; one open at a time. One of
  those with no gametype under the filter and the search hides.
- A battle rifle PAIR (a gametype and its "BR" version: the same display name with the BR word) is ONE row; the row
  shows one of the two (the other the row's value, toggled left / right), its label the name without the BR word.
- A row's label drops TEAM / FFA from the name unless two rows would then read the same.
- Filters by the engine's mode: ALL, SLAYER, CTF, KING, ODDBALL (juggernaut, reverse tag, accumulation and stalker
  run on the oddball engine), RACE, BOMB (Bomb and Grifball, in the builds with Bomb).
- Search: a flat list of the rows whose label or full name holds the query (letters' case aside), each with its
  folder; the filters' counts follow it.
*/

#ifndef __AE_GAMETYPE_PICKER_H
#define __AE_GAMETYPE_PICKER_H

#include "ae_setup_maps.h"

enum
{
	AE_GTP_MAXIMUM = 256, AE_GTP_NAME = 48, AE_GTP_STORED = 16, AE_GTP_START = 32, AE_GTP_FACT = 48,
	AE_GTP_RECENT = 8, AE_GTP_QUERY = 32,
	/* the player's own folders (past FAVOURITES), their names with their end */
	AE_GTP_OWN = 63, AE_GTP_OWN_NAME = 33
};
enum ae_gtp_folder
{
	AE_GTP_FOLDER_RECENT, AE_GTP_FOLDER_FAVOURITES, AE_GTP_FOLDER_MINE, AE_GTP_FOLDER_FFA, AE_GTP_FOLDER_2V2,
	AE_GTP_FOLDER_4V4, AE_GTP_FOLDER_BTB, AE_GTP_FOLDER_OBJECTIVE, AE_GTP_FOLDER_ACTION_SACK, AE_GTP_FOLDER_PRACTICE,
	AE_GTP_FOLDER_NHE,
	/* (OpenCE's built-in gametypes: SWAT, Infection, Gun Game, Hardcore; game_modes.c) */
	AE_GTP_FOLDER_OPENCE,
	AE_GTP_FOLDER_STOCK, AE_GTP_FOLDERS,
	/* (the folders a gametype lives in: the rest are views of them) */
	AE_GTP_FOLDER_FIRST_HOME = AE_GTP_FOLDER_MINE,
	/* the player's own folders: AE_GTP_FOLDER_OWN + their place (A-Z) */
	AE_GTP_FOLDER_OWN = AE_GTP_FOLDERS
};
/* a row's gametype that is an empty folder's line */
enum { AE_GTP_ROW_EMPTY = -2 };
enum ae_gtp_filter { AE_GTF_ALL, AE_GTF_SLAYER, AE_GTF_CTF, AE_GTF_KING, AE_GTF_ODDBALL, AE_GTF_RACE, AE_GTF_BOMB,
	AE_GTP_FILTERS };

/* a gametype's start weapon as the model names it (game_engine.h's _loadout_weapon_* order) */
enum ae_gtp_weapon { AE_GTW_NONE, AE_GTW_RANDOM, AE_GTW_ASSAULT_RIFLE, AE_GTW_PISTOL, AE_GTW_SHOTGUN, AE_GTW_SNIPER,
	AE_GTW_ROCKET, AE_GTW_PLASMA_PISTOL, AE_GTW_PLASMA_RIFLE, AE_GTW_NEEDLER, AE_GTP_WEAPONS };

struct ae_gtp_gametype
{
	char stored[AE_GTP_STORED];       /* the stored name (11 characters): what older machines list */
	char name[AE_GTP_NAME];           /* the full name, upper case ("TEAM AE BR 2V2 CTF") */
	char row[AE_GTP_NAME];            /* the row's label (ae_gtp_finish) */
	char start[AE_GTP_START];         /* "PISTOL START", "BR START" ... (ae_gtp_start_text) */
	short folder, order;              /* its home folder (MY GAMETYPES .. STOCK), its place in it */
	int engine;                       /* AE_SETUP_ENGINE_* */
	int ae;                           /* on AE's rules: its pair's arrows */
	int team;                         /* players a side: 0 free for all, 2, 4, 8 */
	short twin;                       /* the other of its battle rifle pair, -1 none (ae_gtp_finish) */
	int br;                           /* the BR one of its pair */
	short evo_twin;                   /* (0.7.0) the base's EVO one, or the EVO one's base, -1 none (ae_gtp_finish) */
	int evo;                          /* the EVO one of its group ("AE EVO ..."): the START switch's third */
	int favourite;                    /* (the pair's: kept on the base) */
	unsigned long long own;           /* bit k: in the player's folder k (the pair's: kept on the base) */
	long profile;                     /* the glue's (the saved file) */
	/* the panel's facts (the glue's words) */
	const char *description;          /* NULL: none */
	char score[AE_GTP_FACT], time[AE_GTP_FACT], weapons[AE_GTP_FACT], shields[AE_GTP_FACT], sensor[AE_GTP_FACT],
		respawn[AE_GTP_FACT];
};

struct ae_gtp
{
	short count;
	struct ae_gtp_gametype gametypes[AE_GTP_MAXIMUM];
	/* a pair's row shows this one of the two (by the base's index: the base, or its twin) */
	short shown[AE_GTP_MAXIMUM];
	/* RECENT: the gametypes played, newest first, each once: the one used (the one of its pair) */
	short recent[AE_GTP_RECENT], recent_count;
	/* the player's own folders, A-Z */
	short own_count;
	char own_names[AE_GTP_OWN][AE_GTP_OWN_NAME];
	/* (ae_gtp_finish) the gametypes by folder, then place */
	short sorted[AE_GTP_MAXIMUM];
	/* the screen's: RECENT shows its header only (0 open) */
	short recent_folded;
};

/* a row of the list: a folder's header (gametype -1), an empty folder's line (AE_GTP_ROW_EMPTY) or a gametype in a
folder (in RECENT, its place in RECENT; elsewhere a pair's base) */
struct ae_gtp_row { short folder, gametype; };

/* "RECENT", "FAVOURITES", "MY GAMETYPES", "AE FFA" ... "STOCK"; "" out of range */
const char *ae_gtp_folder_name(int folder);
/* the same, and a folder of the player's own its name */
const char *ae_gtp_folder_label(struct ae_gtp const *gtp, int folder);
static inline int ae_gtp_folder_is_own(int folder)
{
	return folder >= AE_GTP_FOLDER_OWN && folder < AE_GTP_FOLDER_OWN + AE_GTP_OWN;
}
/* "ALL", "SLAYER", "CTF", "KING", "ODDBALL", "RACE", "BOMB"; "" out of range */
const char *ae_gtp_filter_name(int filter);
/* the filter of an engine (AE_SETUP_ENGINE_*), ALL for none */
int ae_gtp_filter_of_engine(int engine);
/* the start weapon's words: a pack's short name in the PRIMARY slot ("BR", "SMG": pack, not NULL) first, else a custom
loadout's primary, else the weapon set's (game_engine.c's enum game_engine_weapons) assault rifle; "PISTOL START" */
void ae_gtp_start_text(const char *pack, int custom, int primary, int weapon_set, char *text, int size);
/* what a weapon set makes of a stock weapon (game_engine_remap_weapon's table, for the assault rifle and the pistol);
other weapons as they are */
int ae_gtp_weapon_in_set(int weapon, int weapon_set);
/* "ASSAULT RIFLE", ...; "" out of range */
const char *ae_gtp_weapon_name(int weapon);

/* after the gametypes are filled: the pairs (twin, br; the shown one the base) and the rows' labels */
void ae_gtp_finish(struct ae_gtp *gtp);
/* a pair's base (the one rows keep): the gametype itself, or its twin when it is the BR one */
short ae_gtp_base(struct ae_gtp const *gtp, short gametype);
/* the gametype a row stands for now: RECENT's own, else its pair's shown one; -1 for a header or a line */
short ae_gtp_row_gametype(struct ae_gtp const *gtp, struct ae_gtp_row row);
/* left / right on a row: the START switch, its group's next (step 1) or previous (-1) one shown, PISTOL > BR > EVO,
wrapping, the missing ones skipped. 0 when it has no twin, or on RECENT (the one played: its own) */
int ae_gtp_step(struct ae_gtp *gtp, struct ae_gtp_row row, int step);
/* ae_gtp_step(gtp, row, 1) */
int ae_gtp_toggle(struct ae_gtp *gtp, struct ae_gtp_row row);
/* whether a gametype's row has the arrows: on AE's rules with a twin */
int ae_gtp_has_arrows(struct ae_gtp const *gtp, short gametype);
/* the START switch's pips: its group's size (1-3) and the place of the gametype in it (0 first); 0 for none */
int ae_gtp_start_place(struct ae_gtp const *gtp, short gametype, int *place);

/* whether a gametype suits a filter and a query (NULL or "": any) */
int ae_gtp_suits(struct ae_gtp const *gtp, short gametype, int filter, const char *query);
/* the rows (pairs once) of the home folders that suit */
short ae_gtp_count(struct ae_gtp const *gtp, int filter, const char *query);
/* a folder's rows that suit (RECENT's and FAVOURITES' too) */
short ae_gtp_folder_count(struct ae_gtp const *gtp, int folder, int filter, const char *query);
/* the list: without a query, RECENT's header and rows (when it has games), FAVOURITES' and the player's folders'
headers, then each home folder's header (one with nothing that suits left out), and the open one's rows (an empty
folder of the player's: its line); with a query, the rows that suit, flat, in folder order. Returns the rows written */
short ae_gtp_rows(struct ae_gtp const *gtp, int filter, const char *query, int open_folder, struct ae_gtp_row *rows,
	short maximum);
/* LT / RT: the next (direction > 0) or previous folder's header row (RECENT's aside) from index (wrapping), -1 when
none */
short ae_gtp_folder_step(struct ae_gtp_row const *rows, short count, short index, short direction);
/* the row of a folder's header, -1 when not listed */
short ae_gtp_header_row(struct ae_gtp_row const *rows, short count, int folder);
/* where a query is in a text (letters' case aside): its first character's index, -1 none */
int ae_gtp_match(const char *text, const char *query);

/* a gametype of RECENT's put last (the glue's, newest first); one already in RECENT is not put again */
void ae_gtp_recent_put(struct ae_gtp *gtp, short gametype);
/* FAVOURITE: toggled on the pair */
void ae_gtp_favourite(struct ae_gtp *gtp, short gametype);

/* PLAYS WELL ON: up to maximum maps, by index, that have the engine's mode in their read facts and suit a side's
players (team: 0 free for all, 2 small, 4 medium, 8 large), maps in prefer_groups (AE_SETUP_MAP_GROUP_WORDS words of
group bits, NULL none) first; returns the count. *have gets how many read maps have the mode, *read how many are read */
short ae_gtp_plays_well(struct ae_setup_maps const *maps, int engine, int team, unsigned int const *prefer_groups,
	short *indices, short maximum, short *have, short *read);

#endif
