/*
AE_GLUE_PROFILES.C

The profiles' glue (ae_glue_profiles.h): what upstream's PC menus do with
profiles (port/linux/game/menu_functions.c), from C with no widget:

- the list: profile_list_read (menu_functions.c :1979): the profiles enumerated
  for no particular player (player_profiles_enumerate_available_to_local_player_index
  NONE, the default profiles left out), those with the valid bit, each read
  with player_profile_get;
- a switch: profile_choose (:2104): player_profile_get, then
  player_ui_set_active_player_profile, and for player 1
  player_ui_remember_player1_profile(TRUE);
- a new profile: pc_menu_profile_edit_begin (:683): player_profile_new(0, name)
  (11 characters at most: its name[11] = 0);
- the controls: profile_setting_field (:317): the controller settings' fields,
  vibration stored as vibration_disabled;
- saving: profile_save_changes (:4815) saves through upstream's one edit
  buffer (player_ui_save_profile), then gives the game's copy of the profile
  the controller settings (player_ui_set_active_player_profile). AE has no
  edit buffer: it reads the file, changes its fields, writes it
  (player_profile_save) and gives the game's copy the same fields. Upstream's
  one-edit-at-a-time rule (settings_without_profile, :554; "PLAYER n IS
  EDITING") is kept from the other side: while a player's upstream profile
  edit is held (its buffer open and their profile edit screen up, as
  menu_functions.c's profile_edit_held_by), AE saves nothing.

A guest has no profile (its index NONE, as upstream's players 2-4 without
one): the game's copy is a new profile's defaults (player_profile_new's), and
player_profile_save never writes an index of NONE.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "interface/player_ui.h"
#include "interface/ui_widget.h"
#include "main/main.h"
#include "networking/network_connection.h"
#include "saved games/player_profile.h"
#include "saved games/saved_game_files.h"
#include "ae_glue_profiles.h"
#include "ae_strings.h"

void platform_log(char const *format, ...);
/* ui_widget.c's (menu_functions.c declares it the same way) */
boolean ui_widget_port_local_player_screen_in(short local_player_index, char const *path_part);

enum
{
	LOCAL_PLAYERS = 4,
	/* player_profile.c's DEFAULT_LOOK_SENSITIVITY (private to it), a new profile's */
	NEW_PROFILE_LOOK_SENSITIVITY = 3,
	/* player_profile.c's: every difficulty of a level (NUMBER_OF_GAME_DIFFICULTY_LEVELS bits) */
	ALL_DIFFICULTIES = 0x0F
};

typedef char ae_profiles_local_players_match[LOCAL_PLAYERS == MAXIMUM_NUMBER_OF_LOCAL_PLAYERS ? 1 : -1];
typedef char ae_profiles_wchar_is_ucs2[sizeof(wchar_t) == 2 ? 1 : -1];

/* the local players playing as guests (this glue's ae_profile_guest; a profile chosen since, by anything, ends it) */
static boolean guests[LOCAL_PLAYERS];

static struct ae_result succeeded(void)
{
	struct ae_result result;

	result.ok = 1;
	result.reason[0] = 0;
	return result;
}

/* a failure, its reason logged with the call */
static struct ae_result failed(const char *call, int reason)
{
	struct ae_result result;

	result.ok = 0;
	snprintf(result.reason, sizeof(result.reason), "%s", ae_string(reason));
	platform_log("ae profiles: %s: %s", call, result.reason);
	return result;
}

/* a profile's saved game file index from the API's int (its 32 bits as they are; -1 NONE) */
static long file_index(int index)
{
	return index == -1 ? NONE : (long)(unsigned int)index;
}

/* the name as UTF-8 (the profile's characters are UCS-2) */
static void name_to_utf8(const wchar_t *name, char *text, size_t size)
{
	size_t used = 0;
	short index;

	for (index = 0; index < MAXIMUM_PLAYER_PROFILE_NAME_LENGTH && name[index]; index++)
	{
		unsigned int c = (unsigned short)name[index];
		char bytes[3];
		size_t length;

		if (c < 0x80)
		{
			bytes[0] = (char)c;
			length = 1;
		}
		else if (c < 0x800)
		{
			bytes[0] = (char)(0xC0 | (c >> 6));
			bytes[1] = (char)(0x80 | (c & 0x3F));
			length = 2;
		}
		else
		{
			bytes[0] = (char)(0xE0 | (c >> 12));
			bytes[1] = (char)(0x80 | ((c >> 6) & 0x3F));
			bytes[2] = (char)(0x80 | (c & 0x3F));
			length = 3;
		}
		if (used + length >= size)
			break;
		memcpy(text + used, bytes, length);
		used += length;
	}
	text[used] = 0;
}

enum { NAME_TOO_LONG = -1, NAME_BAD_CHARACTER = -2 };

