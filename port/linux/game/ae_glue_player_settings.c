/*
AE_GLUE_PLAYER_SETTINGS.C

Each local player's own settings, their profile's ae.toml (ae_glue_player_settings.h). The file is beside the
profile's blam.sav (saved_game_file_get_path_to_enclosing_directory: an Xbox path the port's fopen maps into the save
root); deleting the profile deletes its folder, the file with it.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "interface/player_ui.h"
#include "saved games/saved_game_files.h"
#include "../src/port_config.h"
#include "../src/ae_config_players.h"
#include "ae_glue_player_settings.h"
#include "ae_glue_profiles.h"
#include "ae_player_settings.h"

void platform_log(char const *format, ...);

enum { LOCAL_PLAYERS = 4, FILE_TEXT = 16384, SETTLE_MS = 500 };
/* (xbox_files.c's: a file put in another's place in one step) */
int platform_replace_file(char const *path, char const *new_path);
typedef char ae_player_settings_players_match[LOCAL_PLAYERS == AE_CONFIG_PLAYERS ? 1 : -1];
typedef char ae_player_settings_none_match[AE_PLAYER_SETTINGS_NO_PROFILE == NONE ? 1 : -1];

static struct
{
	int started;
	/* the profile each player's values are of now (NONE: a guest, none read; AE_PLAYER_SETTINGS_UNREAD before the
	first look), and that profile's file as read. (A profile's index is negative: ae_player_settings_profile_has_file) */
	long profile[LOCAL_PLAYERS];
	/* the guest's start (ae_profile_guest_generation): a new guest has none of the last one's values */
	int guest[LOCAL_PLAYERS];
	/* a change not written yet, and when it was made (written SETTLE_MS after the last: a slider's steps one write) */
	int dirty[LOCAL_PLAYERS];
	unsigned long changed_at[LOCAL_PLAYERS];
	struct ae_player_settings file[LOCAL_PLAYERS];
} players;

/* the file's Xbox path: the profile's folder + ae.toml; 0 when the profile has none */
static int file_path(long profile, char *path, int size)
{
	char directory[256];

	directory[0] = 0;
	if (profile == NONE || !saved_game_file_get_path_to_enclosing_directory(profile, directory) || !directory[0])
		return 0;
	return snprintf(path, (size_t)size, "%s%s%s", directory,
		directory[strlen(directory) - 1] == '\\' ? "" : "\\", AE_PLAYER_SETTINGS_FILE) < size;
}

static int read_file(long profile, char *text, int size)
{
	char path[300];
	FILE *file;
	size_t length;

	if (!file_path(profile, path, sizeof(path)) || !(file = fopen(path, "rb")))
		return 0;
	length = fread(text, 1, (size_t)size - 1, file);
	fclose(file);
	text[length] = 0;
	return 1;
}

static int write_file(long profile, struct ae_player_settings const *settings)
{
	static char text[FILE_TEXT];
	char path[300], new_path[310];
	FILE *file;
	int written;

	if (!file_path(profile, path, sizeof(path)) ||
		!ae_player_settings_write(settings, ae_config_player_is_string, text, sizeof(text)))
		return 0;
	/* (a whole file or the old one: written beside it, then put in its place in one step, as arena_gametypes.c's
	record) */
	snprintf(new_path, sizeof(new_path), "%s.new", path);
	if (!(file = fopen(new_path, "wb")))
		return 0;
	written = fwrite(text, 1, strlen(text), file) == strlen(text);
	written = fclose(file) == 0 && written;
	return written && platform_replace_file(new_path, path);
}

/* a player's file written now if a change waits (the settle after a slider's steps, a KEEP, leaving, a profile
switch) */
static void flush(short player)
{
	if (!players.dirty[player])
		return;
	players.dirty[player] = 0;
	if (!ae_player_settings_profile_has_file(players.profile[player]))
		return;
	if (write_file(players.profile[player], &players.file[player]))
		platform_log("ae player settings: player %d %08lX written (%d values)", player + 1,
			(unsigned long)players.profile[player], players.file[player].count);
	else
		platform_log("ae player settings: player %d: %s could not be written", player + 1, AE_PLAYER_SETTINGS_FILE);
}

void ae_player_settings_flush(void)
{
	short player;

	for (player = 0; player < LOCAL_PLAYERS; player++)
		flush(player);
}

