/*
AE_BOTS_VANTAGE_RULES.H

Arena Evolved: the map's vantage spots for the bots (ae_bots_vantage.c reads the map, this file is the pure part;
port/linux/tests/ae_bots_vantage_rules_test.c). From halo-ce-vita (GPL-3.0) via Halo Content Evolved
(Watanoge/halo-content-evolved, AGPL-3.0-or-later): the scoring and the choice are theirs, adapted here.

THE SPOTS. The places that matter (flag stands, hills, ball spawns, the direct route at a quarter, a half and three quarters,
the side lanes' waypoints, weapon and item spawns, 16 at most, each with a weight) are scored from the map's flat walkable
surfaces reached from the bases: first by how high a spot stands over the surfaces about it and the places within a rifle's
reach (the best 30 kept, 6 units apart); then, with a sight test from its eyes to each place within 90 units and eight rays
about it for openness, by the places it sees (their weight, by a distance band best from 12 to 50 units), its height and its
openness. The best 6 that see two places or more, 12 units apart, are the map's.

HOLDING ONE. A free bot with a long precision weapon (a scoped rifle) or a heavy one (a rocket launcher) takes a free spot and
holds it a while. At most a third of a team's bots hold spots at once, half with long weapons; never two on one spot, not the spot
it held last, not a spot it found no way to. It leaves when an enemy comes within 8 units, it is hurt below half, it is low on
ammunition, or it gets an order or an objective.
*/

#ifndef __AE_BOTS_VANTAGE_RULES_H
#define __AE_BOTS_VANTAGE_RULES_H

#define AE_VANTAGE_CANDIDATES 30
#define AE_VANTAGE_CANDIDATE_SPACING 6.0f
#define AE_VANTAGE_SPACING 12.0f
#define AE_VANTAGE_MAXIMUM 6
#define AE_VANTAGE_PLACES 16
#define AE_VANTAGE_SIGHT 90.0f
#define AE_VANTAGE_OPEN_RAYS 8
#define AE_VANTAGE_MINIMUM_SEEN 2

struct ae_vantage_place
{
	float x, y, z;
	float weight;
};

struct ae_vantage_spot
{
	float x, y, z;
	long surface;
	float height;
	float first_score;
	float score;
	/* a bit for each place seen, and how many; open rays clear of the eight */
	unsigned long sees;
	short sees_count;
	short open;
};

/* how much a place this far away counts: 0.2 under 5 units, up to 1 at 12, 1 to 50, then down to 0.3 at 90, none beyond */
float ae_vantage_range(float distance);
/* a spot's first score, before any ray: its height (pinned from -2 to 8) and a quarter of what the places within reach count */
float ae_vantage_first_score(float height, struct ae_vantage_place const *places, int place_count, float x, float y, float z);
/* a spot among the candidates (at most AE_VANTAGE_CANDIDATES): a better one within 6 units takes the place of one near it, else it
joins, or replaces the worst if it is better */
void ae_vantage_consider(struct ae_vantage_spot *candidates, int *count, struct ae_vantage_spot const *spot);
/* a spot's final score from what it sees (sees, open set): the places by weight and distance, its height and its openness */
void ae_vantage_score(struct ae_vantage_spot *spot, struct ae_vantage_place const *places, int place_count);
/* the map's spots: the best of the scored candidates that see two places or more, 12 units apart, at most AE_VANTAGE_MAXIMUM;
returns how many, in best-first order */
int ae_vantage_choose(struct ae_vantage_spot const *candidates, int count, struct ae_vantage_spot *points);

/* the height grid: the walkable area and its area-weighted height in cells, for how high a spot stands over its surroundings (the
3 by 3 cells about it) */
struct ae_vantage_grid
{
	float x, y, cell;
	long width, height;
	float *area;
	float *height_sum;
};
/* a grid over the bounds, cells at least `cell` wide and at most `maximum` along a side; 0 for no memory */
int ae_vantage_grid_begin(struct ae_vantage_grid *grid, float x0, float y0, float x1, float y1, float cell, long maximum);
void ae_vantage_grid_add(struct ae_vantage_grid *grid, float x, float y, float z, float area);
float ae_vantage_grid_height(struct ae_vantage_grid const *grid, float x, float y, float z);
void ae_vantage_grid_end(struct ae_vantage_grid *grid);

/* ---------- holding a spot */

/* a bot's weapon for the holds: a long precision weapon (the sniper rifle), a heavy one (a rocket launcher), a precision rifle (a battle
rifle, a DMR, a carbine) or none */
enum { AE_VANTAGE_WEAPON_NONE, AE_VANTAGE_WEAPON_LONG, AE_VANTAGE_WEAPON_HEAVY, AE_VANTAGE_WEAPON_RIFLE };
#define AE_VANTAGE_MAX_DISTANCE 70.0f
#define AE_VANTAGE_ENEMY_NEAR 8.0f
#define AE_VANTAGE_HOLD_SECONDS_LEAST 30
#define AE_VANTAGE_HOLD_SECONDS_MOST 60
#define AE_VANTAGE_REST_SECONDS 20
#define AE_VANTAGE_GO_SECONDS 45
#define AE_VANTAGE_FAILED_SECONDS 60
#define AE_VANTAGE_ARRIVED 2.0f
int ae_vantage_weapon_class(int sniper_rifle, int precision_rifle, int rocket_launcher);
/* how many of a team's bots may hold spots at once: a third, a half where the bot has the sniper rifle, one at least */
int ae_vantage_cap(int team_bots, int weapon);
/* whether one more bot, with this weapon, may take a spot when `holders` of its team hold one now */
int ae_vantage_may_take(int holders, int team_bots, int weapon);
/* the spot for a bot: the best by score less a tenth of its distance and the bot's own noise (0 to 2); not one in `excluded` (a bit a
spot: taken by another, or not over the hill), not the one it held last (unless the map has only one), not one in `failed`, none past
`maximum_distance`. -1 for none */
int ae_vantage_pick(int count, float const *scores, float const *distances, unsigned long excluded, int last, unsigned long failed,
	float maximum_distance, float const *noise);
/* the length of a hold, 30 to 60 seconds, from a number 0 to 1 */
long ae_vantage_hold_ticks(float unit_random, long ticks_per_second);
/* whether the bot leaves, and why, in this order: its weapon is not the one it holds for, it was hurt where it could not see who
did it (shot from behind, flanked), an enemy is within 8 units, it is hurt below half (health and shield together, of 2), it is low
on ammunition, its time is up */
enum { AE_VANTAGE_STAY, AE_VANTAGE_LEFT_WEAPON, AE_VANTAGE_LEFT_FLANKED, AE_VANTAGE_LEFT_ENEMY, AE_VANTAGE_LEFT_HURT,
	AE_VANTAGE_LEFT_AMMO, AE_VANTAGE_LEFT_TIME, AE_VANTAGE_LEFT_ORDER, AE_VANTAGE_LEFT_OBJECTIVE, AE_VANTAGE_LEFT_REASONS };
int ae_vantage_leave(int weapon_now, int weapon_held, int flanked, float enemy_distance, float vitality, int ammunition_low,
	long held_ticks, long hold_ticks);
/* whether a magazine and its reserve are low: under a quarter of the most the weapon holds */
int ae_vantage_ammunition_low(int rounds_left, int rounds_most);
char const *ae_vantage_leave_name(int reason);

#endif
