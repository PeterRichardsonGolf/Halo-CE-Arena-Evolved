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
#include "ae_bomb_assault.h"
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
#ifdef HALO_GAME_BROWSER
	/* (Bomb and Grifball: ae_bots_objective_read_bomb) */
	case game_engine_bomb:
#endif
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
	long grabs, captures, returns, ball_pickups, hill_entries, laps, race_touches, plants, detonations;
	long fight_drops, fight_pickups;
	long wingman_carried_ticks, wingman_ticks;
	long snd_defender_ticks, snd_hold_ticks;
	/* SEARCH AND DESTROY: bot ticks by side (0 attackers, 1 defenders) and where (home, mid, the enemy site), by role, the
	roles' changes, and the map data a role lacked */
	long snd_zone_ticks[2][3], snd_role_ticks[AE_BOTS_SND_ROLES], snd_role_changes;
	long snd_no_goal_ticks, snd_no_spot, snd_lane_failed, snd_lane_made;
	long wingman_carried_counted_tick[2], wingman_counted_tick[2];
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
#ifdef HALO_GAME_BROWSER
	case game_engine_bomb:
		return AE_BOTS_MODE_BOMB;
#endif
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

/* the bomb, as its markers show it to everyone: lying loose or planted (BOMB_GOAL_BOMB), its carrier (to both teams),
the goals (to the team that attacks each) */
static void ae_bots_objective_read_bomb(void)
{
#ifdef HALO_GAME_BROWSER
	struct item_datum *item;
	long team_index;

	snapshot.bomb_weapon_index = game_engine_bomb_weapon();
	snapshot.bomb_planted_goal = game_engine_bomb_planted_goal();
	snapshot.bomb_melee_only = game_engine_bomb_melee_only();
	snapshot.bomb_attacking_team = ae_bomb_assault_attacking_team();
	for (team_index = 0; team_index < 2; team_index++)
		snapshot.bomb_has_goal[team_index] = game_engine_bomb_goal(team_index, &snapshot.bomb_goals[team_index]);
	item = snapshot.bomb_weapon_index != NONE ?
		(struct item_datum *)object_try_and_get_and_verify_type(snapshot.bomb_weapon_index, _object_mask_weapon) : NULL;
	if (!item)
	{
		snapshot.bomb_weapon_index = NONE;
		return;
	}
	item_get_position_even_if_in_inventory(snapshot.bomb_weapon_index, &snapshot.bomb_position);
	/* (its carrier: the bomb in its hands, or holstered: game_engine_bomb.c's bomb_engine_player_update) */
	{
		struct data_iterator iterator;
		struct player_datum *player;

		data_iterator_new(&iterator, player_data);
		while (snapshot.bomb_carrier_player_index == NONE &&
			(player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			if (player->unit_index != NONE && unit_has_weapon(player->unit_index, snapshot.bomb_weapon_index))
				snapshot.bomb_carrier_player_index = iterator.datum_index;
		}
	}
	/* (taken: lying loose on the map, not planted in a goal, nor waiting there for its countdown and blast) */
	snapshot.bomb_takeable = snapshot.bomb_carrier_player_index == NONE && item->object.parent_object_index == NONE &&
		TEST_FLAG(item->object.flags, _object_connected_to_map_bit) &&
		!TEST_FLAG(item->item.flags, _item_attached_to_unit_bit) && snapshot.bomb_planted_goal == NONE &&
		game_engine_bomb_countdown() == 0 && !game_engine_bomb_holds_spawns();
#endif
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
	case AE_BOTS_MODE_BOMB:
		length += snprintf(line + length, sizeof(line) - (size_t)length,
			"took the bomb %ld times, planted %ld, detonated %ld, carried %ld%% of the time (by bots %ld%%)", totals.grabs,
			totals.plants, totals.detonations, totals.ticks ? totals.held_ticks * 100 / totals.ticks : 0L,
			totals.ticks ? totals.bot_held_ticks * 100 / totals.ticks : 0L);
		break;
	default:
		break;
	}
	if (snapshot.mode == AE_BOTS_MODE_ODDBALL || snapshot.mode == AE_BOTS_MODE_BOMB)
	{
		length += snprintf(line + length, sizeof(line) - (size_t)length,
			"; recovered %ld of %ld fight drops", totals.fight_pickups, totals.fight_drops);
	}
	if (totals.wingman_carried_ticks)
		length += snprintf(line + length, sizeof(line) - (size_t)length, "; wingman %ld%% of carried time",
			totals.wingman_ticks * 100 / totals.wingman_carried_ticks);
	if (totals.snd_defender_ticks)
		length += snprintf(line + length, sizeof(line) - (size_t)length, "; defenders holding %ld%%",
			totals.snd_hold_ticks * 100 / totals.snd_defender_ticks);
	length += snprintf(line + length, sizeof(line) - (size_t)length, "; behaviours");
	for (behaviour = 0; behaviour < AE_BOTS_BEHAVIOURS; behaviour++)
	{
		if (totals.behaviour_ticks[behaviour] && length < (int)sizeof(line) - 24)
			length += snprintf(line + length, sizeof(line) - (size_t)length, " %s %ld%%", ae_bots_behaviour_name(behaviour),
				totals.behaviour_ticks[behaviour] * 100 / bot_ticks);
	}
	platform_log("bots: objective tick %ld (%s): %s", snapshot.tick, ae_bots_mode_name(snapshot.mode), line);
	if (snapshot.mode == AE_BOTS_MODE_BOMB && (totals.snd_zone_ticks[0][0] || totals.snd_zone_ticks[1][0]))
	{
		static char const *const sides[2] = { "attackers", "defenders" };
		int side, role;

		length = 0;
		for (side = 0; side < 2; side++)
		{
			long all = totals.snd_zone_ticks[side][0] + totals.snd_zone_ticks[side][1] + totals.snd_zone_ticks[side][2];

			if (all)
				length += snprintf(line + length, sizeof(line) - (size_t)length, "%s%s home %ld%% mid %ld%% site %ld%%",
					side ? "; " : "", sides[side], totals.snd_zone_ticks[side][0] * 100 / all,
					totals.snd_zone_ticks[side][1] * 100 / all, totals.snd_zone_ticks[side][2] * 100 / all);
		}
		length += snprintf(line + length, sizeof(line) - (size_t)length, "; roles");
		for (role = 0; role < AE_BOTS_SND_ROLES; role++)
		{
			if (totals.snd_role_ticks[role] && length < (int)sizeof(line) - 24)
				length += snprintf(line + length, sizeof(line) - (size_t)length, " %s %ld%%", ae_bots_snd_role_name(role),
					totals.snd_role_ticks[role] * 100 / (totals.snd_zone_ticks[0][0] + totals.snd_zone_ticks[0][1] +
					totals.snd_zone_ticks[0][2] + totals.snd_zone_ticks[1][0] + totals.snd_zone_ticks[1][1] +
					totals.snd_zone_ticks[1][2]));
		}
		platform_log("bots: snd tick %ld: %s; role changes %ld, no goal %ld ticks, no site spot %ld, lanes made %ld failed %ld",
			snapshot.tick, line, totals.snd_role_changes, totals.snd_no_goal_ticks, totals.snd_no_spot,
			totals.snd_lane_made, totals.snd_lane_failed);
	}
}

