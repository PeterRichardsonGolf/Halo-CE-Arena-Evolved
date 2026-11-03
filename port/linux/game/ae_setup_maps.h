/*
AE_SETUP_MAPS.H

The game setup spike's CHANGE MAP model (ae_setup_maps.c; notes/plans/2026-10-21-game-setup-spike.md): the maps in
folders (groups) that open in place, and a filter by AE's gametypes and NHE (LT / RT): which maps
suit a filter, each folder's count under it, the rows the list shows, and the gametypes a map plays well with. Pure C,
no engine includes (unit test: port/linux/tests/ae_setup_maps_test.c). The glue (ae_glue_setup_maps.c) fills the maps
and names the groups; a map is in any number of groups (a bit each).

A filter on modes reads the map file's own mode bits (ae_mapinfo.h's AE_MODE_*); a map whose file is not read yet, or
can't be, suits every filter (never hidden for want of facts). Ideas from the shelved M4's ae_map_catalog.c.

AE menus Phase 2 (.superpowers/sdd/2026-10-27/menus-p2-pickers.md): RECENT's games over the folders (the caller's,
by count), the player's own folders (listed with no map in them, an empty one's line under it when open), and the
search: a flat list of the maps whose name, credits or source group hold the letters, in the groups' order.
*/

#ifndef __AE_SETUP_MAPS_H
#define __AE_SETUP_MAPS_H

enum { AE_SETUP_MAPS_MAXIMUM = 2048, AE_SETUP_MAP_GROUPS = 128, AE_SETUP_MAP_GROUP_WORDS = AE_SETUP_MAP_GROUPS / 32,
	AE_SETUP_MAP_TEXT = 64, AE_SETUP_MAP_GROUP_NAME = 40, AE_SETUP_MAP_QUERY = 32 };
/* a group's flags: listed with no map in it (the player's folders); one of the player's own (its name in its own case,
X renames or deletes it) */
enum { AE_SETUP_GROUP_LISTED = 1, AE_SETUP_GROUP_OWN = 2 };
/* the list's rows that are neither a map (>= 0) nor a group (-1 - group): RECENT's header and its games (the caller's
k-th), an empty folder's line */
enum { AE_SETUP_ROW_RECENT = -1000, AE_SETUP_ROW_RECENT_GAME = -1001, AE_SETUP_ROW_EMPTY = -2000,
	AE_SETUP_ROW_RECENT_MAXIMUM = 64 };   /* (RECENT's rows: -1001 down to -1064, clear of EMPTY's) */
enum ae_setup_map_filter { AE_MF_ALL, AE_MF_FFA_SLAYER, AE_MF_TEAM_SLAYER, AE_MF_KING, AE_MF_BALL, AE_MF_CTF,
	AE_MF_NHE, AE_MAP_FILTERS };
/* why a listed map can't be picked: Steam is updating its file (an MCC Workshop map, 0.6.0) */
enum { AE_SETUP_MAP_UPDATING = 1 };
enum { AE_SETUP_FACTS_NONE, AE_SETUP_FACTS_READ, AE_SETUP_FACTS_UNREADABLE };
/* (ae_mapinfo.h's: the model needs no engine or reader to test) */
enum { AE_SETUP_MODE_CTF = 1, AE_SETUP_MODE_SLAYER = 2, AE_SETUP_MODE_ODDBALL = 4, AE_SETUP_MODE_KING = 8 };

struct ae_setup_map
{
	char file[AE_SETUP_MAP_TEXT];      /* the game's list name (a CE map's <file>@ce) */
	char id[AE_SETUP_MAP_TEXT];        /* the player's folders' and RECENT's: the list name's last part (<file>,
	                                      <file>@<family>), the same whatever folder the map is in */
	char name[AE_SETUP_MAP_TEXT];      /* shown */
	char credits[AE_SETUP_MAP_TEXT];   /* maps.txt's, "" none */
	unsigned int groups[AE_SETUP_MAP_GROUP_WORDS];   /* bit n: group n (ae_setup_map_in) */
	short source;                      /* the group it comes from (its badge, the search's group): -1 none */
	int facts, modes;                  /* AE_SETUP_FACTS_*, AE_SETUP_MODE_* */
	short players_min, players_max;    /* 0: unknown */
	char size;                         /* 's', 'm', 'l'; 0 unknown */
	int nhe;                           /* maps.txt's nhe-vehicles, or the NHE disc's own */
	short list_index;                  /* the game's list's, -1 not in it (an unavailable map) */
	int unavailable;                   /* AE_SETUP_MAP_UPDATING: listed, dimmed, never picked; 0 playable */
};
struct ae_setup_maps
{
	short count;
	struct ae_setup_map maps[AE_SETUP_MAPS_MAXIMUM];
	short group_count;
	char group_names[AE_SETUP_MAP_GROUPS][AE_SETUP_MAP_GROUP_NAME];
	unsigned char group_flags[AE_SETUP_MAP_GROUPS];   /* AE_SETUP_GROUP_* */
	short group_folder[AE_SETUP_MAP_GROUPS];          /* the glue's: its folder in the player's folders, -1 none */
	short group_parent[AE_SETUP_MAP_GROUPS];          /* the folder it is in (0.6.0, WORKSHOP's collections): that
	                                                     group + 1, 0 none (zeroed: no parent) */
	short recent_folded;                              /* the screen's: RECENT shows its header only (0 open) */
};

