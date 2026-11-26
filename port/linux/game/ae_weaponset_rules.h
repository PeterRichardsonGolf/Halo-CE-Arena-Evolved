/*
AE_WEAPONSET_RULES.H

port: AE weapon spawners (0.7.8). A WEAPON SET
fills a map's spawners: for each (class, tier) cell its spawn logic, its seconds, its first-spawn delay and its pool of
weapons; plus the set's draw rule, tier weights, whether a map item's own time wins, how a rack treats a held weapon's
ammo, whether SCAN is on, the timed-drop channels, the swaps and the override scopes. Three sets ship built in (casual,
comp, btb: tools/weapon_cache/weaponsets/<name>.txt, generated into ae_weaponsets.inc by tools/weapon_cache/weaponsets.py
and held equal by tools/test_weaponsets.py), so a set id on the wire is enough. Pure: no engine includes;
port/linux/tests/ae_weaponset_rules_test.c compiles this alone; ae_spawners.c applies a set in the game.

The file (version 1; a line the reader does not know refuses the whole file, as a layout's does):

	weaponset 1 <name>                  the header (first line with words)
	format 1                            the file format version; another refuses the file
	extends <name>                      rows below replace the parent's by (class, tier); its scalars are inherited
	draw rotate|random                  rotate: no repeat until the pool is used up (default)
	tier_weight BASE 70 POWER 30        name / value pairs for a spawner of tier ANY (tiers are an open named list)
	map_time on|off                     POWER rows: a map item's own nonzero time wins (default on)
	rack_ammo fresh|topup               read and kept for the files: the spots go by WEAPON SPAWNS instead (RACKS
	                                    never refill a held weapon from a rack's fresh one, a fresh swap only; both
	                                    CLASSICs refill as Halo CE)
	scan on|off                         the SCAN pulse in this set (default on)
	rules ae|reference                  where the two rule sets differ (ae_spawner_logic_rules.h; default ae): a static
	                                    POWER pad's beat under 15 s after a pickup (ae spawns, reference skips), an
	                                    expired rack's weapon held empty (ae: in play, reference: gone), a taken POWER
	                                    pad (ae: red, reference: never red)
	swap <ref> <ref>                    wherever the set would give the first, give the second
	row <CLASS|ANY> <TIER|ANY> logic <L> [respawn <S>] [delay <S>] [ammo <pct>] [channel <NAME>] weapons <ref>[*<w>] ...
	                                    respawn needed unless the logic is trigger (its channel's beat); weights 1-9
	row <CLASS> <TIER> mute             nothing spawns in this cell under this set
	channel <NAME> every <S> variance <pct>
	override global|tier <T>|class <C>|cell <C> <T> <field> <value> [...]
	                                    field: logic respawn delay ammo (one or more pairs); most specific wins:
	                                    cell beats class beats tier beats global beats the row

<ref>: a registry key or alias, `<key>:<config>`, or `stock:<word>` (ae_spawner_rules.h's stock words). The reader
checks the shape only; what the cache and the map can supply is the engine's business (an entry it cannot supply is
dropped with a log line; an empty pool spawns the map's own weapon).
*/

#ifndef __AE_WEAPONSET_RULES_H
#define __AE_WEAPONSET_RULES_H

#include "ae_pure_types.h"
#include "ae_spawner_rules.h"

#define AE_WEAPONSET_VERSION 1
#define AE_WEAPONSET_NAME_SIZE 16
#define AE_WEAPONSET_REF_SIZE 32
#define AE_WEAPONSET_POOL_MAXIMUM 12
#define AE_WEAPONSET_ROWS_MAXIMUM 24
#define AE_WEAPONSET_CHANNELS_MAXIMUM 16
#define AE_WEAPONSET_SWAPS_MAXIMUM 16
#define AE_WEAPONSET_OVERRIDES_MAXIMUM 32
#define AE_WEAPONSET_RESPAWN_MAXIMUM 3600
#define AE_WEAPONSET_WEIGHT_MAXIMUM 9

/* the built-in sets' ids (AECW's set id: 0 casual, 1 comp, 2 btb, 3-14 reserved, 15 custom) */
#define AE_WEAPONSET_CASUAL 0
#define AE_WEAPONSET_COMP 1
#define AE_WEAPONSET_BTB 2
#define AE_WEAPONSET_BUILTIN_COUNT 3
#define AE_WEAPONSET_CUSTOM 15

enum ae_weaponset_draw { AE_WEAPONSET_ROTATE = 0, AE_WEAPONSET_RANDOM };

/* the fields an override may set (bits) */
#define AE_WEAPONSET_FIELD_LOGIC 1
#define AE_WEAPONSET_FIELD_RESPAWN 2
#define AE_WEAPONSET_FIELD_DELAY 4
#define AE_WEAPONSET_FIELD_AMMO 8

