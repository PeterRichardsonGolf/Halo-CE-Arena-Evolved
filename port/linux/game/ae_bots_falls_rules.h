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
/* A HURT BOT'S LETHAL DROP (0.9.4; Damnation 11-15: a bot fighting on the top level stepped off a drop of 8.1, under the
gametype's full damage distance of 10, and died of the landing): with falling damage on, a landing's damage grows with
its speed, from none at the lower distance to all at the upper one (bipeds.c: the scale is the velocity's, each
velocity sqrt(2 * gravity * distance)); a bot whose body has this fraction left (0 .. 1) dies of the drop whose scale
is that fraction. The lethal drop (ae_bots_edge_lethal_distance's) for that body, never above it: with no falling damage,
no lower and upper distances that make sense (lower above 0.1, upper above lower) or a full body, the lethal drop as given */
float ae_bots_edge_hurt_lethal_distance(float lethal_distance, int no_falling_damage, float lower_distance,
	float upper_distance, float body);
/* A BOT KEEPS ITS LEVEL (0.9.4; owner 11-16 on Prisoner: "they jump down if they get shot a couple times and that is
a death sentence especially if they can pop back into cover; on some of the levels there are rooms"; then, the shot rule
in, "still jumping off top level this would be a death sentence on stock ce even with ae fall damage settings because of
the stun lock when you hit the ground. jumping down on prisoner should be seen the same as falling in pit zone"): a bot
not carrying an objective takes no drop of AE_BOTS_EDGE_KEEP_LEVEL_DROP or more, fighting or not, whatever its shields
(Prisoner's top floor to the ground is 3.6, a landing of about half a second's stun and falling damage from 3.0; its
middle floor 1.8 below the top is still dropped to; a ramp walked down, its probe up to 2.5 units ahead with the
momentum's, drops far less): the guard's lethal drop for it, never above the lethal drop given. The travel graph's drop
links (out of a piece no link leaves, into one no link reaches) are taken as before. */
#define AE_BOTS_EDGE_KEEP_LEVEL_DROP 2.5f
float ae_bots_edge_keep_level_lethal(float lethal_distance, int carrying);
/* THE LANDING'S RING (0.9.4; Damnation 11-15: bots stepped off the upper level onto the floor hole's rim, a drop of 3.3,
and their speed took them into the hole beside the line the guard probed beyond the landing along): a big drop's landing
is probed round about too, AE_BOTS_EDGE_RING_POINTS points AE_BOTS_EDGE_RING_RADIUS from it (point 0 along the step,
then each an eighth turn on). The landing is lethal when one of the points within an eighth turn of the step's way
(ae_bots_edge_ring_forward: where its speed takes it) is, or AE_BOTS_EDGE_RING_MOST of all of them (a ledge with a drop
on one side keeps its landing) */
#define AE_BOTS_EDGE_RING_POINTS 8
#define AE_BOTS_EDGE_RING_RADIUS 0.75f
#define AE_BOTS_EDGE_RING_MOST 3
int ae_bots_edge_ring_forward(int point);
int ae_bots_edge_ring_lethal(int lethal_points, int forward_lethal);
/* A LINK JUMP'S RUN-UP (0.9.4; Damnation 11-15: a bot at a jump's entry on the upper level walked back to the start of
its run-up, which lay over the ledge, and fell): a step at a link jump's entry (not yet in the air) that is not along the
jump's way (the cosine of the angle between the two under AE_BOTS_EDGE_LINK_ALONG) is guarded as any step; the jump's
own way, over its gap, is not */
#define AE_BOTS_EDGE_LINK_ALONG 0.7f
int ae_bots_edge_link_step_checked(float heading_cosine);
/* DEADLY VOLUMES (0.9.4; Fractured Peak 11-15: the map's script kills a unit in a trigger volume under the cracks, 1.25
to 3 units under the floor; 43 bots stepped into a crack 3 deep, a drop the guard took for nothing, and died): a trigger
volume a unit was killed in by the map (an act of god: no owner, at once) is DEADLY once AE_BOTS_FALLS_VOLUME_KILLS such
kills, at least AE_BOTS_FALLS_VOLUME_KILL_APART ticks apart (a round's reset, a bomb's blast kill many at once; review
0.9.4: a telefrag, a reset, a seat's occupants killed with their vehicle are acts of god too), were in it in one game,
of units not riding and with no other living unit in it then (ae_bots_falls_volume_learn; the smallest such volume a
death is in counts it); a drop onto a floor is lethal when a unit's middle, AE_BOTS_EDGE_VOLUME_MIDDLE above
its feet, would be in a deadly volume anywhere on the way down. The heights tested, from the floor's up past the feet,
every AE_BOTS_EDGE_VOLUME_STEP, at most `maximum` (ae_bots_edge_volume_heights: how many). A deadly volume a living bot
stays in for AE_BOTS_FALLS_VOLUME_ALIVE_TICKS is not one (ae_bots_falls_volume_cleared) */
#define AE_BOTS_EDGE_VOLUME_MIDDLE 0.5f
#define AE_BOTS_EDGE_VOLUME_STEP 1.0f
#define AE_BOTS_FALLS_VOLUME_ALIVE_TICKS 90
int ae_bots_edge_volume_heights(float feet_z, float floor_z, float *heights, int maximum);
/* (0.9.4 freeze review: 2 kills poisoned a volume too easily, a round's reset or a spawn's telefrag twice; now 3 kills of
different players, none while a round's countdown runs) */
#define AE_BOTS_FALLS_VOLUME_KILLS 3
/* whether this victim (its player's index) is not among the volume's counted ones (victims, count of them): 1 */
int ae_bots_falls_volume_victim_new(long const *victims, int count, long victim);
#define AE_BOTS_FALLS_VOLUME_KILL_APART 30
/* whether a kill counts towards a volume's: its first, or one this many ticks after the last counted */
int ae_bots_falls_volume_kill_counts(int first, long since_last_ticks);
int ae_bots_falls_volume_learn(int owner_none, int kill_instantly, int others_alive_inside, int riding, int kills);
int ae_bots_falls_volume_cleared(long alive_inside_ticks);

