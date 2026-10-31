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
struct ae_profile_controls { short layout; short look_sensitivity; int invert; int vibration;
	/* (0.3.7 SETTINGS) the stick layout (player_profile.h's _joystick_preset_*), INVERT FLIGHT, AUTO-CENTER LOOK */
	short stick; int invert_flight; int autocenter;
	/* (0.3.7 SETTINGS phase B) IN-GAME HELP: the campaign's hints (stored as ingame_help_disabled) */
	int help; };

/* the profiles on this PC (not the game's two default profiles), in the save files' order; up to maximum */
struct ae_result ae_profiles_list(struct ae_profile_summary *profiles, short maximum, short *count);
/* the local player plays with that profile from now on (player 1's is also the one remembered, as upstream's
profile list's choice); at the menus only */
struct ae_result ae_profile_switch(short local_player, int profile_index);
/* a new profile with that name (1-11 characters, not another profile's), as upstream's (player_profile_new):
its index; at the menus only. Nobody plays with it until a switch */
struct ae_result ae_profile_new(const char *name, int *profile_index);
/* player 1's profile when none is active (nothing chose one since the game started: only CAMPAIGN and a game do):
the one used last, else the first on this PC, read into profile; its index, NONE for none on this PC */
struct player_profile;
long ae_profile_player1_fallback(struct player_profile *profile);
/* player 1 takes that profile when none is active and it isn't a guest (before SETTINGS edits it, and before a
lobby adds player 1, whose name and colour the game reads from it) */
void ae_profile_player1_ensure(void);
/* plays without a profile: settings apply to this session only (never saved) */
struct ae_result ae_profile_guest(short local_player);
int ae_profile_is_guest(short local_player);
/* how many times the player became a guest: a new guest in the same place counts one more */
int ae_profile_guest_generation(short local_player);
/* the player's controls as the game has them now (a guest's included) */
struct ae_result ae_profile_get_controls(short local_player, struct ae_profile_controls *controls);
/* saved to that player's own profile at once (no shared edit buffer, no "PLAYER n IS EDITING": M7's rule); a
guest's apply to the session and return ok with reason AE_STR_ERR_GUEST (a note, not a failure) */
struct ae_result ae_profile_set_controls(short local_player, struct ae_profile_controls const *controls);
/* saved at once; the player's colour changes from the next game they join, as upstream's CHANGE COLOR says: the
game's copy is updated now, but it is read only when a player is added to a game, so a match under way (and the
lobby the player is in) keeps the colour the player joined with */
struct ae_result ae_profile_set_color(short local_player, short color);

/* (the 0.3.7 menus' PROFILES: any profile on this PC, by its index, whoever plays with it) */
/* its controls and colour (the colour index; -1: none, the game's random one) as its file has them */
struct ae_result ae_profile_read(int profile_index, struct ae_profile_controls *controls, short *color);
/* saved to its file at once, and to the game's copy for each local player playing with it (as the calls above) */
struct ae_result ae_profile_save_controls(int profile_index, struct ae_profile_controls const *controls);
struct ae_result ae_profile_save_color(int profile_index, short color);
/* the colours a profile can have (the game's primary colours), and one as the game draws it (0xRRGGBBAA; -1: white) */
short ae_profile_color_count(void);
unsigned int ae_profile_color_rgba(short color);
/* its file deleted; refused while a local player plays with it; at the menus only */
struct ae_result ae_profile_delete(int profile_index);

/* every profile's button layout moved to 0.5.0's numbers, once a save folder (ae_layout_numbers.h); the profile
glue calls it before its first look at the profiles */
void ae_profiles_layouts_move(void);

#endif
