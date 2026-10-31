/*
AE_GLUE_LOBBY.C

The lobby's glue (ae_glue_lobby.h), from the scratch draft of M1 Task 6's Check B
(branch ae/menus-checks, df6f6a3e; notes/ae-early-checks.md "Check B"): the
steps of player_ui_fast_setup_network_server without its pregame widget, which
proved, from C and with no widget at all, hosting, a local player, the roster,
the map and gametype, starting, ending (game_engine_end_game) and going back to
the lobby (network_game_server_reset_to_pregame).

Back from a game, upstream loads its own SELECT MAP screen
(network_game_reset_to_pregame_ui, ui_widget.c): for AE's own sessions the hook
there (ae_lobby_take_pregame_ui) shows AE's pregame screen instead.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "saved games/player_profile.h"
#include "interface/player_ui.h"
#include "interface/ui_widget.h"
#include "main/main.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_game_protocol.h"
#include "networking/network_server_manager.h"
#include "../src/p2p.h"
#include "../src/ae_resolve.h"
#include "halo_port_limits.h"
#include "ae_glue_lobby.h"
#include "ae_glue_profiles.h"
#include "ae_invite_rules.h"
#include "ae_lan_bots.h"
#include "bots.h"
#include "ae_hooks.h"
#include "ae_strings.h"
#include "ae_ui.h"

void platform_log(char const *format, ...);
#ifdef HALO_GAME_BROWSER
/* network_client_manager.c (the Server Browser's: browser_screen.c declares it the same way): 1 joining the invite's
host, 0 not advertised yet, -1 it can't be joined (another version: the player is told) */
long network_game_client_join_invite_host(char const *invite);
#endif
/* ae_direct_join_body.h (inside network_client_manager.c): the connection to a host at an address: 0 not searching yet, 1
started, -1 could not */
long ae_network_game_client_join_address(unsigned long address, unsigned short port);
/* port_config.c (declared here as the game's other port units do) */
int config_boolean(char const *name);
/* (0.4.3) network_server_manager.c's AE hooks: the server's pregame countdown from the host's time; its cancel, with one
NONE countdown message to every machine. FALSE: no lobby to count (or the game already started) */
boolean ae_lobby_server_countdown_start(struct network_game_server *server, long milliseconds);
boolean ae_lobby_server_countdown_cancel(struct network_game_server *server);

/* network_game_client_get_state: its client's states (network_client_manager.c's enum network_game_client_state,
private to it): searching 0, joining 1, then pregame 2, ingame 3, postgame 4 once joined */
enum { AE_CLIENT_STATE_PREGAME = 2 };

typedef char ae_lobby_players_match[AE_LOBBY_MAXIMUM_PLAYERS == HALO_PORT_MAXIMUM_NETWORK_PLAYERS ? 1 : -1];

/* how long a join searches the LAN before it gives up */
enum { JOIN_SEARCH_MS = 10000, JOIN_VERSION_MS = 3000, JOIN_CONNECT_MS = 15000, JOIN_ADDRESS_MS = 10000 };
/* (JOIN_LISTING: LAN GAMES' search, no game joined until one is picked) */
enum { JOIN_NONE, JOIN_SEARCHING, JOIN_INVITE, JOIN_CONNECTING, JOIN_JOINED, JOIN_FAILED, JOIN_LISTING, JOIN_ADDRESS, JOIN_RESOLVING };

void main_menu_scene_start_for_lobby(void); /* main.c */

static struct
{
	/* AE's glue made the session there is now (hosted or joined it) */
	boolean owns;
	struct ae_screen_class const *pregame_screen;
	void *pregame_data;
	/* a join: its state, since when, its failure; the hosts heard advertising (their network versions, through
	the advertisement hook) */
	int join;
	unsigned long join_since, now;
	struct ae_result join_failure;
	/* (the hosts the search heard: on this machine's network version, on another (the last such)) */
	int heard_compatible, heard_incompatible;
	unsigned short incompatible_version;
	/* the ui scenario to load again under the lobby (after a game) */
	int background_pending;
	/* an invite's code (its host joined by it) */
	char invite[128];
	/* (0.7.0) a join by address: the host's IPv4 address (host order) and port, until its connection is made; the
	join's connection then watched for JOIN_ADDRESS_MS */
	unsigned long address;
	unsigned short address_port;
	int address_join;
	/* (a name being looked up, on a thread of its own, before the client is made) */
	char address_host[128];
} lobby;

static struct ae_result succeeded(void)
{
	struct ae_result result;

	result.ok = 1;
	result.reason[0] = 0;
	return result;
}

/* a failure: its reason (a string, formatted with argument when it has a %s) logged with the call */
static struct ae_result failed(const char *call, int reason, const char *argument)
{
	struct ae_result result;

	result.ok = 0;
	if (argument)
		snprintf(result.reason, sizeof(result.reason), ae_string(reason), argument);
	else
		snprintf(result.reason, sizeof(result.reason), "%s", ae_string(reason));
	platform_log("ae lobby: %s: %s", call, result.reason);
	return result;
}