void ae_bots_objective_fight_drop_note(long bot_index, boolean picked_up)
{
	char const *name;

	if (!snapshot_valid || bot_index < 0 || bot_index >= BOTS_BRAINS)
		return;
	name = snapshot.mode == AE_BOTS_MODE_BOMB ? "bomb" : "ball";
	if (picked_up)
	{
		totals.fight_pickups++;
		platform_log("bots: bot %ld picked up the %s after fighting", bot_index + 1, name);
	}
	else
	{
		totals.fight_drops++;
		platform_log("bots: bot %ld dropped the %s to fight", bot_index + 1, name);
	}
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
		totals.wingman_carried_counted_tick[0] = totals.wingman_carried_counted_tick[1] = NONE;
		totals.wingman_counted_tick[0] = totals.wingman_counted_tick[1] = NONE;
	}
	snapshot_valid = TRUE;
	snapshot.tick = now;
	snapshot.mode = ae_bots_objective_mode();
	snapshot.teams = game_engine_has_teams();
	snapshot.ball_count = 0;
	snapshot.race_flag_count = 0;
	snapshot.hill = FALSE;
	snapshot.bomb_weapon_index = NONE;
	snapshot.bomb_takeable = FALSE;
	snapshot.bomb_attacking_team = NONE;
	snapshot.bomb_carrier_player_index = NONE;
	snapshot.bomb_planted_goal = NONE;
	snapshot.bomb_has_goal[0] = snapshot.bomb_has_goal[1] = FALSE;
	snapshot.bomb_melee_only = FALSE;
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
	case AE_BOTS_MODE_BOMB:
		ae_bots_objective_read_bomb();
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