/* GRENADES: a live grenade's danger radius (world units; CE's frag and plasma blasts reach about 5) */
#define AE_BOTS_GRENADE_DANGER 6.0f
/* whether the bot runs from a live grenade this far off: its own (it threw it) at EASY and up (RECRUIT throws none) */
int ae_bots_grenade_flee(int level, int own, float distance);
/* (owner 2026-11-11: "bots do not evade grenades anymore i see them walk into them": only their own were fled) someone
else's live grenade (an enemy's or a teammate's): noticed within AE_BOTS_GRENADE_NOTICE, judged once a grenade by the
level's dodge chance (ae_bots_dodge_chance: a roll in 0..1 under it), and fled within the danger once it has been
noticed the level's dodge reaction (ae_bots_dodge_reaction); RECRUIT never */
#define AE_BOTS_GRENADE_NOTICE 9.0f
int ae_bots_grenade_judge(int level, float roll);
int ae_bots_grenade_flee_other(int level, float noticed_seconds, int judged_flee, float distance);
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

/* DODGING (ae_bots_ut.inc): a bot's step out of a slow shot's or a vehicle's way. The level's least warning (seconds) and
its chance to act on a threat it could. A pass an ENEMY's vehicle makes at the bot is judged again every
AE_BOTS_DODGE_REROLL_TICKS while it closes (owner 10-31: bots stopped getting out of a ghost's way; one roll a pass left
EASY at 25 % and NORMAL at 45 % however near it came), each later roll at half the level's chance, while the warning
holds (RECRUIT never). */
#define AE_BOTS_DODGE_REROLL_TICKS 10
float ae_bots_dodge_reaction(int level);
float ae_bots_dodge_chance(int level);
float ae_bots_dodge_reroll_chance(int level);
/* a threat judged and not dodged is rolled again: only a vehicle's, once a roll's interval has passed */
int ae_bots_dodge_reroll_due(int vehicle, int judged_dodge, long now, long last_roll_tick);

/* MOVEMENT PRIORITY: an imminent dodge, and a recovery from being stuck, are the last word on a bot's walk over the
tactical roles (flank, hold, suppress: ae_bots_fear.inc); the edge guard after them keeps its veto at a lethal drop. Nonzero:
the roles may move the bot this tick. A role set free (until a tick) stands off the same way. */
int ae_bots_tactical_move_allowed(int dodging, long now, long unstick_until_tick, long free_until_tick);

#endif
