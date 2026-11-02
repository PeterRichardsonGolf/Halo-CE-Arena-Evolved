/*
AE_LAYOUT_RULES.H

AE map layout files (queue 113, notes/specs/2026-10-15-map-layouts.md): a small text file per map, shipped in the map
pack beside the map (maps/ce/layouts/<map file name without .map>.txt, never in the repo), that changes what the map places (its
netgame items, its vehicles) without touching the map file. This half is pure (no engine includes): the file read into
a struct ae_layout, every line checked; port/linux/game/ae_layout.c applies it to the loaded scenario.

The file (version 1):

	layout 1 <map file name, no .map> <map hash: 8 hex digits, the map file's hash prefix (map_file_hash.h)>
	# a comment (also after a line's words)
	clear items                       drop every netgame item of the map (then add the wanted ones)
	remove item <n>                   drop the map's item n (its index as the log lists it)
	item <what> at <x> <y> <z> [facing <degrees>] [respawn <seconds>] [team red|blue|neutral] [games all]
	remove vehicle <type> [near <x> <y> <z> <radius>]
	remove scenery <name or n> [near <x> <y> <z> <radius>]
	remove object <type> <name or n> [near <x> <y> <z> <radius>]
	                                  drop placed objects of the scenario (a fusion coil, a tree): <type> one of scenery
	                                  machine control light_fixture sound_scenery biped weapon equipment (`remove scenery`
	                                  is `remove object scenery`); <name> a word of the object's tag name (quoted when it
	                                  has spaces; case aside), <n> the palette number of that type. Scenario objects other
	                                  than items and vehicles are not sent over the network: each machine that has the
	                                  layout drops them from its own copy of the level, a machine without the file keeps
	                                  them (and their collision)
	name item <n> <label>             the map's item n's name on the item timers and waypoints (e.g. SWORD where the map
	                                  puts its sword in the shotgun's slot); <label> one word or quoted, up to 15 characters
	bot jump at <x> <y> <z> to <x> <y> <z>
	                                  for the bots (ae_bots_travel.c): a jump the walkable surfaces do not know (a gap, a
	                                  ledge up or down): a bot whose way passes within a unit of the first point, moving
	                                  towards the second, faces it and jumps, then goes on from where it lands; its route
	                                  may also take the jump on purpose. The two points 0.5 to 20 units apart
	clear spawns                      drop every player spawn of the map (then add the wanted ones)
	remove spawn <n>                  drop the map's spawn n
	spawn at <x> <y> <z> [facing <degrees>] [team red|blue] [games <up to 4 of all ctf slayer oddball king race>]
	                                  a player spawn (team red when not given; games all when not given)
	clear flags                       drop every netgame flag of the map (CTF flags, hills, teleporters ...)
	remove flag <n>                   drop the map's flag n
	flag <ctf|ctf_vehicle|ball|race|race_vehicle|teleport_from|teleport_to|hill> at <x> <y> <z> [facing <degrees>]
	     [team red|blue|<0-15>]       a netgame flag: a CTF flag's team, a hill's or a race flag's number, a teleporter
	                                  pair's number (team 0 when not given). Bug 139 / queue 141: maps without CTF flags
	                                  (the H2 collection's Terminal) get them here
	bot avoid <x> <y> <z> radius <r>  for the bots: a place they keep out of (a pit's edge, a ledge they fall from): the
	                                  walkable surfaces whose middle lies within r of the point are closed to their routes,
	                                  and no roaming place there; r more than 0, at most 50
	jump <percent>                    every player's jump speed on this map, 50 to 200 percent of the map's own (owner
	                                  2026-10-29: H2 demakes whose ledges want H2's higher jump; 115 lifts a jump's top
	                                  by about a third). The host's and its guests' copies agree (the layout travels)
	vehicle <warthog|rocket_warthog|ghost|scorpion|banshee|turret|<n>> at <x> <y> <z> [facing <degrees>]
	     [team red|blue|neutral] [games <up to 4 of all ctf slayer oddball king race>]
	                                  a multiplayer vehicle spawn of a vehicle the map already carries (0.3.5; owner:
	                                  "can't we just use the halo ce vehicles for the vehicle spawns?"): <n> its vehicle
	                                  palette's entry; a kind: the palette's entry of that kind, else a vehicle tag of that
	                                  kind the map has loaded (its globals' multiplayer warthog, ghost and scorpion). It
	                                  becomes one of the scenario's vehicle placements, so the host places it, the
	                                  gametype's vehicle set counts it and it respawns as the map's own do. team: the side
	                                  it counts for in a set of each team's (the nearest team's spawns when not given);
	                                  games: the gametypes it is placed in (all when not given). A kind the map does not
	                                  carry: that line is skipped and logged, as an item the map lacks

A bot line changes nothing of the map: a guest or a game without bots reads it and does nothing with it. Authoring:
debug.bot_hints logs each of player 1's jumps as a ready `bot jump` line, and a `bot avoid` line where player 1
crouches still a second (SHOW COORDINATES on Video Setup > GRAPHICS shows where player 1 stands).

<what>: `like <n>` (the map's own item n's collection: works on protected maps, whose tag names are scrambled), an
alias (rockets, sniper, shotgun, pistol, overshield, camo: found by what the item is, not by its name), a weapon
pack's weapon (battle_rifle or br, smg, smg_silenced or silenced_smg, carbine, ar_silenced or silenced_ar:
ae_weapon_slots.h; the entry is the map's own FALLBACK item, the pistol for the BR and the carbine, the assault rifle
for the SMGs and the silenced assault rifle, and the pack's weapon
spawns there only where the gametype names that pack and it is on the map, else the fallback does), or a tag path
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
#define AE_LAYOUT_MAXIMUM_OBJECT_RULES 32
#define AE_LAYOUT_MAP_FILE_SIZE 64
#define AE_LAYOUT_TAG_SIZE 128
#define AE_LAYOUT_MAXIMUM_NAMES 64
#define AE_LAYOUT_LABEL_SIZE 16
#define AE_LAYOUT_MAXIMUM_BOT_JUMPS 64
#define AE_LAYOUT_MAXIMUM_BOT_AVOIDS 64
#define AE_LAYOUT_BOT_JUMP_SHORTEST 0.5f
#define AE_LAYOUT_BOT_JUMP_LONGEST 20.0f
#define AE_LAYOUT_BOT_AVOID_LARGEST 50.0f
#define AE_LAYOUT_MAXIMUM_SPAWNS 256
#define AE_LAYOUT_MAXIMUM_FLAGS 64
#define AE_LAYOUT_MAXIMUM_VEHICLES 32
/* a spawn's game types, as the scenario's (game_engine.c: the engines' numbers, 12 = every gametype) */
#define AE_LAYOUT_GAMES_ALL 12
#define AE_LAYOUT_JUMP_LOWEST 50
#define AE_LAYOUT_JUMP_HIGHEST 200