/* the profile's values into the player's memory: read, seeded the first time (and written) */
static void load(short player, long profile)
{
	static char text[FILE_TEXT];
	static const char *names[AE_PLAYER_SETTINGS_MAXIMUM];
	struct ae_player_settings *file = &players.file[player];
	int found, index, count = ae_config_player_setting_count();

	flush(player);
	ae_config_player_clear(player);
	ae_player_settings_clear(file);
	players.profile[player] = profile;
	if (profile == NONE)
	{
		/* (a guest of player 2-4: player 1's values copied, as its profile is, guest_profile; memory only) */
		static char value[256];
		int copied = 0;

		for (index = 0; player > 0 && index < count; index++)
		{
			const char *name = ae_config_player_setting_name(index);

			if (ae_config_player_has(0, name) && ae_config_player_text(0, name, value, sizeof(value)))
				copied += ae_config_player_set(player, name, value);
		}
		if (copied)
			platform_log("ae player settings: player %d guest copies player 1's (%d values)", player + 1, copied);
		return;
	}
	found = read_file(profile, text, sizeof(text));
	if (found)
		ae_player_settings_parse(text, file);
	if (ae_player_settings_needs_seed(found, file->version))
	{
		int seeded;

		for (index = 0; index < count && index < AE_PLAYER_SETTINGS_MAXIMUM; index++)
			names[index] = ae_config_player_setting_name(index);
		seeded = ae_player_settings_seed(file, names, index, ae_config_player_seed_text);
		platform_log("ae player settings: player %d %08lX seeded (%d values from config.toml)%s", player + 1,
			(unsigned long)profile, seeded, write_file(profile, file) ? "" : ": not written");
	}
	for (index = 0; index < file->count; index++)
	{
		if (ae_config_player_setting(file->settings[index].name))
			ae_config_player_set(player, file->settings[index].name, file->settings[index].value);
	}
	platform_log("ae player settings: player %d %08lX read (%d values)", player + 1, (unsigned long)profile,
		file->count);
}

/* config_write of a per-player setting (the old menus' rows, the MCC mouse's seed): the player's the game worked for */
static void written(int player, const char *name, const char *value)
{
	if (player < 0 || player >= LOCAL_PLAYERS)
		return;
	ae_player_setting_put((short)player, name, value);
}

void ae_player_settings_update(void)
{
	short player;

	if (!players.started)
	{
		players.started = 1;
		for (player = 0; player < LOCAL_PLAYERS; player++)
			players.profile[player] = AE_PLAYER_SETTINGS_UNREAD;
		ae_config_player_set_writer(written);
	}
	for (player = 0; player < LOCAL_PLAYERS; player++)
	{
		long profile = player_ui_get_active_player_profile_index(player);
		int guest = ae_profile_guest_generation(player);

		if (profile != players.profile[player] || (profile == NONE && guest != players.guest[player]))
		{
			players.guest[player] = guest;
			load(player, profile);
		}
		else if (players.dirty[player] && system_milliseconds() - players.changed_at[player] >= SETTLE_MS)
			flush(player);
	}
}

/* player 1 on their profile before their values are read or changed, and that profile's values in memory: AE's
menus right after a launch, a lobby left or a game's end (the game's teardown takes every local player's profile:
player_ui_clear_multiplayer_joins), a pause's SETTINGS (which takes player 1's as it opens: its rows read in the same
frame). Player 1 takes the profile used last, as the lobby and CAMPAIGN do (a guest stays a guest; players 2-4 choose
their own) */
static void player_profile_taken(short local_player)
{
	if (local_player == 0)
		ae_profile_player1_ensure();
	ae_player_settings_update();
}

int ae_player_setting_get(short local_player, const char *name, char *text, int size)
{
	if (local_player >= 0 && local_player < LOCAL_PLAYERS && ae_config_player_setting(name))
		player_profile_taken(local_player);
	return ae_config_player_text(local_player, name, text, (size_t)size);
}

int ae_player_setting_put(short local_player, const char *name, const char *value)
{
	long profile;

	if (local_player < 0 || local_player >= LOCAL_PLAYERS || !ae_config_player_setting(name))
		return 0;
	player_profile_taken(local_player);
	if (!ae_config_player_set(local_player, name, value))
		return 0;
	profile = players.profile[local_player];
	platform_log("ae player settings: player %d set %s %s", local_player + 1, name, value ? value : "(none)");
	/* (a guest's, or with no profile on this PC at all: memory only) */
	if (!ae_player_settings_profile_has_file(profile))
		return 1;
	if (value)
		ae_player_settings_put(&players.file[local_player], name, value);
	else
	{
		/* (none of their own: the file without it) */
		struct ae_player_settings *file = &players.file[local_player];
		int index;

		for (index = 0; index < file->count; index++)
		{
			if (!strcmp(file->settings[index].name, name))
			{
				file->settings[index] = file->settings[file->count - 1];
				file->count--;
				break;
			}
		}
	}
	/* (written once it settles, or at once on a KEEP, leaving SETTINGS or a profile switch: ae_player_settings_flush) */
	players.dirty[local_player] = 1;
	players.changed_at[local_player] = system_milliseconds();
	return 1;
}

int ae_player_setting_default(const char *name, char *text, int size)
{
	return config_default(name, text, (size_t)size);
}

int ae_player_setting_is_own(const char *name)
{
	return ae_config_player_setting(name);
}

/* ---------- the PC menus */

static short menu_player;

void ae_player_settings_menu_player(short local_player)
{
	menu_player = local_player >= 0 && local_player < LOCAL_PLAYERS ? local_player : 0;
}

int ae_player_settings_menu_text(const char *name, char *text, unsigned int size)
{
	if (ae_config_player_setting(name))
		return ae_player_setting_get(menu_player, name, text, (int)size);
	return config_text(name, text, size);
}

int ae_player_settings_menu_write(const char *name, const char *value)
{
	if (ae_config_player_setting(name))
		return ae_player_setting_put(menu_player, name, value);
	return config_write(name, value);
}
