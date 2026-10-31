/*
AE_TEAM_SHUFFLE.C

The PC lobby's SHUFFLE (ae_team_shuffle.h).
*/

#include "cseries.h"
#include "game/players.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager.h"
#include "networking/network_server_manager_internal.h"
#include "networking/network_server_message_handler.h"

#include "ae_lan_bots.h"
#include "ae_team_shuffle.h"
#include "bots.h"

#include <time.h>

void platform_log(char const *format, ...);

/* whether the lobby's player is a bot (a local game's, on a machine of its own the game has not, or a LAN game's, a
controller of the host's): bots keep their own team rule (network_server_manager.c, ae_lan_bots.c), which runs on every
lobby update, so only people are dealt */
static boolean player_is_bot(struct network_game const *game, struct network_player const *player)
{
	if (bots_machine_is_bot(player->machine_index) &&
		game->machines[player->machine_index].machine_index != player->machine_index)
	{
		return TRUE;
	}
	return ae_lan_bots_network_player(player);
}

static struct network_game *open_team_lobby(struct network_game_server **server_out)
{
	struct network_game_server *server = global_network_game_server_get();
	struct network_game *game = server ? network_game_server_get_game(server) : NULL;

	if (!game || !network_game_server_lobby_is_open(server) || !game->variant.universal_variant.teams)
		return NULL;
	*server_out = server;
	return game;
}

int ae_team_shuffle_shown(void)
{
	struct network_game_server *server;

	return open_team_lobby(&server) != NULL;
}

int ae_team_shuffle(void)
{
	struct network_game_server *server = NULL;
	struct network_game *game = open_team_lobby(&server);
	long slots[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	signed char teams[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	int count = 0, bots = 0, index, red = 0;

	if (!game)
	{
		platform_log("team shuffle: not now (only the host of a team game's lobby shuffles)");
		return FALSE;
	}
	for (index = 0; index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; index++)
	{
		if (!network_player_is_valid(&game->players[index]) || !VALID_INDEX(game->players[index].team_index, 2))
			continue;
		if (player_is_bot(game, &game->players[index]))
			bots++;
		else
			slots[count++] = index;
	}
	/* (bots set against the people put every person on one team: nothing to deal) */
	if (bots > 0 && bots_teams() == _bots_teams_against)
	{
		platform_log("team shuffle: not now (the bots play against the people, who stay on one team)");
		return FALSE;
	}
	ae_team_shuffle_deal(count, (unsigned long)time(NULL) ^ ((unsigned long)clock() << 7), teams);
	for (index = 0; index < count; index++)
	{
		game->players[slots[index]].team_index = teams[index];
		red += teams[index] == 0;
	}
	/* (the bots the host moved with the lobby's team arrows are dealt again too) */
	ae_lan_bots_clear_teams();
	network_game_server_send_game_data_pregame(server);
	platform_log("team shuffle: %d people dealt, %d red and %d blue (%d bots even the teams up after)", count, red,
		count - red, bots);
	return TRUE;
}
