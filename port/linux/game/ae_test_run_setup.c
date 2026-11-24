/* ae_test_run_setup.c: Arena Evolved TEST RUN, a test's setup applied in the running game (see
ae_test_run_setup.h). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "game/players.h"
#include "interface/ui_widget.h"
#include "main/main.h"
#include "objects/objects.h"
#include "tag_files/tag_files.h"
#include "units/units.h"
#include "../src/ae_config_players.h"
#include "../src/port_config.h"
#include "ae_companion.h"
#include "ae_glue_bot_records.h"
#include "ae_hooks.h"
#include "ae_test_run_saved.h"
#include "ae_test_run_setup.h"
#include "ae_weapon_selection_game.h"
#include "network_coop.h"

/* main.c's, platform.h's */
void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
void main_goto_main_menu(void);
boolean main_menu_is_active(void);

enum { PHASE_IDLE, PHASE_LEAVING, PHASE_LOADING, PHASE_READY };

/* the scenario must hold for this long before it is ready (the menus' 200 ms slide; a map's game: its players
spawned and playing, the fade in under way; the test's own setup waits for the game to settle besides) */
#define SCREEN_STABLE_MS 250
#define MAP_STABLE_MS 300
/* (owner 11-11, a Windows test run) a map test's game that has not started this long after its load is asked for again, at most this
many times: on Windows the local game hosted again seconds after the last one ended failed to join ("a networking
error"), the same load a minute later did */
#define MAP_RETRY_MS 10000
#define MAP_RETRIES 6

static struct
{
	struct ae_test_run_saved saved;
	short phase, screen, players, menus;
	short campaign;                  /* the map is a campaign level (gametype coop): no game engine runs in it */
	char id[32];
	char error[96];
	unsigned long stable_since;
	unsigned long loading_since;     /* when the map's load was asked for (MAP_RETRY_MS), 0 not yet */
	short retries;
	unsigned long skippable_since;   /* a campaign level's intro: since when it can be skipped, 0 it cannot */
	unsigned long skipped_at;        /* when the runner last skipped it, 0 not yet */
	short skips;                     /* skips in this load */
	unsigned long intro_logged_at;   /* when the intro's state was last logged */
	short fast;                      /* the game runs at INTRO_FAST_SPEED: an intro that cannot be skipped plays */
} setup;

/* a campaign level's intro is skipped once it has been skippable this long (the save its skip goes back to is written
first), at most INTRO_SKIPS times a load, INTRO_SKIP_GAP_MS apart (a skip reverts the game, and the intro stays skippable
until its script moves past it) */
#define INTRO_SKIPPABLE_MS 500
#define INTRO_SKIP_GAP_MS 3000
#define INTRO_SKIPS 2

/* a campaign level's intro that its script never lets a player skip (the ride in to the beach, the wake-up) plays at
this many times the game's speed until the test's scene is up, so a test opens in a few seconds, not in half a minute */
#define INTRO_FAST_SPEED 8.0f

/* the test's `set` values as the settings held them right after they were set: ae_test_run_setup_lost compares */
static struct { char key[64]; char value[128]; } held[AE_TEST_RUN_SETS];
static short held_count;

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

/* the game's speed while a campaign test loads: fast while an intro plays that cannot be skipped, the game's own again
once the scene is up, the load is given up or its settings are undone */
static void intro_fast(int fast)
{
	if ((fast != 0) == (setup.fast != 0))
		return;
	setup.fast = (short)(fast != 0);
	game_time_set_speed(fast ? INTRO_FAST_SPEED : 1.0f);
	platform_log("test run setup: %s: the level's intro cannot be skipped: the game runs at %s speed", setup.id,
		fast ? "8 times its" : "its own");
}

/* (config_set leaves the setting's "chosen" flag set and does not run config_carry_over, so undo puts back the value
but not what other settings derive from it: a test's `set` keys should be plain settings) */
void ae_test_run_setup_undo(void)
{
	short changed = setup.saved.count;

	intro_fast(0);
	if (ae_test_run_saved_undo(&setup.saved, restore, NULL))
		platform_log("test run setup: a value could not be put back");
	ae_ui_classic_release();
	/* (a test's `setup botrecord demo` lasts that test only) */
	ae_bot_records_demo_end("the test's setup undone");
	/* (network_test read the test's settings: it reads the restored ones, and its host, rotation and bots stop) */
	if (changed)
		network_test_restart();
	/* (the test host's START weapon, debug.network_test_cache_weapon, goes with its setting: a game the menus host next,
	a social launch, plays its own gametype's start, not the earlier test's weapon) */
	ae_weapon_selection_network_test_options(NULL);
	held_count = 0;
	setup.phase = PHASE_IDLE;
}

