/*
AE_BOTS_LIVE.C

bots.count changed during a local (split screen) game (Settings > MODS >
BOTS, the pause menu's SETTINGS) takes effect at once (ae_bots_live.h; the
rules: ae_bots_live_rules.c). The lobby's bots (network_server_manager.c's
network_game_server_port_bots) are players of machines of their own
(BOTS_FIRST_MACHINE up); in the game in progress they come and go the way a
player of a machine that joins or leaves the game does:

- A bot joins: the host adds its network player to the game
  (network_game_add_player, a slot no one who left holds) and tells every
  machine with _message_server_add_player_ingame
  (network_game_server_send_player_joined_info_ingame), as for a player
  added in game. The host's own machine, the only one of a local game,
  spawns it there (network_game_client_add_player_to_game: the player, its
  team, the game type's data, its queue) and the game respawns it; bots.c
  finds the new bot player in its next tick and plays it.
- A bot leaves: the host takes it out of the game (network_game_remove_player)
  and sends _message_server_remove_player_ingame with its quit time, as for
  a machine that leaves (network_game_server_send_player_quit_messages_ingame):
  the player quits there at that time (game.c's
  remove_quitting_players_from_game: its unit killed with no score, its
  weapons dropped) and its slot is free again once its body is gone. Until
  then bots.c lets it be (ae_bots_live_quitting).

One change a frame. The postgame waits for the next game's lobby, which
takes its bots from the setting as before. LAN games keep their lobby's bots
(ae_lan_bots.c) until the bots' networking is done.

HALO_BOTS_LIVE_TEST="<second>:<count>,..." (tests only) writes bots.count at
those seconds of the game's clock, as the menus would; in a LAN or internet
game the host hosts too (its network bots follow it: ae_lan_bots.c).
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_messages.h"
#include "networking/network_server_message_handler.h"

#include "bots.h"
#include "ae_bots.h"
#include "ae_bots_live_rules.h"

#include <stdio.h>
#include <stdlib.h>

/* port/linux/src/port_config.c */
int config_write(char const *name, char const *value);
void platform_log(char const *format, ...);
/* network_server_manager.c's */
void get_unique_random_color(struct network_game_server *server, struct network_player *player);

enum
{
	/* network_client_manager.c's _network_game_client_state_ingame */
	AE_BOTS_LIVE_CLIENT_INGAME = 3,
	/* network_server_manager.c's NETWORK_GAME_PLAYER_QUIT_DELAY: the ticks
	from a quit message to the quit */
	AE_BOTS_LIVE_QUIT_DELAY = 33,
};

/* _message_server_remove_player_ingame's record (network_server_manager.c's
message_server_remove_player_ingame): the player and its quit time */
struct ae_bots_live_remove_message
{
	struct network_player player;
	long quit_time;
};

static struct
{
	/* HALO_BOTS_LIVE_TEST: the next step, and the game it is for */
	int test_step;
	unsigned long test_game;
} ae_bots_live_globals;

/* HALO_BOTS_LIVE_TEST: bots.count written at its seconds of the game */
static void ae_bots_live_test(struct network_game const *game)
{
	char const *text = getenv("HALO_BOTS_LIVE_TEST");
	double second;
	int count;

	if (!text || !*text)
		return;
	/* (each game from its first step) */
	if (ae_bots_live_globals.test_game != (unsigned long)game->number_of_games_played + 1)
	{
		ae_bots_live_globals.test_game = (unsigned long)game->number_of_games_played + 1;
		ae_bots_live_globals.test_step = 0;
	}
	while (ae_bots_live_test_step(text, ae_bots_live_globals.test_step, &second, &count) &&
		game_time_get() >= (long)(second * TICKS_PER_SECOND))
	{
		char value[16];

		snprintf(value, sizeof(value), "%d", count);
		platform_log("bots: test sets bots.count to %d at tick %ld", count, game_time_get());
		config_write("bots.count", value);
		ae_bots_live_globals.test_step++;
	}
}

