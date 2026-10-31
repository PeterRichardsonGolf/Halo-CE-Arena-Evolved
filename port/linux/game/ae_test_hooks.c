/*
AE_TEST_HOOKS.C

AE's test hooks in the game's code (ae_test_hooks.h), each OFF by default.
*/

#include "cseries.h"
#include "game/cheats.h"
#include "game/game_globals.h"
#include "game/game.h"
#include "game/players.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "tag_files/tag_files.h"

#include "../src/capture.h"
#include "ae_test_hooks.h"

/* the platform layer's (port/linux/src/port_config.c) */
int config_boolean(char const *name);
void platform_log(char const *format, ...);

/* ---------- debug.capture_on_event */

void ae_test_capture(const char *kind, int delay_ms)
{
	capture_test_event(kind, "", delay_ms);
}

void ae_test_capture_hud(const wchar_t *text)
{
	char ascii[64];
	long i;

	for (i = 0; text && i < (long)sizeof(ascii) - 1 && text[i]; i++)
		ascii[i] = text[i] > 0 && text[i] < 128 ? (char)text[i] : '?';
	ascii[i] = 0;
	capture_test_event("hud", ascii, 600);
}

void ae_test_capture_player_killed(long dead_player_index, long killing_player_index)
{
	short local_index;

	for (local_index = 0; local_index < MAXIMUM_LOCAL_PLAYERS; local_index++)
	{
		long local_player = local_player_get_player_index(local_index);

		if (local_player != NONE && local_player == dead_player_index)
			capture_test_event("death", "", 800);
		else if (local_player != NONE && local_player == killing_player_index)
			capture_test_event("kill", "", 300);
	}
}

/* ---------- debug.scoreboard_shown */

int ae_test_scoreboard_shown(void)
{
	/* (read once: an environment setting of a test run) */
	static int shown = -1;

	if (shown < 0)
		shown = config_boolean("debug.scoreboard_shown") ? 1 : 0;
	return shown;
}

/* ---------- debug.network_test_all_weapons */

static long weapons_and_equipment_count(void)
{
	struct object_iterator iterator;
	long count = 0;

	object_iterator_new(&iterator, _object_mask_weapon | _object_mask_equipment, 0);
	while (object_iterator_next(&iterator))
		count++;
	return count;
}

static void all_weapons_give(void)
{
	struct game_globals *globals = scenario_get_game_globals();
	long index, before, after;

	platform_log("network test: cheat_all_weapons at tick %ld: the map's list has %ld weapons",
		game_time_get(), (long)globals->weapon_list.count);
	for (index = 0; index < (long)globals->weapon_list.count && globals->weapon_list.address && index < 64; index++)
	{
		struct tag_reference *reference = TAG_BLOCK_GET_ELEMENT(&globals->weapon_list, index, struct tag_reference);

		platform_log("network test:   weapon %ld: %s", index,
			reference->index != NONE ? tag_get_name(reference->index) : "(none)");
	}
	before = weapons_and_equipment_count();
	cheat_all_weapons();
	after = weapons_and_equipment_count();
	platform_log("network test: cheat_all_weapons created %ld weapon and equipment objects (%ld before, %ld after)",
		after - before, before, after);
}

void ae_test_all_weapons_update(void)
{
	/* (read once: the setting is a launch setting; the unit it was done for, the tick it is due) */
	static int setting = -1;
	static long unit = NONE;
	static long due_tick;
	struct data_iterator iterator;
	struct player_datum *player;

	if (setting < 0)
		setting = config_boolean("debug.network_test_all_weapons") != 0;
	if (!setting)
		return;
	/* a new unit for the host's first local player: the map's weapons around it a second on (init.txt runs before
	any player exists) */
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->local_player_index == NONE)
			continue;
		if (player->unit_index == NONE)
		{
			unit = NONE;
		}
		else if (player->unit_index != unit)
		{
			unit = player->unit_index;
			due_tick = game_time_get() + TICKS_PER_SECOND;
		}
		else if (due_tick && game_time_get() >= due_tick)
		{
			due_tick = 0;
			all_weapons_give();
		}
		break;
	}
}
