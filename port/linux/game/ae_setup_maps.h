/*
AE_SETUP_MAPS.H

The game setup spike's CHANGE MAP model (ae_setup_maps.c; notes/plans/2026-10-21-game-setup-spike.md): the maps in
folders (groups) that open in place, Gravemind's way, and a filter by AE's gametypes and NHE (LT / RT): which maps
suit a filter, each folder's count under it, the rows the list shows, and the gametypes a map plays well with. Pure C,
no engine includes (unit test: port/linux/tests/ae_setup_maps_test.c). The glue (ae_glue_setup_maps.c) fills the maps
and names the groups; a map is in any number of groups (a bit each).

A filter on modes reads the map file's own mode bits (ae_mapinfo.h's AE_MODE_*); a map whose file is not read yet, or
can't be, suits every filter (never hidden for want of facts). Ideas from the shelved M4's ae_map_catalog.c.
*/

#ifndef __AE_SETUP_MAPS_H
#define __AE_SETUP_MAPS_H

enum { AE_SETUP_MAPS_MAXIMUM = 512, AE_SETUP_MAP_GROUPS = 32, AE_SETUP_MAP_TEXT = 64, AE_SETUP_MAP_GROUP_NAME = 32 };
enum ae_setup_map_filter { AE_MF_ALL, AE_MF_FFA_SLAYER, AE_MF_TEAM_SLAYER, AE_MF_KING, AE_MF_BALL, AE_MF_CTF,
	AE_MF_NHE, AE_MAP_FILTERS };
enum { AE_SETUP_FACTS_NONE, AE_SETUP_FACTS_READ, AE_SETUP_FACTS_UNREADABLE };
/* (ae_mapinfo.h's: the model needs no engine or reader to test) */
enum { AE_SETUP_MODE_CTF = 1, AE_SETUP_MODE_SLAYER = 2, AE_SETUP_MODE_ODDBALL = 4, AE_SETUP_MODE_KING = 8 };

struct ae_setup_map
{
	char file[AE_SETUP_MAP_TEXT];      /* the game's list name (a CE map's <file>@ce) */
	char name[AE_SETUP_MAP_TEXT];      /* shown */
	char credits[AE_SETUP_MAP_TEXT];   /* maps.txt's, "" none */
	unsigned int groups;               /* bit n: group n */
	int facts, modes;                  /* AE_SETUP_FACTS_*, AE_SETUP_MODE_* */
	short players_min, players_max;    /* 0: unknown */
	char size;                         /* 's', 'm', 'l'; 0 unknown */
	int nhe;                           /* maps.txt's nhe-vehicles, or the NHE disc's own */
	int favourite;
	short list_index;                  /* the game's list's */
};
struct ae_setup_maps
{
	short count;
	struct ae_setup_map maps[AE_SETUP_MAPS_MAXIMUM];
	short group_count;
	char group_names[AE_SETUP_MAP_GROUPS][AE_SETUP_MAP_GROUP_NAME];
};

/* "ALL", "AE FFA SLAY", "AE TEAM SLAY", "AE KING", "AE FFA BALL", "AE CTF", "NHE"; "" out of range */
const char *ae_setup_map_filter_name(int filter);
int ae_setup_map_suits(struct ae_setup_map const *map, int filter);
/* the maps that suit a filter, each once */
short ae_setup_map_suiting(struct ae_setup_maps const *maps, int filter);
/* a group's maps that suit a filter */
short ae_setup_map_group_count(struct ae_setup_maps const *maps, int group, int filter);

/* the list: a row per group with a map that suits the filter (an entry < 0: -1 - group), and after the open group's
row its maps that suit (entries >= 0: indices into maps), in the maps' order. Returns the entries written */
short ae_setup_map_rows(struct ae_setup_maps const *maps, int filter, int open_group, short *rows, short maximum);
static inline int ae_setup_map_row_is_group(short row) { return row < 0; }
static inline int ae_setup_map_row_group(short row) { return -1 - row; }
/* LB / RB: the next (direction > 0) or previous group row from index (wrapping), or index when there is none */
short ae_setup_map_group_step(short const *rows, short count, short index, short direction);
/* the row of a group, -1 when not listed */
short ae_setup_map_group_row(short const *rows, short count, int group);

/* the gametypes (filters past ALL) a map is known to play well with, in the filters' order: only from read facts
(none while not read); NHE when it is an NHE map. Returns the count written */
short ae_setup_map_plays_well(struct ae_setup_map const *map, int *filters, short maximum);

#endif
