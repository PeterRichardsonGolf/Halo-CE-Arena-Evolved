/*
CAPTURE_GAME.C

What screenshots and recording (port/linux/src/capture.c) need to know of
the game: whether it is being played, for debug.record_seconds, and the
map's name, for the files' names. The platform layer cannot see the game's
types, so it asks here (this file is compiled as the game's own sources
are).
*/

#include "cseries.h"
#include "game/game.h"
#include "cutscene/cinematics.h"
#include "bink/bink_playback.h"
#include "interface/ui_widget.h"
#include "scenario/scenario.h"
#include "tag_files/tag_files.h"

/* a map other than the main menu's is up and no movie is over it (tests
cinematic_globals first, as touch_game.c does: the game's clock exists
before it, and the picture is presented before either) */
int capture_game_playing(void)
{
	return cinematic_globals && game_in_progress() && !main_menu_is_active() && !bink_playback_in_progress();
}

/* the map's name, without its folders ("bloodgulch"); empty at the main
menu and before a map */
void capture_game_map_name(char *name, int size)
{
	char const *path;
	int length = 0;

	name[0] = 0;
	if (!cinematic_globals || global_scenario_index == NONE || main_menu_is_active() || size <= 1)
		return;
	path = tag_name_strip_path(tag_get_name(global_scenario_index));
	/* (letters, digits, - and _ only: it goes in a file's name) */
	for (; path && *path && length < size - 1; path++)
	{
		char c = *path;

		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_')
			name[length++] = c;
	}
	name[length] = 0;
}
