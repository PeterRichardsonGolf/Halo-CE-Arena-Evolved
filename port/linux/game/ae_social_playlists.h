/*
AE_SOCIAL_PLAYLISTS.H

The social playlists of the offline career (0.9.3): port/assets/network/social.txt read, which of a playlist's maps this PC has,
and the pick of the next game. Pure code (no engine headers): port/linux/tests/ae_social_playlists_test.c runs it. A map's
presence is asked of a callback (the game's own check, ae_stage_map_file_present), so a test or a DEV command can hide maps.

The file is AE's playlist text (ae_playlist.h) in sections. "# social: <id>" starts a playlist, its "# key: value" lines follow
(label, about, teams "<n> x <size>" or players "<n>", party, kind social | ranked | comp, starts "br 1, evo 1"), then entries "map:file=<GAMETYPE>  # weight n". A map
named <file>@smp is a converted pack map: never shipped, so an entry whose map is not here is skipped. An entry with the word
"fallback" in its comment is a STOCK FALLBACK. Other comment lines are skipped.

Which entries play: those whose map is present. With AE_SOCIAL_PACK_MINIMUM or more present maps (distinct) among the entries that
are not fallbacks, only those entries; with fewer, the fallbacks too. If that leaves nothing, any present entry.
The pick is by weight and never the map just played while two or more maps are on offer.

STARTS (0.9.3). "# starts: br 1, evo 1" (words br, evo, magnum, protomag, each with an optional weight 1 .. 5) makes each game of the playlist
start with one of those loadouts, picked by weight from the game's seed, where the entry's game type has it (the game's own
start-variant table: arena_gametypes.c). A file with no starts line plays each entry's game type as it is. A comp playlist (kind
comp) with no starts line mixes evo and magnum.

SHOWN (rc2). "# shown: with maps" lists the playlist (AE TOURNAMENT's rows) only while one of its maps is on this PC: DIGSITE's
maps are a Steam Workshop pack (<name>@ws<item id>), so a PC without the pack has no such row. Every other playlist is listed always.
*/

#ifndef __AE_SOCIAL_PLAYLISTS_H
#define __AE_SOCIAL_PLAYLISTS_H

enum
{
	AE_SOCIAL_PLAYLISTS = 16,
	AE_SOCIAL_ENTRIES = 256,
	AE_SOCIAL_ID = 24,
	AE_SOCIAL_LABEL = 25,
	AE_SOCIAL_ABOUT = 97,
	AE_SOCIAL_MAP = 64,
	AE_SOCIAL_GAMETYPE = 32,
	/* the present pack maps a playlist needs before its stock fallbacks drop out (the owner's rule, open question 7) */
	AE_SOCIAL_PACK_MINIMUM = 3,
	AE_SOCIAL_WEIGHT_MAX = 5,
	AE_SOCIAL_KIND_SOCIAL = 0,
	AE_SOCIAL_KIND_RANKED = 1,
	AE_SOCIAL_KIND_COMP = 2,
	/* a game's start loadout (the numbers are arena_gametypes.h's _arena_gametype_kind_*; 0 is the entry's own game type) */
	AE_SOCIAL_START_NONE = 0,
	AE_SOCIAL_START_BR = 1,
	AE_SOCIAL_START_EVO = 2,
	AE_SOCIAL_START_MAGNUM = 3,
	/* (rc2) the pistol start with the PROTOMAG (the Digsite pack's starter pistol, weapon cache id 25) in hand: a game type has it
	where it has the pistol start; it is no loadout kind of arena_gametypes.h (the game puts the weapon over the pistol start) */
	AE_SOCIAL_START_PROTOMAG = 4,
	/* the weapon cache id of the PROTOMAG (tools/weapon_cache/registry.json: starter_pistol, 25) */
	AE_SOCIAL_PROTOMAG_ID = 25,
	AE_SOCIAL_STARTS = 4
};

