/*
NETWORK_TEST.C

Automated system link sessions for testing the netcode without the menus
(debug.network_test in config.toml, HALO_NETWORK_TEST):

- "host:<map>[:<variant>[,<variant>...]]" hosts a game on that
  multiplayer map (bloodgulch, ...) with one of the built-in game variants
  (slayer by default; game_engine_get_variant_by_name), as the pregame
  screen's fast setup does, and starts it debug.network_test_start seconds
  later; with more variants, once a game is over (debug.network_test_score
  makes it short) the next, as the host's button on the scores does;
- "host:<map>+<map>...:<variant>,<variant>..." changes map between those
  games: game N plays the Nth map of the list (the last one from then on),
  so that one run loads several maps one after another (the weapon pack
  added to each and let go again: ae_weapon_pack.c);
- "host:<level>:coop" hosts a network co-op game of that campaign level
  (a10, ...) at Normal, as choosing the level and its difficulty on the
  PC menus' Map screen does (ui_widget_port_cooperative_level_choose);
- "local:<map>[:<variant>...]" the same with a local (split screen) game of
  one player, as Multiplayer > Split Screen of the Xbox's menus sets one up
  (no other machine joins it: the bots' games, bots.count);
- "join" searches for games and joins the first it finds, as picking it in
  the system link list does;
- "browse" joins the game with the most players that the server browser
  lists after five seconds (internet play's public games: a host's with
  debug.network_test_public), co-op ones last, as picking it there does,
  and then as "join". It logs what it lists.
- "log" hosts and joins nothing: the game that runs (map_name, in init.txt,
  plays one without the menus) is logged, as below.
- "probe" (halo --probe [<invite>]: a list site's look at a server) searches
  as "join" does, joins nothing, and prints the first game it finds (with
  an invite, only its host's, over its tunnel; without, one on the LAN) on
  the standard output as one line of JSON, then exits (1, with ok false, if
  none is found in PROBE_SECONDS).

Once the game runs, every second each machine logs where every player's
unit is, so the machines' views of the game can be compared, and the host
how far its own run of each client's player came from the client's
(network_distributed_divergence): tools/net_compare.py compares two builds
so (port/linux/NETCODE.md, "Comparing two builds", has the line's format).

Scripted play for the netcode's parts the bots' wandering does not reach:
debug.network_test_kill (the host's player, else the first, kills the last
other player every so often),
debug.network_test_shoot (every so often each machine's player hits the
next with their weapon's projectile: a client's through its report to the
host) and debug.network_test_vehicle (the host seats the last player as a
vehicle's driver that many seconds in, takes them out 15 seconds on, and 5
seconds later stands them at a vehicle's driver's entrance, where a joining
machine's player holds the action button, as getting in does)
and debug.network_test_pickup (the last player stands on a weapon lying
about that many seconds in, and a joining machine's player holds the action
button a second later, to pick it up); debug.network_test_hurt (the host
leaves the first player 40% of their health, shields full, that many
seconds in, as the port's health regeneration starts from).
debug.network_test_team_change has each machine's player ask to change
teams every so often (team_change.c); debug.network_test_pause opens the
first player's pause menu that many seconds in.

debug.network_test_flags sets bits of the host's game variant's flags (the
port's gametype options: game_engine.h's _game_variant_..._bit).
debug.network_test_gametype: the host plays a custom gametype of the save
root (by its stored name, with its PC options) in place of the built-in one.
debug.network_test_pack_loadout (primary, secondary or both; primary2,
secondary2, first_second, second_first for the second named pack's weapon;
primary3 .. primary5, secondary3 .. secondary5 for the third to fifth's),
debug.network_test_pack_map_spawns and debug.network_test_weapon_packs (the
packs' slot mask, ae_weapon_slots.h: 3 the BR and the SMG): the host's gametype
uses the weapon packs (game_engine.h's weapon_pack, pack_loadout and
pack_map_spawns), with a custom loadout whose stock values are the fallbacks
(an assault rifle, a pistol).

debug.network_test_cues: console lines the host (or a game of one machine)
runs at their ticks, counted from the first tick every player of
debug.network_test_cue_players has a unit (tools/stagekit's scenarios).

Called from the main loop every frame (main.c).
*/

/* port: none of this in a console's build (halo_port_features.h) but
network_test_update, which does nothing there */
#ifndef HALO_CONSOLE_NO_NETWORK_TEST

#include "cseries.h"
#include "game_modes.h"
#include "main/main.h"
#include "bink/bink_playback.h"
#include "interface/player_ui.h"
#include "saved games/player_profile.h"
#include "interface/ui_widget.h"
#include "networking/network_game_globals.h"
#include "networking/network_client_manager.h"
#include "networking/network_server_manager.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "objects/objects.h"
#include "units/units.h"
#include "units/unit_definitions.h"
#include "items/weapons.h"
#include "items/weapon_definitions.h"
#include "items/projectile_definitions.h"
#include "items/items.h"
#include "objects/damage.h"
#include "scenario/scenario.h"
#include "tag_files/tag_files.h"
#include "camera/observer.h"
#include "saved games/player_profile.h"
#include "saved games/saved_game_files.h"
#include "saved games/playlist_profile.h"
#include "saved games/arena_gametypes.h"
#include "ae_silent.h" /* AE hook */
#include "ae_burst_rifle.h" /* AE hook */
#include "ae_saved_limits.h" /* AE hook */
#include "ae_weapon_slots.h" /* AE hook */
#include "ae_test_hooks.h" /* AE hook */
#include "ae_hooks.h" /* AE hook */
#include "ae_screen_create_game.h" /* AE hook */
#include <xtl.h> /* (MAX_GAMENAME) */
#include "mcc_cache.h"
#include "hs/hs.h"

#include "../src/p2p.h" /* port: port/linux/src/p2p.c, p2p_lobby.c */
#include "network_chat.h"
#include "network_voice.h"
#include "halo_keyboard.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* players.c's (not in players.h) */
boolean player_handle_powerup(long player_index, short powerup_type, short duration);

/* the platform layer's (port/linux/src/port_config.c) */
const char *config_string(char const *name);
int config_boolean(char const *name);
double config_real(char const *name);
long config_integer(char const *name);
int config_boolean(char const *name);
int config_write(char const *name, char const *value);
void platform_log(char const *format, ...);
/* main.c's and ui_widget_event_handler_functions.c's (network co-op) */
short main_get_solo_level_from_name(char const *name);
boolean ui_widget_port_cooperative_level_choose(char const *map_name, short difficulty);
/* damage.c's */
void damage_kill_object_for_player(long object_index, long player_index);
/* network_distributed.c's */
void network_distributed_statistics(long *sent, long *received, long *corrections);
void network_distributed_divergence(long *samples, long *apart, real *mean, real *largest);
void network_distributed_item_statistics(long *creates, long *deletes, long *failures, long *removed);
void network_damage_statistics(long *sent_reports, long *dealt_reports, long *rejected_reports, long *replayed_events);
/* xinput_sdl.c's */
void test_input_hold_action(int hold);
/* ui_widget_event_handler_functions.c's (the PC menus' gametype list) */
short ui_widget_port_gametypes(long *indices, short maximum, short *last_used);
void test_input_press(unsigned long buttons);
/* menu_functions.c's: whether the client's advertised game at the index is
an internet play peer's (over the tunnel), not the LAN's */
boolean menu_advertised_game_from_peer(long index);

enum
{
	/* how long a probe looks for a game (halo --probe), and the client's
	advertised games (network_client_manager.c's) */
	PROBE_SECONDS = 30,
	PROBE_MAXIMUM_GAMES = 9,
};

enum
{
	_network_test_off,
	_network_test_host,
	_network_test_join,
	_network_test_browse,
	_network_test_log,
	_network_test_probe,
};

static struct
{
	boolean checked;
	short mode;
	/* (port: weapon pack tests: room for a rotation of every stock map, "host:<map>+<map>...:<variant>,...",
	which is read into map_name whole before its variants are cut off it) */
	char map_name[384];
	char variant_name[256];
	/* "local:": a local (split screen) game, not a system link one */
	boolean local;
	/* ... the variant of this game, of variant_name's list; and the seconds
	the last game's scores have been shown */
	short variant_index;
	real postgame_seconds;
	boolean game_over;
	real start_delay;
	real menu_seconds;
	boolean set_up;
	/* (the hosted game's whole invite logged, for the test that joins it:
	the game's own lines give its first digits only, since players post
	their logs. This is debug.network_test's host alone, a test's machine,
	whose log nobody posts) */
	boolean invite_logged;
	real setup_seconds;
	boolean started;
	boolean joined;
	boolean map_set;
	boolean player_added;
	/* (the host's: debug.network_test_local_players, and how many added) */
	short local_players;
	short local_players_added;
	real joined_seconds;
	boolean team_set;
	real kill_interval;
	real shoot_interval;
	real vehicle_time;
	real pickup_time;
	real team_change_interval;
	/* debug.network_test_pause: the pause menu opened then, once */
	real pause_time;
	boolean paused;
	char pickup_weapon[64];
	/* (debug.network_test_pickup_give: the host puts the weapon the last
	player stands on in its inventory two seconds on, a pickup every machine
	sees; the weapon, NONE for none) */
	boolean pickup_give;
	long pickup_weapon_index;
	real hurt_time;
	boolean hurt;
	/* (debug.network_test_damage: "seconds:scale" hits on the host's first
	player, by its weapon's bullet; applied once each) */
	char damage_script[512]; /* (port: weapon pack tests: room for the battle rifle's 25-hit scripts) */
	long damage_done;
	long damage_logged_time;
	real damage_last[3];
	real quit_time;
	boolean quit;
	/* (debug.network_test_ball: "seconds[:shield[:offset]]": the host's first local
	player takes a ball in hand at that game time, once; the last other player's
	shields are set to shield (an overshield's, over 1) if given and is stood
	offset (default 1.2) along x from them: that player looks along +x, so a
	positive offset puts them with their back to the blow, a negative one facing it) */
	real ball_time;
	real ball_shield;
	real ball_offset;
	boolean ball_done;
	unsigned long variant_flags;
	/* (debug.network_test_ball_melee: the host's gametype gets BALL MELEE LETHAL) */
	boolean ball_melee;
	/* (debug.network_test_melee: network_test_shoot strikes with the weapon's melee) */
	boolean shoot_melee;
	/* (debug.network_test_gametype: a custom gametype's stored name, empty
	for the built-in one) */
	char saved_gametype[64];
	/* debug.network_test_chat's, and the lines said this game */
	real chat_time;
	short chat_lines_said;
	long score_to_win;
	long time_limit;	/* (debug.network_test_time_limit: minutes, 0 the variant's) */
	/* (debug.network_test_loadout: a custom loadout's weapons, game_engine.h's
	_loadout_weapon_* values; NONE: the gametype's loadout) */
	long loadout_primary;
	long loadout_secondary;
	/* (debug.network_test_pack_loadout: the variant's pack_loadout byte, 0 none;
	debug.network_test_pack_map_spawns: its BATTLE RIFLE ON MAP) */
	byte pack_loadout;
	/* (its later packs' bits of the variant's flags: "primary3" .. "secondary5", ae_weapon_slots.h) */
	unsigned long pack_later_flags;
	unsigned char pack_extra;
	boolean pack_map_spawns;
	/* (debug.network_test_weapon_packs: the variant's weapon_pack mask, 0 the battle rifle's alone) */
	byte weapon_packs;
	/* (debug.network_test_auto_balance: the gametype's AUTO TEAM BALANCE on,
	so a team game whose players all joined one team (NHE EXTRAS) starts) */
	boolean auto_balance;
	/* (debug.network_test_kill_host: the kill hook's victim is the host's own
	player, killed by another of this machine's players: a local player's
	death, its input from debug.test_input) */
	boolean kill_host;
	long logged_time;
	boolean public_game;
	boolean browsing;
	boolean browse_listed;
	float browse_seconds;
	float probe_seconds;
	/* debug.network_test_cues: each line and its tick from the cues' start,
	the first tick every player of cue_players has a unit (NONE until then) */
	struct
	{
		long tick;
		char line[256];
	} cues[32];
	short cue_count;
	short next_cue;
	long cue_players;
	long cues_start;
} network_test;
/* (the AE menus' test launch: the host's gametype and PC options, network_test_host_variant's) */
static struct { struct game_variant variant; struct game_variant_options options; } network_test_ae_rules; /* AE hook */
/* (TEST RUN: the settings read again and the state started over, as at the first call of network_test_update) */
void network_test_restart(void) { memset(&network_test, 0, sizeof(network_test)); } /* AE hook */