/* UTF-8 text as a profile's name (up to 11 characters, as upstream's new profile: the 12th is its end); the
characters' count, NAME_BAD_CHARACTER for one a name can't hold (a control character or tab, a character outside
UCS-2: 4-byte UTF-8, a surrogate, text that is not UTF-8), else NAME_TOO_LONG for more than 11 */
static short name_from_utf8(const char *text, wchar_t *name)
{
	const unsigned char *cursor = (const unsigned char *)text;
	short count = 0;

	while (*cursor)
	{
		unsigned int c;

		if (cursor[0] < 0x80)
			c = *cursor++;
		else if ((cursor[0] & 0xE0) == 0xC0 && (cursor[1] & 0xC0) == 0x80)
		{
			c = ((cursor[0] & 0x1Fu) << 6) | (cursor[1] & 0x3Fu);
			cursor += 2;
			if (c < 0x80)
				return NAME_BAD_CHARACTER;
		}
		else if ((cursor[0] & 0xF0) == 0xE0 && (cursor[1] & 0xC0) == 0x80 && (cursor[2] & 0xC0) == 0x80)
		{
			c = ((cursor[0] & 0x0Fu) << 12) | ((cursor[1] & 0x3Fu) << 6) | (cursor[2] & 0x3Fu);
			cursor += 3;
			if (c < 0x800 || (c >= 0xD800 && c <= 0xDFFF))
				return NAME_BAD_CHARACTER;
		}
		else
			return NAME_BAD_CHARACTER;
		/* (C0 and C1 control characters, tab and delete among them) */
		if (c < 0x20 || (c >= 0x7F && c <= 0x9F))
			return NAME_BAD_CHARACTER;
		if (count >= MAXIMUM_PLAYER_PROFILE_NAME_LENGTH - 1)
			return NAME_TOO_LONG;
		name[count++] = (wchar_t)c;
	}
	name[count] = 0;
	return count;
}

/* the profile's colour as the game draws it (white for none: player_profile_get_rgb_color clamps it), 0xRRGGBBAA */
static unsigned int color_rgba(short color_index)
{
	real_argb_color argb = player_profile_get_argb_color(color_index);

	return ((unsigned int)(argb.rgb.red * 255.0f + 0.5f) << 24) | ((unsigned int)(argb.rgb.green * 255.0f + 0.5f) << 16) |
		((unsigned int)(argb.rgb.blue * 255.0f + 0.5f) << 8) | 0xFFu;
}

/* the player upstream's one profile edit is held by (its buffer open and that player's profile edit screen up:
menu_functions.c's profile_edit_held_by), NONE when nobody's */
static short upstream_edit_holder(void)
{
	short player;

	if (!player_ui_get_edit_player_profile())
		return NONE;
	for (player = 0; player < LOCAL_PLAYERS; player++)
	{
		if (ui_widget_port_local_player_screen_in(player, "\\player_profile_edit\\"))
			return player;
	}
	return NONE;
}

static int local_player_valid(short local_player)
{
	return local_player >= 0 && local_player < LOCAL_PLAYERS;
}

int ae_profile_is_guest(short local_player)
{
	return local_player_valid(local_player) && guests[local_player] &&
		player_ui_get_active_player_profile_index(local_player) == NONE;
}

struct ae_result ae_profiles_list(struct ae_profile_summary *profiles, short maximum, short *count)
{
	long indices[AE_PROFILES_MAXIMUM];
	word found = AE_PROFILES_MAXIMUM;
	word index;

	if (count)
		*count = 0;
	if (!profiles || !count || maximum < 0)
		return failed("list", AE_STR_ERR_PROFILE);
	if (maximum > AE_PROFILES_MAXIMUM)
		maximum = AE_PROFILES_MAXIMUM;
	player_profiles_enumerate_available_to_local_player_index(NONE, &found, indices, FALSE);
	for (index = 0; index < found && *count < maximum; index++)
	{
		struct player_profile profile;
		struct ae_profile_summary *summary = &profiles[*count];

		/* (menu_functions.c profile_list_read: only the valid ones) */
		if (!TEST_FLAG(indices[index], _saved_game_file_index_valid_bit) || !player_profile_get(indices[index], &profile))
			continue;
		memset(summary, 0, sizeof(*summary));
		summary->index = (int)(unsigned int)indices[index];
		name_to_utf8(profile.player_name, summary->name, sizeof(summary->name));
		summary->color = color_rgba(profile.primary_color_index);
		summary->layout = profile.controller_settings.button_preset;
		(*count)++;
	}
	return succeeded();
}

struct ae_result ae_profile_switch(short local_player, int profile_index)
{
	struct player_profile profile;
	long index = file_index(profile_index);

