/* The second-generation game's King of the Hill settings and TERRITORIES (the AE block, ae_variant_ext_rules.h):
what the AE hook lines in source/game/game_engine_king.c call. Nothing changes with the default block. */
#ifndef AE_H2_KING_H
#define AE_H2_KING_H

#include "cseries.h"
#include "math/real_math.h"

enum
{
	AE_H2_TERRITORIES_MAXIMUM = 8,
	AE_H2_TERRITORIES_MAGIC = 0x52544541, /* 'AETR' little-endian */
};

/* the hills' state a host sends after the King engine's in a TERRITORIES game (its clients take the owners) */
struct ae_h2_territories_network_state
{
	long magic;
	long count;
	long hill_ids[AE_H2_TERRITORIES_MAXIMUM];
	signed char owners[AE_H2_TERRITORIES_MAXIMUM];
};

/* a TERRITORIES game now (the King engine, a team game, the block's mode) */
int ae_h2_territories_on(void);
/* the hills in play at once: the territories' count (of the map's hills, up to maximum), else Multi-Hill's own */
long ae_h2_king_hills_wanted(long multi_hill_wanted, long map_hills, long maximum);
/* whether the hill moves (never in TERRITORIES), and its clock in ticks (HILL MOVES EVERY, else the game's) */
int ae_h2_king_moving(int moving_hill);
long ae_h2_king_move_ticks(long default_ticks);
/* a player on a hill: the ticks of score this tick (0: none: TERRITORIES scores by owner, an UNCONTESTED HILL with
an enemy in it scores nothing), and whether each player on it scores (TEAM TIME MULTIPLIER) */
long ae_h2_king_score_step(int contested);
int ae_h2_king_every_player_scores(void);
/* the host each tick: the territories' owners from who stands in each (counts[hill][team], two teams), the scores
of the held ones added; returns 1 when a team reached score_to_win (the engine ends the game) */
int ae_h2_territories_tick(short hill_count, long const *hill_ids, long const (*counts)[2], long *team_scores,
	long score_to_win);
/* the hill's state as drawn (an owner's colour), from the engine's own */
long ae_h2_territories_hill_state(short hill_index, long engine_state);
/* the same from the King engine's hills (ae_king_* accessors) and every living player's team */
int ae_h2_territories_engine_tick(long *team_scores, long score_to_win);
/* a client: the territory's hill id the host sent (NONE past the count) */
long ae_h2_territories_hill_id(short hill_index);
/* forget the owners (a new map, a new round) */
void ae_h2_territories_reset(void);
/* the network tail: the host's size written (0: none), a client's read (1: taken) */
long ae_h2_territories_write(unsigned char *buffer, long size, short hill_count, long const *hill_ids);
int ae_h2_territories_read(unsigned char const *buffer, long size);

/* the owners as counts (red, blue, neutral, of the total in play); 0 when it is not a TERRITORIES game */
int ae_h2_territories_held(long *red, long *blue, long *neutral, long *total);
/* a territory's waypoint arrow by name (owner's colour; "crown_blue" outside TERRITORIES) */
char const *ae_h2_territories_icon(short hill_index);
/* the goal being drawn as a waypoint (NONE: none): a neutral territory's arrow is tinted white by the tint hook */
void ae_h2_territories_nav_begin(long goal_index);
void ae_h2_territories_nav_tint(real_rgb_color *color);

/* a bot's territory to go to (TERRITORIES only): the hills its team does not hold first, one for each player of the team
in turn, the rest hold; 1 when target is set, in_target says the bot is in it */
int ae_h2_territories_bot_target(long team_index, long player_index, real_point3d const *from, real_point3d *target,
	int *in_target);

#endif
