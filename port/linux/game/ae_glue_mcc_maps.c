/*
AE_GLUE_MCC_MAPS.C

(Arena Evolved 0.6.0) MCC's maps read in place, for the menus (ae_glue_mcc_maps.h): the stock maps the player's MCC
has and the Workshop maps it found, into the game's map list and CHANGE MAP. The detection, the catalog and the
files are port/linux/src/ae_mcc_platform.c's (ae_mcc.h, ae_mcc_workshop.h).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ae_mcc.h"
#include "../src/ae_mcc_workshop.h"
#include "ae_map_kind.h"
#include "ae_glue_mcc_maps.h"

void platform_log(char const *format, ...);

/* the stock maps' names as MCC's menus show them, by file (MCC's order: the Xbox's 13, then Halo PC's 6) */
static char const *const stock_names[][2] = {
	{ "beavercreek", "Battle Creek" }, { "bloodgulch", "Blood Gulch" }, { "boardingaction", "Boarding Action" },
	{ "carousel", "Derelict" }, { "chillout", "Chill Out" }, { "damnation", "Damnation" },
	{ "hangemhigh", "Hang 'Em High" }, { "longest", "Longest" }, { "prisoner", "Prisoner" },
	{ "putput", "Chiron TL-34" }, { "ratrace", "Rat Race" }, { "sidewinder", "Sidewinder" }, { "wizard", "Wizard" },
	{ "dangercanyon", "Danger Canyon" }, { "deathisland", "Death Island" }, { "gephyrophobia", "Gephyrophobia" },
	{ "icefields", "Ice Fields" }, { "infinity", "Infinity" }, { "timberland", "Timberland" } };

static char const *stock_name(char const *file)
{
	size_t index;

	for (index = 0; index < sizeof(stock_names) / sizeof(stock_names[0]); index++)
	{
		if (!strcmp(stock_names[index][0], file))
			return stock_names[index][1];
	}
	return file;
}

/* UTF-8 into wide characters (a bad byte as '?'), cut to size with its NUL */
static void widen(char const *text, wchar_t *wide, size_t size)
{
	size_t written = 0;
	unsigned char const *at = (unsigned char const *)text;

	while (*at && written + 1 < size)
	{
		unsigned long code = *at++;
		int more = code >= 0xF0 ? 3 : code >= 0xE0 ? 2 : code >= 0xC0 ? 1 : 0;

		if (code >= 0x80 && code < 0xC0)
			code = '?';
		else if (more)
		{
			code &= 0x3F >> more;
			for (; more && (*at & 0xC0) == 0x80; more--)
				code = code << 6 | (*at++ & 0x3F);
			if (more || code > 0xFFFF)
				code = '?';
		}
		wide[written++] = (wchar_t)code;
	}
	wide[written] = 0;
}

static int mcc_on(void)
{
	return ae_mcc_use() != AE_MCC_USE_NO && ae_mcc_root() != NULL;
}

