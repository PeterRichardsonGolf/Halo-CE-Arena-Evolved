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

The menus pick their default map the same way (0.3.9: AE's CREATE GAME on a
data folder without beavercreek.map chose Battle Creek, the list's first and
the "used last" when nothing was remembered: the game's damaged-disc loop then
ran the AE screens inside itself, which chose it again, until the stack ran
out): ae_stage_map_file_present and ae_stage_map_playable_index.
*/

#include "cseries.h"
#include "cache/cache_files.h"
#include "tag_files/tag_files.h"

#include <xtl.h>
#include <stdio.h>
#include <string.h>

#include "halo_map_families.h"
#include "halo_map_packs.h"
#include "ae_glue_mcc_maps.h"
#include "ae_map_kind.h"
#include "halo_ui_map_list.h"
#include "ae_stage_map.h"
#include "mcc_cache.h"

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
	/* (0.6.0: an MCC map read in place, <name>@mcc or <name>@ws<item id>: its file in MCC's folders now) */
	if (ae_map_kind_of(name) == AE_MAP_KIND_MCC_STOCK || ae_map_kind_of(name) == AE_MAP_KIND_MCC_WORKSHOP)
		return ae_mcc_map_present(name) ? TRUE : FALSE;
	/* (a Custom Edition or other family's map: its file found now, not only when the list was made: a file gone
	since would stop the game on the damaged-disc screen) */
	file[0] = 0;
	family = map_family_parse(name, file, (long)sizeof(file));
	/* (a pack's map, <file>@<pack>: its file in maps_<pack>, which the file name alone doesn't name) */
	if (family == _map_family_pack)
	{
		char pack[MAP_PACK_ID_LENGTH + 1];

		return file[0] && map_family_pack_id(name, pack, (long)sizeof(pack)) && map_pack_present(pack, file);
	}
	return file[0] && map_family_find(family, file, path, (long)sizeof(path));
}

void ae_stage_map_check(char *map_name, long size)
{
	char const *name;
	char **names;
	long count, index;

	if (!map_name || !map_name[0] || size <= 0)
		return;
	/* (an MCC map, mcc_maps\<name>: the MCC loader finds its file and refuses it with a reason when it can't) */
	if (mcc_level_name(map_name))
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

int ae_stage_map_file_present(char const *list_name)
{
	char const *name;

	if (!list_name || !list_name[0])
		return 0;
	name = tag_name_strip_path(list_name);
	return name && name[0] && map_present(name) ? 1 : 0;
}

short ae_stage_map_playable_index(char const *const *names, short count, short preferred)
{
	short index;

	if (!names || count <= 0)
		return -1;
	if (preferred >= 0 && preferred < count && ae_stage_map_file_present(names[preferred]))
		return preferred;
	for (index = 0; index < count; index++)
	{
		if (!ae_stage_map_file_present(names[index]))
			continue;
		if (preferred >= 0 && preferred < count)
			platform_log("stage map: '%s' has no map file here; the menus start on '%s' (the first map of the list that "
				"has one)", tag_name_strip_path(names[preferred]), tag_name_strip_path(names[index]));
		return index;
	}
	platform_log("stage map: no map of the list has a map file here");
	return -1;
}