/* the variant at the index of the list (copied to name), FALSE past its end */
static boolean network_test_variant(
	short index,
	char *name,
	size_t size)
{
	char const *variant = network_test.variant_name;

	for (; index > 0 && variant; index--)
	{
		variant = strchr(variant, ',');
		if (variant)
			variant++;
	}
	if (!variant || !*variant)
		return FALSE;
	if (name)
	{
		size_t length = strcspn(variant, ",");

		snprintf(name, size, "%.*s", (int)length, variant);
	}
	return TRUE;
}

/* a local player the test adds plays with a profile, as a lobby's player
picks one: player 1's last used, else the first saved (System Link's Start:
browser_screen.c's join_first_player), one profile shared by every local
player when it is the only one, as choosing it in the lobby is; a player
that has one already keeps it. Its controller settings (layout, sensitivity)
then drive that player's controller, and its name is the player's
(network_game_client_add_player reads the active profile); none saved, the
player keeps the default profile */
static void network_test_local_player_profile(
	short local_player_index)
{
	long profile_index = player_ui_get_player1_last_used_profile_index();
	struct player_profile profile;

	/* (debug.network_test_profiles false: none, to test a player with none) */
	if (player_ui_get_active_player_profile_index(local_player_index) != NONE ||
		!config_boolean("debug.network_test_profiles"))
	{
		return;
	}
	if (profile_index == NONE || !TEST_FLAG(profile_index, _saved_game_file_index_valid_bit))
	{
		long profile_indices[100];
		word profile_count = NUMBEROF(profile_indices);

		player_profiles_enumerate_available_to_local_player_index(NONE, &profile_count, profile_indices, FALSE);
		profile_index = profile_count ? profile_indices[0] : NONE;
	}
	if (profile_index != NONE && TEST_FLAG(profile_index, _saved_game_file_index_valid_bit) &&
		player_profile_get(profile_index, &profile))
	{
		char name[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH + 1];
		short index;

		player_ui_set_active_player_profile(local_player_index, profile_index, &profile);
		for (index = 0; index < MAXIMUM_PLAYER_PROFILE_NAME_LENGTH && profile.player_name[index]; index++)
			name[index] = profile.player_name[index] < 128 ? (char)profile.player_name[index] : '?';
		name[index] = 0;
		platform_log("network test: local player %d plays with profile \"%s\" (button layout %d, look sensitivity %d)",
			local_player_index + 1, name, (int)profile.controller_settings.button_preset,
			(int)profile.controller_settings.look_sensitivity);
	}
	else
		platform_log("network test: local player %d has no saved profile to play with", local_player_index + 1);
}

/* port: weapon pack tests' map rotation ("host:<map>+<map>..."): game N plays
the Nth map of the list, the last one from then on; one map is itself */
static char const *network_test_map(
	void)
{
	static char map[64];
	char const *piece = network_test.map_name;
	short index;

	for (index = 0; index < network_test.variant_index && strchr(piece, '+'); index++)
		piece = strchr(piece, '+') + 1;
	snprintf(map, sizeof(map), "%.*s", (int)strcspn(piece, "+"), piece);

	return map;
}

static void network_test_read_settings(
	void)
{
	char const *setting = config_string("debug.network_test");

	network_test.checked = TRUE;
	if ((!strncmp(setting, "host:", 5) && setting[5]) || (!strncmp(setting, "local:", 6) && setting[6]))
	{
		char *colon;

		network_test.mode = _network_test_host;
		network_test.local = setting[0] == 'l' && !ae_menus_active(); /* AE hook */
		snprintf(network_test.map_name, sizeof(network_test.map_name), "%s", strchr(setting, ':') + 1);
		snprintf(network_test.variant_name, sizeof(network_test.variant_name), "slayer");
		colon = strchr(network_test.map_name, ':');
		if (colon)
		{
			*colon = 0;
			snprintf(network_test.variant_name, sizeof(network_test.variant_name), "%s", colon + 1);
		}
	}
	else if (!strcmp(setting, "join"))
	{
		network_test.mode = _network_test_join;
	}
	else if (!strcmp(setting, "browse"))
	{
		network_test.mode = _network_test_browse;
	}
	else if (!strcmp(setting, "log"))
	{
		network_test.mode = _network_test_log;
	}
	else if (!strcmp(setting, "probe"))
	{
		network_test.mode = _network_test_probe;
	}
	network_test.public_game = config_boolean("debug.network_test_public") != 0;
	network_test.start_delay = (real)config_real("debug.network_test_start");
	network_test.kill_interval = (real)config_real("debug.network_test_kill");
	network_test.shoot_interval = (real)config_real("debug.network_test_shoot");
	network_test.vehicle_time = (real)config_real("debug.network_test_vehicle");
	network_test.pickup_time = (real)config_real("debug.network_test_pickup");
	network_test.pickup_give = config_boolean("debug.network_test_pickup_give") != 0;
	network_test.pickup_weapon_index = NONE;
	network_test.chat_time = (real)config_real("debug.network_test_chat");
	network_test.team_change_interval = (real)config_real("debug.network_test_team_change");
	network_test.pause_time = (real)config_real("debug.network_test_pause");
	snprintf(network_test.pickup_weapon, sizeof(network_test.pickup_weapon), "%s",
		config_string("debug.network_test_pickup_weapon"));
	network_test.score_to_win = (long)config_integer("debug.network_test_score");
	network_test.time_limit = (long)config_integer("debug.network_test_time_limit");
	network_test.loadout_primary = NONE;
	network_test.loadout_secondary = NONE;
	if (sscanf(config_string("debug.network_test_loadout"), "%ld,%ld", &network_test.loadout_primary,
		&network_test.loadout_secondary) != 2 ||
		network_test.loadout_primary < _loadout_weapon_none || network_test.loadout_primary >= NUMBER_OF_LOADOUT_WEAPONS ||
		network_test.loadout_secondary < _loadout_weapon_none ||
		network_test.loadout_secondary >= NUMBER_OF_LOADOUT_WEAPONS)
	{
		if (config_string("debug.network_test_loadout")[0])
			platform_log("network test: debug.network_test_loadout '%s' is not two weapon numbers 0..%d: ignored",
				config_string("debug.network_test_loadout"), NUMBER_OF_LOADOUT_WEAPONS - 1);
		network_test.loadout_primary = NONE;
		network_test.loadout_secondary = NONE;
	}
	network_test.auto_balance = config_boolean("debug.network_test_auto_balance") != 0;
	network_test.pack_loadout = 0;
	network_test.pack_later_flags = 0;
	network_test.pack_extra = 0;
	network_test.pack_map_spawns = config_boolean("debug.network_test_pack_map_spawns") != 0;
	{
		char const *pack_loadout = config_string("debug.network_test_pack_loadout");

		if (!strcmp(pack_loadout, "primary") || !strcmp(pack_loadout, "both"))
			network_test.pack_loadout |= (byte)(_pack_loadout_pack << PACK_LOADOUT_SLOT_SHIFT(_pack_loadout_primary));
		if (!strcmp(pack_loadout, "secondary") || !strcmp(pack_loadout, "both"))
			network_test.pack_loadout |= (byte)(_pack_loadout_pack << PACK_LOADOUT_SLOT_SHIFT(_pack_loadout_secondary));
		/* (the second named pack's weapon, ae_weapon_slots.h: "primary2", "secondary2", or the first's in the primary
		and the second's in the secondary, "first_second", or the other way round, "second_first") */
		if (!strcmp(pack_loadout, "primary2"))
			network_test.pack_loadout |= (byte)(_pack_loadout_second_pack << PACK_LOADOUT_SLOT_SHIFT(_pack_loadout_primary));
		if (!strcmp(pack_loadout, "secondary2"))
			network_test.pack_loadout |= (byte)(_pack_loadout_second_pack << PACK_LOADOUT_SLOT_SHIFT(_pack_loadout_secondary));
		if (!strcmp(pack_loadout, "first_second"))
			network_test.pack_loadout |= (byte)((_pack_loadout_pack << PACK_LOADOUT_SLOT_SHIFT(_pack_loadout_primary)) |
				(_pack_loadout_second_pack << PACK_LOADOUT_SLOT_SHIFT(_pack_loadout_secondary)));
		/* ("primary3" .. "primary7", "secondary3" .. "secondary7": the third to seventh named pack's weapon) */
		{
			int rank;
			unsigned long *later = &network_test.pack_later_flags;

			if (sscanf(pack_loadout, "primary%d", &rank) == 1 && rank >= 3 && rank <= 7)
				ae_weapon_slots_rank_set(&network_test.pack_loadout, later, &network_test.pack_extra, _pack_loadout_primary, (unsigned int)rank);
			if (sscanf(pack_loadout, "secondary%d", &rank) == 1 && rank >= 3 && rank <= 7)
				ae_weapon_slots_rank_set(&network_test.pack_loadout, later, &network_test.pack_extra, _pack_loadout_secondary, (unsigned int)rank);
		}
		if (!strcmp(pack_loadout, "second_first"))
			network_test.pack_loadout |= (byte)((_pack_loadout_second_pack << PACK_LOADOUT_SLOT_SHIFT(_pack_loadout_primary)) |
				(_pack_loadout_pack << PACK_LOADOUT_SLOT_SHIFT(_pack_loadout_secondary)));
		if (pack_loadout[0] && !network_test.pack_loadout)
			platform_log("network test: debug.network_test_pack_loadout '%s' is not primary, secondary, both, primary2, "
				"secondary2, first_second, second_first or primary3-5 / secondary3-5: ignored", pack_loadout);
	}
	/* (debug.network_test_weapon_packs: the packs' slots, a mask: 0 the battle rifle's alone) */
	network_test.weapon_packs = (byte)(config_integer("debug.network_test_weapon_packs") & WEAPON_PACK_MAXIMUM);
	network_test.kill_host = config_boolean("debug.network_test_kill_host") != 0;
	network_test.hurt_time = (real)config_real("debug.network_test_hurt");
	snprintf(network_test.damage_script, sizeof(network_test.damage_script), "%s",
		config_string("debug.network_test_damage"));
	network_test.quit_time = (real)config_real("debug.network_test_quit");
	network_test.ball_time = 0.0f;
	network_test.ball_shield = 0.0f;
	{
		double ball_time = 0.0, ball_shield = 0.0, ball_offset = 1.2;
		int fields = sscanf(config_string("debug.network_test_ball"), "%lf:%lf:%lf", &ball_time, &ball_shield,
			&ball_offset);

		network_test.ball_offset = (real)ball_offset;

		if (fields >= 1)
			network_test.ball_time = (real)ball_time;
		if (fields >= 2)
			network_test.ball_shield = (real)ball_shield;
	}
	network_test.variant_flags = (unsigned long)config_integer("debug.network_test_flags");
	network_test.ball_melee = config_boolean("debug.network_test_ball_melee") != 0;
	network_test.shoot_melee = config_boolean("debug.network_test_melee") != 0;
	snprintf(network_test.saved_gametype, sizeof(network_test.saved_gametype), "%s",
		config_string("debug.network_test_gametype"));
	network_test.local_players = (short)PIN(config_integer("debug.network_test_local_players"), 1, MAXIMUM_LOCAL_PLAYERS);
	network_test.cue_players = MAX(1, config_integer("debug.network_test_cue_players"));
	network_test.cues_start = NONE;
#ifndef HALO_WEB
	/* (not on the web, where a link's ?set= would have it run what it says) */
	{
		char const *cue = config_string("debug.network_test_cues");

		while (*cue)
		{
			char const *end = strchr(cue, '|');
			int length = end ? (int)(end - cue) : (int)strlen(cue);
			char *line;
			double second = strtod(cue, &line);
			short index;

			if (!(second >= 0.0 && second < 86400.0) || line == cue || line >= cue + length ||
				cue + length - line >= (int)sizeof(network_test.cues[0].line))
			{
				platform_log("network test: cue \"%.*s\" is not \"<second> <line>\" of %d characters at most: left out",
					length, cue, (int)sizeof(network_test.cues[0].line) - 1);
			}
			else if (network_test.cue_count >= (short)NUMBEROF(network_test.cues))
			{
				platform_log("network test: cues past %d left out", (int)NUMBEROF(network_test.cues));
			}
			else
			{
				/* (in the order they run) */
				long tick = (long)(second * TICKS_PER_SECOND + 0.5);

				for (index = network_test.cue_count; index > 0 && network_test.cues[index - 1].tick > tick; index--)
					network_test.cues[index] = network_test.cues[index - 1];
				network_test.cues[index].tick = tick;
				snprintf(network_test.cues[index].line, sizeof(network_test.cues[index].line), "%.*s",
					(int)(cue + length - line), line);
				network_test.cue_count++;
			}
			cue += length + (end != NULL);
		}
	}
#endif
	if (network_test.mode != _network_test_off)
		platform_log("network test: %s", setting);
}

