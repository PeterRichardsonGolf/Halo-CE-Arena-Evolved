/*
AE_BOTS_OBJECTIVE.C

The bots' objective play (ae_bots_objective.h): the game's state as the bots
read it, once a tick, and halo.log's objective lines. The choice is the pure
rules' (ae_bots_objective_rules.c); the goals it gives are bots.c's, in
ae_bots_objective.inc.

The state is the host's own (a local game's, a LAN host's): what the game's
types keep (game_engine_ctf.c's flags, the oddball engine's holders, the
hill, the race's next flags) and what every player sees of it (each mode
shows its flag, ball, hill or juggernaut on the HUD), never more.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "items/items.h"
#include "items/weapons.h"
#include "units/units.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_groups.h"

#include "bots.h"
#include "ae_juggernaut.h"
#include "ae_strip_engine.h"
#include "ae_bots_objective.h"
#include "ae_bots_combat.h"

#include <string.h>
#include <stdio.h>

/* port/linux/src/port_config.c */
long config_integer(char const *name);
void platform_log(char const *format, ...);

/* the oddball engine's ball types (game_engine_oddball.c's enum oddball_ball_type) */
#define AE_BOTS_ODDBALL_MAGIC 1
#define AE_BOTS_ODDBALL_TERMINATOR 2
/* the HUD's marker stands above its point (game_engine_set_goal_position) */
#define AE_BOTS_GOAL_MARKER_HEIGHT 0.63f
/* a bot entering the hill again this soon is not logged again */
#define AE_BOTS_HILL_LOG_TICKS (5 * TICKS_PER_SECOND)

/* ---------- which games bots play */

boolean ae_bots_engine_played(long game_engine_index)
{
	switch (game_engine_index)
	{
	case game_engine_slayer:
	case game_engine_ctf:
	case game_engine_oddball:
	case game_engine_king:
	case game_engine_race:
		return TRUE;
	default:
		return FALSE;
	}
}

/* ---------- the state */

static struct ae_bots_objective_snapshot snapshot;
static boolean snapshot_valid;

/* the minute's line, counted from the game's start */
static struct
{
	long last_report_tick;
	long ticks;
	/* ticks some player held the ball, or was in the hill; ticks a bot did */
	long held_ticks;
	long bot_held_ticks;
	long bot_held_counted_tick;
	/* the bots' events */
	long grabs, captures, returns, ball_pickups, hill_entries, laps, race_touches;
	/* bot ticks by behaviour */
	long behaviour_ticks[AE_BOTS_BEHAVIOURS];
} totals;

static struct
{
	long player_index;
	boolean carrying;
	boolean in_hill;
	long hill_logged_tick;
	short grabs, returns, scores, laps;
	long race_touched;
	short behaviour;
} notes[BOTS_BRAINS];

static short ae_bots_objective_mode(void)
{
	struct game_variant *variant;

	if (!game_engine)
		return AE_BOTS_MODE_SLAYER;
	variant = game_engine_get_variant();
	switch (variant->game_engine_index)
	{
	case game_engine_ctf:
		return AE_BOTS_MODE_CTF;
	case game_engine_oddball:
		if (ae_juggernaut_active())
			return AE_BOTS_MODE_JUGGERNAUT;
		if (variant->game_engine_variant.oddball.oddball_ball_type == AE_BOTS_ODDBALL_MAGIC)
			return AE_BOTS_MODE_TAG;
		if (variant->game_engine_variant.oddball.oddball_ball_type == AE_BOTS_ODDBALL_TERMINATOR)
			return AE_BOTS_MODE_JUGGERNAUT;
		return AE_BOTS_MODE_ODDBALL;
	case game_engine_king:
		return AE_BOTS_MODE_KING;
	case game_engine_race:
		return AE_BOTS_MODE_RACE;
	default:
		return AE_BOTS_MODE_SLAYER;
	}
}

static void ae_bots_objective_read_ctf(void)
{
	long team_index;

	for (team_index = 0; team_index < 2; team_index++)
	{
		struct ae_bots_objective_flag *flag = &snapshot.flags[team_index];
		boolean handled = FALSE;
		struct weapon_datum *weapon;

		csmemset(flag, 0, sizeof(*flag));
		flag->weapon_index = NONE;
		flag->carrier_player_index = NONE;
		if (!ae_bots_ctf_flag(team_index, &flag->weapon_index, &flag->stand, &handled))
			continue;
		flag->exists = TRUE;
		flag->position = flag->stand;
		flag->home = TRUE;
		weapon = flag->weapon_index != NONE ? weapon_try_and_get(flag->weapon_index) : NULL;
		if (!weapon)
		{
			flag->weapon_index = NONE;
			continue;
		}
		item_get_position_even_if_in_inventory(flag->weapon_index, &flag->position);
		flag->home = !handled;
		if (weapon->object.parent_object_index != NONE)
		{
			struct unit_datum *unit = unit_try_and_get(weapon->object.parent_object_index);

			if (unit)
				flag->carrier_player_index = unit->unit.player_index;
		}
	}
}

