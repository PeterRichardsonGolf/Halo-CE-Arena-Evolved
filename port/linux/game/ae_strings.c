/* ae_strings.c: Arena Evolved menus, the texts (see ae_strings.h). No engine includes. */

#include <stddef.h>
#include "ae_strings.h"

static const char *const strings[AE_NUMBER_OF_STRINGS] =
{
	/* prompts */
	"Select",
	"Back",
	"Cancel",
	"Done",
	"Save",
	"Type",
	"All values",
	"Reset row",
	"Search",
	"Pages",
	"Tabs",
	"Change",
	"Profile",
	"Quit",
	"Switch",
	"Backspace",
	"Space",
	"Move caret",
	"Shift",
	/* key caps (spec 4.10) */
	"Enter",
	"Esc",
	"Ctrl+F",
	"R",
	"Q",
	"E",
	"PgUp",
	"PgDn",
	"Tab",
	"Ctrl+S",
	"Ctrl+Shift+S",
	"OPTIONS",
	"CREATE",
	/* rows, lists, help */
	"%d more",
	"VALUES",
	"Default: %s",
	"Changed from %s",
	"Reset",
	"AE",
	"HOST",
	/* text entry */
	"%d / %d",
	"SHIFT",
	"#+=",
	"ABC",
	"SPACE",
	"DONE",
	/* dialogs */
	"Reverting in %d s",
	"KEEP",
	"REVERT",
	"OK",
	"CANCEL",
	"LEAVE",
	"STAY",
	/* roster */
	"Press",
	"to add a player (split screen)",
	"Connect a controller and press START",
	"guest (not saved)",
	"Controller %d",
	"Keyboard & mouse",
	"disconnected",
	"away",
	"PLAYER %d",
	/* the old menu off placeholder (spec 8) */
	"HALO: COMBAT EVOLVED",
	"ARENA EVOLVED",
	"CAMPAIGN",
	"CUSTOM GAMES",
	"SERVER BROWSER",
	"SETTINGS",
	/* glue reasons (Tasks 12-14) */
	"Not hosting a game",
	"Lobby full",
	"No game found on the LAN",
	"Host is on version %d, you're on %d",
	"Map not installed: %s",
	"Game type not found: %s",
	"Could not host the game",
	"Profile not found",
	"A guest's settings are not saved",
	"Not while a game is running",
};

const char *ae_string(int id)
{
	return id >= 0 && id < AE_NUMBER_OF_STRINGS && strings[id] ? strings[id] : "";
}
