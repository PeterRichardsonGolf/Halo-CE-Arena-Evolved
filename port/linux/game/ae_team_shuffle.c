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

#include "ae_team_shuffle.h"

#include <time.h>

void platform_log(char const *format, ...);

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
	int count = 0, index, red = 0;

	if (!game)
	{
		platform_log("team shuffle: not now (only the host of a team game's lobby shuffles)");
		return FALSE;
	}
	for (index = 0; index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; index++)
	{
		if (network_player_is_valid(&game->players[index]) && VALID_INDEX(game->players[index].team_index, 2))
			slots[count++] = index;
	}
	ae_team_shuffle_deal(count, (unsigned long)time(NULL) ^ ((unsigned long)clock() << 7), teams);
	for (index = 0; index < count; index++)
	{
		game->players[slots[index]].team_index = teams[index];
		red += teams[index] == 0;
	}
	network_game_server_send_game_data_pregame(server);
	platform_log("team shuffle: %d players dealt, %d red and %d blue", count, red, count - red);
	return TRUE;
}