static void ae_bots_objective_read_balls(void)
{
	struct object_iterator iterator;

	snapshot.ball_count = 0;
	object_iterator_new(&iterator, _object_mask_weapon, 0);
	while (object_iterator_next(&iterator) && snapshot.ball_count < AE_BOTS_OBJECTIVE_MAXIMUM_BALLS)
	{
		struct item_datum *item = item_get(iterator.index);

		if (!weapon_is_flag(iterator.index) || item->object.parent_object_index != NONE ||
			!TEST_FLAG(item->object.flags, _object_connected_to_map_bit) ||
			TEST_FLAG(item->item.flags, _item_attached_to_unit_bit))
		{
			continue;
		}
		snapshot.ball_indices[snapshot.ball_count] = iterator.index;
		snapshot.ball_positions[snapshot.ball_count] = item->object.position;
		snapshot.ball_count++;
	}
}

static void ae_bots_objective_read_race(void)
{
	struct scenario *scenario = global_scenario_get();
	long index;

	snapshot.race_flag_count = 0;
	for (index = 0; index < scenario->netgame_flags.count &&
		snapshot.race_flag_count < AE_BOTS_OBJECTIVE_MAXIMUM_RACE_FLAGS; index++)
	{
		struct scenario_netgame_flag *flag = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index,
			struct scenario_netgame_flag);

		if (flag->type != _netgame_flag_race_track)
			continue;
		snapshot.race_flag_orders[snapshot.race_flag_count] = flag->team_index;
		snapshot.race_flag_positions[snapshot.race_flag_count] = flag->position;
		snapshot.race_flag_count++;
	}
}

static void ae_bots_objective_report(void)
{
	char line[480];
	int length = 0;
	short behaviour;
	long bot_ticks = 0;

	for (behaviour = 0; behaviour < AE_BOTS_BEHAVIOURS; behaviour++)
		bot_ticks += totals.behaviour_ticks[behaviour];
	if (!bot_ticks)
		return;
	line[0] = 0;
	switch (snapshot.mode)
	{
	case AE_BOTS_MODE_CTF:
		length += snprintf(line + length, sizeof(line) - (size_t)length, "took %ld, captured %ld, returned %ld",
			totals.grabs, totals.captures, totals.returns);
		break;
	case AE_BOTS_MODE_ODDBALL:
	case AE_BOTS_MODE_TAG:
	case AE_BOTS_MODE_JUGGERNAUT:
		length += snprintf(line + length, sizeof(line) - (size_t)length,
			"%s %ld times, held %ld%% of the time (by bots %ld%%)",
			snapshot.mode == AE_BOTS_MODE_JUGGERNAUT ? "became the juggernaut" : snapshot.mode == AE_BOTS_MODE_TAG ?
			"became it" : "picked up the ball", totals.ball_pickups,
			totals.ticks ? totals.held_ticks * 100 / totals.ticks : 0L,
			totals.ticks ? totals.bot_held_ticks * 100 / totals.ticks : 0L);
		break;
	case AE_BOTS_MODE_KING:
		length += snprintf(line + length, sizeof(line) - (size_t)length,
			"entered the hill %ld times, hill held %ld%% of the time (by bots %ld%%)", totals.hill_entries,
			totals.ticks ? totals.held_ticks * 100 / totals.ticks : 0L,
			totals.ticks ? totals.bot_held_ticks * 100 / totals.ticks : 0L);
		break;
	case AE_BOTS_MODE_RACE:
		length += snprintf(line + length, sizeof(line) - (size_t)length, "flags touched %ld, laps %ld", totals.race_touches,
			totals.laps);
		break;
	default:
		break;
	}
	length += snprintf(line + length, sizeof(line) - (size_t)length, "; behaviours");
	for (behaviour = 0; behaviour < AE_BOTS_BEHAVIOURS; behaviour++)
	{
		if (totals.behaviour_ticks[behaviour] && length < (int)sizeof(line) - 24)
			length += snprintf(line + length, sizeof(line) - (size_t)length, " %s %ld%%", ae_bots_behaviour_name(behaviour),
				totals.behaviour_ticks[behaviour] * 100 / bot_ticks);
	}
	platform_log("bots: objective tick %ld (%s): %s", snapshot.tick, ae_bots_mode_name(snapshot.mode), line);
}