static boolean network_test_gametype_named(long profile_index, char const *wanted);

/* debug.network_test_gametype: the save root's custom gametype of that
stored name (letters' case aside, as the lists sort them), its variant and
PC options, as the menus' gametype list has them (the Arena Evolved
gametypes seeded first); an old name of a seeded one (its migrations: "AE
PRO TS") finds it under its name now; FALSE when there is none */
static boolean network_test_saved_gametype(
	struct game_variant *variant,
	struct game_variant_options *options)
{
	/* (at most 100 saved gametypes a memory unit, saved_game_files.c, and
	the built-in ones: the menus' lists hold 128, menu_functions.c's
	MAXIMUM_GAMETYPES) */
	long indices[AE_SAVED_GAMES_MAXIMUM]; /* AE hook */
	short last_used;
	short count;
	short attempt;

	if (!network_test.saved_gametype[0])
		return FALSE;
	count = ui_widget_port_gametypes(indices, (short)NUMBEROF(indices), &last_used);
	platform_log("network test: %d gametypes listed (at most %d)", (int)count, (int)NUMBEROF(indices));
	if (count >= (short)NUMBEROF(indices))
		platform_log("network test: the first %d gametypes searched (the list is longer)", (int)count);
	for (attempt = 0; attempt < 2; attempt++)
	{
		char const *wanted = attempt ? arena_gametypes_migrated_name(network_test.saved_gametype) :
			network_test.saved_gametype;
		short index;

		if (attempt && wanted == network_test.saved_gametype)
			break;
		for (index = 0; index < count; index++)
		{
			if (network_test_gametype_named(indices[index], wanted) && playlist_profile_get(indices[index], variant))
			{
				playlist_profile_get_options(indices[index], options);
				if (attempt)
					platform_log("network test: gametype '%s' (once '%s') from the save root", wanted,
						network_test.saved_gametype);
				else
					platform_log("network test: gametype '%s' from the save root", wanted);
				return TRUE;
			}
		}
	}
	platform_log("network test: gametype '%s' not found in the save root", network_test.saved_gametype);
	return FALSE;
}

/* whether a gametype of the list is a saved one of that stored name
(letters' case aside) */
static boolean network_test_gametype_named(
	long profile_index,
	char const *wanted)
{
	wchar_t name[MAX_GAMENAME];
	short character;

	/* (the saved ones: the built-in ones have no stored name) */
	if (!(profile_index & 0x80000000) || !playlist_profile_get_display_name(profile_index, name))
		return FALSE;
	for (character = 0; character < MAX_GAMENAME; character++)
	{
		wchar_t a = name[character];
		wchar_t b = (wchar_t)(unsigned char)wanted[character];

		if (a >= 'a' && a <= 'z')
			a = (wchar_t)(a - 'a' + 'A');
		if (b >= 'a' && b <= 'z')
			b = (wchar_t)(b - 'a' + 'A');
		if (a != b || !a)
			break;
	}
	return !(character < MAX_GAMENAME && (name[character] || wanted[character]));
}

/* port: the gametype and PC options the host's game plays (the game of the
list: network_test.variant_index), as the host's lobby sets them up and the AE
menus' test launch does (ae_screen_create_game_test_open): FALSE for a "coop"
game, which is a campaign level, not a variant */
static boolean network_test_host_variant(
	struct game_variant *variant,
	struct game_variant_options *options)
{
	char variant_name[64];
	/* debug.network_test_gametype: a saved gametype and its PC
	options, as picking it in the menus does; else (and when it
	is not there) one of OpenCE's gametypes by its name
	(game_modes.c); else the variant, as picking the game settings
	does */
	boolean saved;
	boolean opence;

	network_test_variant(network_test.variant_index, variant_name, sizeof(variant_name));
	if (!strcmp(variant_name, "coop"))
		return FALSE;
	saved = network_test_saved_gametype(variant, options);
	opence = !saved && game_modes_gametype_by_name(variant_name, variant, options);
	if (!saved && !opence)
		*variant = *game_engine_get_variant_by_name(variant, variant_name);
	platform_log("network test: game %d, %s", network_test.variant_index + 1,
		saved ? network_test.saved_gametype : variant_name);
	/* port: (the map rotation: which map this game plays) */
	if (strchr(network_test.map_name, '+'))
		platform_log("network test: game %d plays %s", network_test.variant_index + 1, network_test_map());
	/* debug.network_test_score: a short game, to test the next */
	if (network_test.score_to_win > 0)
		variant->universal_variant.score_to_win = network_test.score_to_win;
	/* debug.network_test_flags: the port's gametype options */
	variant->universal_variant.flags |= network_test.variant_flags | network_test.pack_later_flags;
	if (network_test.ball_melee)
		variant->universal_variant.ball_melee = (byte)((variant->universal_variant.ball_melee & 0xF0) | 1);
	variant->universal_variant.ball_melee |= network_test.pack_extra;
	ae_silent_network_test(variant); /* AE hook */
	/* debug.network_test_pack_loadout / _pack_map_spawns: the weapon pack */
	if (network_test.pack_loadout || network_test.pack_map_spawns || network_test.weapon_packs)
	{
		variant->universal_variant.weapon_pack = network_test.weapon_packs ? network_test.weapon_packs :
			_weapon_pack_battle_rifle;
		/* (the gametype's own bits 5-7, INFINITE AMMO, BOTTOMLESS CLIP and MAP GRENADES OFF, kept:
		ae_weapon_rules.h) */
		variant->universal_variant.pack_loadout = (byte)(network_test.pack_loadout |
			(network_test.pack_map_spawns ? PACK_MAP_SPAWNS_BIT : 0) |
			(variant->universal_variant.pack_loadout & 0xE0));
	}
	/* the PC options: the variant's defaults (as player_ui_set_game_variant
	sets them), else the saved or OpenCE gametype's own, and
	debug.network_test_time_limit: the gametype's PC option (MATCH CLOCK's
	time left, its headings) */
	if (!saved && !opence)
		game_variant_options_default(variant, options);
	if (network_test.time_limit > 0)
		options->time_limit = (short)network_test.time_limit;
	/* debug.network_test_loadout: a custom loadout */
	if (network_test.loadout_primary != NONE)
	{
		options->loadout = _loadout_custom;
		options->primary_weapon = (byte)network_test.loadout_primary;
		options->secondary_weapon = (byte)network_test.loadout_secondary;
	}
	/* debug.network_test_pack_loadout: a custom loadout (the test
	loadout's weapons, a saved gametype's own, else the fallbacks the stock
	values hold) */
	if (network_test.pack_loadout)
	{
		if (options->loadout != _loadout_custom && network_test.loadout_primary == NONE)
		{
			options->primary_weapon = _loadout_weapon_assault_rifle;
			options->secondary_weapon = _loadout_weapon_pistol;
		}
		options->loadout = _loadout_custom;
	}
	/* debug.network_test_auto_balance */
	if (network_test.auto_balance)
		options->auto_team_balance = TRUE;
	return TRUE;
}

/* appends to a line, cut short when it is full */
static void network_test_append(
	char *line,
	int size,
	int *length,
	char const *format,
	...)
{
	va_list arguments;
	int written;

	if (*length >= size - 1)
		return;
	va_start(arguments, format);
	written = vsnprintf(line + *length, (size_t)(size - *length), format, arguments);
	va_end(arguments);
	if (written > 0)
		*length = MIN(*length + written, size - 1);
}

/* every player's unit, as this machine sees it */
static void network_test_log_players(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	char line[4096];
	int length = 0;

	line[0] = 0;
	/* (each player's name, once a game: for tests that name a player, such
	as the host's ban command) */
	{
		static wchar_t named[HALO_PORT_MAXIMUM_NETWORK_PLAYERS][12];

		if (game_time_get() < 2 * TICKS_PER_SECOND)
			csmemset(named, 0, sizeof(named));
		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			long absolute_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
			char name[13];
			short index;

			if (absolute_index >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS ||
				!csmemcmp(named[absolute_index], player->name, sizeof(named[absolute_index])))
			{
				continue;
			}
			csmemcpy(named[absolute_index], player->name, sizeof(named[absolute_index]));
			for (index = 0; index < 12 && player->name[index]; index++)
				name[index] = player->name[index] >= 32 && player->name[index] < 127 ? (char)player->name[index] : '?';
			name[index] = 0;
			platform_log("network test: player %ld is named %s", absolute_index, name);
		}
	}
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->unit_index != NONE)
		{
			struct object_datum *object = object_get(player->unit_index);
			struct unit_datum *unit = unit_get(player->unit_index);
			short slot;

			/* (riding: where its vehicle is) */
			struct object_datum *placed = object->object.parent_object_index != NONE ?
				object_get(object->object.parent_object_index) : object;

			network_test_append(line, (int)sizeof(line), &length, " player %ld: (%.3f %.3f %.3f) h%.2f/%.2f%s%s%s g%d/%d w",
				(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index), placed->object.position.x,
				placed->object.position.y, placed->object.position.z, object->object.body_vitality,
				object->object.shield_vitality, placed != object ? " riding" : "",
				TEST_FLAG(unit->unit.flags, _unit_active_camouflaged_bit) ? " camo" : "",
				TEST_FLAG(unit->unit.flags, _unit_integrated_light_on_bit) ? " light" : "",
				unit->unit.grenade_counts[0], unit->unit.grenade_counts[1]);
			/* where it aims (yaw and pitch, degrees), its animation state and
			how hard it is moving */
			network_test_append(line, (int)sizeof(line), &length, " a%.0f/%.0f f%.0f l%.0f/%.0f as%d/%d st%d thr%.2f",
				atan2(unit->unit.aiming_vector.j, unit->unit.aiming_vector.i) * 57.29578,
				asin(PIN(unit->unit.aiming_vector.k, -1.0f, 1.0f)) * 57.29578,
				atan2(object->object.forward.j, object->object.forward.i) * 57.29578,
				atan2(unit->unit.looking_vector.j, unit->unit.looking_vector.i) * 57.29578,
				asin(PIN(unit->unit.looking_vector.k, -1.0f, 1.0f)) * 57.29578,
				(int)unit->unit.animation.aiming_screen_index, (int)unit->unit.animation.looking_screen_index,
				(int)unit->unit.animation.state,
				sqrt(unit->unit.throttle.i * unit->unit.throttle.i + unit->unit.throttle.j * unit->unit.throttle.j));
			for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
			{
				long weapon_index = unit->unit.weapon_object_indices[slot];

				if (weapon_index != NONE)
				{
					struct weapon_datum *weapon = weapon_get(weapon_index);

					network_test_append(line, (int)sizeof(line), &length, " %lx:%d",
						(unsigned long)weapon->definition_index & 0xFFFF, weapon->weapon.magazines[0].rounds_total +
						weapon->weapon.magazines[0].rounds_loaded);
				}
			}
		}
		else
		{
			network_test_append(line, (int)sizeof(line), &length, " player %ld: dead",
				(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index));
		}
		/* the game type's score and the kills and deaths */
		network_test_append(line, (int)sizeof(line), &length, " s%ld k%d d%d f%d t%ld m%d",
			game_engine && game_engine->get_player_score ?
				game_engine->get_player_score(iterator.datum_index, _get_score_individual) : -1L,
			player->statistics.kills[0], player->statistics.deaths, player->statistics.friendly_fire_kills, (long)player->team_index,
			(int)player->network_player_data.machine_index);
	}
	{
		long sent, received, corrections;
		struct object_iterator objects;
		long ground_items = 0;
		long creates, deletes, failures, removed;
		long sent_reports, dealt_reports, rejected_reports, replayed_events;
		/* this machine's player and where its camera is (a player that never
		spawns leaves it where it began) */
		long local_player_index = local_player_get_player_index(0);
		struct observer_result const *camera = observer_get_camera(0);
		long divergence_samples, divergence_apart;
		real divergence_mean, divergence_largest;

		object_iterator_new(&objects, _object_mask_weapon | _object_mask_equipment, 0);
		while (object_iterator_next(&objects))
		{
			struct item_datum *item = item_get(objects.index);

			if (item->object.parent_object_index == NONE &&
				TEST_FLAG(item->object.flags, _object_connected_to_map_bit) &&
				!TEST_FLAG(item->item.flags, _item_attached_to_unit_bit))
			{
				ground_items++;
			}
		}
		network_distributed_statistics(&sent, &received, &corrections);
		network_distributed_item_statistics(&creates, &deletes, &failures, &removed);
		network_damage_statistics(&sent_reports, &dealt_reports, &rejected_reports, &replayed_events);
		network_distributed_divergence(&divergence_samples, &divergence_apart, &divergence_mean, &divergence_largest);

		platform_log("network test: tick %ld%s | items %ld (+%ld -%ld !%ld x%ld) | %s to %ld | sent %ld received %ld corrected %ld"
			" | hits %ld dealt %ld rejected %ld replayed %ld | local %ld camera (%.1f %.1f %.1f) respawn %ld"
			" | drift %ld %ld %.6f %.6f",
			game_time_get(), line, ground_items, creates, deletes, failures, removed,
			game_engine_can_score() ? "playing" : "game over",
			game_engine_running() ? (long)game_engine_get_variant()->universal_variant.score_to_win : 0L,
			sent, received, corrections,
			sent_reports, dealt_reports, rejected_reports, replayed_events,
			local_player_index == NONE ? -1L : (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(local_player_index),
			camera ? camera->position.x : 0.0f, camera ? camera->position.y : 0.0f, camera ? camera->position.z : 0.0f,
			local_player_index == NONE ? 0L : (long)player_get(local_player_index)->respawn_timer,
			divergence_samples, divergence_apart, divergence_mean, divergence_largest);
	}
}

