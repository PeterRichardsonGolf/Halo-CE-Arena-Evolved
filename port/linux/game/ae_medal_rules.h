/*
AE_MEDAL_RULES.H

MEDALS (ae_medals.c): the rules that say who earns which medal, as pure code (port/linux/tests/ae_medal_rules_test.c
runs them). No engine headers: ae_medals.c reads the game, fills the event structs and hands them in; what comes out
is a list of (player, medal), in the order the medals are given. Medals are ae_medals.h's numbers.

Players are slots (the absolute player index), -1 for none. All times are game ticks, 30 a second.

- A kill of a player by an enemy gives, in this order: assists, KILLJOY, spree, multikill, weapon runs, comeback,
  kill from the grave, I see you, reload this, snapshot, revenge, avenger, headshot (a sniper's: SNIPER KILL; a perfect
  kill's: PERFECT), the weapon's, the ones the kill's facts and the records decide (yard sale, last shot, hail mary, cluster
  luck, quigley, autopilot, driveby, pineapple express, no scope, quick draw, reversal, nade shot, 360, ninja, off the rack,
  and the five kills of a kind), wheelman, extermination, close call, protector (guardian angel, bodyguard), first strike,
  the game type's, and KILL if the killer got no other.
- A death (any) ends the dead player's spree, multikill, weapon runs, hail count and last victim. A betrayal or a
  suicide counts for no one: no kill, no medal.
- The counters are exact: a medal comes on the kill that brings the count to its number, not after.
*/

#ifndef __AE_MEDAL_RULES_H
#define __AE_MEDAL_RULES_H

#include "ae_medals.h"

enum
{
	AE_MEDAL_RULES_PLAYERS = 128,
	AE_MEDAL_RULES_ATTACKERS = 4,
	/* the most medals one call gives (the end of a game gives the most) */
	AE_MEDAL_RULES_OUT = 640,
};

/* the windows and thresholds */
enum
{
	/* kills this close together (ticks from the last kill) are one multikill */
	AE_MEDAL_MULTIKILL_TICKS = 120,
	/* an attacker's damage counts for an assist this long (ticks) */
	AE_MEDAL_ASSIST_TICKS = 180,
	AE_MEDAL_ASSIST_PERCENT = 40,
	AE_MEDAL_KILLJOY_SPREE = 5,
	AE_MEDAL_COMEBACK_DEATHS = 3,
	AE_MEDAL_HAIL_KILLS = 5,
	AE_MEDAL_JUGGERNAUT_SPREE_KILLS = 5,
	AE_MEDAL_IMMOVABLE_KILLS = 10,
	AE_MEDAL_EXTERMINATION_MULTIKILL = 4,
	/* the ball's time held (ticks) */
	AE_MEDAL_BALL_HOLDER_TICKS = 300,
	AE_MEDAL_BALL_KEEPER_TICKS = 600,
	AE_MEDAL_BALL_MASTER_TICKS = 900,
	AE_MEDAL_BALL_HOG_TICKS = 1350,
	AE_MEDAL_MAGIC_HANDS_TICKS = 1800,
	/* race: the checkpoints of REDLINED IT, in the game's first this many ticks */
	AE_MEDAL_REDLINED_CHECKPOINTS = 10,
	AE_MEDAL_REDLINED_TICKS = 1800,
	/* a game's end: the lead (kills or points) of STEAKTACULAR, and PERFECTION's and TRIPLE DOUBLE's kills */
	AE_MEDAL_STEAKTACULAR_LEAD = 20,
	AE_MEDAL_PERFECTION_KILLS = 15,
	AE_MEDAL_TRIPLE_DOUBLE_EACH = 10,
	/* the second grenade or sniper kill of a pair comes this many ticks after the first at the most */
	AE_MEDAL_PAIR_TICKS = 3,
	/* exactly this many kills of a kind in a game give its proficiency medal */
	AE_MEDAL_PROFICIENCY_KILLS = 5,
	AE_MEDAL_WINGMAN_ASSISTS = 10,
	AE_MEDAL_BODYGUARD_KILLS = 5,
	/* holding the ball without a drop (ticks), and the hill's time uncontested before a guardian medal (ticks) */
	AE_MEDAL_STRAIGHT_BALLING_TICKS = 1800,
	AE_MEDAL_HILL_GUARDIAN_TICKS = 900,
	/* a ball carrier killed with this share (percent) of the score to win or more */
	AE_MEDAL_FUMBLE_PERCENT = 95,
};

