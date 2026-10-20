/*
AE_BOTS_TEAM.INC

Included by ae_bots_lan.inc, so at the end of bots.c: the bots playing as a team (ae_bots_team_rules.h). In a team
game, bots_choose_goal asks after the power items (its AE hook line):

- WINGMAN: each human player's nearest teammate bot (chosen again every 3 s, kept while it is in the game, through its deaths) stays near that
  player: past 8 units it goes to them; within, it moves to a spot 3 to 6 units from them every 4 s. In a fight it
  fights as ever. A wingman goes for a power item only within 15 units (ae_bots_power.inc asks).
- THE NUMBERS GAME: two or more behind, a bot not fighting regroups on its nearest living teammate (when farther than
  6 units) instead of roaming.

OUTNUMBERED is the fight's (ae_bots_combat.inc takes ae_bots_team_outnumbered from its confidence).
*/

#include "ae_bots_team_rules.h"

static struct
{
	long check_tick;
	/* each human player's wingman: the bot's index, or NONE (by the human's player absolute index) */
	long human_player[MAXIMUM_LOCAL_PLAYERS + 16];
	long wingman_bot[MAXIMUM_LOCAL_PLAYERS + 16];
	short human_count;
	/* when each bot picks its next spot about its player */
	long spot_tick[BOTS_MAXIMUM];
} ae_bots_team;

/* the bot of a player, or NONE for a human */
static long ae_bots_team_bot_of(long player_index)
{
	long index;

	for (index = 0; index < BOTS_MAXIMUM; index++)
	{
		if (bots_globals.bots[index].player_index == player_index)
			return index;
	}
	return NONE;
}

static boolean ae_bots_team_alive(struct player_datum const *player)
{
	struct object_datum const *object;

	if (player->unit_index == NONE ||
		(object = (struct object_datum const *)object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit)) == NULL)
		return FALSE;
	return (boolean)!TEST_FLAG(object->object.damage_flags, _object_dead_bit);
}

/* the humans of the game and each one's wingman, every 3 s: the nearest living bot of that human's team that is no
other human's wingman, the one before kept while it is in the game and of that team, dead or alive */
static void ae_bots_team_update(long now)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long previous_human[MAXIMUM_LOCAL_PLAYERS + 16], previous_bot[MAXIMUM_LOCAL_PLAYERS + 16];
	short previous_count = ae_bots_team.human_count, index;

	if (now < ae_bots_team.check_tick)
		return;
	ae_bots_team.check_tick = now + 3 * TICKS_PER_SECOND;
	csmemcpy(previous_human, ae_bots_team.human_player, sizeof(previous_human));
	csmemcpy(previous_bot, ae_bots_team.wingman_bot, sizeof(previous_bot));
	ae_bots_team.human_count = 0;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL &&
		ae_bots_team.human_count < (short)NUMBEROF(ae_bots_team.human_player))
	{
		long player_index = iterator.datum_index;
		long kept = NONE, best = NONE, bot_index;
		real best_distance = REAL_MAX;

		if (ae_bots_team_bot_of(player_index) != NONE || !ae_bots_team_alive(player))
			continue;
		for (index = 0; index < previous_count; index++)
		{
			if (previous_human[index] == player_index)
				kept = previous_bot[index];
		}
		/* (the wingman of before kept while it is in the game and of the player's team, dead or alive: it comes back
		to them after it respawns; handing the role to the nearest at every death made it hop about 20 times in a
		7-minute game, 10-20 Sali Creek) */
		if (kept != NONE && bots_globals.bots[kept].player_index != NONE)
		{
			struct player_datum const *mate = player_try_and_get(bots_globals.bots[kept].player_index);

			if (mate && mate->team_index == player->team_index)
				best = kept;
		}
		for (bot_index = 0; best == NONE && bot_index < BOTS_MAXIMUM; bot_index++)
		{
			struct player_datum const *mate;
			boolean taken = FALSE;
			real distance;

			if (bots_globals.bots[bot_index].player_index == NONE ||
				(mate = player_try_and_get(bots_globals.bots[bot_index].player_index)) == NULL ||
				mate->team_index != player->team_index || !ae_bots_team_alive(mate))
			{
				continue;
			}
			for (index = 0; index < ae_bots_team.human_count; index++)
			{
				if (ae_bots_team.wingman_bot[index] == bot_index)
					taken = TRUE;
			}
			if (taken)
				continue;
			distance = bots_distance3d(&object_get(mate->unit_index)->object.position,
				&object_get(player->unit_index)->object.position);
			if (distance < best_distance)
			{
				best_distance = distance;
				best = bot_index;
			}
		}
		/* (a fresh choice of the nearest: the loop above ran to its end) */
		if (best == NONE)
			continue;
		if (best != kept && ae_bots_events_on())
			platform_log("bots: bot %ld is the wingman of player %ld", (long)(best + 1), DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index));
		ae_bots_team.human_player[ae_bots_team.human_count] = player_index;
		ae_bots_team.wingman_bot[ae_bots_team.human_count] = best;
		ae_bots_team.human_count++;
	}
}

