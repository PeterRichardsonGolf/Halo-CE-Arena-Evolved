/*
AE_GLUE_PLAYER_SETTINGS.C

Each local player's own settings, their profile's ae.toml (ae_glue_player_settings.h). The file is beside the
profile's blam.sav (saved_game_file_get_path_to_enclosing_directory: an Xbox path the port's fopen maps into the save
root); deleting the profile deletes its folder, the file with it.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "interface/player_ui.h"
#include "saved games/saved_game_files.h"
#include "../src/port_config.h"
#include "../src/ae_config_players.h"
#include "ae_glue_player_settings.h"
#include "ae_player_settings.h"

void platform_log(char const *format, ...);

enum { LOCAL_PLAYERS = 4, FILE_TEXT = 16384 };
typedef char ae_player_settings_players_match[LOCAL_PLAYERS == AE_CONFIG_PLAYERS ? 1 : -1];

static struct
{
	int started;
	/* the profile each player's values are of now (NONE: a guest, none read), and that profile's file as read */
	long profile[LOCAL_PLAYERS];
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
	char path[300];
	FILE *file;
	int written;

	if (!file_path(profile, path, sizeof(path)) ||
		!ae_player_settings_write(settings, ae_config_player_is_string, text, sizeof(text)))
		return 0;
	if (!(file = fopen(path, "wb")))
		return 0;
	written = fwrite(text, 1, strlen(text), file) == strlen(text);
	written = fclose(file) == 0 && written;
	return written;
}

/* the profile's values into the player's memory: read, seeded the first time (and written) */
static void load(short player, long profile)
{
	static char text[FILE_TEXT];
	static const char *names[AE_PLAYER_SETTINGS_MAXIMUM];
	struct ae_player_settings *file = &players.file[player];
	int found, index, count = ae_config_player_setting_count();

	ae_config_player_clear(player);
	ae_player_settings_clear(file);
	players.profile[player] = profile;
	if (profile == NONE)
		return;
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
			players.profile[player] = -2;
		ae_config_player_set_writer(written);
	}
	for (player = 0; player < LOCAL_PLAYERS; player++)
	{
		long profile = player_ui_get_active_player_profile_index(player);

		if (profile != players.profile[player])
			load(player, profile);
	}
}

int ae_player_setting_get(short local_player, const char *name, char *text, int size)
{
	return ae_config_player_text(local_player, name, text, (size_t)size);
}

int ae_player_setting_put(short local_player, const char *name, const char *value)
{
	long profile;

	if (local_player < 0 || local_player >= LOCAL_PLAYERS || !ae_config_player_setting(name))
		return 0;
	ae_player_settings_update();
	if (!ae_config_player_set(local_player, name, value))
		return 0;
	profile = players.profile[local_player];
	platform_log("ae player settings: player %d set %s %s", local_player + 1, name, value ? value : "(none)");
	if (profile == NONE || profile < 0)
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
	if (!write_file(profile, &players.file[local_player]))
		platform_log("ae player settings: player %d: %s could not be written", local_player + 1,
			AE_PLAYER_SETTINGS_FILE);
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
