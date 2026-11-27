/*
AE_GLUE_PVE_MENUS.C

See ae_glue_pve_menus.h.
*/

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "interface/player_ui.h"
#include "interface/ui_widget.h"
#include "main/main.h"
#include "saved games/player_profile.h"
#include "saved games/saved_game_files.h"
#include "campaign_score.h"
#include "ae_glue_pve_menus.h"
#include "cev/tag_injector.h"
#include "../src/port_tag_profiles.h"
#include "../src/port_config.h"
#include "ae_pve_menus_rules.h"

void platform_log(char const *format, ...);
void randomizer_loadout_tag_value(char const *path, char *value, unsigned int size);

/* ui_widget_event_handler_functions.c (declared as ae_glue_campaign.c does) */
boolean ui_widget_port_saved_game(char const **map_name, short *level, short *difficulty);
boolean player_profile_get_enclosing_directory_path(long profile, char *full_path);

/* the profiles of this PC, valid ones, in the save files' order */
static int profiles_list(long *indices, int maximum)
{
	long found[64];
	word count = 64;
	int each, kept = 0;

	player_profiles_enumerate_available_to_local_player_index(NONE, &count, found, FALSE);
	for (each = 0; each < (int)count && kept < maximum; each++)
		if (TEST_FLAG(found[each], _saved_game_file_index_valid_bit))
			indices[kept++] = found[each];
	return kept;
}

static void profile_name(struct player_profile const *profile, char *name, int size)
{
	int length = 0;
	wchar_t const *wide = profile->player_name;

	for (; *wide && length < size - 1; wide++)
		name[length++] = *wide >= 0x20 && *wide < 0x7F ? (char)*wide : '?';
	name[length] = 0;
}

long ae_pve_player_profile(void)
{
	return player_ui_get_active_player_profile_index(0);
}

static int savegame_path(long profile_index, char *path, int size)
{
	char directory[256];

	if (!player_profile_get_enclosing_directory_path(profile_index, directory) || !directory[0] ||
		strlen(directory) + 13 >= (size_t)size)
		return 0;
	snprintf(path, (size_t)size, "%ssavegame.bin", directory);
	return 1;
}

/* the saved game file's time, as a local day and clock */
static void saved_time(long profile_index, struct ae_pve_saved *saved)
{
	char path[300];
	HANDLE file;
	FILETIME written;
	struct tm *local;
	time_t seconds;
	unsigned long long ticks;

	saved->day = -1;
	if (!savegame_path(profile_index, path, sizeof(path)))
		return;
	file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return;
	memset(&written, 0, sizeof(written));
	GetFileTime(file, NULL, NULL, &written);
	CloseHandle(file);
	ticks = ((unsigned long long)written.dwHighDateTime << 32) | (unsigned long long)written.dwLowDateTime;
	if (ticks < 116444736000000000ULL)
		return;
	seconds = (time_t)((ticks - 116444736000000000ULL) / 10000000ULL);
	local = localtime(&seconds);
	if (!local)
		return;
	saved->hour = local->tm_hour;
	saved->minute = local->tm_min;
	saved->month = local->tm_mon;
	saved->year = local->tm_year + 1900;
	saved->month_day = local->tm_mday;
	/* (the day number: days since 1970 as the local calendar counts them) */
	saved->day = ae_pve_menus_day(saved->year, saved->month, saved->month_day);
}

int ae_pve_saved_games(struct ae_pve_saved *saved, int maximum)
{
	long indices[AE_PVE_PROFILES];
	struct player_profile profile, active;
	long active_index = player_ui_get_active_player_profile_index(0);
	int count = profiles_list(indices, AE_PVE_PROFILES), each, found = 0, have_active;

	have_active = active_index != NONE && player_profile_get(active_index, &active);
	for (each = 0; each < count && found < maximum; each++)
	{
		char const *map_name;
		struct ae_pve_saved *row = &saved[found];

		if (!player_profile_get(indices[each], &profile))
			continue;
		/* (read with the profile as player 1's: the game's own read of the saved game of the active profile) */
		player_ui_set_active_player_profile(0, indices[each], &profile);
		memset(row, 0, sizeof(*row));
		if (!ui_widget_port_saved_game(&map_name, &row->level, &row->difficulty))
			continue;
		row->profile_index = indices[each];
		row->active = indices[each] == active_index;
		profile_name(&profile, row->name, sizeof(row->name));
		saved_time(indices[each], row);
		found++;
	}
	if (have_active)
		player_ui_set_active_player_profile(0, active_index, &active);
	return found;
}

