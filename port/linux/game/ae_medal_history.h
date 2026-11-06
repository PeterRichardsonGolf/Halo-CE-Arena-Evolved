/*
AE_MEDAL_HISTORY.H

MEDALS (ae_medals.c): the records the host keeps of who fired, who hurt whom, how a player moved and what they picked up, and the
rules that read them, as pure code (port/linux/tests/ae_medal_history_test.c runs them). No engine headers: ae_medals.c fills the
records as the game goes and asks the questions at a kill. Times are game ticks, 30 a second; players are slots (the absolute
player index), -1 for none.

- A SHOT RING for each player: the tick, the weapon, whether the player was zoomed in, how many projectiles the round made.
- A DAMAGE LOG for each player's life: who hurt them, when, with which damage, how much of the shield and of the body it took.
  It starts over when the player dies.
- A RING OF STEPS for each player, a sample a tick: which way they faced, where they were, whether they were in the air.
- The PICKUP LINK: the weapons the map's spawners made, and which player took which of them when.

The questions: NO SCOPE, QUICK DRAW, REVERSAL, NADE SHOT, a turn of 360 (TURNED), the NINJA, OFF THE RACK and PERFECT.
*/

#ifndef __AE_MEDAL_HISTORY_H
#define __AE_MEDAL_HISTORY_H

#include "ae_medal_rules.h"

enum
{
	AE_HISTORY_SHOTS = 32,
	AE_HISTORY_HITS = 48,
	AE_HISTORY_SAMPLES = 96,
	AE_HISTORY_SPAWNED = 64,
	AE_HISTORY_PICKUPS = 4,
};

/* the windows (ticks) and numbers of the questions */
enum
{
	/* NO SCOPE: the sniper shot that killed was the player's last one this recent */
	AE_HISTORY_NO_SCOPE_TICKS = 60,
	/* QUICK DRAW: the other weapon's last damage was this recent, the killer's share (percent) of the victim, at least */
	AE_HISTORY_QUICK_DRAW_TICKS = 45,
	AE_HISTORY_KILLER_PERCENT = 95,
	/* REVERSAL: the victim struck first by this long at the most, and had done this share (percent) of the killer by then */
	AE_HISTORY_REVERSAL_TICKS = 27,
	AE_HISTORY_REVERSAL_PERCENT = 25,
	/* NADE SHOT: the longest gap between the grenade's last damage and the gun's first */
	AE_HISTORY_NADE_GAP_TICKS = 60,
	/* 360: the window of the turn (2.5 s) */
	AE_HISTORY_TURN_TICKS = 75,
	/* NINJA: the kill is this soon after the landing (0.5 s) */
	AE_HISTORY_NINJA_LANDED_TICKS = 15,
	/* OFF THE RACK: the kill is this soon after the pickup (2.75 s) */
	AE_HISTORY_OFF_THE_RACK_TICKS = 82,
	/* PERFECT: the rounds count from this long before the first damage; a round's hit comes this soon after it was fired
	at the most (more for a player on another machine); damage from other hands of this share (percent) of the victim's
	whole vitality is forgiven; the allowance on the fastest time is half again and half a second */
	AE_HISTORY_PERFECT_BEFORE_TICKS = 8,
	AE_HISTORY_PERFECT_FLIGHT_TICKS = 8,
	AE_HISTORY_PERFECT_REMOTE_TICKS = 4,
	AE_HISTORY_PERFECT_CHIP_PERCENT = 10,
	AE_HISTORY_PERFECT_PULL_GAP_TICKS = 4,
	AE_HISTORY_PERFECT_TIME_EXTRA_TICKS = 15,
};

/* 360: the turn needed, in degrees; NINJA: the sideways distance and the height band over the victim (world units) */
#define AE_HISTORY_TURN_DEGREES 288.0f
#define AE_HISTORY_NINJA_SIDE 0.16f
#define AE_HISTORY_NINJA_HEIGHT 1.3f

struct ae_history_shot
{
	long tick;
	long weapon;
	short role;
	unsigned char zoomed;
	/* the projectiles the round made */
	unsigned char pellets;
};

/* one blow that landed on a player */
struct ae_history_hit
{
	long tick;
	/* the attacker's slot, -1 for none (a fall, a vehicle's, the world) */
	int attacker;
	/* the damage tag (a number the glue gives it), its category, and what its path says of the weapon
	(AE_MEDAL_WEAPON_*) */
	long tag;
	short category;
	short kind;
	unsigned char head;
	/* the part of the shield, of the body and of the whole vitality (the two weighed by their sizes) the blow took */
	float shield;
	float body;
	float taken;
};

struct ae_history_step
{
	long tick;
	/* which way the player faced, radians */
	float yaw;
	float x, y, z;
	unsigned char airborne;
};

struct ae_history_pickup
{
	long weapon;
	long tick;
};

struct ae_history_life
{
	int count;
	/* more blows came than the log holds: the life is no one's to read */
	int overflow;
	/* the shield and the body, in parts of their full, when the first blow came */
	float start_shield;
	float start_body;
	struct ae_history_hit hits[AE_HISTORY_HITS];
};

struct ae_history_player
{
	int shot_count;
	int shot_next;
	struct ae_history_shot shots[AE_HISTORY_SHOTS];
	struct ae_history_life life;
	int step_count;
	int step_next;
	struct ae_history_step steps[AE_HISTORY_SAMPLES];
	int pickup_next;
	struct ae_history_pickup pickups[AE_HISTORY_PICKUPS];
};