/* distances in world units, and a speed in world units a second */
#define AE_MEDAL_HAIL_MARY_DISTANCE 11.7f
#define AE_MEDAL_GUARDIAN_ANGEL_DISTANCE 10.4f
#define AE_MEDAL_STAND_DISTANCE 2.0f
#define AE_MEDAL_AUTOPILOT_SPEED 1.0f

/* the game's damage categories (damage.c's), the numbers a kill's damage tells */
enum
{
	AE_MEDAL_CATEGORY_NONE = 0,
	AE_MEDAL_CATEGORY_BULLET = 2,
	AE_MEDAL_CATEGORY_GRENADE = 3,
	AE_MEDAL_CATEGORY_HIGH_EXPLOSIVE = 4,
	AE_MEDAL_CATEGORY_SNIPER = 5,
	AE_MEDAL_CATEGORY_MELEE = 6,
	AE_MEDAL_CATEGORY_FLAME = 7,
	AE_MEDAL_CATEGORY_MOUNTED = 8,
	AE_MEDAL_CATEGORY_VEHICLE = 9,
	AE_MEDAL_CATEGORY_PLASMA = 10,
	AE_MEDAL_CATEGORY_NEEDLE = 11,
	AE_MEDAL_CATEGORY_SHOTGUN = 12,
};

/* the damage's side effect that kills the unsuspecting outright (damage.c's) */
enum
{
	AE_MEDAL_SIDE_EFFECT_LETHAL_TO_THE_UNSUSPECTING = 2,
};

/* what a damage tag's path says of the weapon (ae_medal_rules_weapon) */
enum
{
	AE_MEDAL_WEAPON_NONE,
	AE_MEDAL_WEAPON_SNIPER,
	AE_MEDAL_WEAPON_FRAG_GRENADE,
	AE_MEDAL_WEAPON_PLASMA_GRENADE,
	AE_MEDAL_WEAPON_PLASMA_GRENADE_STUCK,
	AE_MEDAL_WEAPON_ROCKET,
	/* a needler's blast of the combined needles (not its needle, its detonation, or its melee) */
	AE_MEDAL_WEAPON_NEEDLER_SUPERCOMBINE,
	AE_MEDAL_WEAPON_NEEDLER,
	AE_MEDAL_WEAPON_FUEL_ROD,
	AE_MEDAL_WEAPON_FLAG,
	AE_MEDAL_WEAPON_BALL,
	AE_MEDAL_WEAPON_OTHER,
};

/* what a weapon in hand is (ae_medal_rules_role, from its weapon tag's path) */
enum
{
	AE_MEDAL_ROLE_NONE,
	AE_MEDAL_ROLE_PISTOL,
	AE_MEDAL_ROLE_PLASMA_PISTOL,
	AE_MEDAL_ROLE_ASSAULT_RIFLE,
	AE_MEDAL_ROLE_SMG,
	AE_MEDAL_ROLE_BATTLE_RIFLE,
	AE_MEDAL_ROLE_EVO_RIFLE,
	AE_MEDAL_ROLE_DMR,
	AE_MEDAL_ROLE_CARBINE,
	AE_MEDAL_ROLE_PLASMA_RIFLE,
	AE_MEDAL_ROLE_SHOTGUN,
	AE_MEDAL_ROLE_SNIPER,
	AE_MEDAL_ROLE_ROCKET,
	/* the fuel rod and the plasma cannon */
	AE_MEDAL_ROLE_FUEL_ROD,
	AE_MEDAL_ROLE_SWORD,
	AE_MEDAL_ROLE_HAMMER,
	AE_MEDAL_ROLE_NEEDLER,
	AE_MEDAL_ROLE_OTHER,
};

/* the seat a killer sat in, and what it rode */
enum
{
	AE_MEDAL_SEAT_NONE,
	AE_MEDAL_SEAT_DRIVER,
	AE_MEDAL_SEAT_GUNNER,
	/* any other seat: where a rider may fire a weapon of their own */
	AE_MEDAL_SEAT_PASSENGER,
};

