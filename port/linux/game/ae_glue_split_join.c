/*
AE_GLUE_SPLIT_JOIN.C

See ae_glue_split_join.h.
*/

#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/player_ui.h"
#include "main/main.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager.h"
#include "halo_port_limits.h"
#include "ae_glue_lobby.h"
#include "ae_glue_social_game.h"
#include "ae_glue_split_join.h"
#include "ae_bots_live.h"
#include "ae_host_vote.h"
#include "bots.h"
#include "ae_lan_bots.h"
#include "ae_split_join_rules.h"
#include "ae_variant_ext.h"
#include "network_coop.h"
#include "input/input.h"

void platform_log(char const *format, ...);

/* network_client_manager.c's _network_game_client_state_ingame */
enum { CLIENT_STATE_INGAME = 3 };

/* a request answers once: another within this long is the same (the host's answer is a network round trip) */
#define WAIT_MILLISECONDS 4000UL

/* controllers DEV MODE's `split pad` counts as pads (a test with no second pad) */
static unsigned staged_pads;

void ae_split_join_stage_pad(short controller)
{
	if (controller >= 1 && controller < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		staged_pads |= 1u << controller;
}

static unsigned long pending_at[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
static int pending[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
static unsigned long asked_at[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
static int asked[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];

void ae_split_join_new_game(void)
{
	staged_pads = 0;
	memset(pending, 0, sizeof(pending));
	memset(pending_at, 0, sizeof(pending_at));
	memset(asked, 0, sizeof(asked));
	memset(asked_at, 0, sizeof(asked_at));
}

int ae_split_join_local_players(void)
{
	return ae_lobby_local_players();
}

/* the bots of the game: [0] the local game's (the host's own: they leave through ae_bots_live_make_room), [1] a LAN or
internet game's (the host makes room as it adds the player: ae_lan_bots_make_room) */
static void bots_in_game(struct network_game *game, int *local_bots, int *network_bots)
{
	short index;

	*local_bots = *network_bots = 0;
	for (index = 0; game && index < (short)NUMBEROF(game->players); index++)
	{
		struct network_player const *player = &game->players[index];

		if (!network_player_is_valid(player))
			continue;
		if (ae_lan_bots_network_player(player))
			(*network_bots)++;
		else if (bots_machine_is_bot(player->machine_index))
			(*local_bots)++;
	}
}

/* free player slots of the game, a bot's place counted as free (it leaves for a person) */
static int slots_free(struct network_game *game)
{
	int local_bots, network_bots;

	if (!game)
		return 0;
	if (network_game_has_free_player_slot(game))
		return 1;
	bots_in_game(game, &local_bots, &network_bots);
	return local_bots + network_bots;
}

int ae_split_join_reason(short controller, int pad_staged)
{
	struct ae_split_join_facts facts;
	struct network_game *game = network_game_get_game();
	struct network_game_client *client = global_network_game_client_get();
	struct ae_variant_ext const *ext = ae_variant_ext_game();

	memset(&facts, 0, sizeof(facts));
	facts.multiplayer = client && game && game_in_progress() && game_engine_running() && network_game_is_active() &&
		!network_coop_active() && network_game_client_get_state(client, NULL) == CLIENT_STATE_INGAME;
	facts.replay = game_connection() == _game_connection_film_playback;
	facts.postgame = game_engine_showing_postgame() || game_engine_game_over();
	facts.option_on = ext && ext->split_join == AE_EXT_SPLIT_JOIN_ON;
	facts.tournament = ae_social_game_active();
	facts.vote = ae_host_vote_running();
	facts.pad = controller >= 1 && controller < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS &&
		(pad_staged || input_has_gamepad(controller) || (staged_pads & (1u << controller)));
	facts.controller_plays = ae_lobby_controller_plays(controller);
	facts.local_players = ae_lobby_local_players();
	facts.slots_free = slots_free(game);
	return ae_split_join_check(&facts);
}

short ae_split_join_free_controller(void)
{
	unsigned plays = 0, pads = 0;
	short controller;

	for (controller = 0; controller < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; controller++)
	{
		if (ae_lobby_controller_plays(controller))
			plays |= 1u << controller;
		if (controller >= 1 && (input_has_gamepad(controller) || (staged_pads & (1u << controller))))
			pads |= 1u << controller;
	}
	return (short)ae_split_join_controller(plays, pads, -1);
}

int ae_split_join_request(short controller, int pad_staged)
{
	int reason = ae_split_join_reason(controller, pad_staged);
	struct ae_result result;

	if (reason != AE_SPLIT_JOIN_OK)
	{
		struct network_game *game = network_game_get_game();

		platform_log("split join: controller %d refused: %s (game %d of %d players, %d here)", (int)controller + 1,
			ae_split_join_reason_text(reason), game ? (int)game->player_count : -1, game ? (int)game->maximum_players : -1,
			ae_lobby_local_players());
		return reason;
	}
	if (ae_split_join_waiting(controller))
		return AE_SPLIT_JOIN_OK;
	/* (a full local game: its last bot leaves for the player, who asks once the place is free: ae_split_join_tick) */
	{
		struct network_game *game = network_game_get_game();
		int local_bots, network_bots;

		bots_in_game(game, &local_bots, &network_bots);
		if (game && !network_game_has_free_player_slot(game) && local_bots > 0 && network_bots == 0)
		{
			ae_bots_live_make_room();
			pending[controller] = 1;
			pending_at[controller] = system_milliseconds();
			platform_log("split join: controller %d waits for a bot to leave its place", (int)controller + 1);
			return AE_SPLIT_JOIN_OK;
		}
	}
	result = ae_lobby_join_controller(controller);
	if (!result.ok)
	{
		platform_log("split join: controller %d could not ask the host: %s", (int)controller + 1, result.reason);
		return AE_SPLIT_JOIN_FULL;
	}
	asked[controller] = 1;
	asked_at[controller] = system_milliseconds();
	platform_log("split join: controller %d asked the host for a player (%d players here)", (int)controller + 1,
		ae_lobby_local_players());
	return AE_SPLIT_JOIN_OK;
}

int ae_split_join_waiting(short controller)
{
	if (controller < 0 || controller >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		return 0;
	if (pending[controller])
		return 1;
	if (!asked[controller])
		return 0;
	if (ae_lobby_controller_plays(controller) &&
		local_player_get_player_index(controller) != NONE)
	{
		asked[controller] = 0;
		return 0;
	}
	if (system_milliseconds() - asked_at[controller] >= WAIT_MILLISECONDS)
	{
		asked[controller] = 0;
		return 0;
	}
	return 1;
}

int ae_split_leave_request(short controller)
{
	if (!ae_split_leave_allowed(ae_lobby_controller_plays(controller), ae_lobby_local_players()))
	{
		platform_log("split join: controller %d may not leave (not a player, or the last one here)", (int)controller + 1);
		return -1;
	}
	network_game_client_local_player_quit(controller);
	platform_log("split join: controller %d asked to leave the game (%d players here)", (int)controller + 1,
		ae_lobby_local_players());
	return 0;
}

/* when each machine's first player asked to join this game (0: the host did not see it) */
enum { MAXIMUM_ASK_MACHINES = 64 };
static unsigned long first_ask_at[MAXIMUM_ASK_MACHINES];

int ae_split_join_host_allows_machine(short machine_index)
{
	struct network_game *game = network_game_get_game();
	struct ae_variant_ext const *ext = ae_variant_ext_game();
	short index;
	int players = 0;
	long since_first = -1;

	for (index = 0; game && index < (short)NUMBEROF(game->players); index++)
	{
		if (network_player_is_valid(&game->players[index]) && (short)game->players[index].machine_index == machine_index &&
			!ae_lan_bots_network_player(&game->players[index]))
			players++;
	}
	/* (a machine's first request in this game: the pads it joins with ask within moments of it) */
	if (machine_index >= 0 && machine_index < MAXIMUM_ASK_MACHINES)
	{
		if (players == 0)
			first_ask_at[machine_index] = system_milliseconds() + (system_milliseconds() == 0);
		else if (first_ask_at[machine_index])
			since_first = (long)(system_milliseconds() - first_ask_at[machine_index]);
	}
	if (!ae_split_join_host_allows(ext && ext->split_join == AE_EXT_SPLIT_JOIN_ON, ae_social_game_active(), players,
		since_first))
	{
		platform_log("split join: machine %d asked for another player and the gametype keeps its %d (refused)",
			(int)machine_index, players);
		return 0;
	}
	return 1;
}

const char *ae_split_join_words(int reason)
{
	return ae_split_join_reason_text(reason);
}

void ae_split_join_joined(short controller)
{
	if (controller >= 0 && controller < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		asked[controller] = 0;
	platform_log("split join: controller %d is in the game (%d players here)", (int)controller + 1,
		ae_lobby_local_players());
}

/* a local player's slot never names a player that is gone (a slot a joiner takes deletes the player who quit in it) */
void ae_split_join_player_deleted(long player_index)
{
	short local_player_index;

	for (local_player_index = 0; local_player_index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; local_player_index++)
	{
		if (players_globals && players_globals->local_players[local_player_index] == player_index)
			players_globals->local_players[local_player_index] = NONE;
	}
}

/* a controller waiting for a bot's place asks once the place is free (a bot goes at the host's next step) */
void ae_split_join_tick(void)
{
	short controller;

	for (controller = 0; controller < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; controller++)
	{
		struct network_game *game = network_game_get_game();

		if (!pending[controller])
			continue;
		if (system_milliseconds() - pending_at[controller] > 8000UL || !game)
		{
			pending[controller] = 0;
			platform_log("split join: controller %d gave up waiting for a place", (int)controller + 1);
		}
		else if (network_game_has_free_player_slot(game))
		{
			pending[controller] = 0;
			if (ae_lobby_join_controller(controller).ok)
			{
				asked[controller] = 1;
				asked_at[controller] = system_milliseconds();
				platform_log("split join: controller %d asked the host for a player (%d players here)",
					(int)controller + 1, ae_lobby_local_players());
			}
		}
	}
}
