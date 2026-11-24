/* AE_FOOTER_CANON_RULES.C: see ae_footer_canon_rules.h */

#include <math.h>
#include <string.h>
#include "ae_footer_canon_rules.h"

int ae_canon_b_label_allowed(const char *screen, const char *label)
{
	if (!label)
		return 0;
	if (!strcmp(label, "Back") || !strcmp(label, "Cancel") || !strcmp(label, "Leave") || !strcmp(label, "Close"))
		return 1;
	return screen && ((!strcmp(screen, "INVITE") && !strcmp(label, "Decline")) ||
		(!strcmp(screen, "MAIN MENU") && !strcmp(label, "Quit")) || (!strcmp(screen, "PAUSE") && !strcmp(label, "Resume")));
}

int ae_canon_x_exception(const char *screen, const char *label)
{
	return screen && label && ((!strcmp(screen, "SETTINGS group page") && !strcmp(label, "Player")) ||
		(!strcmp(screen, "CROSSHAIR") && !strcmp(label, "Background")));
}

int ae_canon_footer_check(const char *screen, struct ae_canon_prompt const *prompts, int count)
{
	int index, other, has_b = 0;

	for (index = 0; index < count; index++)
	{
		if (!prompts[index].label || !prompts[index].label[0])
			return 4;
		for (other = index + 1; other < count; other++)
			if (prompts[other].button == prompts[index].button)
				return 1;
		if (prompts[index].button == AE_CANON_B)
		{
			has_b = 1;
			if (!ae_canon_b_label_allowed(screen, prompts[index].label))
				return 2;
		}
	}
	return has_b ? 0 : 3;
}

struct ae_canon_footer const ae_canon_footers[] =
{
	{ "SERVER BROWSER", 6, { { AE_CANON_A, "Join" }, { AE_CANON_Y, "Refresh" }, { AE_CANON_LB_RB, "Filter" },
		{ AE_CANON_LT_RT, "Page" }, { AE_CANON_STICK, "AE games" }, { AE_CANON_B, "Back" } } },
	{ "JOIN LAN GAMES", 6, { { AE_CANON_A, "Join" }, { AE_CANON_Y, "Search again" }, { AE_CANON_LB_RB, "Filter" },
		{ AE_CANON_LT_RT, "Page" }, { AE_CANON_STICK, "AE games" }, { AE_CANON_B, "Back" } } },
	{ "SERVER BROWSER joining", 1, { { AE_CANON_B, "Leave" } } },
	{ "CHANGE GAMETYPE own", 6, { { AE_CANON_A, "Select" }, { AE_CANON_B, "Back" }, { AE_CANON_X, "Manage" },
		{ AE_CANON_Y, "Search" }, { AE_CANON_LB_RB, "Filter" }, { AE_CANON_LT_RT, "Folder" } } },
	{ "CHANGE GAMETYPE built-in", 6, { { AE_CANON_A, "Select" }, { AE_CANON_B, "Back" }, { AE_CANON_X, "Folders" },
		{ AE_CANON_Y, "Search" }, { AE_CANON_LB_RB, "Filter" }, { AE_CANON_LT_RT, "Folder" } } },
	{ "CHANGE GAMETYPE searching", 3, { { AE_CANON_A, "To the list" }, { AE_CANON_B, "Cancel" },
		{ AE_CANON_LB_RB, "Filter" } } },
	{ "GAME OPTIONS", 7, { { AE_CANON_A, "All values" }, { AE_CANON_B, "Back" }, { AE_CANON_DPAD, "Change" },
		{ AE_CANON_LB_RB, "Filter" }, { AE_CANON_LT_RT, "Group" }, { AE_CANON_X, "Reset" }, { AE_CANON_Y, "Save as new" } } },
	{ "SETTINGS", 4, { { AE_CANON_A, "Open" }, { AE_CANON_B, "Back" }, { AE_CANON_LB_RB, "Player" },
		{ AE_CANON_Y, "Find" } } },
	{ "SETTINGS group page", 5, { { AE_CANON_A, "All values" }, { AE_CANON_B, "Back" }, { AE_CANON_LB_RB, "Section" },
		{ AE_CANON_Y, "Restore defaults" }, { AE_CANON_X, "Player" } } },
	{ "CROSSHAIR", 5, { { AE_CANON_A, "All values" }, { AE_CANON_B, "Back" }, { AE_CANON_LB_RB, "Tab" },
		{ AE_CANON_Y, "Share" }, { AE_CANON_X, "Background" } } },
	{ "PLAYLISTS", 4, { { AE_CANON_A, "Use" }, { AE_CANON_X, "Manage" }, { AE_CANON_B, "Back" }, { AE_CANON_Y, "New playlist" } } },
	{ "PLAYLIST games", 5, { { AE_CANON_A, "Change" }, { AE_CANON_DPAD, "Weight" }, { AE_CANON_X, "Remove" },
		{ AE_CANON_B, "Back" }, { AE_CANON_Y, "Use" } } },
	{ "MAIN MENU", 3, { { AE_CANON_A, "Select" }, { AE_CANON_B, "Quit" }, { AE_CANON_X, "Party" } } },
	{ "PAUSE", 2, { { AE_CANON_A, "Select" }, { AE_CANON_B, "Resume" } } },
	{ "INVITE", 2, { { AE_CANON_A, "Select" }, { AE_CANON_B, "Decline" } } },
};
const int ae_canon_footer_count = (int)(sizeof(ae_canon_footers) / sizeof(ae_canon_footers[0]));
