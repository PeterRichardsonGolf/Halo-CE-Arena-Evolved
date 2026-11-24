#include "harness.h"
/* team_change.c's pending changes are kept by player slot: a player who joins into a slot another left (the same
absolute index, another identifier) starts with no change and no cooldown of the old player's */

#include "config.inc"
struct player_datum { short team_index; long unit_index; boolean quit_out_of_game; long respawn_timer; };
#define DATUM_INDEX_TO_ABSOLUTE_INDEX(index) ((index) & 0xFFFF)
#define TICKS_PER_SECOND 30
#define VALID_INDEX(index, count) ((index) >= 0 && (index) < (count))

static struct player_datum players[4];
static long handles[4];
static int kills, applied;
static int server_object;

static struct player_datum *player_try_and_get(long index)
{
	long slot = index == NONE ? NONE : DATUM_INDEX_TO_ABSOLUTE_INDEX(index);
	return slot >= 0 && slot < 4 && handles[slot] == index ? &players[slot] : NULL;
}
static void *global_network_game_server_get(void) { return &server_object; }
static void unit_kill_no_statistics(long unit_index) { kills++; }
static void platform_log(char const *format, ...) { }
static boolean game_engine_has_teams(void) { return TRUE; }
static void team_change_apply(long player_index, short team) { applied++; }
#include "under_test.inc"

static void reset_table(void)
{
	long index;
	memset(&team_change, 0, sizeof(team_change));
	for (index = 0; index < TEAM_CHANGE_PLAYERS; index++)
		team_change.players[index].last_time = NONE;
}

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	long old_player = 0xE1230003, new_player = 0xE1240003;
	struct team_change_player *change;

	reset_table();
	memset(players, 0, sizeof(players));
	players[3].team_index = 0;
	players[3].unit_index = 77;
	/* the old player in slot 3 asked to change to blue in a round-based game, 2 s ago */
	handles[3] = old_player;
	change = change_of(old_player);
	CHECK(change != NULL, "no entry for the player");
	change->waiting = TRUE;
	change->team = 1;
	change->last_time = 1000;

	CASE("same-player")
	{
		CHECK(effective_team(old_player, &players[3]) == 1, "the waiting change lost");
		CHECK(team_change_round_started(old_player) == TRUE && kills == 1, "the change not made at the round start");
		return 0;
	}
	CASE("new-player-in-slot")
	{
		/* the old player leaves; a joiner gets the slot with a new identifier */
		handles[3] = new_player;
		CHECK(effective_team(new_player, &players[3]) == 0, "the joiner takes the old player's chosen team");
		CHECK(team_change_round_started(new_player) == FALSE && kills == 0, "the joiner killed at the round start");
		change = change_of(new_player);
		CHECK(change && change->last_time == NONE, "the joiner has the old player's cooldown");
		return 0;
	}
	CASE("new-player-died")
	{
		handles[3] = new_player;
		team_change_player_died(new_player);
		CHECK(applied == 0, "the joiner moved by the old player's change at their first death");
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
