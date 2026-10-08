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
#include "interface/player_ui.h"
#include "interface/ui_widget.h"
#include "main/main.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager.h"
#include "../src/p2p.h"
#include "halo_port_limits.h"
#include "ae_glue_lobby.h"
#include "ae_hooks.h"
#include "ae_strings.h"
#include "ae_ui.h"

void platform_log(char const *format, ...);
#ifdef HALO_GAME_BROWSER
/* network_client_manager.c (the Server Browser's: browser_screen.c declares it the same way): 1 joining the invite's
host, 0 not advertised yet, -1 it can't be joined (another version: the player is told) */
long network_game_client_join_invite_host(char const *invite);
#endif
/* port_config.c (declared here as the game's other port units do) */
int config_boolean(char const *name);

/* network_game_client_get_state: its client's states (network_client_manager.c's enum network_game_client_state,
private to it): searching 0, joining 1, then pregame 2, ingame 3, postgame 4 once joined */
enum { AE_CLIENT_STATE_PREGAME = 2 };

typedef char ae_lobby_players_match[AE_LOBBY_MAXIMUM_PLAYERS == HALO_PORT_MAXIMUM_NETWORK_PLAYERS ? 1 : -1];

/* how long a join searches the LAN before it gives up */
enum { JOIN_SEARCH_MS = 10000, JOIN_VERSION_MS = 3000, JOIN_CONNECT_MS = 15000 };
enum { JOIN_NONE, JOIN_SEARCHING, JOIN_INVITE, JOIN_CONNECTING, JOIN_JOINED, JOIN_FAILED };

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
	/* an invite's code (its host joined by it) */
	char invite[128];
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

/* a host on another network version: "Host is on version %d, you're on %d" */
static struct ae_result failed_version(const char *call, int theirs, int ours)
{
	struct ae_result result;

	result.ok = 0;
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
	short index, local_machine = network_game_client_get_local_machine_index();

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
		/* (this machine's players: the client's machine index; the host's machine: this one when it hosts, else
		the first machine, the server's own (an assumption: a joining client has no other word for it)) */
		player->local = local_machine != NONE && source->machine_index == local_machine;
		player->host = global_network_game_server_get() ? player->local : source->machine_index == 0;
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

#ifdef HALO_GAME_BROWSER
/* the invite's code in the text itself (hexadecimal, lower case, as network_game_client_join_invite_host reads
it): after "halo://join/" (any case) when the text is a link, else the text's digits alone; as p2p.c parse_invite
reads it. FALSE if the text holds no invite of this version (a key link, an older invite, anything else) */
static int invite_code(const char *text, char *code, size_t size)
{
	static const char prefix[] = "halo://join/";
	const char *start = text;
	const char *search;
	size_t digits;

	for (search = text; *search; search++)
	{
		size_t length;

		for (length = 0; prefix[length] && search[length] && (search[length] | 0x20) == prefix[length]; length++)
			;
		if (!prefix[length])
		{
			start = search + length;
			break;
		}
	}
	for (digits = 0; (start[digits] >= '0' && start[digits] <= '9') || ((start[digits] | 0x20) >= 'a' &&
		(start[digits] | 0x20) <= 'f'); digits++)
		;
	/* (the host's key hash and the token: 16 bytes each, p2p_internal.h; a bare code is its digits alone) */
	if (digits != 64 || digits >= size || (start == text && start[digits]))
		return 0;
	for (search = start; search < start + digits; search++)
		*code++ = (char)(*search >= 'A' && *search <= 'F' ? *search | 0x20 : *search);
	*code = 0;
	return 1;
}
#endif

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
	this one waits for internet play to start) */
	if (!invite_code(text, code, sizeof(code)))
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

int ae_lobby_join_state(struct ae_result *failure)
{
	if (failure)
		*failure = lobby.join_failure;
	switch (lobby.join)
	{
	case JOIN_JOINED: return 1;
	case JOIN_FAILED: return -1;
	case JOIN_SEARCHING: case JOIN_INVITE: case JOIN_CONNECTING: return 0;
	default: return -2;
	}
}

struct ae_result ae_lobby_leave(void)
{
	boolean hosting = global_network_game_server_get() != NULL;

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
	lobby.owns = FALSE;
	lobby.join = JOIN_NONE;
	platform_log(hosting ? "ae lobby: closed" : "ae lobby: left");
	return succeeded();
}

void ae_lobby_advertised(unsigned short version, unsigned char flags)
{
	int theirs = version, compatible = theirs >= delta_legacy_minimum() && theirs <= delta_legacy_maximum() &&
		(flags & HALO_PORT_ADVERTISED_DISTRIBUTED_FLAG) != 0;

	/* (as network_client_manager.c's compatibility check: a version in the legal range on the distributed netcode;
	an incompatible one is kept until the search ends, whichever host advertised last) */
	if (compatible)
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

	lobby.now = now;
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
			search_failed(now, lobby.heard_incompatible ? AE_STR_ERR_VERSION : lobby.heard_compatible ?
				AE_STR_ERR_LOBBY_FULL : AE_STR_ERR_NO_GAME);
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
	else if (lobby.join == JOIN_CONNECTING)
	{
		if (client && network_game_client_get_state(client, NULL) >= AE_CLIENT_STATE_PREGAME)
		{
			lobby.join = JOIN_JOINED;
			platform_log("ae lobby: joined");
		}
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
	/* what upstream's branch does besides loading its widget: the host's countdown held (ui_widget.c
	network_game_reset_to_pregame_ui); then AE's pregame screen, once */
	if (global_network_game_server_get())
		network_game_server_pause_countdown(global_network_game_server_get(), TRUE);
	if (lobby.pregame_screen && !ae_ui_holds(lobby.pregame_data))
		ae_ui_push(lobby.pregame_screen, AE_OWNER_ANY, lobby.pregame_data);
	platform_log("ae lobby: took the pregame UI (the game's SELECT MAP not loaded)");
	return TRUE;
}
