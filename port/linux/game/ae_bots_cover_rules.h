/*
AE_BOTS_COVER_RULES.H

Arena Evolved: an objective carrier hides and holds (owner 2026-10-25, his Zero Hour Oddball recording: "bots are going
out in the middle of the map with skull and not hiding in covered places and moving when appropriate"). Everything is
found from the map's own geometry as the game runs (ae_bots_cover.inc): no spot lists, no per-map data, any map works.
Pure numbers only (port/linux/tests/ae_bots_cover_rules_test.c). Levels as ae_bots_combat_rules.h's.

- COVER: a place on a walkable surface, its eight sides probed at a biped's chest (AE_BOTS_COVER_WALL_REACH) and once
  up (AE_BOTS_COVER_ROOF_REACH). Walls on two sides or more, or a roof, make it cover; a corner (two walls side by side)
  and an alcove (five sides or more) score more.
- HIDE SPOT: among the cover places near the carrier, the one the enemies it knows of (in sight, on its motion tracker,
  last seen by it or a teammate) do not see, far from them, near its teammates, not the way through them, close enough
  to reach. The cheap score ranks them; the best few (more at the higher levels) are then tried with the enemies' lines
  of sight, the bots' own sight test.
- HOLD: there, it stays (crouched, looking out its open side) until an enemy it knows of is near, an enemy sees it, it
  is hurt, or its hold runs out; then the next spot, away from the threat. A camouflaged carrier holds more boldly (its
  enemies see it at a short reach, as bots.c's own sight of camouflage). RECRUIT and EASY hide worse: fewer spots tried,
  more chance in the pick, shorter holds, slower to notice they are seen.
*/

#ifndef __AE_BOTS_COVER_RULES_H
#define __AE_BOTS_COVER_RULES_H

/* the probes: eight sides at a biped's chest, this far; up, this far */
#define AE_BOTS_COVER_SIDES 8
#define AE_BOTS_COVER_WALL_REACH 1.5f
#define AE_BOTS_COVER_ROOF_REACH 4.0f
/* the probes' height above the floor (a crouched biped is hidden by what stands this high) */
#define AE_BOTS_COVER_CHEST 0.45f
/* a place with the level less than this above it is no place to stand */
#define AE_BOTS_COVER_HEADROOM 0.75f
/* places kept apart at least this */
#define AE_BOTS_COVER_SPACING 1.2f
/* the spots looked at: this near the carrier (across), at least this far, and this much up or down */
#define AE_BOTS_COVER_NEAREST 2.0f
#define AE_BOTS_COVER_FARTHEST 22.0f
#define AE_BOTS_COVER_HEIGHT 6.0f
/* the threats: an enemy this near the carrier makes it move on (a camouflaged carrier: AE_BOTS_COVER_THREAT_CAMO) */
#define AE_BOTS_COVER_THREAT_NEAR 8.0f
#define AE_BOTS_COVER_THREAT_CAMO 4.0f
/* the enemies whose lines of sight count, within this */
#define AE_BOTS_COVER_SIGHT 35.0f

/* the cover score of a place: walls is the eight sides' bitmask (bit n: side n, n * 45 degrees, hit within the
reach), roof whether the level is above it; 0 for no cover */
int ae_bots_cover_score(unsigned walls, int roof);

/* the way out of a place (its open sides' middle), as an index 0-7 of AE_BOTS_COVER_SIDES; -1 for none open */
int ae_bots_cover_open_side(unsigned walls);

/* a hide spot's cheap score (the higher the better): own_distance from the carrier, threat_distance to the nearest
enemy it knows of (negative: none known), cover the place's score, allies_near the teammates near it, through whether
the way there passes by a known enemy, chance 0-1 (the bot's random number) */
float ae_bots_cover_spot_score(int level, float own_distance, float threat_distance, int cover, int allies_near,
	int through, float chance);
/* the cheap score less what each enemy that sees the spot takes */
float ae_bots_cover_seen_penalty(int level, int seen_by);

/* how many of the best spots by the cheap score are tried with the lines of sight */
int ae_bots_cover_candidates(int level);
/* ticks a carrier holds a spot at most (30 ticks a second) */
long ae_bots_cover_hold_ticks(int level);
/* ticks between its looks for whether an enemy sees it */
long ae_bots_cover_watch_ticks(int level);

/* why a holder moves on now: 0 it holds; 1 an enemy near; 2 an enemy sees it; 3 it was hurt; 4 the hold ran out.
nearest_enemy is the nearest known enemy's distance (negative: none), seen_by the enemies that see it, camouflaged the
carrier's camouflage (an enemy seeing it farther than the camouflage reach does not count: seen_by_near counts those
within it) */
int ae_bots_cover_leave(int level, float nearest_enemy, int seen_by, int seen_by_near, int hurt, long held_ticks,
	int camouflaged);

/* COVER ON A RETREAT (0.9.4; owner 11-16: "more experienced players have a sense of what is behind them, always a retreat
pathway while they are shooting, and getting to cover if they are hurt"). The places are the carriers' (the map's own
geometry, above). A bot in a fight that retreats (NORMAL and up: ae_bots_retreat_cover_use) backs off to a covered place
no known enemy sees, on its own level; HEROIC and up (ae_bots_retreat_cover_plans) keep the place in mind as they fight,
looked for again every AE_BOTS_RETREAT_COVER_PLAN_TICKS, their back-off in a fight turned towards it, and go there at
once when the retreat starts. At the place it holds (still fighting from it) until its shields are back and the
retreat ends. A place: AE_BOTS_RETREAT_COVER_NEAREST to AE_BOTS_RETREAT_COVER_FARTHEST off across, from
AE_BOTS_RETREAT_COVER_BELOW under the bot to AE_BOTS_RETREAT_COVER_ABOVE over it, not on the target's side
(the cosine of its way against the target's under AE_BOTS_RETREAT_COVER_TOWARD); the nearest, the better covered and
the farther from the target win */
#define AE_BOTS_RETREAT_COVER_NEAREST 2.0f
#define AE_BOTS_RETREAT_COVER_FARTHEST 15.0f
#define AE_BOTS_RETREAT_COVER_BELOW 2.5f
#define AE_BOTS_RETREAT_COVER_ABOVE 3.0f
#define AE_BOTS_RETREAT_COVER_TOWARD 0.3f
#define AE_BOTS_RETREAT_COVER_PLAN_TICKS 30
/* a place this near across (and within 1.5 up or down) is reached */
#define AE_BOTS_RETREAT_COVER_THERE 0.8f
int ae_bots_retreat_cover_use(int level);
int ae_bots_retreat_cover_plans(int level);
/* a place's score, AE_BOTS_RETREAT_COVER_NONE when it is not one: own_distance across from the bot, rise its height over
the bot's feet, toward_cosine its way's cosine against the target's, target_gain how much farther from the target it
is than the bot (negative: nearer), cover its cover score */
#define AE_BOTS_RETREAT_COVER_NONE (-1.0e9f)
float ae_bots_retreat_cover_score(float own_distance, float rise, float toward_cosine, float target_gain, int cover);
/* the back-off's way in a fight with a place in mind (HEROIC and up): towards the place when it is behind the bot
(more than a quarter turn from the target, the angles in radians), else straight away from the target */
float ae_bots_retreat_cover_back_way(float to_target, float to_place);

#endif
