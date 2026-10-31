/*
AE_BOTS_TEAM.INC

Included by ae_bots_lan.inc, so at the end of bots.c: the bots playing as a team (ae_bots_team_rules.h). In a team
game, bots_choose_goal asks after the power items (its AE hook line):

- WINGMAN: each human player's nearest teammate bots, ae_bots_team_human_squad of them (chosen again every 3 s, kept while it is in the game, through its deaths) stays near that
  player: past 8 units it goes to them; within, it moves to a spot 3 to 6 units from them every 4 s. In a fight it
  fights as ever. A wingman goes for a power item only within 15 units (ae_bots_power.inc asks).
- THE NUMBERS GAME: two or more behind, a bot not fighting regroups on its nearest living teammate (when farther than
  6 units) instead of roaming.

OUTNUMBERED is the fight's (ae_bots_combat.inc takes ae_bots_team_outnumbered from its confidence).
*/

#include "ae_bots_team_rules.h"
#include "items/projectiles.h"

/* (a pair for each human's wingman) */
#define AE_BOTS_TEAM_WINGMEN (BOTS_BRAINS + MAXIMUM_LOCAL_PLAYERS + 16)

static struct
{
	long check_tick;
	/* each human player's wingmen, a pair for each: the human's player index, the bot's index */
	long human_player[AE_BOTS_TEAM_WINGMEN];
	long wingman_bot[AE_BOTS_TEAM_WINGMEN];
	short human_count;
	/* when each bot picks its next spot about its player */
	long spot_tick[BOTS_BRAINS];
} ae_bots_team;

