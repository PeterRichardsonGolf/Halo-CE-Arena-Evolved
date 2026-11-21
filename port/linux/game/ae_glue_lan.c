/*
AE_GLUE_LAN.C

Join Game > LAN's firewall hint (item 73; ae_glue_lan.h): on Linux, when the list has found nothing for
AE_LAN_HINT_DELAY_MS and a firewall is on (ae_lan_hint.c reads which), its two lines under the list say that a
firewall can hide LAN games and where README says which ports to open, and debug.txt says it once. Elsewhere, or with
games found, or with no firewall found, the list is as it was.

The PC lobby's B from a pad with no player (item 108): the whole machine leaves the lobby, as its last player's B
does. Upstream's B takes out only the player of the pad that pressed it, and a pad with no player only cancels ADD
PLAYER: with two or more pads the lobby looked stuck (the pad whose player had left did nothing).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "interface/player_ui.h"
#include "interface/ui_widget_instance.h"
#include "interface/ui_widget.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager.h"
#include "../src/ae_lan_hint.h"
#include "ae_glue_lan.h"
#include "ae_split_screen.h"
#include "ae_lan_bots.h"

void platform_log(char const *format, ...);

/* the list's labels (strings.xml's join_game_ticker_labels: Players, Rules, Message) */
enum { LABEL_RULES = 1, LABEL_MESSAGE = 2 };

static struct
{
	unsigned long begin;
	/* looked for a firewall in this search, what was found */
	int looked;
	int firewall;
	char what[32];
	/* the hint shown this frame; said in debug.txt this run */
	int shown;
	int logged;
} lan;

void ae_lan_hint_begin(void)
{
	lan.begin = system_milliseconds();
	lan.looked = FALSE;
	lan.shown = FALSE;
}

int ae_lan_hint_line(int line, int empty, wchar_t *text, short size)
{
	char const *hint = line ? AE_LAN_HINT_LINE_2 : AE_LAN_HINT_LINE_1;
	short index;

	if (line == 0)
	{
		unsigned long elapsed = system_milliseconds() - lan.begin;

		/* (the files read once a search, when the delay is over with nothing found) */
		if (empty && elapsed >= AE_LAN_HINT_DELAY_MS && !lan.looked)
		{
			lan.looked = TRUE;
			lan.firewall = ae_lan_firewall(lan.what, sizeof(lan.what));
		}
		lan.shown = ae_lan_hint_due(elapsed, empty, lan.looked && lan.firewall);
		if (lan.shown && !lan.logged)
		{
			lan.logged = TRUE;
			platform_log("lan: no games found in %lu s and a firewall is on (%s): %s %s",
				AE_LAN_HINT_DELAY_MS / 1000, lan.what, AE_LAN_HINT_LINE_1, AE_LAN_HINT_LINE_2);
		}
	}
	if (!lan.shown || !text || size <= 0)
		return FALSE;
	for (index = 0; index < size - 1 && hint[index]; index++)
		text[index] = (wchar_t)hint[index];
	text[index] = 0;
	return TRUE;
}

void ae_lan_hint_label(struct widget_instance *label)
{
	if (label)
		label->parameters.text_box.string_list_index = lan.shown ? LABEL_MESSAGE : LABEL_RULES;
}

int ae_lobby_leave_all(short controller)
{
	struct network_game_client *client = global_network_game_client_get();
	struct network_game *game = client ? network_game_client_get_game(client) : NULL;
	short machine_index = network_game_client_get_local_machine_index();
	short state, index, removed = 0;

	if (!game || network_game_client_get_state(client, &state) != 2)
		return FALSE;
	/* every player of this machine asks to leave (as each one's own B would) */
	for (index = 0; machine_index != NONE && index < (short)NUMBEROF(game->players); index++)
	{
		struct network_player *player = &game->players[index];

		if (network_player_is_valid(player) && (short)player->machine_index == machine_index)
		{
			if (!network_game_client_request_remove_player(client, player))
				platform_log("ae lobby: B: the removal of the player of controller #%d was not sent",
					(int)player->controller_index);
			player_ui_clear_multiplayer_autojoin_for_local_player((short)player->controller_index);
			removed++;
		}
	}
	/* then what the last player's B does (netgame_unjoin_player): a host still taking machines holds its countdown,
	else the game is left */
	/* (AE: a split screen game too, which takes no machine: kept for Server Setup's START GAME, ae_split_screen.h) */
	if (global_network_game_server_get() && (network_game_should_accept_remote_connections() == TRUE ||
		ae_split_screen_hosting()))
		network_game_server_pause_countdown(global_network_game_server_get(), TRUE);
	else
	{
		dispose_global_network_game_client();
		dispose_global_network_game_server();
	}
	player_ui_autojoin_players_to_next_multiplayer_game();
	platform_log("ae lobby: B of controller #%d (no player of its own): this machine left the lobby (%d players)",
		(int)controller, (int)removed);
	return TRUE;
}

int ae_lobby_only_bots_stay(short controller)
{
	struct network_game_client *client = global_network_game_client_get();
	struct network_game *game = client ? network_game_client_get_game(client) : NULL;
	short machine_index = network_game_client_get_local_machine_index();
	short state, index;
	boolean mine = FALSE;
	short bots = 0;

	if (!game || machine_index == NONE || network_game_client_get_state(client, &state) != 2)
		return FALSE;
	for (index = 0; index < (short)NUMBEROF(game->players); index++)
	{
		struct network_player *player = &game->players[index];

		if (!network_player_is_valid(player) || (short)player->machine_index != machine_index)
			continue;
		if ((short)player->controller_index == controller && !ae_lan_bots_network_player(player))
			mine = TRUE;
		else if (ae_lan_bots_network_player(player))
			bots++;
		else
			return FALSE; /* (another person of this machine stays: the game's own B) */
	}
	return mine && bots > 0;
}

boolean ui_widget_port_multiplayer_player(short controller_index, long profile_index);
boolean ui_widget_port_host(struct widget_instance *widget, struct event_record *event, boolean *widget_deleted);

unsigned char ae_lobby_server_rehost(struct widget_instance *widget, struct event_record *event,
	unsigned char *widget_deleted)
{
	long profile = player_ui_get_active_player_profile_index(0);
	boolean deleted = FALSE, result;

	if (global_network_game_server_get())
		return TRUE;
	platform_log("ae lobby: Server Setup's START GAME with its game closed (the lobby was left): hosting again");
	if (profile == NONE || !ui_widget_port_multiplayer_player(0, profile))
		return (unsigned char)ae_lobby_server_gone();
	result = ui_widget_port_host(widget, event, &deleted);
	if (widget_deleted)
		*widget_deleted = (unsigned char)deleted;
	return (unsigned char)(result && global_network_game_server_get() ? TRUE : ae_lobby_server_gone());
}

int ae_lobby_server_gone(void)
{
	display_error_text_deferred(L"The game closed. Press B and host the game again.", NONE);
	platform_log("ae lobby: Server Setup's START GAME with no game hosted (it closed after the lobby was left)");
	return FALSE;
}