/* the human player the bot is wingman to, or NONE */
static long ae_bots_team_wingman_of(struct bots_bot const *bot)
{
	long bot_index = (long)(bot - bots_globals.bots);
	short index;

	if (!game_engine_has_teams() || !ae_bots_team_wingman_level(ae_bots_level_of_skill(bots_globals.skill)))
		return NONE;
	for (index = 0; index < ae_bots_team.human_count; index++)
	{
		if (ae_bots_team.wingman_bot[index] == bot_index)
			return ae_bots_team.human_player[index];
	}
	return NONE;
}

/* the living teammates (the bot counted) and enemies of the bot's player: all of them, and those within near units */
static void ae_bots_team_counts(struct player_datum *player, real_point3d const *position, real near,
	int *allies, int *enemies, int *allies_near, int *enemies_near)
{
	struct data_iterator iterator;
	struct player_datum *other;

	*allies = *enemies = *allies_near = *enemies_near = 0;
	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		boolean is_near;

		if (!ae_bots_team_alive(other))
			continue;
		is_near = bots_distance3d(&object_get(other->unit_index)->object.position, position) <= near;
		if (other == player || (game_engine_has_teams() && other->team_index == player->team_index))
		{
			(*allies)++;
			if (is_near || other == player)
				(*allies_near)++;
		}
		else if (bots_is_enemy(player, other))
		{
			(*enemies)++;
			if (is_near)
				(*enemies_near)++;
		}
	}
}

/* ANGLE ISOLATION's count: the living enemies within AE_BOTS_TEAM_EXPOSED_RANGE with a clear line to the bot's eye,
counted again every 10 ticks (HEROIC and up only: the lines cost) */
static struct { long tick; short exposed; } ae_bots_team_exposure[BOTS_MAXIMUM];

static short ae_bots_team_exposed(struct bots_bot *bot, struct bots_tick *tick, int level)
{
	long bot_index = (long)(bot - bots_globals.bots);
	struct data_iterator iterator;
	struct player_datum *other;
	real_point3d const *eye;
	short exposed = 0;

	if (level < AE_BOTS_LEVEL_HEROIC || bot_index < 0 || bot_index >= BOTS_MAXIMUM)
		return 0;
	if (tick->now >= ae_bots_team_exposure[bot_index].tick && tick->now - ae_bots_team_exposure[bot_index].tick < 10)
		return ae_bots_team_exposure[bot_index].exposed;
	eye = bots_tick_eye(tick);
	data_iterator_new(&iterator, player_data);
	while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct object_datum const *object;

		if (!bots_is_enemy(tick->player, other) || !ae_bots_team_alive(other))
			continue;
		object = object_get(other->unit_index);
		if (bots_distance3d(eye, &object->object.bounding_sphere_center) > AE_BOTS_TEAM_EXPOSED_RANGE)
			continue;
		if (bots_line_of_sight(tick->player->unit_index, eye, &object->object.bounding_sphere_center, other->unit_index))
			exposed++;
	}
	ae_bots_team_exposure[bot_index].tick = tick->now;
	ae_bots_team_exposure[bot_index].exposed = exposed;
	return exposed;
}

/* OUTNUMBERED and ANGLE ISOLATION: the larger of what the enemies near past the teammates near, and the enemies with
a line to it past the first, take from the fight's confidence */
static real ae_bots_team_outnumbered(struct bots_bot *bot, struct bots_tick *tick, int level)
{
	int allies, enemies, allies_near, enemies_near;
	real outnumbered, exposed;

	ae_bots_team_counts(tick->player, &tick->position, AE_BOTS_TEAM_NEAR, &allies, &enemies, &allies_near,
		&enemies_near);
	outnumbered = ae_bots_team_outnumbered_penalty(level, enemies_near, allies_near);
	exposed = ae_bots_team_exposed_penalty(level, ae_bots_team_exposed(bot, tick, level));
	return outnumbered > exposed ? outnumbered : exposed;
}

/* a spot on the ground near a point (a random one between near and far units away; the point itself if none is
found), its surface; FALSE for none */
static boolean ae_bots_team_spot(struct bots_bot *bot, real_point3d const *point, real near, real far,
	real_point3d *ground, long *surface_index)
{
	short tries;

	for (tries = 0; tries < 4; tries++)
	{
		real angle = bots_random_real(bot) * 6.2831853f;
		real distance = near + bots_random_real(bot) * (far - near);
		real_point3d spot = *point;

		spot.x += (real)cos(angle) * distance;
		spot.y += (real)sin(angle) * distance;
		*surface_index = bots_surface_below(&spot, ground);
		if (*surface_index != NONE)
			return TRUE;
	}
	*surface_index = bots_surface_below(point, ground);
	return (boolean)(*surface_index != NONE);
}