static const char *kind_name(int kind)
{
	switch (kind)
	{
	case AE_LOBBY_LOCAL: return "LOCAL";
	case AE_LOBBY_ONLINE: return "ONLINE";
	default: return "LAN";
	}
}

/* a failure with no argument, logged */
static struct ae_result failed_result(const char *call, int reason)
{
	return failed(call, reason, NULL);
}

/* a host on another network version: "Host is on version %d, you're on %d"; one below this build's floor must update:
"Host is on version %d and needs to update to %d or newer" */
static struct ae_result failed_version(const char *call, int theirs, int ours)
{
	struct ae_result result;

	result.ok = 0;
	if (theirs < HALO_PORT_NETWORK_VERSION)
		snprintf(result.reason, sizeof(result.reason), ae_string(AE_STR_ERR_HOST_UPDATE), theirs,
			HALO_PORT_NETWORK_VERSION);
	else
		snprintf(result.reason, sizeof(result.reason), ae_string(AE_STR_ERR_VERSION), theirs, ours);
	platform_log("ae lobby: %s: %s", call, result.reason);
	return result;
}

int ae_lobby_owns_session(void)
{
	/* (its server or client gone, by whatever means: not AE's any more) */
	if (lobby.owns && !global_network_game_server_get() && !global_network_game_client_get())
		lobby.owns = FALSE;
	return lobby.owns;
}

void ae_lobby_set_pregame_screen(struct ae_screen_class const *screen_class, void *data)
{
	lobby.pregame_screen = screen_class;
	lobby.pregame_data = data;
}

struct ae_result ae_lobby_host(int kind)
{
	if (game_engine_running() && !main_menu_is_active())
		return failed("host", AE_STR_ERR_IN_GAME, NULL);
	/* Check B's sequence (player_ui_fast_setup_network_server without its widget) */
	ui_widgets_close_all();
	dispose_global_network_game_server();
	dispose_global_network_game_client();
	lobby.owns = FALSE;
	game_connection_set(_game_connection_local);
	main_set_multiplayer_map_name("");
	game_engine_playlist_initialize();
	/* (a variant an earlier session left keeps no PC options here: as fast_setup clears it, player_ui.c) */
	player_ui_clear_multiplayer_variant();
	/* LOCAL: nobody joins; ONLINE: also hosted for the internet, as upstream's Create Game > internet
	(menu_functions.c multiplayer_host: p2p_set_hosting_allowed), but INVITE ONLY in M2 whatever network.host_public
	says (AE lists nothing the player didn't ask for: a PUBLIC choice comes with M4's PRIVACY row) */
	network_game_accept_remote_connections(kind != AE_LOBBY_LOCAL);
	p2p_set_hosting_allowed(kind == AE_LOBBY_ONLINE);
	p2p_set_hosting_public(FALSE);
	if (!create_global_network_game_server() || !create_global_network_game_client())
	{
		dispose_global_network_game_server();
		dispose_global_network_game_client();
		network_game_accept_remote_connections(FALSE);
		p2p_set_hosting_allowed(FALSE);
		game_connection_set(_game_connection_local);
		return failed("host", AE_STR_ERR_HOST, NULL);
	}
	game_engine_playlist_begin();
	game_connection_set(_game_connection_network_server);
	lobby.owns = TRUE;
	lobby.join = JOIN_NONE;
	platform_log(kind == AE_LOBBY_ONLINE ? "ae lobby: hosting (%s, invite only)" : "ae lobby: hosting (%s)",
		kind_name(kind));
	return succeeded();
}

struct ae_result ae_lobby_add_local_player(short controller)
{
	/* (the game takes players once this machine's client has joined: "can't add players to a game until after a game
	is joined") */
	if (!global_network_game_client_get() ||
		network_game_client_get_state(global_network_game_client_get(), NULL) < AE_CLIENT_STATE_PREGAME)
		return failed("add local player", AE_STR_ERR_NO_GAME, NULL);
	/* (player 1's name and colour come from its profile: none is active until CAMPAIGN or SETTINGS takes one) */
	if (controller == 0)
		ae_profile_player1_ensure();
	if (controller < 0 || controller > 3 || !network_game_client_add_player(global_network_game_client_get(), controller))
		return failed("add local player", AE_STR_ERR_LOBBY_FULL, NULL);
	platform_log("ae lobby: local player added (controller %d)", controller + 1);
	return succeeded();
}

/* (0.3.9) split screen in the AE lobby: menu_functions.c's ADD PLAYER state (a lone gamepad off the keyboard's
controller onto its own) */
void pc_menu_lobby_adding(unsigned char adding);

/* this machine's player of the controller in the network game (bots apart), NULL if none */
static struct network_player *local_player_of(short controller)
{
	struct network_game *game = network_game_get_game();
	short machine = network_game_client_get_local_machine_index();
	short index;

	for (index = 0; game && machine != NONE && index < (short)NUMBEROF(game->players); index++)
	{
		struct network_player *player = &game->players[index];

		if (network_player_is_valid(player) && !ae_lan_bots_network_player(player) &&
			(short)player->machine_index == machine && (short)player->controller_index == controller)
			return player;
	}
	return NULL;
}

int ae_lobby_controller_plays(short controller)
{
	return controller >= 0 && controller < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS && local_player_of(controller) != NULL;
}

