/*
AE_BOTS_FALLS_RULES.H

Arena Evolved: the bots' deaths by their own hand, as pure rules (port/linux/tests/ae_bots_falls_rules_test.c runs
them). The owner, 2026-10-20: "can you have a helper work on tuning bots for suicide and fall deaths. i find that is
what makes our bots still look like bots."

- THE CAUSE OF A DEATH (the fight log's ev=death, ae_bots_events.inc), from the last damage the bot's unit took: its
  damage effect's tag name, its owner and whether it killed at once. AE's gametypes have NO FALLING DAMAGE, so a
  landing never hurts there; a fall past the globals' maximum falling distance still kills (bipeds.c, the "distance"
  damage while falling faster than its speed), and so does the map's own kill (unit_kill, an act of god: a script's
  kill volume, a telefrag), dealt as the "falling" damage with kill instantly and no owner.
- THE EDGE (ae_bots_falls.inc's guard, each tick, after every other part has chosen where the bot goes): the ground a
  step ahead is probed with a ray down; a drop the bot would not live through (or no floor at all) is not stepped
  into: it goes another way (the other side first in a fight), or stands. A jump near such a drop is not made. Levels
  as ae_bots_combat_rules.h's (the bot's MOVEMENT SKILL): RECRUIT and EASY may still slip in a fight, as children do;
  NORMAL and up look every tick, further ahead each level.
- GRENADES: a bot runs from its own live grenade (it knows where it threw it); a throw at a wall a few units ahead
  (it bounces back) or at a target within the blast is not made.
*/

#ifndef __AE_BOTS_FALLS_RULES_H
#define __AE_BOTS_FALLS_RULES_H

enum
{
	/* no damage known, or none of the below (a vehicle's end, a corpse's) */
	AE_BOTS_DEATH_OTHER = 0,
	/* another player's damage */
	AE_BOTS_DEATH_PLAYER,
	/* a landing's damage (gametypes with falling damage) */
	AE_BOTS_DEATH_FALL,
	/* a fall past the maximum falling distance: off a roof, into a pit, out of the map */
	AE_BOTS_DEATH_PIT,
	/* the map's kill: a kill volume or death barrier, a telefrag */
	AE_BOTS_DEATH_BARRIER,
	/* its own grenade */
	AE_BOTS_DEATH_OWN_GRENADE,
	/* its own rocket, fuel rod or other explosion */
	AE_BOTS_DEATH_OWN_EXPLOSIVE,
	/* its own damage of another kind */
	AE_BOTS_DEATH_SELF,
	AE_BOTS_DEATH_CAUSES
};

/* the cause's word in the fight log */
char const *ae_bots_death_cause_name(int cause);

/* the cause of a death from its last damage: known (any damage seen since the unit spawned), the damage effect's tag
name (NULL for none), whether the damage owner is the bot's own player, whether it is no player, whether the damage
killed at once (an act of god), and whether the unit rode a vehicle */
int ae_bots_death_cause(int known, char const *damage_name, int owner_self, int owner_none, int kill_instantly,
	int riding);