/* each of this machine's players hits the next player with their weapon's
projectile, as its impact would, when it is within the projectile's reach
(as a real shot is: the host checks it) */
static void network_test_shoot(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct data_iterator targets;
		struct player_datum *target = NULL;
		struct player_datum *candidate;
		struct unit_datum *unit;
		long weapon_index;
		struct weapon_definition *weapon;
		struct weapon_trigger_definition *trigger;
		long damage_index = NONE;
		real reach = 0.0f;
		boolean melee;
		struct damage_data damage;
		struct object_datum *target_object;
		real_vector3d direction;

		if (player->local_player_index == NONE || player->unit_index == NONE)
			continue;
		data_iterator_new(&targets, player_data);
		while ((candidate = (struct player_datum *)data_iterator_next(&targets)) != NULL)
		{
			if (candidate != player && candidate->unit_index != NONE)
			{
				target = candidate;
				break;
			}
		}
		unit = unit_get(player->unit_index);
		if (!target || unit->unit.current_weapon_index == NONE ||
			TEST_FLAG(object_get(target->unit_index)->object.damage_flags, _object_dead_bit))
		{
			continue;
		}
		weapon_index = unit->unit.weapon_object_indices[unit->unit.current_weapon_index];
		if (weapon_index == NONE)
			continue;
		weapon = weapon_definition_get(object_get(weapon_index)->definition_index);
		if (weapon->weapon.triggers.count > 0)
		{
			trigger = TAG_BLOCK_GET_ELEMENT(&weapon->weapon.triggers, 0, struct weapon_trigger_definition);
			if (trigger->projectile.index != NONE)
			{
				struct projectile_definition *projectile = projectile_definition_get(trigger->projectile.index);

				damage_index = projectile->projectile.impact_damage.index;
				/* (how far it flies: its range, or at its speed for as long as
				its timer runs; none for no bound, network_damage.c) */
				if (projectile->projectile.maximum_range > 0.0f)
					reach = projectile->projectile.maximum_range;
				else if (projectile->projectile.detonation_timer_starts == 0)
				{
					reach = projectile->projectile.timer_upper_bound * TICKS_PER_SECOND *
						MAX(projectile->projectile.initial_velocity, projectile->projectile.final_velocity);
				}
			}
		}
		/* (else its melee: a blow's epicenter is its striker's, as
		unit_cause_player_melee_damage has it) */
		melee = damage_index == NONE || network_test.shoot_melee;
		if (melee)
			damage_index = weapon->weapon.melee_attack_damage.index;
		if (damage_index == NONE)
			continue;
		target_object = object_get(target->unit_index);
		/* (a blow reaches only a target at hand, as the host checks) */
		if (melee)
		{
			real dx = target_object->object.position.x - unit->object.position.x;
			real dy = target_object->object.position.y - unit->object.position.y;
			real dz = target_object->object.position.z - unit->object.position.z;

			/* (1.6, not the 1.5 gather leaves players apart: rounding missed it) */
			if (dx * dx + dy * dy + dz * dz > 1.6f * 1.6f)
				continue;
		}
		/* (a shot reaches no further than its projectile flies, with a margin
		for how far the target is from where the shot started) */
		else if (reach > 0.0f)
		{
			real dx = target_object->object.position.x - unit->object.position.x;
			real dy = target_object->object.position.y - unit->object.position.y;
			real dz = target_object->object.position.z - unit->object.position.z;

			if (dx * dx + dy * dy + dz * dz > 0.9f * reach * 0.9f * reach)
				continue;
		}
		damage_data_new(&damage, damage_index);
		damage.owner_player_index = iterator.datum_index;
		damage.owner_object_index = player->unit_index;
		damage.owner_team_index = unit->object.owner_team_index;
		damage.origin = melee ? unit->object.bounding_sphere_center : target_object->object.position;
		damage.epicenter = melee ? unit->object.bounding_sphere_center : target_object->object.position;
		direction.i = target_object->object.position.x - unit->object.position.x;
		direction.j = target_object->object.position.y - unit->object.position.y;
		direction.k = target_object->object.position.z - unit->object.position.z;
		normalize3d(&direction);
		damage.direction = direction;
		damage.scale = 1.0f;
		scenario_location_from_point(&damage.location, &damage.epicenter);
		object_cause_damage(&damage, target->unit_index, NONE, NONE, NONE, NULL);
		platform_log("network test: player %ld shoots player %ld",
			(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index), (long)(target - (struct player_datum *)xbox_pointer(player_data->data)));
	}
}

/* the host seats the last player as the nearest vehicle's driver, or out */
/* the host brings the players far from the first (on foot) near it, so
that they are within their weapons' reach (network_test_shoot shoots only
so near, as a player does); the host moving a client's player, as a
teleporter does */
static void network_test_gather(
	boolean leave_last)
{
	struct data_iterator iterator;
	struct player_datum *player;
	struct player_datum *first = NULL;
	struct player_datum *last = NULL;
	short count = 0;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (!first)
			first = player;
		last = player;
	}
	if (!first || first->unit_index == NONE)
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct object_datum const *center = object_get(first->unit_index);
		struct object_datum *unit;
		real_point3d position;
		real dx, dy, dz;

		if (player == first || player->unit_index == NONE || (leave_last && player == last))
			continue;
		count++;
		unit = object_get(player->unit_index);
		if (unit->object.parent_object_index != NONE || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			continue;
		dx = unit->object.position.x - center->object.position.x;
		dy = unit->object.position.y - center->object.position.y;
		dz = unit->object.position.z - center->object.position.z;
		if (dx * dx + dy * dy + dz * dz <= 20.0f * 20.0f)
			continue;
		/* (beside it, where the map is open: at its feet and its head) */
		{
			static real const offsets[][2] = { { 1.5f, 0.0f }, { -1.5f, 0.0f }, { 0.0f, 1.5f }, { 0.0f, -1.5f },
				{ 1.5f, 1.5f }, { -1.5f, -1.5f }, { 1.5f, -1.5f }, { -1.5f, 1.5f } };
			short try_index;

			for (try_index = 0; try_index < (short)NUMBEROF(offsets); try_index++)
			{
				short index = (short)((count + try_index) % (short)NUMBEROF(offsets));
				struct location feet, head;
				real_point3d above;

				position = center->object.position;
				position.x += offsets[index][0];
				position.y += offsets[index][1];
				position.z += 0.2f;
				above = position;
				above.z += 0.7f;
				scenario_location_from_point(&feet, &position);
				scenario_location_from_point(&head, &above);
				if (feet.cluster_index != NONE && head.cluster_index != NONE)
					break;
			}
			if (try_index >= (short)NUMBEROF(offsets))
				continue;
		}
		object_set_position(player->unit_index, &position, NULL, NULL);
		platform_log("network test: the host brings player %ld near the first",
			(long)(player - (struct player_datum *)xbox_pointer(player_data->data)));
	}
}

static void network_test_vehicle(
	boolean enter)
{
	struct data_iterator iterator;
	struct player_datum *player;
	struct player_datum *last = NULL;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		last = player;
	if (!last || last->unit_index == NONE)
		return;
	if (!enter)
	{
		if (object_get(last->unit_index)->object.parent_object_index != NONE)
		{
			unit_exit_seat_end(last->unit_index);
			platform_log("network test: the last player leaves the vehicle");
		}
		return;
	}
	{
		struct object_iterator vehicles;
		long nearest_index = NONE;
		real nearest_distance = 0.0f;
		real_point3d const *position = &object_get(last->unit_index)->object.position;
		short seat_index;

		object_iterator_new(&vehicles, _object_mask_vehicle, 0);
		while (object_iterator_next(&vehicles))
		{
			real_point3d const *vehicle_position = &object_get(vehicles.index)->object.position;
			real distance = distance_squared3d(position, vehicle_position);

			if (nearest_index == NONE || distance < nearest_distance)
			{
				nearest_index = vehicles.index;
				nearest_distance = distance;
			}
		}
		if (nearest_index == NONE)
		{
			platform_log("network test: no vehicle");
			return;
		}
		for (seat_index = 0; seat_index < unit_definition_get(object_get(nearest_index)->definition_index)->unit.seats.count; seat_index++)
		{
			if (unit_seat_is_driver(nearest_index, seat_index) &&
				unit_enter_seat(last->unit_index, nearest_index, seat_index))
			{
				platform_log("network test: the last player drives vehicle %lx", nearest_index);
				return;
			}
		}
		platform_log("network test: the last player cannot drive vehicle %lx", nearest_index);
	}
}

/* debug.network_test_name: the name the machine's first player takes (its
profile's, which it joins with), in ASCII */
static void network_test_set_name(
	void)
{
	char const *name = config_string("debug.network_test_name");
	struct player_profile profile;
	short index;

	if (!name || !name[0])
		return;
	player_ui_get_active_player_profile(0, &profile);
	for (index = 0; name[index] && index < MAXIMUM_PLAYER_PROFILE_NAME_LENGTH - 1; index++)
		profile.player_name[index] = (wchar_t)(unsigned char)name[index];
	profile.player_name[index] = 0;
	player_ui_set_active_player_profile(0, player_ui_get_active_player_profile_index(0), &profile);
	platform_log("network test: this machine's player is named %s", name);
}

/* debug.network_test_chat: the host seats the last two players in one
vehicle, the nearest to the first of them with seats for both (any, a map's
own), for the vehicle chat (two joining machines' players, of one team: they
start near each other) */
static void network_test_seat_together(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	struct object_iterator vehicles;
	long units[2];
	long best_index = NONE;
	real best_distance = 0.0f;
	short count = 0;
	short index;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->unit_index == NONE)
			continue;
		units[0] = count ? units[1] : NONE;
		units[1] = player->unit_index;
		count++;
	}
	if (count < 2)
		return;
	object_iterator_new(&vehicles, _object_mask_vehicle, 0);
	while (object_iterator_next(&vehicles))
	{
		real distance;

		if (unit_definition_get(object_get(vehicles.index)->definition_index)->unit.seats.count < 2)
			continue;
		distance = distance_squared3d(&object_get(units[0])->object.position, &object_get(vehicles.index)->object.position);
		if (best_index == NONE || distance < best_distance)
		{
			best_index = vehicles.index;
			best_distance = distance;
		}
	}
	if (best_index == NONE)
	{
		platform_log("network test: no vehicle with two seats");
		return;
	}
	for (index = 0; index < (short)NUMBEROF(units); index++)
	{
		short seat_index;

		for (seat_index = 0;
			seat_index < unit_definition_get(object_get(best_index)->definition_index)->unit.seats.count; seat_index++)
		{
			if (!unit_seat_filled(best_index, seat_index) && unit_enter_seat(units[index], best_index, seat_index))
				break;
		}
	}
	platform_log("network test: the last two players ride vehicle %lx: %s, %s", best_index,
		object_get(units[0])->object.parent_object_index == best_index ? "in" : "out",
		object_get(units[1])->object.parent_object_index == best_index ? "in" : "out");
}