struct ae_history
{
	struct ae_history_player players[AE_MEDAL_RULES_PLAYERS];
	int spawned_next;
	long spawned[AE_HISTORY_SPAWNED];
};

/* ---------- records */

void ae_history_reset(struct ae_history *history);
/* a slot's player changed: their records go */
void ae_history_clear_player(struct ae_history *history, int player);

void ae_history_shot(struct ae_history *history, int player, long tick, long weapon, int role, int zoomed, int pellets);
/* a blow on the victim; before_shield and before_body: their parts of full just before it. The first blow of a life
sets the life's start */
void ae_history_hit(struct ae_history *history, int victim, struct ae_history_hit const *hit, float before_shield,
	float before_body);
/* the victim's life ended (after the kill was read) */
void ae_history_life_end(struct ae_history *history, int victim);
void ae_history_step(struct ae_history *history, int player, long tick, float yaw, float x, float y, float z, int airborne);
/* a new life: the steps of the last are no part of it */
void ae_history_steps_clear(struct ae_history *history, int player);
/* the map's spawner made this weapon object; a player took it (it is from the spawner only once) */
void ae_history_spawned(struct ae_history *history, long weapon);
void ae_history_picked(struct ae_history *history, int player, long weapon, long tick);

/* ---------- questions */

/* the share of the victim's whole vitality the attacker took in this life, 0 to 1 and over */
float ae_history_taken_by(struct ae_history const *history, int victim, int attacker);

/* NO SCOPE: the killer's last round with the sniper rifle was not fired zoomed in (and was this recent) */
int ae_history_no_scope(struct ae_history const *history, int killer, long tick);
/* QUICK DRAW: a pistol's kill (not melee) with at least 95 percent of the victim, after damage from another weapon (not melee)
the last of which was this recent */
int ae_history_quick_draw(struct ae_history const *history, int killer, int victim, long tick, long killing_tag,
	int killer_role, int category);
/* REVERSAL: the victim struck the killer first, by 0.9 s at the most before the killer first struck them, and had done a quarter
of the killer's vitality by then; the killer did at least 95 percent */
int ae_history_reversal(struct ae_history const *history, int killer, int victim);
/* NADE SHOT: a headshot death; the killer did at least 95 percent; with a grenade and one gun's damage only, the grenade's
first, and no more than 2 s between the grenade's last and the gun's first */
int ae_history_nade_shot(struct ae_history const *history, int killer, int victim, int headshot);
/* 360: the player's turns over the last 2.5 s add up to 288 degrees or more, either way. They start over after one */
int ae_history_turned(struct ae_history *history, int player, long tick);
/* NINJA: the killer leapt over the victim from in front and killed from behind within 0.5 s of landing. The victim's place and
the way they face (a vector on the ground) at the kill */
int ae_history_ninja(struct ae_history const *history, int killer, long tick, float victim_x, float victim_y, float victim_z,
	float victim_forward_x, float victim_forward_y);
/* OFF THE RACK: the killer took the weapon in hand from a spawner, 2.75 s ago at the most */
int ae_history_off_the_rack(struct ae_history const *history, int killer, long weapon, long tick);

/* ---------- PERFECT */

struct ae_perfect_damage
{
	/* the damage of a round at full, the shield's size, the head's shield multiplier, the damage tag's modifier for the
	shield's material, the share of a round that leaks past the shield, the head's body multiplier */
	float damage;
	float shield_max;
	float shield_multiplier;
	float shield_modifier;
	float leak;
	float body_multiplier;
};

/* the rounds a head kill needs from a full shield and body, with the game's own float arithmetic: the round after the shield
is gone, or the one that breaks it with something left over (the head takes all of that). 0 when the rule cannot say (a round
that leaks to the body, a number that is not one) */
int ae_perfect_required_hits(struct ae_perfect_damage const *damage);

struct ae_perfect
{
	int killer;
	int victim;
	long tick;
	/* the rounds a head kill takes with this weapon (0: none for it) and the least time they take, in ticks (negative: not
	known; none for a single round is fine) */
	int required;
	/* the most rounds that still count (a burst rifle's kill round may be any of the last burst's; 0: the same as required) and
	the pulls of the trigger they come in (0: not counted); rounds of a pull go out within AE_HISTORY_PERFECT_PULL_GAP_TICKS */
	int required_max;
	int pulls;
	long fastest_ticks;
	/* the killer's weapon (a number the glue gives it), and how long a round takes to hit at the most, in ticks: each hit is
	taken to come from a round fired that long before it or less. Rounds fired after the killing round (the rest of a burst,
	one in the air while the kill's report travelled) are not misses */
	long weapon;
	long flight_ticks;
	/* the game's damage multiplier is 1 */
	int multiplier_ok;
	/* the killing damage can cause headshots, is not a sniper's, and the blow was to the head */
	int head_kill;
	int sniper;
};

/* the kill was perfect (the debug line: how many blows and rounds it found, in *hits and *shots; either may be NULL) */
int ae_history_perfect(struct ae_history const *history, struct ae_perfect const *perfect, int *hits, int *shots);

#endif /* __AE_MEDAL_HISTORY_H */
