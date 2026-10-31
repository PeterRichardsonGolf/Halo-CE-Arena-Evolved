/*
AE_HELD_PLAYERS.C

A joining client's player additions that map sharing held (network_client_manager.c network_game_client_add_player:
the host's settings not come yet, or the map being downloaded: map_share_holds_players). Upstream's pregame widget
asks again each frame, so a held ask is only late there; AE's menus ask once (ae_screen_browser.c, ae_screen_join_invite.c,
ae_glue_lobby.c ae_lobby_add_local_player), and a held ask was lost: the game started with no player for that
machine, its screen black (seen in 0.5.0's test round, a second PC joining a lobby). Each held controller is remembered
and asked for from the client's idle once the hold is over, in the pregame or in a game in progress the client has
loaded, unless the host's settings have the player already (another ask went through meanwhile). Cleared by a
removal ask and the client's reset.
*/

#include "cseries.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_manager.h"
#include "networking/network_messages.h"

#include "ae_hooks.h"
#include "map_share.h"

static boolean held[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];

boolean ae_held_player_note(short local_player_index, boolean is_held)
{
	if (local_player_index >= 0 && local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		held[local_player_index] = is_held;
	return TRUE;
}

void ae_held_players_clear(void)
{
	csmemset(held, 0, sizeof(held));
}

void ae_held_players_send(struct network_game_client *client, boolean ready, long machine_index,
	struct network_game const *game)
{
	short controller;

	if (!client || !game || !ready || map_share_holds_players())
		return;
	for (controller = 0; controller < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; controller++)
	{
		long index;
		boolean in_game = FALSE;

		if (!held[controller])
			continue;
		held[controller] = FALSE;
		for (index = 0; index < (long)NUMBEROF(game->players) && !in_game; index++)
		{
			struct network_player const *player = &game->players[index];

			in_game = network_player_is_valid((struct network_player *)player) && player->machine_index == machine_index &&
				player->controller_index == controller;
		}
		if (in_game)
			continue;
		network_event("the player addition held for the host's settings or the map goes now (controller index #%d)",
			controller);
		network_game_client_add_player(client, controller);
	}
}