int ae_lobby_local_players(void)
{
	short controller;
	int count = 0;

	for (controller = 0; controller < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; controller++)
		count += ae_lobby_controller_plays(controller);
	return count;
}

void ae_lobby_adding_player(int adding)
{
	pc_menu_lobby_adding((unsigned char)(adding != 0));
}

struct ae_result ae_lobby_join_controller(short controller)
{
	long profile_index;
	short other;
	int taken = 0;
	struct ae_result result;

	if (controller < 1 || controller >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		return failed("join controller", AE_STR_ERR_LOBBY_FULL, NULL);
	if (ae_lobby_controller_plays(controller))
		return succeeded();
	/* (its profile when it has one no other player here plays with, else a guest's, "Guest 2": the game's default
	profile has no name, colour or look speed: release review B1) */
	profile_index = player_ui_get_active_player_profile_index(controller);
	for (other = 0; other < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; other++)
	{
		taken |= other != controller && profile_index != NONE && ae_lobby_controller_plays(other) &&
			player_ui_get_active_player_profile_index(other) == profile_index;
	}
	if ((profile_index == NONE && !ae_profile_is_guest(controller)) || taken)
	{
		result = ae_profile_guest(controller);
		if (!result.ok)
			return result;
	}
	player_ui_local_player_joined_multiplayer_game(controller);
	result = ae_lobby_add_local_player(controller);
	if (!result.ok)
	{
		player_ui_local_player_left_multiplayer_game(controller);
		return result;
	}
	pc_menu_lobby_adding(FALSE);
	return result;
}

struct ae_result ae_lobby_leave_controller(short controller)
{
	struct network_player *player = local_player_of(controller);

	/* (the machine's last player leaves the lobby itself: ae_lobby_leave) */
	if (!player || controller < 1 || ae_lobby_local_players() < 2)
		return failed("leave controller", AE_STR_ERR_NO_GAME, NULL);
	if (!network_game_client_request_remove_player(global_network_game_client_get(), player))
		return failed("leave controller", AE_STR_ERR_NO_GAME, NULL);
	player_ui_local_player_left_multiplayer_game(controller);
	platform_log("ae lobby: local player left (controller %d)", controller + 1);
	return succeeded();
}

int ae_lobby_change_team(short controller)
{
	struct network_game_client *client = global_network_game_client_get();
	struct network_game *game = network_game_get_game();
	struct network_player *current = local_player_of(controller);
	struct network_player changed;

	if (!client || network_game_client_get_state(client, NULL) != AE_CLIENT_STATE_PREGAME || !game ||
		!game->variant.universal_variant.teams || !current)
	{
		return 0;
	}
	/* The classic PC/Xbox lobby's multiplayer_game_swap_teams: only this controller's player, sent to the host as a
	player-settings request. The host validates its machine and team; its pregame loop applies auto-balance. */
	changed = *current;
	changed.team_index = changed.team_index == _team_red ? _team_blue : _team_red;
	if (!network_game_client_update_local_player_data(client, &changed))
		return 0;
	platform_log("ae lobby: player %d asks for the %s team", controller + 1,
		changed.team_index == _team_blue ? "blue" : "red");
	return 1;
}

int ae_lobby_change_bot_team(short machine, short controller)
{
	if (!global_network_game_server_get() || !ae_lan_bots_flip_team(machine, controller))
		return 0;
	platform_log("ae lobby: bot at machine %d, controller %d moved to the other team", (int)machine, controller + 1);
	return 1;
}

/* a path's last part ("levels\test\bloodgulch\bloodgulch" -> "bloodgulch") */
static const char *file_name(const char *path)
{
	const char *slash = strrchr(path, '\\');

	return slash ? slash + 1 : path;
}

struct ae_result ae_lobby_roster(struct ae_lobby_roster *roster)
{
	struct network_game *game = network_game_get_game();
	short index, local_machine = network_game_client_get_local_machine_index();