int ae_test_run_setup_lost(char *what, int size)
{
	char now[128];
	short index;

	for (index = 0; index < held_count; index++)
	{
		if (config_text(held[index].key, now, sizeof(now)) && strcmp(now, held[index].value))
		{
			snprintf(what, (size_t)size, "set %s: it is %s now, the test set %s", held[index].key, now[0] ? now : "empty",
				held[index].value[0] ? held[index].value : "empty");
			return 1;
		}
	}
	return 0;
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
	setup.campaign = !test->screen && !test->menus && !strcmp(test->gametype, "coop");
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
	/* (each as the settings give it back now, once all are set: a later one may name the same key) */
	for (index = 0; index < test->sets; index++)
	{
		snprintf(held[index].key, sizeof(held[index].key), "%s", test->set[index].key);
		if (!config_text(test->set[index].key, held[index].value, sizeof(held[index].value)))
			held[index].value[0] = 0;
	}
	held_count = test->sets;
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
		/* (no wait before START GAME: a TEST RUN's game starts as soon as its lobby is up) */
		if (!change("debug.network_test", value) || !change("debug.network_test_local_players", text) ||
			!change("debug.network_test_gametype", test->gametype) || !change("debug.ae_test_screen", "0") ||
			!change("debug.network_test_start", "0"))
		{
			return fail("cannot set ", "the map", "");
		}
	}
	/* (a campaign test's AI squad is in the level from its start, the intro included: its companion line's count, else the
	setting's) */
	if (setup.campaign)
	{
		int squad = -1;

		for (index = 0; index < test->setups; index++)
			if (!strncmp(test->setup[index], "companion ", 10) && atoi(test->setup[index] + 10) > 0)
				squad = atoi(test->setup[index] + 10);
		ae_companion_next_level(squad);
	}
	network_test_restart();
	ae_ui_test_screen_rearm();
	/* (a running game, or the main menu's scenario not yet loaded, goes to the main menu first) */
	setup.phase = main_menu_is_active() ? PHASE_LOADING : PHASE_LEAVING;
	if (setup.phase == PHASE_LEAVING)
		main_goto_main_menu();
	setup.stable_since = 0;
	setup.loading_since = system_milliseconds();
	setup.retries = 0;
	setup.skippable_since = 0;
	setup.skipped_at = 0;
	setup.skips = 0;
	setup.intro_logged_at = 0;
	platform_log("test run setup: %s (%s %s%s%s, %d players here)", setup.id, test->screen ? "screen" : "map",
		test->screen ? "" : test->map, test->gametype[0] ? " " : "", test->gametype, (int)setup.players);
	return 1;
}

/* a map test's game is playing: running past the pre-game countdown, the local players spawned */
static int map_playing(void)
{
	short local;

	if (!game_in_progress() || main_menu_is_active() || (!game_engine_running() && !setup.campaign) ||
		(game_engine_running() && game_engine_pregame_countdown_ticks_left() > 0) || game_time_get() < 1)
	{
		return 0;
	}
	for (local = 0; local < setup.players; local++)
	{
		long player_index = local_player_get_player_index(local);

		if (player_index == NONE || player_get(player_index)->unit_index == NONE)
			return 0;
		/* (a campaign level's intro, the ride in a dropship, has the player's controls off: the test's scene is after it) */
		if (setup.campaign && (!player_input_enabled() ||
			object_get(player_get(player_index)->unit_index)->object.parent_object_index != NONE))
			return 0;
	}
	return 1;
}

