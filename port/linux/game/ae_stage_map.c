/*
AE_STAGE_MAP.C

The next multiplayer game's map when none was picked: the game falls back to
Derelict (levels\test\carousel\carousel, game_engine_playlist_next). A data
folder without carousel.map (a player who removed the stock maps, a test
folder with only custom maps) then failed to open the game: "couldn't find
map 'carousel' on the DVD", and the Xbox's dirty disc screen. When the map
the stage names has no file, the first map of the menus' map list that has
one is played instead (the list's own order: the Xbox's thirteen, then the
community maps, then the Custom Edition ones, which the list only shows
when their files are there).
*/

#include "cseries.h"
#include "cache/cache_files.h"
#include "tag_files/tag_files.h"

#include <xtl.h>
#include <stdio.h>
#include <string.h>

#include "halo_map_families.h"
#include "halo_ui_map_list.h"
#include "ae_stage_map.h"

void platform_log(char const *format, ...);

/* the Xbox's thirteen, in the game's order */
static char const *const stock_maps[] =
{
	"beavercreek", "sidewinder", "damnation", "ratrace", "prisoner", "hangemhigh", "chillout",
	"carousel", "boardingaction", "bloodgulch", "wizard", "putput", "longest",
};

/* whether an Xbox-family map (a name with no "@family") has its file: the
played mod's maps folder first, then the maps folder, as the loader looks */
static boolean xbox_map_file_present(char const *name)
{
	char path[256];
	HANDLE file;

	path[0] = 0;
	cache_files_map_file_path(name, path, sizeof(path));
	if (!path[0])
		return FALSE;
	file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return FALSE;
	CloseHandle(file);
	return TRUE;
}

static boolean map_present(char const *name)
{
	char file[256], path[1024];
	short family;

	if (!strchr(name, '@'))
		return xbox_map_file_present(name);
	/* (a Custom Edition or other family's map: its file found now, not only when the list was made: a file gone
	since would stop the game on the damaged-disc screen) */
	file[0] = 0;
	family = map_family_parse(name, file, (long)sizeof(file));
	return file[0] && map_family_find(family, file, path, (long)sizeof(path));
}

void ae_stage_map_check(char *map_name, long size)
{
	char const *name;
	char **names;
	long count, index;

	if (!map_name || !map_name[0] || size <= 0)
		return;
	name = tag_name_strip_path(map_name);
	if (!name || !name[0] || map_present(name))
		return;
	names = ui_map_list_names();
	count = ui_map_list_count();
	for (index = 0; names && index < count; index++)
	{
		if (!names[index] || !names[index][0] || !map_present(names[index]))
			continue;
		platform_log("stage map: '%s' has no map file here; playing '%s' (the first map of the list that has one)",
			name, names[index]);
		snprintf(map_name, (size_t)size, "levels\\test\\%s\\%s", names[index], names[index]);
		return;
	}
	/* (the list is filled when the menus show it: a game set up without them, the stock maps in the game's order) */
	for (index = 0; index < (long)NUMBEROF(stock_maps); index++)
	{
		if (!xbox_map_file_present(stock_maps[index]))
			continue;
		platform_log("stage map: '%s' has no map file here; playing '%s'", name, stock_maps[index]);
		snprintf(map_name, (size_t)size, "levels\\test\\%s\\%s", stock_maps[index], stock_maps[index]);
		return;
	}
	platform_log("stage map: '%s' has no map file here, nor does any map of the list", name);
}
