/*
SPAWN_HEAT.H

port: TRAINING's live spawn heat (spawn_heat.c): how likely each player
spawn is to be picked next, for the spawn markers (render_spawn_markers.c).
*/

#ifndef __SPAWN_HEAT_H
#define __SPAWN_HEAT_H
#pragma once

/* ---------- constants */

/* the most player starting locations rated (a scenario's tag block holds
at most this many) */
#define SPAWN_HEAT_MAXIMUM_SPAWNS 256

/* the heat is worked out again every this many ticks (5 a second) */
#define SPAWN_HEAT_UPDATE_TICKS 6

/* the spawns that can be picked are "near uniform" (all drawn in one
middle colour, none pulsing) when the least likely of them is at least
this part as likely as the likeliest: free for all, mostly */
#define SPAWN_HEAT_UNIFORM_RATIO 0.8f

/* a real spawn's flash lasts this many ticks (1.5 s) */
#define SPAWN_HEAT_FLASH_TICKS 45
#define SPAWN_HEAT_MAXIMUM_FLASHES 16

/* SPAWN HEAT (display.spawn_heat) */
enum spawn_heat_setting
{
	_spawn_heat_off = 0,	/* the plain green markers */
	_spawn_heat_mine,		/* where this view's player would spawn */
	_spawn_heat_enemy,		/* where an enemy would (team games; else as MINE) */

	NUMBER_OF_SPAWN_HEAT_SETTINGS
};

/* ---------- structures */

struct spawn_heat_spawn
{
	real rating;		/* game_engine_get_starting_location_rating_ex's */
	real probability;	/* of being picked, 0 to 1 */
	short zero_reason;	/* _spawn_rating_..., _spawn_rating_rated when rated */
};

/* one view's heat, as last worked out */
struct spawn_heat_view
{
	long computed_at;			/* the game tick */
	long rated_player_index;	/* whose spawns these are */
	short rated_team_index;
	boolean enemy;				/* rated for an enemy (ENEMY in a team game) */
	boolean any_rated;			/* any spawn rated above 0 */
	long count;					/* spawns rated (the scenario's, at most SPAWN_HEAT_MAXIMUM_SPAWNS) */
	long hottest;				/* the one spawn likelier than any other, NONE for a tie or near uniform */
	boolean near_uniform;		/* spawn_heat_near_uniform's */
	real probability_maximum;
	struct spawn_heat_spawn spawns[SPAWN_HEAT_MAXIMUM_SPAWNS];
};

/* a real spawn's flash */
struct spawn_heat_flash
{
	long spawn_index;
	short team_index;	/* NONE in a game without teams (white) */
	real age;			/* 0 as it spawns to 1 as it ends */
};

/* ---------- prototypes/SPAWN_HEAT.C */

short spawn_heat_setting(void);	/* display.spawn_heat, as _spawn_heat_... */
boolean spawn_heat_shown(void);	/* the markers show heat now (TRAINING's rules and SPAWN HEAT on) */
void spawn_heat_initialize_for_new_map(void);
void spawn_heat_update(void);	/* each game tick, from game_engine_update */

/* the view's heat, NULL while there is none */
struct spawn_heat_view const *spawn_heat_get_view(short local_player_index);

/* the flashes showing now (at most maximum), how many */
short spawn_heat_get_flashes(struct spawn_heat_flash *flashes, short maximum);

/* a player spawned at a starting location: the host's (or a local game's)
player_spawn */
void spawn_heat_note_spawn(long player_index, long starting_location_index);
/* a unit given to a player who had none: a distributed client's spawn
(network_player_attach_unit), matched to its starting location */
void spawn_heat_note_unit_attached(long player_index, long unit_index);

/* each rating's chance of being picked as find_best_starting_location_index
picks (the highest rating * sqrt(random 0..1)): probabilities[i] for
ratings[i], 0 for a rating of 0 or less; all 0 when every one is. Pure */
void spawn_heat_probabilities(long count, real const *ratings, real *probabilities);
/* whether the chances above 0 are near uniform: the least at least
SPAWN_HEAT_UNIFORM_RATIO of the most (FALSE for none). Pure */
boolean spawn_heat_near_uniform(long count, real const *probabilities);

#endif // __SPAWN_HEAT_H