enum ae_layout_what
{
	_ae_layout_like,   /* the map's own item n's collection */
	_ae_layout_alias,  /* an item found by what it is */
	_ae_layout_tag,    /* an item collection's tag path */
	_ae_layout_pack    /* a weapon pack's weapon (ae_weapon_slots.h), made from the map's own fallback item */
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
	short pack_slot;       /* the pack slot (ae_weapon_slots.h), for _ae_layout_pack */
	char tag[AE_LAYOUT_TAG_SIZE];
	real x, y, z;
	boolean has_facing;
	real facing_degrees;
	short respawn_seconds; /* -1: the map's own */
	short team;            /* enum ae_layout_team */
	boolean games_all;     /* games all: in every gametype (the entry's own game types otherwise) */
	boolean drop;          /* drop: made a little above the floor, not at rest, so it falls and settles as a dropped
	                          weapon does (on its side) */
	boolean float_here;    /* float: at rest at its own height, not put on the floor (a powerup over water) */
	short line;            /* the file's line, for the log */
};

/* a vehicle line's kind: the gametypes' vehicle types (game_engine.h's _variant_vehicle_*, in their order), or the
map's vehicle palette's entry */
enum ae_layout_vehicle_kind
{
	_ae_layout_vehicle_palette = -1,
	_ae_layout_vehicle_warthog,
	_ae_layout_vehicle_ghost,
	_ae_layout_vehicle_scorpion,
	_ae_layout_vehicle_rocket_warthog,
	_ae_layout_vehicle_banshee,
	_ae_layout_vehicle_turret,
	NUMBER_OF_AE_LAYOUT_VEHICLE_KINDS
};

/* vehicle <kind|n> at <x y z> ... */
struct ae_layout_vehicle
{
	short kind;            /* enum ae_layout_vehicle_kind */
	long palette_index;    /* for _ae_layout_vehicle_palette */
	real x, y, z;
	boolean has_facing;
	real facing_degrees;
	short team;            /* enum ae_layout_team: _ae_layout_team_unset for the nearest team's */
	short games[4];        /* the scenario's game types (as a spawn's), 0 for none */
	short line;
};

struct ae_layout_vehicle_rule
{
	boolean turret;        /* every turret, else the palette entry */
	long palette_index;
	boolean near;
	real x, y, z, radius;
	short line;
};

/* the scenario's placed objects a layout can drop (remove object <type>) */
enum ae_layout_object_type
{
	_ae_layout_object_scenery,
	_ae_layout_object_machine,
	_ae_layout_object_control,
	_ae_layout_object_light_fixture,
	_ae_layout_object_sound_scenery,
	_ae_layout_object_biped,
	_ae_layout_object_weapon,
	_ae_layout_object_equipment,
	NUMBER_OF_AE_LAYOUT_OBJECT_TYPES
};

