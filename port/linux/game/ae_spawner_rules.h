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

/* the most spawners a map has, and the holograms' spots (one a spawner) */
#define AE_SPAWNERS_MAXIMUM 256

/* a map item's own respawn seconds, as Halo CE reads them: the item's spawn_time, else its item collection's; 0 when
neither has one (the set's row or the game's 30 s then) */
int ae_spawner_own_seconds(int item_seconds, int collection_seconds);

/* which load of which map a spawner table was built for: the scenario, its netgame item list (address and count: a
layout's or an snd line's list differs) and the map load's serial (ae_layout_load_serial); a table whose key differs
from the map's now is rebuilt */
struct ae_spawner_table_key
{
	void const *scenario;
	unsigned long items_address;
	long items_count;
	unsigned long load;
};
int ae_spawner_table_current(struct ae_spawner_table_key const *built, struct ae_spawner_table_key const *now);

/* whether a guest builds its own spawner table (its item timers and holograms from it): once its host's spawner state
(message 128) came, or when its host is a Delta build that plays spawners (1.0.0-rc5 and later). host_answer: 1 the
host speaks Delta (host_plays: its build plays spawners), 0 it does not, -1 not known yet. An older host spawns on Halo
CE's clocks, and the guest's timers follow those */
int ae_spawner_guest_builds(int host_answer, int host_plays, int message_seen);

/* a weapon set switch: whether a spawner keeps the weapon of its old logic as its own. A spot that becomes STATIC does
not (its clock starts over, and its old weapon is just a weapon on the map) */
int ae_spawner_switch_keeps_object(int old_logic, int new_logic);

/* the walk-over ammo refill from a weapon on the ground: refused for a spot's own fresh weapon (on its spot, not yet
taken) on RACKS, so a player holding that weapon does not empty it; CLASSIC and CLASSIC + HOLOGRAMS refill as Halo CE,
and a weapon a player dropped refills everywhere. On RACKS the same weapon in hand swaps for the fresh one instead */
int ae_spawner_refuses_walkover(int look_classic, int own_fresh_weapon);

/* why a spawner's weapon went from the map untaken (no player picked it up), for its log line: a player's walk-over
took its ammo (the game removes a weapon it empties) within AE_SPAWNER_LOST_WINDOW_TICKS of the loss, else something
else removed it. ammo_taken_at: the tick a walk-over last took ammo from it, -1 never */
#define AE_SPAWNER_LOST_WINDOW_TICKS 30
enum { AE_SPAWNER_LOST_REMOVED, AE_SPAWNER_LOST_WALKOVER };
int ae_spawner_lost_reason(long ammo_taken_at, long now);
char const *ae_spawner_lost_words(int reason);

/* RACKS' RANDOMIZE (the AE block's spawner_randomize, the same order): whether a rack draws its weapon again at an event
of the game. EVERY GAME draws as the game starts and keeps it; EVERY ROUND draws again as each round starts; EVERY SPAWN
draws at each spawn. A rack that keeps its weapon draws it from its class's shared bag (ae_spawner_pool_rules.h's
rotation over the racks of one class and tier, in order): no two racks of a class hold the same weapon while others
remain */
enum { AE_SPAWNER_RANDOMIZE_EVERY_GAME, AE_SPAWNER_RANDOMIZE_EVERY_ROUND, AE_SPAWNER_RANDOMIZE_EVERY_SPAWN };
enum { AE_SPAWNER_EVENT_GAME_START, AE_SPAWNER_EVENT_ROUND_START, AE_SPAWNER_EVENT_SPAWN };
int ae_spawner_randomize_draws(int randomize, int event);
/* whether a rack keeps one weapon between draws (EVERY GAME and EVERY ROUND; an unknown value: EVERY GAME) */
int ae_spawner_randomize_keeps(int randomize);

/* WEAPON SPAWNS (the AE block's spawner_look: 0 RACKS, 1 CLASSIC, 2 CLASSIC + HOLOGRAMS): whether its spots play CLASSIC's
rules (plain weapons, one fixed weapon a spot, the fixed timer, Halo CE's walk-over ammo), and whether a spot shows the
hologram of its weapon while it is away (RACKS and CLASSIC + HOLOGRAMS) */
int ae_spawner_look_classic(int look);
int ae_spawner_look_holograms(int look);

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
