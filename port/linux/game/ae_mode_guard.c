/*
AE_MODE_GUARD.C

Bug 139: a mode the loaded map cannot play (ae_mode_guard.h).
*/

#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"
#include "game/game_engine.h"
#include "game/players_runtime.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_groups.h"

#include "ae_mode_guard.h"
#include "ae_mode_guard_rules.h"

#include <stdio.h>

void platform_log(char const *format, ...);

static struct
{
	boolean any_spawns;
	boolean changed;
	wchar_t message[64];
} ae_mode_guard;

boolean ae_mode_guard_new_map(struct game_variant *variant)
{
	struct scenario *scenario = global_scenario_get();
	struct ae_mode_guard_map map;
	char const *missing;
	long mode = variant->game_engine_index, index;
	short count, spawns_for_mode = 0;

	csmemset(&ae_mode_guard, 0, sizeof(ae_mode_guard));
	csmemset(&map, 0, sizeof(map));
	if (!scenario)
		return FALSE;
	for (index = 0; index < scenario->netgame_flags.count; index++)
	{
		struct scenario_netgame_flag const *flag =
			TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index, struct scenario_netgame_flag);

		if (flag->type == _netgame_flag_ctf_flag && (flag->team_index == 0 || flag->team_index == 1))
			map.ctf_flags[flag->team_index]++;
		else if (flag->type == _netgame_flag_hill)
			map.hills++;
		else if (flag->type == _netgame_flag_oddball_ball_spawn)
			map.ball_spawns++;
		else if (flag->type == _netgame_flag_race_track)
			map.race_flags++;
	}
	missing = ae_mode_guard_missing(mode, &map);
	if (missing)
	{
		char text[64];

		platform_log("mode guard: the map has no %s for %s: played as Slayer", missing, ae_mode_guard_mode_name(mode));
		snprintf(text, sizeof(text), "NO %s ON THIS MAP: PLAYING SLAYER", ae_mode_guard_mode_name(mode));
		for (index = 0; text[index] && index < (long)NUMBEROF(ae_mode_guard.message) - 1; index++)
			ae_mode_guard.message[index] = (wchar_t)(unsigned char)text[index];
		ae_mode_guard.message[index] = 0;
		ae_mode_guard.changed = TRUE;
		variant->game_engine_index = game_engine_slayer;
		csmemset(&variant->game_engine_variant, 0, sizeof(variant->game_engine_variant));
		variant->universal_variant.score_to_win = ae_mode_guard_slayer_score(variant->universal_variant.teams);
		platform_log("mode guard: Slayer to %ld", variant->universal_variant.score_to_win);
		mode = game_engine_slayer;
	}
	count = player_get_starting_location_count();
	for (index = 0; index < count; index++)
	{
		if (match_game_type(mode, 4, player_get_starting_location((short)index)->game_types))
			spawns_for_mode++;
	}
	ae_mode_guard.any_spawns = (boolean)ae_mode_guard_spawn_anywhere(spawns_for_mode, count);
	if (ae_mode_guard.any_spawns)
	{
		platform_log("mode guard: none of the map's %d spawns is for %s: players spawn at all of them", count,
			ae_mode_guard_mode_name(mode));
	}
	return ae_mode_guard.changed;
}

boolean ae_mode_guard_any_spawns(void)
{
	return ae_mode_guard.any_spawns;
}

wchar_t const *ae_mode_guard_message(void)
{
	return ae_mode_guard.changed ? ae_mode_guard.message : NULL;
}