	if (!roster)
		return failed("roster", AE_STR_ERR_NO_GAME, NULL);
	memset(roster, 0, sizeof(*roster));
	if (!game || (!global_network_game_client_get() && !global_network_game_server_get()))
		return failed("roster", AE_STR_ERR_NO_GAME, NULL);
	/* (every slot: a player who left leaves a gap, which a LAN bot giving way to a person makes often) */
	for (index = 0; index < (short)NUMBEROF(game->players) && roster->count < AE_LOBBY_MAXIMUM_PLAYERS; index++)
	{
		struct network_player const *source = &game->players[index];
		struct ae_lobby_player *player;

		if (!network_player_is_valid((struct network_player *)source))
			continue;
		player = &roster->players[roster->count++];

		/* (the game's names are 16-bit: -fshort-wchar) */
		ae_lobby_utf16_to_utf8(player->name, sizeof(player->name), (const unsigned short *)source->name,
			NUMBEROF(source->name));
		player->team = source->team_index;
		player->machine = source->machine_index;
		player->controller = source->controller_index;
		{
			real_argb_color argb = player_profile_get_argb_color(source->primary_color_index);

			player->color = ((unsigned int)(argb.rgb.red * 255.0f + 0.5f) << 24) |
				((unsigned int)(argb.rgb.green * 255.0f + 0.5f) << 16) | ((unsigned int)(argb.rgb.blue * 255.0f + 0.5f) << 8) | 0xFFu;
		}
		/* (this machine's players: the client's machine index; the host's machine: this one when it hosts, else
		the first machine, the server's own (an assumption: a joining client has no other word for it)) */
		player->local = local_machine != NONE && source->machine_index == local_machine;
		player->host = global_network_game_server_get() ? player->local : source->machine_index == 0;
		/* (a bot is no one's person: not this machine's, not the host. A network game's on its host: its own word
		(ae_lan_bots.c); a split screen game's: a machine of the bots' own (bots.c); a joiner is told nothing of
		them: the host's virtual machine "Bots", or "BOT " before the name, as the host names them) */
		player->bot = ae_lan_bots_network_player(source);
		if (!player->bot && network_game_is_splitscreen_local())
			player->bot = bots_machine_is_bot(source->machine_index);
		else if (!player->bot && !global_network_game_server_get())
		{
			char machine[48];

			machine[0] = 0;
			if (source->machine_index >= 0 && source->machine_index < (short)NUMBEROF(game->machines) &&
				game->machines[source->machine_index].machine_index == source->machine_index)
				ae_lobby_utf16_to_utf8(machine, sizeof(machine),
					(const unsigned short *)game->machines[source->machine_index].name,
					NUMBEROF(game->machines[source->machine_index].name));
			player->bot = ae_lobby_bot_by_names(machine, player->name);
		}
		if (player->bot)
			player->local = player->host = 0;
	}
	roster->machines = game->machine_count;
	roster->joined = global_network_game_client_get() &&
		network_game_client_get_state(global_network_game_client_get(), NULL) >= AE_CLIENT_STATE_PREGAME;
	snprintf(roster->map, sizeof(roster->map), "%s", file_name(game->map.name));
	ae_lobby_utf16_to_utf8(roster->gametype, sizeof(roster->gametype),
		(const unsigned short *)game->variant.human_readable_game_description,
		NUMBEROF(game->variant.human_readable_game_description));
	roster->team_game = game->variant.universal_variant.teams != 0;
	roster->maximum_players = game->maximum_players;
	roster->postgame = game_engine_showing_postgame();
	roster->in_game = game_engine_running() && !main_menu_is_active() && !roster->postgame;
	ae_lobby_roster_order(roster);
	return succeeded();
}

struct ae_result ae_lobby_set_map(const char *map_file)
{
	char path[160];

	if (!global_network_game_server_get())
		return failed("map", AE_STR_ERR_NOT_HOSTING, NULL);
	if (!map_file || !*map_file || strchr(map_file, '\\') || strlen(map_file) > 48)
		return failed("map", AE_STR_ERR_MAP, map_file ? map_file : "");
	snprintf(path, sizeof(path), "levels\\test\\%s\\%s", map_file, map_file);
	network_game_server_change_map_name(global_network_game_server_get(), path);
	/* the game's own "current stage" too (as the PC menus' map pick does): after every round the server sets the next
	game up from it, so without it the lobby came back on the default map (carousel) */
	main_set_multiplayer_map_name(path);
	game_engine_override_map_name(path);
	platform_log("ae lobby: map %s", map_file);
	return succeeded();
}

struct ae_result ae_lobby_set_gametype(const char *stored_name)
{
	struct game_variant variant;

	if (!global_network_game_server_get())
		return failed("gametype", AE_STR_ERR_NOT_HOSTING, NULL);
	if (!stored_name || !*stored_name)
		return failed("gametype", AE_STR_ERR_GAMETYPE, "");
	/* (a name game_engine_get_variant_by_name doesn't know gives a zeroed variant: no game engine) */
	game_engine_get_variant_by_name(&variant, stored_name);
	if (variant.game_engine_index <= 0)
		return failed("gametype", AE_STR_ERR_GAMETYPE, stored_name);
	player_ui_set_game_variant(&variant);
	network_game_server_change_game_variant(global_network_game_server_get(), &variant);
	/* (the current stage's gametype, for the next round's lobby: it came back with no gametype) */
	game_engine_override_game_variant(&variant);
	platform_log("ae lobby: gametype %s", stored_name);
	return succeeded();
}

struct ae_result ae_lobby_start(void)
{
	if (!global_network_game_server_get())
		return failed("start", AE_STR_ERR_NOT_HOSTING, NULL);
	if (game_engine_running() && !main_menu_is_active())
		return failed("start", AE_STR_ERR_IN_GAME, NULL);
	/* back from a game the take-over held the host's countdown (ae_lobby_take_pregame_ui), and the server ignores
	the start while it is held: this is the start, so it is let go */
	network_game_server_pause_countdown(global_network_game_server_get(), FALSE);
	network_game_client_request_immediate_start();
	platform_log("ae lobby: starting");
	return succeeded();
}

struct ae_result ae_lobby_start_countdown(long milliseconds)
{
	struct network_game_server *server = global_network_game_server_get();
	struct ae_result result;

