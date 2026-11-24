/*
AE_BOTS_CALLS.INC

Included by ae_bots_lan.inc, so at the end of bots.c, after the marks' glue: the bots make the communication wheel's calls
(notes/plans/2026-11-14-bot-wheel-calls-design.md) at moments their brain already acts on: an enemy told to the team (ENEMY),
a power item gone for (SMART), a flank begun (GOING), a defend or hold job taken (DEFEND), a driver waiting for riders
(RIDE), a crew spread far apart (REGROUP). The call is the host's own wheel call for the bot's player (ae_mark_calls_bot_call):
the same marker, label, tone, feed and wire as a person's. How often is ae_bots_calls_rules.h's. Host and local games only,
and only to a team with a person on it; a bot's call is not answered by the bots. Setting: bots.calls.
*/

#include "ae_bots_calls_rules.h"
#include "ae_marks.h"

unsigned long config_changes(void); /* port/linux/src/port_config.c, as ae_bots_travel.c declares it */
boolean ae_mark_calls_bot_call(long absolute, int entry, short kind, real_point3d const *spot); /* ae_mark_calls.c */

/* REGROUP: how far a crew's farthest member is when it counts as spread, and for how long in a row */
#define AE_BOTS_CALLS_SPREAD_DISTANCE 30.0f
#define AE_BOTS_CALLS_SPREAD_TICKS (10 * TICKS_PER_SECOND)
#define AE_BOTS_CALLS_TEAMS 16
/* a person's mark of the same call this near a bot's call wins */
#define AE_BOTS_CALLS_HUMAN_REACH 6.0f

static struct
{
	long last_tick[BOTS_BRAINS];
	long player_of[BOTS_BRAINS];	/* the player a brain's state is for: a brain given to another player starts over */
	long spread_since[BOTS_BRAINS];
	struct ae_bots_call_team teams[AE_BOTS_CALLS_TEAMS];
} ae_bots_calls;

static boolean ae_bots_calls_on(void)
{
	static unsigned long changes = 0xFFFFFFFFUL;
	static boolean on = TRUE;

	if (config_changes() != changes)
	{
		changes = config_changes();
		on = (boolean)(config_boolean("bots.calls") != 0);
	}
	return on;
}

/* a new map or game: all the calls' state starts over */
static void ae_bots_calls_reset(void)
{
	long index;

	for (index = 0; index < BOTS_BRAINS; index++)
	{
		ae_bots_calls.last_tick[index] = 0;
		ae_bots_calls.spread_since[index] = 0;
		ae_bots_calls.player_of[index] = NONE;
	}
	for (index = 0; index < AE_BOTS_CALLS_TEAMS; index++)
		ae_bots_call_team_reset(&ae_bots_calls.teams[index]);
}

/* a brain's slot reused for another player (a bot left, another joined): its call times are not the new bot's */
static void ae_bots_calls_sync_player(long bot_index, long player_index)
{
	if (ae_bots_calls.player_of[bot_index] != player_index)
	{
		ae_bots_calls.player_of[bot_index] = player_index;
		ae_bots_calls.last_tick[bot_index] = 0;
		ae_bots_calls.spread_since[bot_index] = 0;
	}
}

/* whether the team has a person, and whether one of them has the same call marked near the spot */
static boolean ae_bots_calls_team_state(long team_index, int entry, real_point3d const *spot, boolean *person_marked)
{
	struct data_iterator iterator;
	struct player_datum *player;
	boolean has_person = FALSE;

	*person_marked = FALSE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct ae_mark_detail detail;
		real_point3d marked;

		if (player->team_index != team_index || player->quit_out_of_game || ae_obj_player_is_bot(iterator.datum_index))
			continue;
		has_person = TRUE;
		if (!ae_marks_get_detail(DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index), &detail) || detail.entry != entry)
			continue;
		marked.x = detail.x;
		marked.y = detail.y;
		marked.z = detail.z;
		if (bots_distance3d(&marked, spot) < AE_BOTS_CALLS_HUMAN_REACH)
			*person_marked = TRUE;
	}
	return has_person;
}

