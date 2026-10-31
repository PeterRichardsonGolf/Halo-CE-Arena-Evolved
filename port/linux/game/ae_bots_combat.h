/*
AE_BOTS_COMBAT.H

Bots that fight like people of their level (notes/specs/2026-10-16-bots-infinite.md area 3): the engine's side of
ae_bots_combat_rules.c's rules. bots.c's fight, look and walk call it through its AE hook lines and the glue of
ae_bots_combat.inc (included at bots.c's end by ae_bots_lan.inc, where bots.c's own state is visible); the levels'
names and the two settings are ae_bots_skill.inc's. Included after cseries.h.

- SHOOTING SKILL (bots.shooting_skill) and MOVEMENT SKILL (bots.movement_skill): "same" (BOT SKILL: as BOT SKILL, the
  default) or a level of their own. Shooting is the aim, the reaction, how far and wide a bot sees, its grenades,
  its blows and its headshots; movement its strafing, jumping, crouching, closing in and retreating. BOT SKILL
  itself keeps what neither names: how often a roam heads for an enemy, the shared sightings, the halo.log line.
- Each minute of a game with bots that fight: "bots: combat ..." in halo.log, the throws, blows, headshot aims,
  retreats, charges, jumps, halts, shared sightings and friendly-fire holds since the game began; with debug.bot_events the fight log
  (ae_bots_events.inc) gains ev=grenade, ev=retreat, ev=charge and ev=share lines.
*/

#ifndef __AE_BOTS_COMBAT_H
#define __AE_BOTS_COMBAT_H

#include "ae_bots_combat_rules.h"

/* the level (AE_BOTS_LEVEL_*) bots.shooting_skill (movement FALSE) or bots.movement_skill (TRUE) sets, or -1 for
BOT SKILL (read again when the settings change) */
int ae_bots_combat_setting_level(boolean movement);

/* whether a weapon definition kills with a shot to the head once the shields are down (the pistol, the sniper
rifle, the pack's battle rifle): a bot aims at the head with it */
boolean ae_bots_combat_precision_weapon(long definition_index);

/* whether bots zoom a weapon definition at range though it is as true unzoomed (SCOPED RIFLE,
ae_bots_combat_rules.h): the battle rifle pack's and the carbine pack's, whose aim assist reaches 20 and 17 units
unzoomed and the zoom's times that zoomed */
boolean ae_bots_combat_scoped_rifle(long definition_index);

/* the speed a unit's grenade leaves its hand (its definition's), world units a tick */
real ae_bots_combat_grenade_speed(long unit_index);

/* whether a unit has its back to a point (it faces more than 114.6 degrees away from it) */
boolean ae_bots_combat_back_turned(long unit_index, real_point3d const *point);

/* a bot's own state for the fight, by its index in bots.c (forgotten when its unit changes: a new life or game) */
struct ae_bots_combat_bot
{
	boolean known;
	long unit_index;
	struct ae_bots_combat_retreat retreat;
	/* the strafe's end it last saw (a new one: a halt, maybe), and the halt's end */
	long last_strafe_until;
	long halt_until;
	/* the fight's last tick (a new fight after 2 s without one), the next jump in it */
	long last_fight_tick;
	long next_vertical_tick;
	/* FAR SHOTS: on target until this tick, or just off the body */
	long far_shot_until;
	boolean far_shot_on_target;
	/* the tick it may strike, waiting within reach (NONE: not waiting) */
	long melee_ready_tick;
	/* a grenade's throw: the aim leads the target's path until then, by this much */
	long throw_until;
	real throw_lead;
	/* the last tick it told its teammates of an enemy */
	long share_tick;
	boolean charging;
	/* it aims at its target's head (counted once a time it starts) */
	boolean head_aim;
	/* DESCOPE REFLEX: no zoom (and past 10 units no shot) until then, after a hit knocked the zoom out */
	long rescope_until;
	/* PRECISION PACING: the tick it began holding a shot for the bloom to settle (NONE: not holding) */
	long pace_since;
	/* GRENADE AT THE LAST-SEEN SPOT: the sighting it last weighed one for, the throw's aim held until then, at this */
	long prenade_seen_tick;
	long prenade_until;
	real_point3d prenade_point;
	boolean friendly_fire_held;
	boolean friendly_grenade_held;
};
/* every bot starts over (a new map) */
void ae_bots_combat_reset(void);
/* debug.bot_trace (0: off), read again when the settings change: every bot lane's trace lines */
long ae_bots_trace_level(void);
struct ae_bots_combat_bot *ae_bots_combat_bot_state(long bot_index, long unit_index);

/* the counters of the minute line */
enum
{
	AE_BOTS_COMBAT_GRENADES,
	AE_BOTS_COMBAT_MELEES,
	AE_BOTS_COMBAT_MELEES_DECLINED,
	AE_BOTS_COMBAT_HEAD_AIMS,
	AE_BOTS_COMBAT_RETREATS,
	AE_BOTS_COMBAT_CHARGES,
	AE_BOTS_COMBAT_JUMPS,
	AE_BOTS_COMBAT_HALTS,
	AE_BOTS_COMBAT_SHARES,
	AE_BOTS_COMBAT_PACED,
	AE_BOTS_COMBAT_RESCOPES,
	AE_BOTS_COMBAT_PRENADES,
	AE_BOTS_COMBAT_HIGH_GROUND,
	AE_BOTS_COMBAT_OUT_OF_VERTICAL_VIEW,
	AE_BOTS_COMBAT_HEADS_SEEN,
	AE_BOTS_COMBAT_FRIENDLY_FIRE_HELD,
	AE_BOTS_COMBAT_FRIENDLY_GRENADES_HELD,
	AE_BOTS_COMBAT_COUNTERS
};
void ae_bots_combat_count(int counter);
/* halo.log's "bots: combat" line, a minute into the game and each minute on (now: the game's tick) */
void ae_bots_combat_report(long now, char const *shooting, char const *movement);
/* (debug.bot_events) one line of the bots' fight log, in ae_bots_events.inc's words */
void ae_bots_combat_event(long bot_index, long now, char const *skill_name, char const *event, long target,
	real distance, char const *extra);

#endif
