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
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/player_ui.h"
#include "interface/ui_widget.h"
#include "main/main.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager.h"
#include "../src/p2p.h"
#include "ae_glue_lobby.h"
#include "ae_hooks.h"
#include "ae_strings.h"
#include "ae_ui.h"

void platform_log(char const *format, ...);
/* port_config.c (declared here as the game's other port units do) */
int config_boolean(char const *name);

/* network_game_client_get_state: its client's states (network_client_manager.c's enum network_game_client_state,
private to it): searching 0, joining 1, then pregame 2, ingame 3, postgame 4 once joined */
enum { AE_CLIENT_STATE_PREGAME = 2 };

typedef char ae_lobby_players_match[AE_LOBBY_MAXIMUM_PLAYERS == HALO_PORT_MAXIMUM_NETWORK_PLAYERS ? 1 : -1];

static struct
{
	/* AE's glue made the session there is now (hosted it; Task 13: joined it) */
	boolean owns;
	struct ae_screen_class const *pregame_screen;
	void *pregame_data;
} lobby;

static struct ae_result succeeded(void)
{
	struct ae_result result;

	result.ok = 1;
	result.reason[0] = 0;
	return result;
}

/* a failure: its reason (a string, formatted with argument when it has a %s or %d) logged with the call */
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
	/* LOCAL: nobody joins; ONLINE: also hosted for the internet, as upstream's Create Game > internet
	(menu_functions.c multiplayer_host: p2p_set_hosting_allowed / _public; network.list_hosted_games is p2p's) */
	network_game_accept_remote_connections(kind != AE_LOBBY_LOCAL);
	p2p_set_hosting_allowed(kind == AE_LOBBY_ONLINE);
	p2p_set_hosting_public(kind == AE_LOBBY_ONLINE && config_boolean("network.host_public"));
	if (!create_global_network_game_server() || !create_global_network_game_client())
	{
		dispose_global_network_game_server();
		dispose_global_network_game_client();
		network_game_accept_remote_connections(FALSE);
		p2p_set_hosting_allowed(FALSE);
		return failed("host", AE_STR_ERR_HOST, NULL);
	}
	game_engine_playlist_begin();
	game_connection_set(_game_connection_network_server);
	lobby.owns = TRUE;
	platform_log("ae lobby: hosting (%s)", kind_name(kind));
	return succeeded();
}

struct ae_result ae_lobby_add_local_player(short controller)
{
	char number[16];

	/* (the game takes players once this machine's client has joined: "can't add players to a game until after a game
	is joined") */
	if (!global_network_game_client_get() ||
		network_game_client_get_state(global_network_game_client_get(), NULL) < AE_CLIENT_STATE_PREGAME)
		return failed("add local player", AE_STR_ERR_NO_GAME, NULL);
	snprintf(number, sizeof(number), "%d", controller + 1);
	if (controller < 0 || controller > 3 || !network_game_client_add_player(global_network_game_client_get(), controller))
		return failed("add local player", AE_STR_ERR_LOBBY_FULL, NULL);
	platform_log("ae lobby: local player added (controller %d)", controller + 1);
	return succeeded();
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
	short index;

	if (!roster)
		return failed("roster", AE_STR_ERR_NO_GAME, NULL);
	memset(roster, 0, sizeof(*roster));
	if (!game || (!global_network_game_client_get() && !global_network_game_server_get()))
		return failed("roster", AE_STR_ERR_NO_GAME, NULL);
	for (index = 0; index < game->player_count && index < AE_LOBBY_MAXIMUM_PLAYERS; index++)
	{
		struct network_player const *source = &game->players[index];
		struct ae_lobby_player *player = &roster->players[roster->count++];

		/* (the game's names are 16-bit: -fshort-wchar) */
		ae_lobby_utf16_to_utf8(player->name, sizeof(player->name), (const unsigned short *)source->name,
			NUMBEROF(source->name));
		player->team = source->team_index;
		player->machine = source->machine_index;
		player->controller = source->controller_index;
		/* (the host's machine is the first: the server's own) */
		player->host = source->machine_index == 0;
		player->local = global_network_game_server_get() ? source->machine_index == 0 : 0;
	}
	roster->machines = game->machine_count;
	roster->joined = global_network_game_client_get() &&
		network_game_client_get_state(global_network_game_client_get(), NULL) >= AE_CLIENT_STATE_PREGAME;
	snprintf(roster->map, sizeof(roster->map), "%s", file_name(game->map.name));
	ae_lobby_utf16_to_utf8(roster->gametype, sizeof(roster->gametype),
		(const unsigned short *)game->variant.human_readable_game_description,
		NUMBEROF(game->variant.human_readable_game_description));
	roster->team_game = game->variant.universal_variant.teams != 0;
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
	platform_log("ae lobby: gametype %s", stored_name);
	return succeeded();
}

struct ae_result ae_lobby_start(void)
{
	if (!global_network_game_server_get())
		return failed("start", AE_STR_ERR_NOT_HOSTING, NULL);
	if (game_engine_running() && !main_menu_is_active())
		return failed("start", AE_STR_ERR_IN_GAME, NULL);
	network_game_client_request_immediate_start();
	platform_log("ae lobby: starting");
	return succeeded();
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
	/* what upstream's branch does besides loading its widget: the host's countdown held (ui_widget.c
	network_game_reset_to_pregame_ui); then AE's pregame screen, once */
	if (global_network_game_server_get())
		network_game_server_pause_countdown(global_network_game_server_get(), TRUE);
	if (lobby.pregame_screen && !ae_ui_holds(lobby.pregame_data))
		ae_ui_push(lobby.pregame_screen, AE_OWNER_ANY, lobby.pregame_data);
	platform_log("ae lobby: took the pregame UI (the game's SELECT MAP not loaded)");
	return TRUE;
}