struct ae_bots_objective_snapshot const *ae_bots_objective_snapshot(void)
{
	long now;

	if (!game_engine || !game_engine_running())
	{
		snapshot_valid = FALSE;
		return NULL;
	}
	now = game_time_get();
	if (snapshot_valid && snapshot.tick == now)
		return &snapshot;
	/* (a new game: its own counts) */
	if (!snapshot_valid || now < snapshot.tick)
	{
		csmemset(&totals, 0, sizeof(totals));
		csmemset(notes, 0, sizeof(notes));
		{
			long index;

			for (index = 0; index < BOTS_BRAINS; index++)
				notes[index].player_index = NONE;
		}
		totals.last_report_tick = now;
		totals.bot_held_counted_tick = NONE;
	}
	snapshot_valid = TRUE;
	snapshot.tick = now;
	snapshot.mode = ae_bots_objective_mode();
	snapshot.teams = game_engine_has_teams();
	snapshot.ball_count = 0;
	snapshot.race_flag_count = 0;
	snapshot.hill = FALSE;
	csmemset(snapshot.flags, 0, sizeof(snapshot.flags));
	snapshot.flags[0].weapon_index = snapshot.flags[1].weapon_index = NONE;
	snapshot.flags[0].carrier_player_index = snapshot.flags[1].carrier_player_index = NONE;
	switch (snapshot.mode)
	{
	case AE_BOTS_MODE_CTF:
		ae_bots_objective_read_ctf();
		break;
	case AE_BOTS_MODE_ODDBALL:
		ae_bots_objective_read_balls();
		break;
	case AE_BOTS_MODE_KING:
		if (game_engine_get_goal_in_use(0))
		{
			game_engine_get_goal_position(&snapshot.hill_centre, 0);
			snapshot.hill_centre.z -= AE_BOTS_GOAL_MARKER_HEIGHT;
			snapshot.hill = TRUE;
		}
		break;
	case AE_BOTS_MODE_RACE:
		ae_bots_objective_read_race();
		break;
	default:
		break;
	}
	/* (the share of the time someone held the ball or the hill) */
	if (snapshot.mode != AE_BOTS_MODE_SLAYER && snapshot.mode != AE_BOTS_MODE_CTF && snapshot.mode != AE_BOTS_MODE_RACE)
	{
		struct data_iterator iterator;
		struct player_datum *player;
		boolean held = FALSE;

		data_iterator_new(&iterator, player_data);
		while (!held && (player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			held = snapshot.mode == AE_BOTS_MODE_KING ? ae_bots_objective_player_in_hill(iterator.datum_index) :
				ae_bots_objective_player_carries(&snapshot, iterator.datum_index);
		}
		totals.held_ticks += held ? 1 : 0;
	}
	totals.ticks++;
	if (now - totals.last_report_tick >= TICKS_PER_SECOND * 60)
	{
		totals.last_report_tick = now;
		ae_bots_objective_report();
	}
	return &snapshot;
}

boolean ae_bots_objective_player_carries(struct ae_bots_objective_snapshot const *state, long player_index)
{
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;

	if (!state || !player || player->unit_index == NONE)
		return FALSE;
	switch (state->mode)
	{
	case AE_BOTS_MODE_CTF:
		return state->flags[0].carrier_player_index == player_index || state->flags[1].carrier_player_index == player_index;
	case AE_BOTS_MODE_ODDBALL:
	case AE_BOTS_MODE_TAG:
	case AE_BOTS_MODE_JUGGERNAUT:
		return ae_strip_oddball_ball_count(player_index) > 0 || (ae_juggernaut_active() && ae_juggernaut_player_is(player_index));
	default:
		return FALSE;
	}
}

boolean ae_bots_objective_player_in_hill(long player_index)
{
	return snapshot_valid && snapshot.mode == AE_BOTS_MODE_KING && snapshot.hill && player_index != NONE &&
		ae_strip_king_player_on_hill(player_index);
}

boolean ae_bots_objective_race_may_touch(long player_index, long order)
{
	return player_index != NONE && ae_bots_race_can_touch(player_index, order);
}

/* ---------- the bots' lines */

void ae_bots_objective_note(long bot_index, long player_index, short behaviour)
{
	struct player_datum *player = player_try_and_get(player_index);
	union multiplayer_statistics *statistics;
	boolean carrying, in_hill;
	long n = bot_index + 1;

	if (!snapshot_valid || !player || bot_index < 0 || bot_index >= BOTS_BRAINS ||
		behaviour < 0 || behaviour >= AE_BOTS_BEHAVIOURS)
	{
		return;
	}
	statistics = &player->statistics.multiplayer_statistics;
	totals.behaviour_ticks[behaviour]++;
	carrying = ae_bots_objective_player_carries(&snapshot, player_index);
	in_hill = ae_bots_objective_player_in_hill(player_index);
	if (notes[bot_index].player_index != player_index)
	{
		csmemset(&notes[bot_index], 0, sizeof(notes[bot_index]));
		notes[bot_index].player_index = player_index;
		notes[bot_index].grabs = statistics->ctf_statistics.flag_grabs;
		notes[bot_index].returns = statistics->ctf_statistics.flag_returns;
		notes[bot_index].scores = statistics->ctf_statistics.flag_scores;
		notes[bot_index].laps = statistics->race_statistics.laps;
		notes[bot_index].race_touched = snapshot.mode == AE_BOTS_MODE_RACE ? ae_strip_race_flags_touched(player_index) : 0;
		notes[bot_index].carrying = carrying;
		notes[bot_index].in_hill = in_hill;
		notes[bot_index].behaviour = behaviour;
		return;
	}
	/* (debug.bot_trace: each change of what it does) */
	if (behaviour != notes[bot_index].behaviour)
	{
		if (ae_bots_trace_level() > 0)
			platform_log("bots: trace %ld bot %ld now %s", snapshot.tick, n, ae_bots_behaviour_name(behaviour));
		notes[bot_index].behaviour = behaviour;
	}
	switch (snapshot.mode)
	{
	case AE_BOTS_MODE_CTF:
		if (carrying && !notes[bot_index].carrying)
		{
			totals.grabs++;
			platform_log("bots: bot %ld took the flag", n);
		}
		if (statistics->ctf_statistics.flag_scores > notes[bot_index].scores)
		{
			totals.captures++;
			platform_log("bots: bot %ld captured the flag", n);
		}
		else if (!carrying && notes[bot_index].carrying)
			platform_log("bots: bot %ld dropped the flag", n);
		if (statistics->ctf_statistics.flag_returns > notes[bot_index].returns)
		{
			totals.returns++;
			platform_log("bots: bot %ld returned the flag", n);
		}
		notes[bot_index].grabs = statistics->ctf_statistics.flag_grabs;
		notes[bot_index].returns = statistics->ctf_statistics.flag_returns;
		notes[bot_index].scores = statistics->ctf_statistics.flag_scores;
		break;
	case AE_BOTS_MODE_ODDBALL:
	case AE_BOTS_MODE_TAG:
	case AE_BOTS_MODE_JUGGERNAUT:
		if (carrying && totals.bot_held_counted_tick != snapshot.tick)
		{
			totals.bot_held_ticks++;
			totals.bot_held_counted_tick = snapshot.tick;
		}
		if (carrying && !notes[bot_index].carrying)
		{
			totals.ball_pickups++;
			platform_log("bots: bot %ld %s", n, snapshot.mode == AE_BOTS_MODE_JUGGERNAUT ? "is the juggernaut" :
				snapshot.mode == AE_BOTS_MODE_TAG ? "is it" : "holds the ball");
		}
		else if (!carrying && notes[bot_index].carrying && snapshot.mode == AE_BOTS_MODE_ODDBALL)
			platform_log("bots: bot %ld dropped the ball", n);
		break;
	case AE_BOTS_MODE_KING:
		if (in_hill && totals.bot_held_counted_tick != snapshot.tick)
		{
			totals.bot_held_ticks++;
			totals.bot_held_counted_tick = snapshot.tick;
		}
		if (in_hill && !notes[bot_index].in_hill)
		{
			totals.hill_entries++;
			if (snapshot.tick - notes[bot_index].hill_logged_tick >= AE_BOTS_HILL_LOG_TICKS ||
				!notes[bot_index].hill_logged_tick)
			{
				platform_log("bots: bot %ld in the hill", n);
			}
			notes[bot_index].hill_logged_tick = snapshot.tick;
		}
		break;
	case AE_BOTS_MODE_RACE:
		if (statistics->race_statistics.laps > notes[bot_index].laps)
		{
			totals.laps++;
			platform_log("bots: bot %ld finished a lap", n);
		}
		notes[bot_index].laps = statistics->race_statistics.laps;
		{
			long touched = ae_strip_race_flags_touched(player_index);

			if (touched > notes[bot_index].race_touched)
				totals.race_touches += touched - notes[bot_index].race_touched;
			notes[bot_index].race_touched = touched;
		}
		break;
	default:
		break;
	}
	notes[bot_index].carrying = carrying;
	notes[bot_index].in_hill = in_hill;
}