void ae_bots_live(struct network_game_server *server, struct network_game *game, boolean local_ingame)
{
	struct network_game_client *client = global_network_game_client_get();
	int bot_team[BOTS_MAXIMUM];
	long bot_slot[BOTS_MAXIMUM];
	long team_counts[2] = { 0, 0 };
	long people_team = NONE;
	long wanted = 0;
	long slot;
	struct ae_bots_live_change change;

	/* (the host's own machine plays the game: it takes the in-game
	messages) */
	if (!client || network_game_client_get_state(client, NULL) != AE_BOTS_LIVE_CLIENT_INGAME ||
		!game_engine_running())
	{
		return;
	}
	/* (the test's bots.count in a LAN or internet game too: the network bots
	follow it, ae_lan_bots.c's ae_lan_bots_ingame) */
	ae_bots_live_test(game);
	if (!local_ingame)
		return;
	if (ae_bots_engine_played(game->variant.game_engine_index))
		wanted = bots_wanted_count();

	for (slot = 0; slot < BOTS_MAXIMUM; slot++)
	{
		bot_team[slot] = -1;
		bot_slot[slot] = NONE;
	}
	for (slot = 0; slot < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; slot++)
	{
		struct network_player *player = &game->players[slot];
		long team = VALID_INDEX(player->team_index, 2) ? player->team_index : 0;

		if (!network_player_is_valid(player))
			continue;
		if (VALID_INDEX(player->team_index, 2))
			team_counts[player->team_index]++;
		/* (as the lobby tells them: a bot's machine is none of the game's) */
		if (bots_machine_is_bot(player->machine_index) &&
			game->machines[(long)player->machine_index].machine_index != player->machine_index)
		{
			bot_team[player->machine_index - BOTS_FIRST_MACHINE] = (int)team;
			bot_slot[player->machine_index - BOTS_FIRST_MACHINE] = slot;
		}
		else if (people_team == NONE)
		{
			people_team = team;
		}
	}

	change = ae_bots_live_decide((int)wanted, BOTS_MAXIMUM, bot_team, game->variant.universal_variant.teams != 0,
		bots_teams() == _bots_teams_against, (int)people_team, (int)team_counts[0], (int)team_counts[1],
		network_game_has_free_player_slot(game));
	if (change.kind == AE_BOTS_LIVE_ADD)
	{
		struct network_player joining;

		bots_network_player(change.bot, &joining);
		joining.team_index = (char)change.team;
		joining.primary_color_index = NONE;
		get_unique_random_color(server, &joining);
		joining.player_list_index = NONE;
		if (!network_game_add_player(game, &joining))
		{
			network_event("bots: bot %d could not join the game in progress", change.bot + 1);
			return;
		}
		if (!network_game_server_send_player_joined_info_ingame(server, &joining))
			network_event("bots: the machines missed bot %d joining the game in progress", change.bot + 1);
		network_event("bots: bot %d joined the game in progress (team %d)", change.bot + 1, change.team);
	}
	else if (change.kind == AE_BOTS_LIVE_REMOVE)
	{
		struct ae_bots_live_remove_message remove_player;
		void *message;

		remove_player.player = game->players[bot_slot[change.bot]];
		if (!network_game_remove_player(game, &remove_player.player))
		{
			network_event("bots: bot %d could not leave the game in progress", change.bot + 1);
			return;
		}
		remove_player.quit_time = game_time_get() + AE_BOTS_LIVE_QUIT_DELAY;
		message = create_network_game_message(_message_server_remove_player_ingame, &remove_player,
			sizeof(remove_player));
		if (!message || !network_game_server_send_message_to_all_machines(server, message))
			network_event("bots: the machines missed bot %d leaving the game in progress", change.bot + 1);
		network_event("bots: bot %d left the game in progress", change.bot + 1);
	}
}

boolean ae_bots_live_quitting(struct player_datum const *player, long absolute_index, struct player_action *actions)
{
	struct player_action *action;

	if (player->quit_out_of_game_time == NONE)
		return FALSE;
	/* (its facing as it was: nothing pressed, nothing moved) */
	if (absolute_index >= 0 && absolute_index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
	{
		action = &actions[absolute_index];
		action->control_flags = 0;
		action->throttle.i = 0.0f;
		action->throttle.j = 0.0f;
		action->primary_trigger = 0.0f;
		action->desired_weapon_index = NONE;
		action->desired_grenade_index = NONE;
		action->desired_zoom_level = NONE;
	}
	return TRUE;
}
