/*
AE_LAYOUT_RULES.H

AE map layout files (queue 113, notes/specs/2026-10-15-map-layouts.md): a small text file per map, shipped in the map
pack beside the map (maps/ce/layouts/<map file name>.txt, never in the repo), that changes what the map places (its
netgame items, its vehicles) without touching the map file. This half is pure (no engine includes): the file read into
a struct ae_layout, every line checked; port/linux/game/ae_layout.c applies it to the loaded scenario.

The file (version 1):

	layout 1 <map file name> <map hash: 8 hex digits, the map file's hash prefix (map_file_hash.h)>
	# a comment (also after a line's words)
	clear items                       drop every netgame item of the map (then add the wanted ones)
	remove item <n>                   drop the map's item n (its index as the log lists it)
	item <what> at <x> <y> <z> [facing <degrees>] [respawn <seconds>] [team red|blue|neutral]
	remove vehicle <type> [near <x> <y> <z> <radius>]

<what>: `like <n>` (the map's own item n's collection: works on protected maps, whose tag names are scrambled), an
alias (rockets, sniper, shotgun, pistol, overshield, camo: found by what the item is, not by its name), or a tag path
of an item collection in double quotes (unprotected maps only). <type>: turret, or the index of the map's vehicle palette entry.

A line the reader does not understand refuses the whole file (a half-applied layout is worse than none); a weapon the
map lacks is the applying half's business (that one item is skipped and logged: owner 2026-10-16).
*/

#ifndef __AE_LAYOUT_RULES_H
#define __AE_LAYOUT_RULES_H

#include "ae_pure_types.h"

#define AE_LAYOUT_VERSION 1
#define AE_LAYOUT_MAXIMUM_FILE_SIZE 65536
#define AE_LAYOUT_MAXIMUM_LINES 512
#define AE_LAYOUT_MAXIMUM_LINE 200
#define AE_LAYOUT_MAXIMUM_ITEMS 256
#define AE_LAYOUT_MAXIMUM_REMOVED 256
#define AE_LAYOUT_MAXIMUM_VEHICLE_RULES 32
#define AE_LAYOUT_MAP_FILE_SIZE 64
#define AE_LAYOUT_TAG_SIZE 128

enum ae_layout_what
{
	_ae_layout_like,   /* the map's own item n's collection */
	_ae_layout_alias,  /* an item found by what it is */
	_ae_layout_tag     /* an item collection's tag path */
};

enum ae_layout_alias
{
	_ae_layout_rockets,
	_ae_layout_sniper,
	_ae_layout_shotgun,
	_ae_layout_pistol,
	_ae_layout_overshield,
	_ae_layout_camo,
	NUMBER_OF_AE_LAYOUT_ALIASES
};

enum ae_layout_team
{
	_ae_layout_team_unset = -1,
	_ae_layout_team_red,
	_ae_layout_team_blue,
	_ae_layout_team_neutral
};

struct ae_layout_item
{
	short what;            /* enum ae_layout_what */
	short alias;           /* enum ae_layout_alias, for _ae_layout_alias */
	long like;             /* the map's item index, for _ae_layout_like */
	char tag[AE_LAYOUT_TAG_SIZE];
	real x, y, z;
	boolean has_facing;
	real facing_degrees;
	short respawn_seconds; /* -1: the map's own */
	short team;            /* enum ae_layout_team */
	short line;            /* the file's line, for the log */
};

struct ae_layout_vehicle_rule
{
	boolean turret;        /* every turret, else the palette entry */
	long palette_index;
	boolean near;
	real x, y, z, radius;
	short line;
};

struct ae_layout
{
	char map_file[AE_LAYOUT_MAP_FILE_SIZE];
	unsigned long map_hash;
	boolean clear_items;
	short removed_count;
	long removed[AE_LAYOUT_MAXIMUM_REMOVED];
	short item_count;
	struct ae_layout_item items[AE_LAYOUT_MAXIMUM_ITEMS];
	short vehicle_rule_count;
	struct ae_layout_vehicle_rule vehicle_rules[AE_LAYOUT_MAXIMUM_VEHICLE_RULES];
};

/* the file's text (length bytes, not necessarily terminated) read into *out: 0 when it is a layout of version 1 with
every line understood; else the line number at fault (1-based; the first line past a limit) with the reason in error,
and *out holds nothing to apply */
int ae_layout_parse(char const *text, long length, struct ae_layout *out, char *error, int error_size);

/* whether a layout was made for this map: its file name (case aside) and hash prefix */
boolean ae_layout_for_map(struct ae_layout const *layout, char const *map_file, unsigned long map_hash);

/* an alias's name ("rockets"), for the log */
char const *ae_layout_alias_name(short alias);

#endif /* __AE_LAYOUT_RULES_H */
