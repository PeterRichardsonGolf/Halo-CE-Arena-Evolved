/*
AE_BOTS_DEFEND_RULES.H

Arena Evolved: a fireteam told DEFENDING HERE, and a bot squad whose role is to defend, hold the point. They take spaced cover
around it, each watching a way in, fight from there without chasing, keep moving where there is no cover, and fall back together
when it goes badly, regroup, and come back. The engine side is ae_bots_defend.inc (the places are the cover scan's walls and
roof, the ways are the map's own geometry); this file is the pure numbers (port/linux/tests/ae_bots_defend_rules_test.c). Levels
as ae_bots_combat_rules.h's.

- PROFILE: the largest team of the game, humans and bots together, five or fewer is an ARENA game, six or more a BTB game.
  ARENA holds a smaller area closer together; BTB a wider one, also blocks vehicles and weighs long lines more.
- SCOPE: a leader's own DEFENDING HERE order holds in both. A role (the mode's default for a bot squad) holds in BTB only
  (flag stand, the bomb sites' defenders, the hill); in an ARENA game the roles are left exactly as they were.
- PLACE: inside the area, walls toward where the enemy comes from count three times the others, a roof counts, a view back
  onto its way in counts; closer to the point is better. Open ground is no place: a defender there keeps moving.
- EXPOSURE: 0 to 70; a defender moves to another place above 30 and is settled again below 20, and not before it has been
  there a moment. A place just chosen is held a few seconds against the other goals (sticky).
- MORALE: 0 (calm) to 1 (broken) per fireteam; members down, being outnumbered and a dead leader raise it, time lowers it.
  A human-led fireteam falls back only with its leader dead AND morale broken; a bot-only squad when morale breaks. They
  regroup at the fall-back spot first, then go back to the anchor when morale is calm (and, human-led, the leader lives;
  bot-only, enough of the squad is back).
*/

#ifndef __AE_BOTS_DEFEND_RULES_H
#define __AE_BOTS_DEFEND_RULES_H

#include "ae_fireteam_rules.h"

enum { AE_DEFEND_ARENA, AE_DEFEND_BTB };

/* the profile of a game from its largest team (humans and bots) */
int ae_defend_profile(int largest_team);

struct ae_defend_numbers
{
	float radius;			/* the area held around the point, wu */
	float spacing;			/* defenders' places kept this far apart, wu */
	float leash;			/* a dodge or a line broken may take a defender this far past the area, wu */
	float morale_decay_seconds;	/* morale falls by 1/e in this time */
	float sniper_weight;		/* how much a long line to the defender counts */
	int vehicle_block;		/* no place a vehicle can drive onto */
	long jink_min_ticks, jink_max_ticks;	/* a defender moving in the open changes direction this often */
};

void ae_defend_numbers_of(int profile, struct ae_defend_numbers *out);

/* whether the hold applies: switch_on the feature switch, explicit_order the fireteam's order is its leader's own, order_kind
the standing order, role the squad's AE_ROLE_* */
int ae_defend_in_scope(int switch_on, int profile, int explicit_order, short order_kind, int role);

/* the sides (AE_BOTS_COVER_SIDES' eight, side n at n * 45 degrees) that face the threats: each yaw's nearest side and its
two neighbours */
unsigned ae_defend_threat_sides(float const *threat_yaws, int count);

/* a place's score (0 or less: unusable): walls the eight sides' bitmask, roof, the threat sides, the place's distance from
the point and the area's radius, sees_way whether it has a line back onto its way in */
float ae_defend_place_score(unsigned walls, int roof, unsigned threat_sides, float point_distance, float radius, int sees_way);

/* whether a place is far enough from the nearest other defender's (negative: there is none) */
int ae_defend_spaced(float nearest_other_place, float spacing);

/* whether a defender is inside the area and its leash */
int ae_defend_within_leash(float anchor_distance, float radius, float leash);

/* ---------- hold */

/* a defender's exposure, 0 to 70: the enemies that see it (seen_by) and those of them near, hurt just now, open ground
toward the one that sees it */
float ae_defend_exposure(int seen_by, int seen_by_near, int hurt, int open_toward_seer);
/* whether a defender should move to another place: was_exposed (it was last tick), the exposure, the ticks it has been at
its place (it settles first) and its level. 1: move */
int ae_defend_exposed_step(int was_exposed, float exposure, long held_ticks, int level);
/* a place's score kept up for a few seconds once chosen (30 ticks a second): the other goals must beat this */
float ae_defend_sticky(float hold_score, long since_chosen_ticks, int level);
/* ticks to the next change of direction of a defender moving in the open; chance 0 to 1 */
long ae_defend_jink_ticks(int profile, float chance);
/* in the open (no cover toward the enemy that sees it): keep moving (JINK) or move to break the line it is shot along */
enum { AE_DEFEND_OPEN_JINK, AE_DEFEND_OPEN_BREAK_LINE };
int ae_defend_open_action(int profile, int shot_from_range, int line_breaker_known);

/* ---------- morale and the phases */

/* morale after seconds: it falls by its decay and rises with the members down, the enemies more than the fireteam near the
anchor (outnumbered_by) and a dead leader; 0 calm, 1 broken */
float ae_defend_morale_step(float morale, int members_down, int outnumbered_by, int leader_dead, float seconds,
	float decay_seconds);
#define AE_DEFEND_MORALE_BREAK 0.6f
#define AE_DEFEND_MORALE_SAFE 0.25f

enum { AE_DEFEND_HOLD, AE_DEFEND_FALL_BACK, AE_DEFEND_REGROUP, AE_DEFEND_RETURN };
/* whether a member counts as down: it was seen alive this round and is not now (one not yet spawned after a reset or the game's
start is not down) */
int ae_defend_member_down(int seen_alive, int alive_now);

/* the fireteam's phase next: at_fallback every living member that matters is at the fall-back spot, all_regrouped every
living member is within AE_FIRETEAM_REFORM_RADIUS of it, living of squad members alive, back_at_anchor the members are at
the anchor again */
int ae_defend_phase_step(int phase, int human_led, int leader_alive, float morale, int at_fallback, int all_regrouped,
	int living, int squad, int back_at_anchor);

/* whether a role squad's hold gives way to the objective now: its own flag dropped, the enemy carrying it in sight, a bomb to
defuse. Only a role's hold (role_hold) does */
int ae_defend_objective_break(int role_hold, int own_flag_dropped, int enemy_has_own_flag_in_sight, int bomb_to_defuse);

#endif