/* THE EDGE: how far ahead the bot probes the ground, world units: RECRUIT 0.7, EASY 0.8, NORMAL 1.0, HEROIC 1.2,
LEGENDARY and DEMON 1.4 (a running Spartan covers about 0.075 a tick) */
float ae_bots_edge_lookahead(int level);
/* the chance the bot looks at the edge in a half second: always while it walks; in a fight RECRUIT 0.8, EASY 0.85,
NORMAL and up 1 (Fallout 10-20: RECRUIT at 0.6 still had 22 of its 32 deaths by falls, EASY at 0.85 2 of 30) */
float ae_bots_edge_attention(int level, int fighting);
/* the drop that kills: with no falling damage (AE's gametypes) the globals' maximum falling distance (a fall past it
kills in the air: bipeds.c), else the lesser of it and the distance a landing does its full damage; 15 units when the
globals give none */
float ae_bots_edge_lethal_distance(int no_falling_damage, float maximum_distance, float full_damage_distance);
/* whether a step is lethal: no floor found within the probe, or one this far below the bot's feet (nine tenths of
the lethal distance: a slope's run, the jump's height) */
int ae_bots_edge_lethal(int floor_found, float drop, float lethal_distance);
/* the probe ahead of a jump: about where a running jump lands */
#define AE_BOTS_EDGE_JUMP_REACH 2.2f
/* the ways tried when the way ahead is lethal, turned from it (radians), attempt 0 .. AE_BOTS_EDGE_TURNS - 1: in a
fight a quarter turn to the given side first (side 1 or -1), then the other, then an eighth either way, three eighths,
back; walking, back first, then three eighths, a quarter, an eighth (along the edge last: Cold Fissure 10-20, walking
bots turned along the brink each tick drifted over a corner of it) */
#define AE_BOTS_EDGE_TURNS 7
float ae_bots_edge_turn(int attempt, int side, int walking);
/* a SMALL DROP TO A LOWER GOAL: whether the guard lets a step go over a ledge whose landing is not lethal (the landing
beyond it is not probed): the drop (world units, the floor under the step) at most AE_BOTS_EDGE_SMALL_DROP, the place
the bot walks to (its next path point, else its goal) at least AE_BOTS_EDGE_TARGET_BELOW under its feet, and the step
towards it (the cosine of the angle between the step's way and the way to that place) at least 0.5. Weld 10-28: a
bot on a raised spot, its goal 1.6 units below, was turned back at the ledge for 75 seconds. */
#define AE_BOTS_EDGE_SMALL_DROP 2.0f
#define AE_BOTS_EDGE_TARGET_BELOW 0.5f
int ae_bots_edge_drop_allowed(float drop, float target_below, float heading_cosine);
/* the ticks a walking bot stands blocked at an edge before it gives up its goal (its path goes over the edge) */
#define AE_BOTS_EDGE_GIVE_UP_TICKS 30

/* GRENADES: a live grenade's danger radius (world units; CE's frag and plasma blasts reach about 5) */
#define AE_BOTS_GRENADE_DANGER 6.0f
/* whether the bot runs from a live grenade this far off: its own (it threw it) at EASY and up (RECRUIT throws none) */
int ae_bots_grenade_flee(int level, int own, float distance);
/* whether a throw is safe: the first wall along the throw this far off (a large number for none), its target this far
(a large number for none). Not at a wall within AE_BOTS_GRENADE_WALL (it bounces back), nor at a target within
AE_BOTS_GRENADE_BLAST, nor into a wall well short of the target and within AE_BOTS_GRENADE_SHORT (Fallout 10-20: frags
thrown at targets 10 to 15 units off, over a roof's parapet, came back and killed the thrower) */
int ae_bots_grenade_throw_safe(float wall_distance, float target_distance);
#define AE_BOTS_GRENADE_WALL 4.0f
#define AE_BOTS_GRENADE_BLAST 5.0f
#define AE_BOTS_GRENADE_SHORT 10.0f
/* (how far short of the target a wall is a wall in the way, not the target's own cover) */
#define AE_BOTS_GRENADE_SHORT_MARGIN 3.0f
/* EXPLOSIVES (the rocket launcher, the fuel rod gun): whether a shot is safe, the first wall along the aim and the target
this far: NORMAL and up never fire one nearer than their level's distance (Fallout 10-20: LEGENDARY bots killed
themselves with rockets at targets 1.4 and 4.2 units off; owner 10-20 after Octagon and Damnation, the bots "really
struggling with the rockets, just getting the shots off" when it was the whole blast: NORMAL 4 units, HEROIC 3.5,
LEGENDARY and DEMON 3, a hit's splash taken on the shields as a player takes it); RECRUIT and EASY may, as children do */
int ae_bots_explosive_fire_safe(int level, float wall_distance, float target_distance);
/* the nearest the level fires an explosive at (0: any) */
float ae_bots_explosive_nearest(int level);
#define AE_BOTS_EXPLOSIVE_BLAST 5.0f

#endif