/* debug.network_test_chat: a line typed on the text chat's on-screen
keyboard with the controller's buttons, ticks since it began: each held
for two frames and let go for two, whatever the frame rate (a press of two
ticks falls between two frames of a machine drawing slowly) (the keyboard
opens on the first key, 'q': "gg ez", then D-pad Down to the team, then
Start) */
static void network_test_keyboard_line(
	long ticks)
{
	enum { STEP_FRAMES = 4 };
	static long frames;
	static long last_ticks = -1;
	static unsigned long const steps[] =
	{
		1UL << HALO_CHAT_PAD_UP,
		1UL << HALO_CHAT_PAD_STICK_DOWN,
		1UL << HALO_CHAT_PAD_STICK_RIGHT, 1UL << HALO_CHAT_PAD_STICK_RIGHT, 1UL << HALO_CHAT_PAD_STICK_RIGHT,
		1UL << HALO_CHAT_PAD_STICK_RIGHT,
		1UL << HALO_CHAT_PAD_A, 1UL << HALO_CHAT_PAD_A,
		1UL << HALO_CHAT_PAD_STICK_DOWN, 1UL << HALO_CHAT_PAD_STICK_DOWN,
		1UL << HALO_CHAT_PAD_A,
		1UL << HALO_CHAT_PAD_STICK_UP, 1UL << HALO_CHAT_PAD_STICK_UP, 1UL << HALO_CHAT_PAD_STICK_UP,
		1UL << HALO_CHAT_PAD_STICK_LEFT, 1UL << HALO_CHAT_PAD_STICK_LEFT,
		1UL << HALO_CHAT_PAD_A,
		1UL << HALO_CHAT_PAD_STICK_DOWN, 1UL << HALO_CHAT_PAD_STICK_DOWN,
		1UL << HALO_CHAT_PAD_STICK_LEFT, 1UL << HALO_CHAT_PAD_STICK_LEFT,
		1UL << HALO_CHAT_PAD_A,
		1UL << HALO_CHAT_PAD_DOWN,
		1UL << HALO_CHAT_PAD_START,
	};
	long step;

	/* (frames since it began: again for the next game's line) */
	if (ticks < last_ticks)
		frames = 0;
	last_ticks = ticks;
	step = frames / STEP_FRAMES;
	frames++;
	if (step >= (long)NUMBEROF(steps))
	{
		/* (after the line, let go: a held key would move nothing more) */
		if (step == (long)NUMBEROF(steps))
			test_input_press(0);
		return;
	}
	test_input_press((frames - 1) % STEP_FRAMES < STEP_FRAMES / 2 ? steps[step] : 0);
}

/* debug.network_test_chat: the last player (absolute index; NONE), whom the
host mutes; whether it is this machine's; and the first player's machine
(the network's index: the host's), which the last player's mutes */
static short network_test_last_player_index(
	void)
{
	struct data_iterator iterator;
	short last = NONE;

	data_iterator_new(&iterator, player_data);
	while (data_iterator_next(&iterator) != NULL)
		last = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
	return last;
}

static boolean network_test_last_player_local(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	struct player_datum *last = NULL;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		last = player;
	return last && last->local_player_index != NONE;
}

static long network_test_first_player_machine(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	player = (struct player_datum *)data_iterator_next(&iterator);
	return player ? player->network_player_data.machine_index : NONE;
}

/* debug.network_test_chat: the last player's name, in ASCII (the host
mutes it) */
static void network_test_last_player_name(
	char *name,
	short size)
{
	struct data_iterator iterator;
	struct player_datum *player;
	struct player_datum *last = NULL;
	short index;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		last = player;
	for (index = 0; last && index < size - 1 && index < (short)NUMBEROF(last->name) && last->name[index]; index++)
		name[index] = player_name_character_ascii(last->name[index]);
	name[index] = 0;
}

/* both machines stand the last player at the driver's entrance of the
nearest vehicle no one rides (a joining machine's player then holds the
action button, as a player getting in does) */
static void network_test_vehicle_approach(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	struct player_datum *last = NULL;
	struct object_iterator vehicles;
	long nearest_index = NONE;
	real nearest_distance = 0.0f;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		last = player;
	if (!last || last->unit_index == NONE || object_get(last->unit_index)->object.parent_object_index != NONE)
		return;
	object_iterator_new(&vehicles, _object_mask_vehicle, 0);
	while (object_iterator_next(&vehicles))
	{
		struct unit_datum *vehicle = unit_get(vehicles.index);
		/* (the same vehicle on every machine: the lowest index) */
		real distance = (real)DATUM_INDEX_TO_ABSOLUTE_INDEX(vehicles.index);

		if (vehicle->unit.driver_object_index != NONE || TEST_FLAG(vehicle->object.damage_flags, _object_dead_bit))
			continue;
		if (nearest_index == NONE || distance < nearest_distance)
		{
			nearest_index = vehicles.index;
			nearest_distance = distance;
		}
	}
	if (nearest_index != NONE)
	{
		short seat_index;

		for (seat_index = 0; seat_index < unit_definition_get(object_get(nearest_index)->definition_index)->unit.seats.count;
			seat_index++)
		{
			real_point3d entrance;
			real_point3d seat;

			if (unit_seat_is_driver(nearest_index, seat_index) &&
				unit_get_seat_entrance_point(last->unit_index, nearest_index, seat_index, &entrance, &seat, NULL))
			{
				object_set_position(last->unit_index, &entrance, NULL, NULL);
				platform_log("network test: the last player stands at vehicle %lx's driver's entrance (%.2f %.2f %.2f)",
					nearest_index, entrance.x, entrance.y, entrance.z);
				return;
			}
		}
	}
	platform_log("network test: no vehicle to get into");
}

/* the host gives the last player a second weapon, lying about */
static void network_test_second_weapon(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	struct player_datum *last = NULL;
	struct object_iterator weapons;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		last = player;
	if (!last || last->unit_index == NONE)
		return;
	object_iterator_new(&weapons, _object_mask_weapon, 0);
	while (object_iterator_next(&weapons))
	{
		struct object_datum *weapon = object_get(weapons.index);
		struct unit_datum *unit = unit_get(last->unit_index);
		long current_index = unit->unit.weapon_object_indices[0];

		if (weapon->object.parent_object_index == NONE && TEST_FLAG(weapon->object.flags, _object_connected_to_map_bit) &&
			(current_index == NONE || object_get(current_index)->definition_index != weapon->definition_index) &&
			unit_add_weapon_to_inventory(last->unit_index, weapons.index, TRUE))
		{
			platform_log("network test: the last player takes a second weapon (%lx)", weapon->definition_index);
			/* (and camouflage, as a powerup gives) */
			player_handle_powerup(DATUM_INDEX_NEW(last - (struct player_datum *)xbox_pointer(player_data->data), last->identifier),
				_player_powerup_active_camouflage, 10 * TICKS_PER_SECOND);
			return;
		}
	}
}

/* debug.network_test_cues: from the first tick every player waited for has a
unit (logged, cues or not: where a test's seconds start), each line at its
tick (the host's, or a game of one machine: a client runs none) */
static void network_test_run_cues(
	void)
{
	if (network_test.mode != _network_test_host && network_test.mode != _network_test_log)
		return;
	if (network_test.cues_start == NONE)
	{
		struct data_iterator iterator;
		struct player_datum *player;
		long players = 0;

		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
			players += player->unit_index != NONE;
		if (players < network_test.cue_players)
			return;
		network_test.cues_start = game_time_get();
		platform_log("network test: cues from tick %ld", network_test.cues_start);
	}
	while (network_test.next_cue < network_test.cue_count &&
		game_time_get() >= network_test.cues_start + network_test.cues[network_test.next_cue].tick)
	{
		platform_log("network test: cue at tick %ld: %s", game_time_get(), network_test.cues[network_test.next_cue].line);
		hs_compile_and_evaluate(network_test.cues[network_test.next_cue].line);
		network_test.next_cue++;
	}
}

/* both machines stand the last player on the first weapon lying about that
it does not carry (the same object on both: a client's own player is where
it has it, within a tolerance) */
static void network_test_pickup(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	struct player_datum *last = NULL;
	struct object_iterator weapons;
	long nearest_index = NONE;
	real nearest_distance = 0.0f;
	struct unit_datum *unit;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		last = player;
	if (!last || last->unit_index == NONE)
		return;
	unit = unit_get(last->unit_index);
	object_iterator_new(&weapons, _object_mask_weapon, 0);
	while (object_iterator_next(&weapons))
	{
		struct object_datum *weapon = object_get(weapons.index);
		short slot;
		boolean carried = FALSE;
		real distance;

		if (weapon->object.parent_object_index != NONE ||
			!TEST_FLAG(weapon->object.flags, _object_connected_to_map_bit))
		{
			continue;
		}
		for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
		{
			long carried_index = unit->unit.weapon_object_indices[slot];

			carried |= carried_index != NONE && object_get(carried_index)->definition_index == weapon->definition_index;
		}
		if (network_test.pickup_weapon[0] && !strstr(tag_get_name(weapon->definition_index), network_test.pickup_weapon))
			continue;
		distance = (real)DATUM_INDEX_TO_ABSOLUTE_INDEX(weapons.index);
		if (!carried && (nearest_index == NONE || distance < nearest_distance))
		{
			nearest_index = weapons.index;
			nearest_distance = distance;
		}
	}
	if (nearest_index != NONE)
	{
		real_point3d position = object_get(nearest_index)->object.position;

		position.z += 0.1f;
		object_set_position(last->unit_index, &position, NULL, NULL);
		network_test.pickup_weapon_index = nearest_index;
		platform_log("network test: the last player stands on weapon %lx (%lx)", nearest_index,
			object_get(nearest_index)->definition_index);
	}
}

/* debug.network_test_ball: the host's first local player takes a ball in hand
(a ball pickup every machine sees; debug.network_test_shoot then has it strike
the first other player with the ball's melee), and the last other player's
shields are set, for tests of BALL MELEE */
static void network_test_ball(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	struct player_datum *first = NULL;
	struct player_datum *last = NULL;
	struct object_iterator weapons;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->unit_index == NONE)
			continue;
		if (!first && player->local_player_index != NONE)
			first = player;
		else
			last = player;
	}
	if (!first || !last)
		return;
	object_iterator_new(&weapons, _object_mask_weapon, 0);
	while (object_iterator_next(&weapons))
	{
		if (object_get(weapons.index)->object.parent_object_index == NONE && weapon_is_flag(weapons.index) &&
			unit_add_weapon_to_inventory(first->unit_index, weapons.index, TRUE))
		{
			platform_log("network test: the first player takes the ball (%lx) at tick %ld", weapons.index,
				game_time_get());
			/* (and the last other player beside it, within a blow's reach) */
			{
				real_point3d position = object_get(first->unit_index)->object.position;

				position.x += network_test.ball_offset;
				object_set_position(last->unit_index, &position, NULL, NULL);
			}
			if (network_test.ball_shield > 0.0f)
			{
				object_get(last->unit_index)->object.shield_vitality = network_test.ball_shield;
				platform_log("network test: the last player's shields set to %g", (double)network_test.ball_shield);
			}
			network_test.ball_done = TRUE;
			return;
		}
	}
}

