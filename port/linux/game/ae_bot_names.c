/*
AE_BOT_NAMES.C

The bots' names from bot_names.txt in the save root (or its private folder), one a line (ae_bot_names_rules.h): the
host's bots take them in order before bots.c's built-in Marines. Missing or empty: the built-in list as ever. Read
when the first bot is named and again, at most every two seconds, while no game is running (a lobby), never in a
game: the names in a game do not change under it. The clip to the player name's length happens here; a network
game's "BOT " tag (ae_lan_bots_name) clips the rest.
*/

#include "cseries.h"
#include "cseries_windows.h"
#include "game/game_engine.h"

#include "ae_bot_names_rules.h"
#include "../src/ae_private_files.h"

#include <stdio.h>
#include <string.h>

void platform_log(char const *format, ...);

static struct
{
	int read, logged;
	unsigned long when;
	int count;
	char names[AE_BOT_NAMES_MAXIMUM][AE_BOT_NAMES_LENGTH + 1];
} ae_bot_names_globals;

static void load(void)
{
	static char text[16384];
	long read;
	int count;

	ae_bot_names_globals.read = 1;
	ae_bot_names_globals.when = system_milliseconds();
	read = ae_save_root_file_read("bot_names.txt", text, sizeof(text));
	count = read > 0 ? ae_bot_names_parse(text, ae_bot_names_globals.names, AE_BOT_NAMES_MAXIMUM) : 0;
	if (count != ae_bot_names_globals.count || !ae_bot_names_globals.logged)
		platform_log("bot names: bot_names.txt %s: %d names", read < 0 ? "not there" : read == 0 ? "empty" : "read", count);
	ae_bot_names_globals.count = count;
	ae_bot_names_globals.logged = 1;
}

int ae_bot_names_name(long bot_index, char *name, int size)
{
	if (!ae_bot_names_globals.read || (!game_engine_running() &&
		system_milliseconds() - ae_bot_names_globals.when >= 2000))
	{
		load();
	}
	if (ae_bot_names_globals.count <= 0)
		return 0;
	ae_bot_names_pick(ae_bot_names_globals.names, ae_bot_names_globals.count, bot_index, name, size);
	return name[0] != 0;
}