	if (milliseconds <= 0)
		return ae_lobby_start();
	if (!server)
		return failed("start", AE_STR_ERR_NOT_HOSTING, NULL);
	if (game_engine_running() && !main_menu_is_active())
		return failed("start", AE_STR_ERR_IN_GAME, NULL);
	/* (the server's own pregame countdown, un-held: the stock lobby's, sent to every machine as the stock one is; at 0
	the game starts once every machine has the map) */
	if (!ae_lobby_server_countdown_start(server, milliseconds))
	{
		result.ok = 0;
		snprintf(result.reason, sizeof(result.reason), "The lobby can't start yet");
		platform_log("ae lobby: start: %s (the server's countdown did not begin)", result.reason);
		return result;
	}
	platform_log("ae lobby: starting (countdown %ld ms)", milliseconds);
	return succeeded();
}

struct ae_result ae_lobby_cancel_start(void)
{
	struct network_game_server *server = global_network_game_server_get();
	struct ae_result result;

	if (!server)
		return failed("cancel start", AE_STR_ERR_NOT_HOSTING, NULL);
	/* (once the server sent the start, every machine is loading the game: too late) */
	if (!ae_lobby_server_countdown_cancel(server))
	{
		result.ok = 0;
		snprintf(result.reason, sizeof(result.reason), "The game already started");
		platform_log("ae lobby: cancel start: %s", result.reason);
		return result;
	}
	platform_log("ae lobby: start cancelled (the countdown held, NONE sent)");
	return succeeded();
}

short ae_lobby_seconds_to_start(void)
{
	struct network_game_client *client = global_network_game_client_get();

	if (!client)
		return AE_COUNTDOWN_SECONDS_NONE;
	/* (the client clears its seconds as the game starts: past its pregame, the game is under way) */
	if (network_game_client_get_state(client, NULL) > AE_CLIENT_STATE_PREGAME)
		return AE_COUNTDOWN_SECONDS_STARTED;
	if (network_game_client_get_state(client, NULL) < AE_CLIENT_STATE_PREGAME)
		return AE_COUNTDOWN_SECONDS_NONE;
	return network_game_client_get_seconds_to_game_start(client) < 0 ? AE_COUNTDOWN_SECONDS_NONE :
		network_game_client_get_seconds_to_game_start(client);
}

int ae_lobby_pregame_countdown(void)
{
	struct network_game *game = network_game_get_game();

	return game && TEST_FLAG(game->variant.universal_variant.flags, _game_variant_pregame_countdown_bit);
}

struct ae_result ae_lobby_end_game(void)
{
	if (!global_network_game_server_get())
		return failed("end game", AE_STR_ERR_NOT_HOSTING, NULL);
	if (!game_engine_running() || main_menu_is_active() || game_engine_showing_postgame())
		return failed("end game", AE_STR_ERR_NO_GAME, NULL);
	game_engine_end_game();
	platform_log("ae lobby: game ended");
	return succeeded();
}

struct ae_result ae_lobby_back_to_pregame(void)
{
	if (!global_network_game_server_get())
		return failed("back to pregame", AE_STR_ERR_NOT_HOSTING, NULL);
	if (!game_engine_showing_postgame())
		return failed("back to pregame", AE_STR_ERR_IN_GAME, NULL);
	network_game_server_reset_to_pregame(global_network_game_server_get());
	platform_log("ae lobby: back to pregame");
	return succeeded();
}

/* ---------- joining and leaving (Task 13) */

/* a join that failed: the client gone, the connection local, nothing AE's; its reason kept */
static void join_failed(struct ae_result result)
{
	dispose_global_network_game_client();
	game_connection_set(_game_connection_local);
	lobby.owns = FALSE;
	lobby.join = JOIN_FAILED;
	lobby.join_failure = result;
}

/* the client for a join: this machine searching, AE's session from now (network_test.c's join path) */
static boolean join_client(int kind)
{
	lobby.address_join = FALSE;
	ui_widgets_close_all();
	dispose_global_network_game_client();
	dispose_global_network_game_server();
	network_game_accept_remote_connections(FALSE);
	p2p_set_hosting_allowed(FALSE);
	p2p_set_hosting_public(FALSE);
	lobby.join_failure.ok = 0;
	lobby.join_failure.reason[0] = 0;
	lobby.heard_compatible = lobby.heard_incompatible = FALSE;
	if (!create_global_network_game_client())
	{
		join_failed(failed_result("join", AE_STR_ERR_NO_GAME));
		return FALSE;
	}
	game_connection_set(_game_connection_network_client);
	lobby.owns = TRUE;
	lobby.join = kind;
	/* (its clock its own: a join may start before the first update) */
	lobby.join_since = system_milliseconds();
	return TRUE;
}

struct ae_result ae_lobby_join_first_available(void)
{
	if (!main_menu_is_active())
		return failed("join", AE_STR_ERR_IN_GAME, NULL);
	if (!join_client(JOIN_SEARCHING))
		return lobby.join_failure;
	platform_log("ae lobby: searching the LAN");
	return succeeded();
}

struct ae_result ae_lobby_lan_search(void)
{
	if (!main_menu_is_active())
		return failed("lan search", AE_STR_ERR_IN_GAME, NULL);
	if (!join_client(JOIN_LISTING))
		return lobby.join_failure;
	platform_log("ae lobby: listing the LAN's games");
	return succeeded();
}

int ae_lobby_lan_searching(void)
{
	return lobby.join == JOIN_LISTING && global_network_game_client_get() != NULL;
}

short ae_lobby_lan_games(struct ae_lan_game *games, short maximum)
{
	struct network_available_game game;
	short count = 0;
	long index;

	if (!ae_lobby_lan_searching())
		return 0;
	for (index = 0; index < NETWORK_GAME_CLIENT_AVAILABLE_GAMES && count < maximum; index++)
	{
		struct ae_lan_game *out = &games[count];

		if (!network_game_client_available_game_read(index, &game) || !game.open)
			continue;
		memset(out, 0, sizeof(*out));
		out->index = (short)index;
		memcpy(out->nonce, game.nonce, sizeof(out->nonce));
		/* (the game's names are 16-bit: -fshort-wchar) */
		ae_lobby_utf16_to_utf8(out->name, sizeof(out->name), (const unsigned short *)game.name, NUMBEROF(game.name));
		snprintf(out->map, sizeof(out->map), "%s", game.map);
		out->engine = game.engine;
		out->players = game.players;
		out->maximum_players = game.maximum_players;
		out->teams = game.teams;
		out->in_progress = game.in_progress;
		out->join_state = game.join_state;
		out->version = game.version;
		out->kind = game.kind;
		count++;
	}
	return count;
}

struct ae_result ae_lobby_lan_join(short index, const unsigned char nonce[8])
{
	struct network_available_game game;