/* debug.network_test_damage: "seconds:scale,seconds:scale ...": at that game
time the host's first player is hit by its weapon's bullet, its damage
multiplied by the scale (a small one only the shields feel); every change of
the player's health, shields or shield stun is logged with its tick. */
static void network_test_damage_script(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	struct object_datum *object;
	char const *cursor = network_test.damage_script;
	long entry = 0;

	if (!network_test.damage_script[0])
		return;
	data_iterator_new(&iterator, player_data);
	player = (struct player_datum *)data_iterator_next(&iterator);
	if (!player || player->unit_index == NONE)
		return;
	object = object_get(player->unit_index);
	while (*cursor)
	{
		char *end;
		double seconds = strtod(cursor, &end);
		double scale;
		boolean head;

		if (end == cursor || *end != ':')
			break;
		scale = strtod(end + 1, &end);
		/* (port: weapon pack tests: "seconds:scaleh" hits the head, its collision model's head material, for the
		battle rifle's headshot counts) */
		head = *end == 'h';
		if (head)
			end++;
		if (entry == network_test.damage_done && game_time_get() >= (long)(seconds * TICKS_PER_SECOND))
		{
			struct unit_datum *unit = unit_get(player->unit_index);
			long weapon_index = unit->unit.current_weapon_index == NONE ? NONE :
				unit->unit.weapon_object_indices[unit->unit.current_weapon_index];

			network_test.damage_done++;
			if (weapon_index != NONE)
			{
				struct weapon_definition *weapon = weapon_definition_get(object_get(weapon_index)->definition_index);

				if (weapon->weapon.triggers.count > 0)
				{
					struct weapon_trigger_definition *trigger =
						TAG_BLOCK_GET_ELEMENT(&weapon->weapon.triggers, 0, struct weapon_trigger_definition);

					if (trigger->projectile.index != NONE)
					{
						struct damage_data damage;

						damage_data_new(&damage, projectile_definition_get(trigger->projectile.index)->projectile.impact_damage.index);
						damage.owner_player_index = NONE;
						damage.owner_object_index = NONE;
						damage.owner_team_index = NONE;
						damage.origin = object->object.position;
						damage.epicenter = object->object.position;
						damage.direction.i = 1.0f;
						damage.direction.j = 0.0f;
						damage.direction.k = 0.0f;
						damage.scale = (real)scale;
						scenario_location_from_point(&damage.location, &damage.epicenter);
						object_cause_damage(&damage, player->unit_index, NONE, NONE,
							head ? ae_damage_head_material_index(player->unit_index) : NONE, NULL);
						platform_log("network test: tick %ld hit x%.1f%s: health %.4f shield %.4f stun %d dead %d",
							game_time_get(), scale, head ? " head" : "", object->object.body_vitality,
							object->object.shield_vitality, (int)object->object.shield_stun_ticks,
							TEST_FLAG(object->object.damage_flags, _object_dead_bit) ? 1 : 0);
					}
				}
			}
		}
		entry++;
		cursor = *end == ',' ? end + 1 : end;
	}
	if (network_test.damage_logged_time != game_time_get() && (
		network_test.damage_last[0] != object->object.body_vitality ||
		network_test.damage_last[1] != object->object.shield_vitality ||
		network_test.damage_last[2] != (real)object->object.shield_stun_ticks))
	{
		network_test.damage_logged_time = game_time_get();
		network_test.damage_last[0] = object->object.body_vitality;
		network_test.damage_last[1] = object->object.shield_vitality;
		network_test.damage_last[2] = (real)object->object.shield_stun_ticks;
		platform_log("network test: tick %ld health %.4f shield %.4f stun %d", game_time_get(),
			object->object.body_vitality, object->object.shield_vitality, (int)object->object.shield_stun_ticks);
	}
}

/* (a probe: halo --probe) the first game found, printed as JSON on the
standard output, and the end; when given_up, that none was. With an
invite (p2p.c joins the command line's), the invite's host's game alone:
the tunnel's peers are its host only, and a game on the LAN is another */
static void network_test_probe_answer(
	boolean given_up)
{
	static char const *const engines[] = { "none", "ctf", "slayer", "oddball", "king", "race" };
	struct network_advertised_game_summary game;
	long index;
	int stage;
	boolean invited = p2p_join_progress(&stage, NULL, 0) != P2P_JOIN_NONE;

	for (index = 0; index < PROBE_MAXIMUM_GAMES; index++)
	{
		char name[2 * NUMBEROF(game.name) + 1];
		char map[2 * sizeof(game.map_name) + 1];
		char const *last;
		long character, length = 0;

		if (!network_game_client_port_advertised_game_summary(index, &game) ||
			(invited && !menu_advertised_game_from_peer(index)))
		{
			continue;
		}
		/* (printable ASCII, quotes and backslashes escaped) */
		for (character = 0; character < (long)NUMBEROF(game.name) && game.name[character]; character++)
		{
			char ascii = game.name[character] >= 0x20 && game.name[character] < 0x7F ? (char)game.name[character] : '?';

			if (ascii == '"' || ascii == '\\')
				name[length++] = '\\';
			name[length++] = ascii;
		}
		name[length] = 0;
		/* (the map's name, the last part of its path) */
		last = strrchr(game.map_name, '\\');
		last = last ? last + 1 : game.map_name;
		for (length = 0, character = 0; last[character]; character++)
		{
			char ascii = (unsigned char)last[character] >= 0x20 && (unsigned char)last[character] < 0x7F ?
				last[character] : '?';

			if (ascii == '"' || ascii == '\\')
				map[length++] = '\\';
			map[length++] = ascii;
		}
		map[length] = 0;
		printf("{\"ok\":true,\"name\":\"%s\",\"map\":\"%s\",\"gametype\":\"%s\",\"players\":%d,"
			"\"max_players\":%d,\"teams\":%s,\"in_progress\":%s,\"open\":%s,\"dedicated\":%s,"
			"\"network_version\":%u,\"kind\":%u}\n", name, map,
			game.engine_type >= 0 && game.engine_type < (short)NUMBEROF(engines) ? engines[game.engine_type] : "none",
			game.player_count, game.maximum_player_count, game.has_teams ? "true" : "false",
			game.in_progress ? "true" : "false", game.open ? "true" : "false", game.dedicated ? "true" : "false",
			(unsigned)game.version, (unsigned)game.kind);
		fflush(stdout);
		exit(0);
	}
	if (given_up)
	{
		printf("{\"ok\":false,\"error\":\"no game answered in %d seconds%s\"}\n", PROBE_SECONDS,
			invited ? " from the invite's host" : "");
		fflush(stdout);
		exit(1);
	}
}