	if (!local_player_valid(local_player))
		return failed("switch", AE_STR_ERR_PROFILE);
	if (!main_menu_is_active())
		return failed("switch", AE_STR_ERR_IN_GAME);
	if (index == NONE || saved_game_file_get_type(index) != _saved_game_file_type_player_profile ||
		!TEST_FLAG(index, _saved_game_file_index_valid_bit) || !player_profile_get(index, &profile))
	{
		return failed("switch", AE_STR_ERR_PROFILE);
	}
	player_ui_set_active_player_profile(local_player, index, &profile);
	guests[local_player] = FALSE;
	if (local_player == 0)
		player_ui_remember_player1_profile(TRUE);
	platform_log("ae profiles: player %d uses %08X", local_player + 1, (unsigned int)profile_index);
	return succeeded();
}

struct ae_result ae_profile_new(const char *name, int *profile_index)
{
	struct ae_profile_summary profiles[AE_PROFILES_MAXIMUM];
	wchar_t wide[MAXIMUM_PLAYER_PROFILE_NAME_LENGTH];
	char trimmed[48];
	short count, index, length;
	long made;

	if (profile_index)
		*profile_index = -1;
	if (!main_menu_is_active())
		return failed("new", AE_STR_ERR_IN_GAME);
	/* (the spaces around it dropped) */
	for (name = name ? name : ""; *name == ' '; name++)
		;
	snprintf(trimmed, sizeof(trimmed), "%s", name);
	for (length = (short)strlen(trimmed); length > 0 && trimmed[length - 1] == ' '; length--)
		trimmed[length - 1] = 0;
	if (!trimmed[0])
		return failed("new", AE_STR_ERR_NAME_EMPTY);
	switch (name_from_utf8(trimmed, wide))
	{
	case NAME_BAD_CHARACTER: return failed("new", AE_STR_ERR_NAME_CHARACTER);
	case NAME_TOO_LONG: return failed("new", AE_STR_ERR_NAME_LONG);
	}
	/* (two profiles of one name: upstream's lists could not tell them apart) */
	if (ae_profiles_list(profiles, AE_PROFILES_MAXIMUM, &count).ok)
	{
		for (index = 0; index < count; index++)
		{
			if (!strcmp(profiles[index].name, trimmed))
				return failed("new", AE_STR_ERR_NAME_TAKEN);
		}
	}
	made = player_profile_new(0, wide);
	if (made == NONE)
		return failed("new", AE_STR_ERR_PROFILE_NEW);
	if (profile_index)
		*profile_index = (int)(unsigned int)made;
	platform_log("ae profiles: new '%s' -> index %08X", trimmed, (unsigned int)made);
	return succeeded();
}

/* a new profile's settings (player_profile.c player_profile_new's), the name "Guest n" */
static void guest_profile(short local_player, struct player_profile *profile)
{
	char name[16];
	short level;

	memset(profile, 0, sizeof(*profile));
	snprintf(name, sizeof(name), ae_string(AE_STR_GUEST_NAME), local_player + 1);
	name_from_utf8(name, profile->player_name);
	profile->primary_color_index = NONE;
	profile->controller_settings.look_sensitivity = NEW_PROFILE_LOOK_SENSITIVITY;
	profile->controller_settings.button_preset = _button_preset_standard;
	profile->controller_settings.joystick_preset = _joystick_preset_standard;
	for (level = 0; level < NUMBER_OF_SINGLE_PLAYER_LEVELS; level++)
		profile->single_player_map_flags[level] = ALL_DIFFICULTIES;
}

struct ae_result ae_profile_guest(short local_player)
{
	struct player_profile profile;

	if (!local_player_valid(local_player))
		return failed("guest", AE_STR_ERR_PROFILE);
	if (!main_menu_is_active())
		return failed("guest", AE_STR_ERR_IN_GAME);
	guest_profile(local_player, &profile);
	player_ui_set_active_player_profile(local_player, NONE, &profile);
	guests[local_player] = TRUE;
	platform_log("ae profiles: player %d guest", local_player + 1);
	return succeeded();
}

/* the player's profile: its index and the game's copy of it; FALSE (the reason logged) for a player with neither a
profile nor a guest's settings */
static boolean active_profile(const char *call, short local_player, long *index, struct player_profile *profile,
	struct ae_result *failure)
{
	if (!local_player_valid(local_player))
	{
		*failure = failed(call, AE_STR_ERR_PROFILE);
		return FALSE;
	}
	*index = player_ui_get_active_player_profile_index(local_player);
	if (*index == NONE && !ae_profile_is_guest(local_player))
	{
		*failure = failed(call, AE_STR_ERR_PROFILE);
		return FALSE;
	}
	player_ui_get_active_player_profile(local_player, profile);
	return TRUE;
}

struct ae_result ae_profile_get_controls(short local_player, struct ae_profile_controls *controls)
{
	struct player_profile profile;
	struct ae_result failure;
	long index;