	if (!nonce || !ae_lobby_lan_searching() || !network_game_client_available_game_read(index, &game) || !game.open ||
		memcmp(game.nonce, nonce, sizeof(game.nonce)))
		return failed("lan join", AE_STR_ERR_NO_GAME, NULL);
	if (game.join_state != HALO_PORT_JOIN_OK)
		return failed_version("lan join", game.version, delta_legacy_announce());
	if (!network_game_client_join_available_game(index))
		return failed("lan join", AE_STR_ERR_NO_GAME, NULL);
	lobby.join = JOIN_CONNECTING;
	lobby.join_since = system_milliseconds();
	platform_log("ae lobby: joining a LAN game (%d)", (int)index);
	return succeeded();
}

/* the invite's text as pasted: the whitespace and line ends around it dropped (menu_functions.c
direct_link_from_clipboard) */
static void trimmed(const char *text, char *out, size_t size)
{
	size_t length;

	while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
		text++;
	snprintf(out, size, "%s", text);
	for (length = strlen(out); length && (out[length - 1] == ' ' || out[length - 1] == '\t' ||
		out[length - 1] == '\r' || out[length - 1] == '\n'); length--)
		out[length - 1] = 0;
}

struct ae_result ae_lobby_join_address(const char *address)
{
	char text[256];
#ifdef HALO_GAME_BROWSER
	char code[sizeof(lobby.invite)];
#endif

	if (!main_menu_is_active())
		return failed("join", AE_STR_ERR_IN_GAME, NULL);
	trimmed(address ? address : "", text, sizeof(text));
	if (!text[0])
		return failed("join", AE_STR_ERR_NOT_INVITE, NULL);
#ifndef HALO_GAME_BROWSER
	/* (a build without internet play: the dedicated server) */
	return failed("join", AE_STR_ERR_INTERNET_OFF, NULL);
#else
	if (!config_boolean("network.online"))
		return failed("join", AE_STR_ERR_INTERNET_OFF, NULL);
	/* (the code from this text, never p2p_joined_invite's: that is the last invite joined, an older one while
	this one waits for internet play to start; as p2p.c parse_invite reads it: ae_invite_rules.c) */
	if (!ae_invite_code(text, code, sizeof(code)))
		return failed("join", AE_STR_ERR_NOT_INVITE, NULL);
	if (!join_client(JOIN_INVITE))
		return lobby.join_failure;
	/* as upstream's Server Browser (browser_screen.c join, wait_for_host): the invite reached (p2p_join_invite),
	then its own host's game joined once it is advertised (network_game_client_join_invite_host), never another */
	if (!p2p_join_invite(text))
	{
		join_failed(failed_result("join", AE_STR_ERR_NOT_INVITE));
		return lobby.join_failure;
	}
	snprintf(lobby.invite, sizeof(lobby.invite), "%s", code);
	platform_log("ae lobby: reaching the invite's host");
	return succeeded();
#endif
}

unsigned short ae_lobby_host_port(void)
{
	return NETWORK_GAME_SERVER_PORT;
}

/* the address looked up: the client made, the join under way (the resolved address in network byte order) */
static void join_host_found(unsigned long network_order)
{
	/* (the game's addresses are in host order: 127.0.0.1 is 0x7F000001) */
	unsigned long address = ((network_order & 0xFF) << 24) | ((network_order & 0xFF00) << 8) |
		((network_order >> 8) & 0xFF00) | ((network_order >> 24) & 0xFF);
	unsigned short port = lobby.address_port;

	if (!join_client(JOIN_ADDRESS))
		return;
	lobby.address = address;
	lobby.address_port = port;
	lobby.address_join = TRUE;
	platform_log("ae lobby: reaching %lu.%lu.%lu.%lu:%u", (address >> 24) & 0xFF, (address >> 16) & 0xFF,
		(address >> 8) & 0xFF, address & 0xFF, (unsigned)port);
}

struct ae_result ae_lobby_join_host(const char *host, unsigned short port)
{
	if (!main_menu_is_active())
		return failed("join", AE_STR_ERR_IN_GAME, NULL);
	if (!host || !host[0] || !port || strlen(host) >= sizeof(lobby.address_host))
		return failed("join", AE_STR_ERR_NO_GAME, NULL);
	/* (the name looked up on a thread: the system's resolver may wait; the menus go on, and B cancels) */
	if (!ae_resolve_start(host))
		return failed("join", AE_STR_ERR_NO_ANSWER, NULL);
	memcpy(lobby.address_host, host, strlen(host) + 1);
	lobby.address_port = port;
	lobby.join_failure.ok = 0;
	lobby.join_failure.reason[0] = 0;
	lobby.join = JOIN_RESOLVING;
	lobby.join_since = system_milliseconds();
	platform_log("ae lobby: looking up '%s'", host);
	return succeeded();
}

int ae_lobby_join_looking_up(void)
{
	return lobby.join == JOIN_RESOLVING;
}

int ae_lobby_join_state(struct ae_result *failure)
{
	if (failure)
		*failure = lobby.join_failure;
	switch (lobby.join)
	{
	case JOIN_JOINED: return 1;
	case JOIN_FAILED: return -1;
	case JOIN_SEARCHING: case JOIN_INVITE: case JOIN_ADDRESS: case JOIN_RESOLVING: case JOIN_CONNECTING: return 0;
	default: return -2;
	}
}

struct ae_result ae_lobby_leave(void)
{
	boolean hosting = global_network_game_server_get() != NULL;

