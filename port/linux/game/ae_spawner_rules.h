/*
AE_SPAWNER_RULES.H

port: AE weapon spawners (0.7.8). The pure
rules every weapon spot of a map is converted by: the class and tier of each stock weapon, the tier a spot ends up with
(the weapon's, promoted by a map item's own long time), the class a mixed item collection gets, which two map items
lying together become ONE spawner, the default spawn logic of a (class, tier) cell, the words of the three enums, and
the fade of a power pad's waypoint. No engine includes: port/linux/tests/ae_spawner_rules_test.c compiles this alone;
ae_spawners.c applies it in the game.

Words (also the layout sidecar's and the weapon set files'): classes PRECISION AUTOMATIC PISTOL CLOSE LONG_RANGE HEAVY
MELEE ANY; tiers BASE POWER ANY; logic static pickup expired trigger.
*/

#ifndef __AE_SPAWNER_RULES_H
#define __AE_SPAWNER_RULES_H

#include "ae_pure_types.h"

/* the classes: the picker's six kinds plus CLOSE (the shotguns' home) and ANY (a mixed spot) */
enum ae_spawner_class
{
	AE_SPAWNER_PRECISION = 0,
	AE_SPAWNER_AUTOMATIC,
	AE_SPAWNER_PISTOL,
	AE_SPAWNER_CLOSE,
	AE_SPAWNER_LONG_RANGE,
	AE_SPAWNER_HEAVY,
	AE_SPAWNER_MELEE,
	AE_SPAWNER_CLASS_ANY,
	AE_SPAWNER_CLASS_COUNT
};

/* the tiers: an open list by name in the readers (a later T1/T2/T3 adds here), two shipped plus ANY */
enum ae_spawner_tier
{
	AE_SPAWNER_BASE = 0,
	AE_SPAWNER_POWER,
	AE_SPAWNER_TIER_ANY,
	AE_SPAWNER_TIER_COUNT
};

/* the spawn logic (design 3.2): static = the game's own clock, taken or not; pickup = the period after a pickup;
expired = the period after the weapon is out of play (RED until then); trigger = only when its channel fires */
enum ae_spawner_logic
{
	AE_SPAWNER_STATIC = 0,
	AE_SPAWNER_PICKUP,
	AE_SPAWNER_EXPIRED,
	AE_SPAWNER_TRIGGER,
	AE_SPAWNER_LOGIC_COUNT
};

/* the stock weapons a map's items hold: the globals' weapon list order (game_engine.c's private enum), plus the sword
a map may carry (not in the list; found by what it is) */
enum ae_spawner_stock
{
	AE_STOCK_ASSAULT_RIFLE = 0,
	AE_STOCK_FLAMETHROWER,
	AE_STOCK_GRAVITY_RIFLE,
	AE_STOCK_NEEDLER,
	AE_STOCK_PISTOL,
	AE_STOCK_PLASMA_PISTOL,
	AE_STOCK_PLASMA_RIFLE,
	AE_STOCK_ROCKET_LAUNCHER,
	AE_STOCK_SHOTGUN,
	AE_STOCK_SNIPER_RIFLE,
	AE_STOCK_SWORD,
	AE_STOCK_COUNT
};

/* a map item whose own spawn time is this long or longer is a POWER spot whatever its weapon (design 4.3) */
#define AE_SPAWNER_POWER_OWN_SECONDS 90
/* two weapon items within this distance (world units) of each other are one spawner */
#define AE_SPAWNER_MERGE_RADIUS 1.5f
/* a respawn below this is raised to it (design 3.2) */
#define AE_SPAWNER_RESPAWN_FLOOR_SECONDS 5
/* a power pad's waypoint: full past this distance, gone within the near one (the weapon itself is in view) */
#define AE_SPAWNER_WAYPOINT_FAR 12.0f
#define AE_SPAWNER_WAYPOINT_NEAR 6.0f

/* the class and the tier of a stock weapon (the shotgun is CLOSE POWER, a one-shot kill up close that the
classic competitive community times as a power weapon); -1 for a value outside the list */
int ae_spawner_stock_class(int stock);
int ae_spawner_stock_tier(int stock);

/* the stock weapon a layout or set word names ("pistol", "shotgun" ...: the list's words), -1 for none */
int ae_spawner_stock_of_word(char const *word);
char const *ae_spawner_stock_word(int stock);

/* the tier a spot ends up with: the weapon's own, promoted to POWER by a map item's own time of
AE_SPAWNER_POWER_OWN_SECONDS or more; a map time never demotes */
int ae_spawner_tier_of(int weapon_tier, long own_seconds);

/* the class of a spot from the weight each class has in its item collection (an item collection may hold several
weapons): the heaviest, ANY on a tie between the heaviest, ANY when every weight is zero */
int ae_spawner_class_of_weights(int const weights[AE_SPAWNER_CLASS_COUNT]);

/* whether two map weapon items are one spot: within AE_SPAWNER_MERGE_RADIUS of each other and placed in
at least one common game type (two items on one spot with disjoint game masks, Hang 'em High's shotguns "not CTF" over
its snipers "CTF only", stay apart, so each game type plays the map it always did) */
int ae_spawner_merges(real const a[3], real const b[3], unsigned long games_a, unsigned long games_b);

/* the tier two merged items share: the higher */
int ae_spawner_merged_tier(int tier_a, int tier_b);

/* the logic a (class, tier) cell plays when its set row names none: POWER spots static (the pad's fixed clock), BASE
spots pickup (casual's default); a trigger only comes from a channel */
int ae_spawner_default_logic(int cls, int tier);

/* the logic a spot plays under the gametype's WEAPON SPAWNS (the AE block's spawner_look): RACKS (classic 0) the
set's own, CLASSIC (classic 1) the static logic, a timed drop's too (plain weapons on the game's own clock) */
int ae_spawner_look_logic(int classic, int logic);

/* CLASSIC's one weapon of a pool: the index of the heaviest weight (the first of a tie; NULL weights: the first), -1 for
an empty pool. Every spawn of the spot is that weapon */
int ae_spawner_look_pick(int count, unsigned char const *weights);

/* a respawn in seconds with the floor applied */
long ae_spawner_respawn_seconds(long seconds);

/* the fade of a power pad's waypoint by the viewer's distance: 0 within the near distance, 1 past the far one, a
straight ramp between */
real ae_spawner_waypoint_alpha(real distance);

/* the words (upper case as the design writes them for classes and tiers, lower case for the logic) and their reverse
(case aside; -1 for an unknown word) */
char const *ae_spawner_class_word(int cls);
char const *ae_spawner_tier_word(int tier);
char const *ae_spawner_logic_word(int logic);
int ae_spawner_class_of_word(char const *word);
int ae_spawner_tier_of_word(char const *word);
int ae_spawner_logic_of_word(char const *word);

#endif /* __AE_SPAWNER_RULES_H */