int ae_mcc_menu_maps(struct ae_mcc_menu_map *maps, int maximum)
{
	struct ae_mcc_workshop_map *workshop;
	char const *root = ae_mcc_root();
	int count = 0, index, found;

	if (!maps || maximum <= 0 || !mcc_on())
		return 0;
	for (index = 0; index < ae_mcc_stock_map_count() && count < maximum; index++)
	{
		char const *file = ae_mcc_stock_map(index);
		char reason[128];
		int state = file ? ae_mcc_stock_map_state(root, file, reason, sizeof(reason)) : AE_MCC_STOCK_MISSING;
		struct ae_mcc_menu_map *map = &maps[count];

		/* (a stock file that can't be played is left out: only a Workshop map is shown while Steam writes it) */
		if (state != AE_MCC_STOCK_PLAYABLE)
			continue;
		memset(map, 0, sizeof(*map));
		snprintf(map->identity, sizeof(map->identity), "%s@mcc", file);
		snprintf(map->name, sizeof(map->name), "%s", stock_name(file));
		map->kind = AE_MCC_MENU_STOCK;
		count++;
	}
	workshop = malloc(sizeof(*workshop) * AE_MCC_MENU_MAPS_MAXIMUM);
	if (!workshop)
		return count;
	found = ae_mcc_workshop_catalog_copy(workshop, AE_MCC_MENU_MAPS_MAXIMUM);
	for (index = 0; index < found && count < maximum; index++)
	{
		struct ae_mcc_workshop_map const *from = &workshop[index];
		struct ae_mcc_menu_map *map = &maps[count];
		int updating = from->status == AE_MCC_WORKSHOP_HIDDEN && !strcmp(from->reason, "being updated by Steam");

		if (from->status != AE_MCC_WORKSHOP_PLAYABLE && !updating)
			continue;
		/* (review F5: the loader's level names hold 63 characters) */
		if (strlen(from->name) + 3 + strlen(from->item_id) >= AE_MCC_MENU_MAP_IDENTITY)
		{
			platform_log("mcc maps: %s@ws%s not listed: its name is too long", from->name, from->item_id);
			continue;
		}
		memset(map, 0, sizeof(*map));
		snprintf(map->identity, sizeof(map->identity), "%s@ws%s", from->name, from->item_id);
		snprintf(map->name, sizeof(map->name), "%s", from->title[0] ? from->title : from->item_title[0] ?
			from->item_title : from->name);
		snprintf(map->item_id, sizeof(map->item_id), "%s", from->item_id);
		snprintf(map->thumbnail, sizeof(map->thumbnail), "%s", from->thumbnail);
		map->kind = AE_MCC_MENU_WORKSHOP;
		map->updating = updating;
		count++;
	}
	free(workshop);
	return count;
}

void ae_mcc_maps_list(ae_mcc_maps_add_function add)
{
	static struct ae_mcc_menu_map maps[AE_MCC_MENU_MAPS_MAXIMUM];
	int count, index, stock = 0, workshop = 0, updating = 0;
	static int logged = -1;

	if (!add || !mcc_on())
		return;
	ae_mcc_workshop_rescan();
	count = ae_mcc_menu_maps(maps, AE_MCC_MENU_MAPS_MAXIMUM);
	for (index = 0; index < count; index++)
	{
		wchar_t lobby[80];
		char shown[96];

		if (maps[index].updating)
		{
			updating++;
			continue;
		}
		/* (the lobby's and the classic list's name marked, as a pack's is: Blood Gulch [MCC] beside the Xbox's) */
		snprintf(shown, sizeof(shown), "%s%s", maps[index].name, maps[index].kind == AE_MCC_MENU_STOCK ? " [MCC]" : "");
		widen(shown, lobby, sizeof(lobby) / sizeof(lobby[0]));
		/* (no Xbox row; the picture ui.map's unknown level's, as a community map's) */
		add(maps[index].identity, lobby, lobby, maps[index].kind == AE_MCC_MENU_STOCK ? L"MCC map" : L"Workshop map",
			-1, 13, maps[index].identity);
		stock += maps[index].kind == AE_MCC_MENU_STOCK;
		workshop += maps[index].kind == AE_MCC_MENU_WORKSHOP;
	}
	if (logged != stock * 100000 + workshop * 100 + updating)
	{
		logged = stock * 100000 + workshop * 100 + updating;
		platform_log("mcc maps: %d stock and %d Workshop maps listed, %d Workshop maps being updated by Steam", stock,
			workshop, updating);
	}
}

int ae_mcc_map_present(char const *identity)
{
	char name[64], id[24], path[AE_MCC_PATH_SIZE], reason[128];

	if (!identity || !mcc_on())
		return 0;
	switch (ae_map_kind_of(identity))
	{
	case AE_MAP_KIND_MCC_STOCK:
		return ae_map_kind_mcc_stock_file(identity, name, sizeof(name)) &&
			ae_mcc_stock_map_state(ae_mcc_root(), name, reason, sizeof(reason)) == AE_MCC_STOCK_PLAYABLE;
	case AE_MAP_KIND_MCC_WORKSHOP:
		return ae_map_kind_workshop_parts(identity, name, sizeof(name), id, sizeof(id)) &&
			ae_mcc_workshop_map_path(id, name, path, sizeof(path)) &&
			ae_mcc_workshop_map_file_valid(path, reason, sizeof(reason));
	default:
		return 0;
	}
}