/* remove object <type> <name or n> [near x y z r] */
struct ae_layout_object_rule
{
	short type;            /* enum ae_layout_object_type */
	boolean by_name;       /* a word of the palette entry's tag name, else the palette number */
	long palette_index;
	char name[AE_LAYOUT_TAG_SIZE];
	boolean near;
	real x, y, z, radius;
	short line;
};

/* bot jump at <from> to <to> */
struct ae_layout_bot_jump
{
	real from_x, from_y, from_z;
	real to_x, to_y, to_z;
	short line;
};

/* bot avoid <x y z> radius <r> */
struct ae_layout_bot_avoid
{
	real x, y, z, radius;
	short line;
};

/* spawn at <x y z> ... */
struct ae_layout_spawn
{
	real x, y, z;
	boolean has_facing;
	real facing_degrees;
	short team;            /* 0 red, 1 blue */
	short games[4];        /* the scenario's game types, 0 for none */
	short line;
};

/* flag <type> at <x y z> ...: type as scenario_definitions.h's netgame flags */
enum ae_layout_flag_type
{
	_ae_layout_flag_ctf = 0,
	_ae_layout_flag_ctf_vehicle = 1,
	_ae_layout_flag_ball = 2,
	_ae_layout_flag_race = 3,
	_ae_layout_flag_race_vehicle = 4,
	_ae_layout_flag_teleport_from = 6,
	_ae_layout_flag_teleport_to = 7,
	_ae_layout_flag_hill = 8
};

struct ae_layout_flag
{
	short type;            /* enum ae_layout_flag_type */
	short team;
	real x, y, z;
	boolean has_facing;
	real facing_degrees;
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
	short object_rule_count;
	struct ae_layout_object_rule object_rules[AE_LAYOUT_MAXIMUM_OBJECT_RULES];
	short name_count;
	struct { long item; char label[AE_LAYOUT_LABEL_SIZE]; } names[AE_LAYOUT_MAXIMUM_NAMES];
	short bot_jump_count;
	struct ae_layout_bot_jump bot_jumps[AE_LAYOUT_MAXIMUM_BOT_JUMPS];
	short bot_avoid_count;
	struct ae_layout_bot_avoid bot_avoids[AE_LAYOUT_MAXIMUM_BOT_AVOIDS];
	boolean clear_spawns;
	short removed_spawn_count;
	long removed_spawns[AE_LAYOUT_MAXIMUM_SPAWNS];
	short spawn_count;
	struct ae_layout_spawn spawns[AE_LAYOUT_MAXIMUM_SPAWNS];
	boolean clear_flags;
	short removed_flag_count;
	long removed_flags[AE_LAYOUT_MAXIMUM_FLAGS];
	short flag_count;
	struct ae_layout_flag flags[AE_LAYOUT_MAXIMUM_FLAGS];
	short vehicle_count;
	struct ae_layout_vehicle vehicles[AE_LAYOUT_MAXIMUM_VEHICLES];
	short jump_percent;    /* jump <percent>; 0 when not given */
};

/* the file's text (length bytes, not necessarily terminated) read into *out: 0 when it is a layout of version 1 with
every line understood; else the line number at fault (1-based; the first line past a limit) with the reason in error,
and *out holds nothing to apply */
int ae_layout_parse(char const *text, long length, struct ae_layout *out, char *error, int error_size);
/* ae_layout_parse with the game's SEARCH AND DESTROY told (snd nonzero): a line "snd <line>" is read as <line> in a
SEARCH AND DESTROY game and left out in any other (ae_layout_parse: never one) */
int ae_layout_parse_for(char const *text, long length, int snd, struct ae_layout *out, char *error, int error_size);

/* whether a layout was made for this map: its file name (case aside) and hash prefix */
boolean ae_layout_for_map(struct ae_layout const *layout, char const *map_file, unsigned long map_hash);

/* an alias's name ("rockets"), for the log */
char const *ae_layout_alias_name(short alias);

/* an object type's name ("scenery", "machine"), for the log */
char const *ae_layout_object_type_name(short type);

/* a flag type's name ("ctf", "hill"), for the log */
char const *ae_layout_flag_type_name(short type);

/* a vehicle kind's name ("rocket_warthog"), for the log */
char const *ae_layout_vehicle_kind_name(short kind);

/* a vehicle tag's kind by its name, as the gametypes' vehicle sets tell it (game_engine.c's
game_engine_variant_vehicle_type after the globals' three): NUMBER_OF_AE_LAYOUT_VEHICLE_KINDS for none */
short ae_layout_vehicle_kind_of_name(char const *tag_name);

/* whether a line's game types (as a spawn's: 12 every gametype, 0 none) take the gametype engine's (game_engine.h:
1 ctf, 2 slayer, 3 oddball, 4 king, 5 race) */
boolean ae_layout_games_take(short const games[4], long game_engine_index);

#endif /* __AE_LAYOUT_RULES_H */
