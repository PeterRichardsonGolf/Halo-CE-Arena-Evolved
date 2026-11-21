/*
AE_SAVED_GAME_FILE.C

A level's saved game file in the profile folder. See ae_saved_game_file.h.
*/

#include <stdio.h>
#include <string.h>
#include <xtl.h>
#include "halo_port_capacity.h"

#include "ae_saved_game_file.h"
#include "ae_campaign_save_rules.h"
#include "mcc_workshop.h"

/* the scenario loaded (scenario.h, tag_files.h) */
extern long global_scenario_index;
char *tag_get_name(long tag_index);
void platform_log(char const *format, ...);
/* player 1's profile folder, ending in its separator (player_ui.h) */
unsigned char player_ui_get_path_to_local_player_profile_directory(short local_player_index, char *path);

static char asked_level[256];

static const char *scenario_loaded(void)
{
	return global_scenario_index != -1 ? tag_get_name(global_scenario_index) : NULL;
}

static int mcc_maps_name(const char *name)
{
	static const char prefix[] = "mcc_maps";
	size_t index;

	if (!name)
		return 0;
	for (index = 0; index < sizeof(prefix) - 1; index++)
	{
		unsigned char c = (unsigned char)name[index];

		if (c >= 'A' && c <= 'Z')
			c = (unsigned char)(c + 'a' - 'A');
		if (c != (unsigned char)prefix[index])
			return 0;
	}
	return name[index] == '\\' || name[index] == '/';
}

void ae_saved_game_file_level(const char *level)
{
	snprintf(asked_level, sizeof(asked_level), "%s", level ? level : "");
}

/* 0 savegame.bin, 1 a package campaign's own (file), -1 none */
static int level_file(const char *level, char file[AE_CAMPAIGN_SAVE_FILE_SIZE])
{
	char package[64];

	file[0] = 0;
	if (!level || !mcc_maps_name(level))
		return 0;
	if (!mcc_workshop_package_folder(level, package) ||
		!ae_campaign_save_file_name(package, file, AE_CAMPAIGN_SAVE_FILE_SIZE))
		return -1;
	return 1;
}

const char *ae_saved_game_file(void)
{
	static char file[AE_CAMPAIGN_SAVE_FILE_SIZE];
	const char *level = asked_level[0] ? asked_level : scenario_loaded();
	int kind;

	kind = level_file(level, file);
	if (kind < 0)
		return NULL;
	return kind ? file : "savegame.bin";
}

int ae_saved_game_file_readable(void)
{
	const char *level = asked_level[0] ? asked_level : scenario_loaded();
	char file[AE_CAMPAIGN_SAVE_FILE_SIZE], directory[512], path[600];
	int kind = level_file(level, file);

	if (kind <= 0)
		return kind == 0;
	directory[0] = 0;
	if (!player_ui_get_path_to_local_player_profile_directory(0, directory))
		return 0;
	snprintf(path, sizeof(path), "%s%s", directory, file);
	if (GetFileAttributesA(path) == (DWORD)-1)
		return 0;
	/* (a file not of the saved game's size is left as it is: the game's open would fill it with zeros to that size) */
	{
		HANDLE handle = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		DWORD size;

		if (handle == INVALID_HANDLE_VALUE)
		{
			platform_log("ae saved game: %s could not be opened: not read", file);
			return 0;
		}
		size = GetFileSize(handle, NULL);
		CloseHandle(handle);
		if (size != (DWORD)HALO_PORT_GAME_STATE_SIZE)
		{
			platform_log("ae saved game: %s is %lu bytes, not a saved game's %lu: not read, left as it is", file,
				(unsigned long)size, (unsigned long)HALO_PORT_GAME_STATE_SIZE);
			return 0;
		}
	}
	return 1;
}

int ae_saved_game_file_custom(const char *level)
{
	char file[AE_CAMPAIGN_SAVE_FILE_SIZE];

	return level_file(level, file) == 1;
}