/* the bot of a player, or NONE for a human */
static long ae_bots_team_bot_of(long player_index)
{
	long index;

	for (index = 0; index < BOTS_BRAINS; index++)
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

/* whether the player was among the humans of the last update */
static boolean ae_bots_team_was_human(long player_index, long const *humans, short count)
{
	short index;

	for (index = 0; index < count; index++)
	{
		if (humans[index] == player_index)
			return TRUE;
	}
	return FALSE;
}

/* the humans of the game and each one's wingman, every 3 s: the nearest living bot of that human's team that is no
other human's wingman, the one before kept while it is in the game and of that team, dead or alive */
static void ae_bots_team_update(long now)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long previous_human[AE_BOTS_TEAM_WINGMEN], previous_bot[AE_BOTS_TEAM_WINGMEN];
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
		long bot_index;
		int humans = 0, bots = 0, able = 0, want, given = 0;
		struct data_iterator side;
		struct player_datum *other;

		/* (a human, dead or alive: the wingman stays theirs while they respawn) */
		if (ae_bots_team_bot_of(player_index) != NONE ||
			(player->unit_index == NONE && !ae_bots_team_was_human(player_index, previous_human, previous_count)))
			continue;
		data_iterator_new(&side, player_data);
		while ((other = (struct player_datum *)data_iterator_next(&side)) != NULL)
		{
			if (other->team_index != player->team_index)
				continue;
			bot_index = ae_bots_team_bot_of(side.datum_index);
			if (bot_index != NONE)
			{
				bots++;
				able += ae_bots_team_free_for_human(&bots_globals.bots[bot_index], other) != FALSE;
			}
			else
				humans++;
		}
		want = ae_bots_team_human_squad(humans, bots);
		/* (a site defender: a CTF game) */
		if (able < bots)
			want = ae_bots_team_human_squad_leaving_crews(want, able);
		/* (the wingmen of before kept while they are in the game, of the player's team and free to go, dead or alive: they
		come back to them after they respawn; handing the role to the nearest at every death made it hop about 20 times
		in a 7-minute game, 10-20 Sali Creek) */
		for (index = 0; index < previous_count && given < want && ae_bots_team.human_count < (short)NUMBEROF(ae_bots_team.human_player); index++)
		{
			long kept = previous_bot[index];
			struct player_datum *mate;

			if (previous_human[index] != player_index || bots_globals.bots[kept].player_index == NONE)
				continue;
			mate = player_try_and_get(bots_globals.bots[kept].player_index);
			if (!mate || mate->team_index != player->team_index || !ae_bots_team_free_for_human(&bots_globals.bots[kept], mate))
				continue;
			ae_bots_team.human_player[ae_bots_team.human_count] = player_index;
			ae_bots_team.wingman_bot[ae_bots_team.human_count] = kept;
			ae_bots_team.human_count++;
			given++;
		}
		/* (a fresh choice needs the human's body: none between their lives) */
		while (given < want && player->unit_index != NONE && ae_bots_team.human_count < (short)NUMBEROF(ae_bots_team.human_player))
		{
			long best = NONE;
			real best_distance = REAL_MAX;

			for (bot_index = 0; bot_index < BOTS_BRAINS; bot_index++)
			{
				struct player_datum *mate;
				boolean taken = FALSE;
				real distance;

				if (bots_globals.bots[bot_index].player_index == NONE ||
					(mate = player_try_and_get(bots_globals.bots[bot_index].player_index)) == NULL ||
					mate->team_index != player->team_index || !ae_bots_team_alive(mate) ||
					!ae_bots_team_free_for_human(&bots_globals.bots[bot_index], mate))
				{
					continue;
				}
				for (index = 0; index < ae_bots_team.human_count; index++)
				{
					if (ae_bots_team.wingman_bot[index] == bot_index)
						taken = TRUE;
				}
				for (index = 0; index < previous_count; index++)
				{
					/* (another human's of before, not come to yet) */
					if (previous_bot[index] == bot_index && previous_human[index] != player_index)
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
			if (best == NONE)
				break;
			if (ae_bots_events_on())
				platform_log("bots: bot %ld is the wingman of player %ld", (long)(best + 1), DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index));
			ae_bots_team.human_player[ae_bots_team.human_count] = player_index;
			ae_bots_team.wingman_bot[ae_bots_team.human_count] = best;
			ae_bots_team.human_count++;
			given++;
		}
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
static struct { long tick; short exposed; } ae_bots_team_exposure[BOTS_BRAINS];

static short ae_bots_team_exposed(struct bots_bot *bot, struct bots_tick *tick, int level)
{
	long bot_index = (long)(bot - bots_globals.bots);
	struct data_iterator iterator;
	struct player_datum *other;
	real_point3d const *eye;
	short exposed = 0;

	if (level < AE_BOTS_LEVEL_HEROIC || bot_index < 0 || bot_index >= BOTS_BRAINS)
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
		/* (never a spot on a teleporter pad: a wingman waiting there was sent off and back, 10-20 Sali Creek) */
		if (*surface_index != NONE && !ae_bots_travel_near_pad(ground, 2.5f))
			return TRUE;
	}
	*surface_index = bots_surface_below(point, ground);
	return (boolean)(*surface_index != NONE && !ae_bots_travel_near_pad(ground, 2.5f));
}

/* HEARING: a bot with no enemy in mind (none seen for 3 s) hears an enemy's shot (a projectile of its in flight: a
bullet lives under a second) when the shooter is within its level's range, every 5 ticks, and keeps where the shooter is as where an enemy was last seen,
a second ago (bots_choose_goal then goes there; seen less than half a second ago would start a fight it cannot see) */
static long ae_bots_team_hear_tick[BOTS_BRAINS];

static void ae_bots_team_hear(struct bots_bot *bot, long player_index, real_point3d const *position, long now)
{
	long bot_index = (long)(bot - bots_globals.bots);
	real range = ae_bots_team_hearing_range(ae_bots_level_of_skill(bots_globals.skill));
	struct object_iterator iterator;
	struct player_datum *player = player_get(player_index);

	if (range <= 0.0f || bot_index < 0 || bot_index >= BOTS_BRAINS || now < ae_bots_team_hear_tick[bot_index] ||
		(bot->target_player_index != NONE && now - bot->target_seen_tick < 3 * TICKS_PER_SECOND))
	{
		return;
	}
	ae_bots_team_hear_tick[bot_index] = now + 5;
	object_iterator_new(&iterator, _object_mask_projectile, 0);
	while (object_iterator_next(&iterator))
	{
		struct projectile_datum const *projectile = projectile_get(iterator.index);
		long owner = projectile->object.owner_object_index;
		struct data_iterator players;
		struct player_datum *other;

		/* (a projectile owned by the weapon: its holder's. A shooter deleted while its shots fly leaves the
		projectile an owner index that is no object's: object_get_ultimate_parent asserts on it, a Windows crash
		10-20 with 15 bots on H2D Terminal; each step is looked up softly, a gone one is no owner to hear) */
		while (owner != NONE)
		{
			struct object_datum const *object = (struct object_datum const *)object_try_and_get(owner);

			if (!object)
			{
				owner = NONE;
				break;
			}
			if (object->object.parent_object_index == NONE)
				break;
			owner = object->object.parent_object_index;
		}
		if (owner == NONE)
			continue;
		data_iterator_new(&players, player_data);
		while ((other = (struct player_datum *)data_iterator_next(&players)) != NULL)
		{
			long surface_index;
			real_point3d ground;

			if (other->unit_index != owner || !bots_is_enemy(player, other) || !ae_bots_team_alive(other) ||
				bots_distance3d(&object_get(other->unit_index)->object.position, position) > range)
			{
				continue;
			}
			surface_index = bots_unit_surface(other->unit_index, &ground);
			if (surface_index == NONE)
				return;
			bot->target_player_index = players.datum_index;
			bot->target_last_position = object_get(other->unit_index)->object.position;
			bot->target_last_surface_index = surface_index;
			bot->target_seen_tick = now - TICKS_PER_SECOND;
			if (ae_bots_events_on())
				platform_log("bots: bot %ld hears a shot %.0f units away", (long)(bot_index + 1),
					(double)bots_distance3d(&bot->target_last_position, position));
			return;
		}
	}
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

	if (!game_engine_has_teams() || fighting || bot_index < 0 || bot_index >= BOTS_BRAINS)
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
	csmemset(ae_bots_team_hear_tick, 0, sizeof(ae_bots_team_hear_tick));
}