int ae_pve_saved_pick(long profile_index)
{
	struct player_profile profile;

	if (!player_profile_get(profile_index, &profile))
		return 0;
	player_ui_set_active_player_profile(0, profile_index, &profile);
	platform_log("ae pve menus: player 1 plays with profile %08lX", (unsigned long)profile_index);
	return 1;
}

int ae_pve_saved_delete(long profile_index)
{
	char path[300];

	if (!savegame_path(profile_index, path, sizeof(path)))
		return 0;
	if (!DeleteFileA(path))
	{
		platform_log("ae pve menus: could not delete %s", path);
		return 0;
	}
	platform_log("ae pve menus: saved game of profile %08lX deleted", (unsigned long)profile_index);
	return 1;
}

const char *ae_pve_level_map(short level)
{
	return level >= 0 && level < NUMBER_OF_SINGLE_PLAYER_LEVELS ? main_get_solo_level_name(level) : "";
}

const char *ae_pve_level_short(short level)
{
	const char *name = ae_pve_level_map(level), *last = name, *each;

	for (each = name; *each; each++)
		if (*each == '\\' || *each == '/')
			last = each + 1;
	return last;
}

int ae_pve_level_runs(short level, struct ae_pve_run *runs, int maximum)
{
	long indices[AE_PVE_PROFILES];
	int count = profiles_list(indices, AE_PVE_PROFILES), each, found = 0;

	for (each = 0; each < count && found < maximum; each++)
	{
		struct campaign_score_record record;
		struct player_profile profile;

		if (!player_profile_get(indices[each], &profile) || !campaign_score_level_record(ae_pve_level_map(level), indices[each], &record))
			continue;
		runs[found].profile_index = indices[each];
		profile_name(&profile, runs[found].name, sizeof(runs[found].name));
		runs[found].score = record.score;
		runs[found].seconds = record.seconds;
		runs[found].difficulty = record.difficulty;
		runs[found].skulls = record.skulls;
		runs[found].skulls_percent = record.skulls_percent;
		runs[found].medals = record.medals;
		found++;
	}
	return found;
}

/* ---------- the randomizer's item sets */

int ae_pve_item_sets(struct ae_pve_item_set *sets, int maximum)
{
	long count, each;
	int kept = 0;

	port_tag_profiles_scan();
	count = port_tag_profiles_count();
	for (each = 0; each < count && kept < maximum; each++)
	{
		snprintf(sets[kept].folder, sizeof(sets[kept].folder), "%s", port_tag_profiles_folder(each));
		snprintf(sets[kept].name, sizeof(sets[kept].name), "%s", port_tag_profiles_name(each));
		sets[kept].items_on = tag_injector_profile_enabled_count(each, 0);
		kept++;
	}
	return kept;
}

int ae_pve_item_set_played(void)
{
	const char *folder = config_string("game.randomizer_tag_profile");

	return folder && folder[0] ? (int)port_tag_profiles_find(folder) : -1;
}

int ae_pve_item_set_types(int weapons, char (*types)[AE_PVE_ITEM_TYPE], int maximum)
{
	long profile = tag_injector_profile_playable();

	if (profile == NONE || maximum < 1)
		return 0;
	return (int)tag_injector_custom_types(profile, weapons ? 'weap' : 'actv', types, maximum);
}

int ae_pve_item_set_weapons(char (*labels)[48], char (*values)[24], int maximum)
{
	long profile = tag_injector_profile_playable(), tags, tag;
	int kept = 0;

	if (profile == NONE)
		return 0;
	tags = port_tag_profiles_tag_count(profile);
	for (tag = 0; tag < tags && kept < maximum; tag++)
	{
		char name[48];
		int each;

		if (port_tag_profiles_tag_group(profile, tag) != 'weap' || !port_tag_profiles_tag_enabled(profile, tag) ||
			tag_injector_tag_objective(profile, tag))
			continue;
		tag_injector_tag_display_name(profile, tag, name, sizeof(name));
		for (each = 0; name[each]; each++)
			labels[kept][each] = name[each] >= 'a' && name[each] <= 'z' ? (char)(name[each] - 32) : name[each];
		labels[kept][each] = 0;
		randomizer_loadout_tag_value(port_tag_profiles_tag_name(profile, tag), values[kept], 24);
		kept++;
	}
	return kept;
}

void ae_pve_item_set_counts(long profile, long *weapons, long *actors)
{
	*weapons = tag_injector_profile_enabled_count(profile, 'weap');
	*actors = tag_injector_profile_enabled_count(profile, 'actv');
}