struct ae_social_entry
{
	char map[AE_SOCIAL_MAP];            /* "warren@smp", "prisoner" */
	char gametype[AE_SOCIAL_GAMETYPE];  /* "AE TEAM SLY" (the "file=" of the file taken off) */
	short weight;                       /* 1 .. 5 */
	short fallback;                     /* a stock fallback */
	short pack;                         /* the map is a pack map (<file>@smp) */
};

struct ae_social_playlist
{
	char id[AE_SOCIAL_ID];
	char label[AE_SOCIAL_LABEL];
	char about[AE_SOCIAL_ABOUT];
	int teams;                          /* 0: a free for all of size players; else the number of teams */
	int size;                           /* players on a team, or in the free for all */
	int party;                          /* the biggest party of local players (1 .. size) */
	int kind;                           /* AE_SOCIAL_KIND_* */
	int starts;                         /* the start loadouts a game mixes (0: every game the entry's own) */
	int starts_given;                   /* a starts line was there ("none" too): a comp playlist's default mix is not added */
	int weapons_own;                    /* "# weapons: map": the games play the maps' own weapons, AE's are not forced on */
	int hide_empty;                     /* "# shown: with maps": the playlist is listed only when one of its maps is here */
	short start_kind[AE_SOCIAL_STARTS]; /* AE_SOCIAL_START_BR .. PROTOMAG */
	short start_weight[AE_SOCIAL_STARTS]; /* 1 .. 5 */
	int count;
	struct ae_social_entry entry[AE_SOCIAL_ENTRIES];
};

/* the playlists of a file's text (CRLF taken); how many. A playlist with no id, no shape or no entry is dropped; an entry
   with an empty map or gametype is skipped; weights outside 1 .. 5 are taken to the nearest (none: 2) */
int ae_social_playlists_parse(char const *text, struct ae_social_playlist *out, int max);
/* the playlist with this id (any case), -1 for none */
int ae_social_playlist_find(struct ae_social_playlist const *lists, int count, char const *id);
/* the players of a playlist's game */
int ae_social_playlist_seats(struct ae_social_playlist const *playlist);

/* whether a map is on this PC (1) or not (0) */
typedef int (*ae_social_present_fn)(void *context, char const *map);

/* whether the playlist is listed: always, unless it has the "# shown: with maps" line (DIGSITE: the Steam Workshop pack's maps)
   and none of its maps is here */
int ae_social_playlist_shown(struct ae_social_playlist const *playlist, ae_social_present_fn present, void *context);

struct ae_social_counts
{
	int pack_total, pack_present;       /* distinct pack maps the playlist names, and the ones here */
	int stock_total, stock_present;     /* distinct stock maps (fallbacks included), and the ones here */
	int workshop;                       /* the playlist's maps are all Steam Workshop maps (<name>@ws<item id>) */
	int stock_in_use;                   /* the fallbacks play (fewer than AE_SOCIAL_PACK_MINIMUM pack maps are here) */
};

void ae_social_playlist_counts(struct ae_social_playlist const *playlist, ae_social_present_fn present, void *context,
	struct ae_social_counts *counts);
/* the entries that can play now (indexes into playlist->entry, up to AE_SOCIAL_ENTRIES); the count */
int ae_social_playlist_candidates(struct ae_social_playlist const *playlist, ae_social_present_fn present, void *context,
	int *indexes);
/* the next game's entry: by weight among the candidates, not the map of entry last (-1 for none) while another map is on
   offer; random is any number. -1 when nothing can play */
int ae_social_playlist_pick(struct ae_social_playlist const *playlist, ae_social_present_fn present, void *context, int last,
	unsigned long random);

/* the start loadout of a game: by weight among the playlist's starts whose bit (1 << AE_SOCIAL_START_*) is in supported (the game
   type the entry names has that start), by random (the plan's seed: the same seed, the same start). AE_SOCIAL_START_NONE when the
   playlist mixes none or none is supported */
int ae_social_playlist_start_pick(struct ae_social_playlist const *playlist, unsigned supported, unsigned long random);
/* the start's words ("BATTLE RIFLE", "EVO RIFLE", "PISTOL": the CE magnum), "" for none */
char const *ae_social_start_label(int start);

#endif
