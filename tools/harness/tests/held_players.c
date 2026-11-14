/*
HELD_PLAYERS.C (test)

Players held out of a game while their machine downloads the map
(source/networking/network_server_manager.c, OpenCE's) are the pregame's again
when the game ends without the download finishing: AE's hook in
network_game_server_reset_to_pregame (DamnationCE v0.3.41's fix), which puts
back the held players of the machines still in the game and lets go of a gone
machine's. test_held_players.py takes the constants (config.inc) and the code
(under_test.inc): the hold, restore and discard functions and the hook's lines.
*/

#include "harness.h"
#include "config.inc"

#include <stdarg.h>

/* the fake world: what the code reads */
struct network_player
{
	short machine_index;
	short player_list_index;
	boolean valid;
	long identity;
};

struct network_game
{
	struct network_player players[MAXIMUM_NETWORK_PLAYER_COUNT];
	short player_count;
};

struct network_game_server_client_machine
{
	long machine_index;
	boolean joined;
};

struct network_game_server
{
	struct network_game game;
	struct network_game_server_client_machine client_machines[MAXIMUM_NETWORK_MACHINE_COUNT];
};

static boolean machine_needs_map[MAXIMUM_NETWORK_MACHINE_COUNT];

#define csmemset(destination, value, size) memset(destination, value, size)
#define network_event(...) ((void)0)
#define map_share_machine_needs_map(machine_index) (machine_needs_map[(machine_index)])
#define network_player_is_valid(player) ((player)->valid)
#define network_game_server_client_machine_is_joined_to_game(server, machine) ((machine)->joined)

/* (as network_game_manager.c's: a player is in the slot its player_list_index
names, and a requested slot must be free; NONE takes the first free one) */
static boolean network_game_remove_player(struct network_game *game, struct network_player *player)
{
	int index;

	for (index = 0; index < MAXIMUM_NETWORK_PLAYER_COUNT; index++)
	{
		if (game->players[index].valid && game->players[index].identity == player->identity)
		{
			memset(&game->players[index], 0, sizeof(game->players[index]));
			game->players[index].player_list_index = NONE;
			game->player_count--;
			return TRUE;
		}
	}
	return FALSE;
}

static boolean network_game_add_player(struct network_game *game, struct network_player *player)
{
	int slot = player->player_list_index;
	int index;

	if (slot == NONE)
	{
		for (index = 0; index < MAXIMUM_NETWORK_PLAYER_COUNT && slot == NONE; index++)
		{
			if (!game->players[index].valid)
				slot = index;
		}
	}
	if (slot < 0 || slot >= MAXIMUM_NETWORK_PLAYER_COUNT || game->players[slot].valid)
		return FALSE;
	game->players[slot] = *player;
	game->players[slot].player_list_index = (short)slot;
	game->players[slot].valid = TRUE;
	game->player_count++;
	return TRUE;
}

static struct network_player network_game_server_held_players[MAXIMUM_NETWORK_PLAYER_COUNT];
static long network_game_server_held_player_count;

#include "under_test.inc"

/* the reset's held players, as network_game_server_reset_to_pregame handles them */
static void reset_to_pregame(struct network_game_server *server)
{
	int i;

#include "reset_hook.inc"
	csmemset(network_game_server_held_players, 0, sizeof(network_game_server_held_players));
	network_game_server_held_player_count = 0;
	(void)i;
}

static struct network_game_server server;

/* players 1-2 on the host (machine 0), 3 on machine 1, 4-5 on machine 2 */
static void setup(void)
{
	static short const machines[] = { 0, 0, 1, 2, 2 };
	int index;

	memset(&server, 0, sizeof(server));
	memset(machine_needs_map, 0, sizeof(machine_needs_map));
	network_game_server_held_player_count = 0;
	for (index = 0; index < MAXIMUM_NETWORK_PLAYER_COUNT; index++)
		server.game.players[index].player_list_index = NONE;
	for (index = 0; index < MAXIMUM_NETWORK_MACHINE_COUNT; index++)
	{
		server.client_machines[index].machine_index = index;
		server.client_machines[index].joined = index <= 2;
	}
	for (index = 0; index < 5; index++)
	{
		struct network_player player = { machines[index], (short)index, TRUE, 101 + index };

		network_game_add_player(&server.game, &player);
	}
}

static boolean in_game(long identity)
{
	int index;

	for (index = 0; index < MAXIMUM_NETWORK_PLAYER_COUNT; index++)
	{
		if (server.game.players[index].valid && server.game.players[index].identity == identity)
			return TRUE;
	}
	return FALSE;
}

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";

	/* machine 2 still downloading at the start: its players held, then back in the pregame */
	CASE("held-back")
	{
		setup();
		machine_needs_map[2] = TRUE;
		network_game_server_hold_map_fetcher_players(&server);
		CHECK(server.game.player_count == 3, "the downloading machine's players were not held (%d)",
			server.game.player_count);
		CHECK(!in_game(104) && !in_game(105), "a downloading machine's player stayed in the game");
		reset_to_pregame(&server);
		CHECK(server.game.player_count == 5, "the held players are not the pregame's again (%d players)",
			server.game.player_count);
		CHECK(in_game(104) && in_game(105), "a held player was dropped at the reset");
		CHECK(network_game_server_held_player_count == 0, "held players left over after the reset");
		return 0;
	}
	/* a machine that left while held: its players are not put back */
	CASE("gone-machine")
	{
		setup();
		machine_needs_map[2] = TRUE;
		network_game_server_hold_map_fetcher_players(&server);
		server.client_machines[2].joined = FALSE;
		reset_to_pregame(&server);
		CHECK(server.game.player_count == 3, "a gone machine's held players came back (%d players)",
			server.game.player_count);
		CHECK(!in_game(104) && !in_game(105), "a gone machine's player is in the pregame");
		CHECK(in_game(101) && in_game(103), "the others were touched");
		return 0;
	}
	/* a late joiner took a held player's slot in the game: the held player still comes back, in a free slot */
	CASE("slot-taken")
	{
		struct network_player late = { 1, 3, TRUE, 201 };

		setup();
		machine_needs_map[2] = TRUE;
		network_game_server_hold_map_fetcher_players(&server);
		CHECK(!server.game.players[3].valid, "the held player's slot is not free in the game");
		CHECK(network_game_add_player(&server.game, &late), "the late joiner did not get slot 3");
		reset_to_pregame(&server);
		CHECK(server.game.player_count == 6, "a held player whose slot was taken was dropped (%d players)",
			server.game.player_count);
		CHECK(in_game(104) && in_game(105) && in_game(201), "a player is missing after the reset");
		return 0;
	}
	/* nobody held: the reset changes nothing */
	CASE("none-held")
	{
		setup();
		network_game_server_hold_map_fetcher_players(&server);
		CHECK(server.game.player_count == 5, "a player was held with no download (%d)", server.game.player_count);
		reset_to_pregame(&server);
		CHECK(server.game.player_count == 5, "the reset changed the players (%d)", server.game.player_count);
		return 0;
	}
	fprintf(stderr, "no case %s\n", case_name);
	return 2;
}