enum
{
	AE_MEDAL_VEHICLE_NONE,
	AE_MEDAL_VEHICLE_GROUND,
	AE_MEDAL_VEHICLE_AIR,
};

enum
{
	AE_MEDAL_MODE_OTHER,
	AE_MEDAL_MODE_SLAYER,
	AE_MEDAL_MODE_CTF,
	AE_MEDAL_MODE_ODDBALL,
	AE_MEDAL_MODE_KING,
	AE_MEDAL_MODE_RACE,
};

/* the oddball's ball types */
enum
{
	AE_MEDAL_BALL_NORMAL,
	AE_MEDAL_BALL_REVERSE_TAG,
	AE_MEDAL_BALL_JUGGERNAUT,
};

/* ---------- state */

struct ae_medal_player
{
	int spree;
	int multikill;
	long last_kill_tick;
	long strike_tick;		/* the game's count at their last kill, kept through a death: the highest is LAST STRIKE's */
	int sniper_run;
	int shotgun_run;
	int splatter_run;
	int last_category;
	int deaths_since_kill;
	int last_killer;
	int last_victim;
	/* the game's totals, for its end */
	int kills;
	int deaths;
	int assists;
	int headshots;
	int captures;
	long ball_ticks;
	int ball_kills;
	int juggernaut_kills;
	int hail_kills;
	int checkpoints;
	/* batch 2: kills of a kind this game (not reset by a death), and the ticks of the last grenade and sniper kill */
	int gunslinger_kills;
	int rifleman_kills;
	int scattergunner_kills;
	int bomber_kills;
	int grenadier_kills;
	int boxer_kills;
	int driver_kills;
	int pilot_kills;
	int gunner_kills;
	int protector_kills;
	long last_grenade_tick;
	long last_sniper_tick;
	/* the hill's time on it uncontested (ticks) */
	long hill_ticks;
};

struct ae_medal_game
{
	long strikes;			/* kills so far this game, for the order of two in one tick */
	int mode;
	int teams;
	int assault;
	int ball_type;
	int first_strike_done;
	int first_touch_done;
	int pole_position_done;
	/* the last player to get a kill */
	/* the hill: its first scorer, tracked while they hold it (never off it, never dead) */
	int hill_first;
	int hill_valid;
	int hill_on_foot;
	int hill_in_vehicle;
	struct ae_medal_player players[AE_MEDAL_RULES_PLAYERS];
};

struct ae_medal_out_item
{
	short player;
	short medal;
};

/* what a call gave */
struct ae_medal_out
{
	int count;
	struct ae_medal_out_item items[AE_MEDAL_RULES_OUT];
};

/* ---------- events */

struct ae_medal_attacker
{
	/* the attacking player, -1 for none (or not a player) */
	int player;
	long tick;
	float damage;
};

/* a player died: ae_medal_rules_kill. Facts of the moment are the glue's reading of the game. */
struct ae_medal_kill
{
	long tick;
	/* -1: no player's kill. The same as victim: a suicide */
	int killer;
	int victim;
	/* the game's team of each player, by slot (the whole array: the rules read the players they name) */
	short const *team_of;
	/* the killer was the victim's teammate (and not the victim) */
	int betrayal;
	/* the killing blow: its category, its tag path's weapon, and what it was */
	int category;
	int weapon;
	int headshot;
	int killed_instantly;
	int from_behind;		/* the killer stood behind the victim */
	int side_effect;
	/* the killer's unit was dead as the blow landed */
	int killer_dead;
	int victim_invisible;
	int victim_reloading;
	/* the killer, on foot as a biped, with a weapon in hand and not zoomed in */
	int killer_snapshot;
	/* the killer's health under 60 percent with an empty shield, and alive */
	int killer_close_call;
	/* the player who drives the vehicle the killer rides, -1 for none */
	int driver;
	/* a living teammate of the killer was hurt by the victim within the assist window */
	int protected_teammate;
	/* the victim's team's other members alive */
	int victim_team_survivors;
	int victim_carried_flag;
	int victim_carried_ball;
	int killer_carried_ball;
	int killer_on_hill;
	int victim_on_hill;
	int attacker_count;
	struct ae_medal_attacker attackers[AE_MEDAL_RULES_ATTACKERS];
	/* ---- batch 2 (all 0 when the glue knows nothing of them) */
	/* killer to victim, 3D, in world units (0: not known) */
	float distance;
	/* what the killer held in hand (AE_MEDAL_ROLE_*) */
	int killer_role;
	int killer_magazine_empty;
	int killer_seat;
	int killer_vehicle;
	int victim_power_weapon;
	/* the victim held as many weapons as a player may */
	int victim_inventory_full;
	/* the victim's shield was charged past full before the blow */
	int victim_overshield;
	/* the victim drove a vehicle, its speed in world units a second */
	int victim_driving;
	float victim_vehicle_speed;
	int killer_carried_flag;
	/* the victim carried the flag: how far from the stand it scores at (-1: not known) */
	float victim_stand_distance;
	/* the ball carrier's score and the score to win, in the same unit (ticks) */
	long victim_score;
	long victim_score_to_win;
	/* what the records found (ae_medal_history.h) */
	int no_scope;
	int quick_draw;
	int reversal;
	int nade_shot;
	int turned_360;
	int ninja;
	int off_the_rack;
	int perfect;
};