void network_test_update(
	boolean main_menu_loaded,
	real seconds)
{
	if (!network_test.checked)
		network_test_read_settings();
	if (network_test.mode == _network_test_off)
		return;

	/* (per frame, not per second: ticks matter) */
	if (game_in_progress() && !main_menu_loaded && network_test.mode == _network_test_host)
	{
		if (game_time_get() < network_test.damage_logged_time)
		{
			/* (a new game: the script and the change log start over) */
			network_test.damage_done = 0;
			network_test.ball_done = FALSE;
			network_test.damage_logged_time = 0;
			network_test.damage_last[0] = network_test.damage_last[1] = network_test.damage_last[2] = -1.0f;
		}
		network_test_damage_script();
	}
	if (network_test.mode == _network_test_host && game_in_progress() && !main_menu_loaded) ae_test_all_weapons_update(); /* AE hook */
	/* debug.network_test_chat: each client's player types "gg ez" on the
	text chat's on-screen keyboard 32 seconds on, as a controller does, and
	sends it to the team (each frame, not each second, as the rest) */
	if (network_test.chat_time > 0.0f && network_test.mode != _network_test_host && game_in_progress() &&
		!main_menu_loaded &&
		game_time_get() >= (long)(network_test.chat_time * TICKS_PER_SECOND) + 32 * TICKS_PER_SECOND)
	{
		network_test_keyboard_line(game_time_get() - (long)(network_test.chat_time * TICKS_PER_SECOND) -
			32 * TICKS_PER_SECOND);
	}
	/* the game running: report (from the start of each game, or a revert to
	a checkpoint: the time starts over, and the cues with it) */
	if (game_in_progress() && game_time_get() < network_test.logged_time)
	{
		network_test.logged_time = 0;
		network_test.cues_start = NONE;
		network_test.next_cue = 0;
	}
	if (game_in_progress() && !main_menu_loaded)
		network_test_run_cues();
	if (game_in_progress() && !main_menu_loaded && game_time_get() - network_test.logged_time >= TICKS_PER_SECOND)
	{
		network_test.logged_time = game_time_get();
		network_test_log_players();
		if (network_test.shoot_interval > 0.0f &&
			game_time_get() % MAX(1, (long)(network_test.shoot_interval * TICKS_PER_SECOND)) < TICKS_PER_SECOND)
		{
			network_test_shoot();
		}
		/* (the host brings the players near a second before they shoot;
		not the last while the vehicle test has it) */
		if (network_test.mode == _network_test_host && network_test.shoot_interval > 0.0f &&
			(game_time_get() + TICKS_PER_SECOND) % MAX(1, (long)(network_test.shoot_interval * TICKS_PER_SECOND)) <
				TICKS_PER_SECOND)
		{
			long vehicle_time = (long)(network_test.vehicle_time * TICKS_PER_SECOND);

			network_test_gather(network_test.vehicle_time > 0.0f && game_time_get() >= vehicle_time - 2 * TICKS_PER_SECOND);
		}
		if (network_test.mode == _network_test_host && network_test.ball_time > 0.0f && !network_test.ball_done &&
			game_time_get() >= (long)(network_test.ball_time * TICKS_PER_SECOND))
		{
			network_test_ball();
		}
		if (network_test.pickup_time > 0.0f)
		{
			long pickup_time = (long)(network_test.pickup_time * TICKS_PER_SECOND);

			/* (two weapons first: picking up a third swaps) */
			if (network_test.mode == _network_test_host && game_time_get() >= pickup_time - 3 * TICKS_PER_SECOND &&
				game_time_get() - (pickup_time - 3 * TICKS_PER_SECOND) < TICKS_PER_SECOND)
			{
				network_test_second_weapon();
			}

			if (game_time_get() >= pickup_time && game_time_get() - pickup_time < TICKS_PER_SECOND)
			{
				network_test_pickup();
			}
			/* (debug.network_test_pickup_give: the host's pickup for the last
			player, two seconds on) */
			if (network_test.mode == _network_test_host && network_test.pickup_give &&
				network_test.pickup_weapon_index != NONE && game_time_get() >= pickup_time + 2 * TICKS_PER_SECOND)
			{
				struct data_iterator iterator;
				struct player_datum *player;
				struct player_datum *last = NULL;
				struct weapon_datum *weapon = weapon_try_and_get(network_test.pickup_weapon_index);

				data_iterator_new(&iterator, player_data);
				while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
					last = player;
				if (last && last->unit_index != NONE && weapon && weapon->object.parent_object_index == NONE &&
					unit_add_weapon_to_inventory(last->unit_index, network_test.pickup_weapon_index, TRUE))
				{
					platform_log("network test: the host gives the last player weapon %lx at tick %ld",
						network_test.pickup_weapon_index, game_time_get());
				}
				network_test.pickup_weapon_index = NONE;
			}
			/* (standing there for four seconds, the button held from a second
			on) */
			if (network_test.mode == _network_test_join)
			{
				boolean hold = game_time_get() >= pickup_time &&
					game_time_get() < pickup_time + 4 * TICKS_PER_SECOND;

				test_input_hold_action(hold);
			}
		}
		/* debug.network_test_chat: each machine's player says a line to
		everyone that many seconds in, one to the team two seconds on, and
		one to the vehicle two more on, the host having seated the last two
		players in one vehicle (the others in none) three seconds before;
		then the host mutes the last player for a minute (a client's /mute
		of the host refused), each says a line to everyone (the last
		player's going no further than the host), the host unmutes it, and
		each says another; then each says a line with a link (refused, as
		network.text_chat "on" refuses links), the host turns the text chat
		off (network.text_chat "off"), and each says a line (refused); then
		the host mutes the last player for everyone, as its scoreboard does,
		and each says a line, and the last player's machine mutes the host's
		for itself, as a client's scoreboard does, and each says another */
		if (network_test.chat_time > 0.0f)
		{
			long chat_time = (long)(network_test.chat_time * TICKS_PER_SECOND);
			char const *machine = network_test.mode == _network_test_host ? "the host" : "a client";
			char text[64];

			if (game_time_get() < chat_time - 3 * TICKS_PER_SECOND)
				network_test.chat_lines_said = 0;
			if (network_test.mode == _network_test_host && network_test.chat_lines_said == 0 &&
				game_time_get() >= chat_time - 3 * TICKS_PER_SECOND)
			{
				network_test_seat_together();
				network_test.chat_lines_said = -1;
			}
			if (network_test.chat_lines_said <= 0 && game_time_get() >= chat_time)
			{
				/* (long enough to wrap in the chat's column) */
				snprintf(text, sizeof(text), "hello everyone from %s, long enough to wrap the column", machine);
				network_chat_say(_chat_channel_everyone, text);
				network_test.chat_lines_said = 1;
			}
			if (network_test.chat_lines_said == 1 && game_time_get() >= chat_time + 2 * TICKS_PER_SECOND)
			{
				snprintf(text, sizeof(text), "hello team from %s", machine);
				network_chat_say(_chat_channel_team, text);
				network_test.chat_lines_said++;
			}
			if (network_test.chat_lines_said == 2 && game_time_get() >= chat_time + 4 * TICKS_PER_SECOND)
			{
				snprintf(text, sizeof(text), "hello vehicle from %s", machine);
				network_chat_say(_chat_channel_vehicle, text);
				network_test.chat_lines_said++;
			}
			if (network_test.chat_lines_said == 3 && game_time_get() >= chat_time + 6 * TICKS_PER_SECOND)
			{
				char name[16];

				network_test_last_player_name(name, sizeof(name));
				if (network_test.mode == _network_test_host)
					snprintf(text, sizeof(text), "/mute %s 1", name);
				else
					snprintf(text, sizeof(text), "/mute host");
				network_chat_say(_chat_channel_everyone, text);
				network_test.chat_lines_said++;
			}
			if (network_test.chat_lines_said == 4 && game_time_get() >= chat_time + 8 * TICKS_PER_SECOND)
			{
				snprintf(text, sizeof(text), "while muted from %s", machine);
				network_chat_say(_chat_channel_everyone, text);
				network_test.chat_lines_said++;
			}
			if (network_test.chat_lines_said == 5 && game_time_get() >= chat_time + 10 * TICKS_PER_SECOND)
			{
				char name[16];

				network_test_last_player_name(name, sizeof(name));
				if (network_test.mode == _network_test_host)
				{
					snprintf(text, sizeof(text), "/unmute %s", name);
					network_chat_say(_chat_channel_everyone, text);
				}
				network_test.chat_lines_said++;
			}
			if (network_test.chat_lines_said == 6 && game_time_get() >= chat_time + 12 * TICKS_PER_SECOND)
			{
				snprintf(text, sizeof(text), "after unmute from %s", machine);
				network_chat_say(_chat_channel_everyone, text);
				network_test.chat_lines_said++;
			}
			if (network_test.chat_lines_said == 7 && game_time_get() >= chat_time + 14 * TICKS_PER_SECOND)
			{
				/* (a client's sent past its own check, for the host to refuse) */
				snprintf(text, sizeof(text), "join us at discord(dot)gg/halo from %s", machine);
				if (network_test.mode == _network_test_host)
					network_chat_say(_chat_channel_everyone, text);
				else
					network_chat_say_unchecked(_chat_channel_everyone, text);
				network_test.chat_lines_said++;
			}
			if (network_test.chat_lines_said == 8 && game_time_get() >= chat_time + 16 * TICKS_PER_SECOND)
			{
				if (network_test.mode == _network_test_host && !config_write("network.text_chat", "off"))
					platform_log("network test: could not turn the text chat off");
				network_test.chat_lines_said++;
			}
			if (network_test.chat_lines_said == 9 && game_time_get() >= chat_time + 18 * TICKS_PER_SECOND)
			{
				snprintf(text, sizeof(text), "with the chat off from %s", machine);
				if (network_test.mode == _network_test_host)
					network_chat_say(_chat_channel_everyone, text);
				else
					network_chat_say_unchecked(_chat_channel_everyone, text);
				network_test.chat_lines_said++;
			}
			/* (and on again: config.toml is the executable's folder's, which
			every test of it shares) */
			if (network_test.chat_lines_said == 10 && game_time_get() >= chat_time + 20 * TICKS_PER_SECOND)
			{
				if (network_test.mode == _network_test_host && !config_write("network.text_chat", "on"))
					platform_log("network test: could not turn the text chat on");
				network_test.chat_lines_said++;
			}
			/* (the scoreboard's mutes: the host's of the last player for
			everyone, their lines and voice, then the last player's machine's
			own mute of the host's machine, here alone) */
			if (network_test.chat_lines_said == 11 && game_time_get() >= chat_time + 22 * TICKS_PER_SECOND)
			{
				if (network_test.mode == _network_test_host)
					network_chat_host_mute(network_test_last_player_index(), TRUE);
				network_test.chat_lines_said++;
			}
			if (network_test.chat_lines_said == 12 && game_time_get() >= chat_time + 24 * TICKS_PER_SECOND)
			{
				snprintf(text, sizeof(text), "after the scoreboard mute from %s", machine);
				network_chat_say(_chat_channel_everyone, text);
				network_test.chat_lines_said++;
			}
			if (network_test.chat_lines_said == 13 && game_time_get() >= chat_time + 28 * TICKS_PER_SECOND)
			{
				if (network_test.mode == _network_test_host)
					network_chat_host_mute(network_test_last_player_index(), FALSE);
				else if (network_test_last_player_local())
					network_voice_mute_machine(network_test_first_player_machine(), TRUE);
				network_test.chat_lines_said++;
			}
			if (network_test.chat_lines_said == 14 && game_time_get() >= chat_time + 30 * TICKS_PER_SECOND)
			{
				snprintf(text, sizeof(text), "after the local mute from %s", machine);
				network_chat_say(_chat_channel_everyone, text);
				network_test.chat_lines_said++;
			}
		}
		if (network_test.mode == _network_test_host && network_test.vehicle_time > 0.0f)
		{
			long enter_time = (long)(network_test.vehicle_time * TICKS_PER_SECOND);

			if (game_time_get() >= enter_time && game_time_get() - enter_time < TICKS_PER_SECOND)
				network_test_vehicle(TRUE);
			if (game_time_get() >= enter_time + 15 * TICKS_PER_SECOND &&
				game_time_get() - enter_time - 15 * TICKS_PER_SECOND < TICKS_PER_SECOND)
			{
				network_test_vehicle(FALSE);
			}
		}
		/* ... and 20 seconds on, the last player getting in as a player does:
		at the entrance, the button held for three seconds */
		if (network_test.vehicle_time > 0.0f)
		{
			long approach_time = (long)(network_test.vehicle_time * TICKS_PER_SECOND) + 20 * TICKS_PER_SECOND;

			if (game_time_get() >= approach_time && game_time_get() - approach_time < TICKS_PER_SECOND)
				network_test_vehicle_approach();
			if (network_test.mode == _network_test_join && network_test.pickup_time <= 0.0f)
			{
				test_input_hold_action(game_time_get() >= approach_time + TICKS_PER_SECOND &&
					game_time_get() < approach_time + 4 * TICKS_PER_SECOND);
			}
		}
		/* debug.network_test_pause: the first player's pause menu opened
		that many seconds in, as START opens it (to see it) */
		if (network_test.pause_time > 0.0f && !network_test.paused &&
			game_time_get() >= (long)(network_test.pause_time * TICKS_PER_SECOND))
		{
			network_test.paused = TRUE;
			platform_log("network test: the pause menu opens");
			ui_widget_load_by_name_or_tag(network_test.local_players > 1 ?
				"ui\\shell\\multiplayer_game\\pause_game\\2p_pause_game" :
				"ui\\shell\\multiplayer_game\\pause_game\\1p_pause_game", NONE, NULL, 0, NONE, NONE, NONE);
		}
		/* debug.network_test_team_change: every so often a player of each
		machine asks to change teams, as the pause menu's CHANGE TEAMS does
		(team_change.c); the joining machine's half an interval later */
		if (network_test.team_change_interval > 0.0f && game_engine_can_score())
		{
			static long asked_period = 0;
			long interval = MAX(1, (long)(network_test.team_change_interval * TICKS_PER_SECOND));
			long offset = network_test.mode == _network_test_join ? interval / 2 : 0;
			long period = MAX(0, game_time_get() - offset) / interval;
			short players = (short)MAX(1, network_test.local_players);

			if (period < asked_period)
				asked_period = period;
			if (period > asked_period)
			{
				asked_period = period;
				extern boolean team_change_ask(short local_player_index);

				/* (split screen: each player in turn) */
				platform_log("network test: player %d asks to change teams", (int)(period % players) + 1);
				team_change_ask((short)(period % players));
			}
		}
		/* debug.network_test_quit: controller 1's player on this machine
		quits, as their pause menu's QUIT does (network_game_remove_local_player),
		once; with split screen the others stay */
		if (network_test.quit_time > 0.0f && !network_test.quit &&
			game_time_get() >= (long)(network_test.quit_time * TICKS_PER_SECOND))
		{
			network_test.quit = TRUE;
			network_game_client_local_player_quit(0);
			if (local_player_count() > 1)
				player_ui_local_player_left_multiplayer_game(0);
			platform_log("network test: controller 1's player quits");
		}
		/* debug.network_test_hurt: the host leaves the first player 40% of
		their health, once */
		if (network_test.mode == _network_test_host && network_test.hurt_time > 0.0f && !network_test.hurt &&
			game_time_get() >= (long)(network_test.hurt_time * TICKS_PER_SECOND))
		{
			network_test.hurt = TRUE;
			struct data_iterator iterator;
			struct player_datum *player;

			data_iterator_new(&iterator, player_data);
			player = (struct player_datum *)data_iterator_next(&iterator);
			if (player && player->unit_index != NONE)
			{
				object_get(player->unit_index)->object.body_vitality = 0.4f;
				platform_log("network test: the first player hurt to 40%%");
			}
		}
		/* debug.network_test_kill: the host kills the last other player every so
		often, to test deaths and respawns reaching the clients */
		if (network_test.mode == _network_test_host && network_test.kill_interval > 0.0f &&
			game_time_get() % MAX(1, (long)(network_test.kill_interval * TICKS_PER_SECOND)) < TICKS_PER_SECOND)
		{
			struct data_iterator iterator;
			struct player_datum *player;
			struct player_datum *last = NULL;

			struct player_datum *first = NULL;
			long first_index = NONE;

			/* (the killer is the host's own player when it has one, else the
			first: the host deals no damage a remote player owns, which that
			player's machine reports, network_damage_deals; the victim the last
			other player) */
			data_iterator_new(&iterator, player_data);
			while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
			{
				if (!first || (player->local_player_index != NONE && first->local_player_index == NONE))
				{
					first = player;
					first_index = iterator.datum_index;
				}
			}
			data_iterator_new(&iterator, player_data);
			while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
			{
				if (player != first)
					last = player;
			}
			/* (debug.network_test_kill_host: the other way round, the host's
			player killed by the last other of this machine's players) */
			if (network_test.kill_host)
			{
				struct player_datum *victim = NULL;
				struct player_datum *killer = NULL;
				long killer_index = NONE;

				/* (the victim local player 1: controller 1, whose presses
				debug.test_input makes) */
				data_iterator_new(&iterator, player_data);
				while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
				{
					if (player->local_player_index == 0)
						victim = player;
					else if (player->local_player_index != NONE)
					{
						killer = player;
						killer_index = iterator.datum_index;
					}
				}
				if (killer && victim && killer->unit_index != NONE && victim->unit_index != NONE)
				{
					platform_log("network test: another of the host's players kills local player 1");
					damage_kill_object_for_player(victim->unit_index, killer_index);
				}
				last = NULL;
			}
			if (last && first && last->unit_index != NONE && first->unit_index != NONE)
			{
				/* killed by the first player: a kill that scores */
				platform_log("network test: the host's player (else the first) kills the last other");
				damage_kill_object_for_player(last->unit_index, first_index);
				/* and picks up a weapon lying about, and two grenades of each kind */
				{
					struct object_iterator objects;
					struct unit_datum *unit = unit_get(first->unit_index);
					long held_index = unit->unit.weapon_object_indices[0];
					long held_definition_index = held_index != NONE ? weapon_get(held_index)->definition_index : NONE;

					object_iterator_new(&objects, _object_mask_weapon, 0);
					while (object_iterator_next(&objects))
					{
						struct weapon_datum *weapon = weapon_get(objects.index);

						if (weapon->object.parent_object_index == NONE &&
							weapon->definition_index != held_definition_index)
						{
							if (unit_add_weapon_to_inventory(first->unit_index, objects.index, TRUE))
								platform_log("network test: the first player picks up a weapon");
							break;
						}
					}
					unit->unit.grenade_counts[0] = 2;
					unit->unit.grenade_counts[1] = 2;
				}
			}
		}
	}

	/* the next game of the list: once the scores have been shown a while,
	the host's button (the bots may press it first) */
	if (network_test.mode == _network_test_host && network_test.started && game_engine_running() &&
		!main_menu_loaded && !game_engine_can_score())
	{
		network_test.game_over = TRUE;
		if (game_engine_showing_postgame() && global_network_game_server_get())
		{
			network_test.postgame_seconds += seconds;
			if (network_test.postgame_seconds >= 3.0f && network_test_variant(network_test.variant_index + 1, NULL, 0))
			{
				network_test.postgame_seconds = 0.0f;
				network_game_server_reset_to_pregame(global_network_game_server_get());
			}
		}
	}
	/* (a joining machine's player: the other team again in the next game's
	lobby, whose variant may have teams where the last had none) */
	if (network_test.mode == _network_test_join && network_test.team_set && game_engine_running() &&
		!main_menu_loaded && !game_engine_can_score())
	{
		network_test.game_over = TRUE;
	}
	if (network_test.mode == _network_test_join && network_test.game_over && main_menu_loaded)
	{
		network_test.game_over = FALSE;
		network_test.team_set = FALSE;
		network_test.joined_seconds = 0.0f;
	}
	/* ... back in the lobby, set up as the first was */
	if (network_test.mode == _network_test_host && network_test.game_over && main_menu_loaded)
	{
		network_test.game_over = FALSE;
		network_test.postgame_seconds = 0.0f;
		if (network_test_variant(network_test.variant_index + 1, NULL, 0))
		{
			network_test.variant_index++;
			network_test.started = FALSE;
			network_test.map_set = FALSE;
			network_test.setup_seconds = 0.0f;
			network_test.menu_seconds = 0.0f;
			/* (as picking the next game's map does: the scores' map choice
			holds the countdown) */
			if (global_network_game_server_get())
				network_game_server_pause_countdown(global_network_game_server_get(), FALSE);
			/* a join that arrived as the last game ended would refuse every
			later one, the server holding it behind the queued player */
			network_game_server_port_clear_queued_players(global_network_game_server_get());
			platform_log("network test: the next game");
		}
	}

	if (!main_menu_loaded)
		return;
	/* the intro movie skipped first, as a button press does: the movie
	holds the texture cache's spare memory, which starting the map
	takes too (texture_cache_steal_memory's assertion) */
	if (bink_playback_in_progress())
	{
		bink_playback_stop();
		network_test.menu_seconds = 0.0f;
		platform_log("network test: skipped the movie");
		return;
	}
	network_test.menu_seconds += seconds;
	/* (the main menu settling first) */
	if (network_test.menu_seconds < 2.0f)
		return;

	switch (network_test.mode)
	{
	case _network_test_browse:
	{
		struct p2p_listing listed[8], games[8];
		int listed_count, count = 0, pass, index;

		if (!network_test.browsing)
		{
			network_test.browsing = TRUE;
			p2p_lobby_browse(TRUE);
			platform_log("network test: browsing the public games");
		}
		/* (listings arriving meanwhile from every broker) */
		network_test.browse_seconds += seconds;
		listed_count = p2p_lobby_games(listed, NUMBEROF(listed));
		if (!listed_count || network_test.browse_seconds < 5.0f)
			break;
		/* those that can be joined (no password, not failed): the list has
		the most players first; co-op, the game with no game engine, last */
		for (pass = 0; pass < 2; pass++)
		{
			for (index = 0; index < listed_count; index++)
			{
				if (listed[index].invite[0] && !listed[index].failed && !listed[index].engine_type == pass)
					games[count++] = listed[index];
			}
		}
		if (!network_test.browse_listed)
		{
			network_test.browse_listed = TRUE;
			for (index = 0; index < listed_count; index++)
			{
				platform_log("network test: listed %s's game (%s, %s) %d/%d players%s%s", listed[index].name,
					listed[index].map, listed[index].gametype, listed[index].player_count,
					listed[index].maximum_player_count, listed[index].engine_type ? "" : ", co-op",
					listed[index].invite[0] ? "" : ", locked");
			}
		}
		if (count > 0 && p2p_join_listing(&games[0]))
		{
			p2p_lobby_browse(FALSE);
			platform_log("network test: joining %s's game (%s) from the server browser", games[0].name, games[0].map);
			network_test.mode = _network_test_join;
		}
		break;
	}
	case _network_test_host:
		if (network_test.set_up && !network_test.local && !network_test.invite_logged)
		{
			char invite[256];

			if (p2p_invite_link(invite, sizeof(invite)))
			{
				network_test.invite_logged = TRUE;
				platform_log("network test: invite %s", invite);
			}
		}
		if (!network_test.set_up)
		{
			network_test.set_up = TRUE;
			main_set_multiplayer_map_name(network_test_map());
			if (ae_menus_active() && network_test_host_variant(&network_test_ae_rules.variant, &network_test_ae_rules.options) && ae_screen_create_game_test_open(network_test_map(), &network_test_ae_rules.variant, &network_test_ae_rules.options, network_test.local_players, network_test.start_delay)) /* AE hook */
				network_test.started = TRUE; /* AE hook */
			else if (network_test.local) /* AE hook */
			{
				/* as Multiplayer > Split Screen does (split_screen_game_initialize) */
				ui_widgets_close_all();
				dispose_global_network_game_server();
				dispose_global_network_game_client();
				network_game_accept_remote_connections(FALSE);
				game_engine_playlist_initialize();
				if (create_global_network_game_server())
				{
					game_engine_playlist_begin();
					game_connection_set(_game_connection_network_server);
				}
				if (!global_network_game_client_get())
					create_global_network_game_client();
				ui_widget_load_by_name_or_tag(
					"ui\\shell\\main_menu\\multiplayer_type_select\\split_screen\\pregame\\splitscreen_pregame_wrapper_normal",
					NONE, NULL, NONE, NONE, NONE, NONE);
				platform_log("network test: a local game on %s", network_test_map());
			}
			else
			{
				player_ui_fast_setup_network_server();
				platform_log("network test: hosting %s", network_test_map());
				/* (in the server browser, as Create Game > Internet's PUBLIC) */
				if (network_test.public_game)
					p2p_set_hosting_public(TRUE);
			}
		}
		else if (!network_test.started)
		{
			network_test.setup_seconds += seconds;
			/* the map (fast setup clears it), and a player for controller 1, as
			pressing A in the lobby adds one */
			/* ("host:<level>:coop": network co-op of a campaign level, as the
			Map screen's level and difficulty set it up) */
			if (!network_test.map_set && network_test.setup_seconds >= 1.0f && global_network_game_server_get() &&
				!strcmp(network_test.variant_name, "coop"))
			{
				short level = main_get_solo_level_from_name(network_test.map_name);

				if (level != NONE &&
					ui_widget_port_cooperative_level_choose(main_get_solo_level_name(level), _game_difficulty_level_normal))
				{
					platform_log("network test: co-op, %s", main_get_solo_level_name(level));
				}
				else
					platform_log("network test: no co-op level %s", network_test.map_name);
				network_test.map_set = TRUE;
			}
			if (!network_test.map_set && network_test.setup_seconds >= 1.0f && global_network_game_server_get())
			{
				char path[128];
				char variant_name[64];
				struct game_variant variant;

				/* (a level of its own path, else a multiplayer map's name) */
				if (strchr(network_test.map_name, '\\'))
					snprintf(path, sizeof(path), "%s", network_test.map_name);
				else
					snprintf(path, sizeof(path), "levels\\test\\%s\\%s", network_test_map(), network_test_map());
				network_test_variant(network_test.variant_index, variant_name, sizeof(variant_name));
				if (!strcmp(variant_name, "coop"))
				{
					/* (the Map screen's co-op: menu_functions.c) */
					extern boolean ui_widget_port_cooperative_level_choose(char const *map_name, short difficulty);
					extern boolean ui_widget_port_mcc_cooperative_level_choose(char const *map_name, short difficulty);

					/* Exercise the same independent MCC route as its campaign menu. */
					if (!(mcc_level_name(path) ?
						ui_widget_port_mcc_cooperative_level_choose(path, _game_difficulty_level_normal) :
						ui_widget_port_cooperative_level_choose(path, _game_difficulty_level_normal)))
						platform_log("network test: %s is not a campaign level, for co-op", path);
					platform_log("network test: game %d, co-op", network_test.variant_index + 1);
				}
				else
				{
					struct game_variant_options options;

					network_game_server_change_map_name(global_network_game_server_get(), path);
					network_test_host_variant(&variant, &options);
					player_ui_set_game_variant(&variant);
					player_ui_set_game_variant_options(&options);
					network_game_server_change_game_variant(global_network_game_server_get(), &variant);
				}
				network_test.map_set = TRUE;
			}
			/* debug.network_test_local_players: split screen, a player a
			second for the controllers from the last down to 1 (as a second
			controller's player joined first, then player 1's) */
			if (!network_test.player_added &&
				network_test.setup_seconds >= 2.0f + network_test.local_players_added &&
				global_network_game_client_get())
			{
				short controller = (short)(network_test.local_players - 1 - network_test.local_players_added);

				network_test_local_player_profile(controller);
				/* (debug.network_test_name: OpenCE's, player 1's name) */
				if (!controller)
					network_test_set_name();
				if (network_game_client_add_player(global_network_game_client_get(), controller))
				{
					if (network_test.local_players > 1)
						player_ui_local_player_joined_multiplayer_game(controller);
					network_test.local_players_added++;
					platform_log("network test: a player added for controller %d", controller + 1);
				}
				network_test.player_added = network_test.local_players_added >= network_test.local_players;
			}
			/* port: the map rotation ("host:<map>+<map>"): the map and gametype set
			once more a second and a half before the start (a later game's lobby,
			opened from the scores, may have put its own map back meanwhile) */
			{
				static short map_set_again_for = NONE;

				if (strchr(network_test.map_name, '+') && network_test.map_set &&
					map_set_again_for != network_test.variant_index &&
					network_test.setup_seconds >= network_test.start_delay - 1.5f)
				{
					map_set_again_for = network_test.variant_index;
					network_test.map_set = FALSE;
				}
			}
			if (network_test.setup_seconds >= network_test.start_delay)
			{
				/* port: (a later game's lobby opens its map choice a moment after
				the scores, which holds the countdown again: let go here too, or
				the game never starts) */
				if (network_test.variant_index > 0 && global_network_game_server_get())
					network_game_server_pause_countdown(global_network_game_server_get(), FALSE);
				network_test.started = TRUE;
				network_game_client_request_immediate_start();
				platform_log("network test: starting the game");
			}
		}
		break;
	case _network_test_probe:
		if (!network_test.set_up)
		{
			network_test.set_up = TRUE;
			dispose_global_network_game_client();
			dispose_global_network_game_server();
			if (create_global_network_game_client())
				game_connection_set(_game_connection_network_client);
		}
		else
		{
			network_test.probe_seconds += seconds;
			network_test_probe_answer(network_test.probe_seconds >= PROBE_SECONDS);
		}
		break;
	case _network_test_join:
		if (!network_test.set_up)
		{
			network_test.set_up = TRUE;
			dispose_global_network_game_client();
			dispose_global_network_game_server();
			network_test_set_name();
			if (create_global_network_game_client())
			{
				game_connection_set(_game_connection_network_client);
				/* (the player joined to multiplayer first, as a player picking
				their profile: the pregame screen then asks for them every
				frame until they are in the settings) */
				{
					short controller;

					for (controller = 0; controller < network_test.local_players; controller++)
						player_ui_local_player_joined_multiplayer_game(controller);
				}
				platform_log("network test: searching for games");
			}
		}
		else if (!network_test.joined && network_game_client_join_first_available_game())
		{
			network_test.joined = TRUE;
			ui_widgets_close_all();
			ui_widget_load_by_name_or_tag(
				pc_menus_screen("ui\\shell\\main_menu\\multiplayer_type_select\\connected\\pregame\\connected_pregame_screen"),
				NONE, NULL, NONE, NONE, NONE, NONE);
			platform_log("network test: joining");
		}
		else if (network_test.joined && !network_test.player_added)
		{
			network_test.joined_seconds += seconds;
			if (network_test.joined_seconds >= 3.0f && global_network_game_client_get())
				network_test.player_added = TRUE;
		}
		/* (the other team from the host's player: a team game needs both) */
		else if (network_test.player_added && !network_test.team_set)
		{
			network_test.joined_seconds += seconds;
			if (network_test.joined_seconds >= 5.0f)
				network_test.team_set = network_game_client_set_team(NONE);
		}
		break;
	}
}

/* the game's time in seconds while a game runs, -1 otherwise: the scripted
player's clock (xinput_sdl.c, debug.test_input) */
double network_test_game_seconds(
	void)
{
	return game_time_initialized() && game_in_progress() ? (double)game_time_get() / TICKS_PER_SECOND : -1.0;
}

#else

#include "cseries.h"

/* (the main loop's call, answered with nothing) */
void network_test_update(
	boolean main_menu_loaded,
	real seconds)
{
	return;
}

#endif /* HALO_CONSOLE_NO_NETWORK_TEST */