/* the override scopes, in rising precedence */
enum ae_weaponset_scope
{
	AE_WEAPONSET_SCOPE_GLOBAL = 0,
	AE_WEAPONSET_SCOPE_TIER,
	AE_WEAPONSET_SCOPE_CLASS,
	AE_WEAPONSET_SCOPE_CELL
};

struct ae_weaponset_row
{
	signed char cls, tier, logic;
	unsigned char mute;
	short seconds, delay, ammo_percent; /* ammo_percent 0 = the weapon's own */
	char channel[AE_WEAPONSET_NAME_SIZE];
	short weapon_count;
	char weapons[AE_WEAPONSET_POOL_MAXIMUM][AE_WEAPONSET_REF_SIZE];
	unsigned char weights[AE_WEAPONSET_POOL_MAXIMUM];
};

struct ae_weaponset_channel
{
	char name[AE_WEAPONSET_NAME_SIZE];
	short every_seconds, variance_percent;
};

struct ae_weaponset_swap
{
	char from[AE_WEAPONSET_REF_SIZE], to[AE_WEAPONSET_REF_SIZE];
};

struct ae_weaponset_override
{
	signed char scope, cls, tier; /* cls / tier -1 where the scope does not name one */
	unsigned char fields; /* AE_WEAPONSET_FIELD_* bits set */
	signed char logic;
	short seconds, delay, ammo_percent;
};

/* which scalar lines the file set itself (so `extends` can fill the rest from the parent) */
#define AE_WEAPONSET_HAS_DRAW 1
#define AE_WEAPONSET_HAS_TIER_WEIGHT 2
#define AE_WEAPONSET_HAS_MAP_TIME 4
#define AE_WEAPONSET_HAS_RACK_AMMO 8
#define AE_WEAPONSET_HAS_SCAN 16
#define AE_WEAPONSET_HAS_RULES 32

struct ae_weaponset
{
	char name[AE_WEAPONSET_NAME_SIZE];
	char extends[AE_WEAPONSET_NAME_SIZE]; /* "" none */
	unsigned char has; /* AE_WEAPONSET_HAS_* */
	signed char draw;
	short tier_weight[AE_SPAWNER_TIER_COUNT]; /* BASE, POWER (ANY unused) */
	unsigned char map_time, rack_ammo_fresh, scan;
	unsigned char rules_reference; /* `rules reference` (0: `rules ae`) */
	short row_count;
	struct ae_weaponset_row rows[AE_WEAPONSET_ROWS_MAXIMUM];
	short channel_count;
	struct ae_weaponset_channel channels[AE_WEAPONSET_CHANNELS_MAXIMUM];
	short swap_count;
	struct ae_weaponset_swap swaps[AE_WEAPONSET_SWAPS_MAXIMUM];
	short override_count;
	struct ae_weaponset_override overrides[AE_WEAPONSET_OVERRIDES_MAXIMUM];
};

/* reads a set file: 0 accepted (out filled, defaults where a line is absent), else the number of the line refused (1
for a file-level refusal) with the reason in `error` */
int ae_weaponset_read(char const *text, long length, struct ae_weaponset *out, char *error, int error_size);

/* the child takes what it lacks from its parent: the parent's rows whose (class, tier) the child has no row for, its
channels and swaps by name, its scalars where the child set none. The child's `extends` must name the parent; 0 when it
does not (nothing changed) */
int ae_weaponset_extend(struct ae_weaponset *child, struct ae_weaponset const *parent);

/* the row of a cell: the exact (class, tier), else (class, ANY), else (ANY, tier), else (ANY, ANY), else NULL. A row
with `mute` set is returned as it is (the caller spawns nothing) */
struct ae_weaponset_row const *ae_weaponset_row_of(struct ae_weaponset const *set, int cls, int tier);

/* the row of a cell with the set's overrides applied, most specific last: 0 when the cell has no row */
int ae_weaponset_cell(struct ae_weaponset const *set, int cls, int tier, struct ae_weaponset_row *out);

/* a weapon ref after the set's swaps (the first matching swap; a ref swapped to itself stays) */
char const *ae_weaponset_swapped(struct ae_weaponset const *set, char const *ref);

/* a channel by name, NULL for none */
struct ae_weaponset_channel const *ae_weaponset_channel_of(struct ae_weaponset const *set, char const *name);

/* the built-in sets: their text and id by name (NULL / -1 unknown), their name by id ("" unknown) */
char const *ae_weaponsets_builtin_text(char const *name);
int ae_weaponsets_builtin_id(char const *name);
char const *ae_weaponsets_builtin_name(int id);

#endif /* __AE_WEAPONSET_RULES_H */
