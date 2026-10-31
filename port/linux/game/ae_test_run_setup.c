/* ae_test_run_setup.c: Arena Evolved TEST RUN, a test's setup applied in the running game (see
ae_test_run_setup.h). */

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "memory/data.h"
#include "game/players.h"
#include "../src/ae_config_players.h"
#include "../src/port_config.h"
#include "ae_hooks.h"
#include "ae_test_run_saved.h"
#include "ae_test_run_setup.h"

/* main.c's, platform.h's */
void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
void main_goto_main_menu(void);
boolean main_menu_is_active(void);

enum { PHASE_IDLE, PHASE_LEAVING, PHASE_LOADING, PHASE_READY };

/* the scenario must hold for this long before it is ready (the menus' 200 ms slide, the map's fade in) */
#define SCREEN_STABLE_MS 400
#define MAP_STABLE_MS 1000

static struct
{
	struct ae_test_run_saved saved;
	short phase, screen, players, menus;
	char id[32];
	char error[96];
	unsigned long stable_since;
} setup;

static int restore(const char *key, const char *value, void *context)
{
	(void)context;
	return config_set(key, value);
}

/* notes the key's value now and sets it: 0 when there is no such key */
static int change(const char *key, const char *value)
{
	char old[128];

	if (!config_text(key, old, sizeof(old)) || !ae_test_run_saved_note(&setup.saved, key, old) || !config_set(key, value))
		return 0;
	return 1;
}

/* (config_set leaves the setting's "chosen" flag set and does not run config_carry_over, so undo puts back the value
but not what other settings derive from it: a test's `set` keys should be plain settings) */
void ae_test_run_setup_undo(void)
{
	short changed = setup.saved.count;

	if (ae_test_run_saved_undo(&setup.saved, restore, NULL))
		platform_log("test run setup: a value could not be put back");
	/* (network_test read the test's settings: it reads the restored ones, and its host, rotation and bots stop) */
	if (changed)
		network_test_restart();
	setup.phase = PHASE_IDLE;
}

static int fail(const char *before, const char *text, const char *after)
{
	snprintf(setup.error, sizeof(setup.error), "%s%s%s", before, text, after);
	ae_test_run_setup_undo();
	return 0;
}

const char *ae_test_run_setup_error(void)
{
	return setup.error;
}

int ae_test_run_setup_apply(struct ae_test_run_test const *test)
{
	char value[128];
	char text[16];
	short index;

	ae_test_run_setup_undo();
	setup.error[0] = 0;
	snprintf(setup.id, sizeof(setup.id), "%s", test->id);
	setup.screen = test->screen;
	setup.menus = test->menus;
	setup.players = test->players > 1 ? test->players : 1;
	/* (with AE's menus off a map test takes the classic path, which does not start again in session) */
	if (!ae_menus_active())
		return fail("TEST RUN needs AE's menus on", "", "");
	/* (every key known first, so that an error changes nothing) */
	for (index = 0; index < test->sets; index++)
	{
		if (!config_text(test->set[index].key, value, sizeof(value)))
			return fail("no setting ", test->set[index].key, "");
		/* (config_text answers with the player's own value, config_set writes the global one) */
		if (ae_config_player_setting(test->set[index].key))
			return fail("", test->set[index].key, " is a per-player setting");
	}
	for (index = 0; index < test->sets; index++)
	{
		if (!change(test->set[index].key, test->set[index].value))
			return fail("cannot set ", test->set[index].key, "");
	}
	if (test->menus)
	{
		/* (a menus test loads nothing: the owner's own lobby or screen stays as it is; from a game, the main menu) */
		setup.phase = main_menu_is_active() ? PHASE_LOADING : PHASE_LEAVING;
		if (setup.phase == PHASE_LEAVING)
			main_goto_main_menu();
		setup.stable_since = 0;
		platform_log("test run setup: %s (menus, %d players here)", setup.id, (int)setup.players);
		return 1;
	}
	if (test->screen)
	{
		snprintf(text, sizeof(text), "%d", (int)test->screen);
		/* (no game to start from the config, the screen opened as the main menu next shows) */
		if (!change("debug.network_test", "") || !change("debug.ae_test_screen", text))
			return fail("cannot set ", "the screen", "");
	}
	else
	{
		if (test->gametype[0])
			snprintf(value, sizeof(value), "local:%s:%s", test->map, test->gametype);
		else
			snprintf(value, sizeof(value), "local:%s", test->map);
		snprintf(text, sizeof(text), "%d", (int)setup.players);
		/* (the gametype is also asked for as a saved or seeded one, "AE SLAYER": network_test_host_variant looks it up
		by debug.network_test_gametype first, and only then as a built-in variant's name, where an unknown name is an
		empty variant: no respawn, a game to 0) */
		if (!change("debug.network_test", value) || !change("debug.network_test_local_players", text) ||
			!change("debug.network_test_gametype", test->gametype) || !change("debug.ae_test_screen", "0"))
		{
			return fail("cannot set ", "the map", "");
		}
	}
	network_test_restart();
	ae_ui_test_screen_rearm();
	/* (a running game, or the main menu's scenario not yet loaded, goes to the main menu first) */
	setup.phase = main_menu_is_active() ? PHASE_LOADING : PHASE_LEAVING;
	if (setup.phase == PHASE_LEAVING)
		main_goto_main_menu();
	setup.stable_since = 0;
	platform_log("test run setup: %s (%s %s%s%s, %d players here)", setup.id, test->screen ? "screen" : "map",
		test->screen ? "" : test->map, test->gametype[0] ? " " : "", test->gametype, (int)setup.players);
	return 1;
}

/* a map test's game is playing: running past the pre-game countdown, the local players spawned */
static int map_playing(void)
{
	short local;

	if (!game_in_progress() || main_menu_is_active() || !game_engine_running() ||
		game_engine_pregame_countdown_ticks_left() > 0 || game_time_get() < 1)
	{
		return 0;
	}
	for (local = 0; local < setup.players; local++)
	{
		long player_index = local_player_get_player_index(local);

		if (player_index == NONE || player_get(player_index)->unit_index == NONE)
			return 0;
	}
	return 1;
}

int ae_test_run_setup_ready(void)
{
	unsigned long now = system_milliseconds();
	int up;

	if (setup.phase == PHASE_IDLE)
		return 0;
	if (setup.phase == PHASE_LEAVING && main_menu_is_active())
		setup.phase = PHASE_LOADING;
	/* (latched: once reached it stays 1 until the next apply or undo, whatever the game does after) */
	if (setup.phase == PHASE_READY)
		return 1;
	if (setup.phase != PHASE_LOADING)
		return 0;
	up = setup.screen ? ae_ui_test_screen_open() : setup.menus ? main_menu_is_active() : map_playing();
	if (!up)
	{
		setup.stable_since = 0;
		return 0;
	}
	if (!setup.stable_since)
		setup.stable_since = now ? now : 1;
	if (now - setup.stable_since < (setup.screen || setup.menus ? SCREEN_STABLE_MS : MAP_STABLE_MS))
		return 0;
	setup.phase = PHASE_READY;
	platform_log("test run setup ready %s", setup.id);
	return 1;
}