enum
{
	AE_MEDAL_FLAG_GRAB,
	AE_MEDAL_FLAG_RETURN,
	AE_MEDAL_FLAG_CAPTURE,
};

/* a game's end: the players and the standings, for the end-of-game medals */
struct ae_medal_end
{
	/* by slot */
	unsigned char used[AE_MEDAL_RULES_PLAYERS];
	short team[AE_MEDAL_RULES_PLAYERS];
	/* first place (a team game: the player's team is) */
	unsigned char winner[AE_MEDAL_RULES_PLAYERS];
	/* a team game: the leading team, and its lead in points over the next */
	int leading_team;
	int team_lead;
};

/* ---------- code */

/* a new game: nothing counted */
void ae_medal_rules_reset(struct ae_medal_game *game, int mode, int teams, int assault, int ball_type);
/* a slot's player changed: their counts go */
void ae_medal_rules_clear_player(struct ae_medal_game *game, int player);

/* what a damage tag's path says of the weapon (case and slash aside) */
int ae_medal_rules_weapon(char const *path);
/* what a weapon tag's path says of the weapon in hand (AE_MEDAL_ROLE_*; none for a null path) */
int ae_medal_rules_role(char const *path);
/* the weapons that make a kill hard to get back from: the sniper rifle, the rocket launcher, the fuel rod and the plasma
cannon, the sword and the hammer */
int ae_medal_rules_role_is_power(int role);

void ae_medal_rules_kill(struct ae_medal_game *game, struct ae_medal_kill const *kill, struct ae_medal_out *out);
/* CTF: a flag taken, returned or captured by the player (not in Assault, which the caller leaves out) */
void ae_medal_rules_flag(struct ae_medal_game *game, int player, int what, struct ae_medal_out *out);
/* the same, with how far from the opposing stand the returned flag lay (world units; negative: not known) */
void ae_medal_rules_flag_at(struct ae_medal_game *game, int player, int what, float stand_distance,
	struct ae_medal_out *out);
/* King: each tick, each player alive or not: on the hill, and whether an enemy is on it too */
void ae_medal_rules_hill_guardian_tick(struct ae_medal_game *game, int player, int alive, int on_hill, int contested,
	struct ae_medal_out *out);
/* each tick, each player: whether they hold the ball (a normal, reverse tag or juggernaut ball by the game's) */
void ae_medal_rules_ball_tick(struct ae_medal_game *game, int player, int holding, struct ae_medal_out *out);
/* King: each tick, each player alive or not: on the hill, and in a vehicle */
void ae_medal_rules_hill_tick(struct ae_medal_game *game, int player, int alive, int on_hill, int in_vehicle,
	struct ae_medal_out *out);
/* King: the hill moved */
void ae_medal_rules_hill_moved(struct ae_medal_game *game, struct ae_medal_out *out);
/* Race: the player touched a checkpoint at the tick */
void ae_medal_rules_checkpoint(struct ae_medal_game *game, int player, long tick, struct ae_medal_out *out);
/* the game ended */
void ae_medal_rules_game_end(struct ae_medal_game *game, struct ae_medal_end const *end, struct ae_medal_out *out);

#endif /* __AE_MEDAL_RULES_H */
