#include "harness.h"
#include <stdarg.h>
/* server_commands.c's sv_kick / sv_ban: the kick takes the player out of the game's list (the slots after it move
up), so the leave reason goes to the machine copied before it, not to whatever the slot names after */

enum { _server_role_moderator = 1, _server_role_owner = 3, _server_log_info = 1 };
struct network_player_fake { long machine_index; };
struct network_game { struct network_player_fake players[16]; long player_count; };
static struct { int role; } server_commands = { _server_role_owner };
static struct network_game game;
static long removed_machine = -99;
static char removed_why[16];
static long kicked_machine = -99;

static struct network_game *server_game(void) { return &game; }
static void say(char const *format, ...) { }
static long find_player(char const *command, char const *word) { return 1; }
static boolean mod_may_act(char const *command, long slot) { return TRUE; }
static void server_status_player_name(long slot, char *name, long size) { snprintf(name, size, "Kickee"); }
static void server_log(int level, char const *area, char const *format, ...) { }
static char const *server_role_name(int role) { return "owner"; }
static void moderation_event(char const *event, char const *name, char const *reason, unsigned long until) { }
static long parse_duration(char const *word) { return -1; }
static boolean ban_duration_allowed(int role, long seconds) { return TRUE; }
static char const *rest_after_words(char const *line, long words) { return ""; }
static void printable_text(char *destination, long size, char const *source) { destination[0] = 0; }
static unsigned long posix_server_unix_seconds(void) { return 1000; }
static void network_distributed_ban_details(char const *reason, unsigned long until) { }
/* the kick: the player's machine leaves, and the game's list moves up a slot (slot 1 now names slot 2's machine) */
static boolean network_game_server_kick_machine_of_player(long machine_index, boolean ban)
{
	kicked_machine = machine_index;
	game.players[1] = game.players[2];
	game.player_count--;
	return TRUE;
}
static void server_director_player_removed(long machine_index, char const *why)
{
	removed_machine = machine_index;
	snprintf(removed_why, sizeof(removed_why), "%s", why);
}
#include "under_test.inc"

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	char *words[3] = { "sv_kick", "1", NULL };

	game.player_count = 3;
	game.players[0].machine_index = 4;
	game.players[1].machine_index = 7;   /* the player kicked */
	game.players[2].machine_index = 9;
	CASE("kick")
	{
		CHECK(command_kick("sv_kick 1", 2, words), "kick refused");
		CHECK(kicked_machine == 7, "kicked machine %ld", kicked_machine);
		CHECK(removed_machine == 7 && !strcmp(removed_why, "kicked"), "the leave reason went to machine %ld", removed_machine);
		return 0;
	}
	CASE("ban")
	{
		words[0] = "sv_ban";
		CHECK(command_ban("sv_ban 1", 2, words), "ban refused");
		CHECK(kicked_machine == 7, "banned machine %ld", kicked_machine);
		CHECK(removed_machine == 7 && !strcmp(removed_why, "banned"), "the leave reason went to machine %ld", removed_machine);
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
