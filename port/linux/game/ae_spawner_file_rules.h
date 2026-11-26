/*
AE_SPAWNER_FILE_RULES.H

port: AE weapon spawners (0.7.8). A map's
spawner lines: they live in the map's layout file (ae_layout_rules.h: spawner, channel and auto lines among its others),
and the game hands them here as one text, "spawners 1 <map> <hash>" first (ae_layout_spawner_text), read after the
layout applied and the automatic conversion ran, to re-tag, skip, split, pin or add spawners. Optional: a map without
them plays the conversion alone. The same hash, comment, limit and whole-file refusal rules as a layout: a bad spawner
line refuses the whole layout. Pure: no engine includes; port/linux/tests/ae_spawner_file_rules_test.c compiles this
alone; ae_spawners.c applies the edits in the game.

The file (version 1):

	spawners 1 <map file name, no .map> <map hash: 8 hex digits>
	format 1                            the file format version; another refuses the file
	auto on|off                         off: no automatic conversion, only the lines below make spawners (default on)
	spawner <n> [kind K] [class C] [tier T] [logic L] [respawn S] [delay S] [channel NAME] [sets A B ..] [games all|ctf|slayer|oddball|king|race ..]
	                                    re-tag the converted spawner of map item n (n as the log prints it)
	spawner skip <n>                    item n stays a plain map item, not converted
	spawner <n> split                   item n keeps its own spawner (undoes the paired-spot merge)
	spawner <n> weapon <ref>            pin one weapon: the spawner ignores the set's pool (<ref> as a set file's)
	spawner at <x> <y> <z> [facing <degrees>] kind K class C tier T [same options]
	                                    a new spawner where the map has no item
	channel <NAME> symmetrical          every spawner on the channel gets the same weapon each time

kind: rack pad crate hidden (rack = tier BASE, pad = tier POWER, crate = one spawn and never again, hidden = no plate,
no waypoint, no timer). sets: the set names a spawner exists under (casual comp btb custom); none = every set.
*/

#ifndef __AE_SPAWNER_FILE_RULES_H
#define __AE_SPAWNER_FILE_RULES_H

#include "ae_pure_types.h"
#include "ae_spawner_rules.h"

#define AE_SPAWNER_FILE_VERSION 1
#define AE_SPAWNER_FILE_MAP_SIZE 64
#define AE_SPAWNER_FILE_EDITS_MAXIMUM 256
#define AE_SPAWNER_FILE_CHANNELS_MAXIMUM 16
#define AE_SPAWNER_FILE_NAME_SIZE 16
#define AE_SPAWNER_FILE_REF_SIZE 32
#define AE_SPAWNER_FILE_RESPAWN_MAXIMUM 3600

enum ae_spawner_kind
{
	AE_SPAWNER_KIND_NONE = -1,
	AE_SPAWNER_RACK = 0,
	AE_SPAWNER_PAD,
	AE_SPAWNER_CRATE,
	AE_SPAWNER_HIDDEN,
	AE_SPAWNER_KIND_COUNT
};

enum ae_spawner_edit_kind
{
	AE_SPAWNER_EDIT_RETAG = 0,	/* spawner <n> [options] */
	AE_SPAWNER_EDIT_SKIP,	/* spawner skip <n> */
	AE_SPAWNER_EDIT_SPLIT,	/* spawner <n> split */
	AE_SPAWNER_EDIT_PIN,	/* spawner <n> weapon <ref> */
	AE_SPAWNER_EDIT_ADD	/* spawner at x y z ... */
};

/* the game types a `games` word names (bits; the layout's words) */
#define AE_SPAWNER_GAMES_CTF 1
#define AE_SPAWNER_GAMES_SLAYER 2
#define AE_SPAWNER_GAMES_ODDBALL 4
#define AE_SPAWNER_GAMES_KING 8
#define AE_SPAWNER_GAMES_RACE 16
#define AE_SPAWNER_GAMES_ALL 31

/* the sets a `sets` word names (bits by the built-in id; custom = bit 15) */
#define AE_SPAWNER_SETS_ALL 0xFFFFul

/* which options a retag / add line set (bits) */
#define AE_SPAWNER_EDIT_HAS_KIND 1
#define AE_SPAWNER_EDIT_HAS_CLASS 2
#define AE_SPAWNER_EDIT_HAS_TIER 4
#define AE_SPAWNER_EDIT_HAS_LOGIC 8
#define AE_SPAWNER_EDIT_HAS_RESPAWN 16
#define AE_SPAWNER_EDIT_HAS_DELAY 32
#define AE_SPAWNER_EDIT_HAS_CHANNEL 64
#define AE_SPAWNER_EDIT_HAS_SETS 128
#define AE_SPAWNER_EDIT_HAS_GAMES 256
#define AE_SPAWNER_EDIT_HAS_FACING 512

struct ae_spawner_edit
{
	signed char edit; /* enum ae_spawner_edit_kind */
	short n; /* the map item (-1 for an add) */
	unsigned short has; /* AE_SPAWNER_EDIT_HAS_* */
	signed char kind, cls, tier, logic;
	short respawn, delay;
	char channel[AE_SPAWNER_FILE_NAME_SIZE];
	unsigned long sets; /* bits; AE_SPAWNER_SETS_ALL when none named */
	unsigned long games; /* bits; AE_SPAWNER_GAMES_ALL when none named */
	real position[3], facing;
	char weapon[AE_SPAWNER_FILE_REF_SIZE]; /* a pin's ref */
};

struct ae_spawner_file
{
	char map[AE_SPAWNER_FILE_MAP_SIZE];
	unsigned long hash;
	unsigned char auto_convert;
	short edit_count;
	struct ae_spawner_edit edits[AE_SPAWNER_FILE_EDITS_MAXIMUM];
	short symmetrical_count;
	char symmetrical[AE_SPAWNER_FILE_CHANNELS_MAXIMUM][AE_SPAWNER_FILE_NAME_SIZE];
};

/* reads a sidecar: 0 accepted, else the number of the line refused (1 for a file-level refusal) with the reason in
`error` */
int ae_spawner_file_read(char const *text, long length, struct ae_spawner_file *out, char *error, int error_size);

/* whether the file is the loaded map's (name case aside, the hash equal) */
boolean ae_spawner_file_for_map(struct ae_spawner_file const *file, char const *map_file, unsigned long map_hash);

/* whether a channel is symmetrical in this file */
boolean ae_spawner_file_symmetrical(struct ae_spawner_file const *file, char const *channel);

/* the kind words and their reverse (-1 unknown); the tier a kind implies (BASE for a rack, POWER for a pad, ANY else) */
char const *ae_spawner_kind_word(int kind);
int ae_spawner_kind_of_word(char const *word);
int ae_spawner_kind_tier(int kind);

/* a game types word to its bit (0 unknown); a set name to its bit (0 unknown) */
unsigned long ae_spawner_games_bit(char const *word);
unsigned long ae_spawner_sets_bit(char const *word);

#endif /* __AE_SPAWNER_FILE_RULES_H */