/* a campaign test's load: the level's intro (a cutscene) is skipped as the skip key would, so the test opens on its
scene, not after the intro plays */
static void skip_intro(unsigned long now)
{
	if (game_in_progress() && !main_menu_is_active() && cinematic_in_progress() &&
		(!setup.intro_logged_at || now - setup.intro_logged_at >= 2000))
	{
		setup.intro_logged_at = now ? now : 1;
		platform_log("test run setup: %s: the level's intro plays (skippable %d, saving %d, connection %d)", setup.id,
			(int)cinematic_can_be_skipped(), (int)main_saving_map(), (int)game_connection());
	}
	if (!game_in_progress() || main_menu_is_active() || game_connection() == _game_connection_network_client ||
		!cinematic_in_progress() || !cinematic_can_be_skipped() || main_saving_map())
	{
		setup.skippable_since = 0;
		return;
	}
	if (!setup.skippable_since)
		setup.skippable_since = now ? now : 1;
	if (now - setup.skippable_since < INTRO_SKIPPABLE_MS || setup.skips >= INTRO_SKIPS ||
		(setup.skipped_at && now - setup.skipped_at < INTRO_SKIP_GAP_MS))
	{
		return;
	}
	setup.skipped_at = now ? now : 1;
	setup.skips++;
	platform_log("test run setup: %s: skipping the level's intro (%d of %d)", setup.id, (int)setup.skips, INTRO_SKIPS);
	main_skip_cinematic();
}

/* the weapons the first local player holds when a test is ready */
static void log_weapons(void)
{
	long player_index = local_player_get_player_index(0);
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	struct unit_datum *unit;
	char line[512] = "";
	int slot;

	if (setup.screen || setup.menus || !player || player->unit_index == NONE)
		return;
	unit = (struct unit_datum *)object_get(player->unit_index);
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long weapon = unit->unit.weapon_object_indices[slot];

		if (weapon != NONE)
		{
			size_t used = strlen(line);

			snprintf(line + used, sizeof(line) - used, "%s%s", used ? ", " : "", tag_get_name(object_get(weapon)->definition_index));
		}
	}
	platform_log("test run weapons %s: %s", setup.id, line[0] ? line : "none");
}

int ae_test_run_setup_keep(struct ae_test_run_test const *test)
{
	/* (the game itself still playing is enough: a player dead or not yet back, a reset's at the end of the test before,
	is put back by the reset that comes first in this one) */
	/* (a campaign level, co-op with no game engine, is kept while its game plays) */
	if (setup.phase != PHASE_READY || !game_in_progress() || main_menu_is_active() ||
		(setup.campaign ? !network_coop_active() : !game_engine_running() || game_engine_game_over()))
	{
		return 0;
	}
	snprintf(setup.id, sizeof(setup.id), "%s", test->id);
	platform_log("test run setup kept %s (no load)", setup.id);
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
	if (setup.campaign)
		skip_intro(now);
	up = setup.screen ? ae_ui_test_screen_open() : setup.menus ? main_menu_is_active() : map_playing();
	/* (the level's own game is up, the player is not yet in the scene, and no skip is on offer or on its way: its intro
	runs fast. A guest of another's game runs at the host's speed) */
	if (setup.campaign)
	{
		intro_fast(!up && game_in_progress() && !main_menu_is_active() &&
			game_connection() != _game_connection_network_client && !main_saving_map() &&
			!(cinematic_in_progress() && cinematic_can_be_skipped()));
	}
	if (!up)
	{
		setup.stable_since = 0;
		/* (a map's game that did not start: back at the main menu with no game, asked for again) */
		if (!setup.screen && !setup.menus && main_menu_is_active() && !game_in_progress() &&
			!global_network_game_server_get() && !global_network_game_client_get() && setup.loading_since &&
			now - setup.loading_since >= MAP_RETRY_MS && setup.retries < MAP_RETRIES)
		{
			setup.retries++;
			setup.loading_since = now ? now : 1;
			platform_log("test run setup: %s did not start, loading it again (%d of %d)", setup.id, (int)setup.retries,
				MAP_RETRIES);
			network_test_restart();
		}
		return 0;
	}
	if (!setup.stable_since)
		setup.stable_since = now ? now : 1;
	if (now - setup.stable_since < (setup.screen || setup.menus ? SCREEN_STABLE_MS : MAP_STABLE_MS))
		return 0;
	setup.phase = PHASE_READY;
	platform_log("test run setup ready %s", setup.id);
	log_weapons();
	return 1;
}

/* the load did not finish in time: the lobby or game the runner hosted is taken down, so no lobby is left drawn under the main menu,
and the test's settings go back (the runner hosts no more) */
void ae_test_run_setup_give_up(void)
{
	if (setup.phase == PHASE_IDLE)
		return;
	platform_log("test run setup: %s gave up, the hosted game is taken down", setup.id);
	ae_test_run_setup_undo();
	ui_widgets_close_all();
	dispose_global_network_game_client();
	dispose_global_network_game_server();
	game_connection_set(_game_connection_local);
	if (game_in_progress() && !main_menu_is_active())
		main_goto_main_menu();
}
