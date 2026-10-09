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
	"Host is on version %d and needs to update to %d or newer",
	"Map not installed: %s",
	"Game type not found: %s",
	"Could not host the game",
	"Profile not found",
	"A guest's settings are not saved",
	"Not while a game is running",
	"Internet play is off",
	"That is not an invite",
	"That game can't be joined from this version",
	"The host did not answer",
	/* (Task 14: the profiles; a guest's name is its profile name: 11 characters at most) */
	"Player %d is editing a profile",
	"Enter a name",
	"Names are up to 11 characters",
	"That name is already used",
	"Could not make the profile",
	"Not a valid setting",
	"Guest %d",
	"A name can't use that character",
};

const char *ae_string(int id)
{
	return id >= 0 && id < AE_NUMBER_OF_STRINGS && strings[id] ? strings[id] : "";
}