/* the one way a bot calls. kind: the mark's AE_MARK_KIND_*; spot: where the call points. TRUE when the call was made */
static boolean ae_bots_call(struct bots_bot *bot, int entry, short kind, real_point3d const *spot)
{
	struct player_datum *player;
	struct unit_datum *unit;
	struct ae_bots_vehicle_bot *vehicle_state;
	long bot_index = (long)(bot - bots_globals.bots);
	long now = game_time_get();
	long team_index;
	boolean person_marked, has_person, alive, on_foot_or_driver, is_driver;
	int level;

	if (!ae_bots_calls_on() || game_connection() == _game_connection_network_client || !game_engine_has_teams() || !spot ||
		bot_index < 0 || bot_index >= BOTS_BRAINS || bot->player_index == NONE)
	{
		return FALSE;
	}
	ae_bots_calls_sync_player(bot_index, bot->player_index);
	player = player_try_and_get(bot->player_index);
	if (!player)
		return FALSE;
	team_index = player->team_index;
	if (team_index < 0 || team_index >= AE_BOTS_CALLS_TEAMS)
		return FALSE;
	alive = player->unit_index != NONE;
	unit = alive ? (struct unit_datum *)object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit) : NULL;
	alive = unit != NULL;
	vehicle_state = &ae_bots_vehicle_bots[bot_index];
	is_driver = alive && unit->object.parent_object_index != NONE && vehicle_state->vehicle_index != NONE &&
		vehicle_state->seat == AE_BOTS_SEAT_DRIVER;
	on_foot_or_driver = alive && (unit->object.parent_object_index == NONE || is_driver);
	has_person = ae_bots_calls_team_state(team_index, entry, spot, &person_marked);
	level = ae_bots_level_of_skill(bots_globals.skill);
	if (person_marked || !ae_bots_call_allowed(&ae_bots_calls.teams[team_index], ae_bots_calls.last_tick[bot_index],
		level == AE_BOTS_LEVEL_RECRUIT || level == AE_BOTS_LEVEL_EASY, has_person, alive, on_foot_or_driver, is_driver, entry,
		spot->x, spot->y, spot->z, now))
	{
		return FALSE;
	}
	/* (a call the host refused, out of reach or out of sight or at its guard, is not counted or logged) */
	if (!ae_mark_calls_bot_call(DATUM_INDEX_TO_ABSOLUTE_INDEX(bot->player_index), entry, kind, spot))
		return FALSE;
	ae_bots_call_made(&ae_bots_calls.teams[team_index], entry, spot->x, spot->y, spot->z, now);
	ae_bots_calls.last_tick[bot_index] = now;
	if (ae_bots_events_on())
		platform_log("bots: ev=call bot=%ld entry=%d kind=%d x=%.1f y=%.1f z=%.1f", bot_index + 1, entry, (int)kind,
			(double)spot->x, (double)spot->y, (double)spot->z);
	return TRUE;
}

/* REGROUP: once a tick per bot. The first bot of a crew still walking its route, not fighting, whose route-walking crewmates'
farthest has been over the distance away for the time in a row, calls it from where it stands. A crew whose route has ended
(its members spread to their places) does not call */

/* (walking a route: the squad's route is set for this life and was asked for within the last second. A route left for
another job, a power item or a vehicle keeps its active mark until the next route job, so the mark alone is not enough) */
static boolean ae_bots_calls_walking(long bot_index, long unit_index, long now)
{
	return ae_squads.bots[bot_index].active && unit_index != NONE && ae_squads.bots[bot_index].unit_index == unit_index &&
		now - ae_squads.bots[bot_index].last_tick <= TICKS_PER_SECOND;
}

static void ae_bots_calls_tick(struct bots_bot *bot, long bot_index, struct bots_tick *tick)
{
	long index, crew;
	real farthest = 0.0f;
	boolean first = TRUE;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS)
		return;
	ae_bots_calls_sync_player(bot_index, bots_globals.bots[bot_index].player_index);
	crew = ae_squads.bots[bot_index].crew;
	if (crew <= 0 || !ae_bots_calls_walking(bot_index, tick->player->unit_index, tick->now) || tick->fighting || tick->unit->object.parent_object_index != NONE || !ae_bots_calls_on())
	{
		ae_bots_calls.spread_since[bot_index] = 0;
		return;
	}
	for (index = 0; index < BOTS_BRAINS; index++)
	{
		struct bots_bot *other = &bots_globals.bots[index];
		struct player_datum *player;
		real_point3d point;
		real distance;

		if (index == bot_index || other->player_index == NONE || ae_squads.bots[index].crew != crew)
			continue;
		player = player_try_and_get(other->player_index);
		if (!player || player->unit_index == NONE || player->team_index != tick->player->team_index ||
			!ae_bots_calls_walking(index, player->unit_index, tick->now))
			continue;
		if (index < bot_index)
			first = FALSE;
		if (!object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit))
			continue;
		/* (a rider's place in the world, not its seat offset) */
		point = ae_bots_vehicle_point(player->unit_index);
		distance = bots_distance3d(&point, &tick->position);
		if (distance > farthest)
			farthest = distance;
	}
	if (!first || farthest <= AE_BOTS_CALLS_SPREAD_DISTANCE)
	{
		ae_bots_calls.spread_since[bot_index] = 0;
		return;
	}
	if (ae_bots_calls.spread_since[bot_index] == 0)
		ae_bots_calls.spread_since[bot_index] = tick->now;
	if (tick->now - ae_bots_calls.spread_since[bot_index] >= AE_BOTS_CALLS_SPREAD_TICKS)
	{
		ae_bots_call(bot, AE_WHEEL_REGROUP, AE_MARK_KIND_PLACE, &tick->position);
		/* (the next call, if the crew is still apart, counts a full time again) */
		ae_bots_calls.spread_since[bot_index] = tick->now;
	}
}