	/* (a name still being looked up: dropped, its answer never kept) */
	if (lobby.join == JOIN_RESOLVING)
	{
		ae_resolve_cancel();
		lobby.join = JOIN_NONE;
		platform_log("ae lobby: name lookup left");
		return succeeded();
	}
	if (!global_network_game_server_get() && !global_network_game_client_get())
		return failed("leave", AE_STR_ERR_NO_GAME, NULL);
	if (!main_menu_is_active())
		return failed("leave", AE_STR_ERR_IN_GAME, NULL);
	/* (the client, then the server: a host closes its session, a client leaves it; the player UI's joins and
	variant cleared as upstream's teardown does, ui_widget_event_handler_functions.c main_menu_initialize; no upstream
	menu reopened) */
	dispose_global_network_game_client();
	dispose_global_network_game_server();
	player_ui_clear_multiplayer_joins();
	player_ui_clear_multiplayer_variant();
	network_game_accept_remote_connections(FALSE);
	p2p_set_hosting_allowed(FALSE);
	p2p_set_hosting_public(FALSE);
	game_connection_set(_game_connection_local);
	/* (the client's disposal took the update queues, which the menus' running clock needs: the engine's own no-client
	path makes them again) */
	main_menu_ensure_player_queues_exist();
	lobby.owns = FALSE;
	lobby.join = JOIN_NONE;
	platform_log(hosting ? "ae lobby: closed" : "ae lobby: left");
	return succeeded();
}

void ae_lobby_advertised(unsigned short version, unsigned char flags)
{
	/* (the join's own test, halo_port_advertised_joinable: the network version floor, the legal range, the distributed
	netcode; an incompatible host is kept until the search ends, whichever host advertised last) */
	if (halo_port_advertised_joinable(version, flags, (unsigned int)delta_legacy_minimum(),
		(unsigned int)delta_legacy_maximum()))
		lobby.heard_compatible = TRUE;
	else
	{
		lobby.heard_incompatible = TRUE;
		lobby.incompatible_version = version;
	}
}

/* the search gives up: why, and after how long */
static void search_failed(unsigned long now, int reason)
{
	platform_log("ae lobby: join gave up after %lu s", (now - lobby.join_since) / 1000);
	if (reason == AE_STR_ERR_VERSION)
		join_failed(failed_version("join", lobby.incompatible_version, delta_legacy_announce()));
	else
		join_failed(failed_result("join", reason));
}

void ae_lobby_update(unsigned long now)
{
	struct network_game_client *client = global_network_game_client_get();

	/* (a name looked up for JOIN BY ADDRESS: found, the join goes on; not found, it failed) */
	if (lobby.join == JOIN_RESOLVING)
	{
		unsigned long found = 0;
		int answer = ae_resolve_poll(&found);

		if (answer < 0)
			lobby.join = JOIN_NONE;
		else if (answer > 0)
		{
			if (found)
				join_host_found(found);
			else
			{
				platform_log("ae lobby: join by address: '%s' not found", lobby.address_host);
				lobby.join = JOIN_NONE;
				join_failed(failed_result("join", AE_STR_ERR_NO_ANSWER));
			}
			client = global_network_game_client_get();
		}
	}
	lobby.now = now;
	if (lobby.background_pending)
	{
		lobby.background_pending = FALSE;
		main_menu_scene_start_for_lobby();
		platform_log("ae lobby: the menus' background scene started under the lobby");
	}
	/* (the session gone, by whatever means: not AE's any more; checked each frame) */
	ae_lobby_owns_session();
	if (lobby.join == JOIN_SEARCHING)
	{
		if (!client)
			join_failed(failed_result("join", AE_STR_ERR_NO_GAME));
		else if (network_game_client_join_first_available_game())
		{
			lobby.join = JOIN_CONNECTING;
			lobby.join_since = now;
			platform_log("ae lobby: joining");
		}
		/* (only hosts on another version heard, for a few seconds: that, without waiting the whole search) */
		else if (lobby.heard_incompatible && !lobby.heard_compatible && now - lobby.join_since >= JOIN_VERSION_MS)
			search_failed(now, AE_STR_ERR_VERSION);
		else if (now - lobby.join_since >= JOIN_SEARCH_MS)
			search_failed(now, lobby.heard_compatible ? AE_STR_ERR_LOBBY_FULL : lobby.heard_incompatible ?
				AE_STR_ERR_VERSION : AE_STR_ERR_NO_GAME);
	}
	else if (lobby.join == JOIN_INVITE)
	{
#ifdef HALO_GAME_BROWSER
		long joined = client ? network_game_client_join_invite_host(lobby.invite) : 0;
#else
		long joined = 0;
#endif

		/* (the client gone: nothing to join with, as no game found) */
		if (!client)
			join_failed(failed_result("join", AE_STR_ERR_NO_GAME));
		else if (joined > 0)
		{
			lobby.join = JOIN_CONNECTING;
			lobby.join_since = now;
			platform_log("ae lobby: joining");
		}
		else if (joined < 0)
			join_failed(failed_result("join", AE_STR_ERR_INVITE_VERSION));
		else if (now - lobby.join_since >= JOIN_CONNECT_MS)
			join_failed(failed_result("join", AE_STR_ERR_NO_ANSWER));
	}
	else if (lobby.join == JOIN_ADDRESS)
	{
		/* (the host named by its address, joined as the host's own game is: no advertisement, the connection made
		to the address once this machine's client searches) */
		if (!client)
			join_failed(failed_result("join", AE_STR_ERR_NO_GAME));
		else
		{
			long started = ae_network_game_client_join_address(lobby.address, lobby.address_port);

			if (started > 0)
			{
				lobby.join = JOIN_CONNECTING;
				lobby.join_since = now;
				platform_log("ae lobby: joining by address");
			}
			else if (started < 0)
				join_failed(failed_result("join", AE_STR_ERR_NO_ANSWER));
			else if (now - lobby.join_since >= JOIN_ADDRESS_MS)
				join_failed(failed_result("join", AE_STR_ERR_NO_ANSWER));
		}
	}
	else if (lobby.join == JOIN_CONNECTING)
	{
		if (client && network_game_client_get_state(client, NULL) >= AE_CLIENT_STATE_PREGAME)
		{
			lobby.join = JOIN_JOINED;
			platform_log("ae lobby: joined");
		}
		/* (a join by address: nobody answered at it in 10 s) */
		else if (lobby.address_join && client && !network_game_client_get_error(client) &&
			now - lobby.join_since >= JOIN_ADDRESS_MS)
			join_failed(failed_result("join", AE_STR_ERR_NO_ANSWER));
		/* (refused: the host's lobby full, or closed to this machine) */
		else if (!client || network_game_client_get_error(client) || now - lobby.join_since >= JOIN_CONNECT_MS)
			join_failed(failed_result("join", AE_STR_ERR_LOBBY_FULL));
	}
}

boolean ae_lobby_take_pregame_ui(
	void)
{
	if (!ae_menus_active())
		return FALSE;
	if (!ae_lobby_owns_session())
	{
		platform_log("ae lobby: pregame UI left to the game (not AE's session)");
		return FALSE;
	}
	/* (no pregame screen set, or one that can't be pushed: the game loads its own, so the player is never left with
	no menu at all; M2 final review m1) */
	if (!lobby.pregame_screen)
	{
		platform_log("ae lobby: pregame UI left to the game (no pregame screen set)");
		return FALSE;
	}
	if (!ae_ui_holds(lobby.pregame_data) && !ae_ui_push(lobby.pregame_screen, AE_OWNER_ANY, lobby.pregame_data))
	{
		platform_log("ae lobby: pregame UI left to the game (the screen could not be pushed)");
		return FALSE;
	}
	/* the menus' background is the ui scenario's scene (the ring), whose clock the round stopped:
	started again, next frame, outside the network's own update (ae_lobby_update) */
	lobby.background_pending = TRUE;
	/* what upstream's branch does besides loading its widget: the host's countdown held (ui_widget.c
	network_game_reset_to_pregame_ui); AE's pregame screen is up */
	if (global_network_game_server_get())
		network_game_server_pause_countdown(global_network_game_server_get(), TRUE);
	platform_log("ae lobby: took the pregame UI (the game's SELECT MAP not loaded)");
	return TRUE;
}
