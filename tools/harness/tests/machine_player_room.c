/*
MACHINE_PLAYER_ROOM.C (test)

A machine's players (source/game/players.c): each machine has a list of
MAXIMUM_LOCAL_PLAYERS entries (machine_to_player_table). A player made for a
machine whose list is full stopped the game (display_assert, system_exit), and
a host's messages, or a replay's, could ask it: a player taken out with no
quit time kept its entry, and the next one to join that machine found none.
Such a player is now refused (player_new gives NONE, its datum let go,
logged), and a player taken out frees its entry. test_machine_player_room.py
takes machine_add_player, machine_remove_player and player_new from players.c
(under_test.inc).
*/

#include <wchar.h>
#include "harness.h"

#define MAXIMUM_LOCAL_PLAYERS 4
#define MAXIMUM_NUMBER_OF_LOCAL_PLAYERS 4
#define MAXIMUM_NETWORK_MACHINE_COUNT 128
#define MAXIMUM_PLAYERS 16
#define _error_silent 0
#define csmemcpy memcpy

enum { _player_action_result_reload = 3 };

struct network_player
{
	wchar_t name[12];
	short machine_index;
	short controller_index;
};

struct player_datum
{
	short identifier;
	short local_player_index;
	wchar_t name[12];
	long unit_index;
	long dead_unit_index;
	long squad_index;
	short cluster_index;
	long aim_assist_unit_index;
	real speed_multiplier;
	long team_index;
	long action_result;
	long action_object_index;
	long quit_out_of_game_time;
	boolean quit_out_of_game;
	struct network_player network_player_data;
};

static struct data_array *player_data;
static long machine_to_player_table[MAXIMUM_NETWORK_MACHINE_COUNT][MAXIMUM_LOCAL_PLAYERS];
static int errors_logged;

static void error(int kind, const char *format, ...)
{
	(void)kind;
	(void)format;
	errors_logged++;
}

static void display_assert(const char *what, const char *file, int line, boolean halt)
{
	(void)what; (void)file; (void)line; (void)halt;
}

static void system_exit(int code)
{
	fprintf(stderr, "system_exit(%d)\n", code);
	exit(99);
}

static long datum_new_at_index(struct data_array *array, long index)
{
	(void)index;
	return datum_new(array);
}

static struct player_datum *player_get(long index)
{
	return datum_get(player_data, index);
}

static wchar_t *ustrncpy(wchar_t *to, const wchar_t *from, size_t count)
{
	return wcsncpy(to, from, count);
}

#include "under_test.inc"

static long machine_entries(long machine)
{
	long count = 0, index;

	for (index = 0; index < MAXIMUM_LOCAL_PLAYERS; index++)
		count += machine_to_player_table[machine][index] != NONE;
	return count;
}

static void reset(void)
{
	long machine, index;

	player_data = game_state_data_new("players", MAXIMUM_PLAYERS, sizeof(struct player_datum));
	for (machine = 0; machine < MAXIMUM_NETWORK_MACHINE_COUNT; machine++)
		for (index = 0; index < MAXIMUM_LOCAL_PLAYERS; index++)
			machine_to_player_table[machine][index] = NONE;
	errors_logged = 0;
}

int main(int argc, char **argv)
{
	const char *case_name = argc > 1 ? argv[1] : "";
	long players[MAXIMUM_LOCAL_PLAYERS + 1];
	long index;

	reset();
	for (index = 0; index < MAXIMUM_LOCAL_PLAYERS; index++)
	{
		players[index] = player_new(0, NONE, NONE, NULL);
		CHECK(players[index] != NONE, "player %ld of machine 0 was not made", index);
	}
	CHECK(machine_entries(0) == MAXIMUM_LOCAL_PLAYERS, "machine 0 has %ld entries", machine_entries(0));
	CASE("full-refused")
	{
		long count = player_data->count;

		players[MAXIMUM_LOCAL_PLAYERS] = player_new(0, NONE, NONE, NULL);
		CHECK(players[MAXIMUM_LOCAL_PLAYERS] == NONE, "a fifth player of machine 0 was made");
		CHECK(player_data->count == count, "the refused player's datum was kept (%ld, was %ld)", player_data->count, count);
		CHECK(errors_logged == 1, "the refusal was logged %d times", errors_logged);
		/* (another machine has its own room) */
		CHECK(player_new(1, NONE, NONE, NULL) != NONE, "machine 1's first player was refused");
		return 0;
	}
	CASE("room-after-removal")
	{
		machine_remove_player(players[0]);
		CHECK(machine_entries(0) == MAXIMUM_LOCAL_PLAYERS - 1, "the removed player's entry was kept");
		players[MAXIMUM_LOCAL_PLAYERS] = player_new(0, NONE, NONE, NULL);
		CHECK(players[MAXIMUM_LOCAL_PLAYERS] != NONE, "the player joining after a removal was refused");
		CHECK(errors_logged == 0, "a refusal was logged");
		return 0;
	}
	fprintf(stderr, "no case %s\n", case_name);
	return 2;
}
