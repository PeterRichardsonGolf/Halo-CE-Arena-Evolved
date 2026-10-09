/*
AE_GLUE_PROFILES.H

The profiles' glue (ae_glue_profiles.c): the player profiles on this PC
listed, a local player's switched, a new one made, a guest (no profile:
nothing saved), and a player's controls and colour read and saved, from C
with no widget. Every call returns an ae_result (ae_result.h): ok, or the
reason (ae_strings.c), also logged "ae profiles: <call>: <reason>".

Each save goes straight to the player's own profile file (read from disk,
the fields changed, written back) and into the game's copy of it for every
local player using it: there is no shared edit buffer and no "PLAYER n IS
EDITING" lock of AE's own (the master plan's M7). While upstream's SETTINGS
holds its one profile edit (a player's profile edit screen open: in a match,
or the PC menus' Settings), AE's saves wait for it: they fail with "Player n
is editing a profile", so the two never write over each other.

Local players are 0-3 (logged 1-based); a profile is its saved game file
index (player_profile.h). Include after cseries.h.
*/

#ifndef __AE_GLUE_PROFILES_H
#define __AE_GLUE_PROFILES_H

#include "ae_result.h"

enum { AE_PROFILES_MAXIMUM = 64 };

/* a profile as a list shows it: its index, its name (UTF-8), its colour (0xRRGGBBAA, the profile's colour as the
game draws it: player_profile_get_argb_color; white when it has none, the game's random colour) and its controller
layout (the button preset: player_profile.h's _button_preset_*) */
struct ae_profile_summary { int index; char name[48]; unsigned int color; short layout; };
/* a player's controls: layout (the button preset, 0 to NUMBER_OF_BUTTON_PRESETS - 1), look sensitivity (the
profile's own step, 0-9: the game's 1-10), invert (look), vibration (1 on) */
struct ae_profile_controls { short layout; short look_sensitivity; int invert; int vibration; };

/* the profiles on this PC (not the game's two default profiles), in the save files' order; up to maximum */
struct ae_result ae_profiles_list(struct ae_profile_summary *profiles, short maximum, short *count);
/* the local player plays with that profile from now on (player 1's is also the one remembered, as upstream's
profile list's choice); at the menus only */
struct ae_result ae_profile_switch(short local_player, int profile_index);
/* a new profile with that name (1-11 characters, not another profile's), as upstream's (player_profile_new):
its index; at the menus only. Nobody plays with it until a switch */
struct ae_result ae_profile_new(const char *name, int *profile_index);
/* plays without a profile: settings apply to this session only (never saved) */
struct ae_result ae_profile_guest(short local_player);
int ae_profile_is_guest(short local_player);
/* the player's controls as the game has them now (a guest's included) */
struct ae_result ae_profile_get_controls(short local_player, struct ae_profile_controls *controls);
/* saved to that player's own profile at once (no shared edit buffer, no "PLAYER n IS EDITING": M7's rule); a
guest's apply to the session and return ok with reason AE_STR_ERR_GUEST (a note, not a failure) */
struct ae_result ae_profile_set_controls(short local_player, struct ae_profile_controls const *controls);
/* saved at once; the player's colour changes from the next game they join, as upstream's CHANGE COLOR says: the
game's copy is updated now, but it is read only when a player is added to a game, so a match under way (and the
lobby the player is in) keeps the colour the player joined with */
struct ae_result ae_profile_set_color(short local_player, short color);

#endif