	if (!controls)
		return failed("controls", AE_STR_ERR_PROFILE);
	if (!active_profile("controls", local_player, &index, &profile, &failure))
		return failure;
	controls->layout = profile.controller_settings.button_preset;
	controls->look_sensitivity = profile.controller_settings.look_sensitivity;
	controls->invert = profile.controller_settings.invert_look ? 1 : 0;
	controls->vibration = profile.controller_settings.vibration_disabled ? 0 : 1;
	return succeeded();
}

static void apply_controls(struct player_profile *profile, struct ae_profile_controls const *controls)
{
	profile->controller_settings.button_preset = (byte)controls->layout;
	profile->controller_settings.look_sensitivity = (byte)controls->look_sensitivity;
	profile->controller_settings.invert_look = controls->invert ? TRUE : FALSE;
	profile->controller_settings.vibration_disabled = controls->vibration ? FALSE : TRUE;
}

/* the profile's file changed by change (read, changed, written: player_profile_save) and the game's copy of it, for
every local player playing with it; FALSE (the reason in failure) when upstream's edit is held or the file can't be
read */
static boolean save_profile(const char *call, long index, void (*change)(struct player_profile *, void const *),
	void const *data, struct ae_result *failure)
{
	struct player_profile profile;
	short holder = upstream_edit_holder();
	short player;

	if (holder != NONE)
	{
		failure->ok = 0;
		snprintf(failure->reason, sizeof(failure->reason), ae_string(AE_STR_ERR_PROFILE_EDITING), holder + 1);
		platform_log("ae profiles: %s: %s", call, failure->reason);
		return FALSE;
	}
	if (!player_profile_get(index, &profile))
	{
		*failure = failed(call, AE_STR_ERR_PROFILE);
		return FALSE;
	}
	change(&profile, data);
	player_profile_save(index, &profile);
	for (player = 0; player < LOCAL_PLAYERS; player++)
	{
		if (player_ui_get_active_player_profile_index(player) == index)
		{
			struct player_profile active;

			player_ui_get_active_player_profile(player, &active);
			change(&active, data);
			player_ui_set_active_player_profile(player, index, &active);
		}
	}
	return TRUE;
}

static void change_controls(struct player_profile *profile, void const *data)
{
	apply_controls(profile, data);
}

/* (the game's copy's colour is read only when the player is added to a game, network_client_manager.c
network_game_client_add_player: a match under way keeps the colour its network player has; upstream's CHANGE COLOR
leaves the copy as it was, so its "from the next game you join" waits for the profile to be read again) */
static void change_color(struct player_profile *profile, void const *data)
{
	profile->primary_color_index = *(short const *)data;
}

struct ae_result ae_profile_set_controls(short local_player, struct ae_profile_controls const *controls)
{
	struct player_profile profile;
	struct ae_result result;
	long index;

	if (!controls || controls->layout < 0 || controls->layout >= NUMBER_OF_BUTTON_PRESETS ||
		controls->look_sensitivity < 0 || controls->look_sensitivity >= NUMBER_OF_LOOK_SENSITIVITY_SETTINGS)
	{
		return failed("controls", AE_STR_ERR_SETTING);
	}
	if (!active_profile("controls", local_player, &index, &profile, &result))
		return result;
	if (index == NONE)
	{
		/* a guest: the game's copy only (player_profile_save would write nothing for NONE: not even asked) */
		apply_controls(&profile, controls);
		player_ui_set_active_player_profile(local_player, NONE, &profile);
		result = succeeded();
		snprintf(result.reason, sizeof(result.reason), "%s", ae_string(AE_STR_ERR_GUEST));
		platform_log("ae profiles: guest controls not saved");
		return result;
	}
	if (!save_profile("controls", index, change_controls, controls, &result))
		return result;
	platform_log("ae profiles: controls %d %d %d %d saved", controls->layout, controls->look_sensitivity,
		controls->invert ? 1 : 0, controls->vibration ? 1 : 0);
	return succeeded();
}

struct ae_result ae_profile_set_color(short local_player, short color)
{
	struct player_profile profile;
	struct ae_result result;
	long index;

	if (color < 0 || color >= (short)player_profile_number_of_available_primary_colors())
		return failed("colour", AE_STR_ERR_SETTING);
	if (!active_profile("colour", local_player, &index, &profile, &result))
		return result;
	if (index == NONE)
	{
		profile.primary_color_index = color;
		player_ui_set_active_player_profile(local_player, NONE, &profile);
		result = succeeded();
		snprintf(result.reason, sizeof(result.reason), "%s", ae_string(AE_STR_ERR_GUEST));
		platform_log("ae profiles: guest colour not saved");
		return result;
	}
	if (!save_profile("colour", index, change_color, &color, &result))
		return result;
	platform_log("ae profiles: colour %d saved", color);
	return succeeded();
}