void ae_bots_objective_strategy_note(long team_index, boolean carrier_out, boolean wingman,
	boolean snd_defending, boolean holding)
{
	if (!snapshot_valid || !VALID_INDEX(team_index, 2))
		return;
	if (carrier_out && totals.wingman_carried_counted_tick[team_index] != snapshot.tick)
	{
		totals.wingman_carried_counted_tick[team_index] = snapshot.tick;
		totals.wingman_carried_ticks++;
	}
	if (wingman && totals.wingman_counted_tick[team_index] != snapshot.tick)
	{
		totals.wingman_counted_tick[team_index] = snapshot.tick;
		totals.wingman_ticks++;
	}
	if (snd_defending)
	{
		totals.snd_defender_ticks++;
		totals.snd_hold_ticks += holding ? 1 : 0;
	}
}

void ae_bots_objective_snd_note(boolean attacking, int zone, int role, boolean role_changed)
{
	if (!snapshot_valid || !VALID_INDEX(zone, 3) || !VALID_INDEX(role, AE_BOTS_SND_ROLES))
		return;
	totals.snd_zone_ticks[attacking ? 0 : 1][zone]++;
	totals.snd_role_ticks[role]++;
	totals.snd_role_changes += role_changed ? 1 : 0;
}

void ae_bots_objective_snd_event(int kind)
{
	if (!snapshot_valid)
		return;
	switch (kind)
	{
	case AE_BOTS_SND_EVENT_NO_GOAL:
		totals.snd_no_goal_ticks++;
		break;
	case AE_BOTS_SND_EVENT_NO_SPOT:
		totals.snd_no_spot++;
		break;
	case AE_BOTS_SND_EVENT_LANE_MADE:
		totals.snd_lane_made++;
		break;
	case AE_BOTS_SND_EVENT_LANE_FAILED:
		totals.snd_lane_failed++;
		break;
	}
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
	case AE_BOTS_MODE_BOMB:
		return state->bomb_carrier_player_index == player_index;
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
	case AE_BOTS_MODE_BOMB:
		/* (the bomb's goal is counted as a CTF capture: game_engine_bomb.c's bomb_award_score) */
		if (carrying && totals.bot_held_counted_tick != snapshot.tick)
		{
			totals.bot_held_ticks++;
			totals.bot_held_counted_tick = snapshot.tick;
		}
		if (carrying && !notes[bot_index].carrying)
		{
			totals.grabs++;
			platform_log("bots: bot %ld took the bomb", n);
		}
		if (statistics->ctf_statistics.flag_scores > notes[bot_index].scores)
		{
			totals.detonations++;
			platform_log("bots: bot %ld's bomb went off", n);
		}
		else if (!carrying && notes[bot_index].carrying && snapshot.bomb_planted_goal == NONE)
			platform_log("bots: bot %ld dropped the bomb", n);
		else if (!carrying && notes[bot_index].carrying)
		{
			totals.plants++;
			platform_log("bots: bot %ld planted the bomb", n);
		}
		notes[bot_index].scores = statistics->ctf_statistics.flag_scores;
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