static boolean ae_bots_team_goal(struct bots_bot *bot, long player_index, real_point3d const *position,
	boolean fighting)
{
	long now = game_time_get();
	long bot_index = (long)(bot - bots_globals.bots);
	long human;
	int level = ae_bots_level_of_skill(bots_globals.skill);
	real_point3d ground;
	long surface_index;

	if (!game_engine_has_teams() || fighting || bot_index < 0 || bot_index >= BOTS_MAXIMUM)
		return FALSE;
	ae_bots_team_update(now);
	/* WINGMAN */
	human = ae_bots_team_wingman_of(bot);
	if (human != NONE)
	{
		struct player_datum const *human_player = player_try_and_get(human);
		real_point3d const *at;
		real distance;

		if (human_player && ae_bots_team_alive(human_player))
		{
			at = &object_get(human_player->unit_index)->object.position;
			distance = bots_distance3d(position, at);
			if (ae_bots_team_wingman_move(distance) == 1)
			{
				/* (the path to where the player was kept while they stay within 4 units of it: a goal moved every
				tick restarts the path search every tick, and the bot never walks; seen 10-20 on Sali Creek) */
				if (bot->goal_kind == _bots_goal_roam && bots_distance3d(&bot->goal_point, at) < 4.0f)
					return TRUE;
				surface_index = bots_surface_below(at, &ground);
				if (surface_index != NONE)
				{
					bots_set_goal(bot, _bots_goal_roam, &ground, surface_index);
					ae_bots_team.spot_tick[bot_index] = 0;
					return TRUE;
				}
			}
			else
			{
				if (now >= ae_bots_team.spot_tick[bot_index] &&
					ae_bots_team_spot(bot, at, AE_BOTS_WINGMAN_NEAR, AE_BOTS_WINGMAN_SPOT, &ground, &surface_index))
				{
					bots_set_goal(bot, _bots_goal_roam, &ground, surface_index);
					ae_bots_team.spot_tick[bot_index] = now + 4 * TICKS_PER_SECOND;
				}
				return TRUE;
			}
		}
	}
	/* THE NUMBERS GAME */
	{
		struct player_datum *player = player_get(player_index);
		int allies, enemies, allies_near, enemies_near;

		ae_bots_team_counts(player, position, AE_BOTS_TEAM_NEAR, &allies, &enemies, &allies_near, &enemies_near);
		if (ae_bots_team_stance(level, allies, enemies) < 0)
		{
			struct data_iterator iterator;
			struct player_datum *other;
			real nearest = REAL_MAX;
			real_point3d const *mate_at = NULL;

			data_iterator_new(&iterator, player_data);
			while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
			{
				real distance;

				if (other == player || other->team_index != player->team_index || !ae_bots_team_alive(other))
					continue;
				distance = bots_distance3d(&object_get(other->unit_index)->object.position, position);
				if (distance < nearest)
				{
					nearest = distance;
					mate_at = &object_get(other->unit_index)->object.position;
				}
			}
			/* (as the wingman's: the path kept while the teammate stays within 4 units of its goal) */
			if (mate_at && nearest > 6.0f && bot->goal_kind == _bots_goal_roam &&
				bots_distance3d(&bot->goal_point, mate_at) < 4.0f)
			{
				return TRUE;
			}
			if (mate_at && nearest > 6.0f && (surface_index = bots_surface_below(mate_at, &ground)) != NONE)
			{
				bots_set_goal(bot, _bots_goal_roam, &ground, surface_index);
				return TRUE;
			}
		}
	}
	return FALSE;
}

/* WINGMAN's leash in a fight: the direction back to its player when farther than AE_BOTS_WINGMAN_LEASH; FALSE for none */
static boolean ae_bots_team_leash(struct bots_bot *bot, struct bots_tick *tick, real *direction)
{
	long human = ae_bots_team_wingman_of(bot);
	struct player_datum const *human_player;
	real_point3d const *at;

	if (human == NONE || (human_player = player_try_and_get(human)) == NULL || !ae_bots_team_alive(human_player))
		return FALSE;
	at = &object_get(human_player->unit_index)->object.position;
	if (bots_distance3d(&tick->position, at) <= AE_BOTS_WINGMAN_LEASH)
		return FALSE;
	*direction = (real)atan2(at->y - tick->position.y, at->x - tick->position.x);
	return TRUE;
}

/* (a map's start) the wingmen chosen again */
static void ae_bots_team_reset(void)
{
	csmemset(&ae_bots_team, 0, sizeof(ae_bots_team));
	csmemset(ae_bots_team_exposure, 0, sizeof(ae_bots_team_exposure));
}
