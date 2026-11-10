/*
AE_TEAM_OVERRIDE.C

The host's team override (ae_team_override.h).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
unsigned long system_milliseconds(void);
#include "game/players.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager.h"
#include "networking/network_server_manager_internal.h"
#include "networking/network_server_message_handler.h"
#include "halo_port_limits.h"

#include "interface/hud_messaging.h"
#include "../src/delta_wire.h"
#include "ae_lan_bots.h"
#include "ae_team_override.h"
#include "team_change.h"
#ifdef HALO_PORT_CHAT
#include "chat.h"
#endif

void platform_log(char const *format, ...);

/* the slot of the game's player list for a machine and controller, NONE when there is none */
static long slot_of(struct network_game const *game, short machine, short controller)
{
	long slot;

	for (slot = 0; slot < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; slot++)
	{
		struct network_player const *player = &game->players[slot];

		if (network_player_is_valid((struct network_player *)player) && player->machine_index == machine &&
			player->controller_index == controller)
		{
			return slot;
		}
	}
	return NONE;
}

int ae_team_override_lobby(short machine, short controller)
{
	struct network_game_server *server = global_network_game_server_get();
	struct network_game *game = server ? network_game_server_get_game(server) : NULL;
	long slot;

	if (!game || !network_game_server_lobby_is_open(server) || !game->variant.universal_variant.teams)
		return 0;
	slot = slot_of(game, machine, controller);
	if (slot == NONE || !VALID_INDEX(game->players[slot].team_index, 2) || ae_lan_bots_network_player(&game->players[slot]))
		return 0;
	game->players[slot].team_index = (char)(game->players[slot].team_index == 0 ? 1 : 0);
	network_game_server_send_game_data_pregame(server);
	platform_log("team override: the lobby's player %ld (machine %d, controller %d) put on the %s team", slot,
		(int)machine, (int)controller, game->players[slot].team_index ? "blue" : "red");
	return 1;
}

/* ---------- the move made (the host's): the player's own screen, one chat line */

/* the local player index on this machine for the player at machine, controller, NONE when theirs is another PC's */
static short local_index_of(short machine, short controller)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (!player_data)
		return NONE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (!player->quit_out_of_game && player->network_player_data.machine_index == machine &&
			player->network_player_data.controller_index == controller)
		{
			return player->local_player_index;
		}
	}
	return NONE;
}

/* said on the moved player's own screen when they play on this PC (a split-screen player of the host's: a joiner's PC is
told by the chat line alone, a client cannot tell the host's move from a team balance), the hud_print_message's local player */
static void tell_local(short machine, short controller, short team, int deferred)
{
	char const *text = deferred ? (team ? "THE HOST MOVES YOU TO THE BLUE TEAM. At your next death." :
		"THE HOST MOVES YOU TO THE RED TEAM. At your next death.") :
		(team ? "MOVED TO THE BLUE TEAM. By the host. Not counted as a death." :
		"MOVED TO THE RED TEAM. By the host. Not counted as a death.");
	short local = local_index_of(machine, controller);
	wchar_t line[80];
	int at;

	if (local < 0 || local >= 4)
		return;
	for (at = 0; text[at] && at < (int)NUMBEROF(line) - 1; at++)
		line[at] = (wchar_t)text[at];
	line[at] = 0;
	hud_print_message(local, line);
}

/* a move that waits for the player's next death (a round game): its chat line and notice when it happens */
enum { PENDING_MAXIMUM = 8, PENDING_MS = 600000 };
static struct
{
	int used;
	short machine, controller, team;
	unsigned long since;
	char name[48], host[48];
} pending[PENDING_MAXIMUM];

int ae_team_override_game(short machine, short controller, short team, char const *name, char const *host,
	char *text, int size)
{
	struct network_game_server *server = global_network_game_server_get();
	struct network_game *game = server ? network_game_server_get_game(server) : NULL;
	long slot = game ? slot_of(game, machine, controller) : NONE;
	int result, index;

	if (slot == NONE)
	{
		snprintf(text, (size_t)size, "not in the game");
		return -1;
	}
	result = team_change_move(slot, team, text, size);
	platform_log("team override: game player %ld (machine %d, controller %d) to the %s team: result %d (%s)", slot,
		(int)machine, (int)controller, team ? "blue" : "red", result, text);
	if (result == _delta_team_changed)
	{
		ae_team_override_say(host, name, team);
		tell_local(machine, controller, team, 0);
	}
	else if (result == _delta_team_changes_at_respawn)
	{
		/* (said when it happens: ae_team_override_tick) */
		for (index = 0; index < PENDING_MAXIMUM && pending[index].used; index++)
			;
		if (index < PENDING_MAXIMUM)
		{
			pending[index].used = 1;
			pending[index].machine = machine;
			pending[index].controller = controller;
			pending[index].team = team;
			pending[index].since = system_milliseconds();
			snprintf(pending[index].name, sizeof(pending[index].name), "%s", name ? name : "");
			snprintf(pending[index].host, sizeof(pending[index].host), "%s", host ? host : "");
		}
		tell_local(machine, controller, team, 1);
	}
	return result;
}

int ae_team_override_last_of_team(short machine, short controller)
{
	struct network_game_server *server = global_network_game_server_get();
	struct network_game *game = server ? network_game_server_get_game(server) : NULL;
	long slot = game ? slot_of(game, machine, controller) : NONE;

	return slot != NONE && team_change_last_of_team(slot);
}

void ae_team_override_say(char const *host_name, char const *name, short team)
{
	char line[160];

	snprintf(line, sizeof(line), "%s moved %s to the %s Team", host_name && host_name[0] ? host_name : "The host", name,
		team ? "Blue" : "Red");
#ifdef HALO_PORT_CHAT
	chat_server_say(line);
#endif
	platform_log("team override: %s", line);
}

void ae_team_override_tick(void)
{
	struct network_game_server *server = global_network_game_server_get();
	struct network_game *game = server ? network_game_server_get_game(server) : NULL;
	unsigned long now = system_milliseconds();
	int index;

	for (index = 0; index < PENDING_MAXIMUM; index++)
	{
		long slot;

		if (!pending[index].used)
			continue;
		slot = game ? slot_of(game, pending[index].machine, pending[index].controller) : NONE;
		if (slot == NONE || now - pending[index].since > PENDING_MS)
		{
			/* (the player left, the game ended, or the death never came) */
			pending[index].used = 0;
			continue;
		}
		if (game->players[slot].team_index == pending[index].team)
		{
			ae_team_override_say(pending[index].host, pending[index].name, pending[index].team);
			tell_local(pending[index].machine, pending[index].controller, pending[index].team, 0);
			pending[index].used = 0;
		}
	}
}