static inline int ae_setup_map_in(struct ae_setup_map const *map, int group)
{
	return group >= 0 && group < AE_SETUP_MAP_GROUPS && (map->groups[group / 32] & (1u << (group % 32))) != 0;
}
static inline void ae_setup_map_join(struct ae_setup_map *map, int group)
{
	if (group >= 0 && group < AE_SETUP_MAP_GROUPS)
		map->groups[group / 32] |= 1u << (group % 32);
}
static inline void ae_setup_map_leave(struct ae_setup_map *map, int group)
{
	if (group >= 0 && group < AE_SETUP_MAP_GROUPS)
		map->groups[group / 32] &= ~(1u << (group % 32));
}

/* "ALL", "AE FFA SLAY", "AE TEAM SLAY", "AE KING", "AE FFA BALL", "AE CTF", "NHE"; "" out of range */
const char *ae_setup_map_filter_name(int filter);
int ae_setup_map_suits(struct ae_setup_map const *map, int filter);
/* the maps that suit a filter, each once */
short ae_setup_map_suiting(struct ae_setup_maps const *maps, int filter);
/* a group's maps that suit a filter */
short ae_setup_map_group_count(struct ae_setup_maps const *maps, int group, int filter);

/* the list: with recent games, RECENT's header and a row each (AE_SETUP_ROW_RECENT_GAME - k); then a row per group with
a map that suits the filter, or listed with none (an entry -1 - group), and after the open group's row its maps that
suit (entries >= 0: indices into maps), in the maps' order, or, a listed group with no map at all, its empty line.
A group in a folder (group_parent) is listed right under its parent, and only while the parent is open (open_group
the parent or one of its groups); an open parent lists its groups, not its own maps (the glue joins each child's maps
to the parent too, for its count). Returns the entries written */
short ae_setup_map_rows(struct ae_setup_maps const *maps, int filter, int open_group, short recent, short *rows,
	short maximum);
static inline int ae_setup_map_row_is_group(short row) { return row < 0 && row >= -AE_SETUP_MAP_GROUPS; }
static inline int ae_setup_map_row_group(short row) { return -1 - row; }
static inline int ae_setup_map_row_is_recent(short row)
{
	return row <= AE_SETUP_ROW_RECENT_GAME && row > AE_SETUP_ROW_RECENT_GAME - AE_SETUP_ROW_RECENT_MAXIMUM;
}
static inline short ae_setup_map_row_recent(short row) { return (short)(AE_SETUP_ROW_RECENT_GAME - row); }
/* where letters are in a text (their case aside): the first one's index, -1 none (or no letters) */
int ae_setup_map_match(const char *text, const char *query);
/* whether a map suits a filter and holds the letters in its name, credits or source group's name */
int ae_setup_map_matches(struct ae_setup_maps const *maps, short index, int filter, const char *query);
/* the maps that suit a filter and the letters (NULL or "": every one that suits) */
short ae_setup_map_matching(struct ae_setup_maps const *maps, int filter, const char *query);
/* the search's list: the maps that match, flat, by their source group in the groups' order, then the maps' order */
short ae_setup_map_search(struct ae_setup_maps const *maps, int filter, const char *query, short *rows, short maximum);
/* LT / RT: the next (direction > 0) or previous group row from index (wrapping), or index when there is none */
short ae_setup_map_group_step(short const *rows, short count, short index, short direction);
/* the folder a group is in, -1 none (or out of range) */
int ae_setup_map_group_parent(struct ae_setup_maps const *maps, int group);
/* whether a group shows open: it is open_group, or open_group is in it */
int ae_setup_map_group_open(struct ae_setup_maps const *maps, int open_group, int group);
/* the row of a group, -1 when not listed */
short ae_setup_map_group_row(short const *rows, short count, int group);

/* the gametypes (filters past ALL) a map is known to play well with, in the filters' order: only from read facts
(none while not read); NHE when it is an NHE map. Returns the count written */
short ae_setup_map_plays_well(struct ae_setup_map const *map, int *filters, short maximum);

#endif
