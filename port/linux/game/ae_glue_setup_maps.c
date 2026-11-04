/*
AE_GLUE_SETUP_MAPS.C

The game setup spike's CHANGE MAP glue. See ae_glue_setup_maps.h.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* (PNG alone: a map's picture; static, so it never meets posix_ui_font.c's JPEG one. Before cseries.h, whose malloc,
realloc and free are the game's checked ones: stb_image allocates and frees with the C library's, all of a piece) */
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../../third_party/stb/stb_image.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
/* (stb_image brings the C library's <limits.h>: in the 32-bit builds its LONG_MAX, LONG_MIN, CHAR_MAX and CHAR_MIN are
macros, which break cseries.h's enumerators of those names; the 64-bit builds' port/linux/include/limits.h drops them
itself) */
#undef LONG_MAX
#undef LONG_MIN
#undef CHAR_MAX
#undef CHAR_MIN

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "cache/cache_files.h"
#include "saved games/saved_game_files.h"
#include "halo_custom_maps.h"
#include "halo_map_families.h"
#include "halo_map_packs.h"
#include "halo_ui_map_list.h"
#include "../src/ae_discs.h"
#include "../src/ae_draw.h"
#include "../src/ae_worker.h"
#include "ae_catalog.h"
#include "ae_glue_mcc_maps.h"
#include "../src/ae_mcc_workshop.h"
#include "ae_map_kind.h"
#include "ae_glue_picker_stores.h"
#include "ae_glue_setup_maps.h"
#include "ae_map_folders.h"
#include "ae_map_folders_rules.h"
#include "ae_map_picture_rules.h"
#include "ae_map_warm_up.h"
#include "ae_mapinfo.h"
#include "ae_stage_map.h"
#include <xtl.h>


void platform_log(char const *format, ...);
/* ui_widget_event_handler_functions.c (declared as menu_functions.c does) */
short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);
void ui_widget_port_multiplayer_maps_refresh(void);
boolean ui_widget_port_multiplayer_map_choose(short level_index);
/* ae_maps_data.c (tools/ae_maps_data.py) */
extern const unsigned int ae_maps_data_words[];
extern const int ae_maps_data_size;
extern const unsigned int ae_workshop_collections_words[];
extern const int ae_workshop_collections_size;

enum { PATH_SIZE = 512, PICTURES = 32, SMALL_EXTENT = 35, LARGE_EXTENT = 130 };
/* the groups: the player's folders in the first AE_PF_FOLDERS (FAVOURITES, then theirs A-Z: a slot past their count
has no name and is never listed), then the sources and the tag groups (ae_glue_setup_maps.h) */
enum { GROUP_STOCK_OG = AE_PF_FOLDERS, GROUP_STOCK_GEARBOX, GROUP_STOCK_MCC, GROUP_NHE, GROUP_HALO_OG,
	FIRST_SOURCE_GROUP = GROUP_STOCK_OG };
/* the page of the PC maps' folder (maps\ce\gearbox) */
#define GEARBOX_PAGE "GEARBOX"

static struct
{
	/* (about 300 KB: static) */
	struct ae_catalog catalog;
	int catalog_read;
	char text[16384];
	/* each map's source group (its badge), and the groups past the folders' */
	short source[AE_SETUP_MAPS_MAXIMUM];
	int disc_group, warm_up_group, btb_group, mod_group, ce_group, halomd_group;
	/* (0.6.0) MCC's maps read in place: the WORKSHOP folder, its first collection's group (the table's order, then
	OTHER's), and the menus' MCC maps (names, item ids, pictures) */
	int workshop_group, first_collection_group, other_group;
	short mcc_count;
	struct ae_mcc_menu_map mcc[AE_MCC_MENU_MAPS_MAXIMUM];
	short facts_read, facts_unreadable;
	int facts_logged;
	/* the pictures loaded: the map's file, its image, its size (an image 0: none) */
	short picture_count;
	char picture_files[PICTURES][AE_SETUP_MAP_TEXT];
	int picture_images[PICTURES], picture_widths[PICTURES], picture_heights[PICTURES];
	short last_used;
} glue = { .last_used = -1 };

/* ---------- maps.txt */

static void catalog_read(void)
{
	int index;

	if (glue.catalog_read)
		return;
	glue.catalog_read = 1;
	if (ae_maps_data_size <= 0 || ae_maps_data_size >= (int)sizeof(glue.text))
		return;
	/* (the words' bytes little-endian, as tools/ae_maps_data.py packs them) */
	for (index = 0; index < ae_maps_data_size; index++)
		glue.text[index] = (char)(ae_maps_data_words[index / 4] >> (8 * (index % 4)));
	glue.text[ae_maps_data_size] = 0;
	ae_catalog_read_facts(&glue.catalog, glue.text);
	/* (the Workshop collections, 0.6.0: the WORKSHOP folder's groups) */
	if (ae_workshop_collections_size <= 0 || ae_workshop_collections_size >= (int)sizeof(glue.text))
		return;
	for (index = 0; index < ae_workshop_collections_size; index++)
		glue.text[index] = (char)(ae_workshop_collections_words[index / 4] >> (8 * (index % 4)));
	glue.text[ae_workshop_collections_size] = 0;
	ae_catalog_read_collections(&glue.catalog, glue.text);
}

/* an MCC map of the menus' by its identity, NULL none */
static struct ae_mcc_menu_map const *mcc_map_of(char const *identity)
{
	short index;

	for (index = 0; index < glue.mcc_count; index++)
	{
		if (!_stricmp(glue.mcc[index].identity, identity))
			return &glue.mcc[index];
	}
	return NULL;
}

/* a Workshop map joins its collections' groups (OTHER's when it is in none) and WORKSHOP's; its source group: its
first collection's */
static int workshop_join(struct ae_setup_maps *maps, struct ae_setup_map *map, char const *item_id)
{
	int found[AE_CATALOG_COLLECTIONS], count, index, source;

	count = ae_catalog_collections_of(&glue.catalog, item_id, found, AE_CATALOG_COLLECTIONS);
	for (index = 0; index < count; index++)
		ae_setup_map_join(map, glue.first_collection_group + found[index]);
	source = count ? glue.first_collection_group + found[0] : glue.other_group;
	ae_setup_map_join(map, source);
	ae_setup_map_join(map, glue.workshop_group);
	(void)maps;
	return source;
}

static struct ae_map_facts const *facts_of(char const *file)
{
	short index;

	for (index = 0; index < glue.catalog.facts_count; index++)
	{
		if (!_stricmp(glue.catalog.facts[index].file, file))
			return &glue.catalog.facts[index];
	}
	return NULL;
}

static int has_tag(char const *tags, char const *tag)
{
	size_t length = strlen(tag);
	char const *at = tags;

	while ((at = strstr(at, tag)) != NULL)
	{
		if ((at == tags || at[-1] == ',') && (at[length] == 0 || at[length] == ','))
			return 1;
		at += length;
	}
	return 0;
}

/* ---------- the facts: read on a worker thread and kept for the session (an Xbox map's tags are inflated to be
read: 150-250 ms a map, up to 1.5 s, which stalled the menus when read a few a frame). The worker reads the files
with stdio (ae_mapinfo_read) and nothing else of the game's; the menus' thread only queues and copies under the lock.
The thread and the lock are the platform layer's (ae_worker.h: no <pthread.h> in a game unit). The worker opens each
map by its game path (d:\maps\<file>.map, as cache_files_map_file_path and map_family_find give it): ae_mapinfo.c is
a game unit, so its fopen is the port's wrapper, which translates the path the way CreateFile does
(platform_translate_path): halo_linux_fopen on Linux (port/linux/include/stdio.h), halo_windows_fopen on Windows
(port/windows/include/crt/stdio.h, halo_windows_file_names.h: the game units' <stdio.h> there). A host path given to
it would be translated again (under the data root): it never gets one */

enum { FACTS_PENDING, FACTS_DONE };

static struct
{
	struct ae_worker_lock *lock;   /* made at the first queue (the menus' thread), before any worker */
	int running;
	short count;
	struct { char file[AE_SETUP_MAP_TEXT]; char host[PATH_SIZE]; int state, ok, modes; float extent; }
		maps[AE_SETUP_MAPS_MAXIMUM];
	/* each loaded map's entry here */
	short entry_of[AE_SETUP_MAPS_MAXIMUM];
	unsigned long started_at;
} reader;

static short facts_queue(char const *list_name);

/* ---------- loading */

/* a group of that name, -1 for none (never added) */
/* a map pack's group: its name in capitals (map_pack_title: HALO CE+, HALO 1: PATCH EDITION) */
static void pack_group_name(char const *list_name, char *name, long size)
{
	char pack[MAP_PACK_ID_LENGTH + 1];
	long at;

	name[0] = 0;
	if (!map_family_pack_id(list_name, pack, sizeof(pack)))
		return;
	map_pack_title(pack, name, size);
	for (at = 0; name[at]; at++)
		name[at] = name[at] >= 'a' && name[at] <= 'z' ? (char)(name[at] - 32) : name[at];
}

static int group_find(struct ae_setup_maps const *maps, char const *name)
{
	int group;

	for (group = 0; group < maps->group_count; group++)
	{
		if (!strcmp(maps->group_names[group], name))
			return group;
	}
	return -1;
}

static int group_add(struct ae_setup_maps *maps, char const *name)
{
	int group;

	for (group = 0; group < maps->group_count; group++)
	{
		if (!strcmp(maps->group_names[group], name))
			return group;
	}
	if (maps->group_count >= AE_SETUP_MAP_GROUPS)
		return -1;
	snprintf(maps->group_names[maps->group_count], AE_SETUP_MAP_GROUP_NAME, "%s", name);
	return maps->group_count++;
}

static void narrow(wchar_t const *wide, char *text, int size)
{
	int index;

	for (index = 0; index < size - 1 && wide[index]; index++)
		text[index] = wide[index] >= 0x20 && wide[index] < 0x7F ? (char)wide[index] : '?';
	text[index] = 0;
}

static int file_exists(char const *path)
{
	HANDLE file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (file == INVALID_HANDLE_VALUE)
		return 0;
	CloseHandle(file);
	return 1;
}

static int compare_names(void const *a, void const *b)
{
	return ae_map_folders_compare((char const *)a, (char const *)b);
}

void ae_setup_map_id(char const *list_name, char *id, int size)
{
	char const *base = list_name, *cursor;

	for (cursor = list_name; cursor && *cursor; cursor++)
	{
		if (*cursor == '\\' || *cursor == '/')
			base = cursor + 1;
	}
	snprintf(id, (size_t)size, "%s", base ? base : "");
}

/* the player's folders (the store's) as the first groups: their names, flags and maps */
void ae_setup_maps_folders_apply(struct ae_setup_maps *maps)
{
	struct ae_picker_folders const *store = ae_picker_store(AE_PF_MAPS);
	short index, folder;
	int entry;

	for (index = 0; index < maps->count; index++)
	{
		maps->maps[index].groups[0] = 0;
		maps->maps[index].groups[1] = 0;
	}
	for (folder = 0; folder < AE_PF_FOLDERS; folder++)
	{
		maps->group_names[folder][0] = 0;
		maps->group_flags[folder] = 0;
		maps->group_folder[folder] = -1;
		if (folder >= store->folder_count)
			continue;
		/* (FAVOURITES upper case, as AE's groups; the player's own in their own case) */
		snprintf(maps->group_names[folder], AE_SETUP_MAP_GROUP_NAME, "%s", folder == AE_PF_FAVOURITES ? "FAVOURITES" :
			store->names[folder]);
		maps->group_flags[folder] = (unsigned char)(AE_SETUP_GROUP_LISTED | (folder ? AE_SETUP_GROUP_OWN : 0));
		maps->group_folder[folder] = folder;
	}
	/* (an id whose map is not here: kept in its folder, not listed) */
	for (entry = 0; entry < store->entry_count; entry++)
	{
		for (index = 0; index < maps->count; index++)
		{
			if (!strcmp(maps->maps[index].id, store->entries[entry].id))
				ae_setup_map_join(&maps->maps[index], store->entries[entry].folder);
		}
	}
	if (maps->group_count < AE_PF_FOLDERS)
		maps->group_count = AE_PF_FOLDERS;
}

void ae_setup_maps_load(struct ae_setup_maps *maps)
{
	static char pages[AE_SETUP_MAPS_MAXIMUM][AE_MAP_PAGE_NAME_SIZE];
	static char folders[AE_SETUP_MAP_GROUPS][AE_MAP_PAGE_NAME_SIZE];
	char const *const *names = NULL;
	char const *disc = cache_files_mod_started();
	char disc_name[AE_SETUP_MAP_GROUP_NAME];
	short count, index, last_used = 0;
	int folder_count = 0, folder, disc_is_nhe = 0, first_folder;

	catalog_read();
	memset(maps, 0, sizeof(*maps));
	memset(glue.source, -1, sizeof(glue.source));
	glue.facts_read = glue.facts_unreadable = 0;
	glue.facts_logged = 0;
	ui_widget_port_multiplayer_maps_refresh();
	count = ui_widget_port_multiplayer_maps(&names, &last_used);
	/* (the map used last, else the list's first, focused: one that has its file) */
	glue.last_used = ae_stage_map_playable_index(names, count, last_used);
	if (glue.last_used < 0)
		glue.last_used = last_used;
	/* (a full list leaves maps out, the last found first: MCC's Workshop maps are the list's last; said, not silent) */
	if (count >= AE_SETUP_MAPS_MAXIMUM)
		platform_log("ae change map: the map list is full (%d maps): the rest are not listed", (int)AE_SETUP_MAPS_MAXIMUM);
	if (count > AE_SETUP_MAPS_MAXIMUM)
		count = AE_SETUP_MAPS_MAXIMUM;
	/* the folders' pages, A-Z (the PC maps' own a source group of its own) */
	for (index = 0; index < count; index++)
	{
		pages[index][0] = 0;
		if (index >= HALO_STOCK_MULTIPLAYER_MAP_COUNT)
			ae_map_folders_row_page(names[index], pages[index], sizeof(pages[index]));
		if (!strcmp(pages[index], GEARBOX_PAGE))
			continue;
		for (folder = 0; pages[index][0] && folder < folder_count; folder++)
		{
			if (!strcmp(folders[folder], pages[index]))
				break;
		}
		if (pages[index][0] && folder == folder_count &&
			folder_count < AE_SETUP_MAP_GROUPS - FIRST_SOURCE_GROUP - 12)
			snprintf(folders[folder_count++], AE_MAP_PAGE_NAME_SIZE, "%s", pages[index]);
	}
	qsort(folders, (size_t)folder_count, sizeof(folders[0]), compare_names);
	/* (the player's folders' slots first, their names set by ae_setup_maps_folders_apply) */
	maps->group_count = AE_PF_FOLDERS;
	group_add(maps, "STOCK OG");
	group_add(maps, "STOCK GEARBOX");
	group_add(maps, "STOCK MCC");
	group_add(maps, "NHE");
	group_add(maps, "COMMUNITY COMPETITIVE");
	glue.disc_group = -1;
	if (disc && disc[0])
	{
		int at;

		for (at = 0; at < (int)sizeof(disc_name) - 1 && disc[at]; at++)
			disc_name[at] = disc[at] >= 'a' && disc[at] <= 'z' ? (char)(disc[at] - 32) : disc[at];
		disc_name[at] = 0;
		disc_is_nhe = strstr(disc_name, "NHE") != NULL;
		/* (NHE's disc: its maps are NHE's group; another's a group of its name) */
		glue.disc_group = disc_is_nhe ? GROUP_NHE : group_add(maps, disc_name);
	}
	/* (a map pack's maps, <file>@<pack>: a group of the pack's name, HALO CE+ for maps_cex) */
	for (index = 0; index < count; index++)
	{
		char group[AE_SETUP_MAP_GROUP_NAME];

		pack_group_name(names[index], group, sizeof(group));
		if (group[0])
			group_add(maps, group);
	}
	/* (0.6.0: MCC's Workshop maps, one folder opening into the collections: workshop_collections.txt's, then OTHER) */
	glue.mcc_count = (short)ae_mcc_menu_maps(glue.mcc, AE_MCC_MENU_MAPS_MAXIMUM);
	glue.workshop_group = group_add(maps, "WORKSHOP");
	glue.first_collection_group = maps->group_count;
	for (index = 0; index < glue.catalog.collection_count; index++)
	{
		int group = group_add(maps, glue.catalog.collections[index].name);

		if (group >= 0 && glue.workshop_group >= 0)
			maps->group_parent[group] = (short)(glue.workshop_group + 1);
	}
	glue.other_group = group_add(maps, "OTHER");
	if (glue.other_group >= 0 && glue.workshop_group >= 0)
		maps->group_parent[glue.other_group] = (short)(glue.workshop_group + 1);
	glue.warm_up_group = group_add(maps, "WARM UP");
	glue.btb_group = group_add(maps, "BTB");
	first_folder = maps->group_count;
	for (folder = 0; folder < folder_count; folder++)
		group_add(maps, folders[folder]);
	glue.mod_group = group_add(maps, "MOD MAPS");
	glue.ce_group = group_add(maps, "CUSTOM EDITION");
	glue.halomd_group = group_add(maps, "HALOMD");
	for (index = 0; index < count; index++)
	{
		struct ae_setup_map *map = &maps->maps[maps->count];
		struct ae_map_facts const *facts;
		char file[AE_SETUP_MAP_TEXT], path[PATH_SIZE];
		wchar_t caption[AE_SETUP_MAP_TEXT];
		short family = map_family_parse(names[index], file, sizeof(file));
		int source = -1, halo_og = 0, h2_demake = 0;

		snprintf(map->file, sizeof(map->file), "%s", names[index]);
		ae_setup_map_id(names[index], map->id, sizeof(map->id));
		map->list_index = index;
		/* (maps.txt's key: a CE or MD map's list name, <file>@ce; a stock map's file, not its scenario path) */
		facts = facts_of(names[index]);
		if (!facts)
			facts = facts_of(map->id);
		/* (not a pack's map by its file alone: CE+'s bloodgulch.map is Abyss, not the stock map maps.txt names) */
		if (!facts && family != _map_family_pack)
			facts = facts_of(file);
		if (facts && facts->name[0])
			snprintf(map->name, sizeof(map->name), "%s", facts->name);
		else
		{
			ui_map_list_caption(names[index], caption, AE_SETUP_MAP_TEXT);
			narrow(caption, map->name, sizeof(map->name));
		}
		if (facts)
		{
			snprintf(map->credits, sizeof(map->credits), "%s", facts->credits);
			map->players_min = facts->players_min;
			map->players_max = facts->players_max;
			map->size = facts->size;
			map->nhe = has_tag(facts->tags, "nhe-vehicles");
			halo_og = has_tag(facts->tags, "halo-og");
			h2_demake = has_tag(facts->tags, "h2-demake");
		}
		if (index < HALO_STOCK_MULTIPLAYER_MAP_COUNT)
			source = GROUP_STOCK_OG;
		/* (0.6.0: MCC's maps read in place, named as MCC names them) */
		else if (ae_map_kind_of(names[index]) == AE_MAP_KIND_MCC_STOCK ||
			ae_map_kind_of(names[index]) == AE_MAP_KIND_MCC_WORKSHOP)
		{
			struct ae_mcc_menu_map const *mcc = mcc_map_of(names[index]);

			if (mcc && mcc->name[0])
				snprintf(map->name, sizeof(map->name), "%s", mcc->name);
			/* (the card's facts line: where it comes from, MCC's file has no facts the reader takes) */
			if (mcc && !map->credits[0])
				snprintf(map->credits, sizeof(map->credits), mcc->kind == AE_MCC_MENU_WORKSHOP ? "WORKSHOP ITEM %s" :
					"HALO: MCC", mcc->item_id);
			source = mcc && mcc->kind == AE_MCC_MENU_WORKSHOP ? workshop_join(maps, map, mcc->item_id) :
				GROUP_STOCK_MCC;
		}
		else if (!strcmp(pages[index], GEARBOX_PAGE))
			source = GROUP_STOCK_GEARBOX;
		else if (pages[index][0])
		{
			for (folder = 0; folder < folder_count; folder++)
			{
				if (!strcmp(folders[folder], pages[index]))
					source = first_folder + folder;
			}
		}
		else if (family == _map_family_xbox && glue.disc_group >= 0 &&
			snprintf(path, sizeof(path), "d:\\%s\\%s\\maps\\%s.map", ae_disc_folder(disc), disc, file) < (int)sizeof(path) &&
			file_exists(path))
		{
			source = glue.disc_group;
			map->nhe |= disc_is_nhe;
		}
		/* (the 40-map community set, loose in the maps folder: its NHE maps and the rest, by maps.txt's tags) */
		else if (family == _map_family_xbox && map->nhe)
			source = GROUP_NHE;
		else if (family == _map_family_xbox && halo_og)
			source = GROUP_HALO_OG;
		else if (family == _map_family_custom_edition)
			source = glue.ce_group;
		else if (family == _map_family_halomd)
			source = glue.halomd_group;
		else if (family == _map_family_pack)
		{
			char group[AE_SETUP_MAP_GROUP_NAME];

			pack_group_name(names[index], group, sizeof(group));
			source = group[0] ? group_find(maps, group) : -1;
			if (source < 0)
				source = glue.mod_group;
		}
		else
			source = glue.mod_group;
		if (ae_map_warm_up(names[index]))
			ae_setup_map_join(map, glue.warm_up_group);
		if (map->size == 'l')
			ae_setup_map_join(map, glue.btb_group);
		if (source >= 0)
			ae_setup_map_join(map, source);
		/* ((0.4.3, owner) a map in a folder of its own, AE SEARCH AND DESTROY's, is in its tags' groups as well: the
		community set's in COMMUNITY COMPETITIVE, a H2 demake's in the H2 Demakes folder's group when there is one) */
		if (halo_og)
			ae_setup_map_join(map, GROUP_HALO_OG);
		if (h2_demake)
			ae_setup_map_join(map, group_find(maps, "H2 DEMAKES"));
		map->source = (short)source;
		glue.source[maps->count] = (short)source;
		reader.entry_of[maps->count] = facts_queue(names[index]);
		maps->count++;
	}
	/* (the Workshop maps Steam is still writing: listed dimmed, not in the game's list) */
	for (index = 0; index < glue.mcc_count && maps->count < AE_SETUP_MAPS_MAXIMUM; index++)
	{
		struct ae_setup_map *map = &maps->maps[maps->count];

		if (!glue.mcc[index].updating)
			continue;
		memset(map, 0, sizeof(*map));
		snprintf(map->file, sizeof(map->file), "%s", glue.mcc[index].identity);
		ae_setup_map_id(map->file, map->id, sizeof(map->id));
		snprintf(map->name, sizeof(map->name), "%s", glue.mcc[index].name);
		snprintf(map->credits, sizeof(map->credits), "WORKSHOP ITEM %s", glue.mcc[index].item_id);
		map->list_index = -1;
		map->unavailable = AE_SETUP_MAP_UPDATING;
		map->facts = AE_SETUP_FACTS_UNREADABLE;
		map->source = (short)workshop_join(maps, map, glue.mcc[index].item_id);
		glue.source[maps->count] = map->source;
		reader.entry_of[maps->count] = -1;
		maps->count++;
	}
	ae_setup_maps_folders_apply(maps);
	{
		char line[1024];
		int group, length = 0;

		line[0] = 0;
		for (group = 0; group < maps->group_count && length < (int)sizeof(line) - 60; group++)
		{
			if (!maps->group_names[group][0])
				continue;
			length += snprintf(line + length, sizeof(line) - (size_t)length, "%s%s %d", length ? ", " : "",
				maps->group_names[group], ae_setup_map_group_count(maps, group, AE_MF_ALL));
		}
		platform_log("ae change map: maps %d, groups %s", maps->count, line);
	}
}

/* a map's file as the game opens it; 0 for none */
static int map_path(char const *list_name, char *path, long size)
{
	char file[AE_SETUP_MAP_TEXT];
	short family;

	/* (an MCC map read in place: MCC's v13 file isn't the reader's; it suits every filter. A stock one's facts are its
	stock map's, the same level, when that is installed: "bloodgulch@mcc" reads maps/bloodgulch.map) */
	if (ae_map_kind_of(list_name) == AE_MAP_KIND_MCC_STOCK)
	{
		char const *at = strchr(list_name, '@');
		int length = at ? (int)(at - list_name) : 0;

		if (length <= 0 || length >= (int)sizeof(file))
			return 0;
		snprintf(file, sizeof(file), "%.*s", length, list_name);
		cache_files_map_file_path(file, path, size);
		if (path[0])
		{
			FILE *stock = fopen(path, "rb");

			if (stock)
			{
				fclose(stock);
				return 1;
			}
		}
		return 0;
	}
	if (ae_map_kind_of(list_name) == AE_MAP_KIND_MCC_WORKSHOP)
		return 0;
	family = map_family_parse(list_name, file, sizeof(file));
	if (!file[0])
		return 0;
	if (family == _map_family_xbox)
	{
		cache_files_map_file_path(file, path, size);
		return path[0] != 0;
	}
	/* (a pack's map: its file in maps_<pack>, which the file name alone doesn't name) */
	if (family == _map_family_pack)
	{
		char pack[MAP_PACK_ID_LENGTH + 1];

		return map_family_pack_id(list_name, pack, sizeof(pack)) && map_pack_file_path(pack, file, path, size);
	}
	return map_family_find(family, file, path, size) ? 1 : 0;
}

static void *facts_worker(void *unused)
{
	(void)unused;
	for (;;)
	{
		char host[PATH_SIZE];
		struct ae_mapinfo info;
		short index;
		int ok;

		ae_worker_lock_take(reader.lock);
		for (index = 0; index < reader.count && reader.maps[index].state != FACTS_PENDING; index++)
			;
		if (index == reader.count)
		{
			reader.running = 0;
			ae_worker_lock_give(reader.lock);
			return NULL;
		}
		snprintf(host, sizeof(host), "%s", reader.maps[index].host);
		ae_worker_lock_give(reader.lock);
		memset(&info, 0, sizeof(info));
		ok = host[0] && ae_mapinfo_read(host, &info) == AE_MAPINFO_OK;
		/* (the first that can't be read, said once: its host path) */
		{
			static int said;

			if (!ok && !said && host[0])
			{
				said = 1;
				platform_log("ae change map: facts: '%s' unreadable (family %d, '%s')", reader.maps[index].file,
					info.family, host);
			}
		}
		/* (entries are only ever added: index still names this one) */
		ae_worker_lock_take(reader.lock);
		reader.maps[index].ok = ok;
		reader.maps[index].modes = info.modes;
		reader.maps[index].extent = info.play_extent;
		reader.maps[index].state = FACTS_DONE;
		ae_worker_lock_give(reader.lock);
	}
}

/* a map queued for the worker (once a session); its entry, -1 when there is no room */
static short facts_queue(char const *list_name)
{
	char path[PATH_SIZE];
	short index;

	/* (the menus' thread, before any worker: none runs without the lock) */
	if (!reader.lock)
		reader.lock = ae_worker_lock_new();
	ae_worker_lock_take(reader.lock);
	for (index = 0; index < reader.count; index++)
	{
		if (!strcmp(reader.maps[index].file, list_name))
			break;
	}
	if (index == reader.count && reader.count < AE_SETUP_MAPS_MAXIMUM)
	{
		snprintf(reader.maps[index].file, AE_SETUP_MAP_TEXT, "%s", list_name);
		reader.maps[index].host[0] = 0;
		/* (the game's path: the port's fopen, which ae_mapinfo_read calls, makes it the host's itself) */
		if (map_path(list_name, path, sizeof(path)))
			snprintf(reader.maps[index].host, PATH_SIZE, "%s", path);
		reader.maps[index].state = FACTS_PENDING;
		reader.count++;
	}
	else if (index == reader.count)
		index = -1;
	if (index >= 0 && reader.maps[index].state == FACTS_PENDING && !reader.running)
	{
		reader.started_at = system_milliseconds();
		/* (running set before the worker can look at it; no lock, no worker) */
		reader.running = reader.lock != NULL;
		if (!reader.running || !ae_worker_start(facts_worker, NULL))
		{
			short other;

			reader.running = 0;
			platform_log("ae change map: the facts' worker did not start: no facts");
			for (other = 0; other < reader.count; other++)
				reader.maps[other].state = FACTS_DONE;
		}
	}
	ae_worker_lock_give(reader.lock);
	return index;
}

void ae_setup_maps_prefetch(void)
{
	char const *const *names = NULL;
	short count, last = 0, index;

	count = ui_widget_port_multiplayer_maps(&names, &last);
	for (index = 0; index < count && index < AE_SETUP_MAPS_MAXIMUM; index++)
		facts_queue(names[index]);
}

short ae_setup_maps_read_facts(struct ae_setup_maps *maps, short count)
{
	short index, left = 0;

	(void)count;
	ae_worker_lock_take(reader.lock);
	for (index = 0; index < maps->count; index++)
	{
		struct ae_setup_map *map = &maps->maps[index];
		short entry = reader.entry_of[index];

		if (map->facts != AE_SETUP_FACTS_NONE)
			continue;
		if (entry < 0 || entry >= reader.count)
			map->facts = AE_SETUP_FACTS_UNREADABLE;
		else if (reader.maps[entry].state != FACTS_DONE)
		{
			left++;
			continue;
		}
		else if (!reader.maps[entry].ok)
			map->facts = AE_SETUP_FACTS_UNREADABLE;
		else
		{
			map->facts = AE_SETUP_FACTS_READ;
			map->modes = reader.maps[entry].modes;
			/* (maps.txt's size first; else the play area's, as maps.txt's sizes were made) */
			if (!map->size && reader.maps[entry].extent > 0.0f)
				map->size = reader.maps[entry].extent < SMALL_EXTENT ? 's' : reader.maps[entry].extent <= LARGE_EXTENT ?
					'm' : 'l';
			if (map->size == 'l')
				ae_setup_map_join(map, glue.btb_group);
		}
		if (map->facts == AE_SETUP_FACTS_READ)
			glue.facts_read++;
		else
			glue.facts_unreadable++;
	}
	ae_worker_lock_give(reader.lock);
	if (!left && !glue.facts_logged && maps->count)
	{
		glue.facts_logged = 1;
		platform_log("ae change map: facts %d read (%d unreadable), %lu ms since the worker started", glue.facts_read,
			glue.facts_unreadable, system_milliseconds() - reader.started_at);
	}
	return left;
}

/* ---------- the panel's words */

int ae_setup_map_source(short index)
{
	return index >= 0 && index < AE_SETUP_MAPS_MAXIMUM ? glue.source[index] : -1;
}

const char *ae_setup_map_badge(struct ae_setup_maps const *maps, short index)
{
	int source = index >= 0 && index < maps->count ? glue.source[index] : -1;

	if (source < 0)
		return "";
	if (source == glue.mod_group)
		return "MOD MAP";
	if (source >= 0 && glue.workshop_group >= 0 && ae_setup_map_group_parent(maps, source) == glue.workshop_group)
		return "WORKSHOP";
	return maps->group_names[source];
}

const char *ae_setup_map_sharing(struct ae_setup_maps const *maps, short index)
{
	int source = index >= 0 && index < maps->count ? glue.source[index] : -1;

	if (source == GROUP_STOCK_OG)
		return "Every player has it.";
	if (source >= 0 && source == glue.disc_group)
		return "From the disc you started with: players need the same disc to join.";
	if (index >= 0 && index < maps->count && maps->maps[index].unavailable == AE_SETUP_MAP_UPDATING)
		return "Steam is updating it. It can be picked once Steam has finished.";
	if (source == GROUP_STOCK_MCC)
		return "From your MCC, played as MCC has it. Players need MCC to join.";
	if (source >= 0 && glue.workshop_group >= 0 && ae_setup_map_group_parent(maps, source) == glue.workshop_group)
		return "From your Steam Workshop, played as MCC has it. Players without it are told which Workshop item to "
			"subscribe to.";
	return "From your maps folder. Players who don't have it get it from you when they join (map sharing), with a % "
		"in the lobby.";
}

/* ---------- the picture */

/* a Workshop map's thumbnail (a host path in its Steam item, read with the platform's stdio: ae_mcc_workshop_open,
never the game's translated paths), its bytes into data (malloc'd); 0 for none */
static int workshop_picture_bytes(char const *list_name, char const *host, unsigned char **data, unsigned long *size)
{
	FILE *file = ae_mcc_workshop_open(host);
	unsigned char header[AE_MAP_PICTURE_HEADER_SIZE];
	long long length;

	if (!file)
	{
		platform_log("ae change map: picture '%s' none: can't open its thumbnail", list_name);
		return 0;
	}
	length = ae_mcc_workshop_file_size(file);
	fseek(file, 0, SEEK_SET);
	if (length < (long long)sizeof(header) || length > 0x7FFFFFFF || fread(header, 1, sizeof(header), file) !=
		sizeof(header) || !ae_map_picture_png_usable(header, sizeof(header), (unsigned int)length) ||
		!(*data = malloc((size_t)length)))
	{
		fclose(file);
		platform_log("ae change map: picture '%s' none: not a usable PNG", list_name);
		return 0;
	}
	memcpy(*data, header, sizeof(header));
	if (fread(*data + sizeof(header), 1, (size_t)length - sizeof(header), file) != (size_t)length - sizeof(header))
	{
		fclose(file);
		free(*data);
		platform_log("ae change map: picture '%s' none: short read", list_name);
		return 0;
	}
	fclose(file);
	*size = (unsigned long)length;
	return 1;
}

/* a map's picture (AE's previews/<file>.png beside it), its bytes into data (malloc'd); 0 for none */
static int map_picture_bytes(char const *list_name, unsigned char **data, unsigned long *size)
{
	char path[PATH_SIZE], picture[PATH_SIZE], file[AE_SETUP_MAP_TEXT];
	unsigned char header[AE_MAP_PICTURE_HEADER_SIZE];
	unsigned long high = 0, bytes_read = 0;
	HANDLE handle;

	map_family_parse(list_name, file, sizeof(file));
	if (!map_path(list_name, path, sizeof(path)) || !ae_map_picture_path(path, file, picture, sizeof(picture)))
		return 0;
	handle = CreateFileA(picture, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (handle == INVALID_HANDLE_VALUE)
	{
		platform_log("ae change map: picture '%s' none: no %s", list_name, picture);
		return 0;
	}
	*size = GetFileSize(handle, &high);
	if (high || !ReadFile(handle, header, sizeof(header), &bytes_read, NULL) || bytes_read != sizeof(header) ||
		!ae_map_picture_png_usable(header, sizeof(header), (unsigned int)*size) || !(*data = malloc(*size)))
	{
		CloseHandle(handle);
		platform_log("ae change map: picture '%s' none: not a usable PNG", list_name);
		return 0;
	}
	memcpy(*data, header, sizeof(header));
	bytes_read = 0;
	if (!ReadFile(handle, *data + sizeof(header), *size - sizeof(header), &bytes_read, NULL) ||
		bytes_read != *size - sizeof(header))
	{
		CloseHandle(handle);
		free(*data);
		platform_log("ae change map: picture '%s' none: short read", list_name);
		return 0;
	}
	CloseHandle(handle);
	return 1;
}

static int picture_load(char const *list_name, int *width, int *height)
{
	struct ae_mcc_menu_map const *mcc = mcc_map_of(list_name);
	unsigned char *data = NULL, *rgba;
	unsigned long size = 0;
	int w = 0, h = 0, channels = 0, image;

	/* (an MCC map: a Workshop map's own thumbnail; a stock one has none of its own: the card's) */
	if (mcc ? !mcc->thumbnail[0] || !workshop_picture_bytes(list_name, mcc->thumbnail, &data, &size) :
		!map_picture_bytes(list_name, &data, &size))
		return 0;
	rgba = stbi_load_from_memory(data, (int)size, &w, &h, &channels, 4);
	free(data);
	if (!rgba)
	{
		platform_log("ae change map: picture '%s' none: not decoded", list_name);
		return 0;
	}
	image = ae_draw_image_load(rgba, w, h);
	stbi_image_free(rgba);
	*width = w;
	*height = h;
	platform_log("ae change map: picture '%s' %dx%d", list_name, w, h);
	return image;
}

int ae_setup_map_picture(struct ae_setup_maps const *maps, short index, int *width, int *height)
{
	char const *file;
	short slot;

	*width = *height = 0;
	if (index < 0 || index >= maps->count)
		return 0;
	file = maps->maps[index].file;
	for (slot = 0; slot < glue.picture_count; slot++)
	{
		if (!strcmp(glue.picture_files[slot], file))
		{
			*width = glue.picture_widths[slot];
			*height = glue.picture_heights[slot];
			return glue.picture_images[slot];
		}
	}
	/* (the oldest forgotten when full: ae_draw frees images least recently drawn itself) */
	slot = glue.picture_count < PICTURES ? glue.picture_count++ : (short)(index % PICTURES);
	snprintf(glue.picture_files[slot], AE_SETUP_MAP_TEXT, "%s", file);
	glue.picture_images[slot] = picture_load(file, &glue.picture_widths[slot], &glue.picture_heights[slot]);
	*width = glue.picture_widths[slot];
	*height = glue.picture_heights[slot];
	return glue.picture_images[slot];
}

/* ---------- the player's folders, the pick */

int ae_setup_map_folder_toggle(struct ae_setup_maps *maps, short index, short folder)
{
	struct ae_picker_folders *store = ae_picker_store(AE_PF_MAPS);
	struct ae_setup_map *map;
	int in;

	if (index < 0 || index >= maps->count || folder < 0 || folder >= store->folder_count)
		return -1;
	map = &maps->maps[index];
	in = ae_pf_has(store, folder, map->id);
	if (in ? ae_pf_take(store, folder, map->id) != 1 : ae_pf_put(store, folder, map->id) != 1)
		return -1;
	ae_picker_store_save(AE_PF_MAPS);
	ae_setup_maps_folders_apply(maps);
	platform_log("ae change map: '%s' %s %s", map->id, in ? "out of" : "in", ae_pf_name(store, folder));
	return !in;
}

int ae_setup_map_pick(struct ae_setup_maps const *maps, short index)
{
	if (index < 0 || index >= maps->count)
		return 0;
	if (maps->maps[index].list_index < 0)
		return 0;
	platform_log("ae change map: pick '%s'", maps->maps[index].file);
	return ui_widget_port_multiplayer_map_choose(maps->maps[index].list_index) ? 1 : 0;
}

short ae_setup_maps_last_used(void)
{
	return glue.last_used;
}

int ae_setup_current_map_is_nhe(void)
{
	char const *const *names = NULL;
	char const *disc = cache_files_mod_started();
	struct ae_map_facts const *facts;
	short count, last_used = 0, index;
	char file[AE_SETUP_MAP_TEXT], id[AE_SETUP_MAP_TEXT];

	if (disc && (strstr(disc, "NHE") || strstr(disc, "nhe")))
		return 1;
	catalog_read();
	count = ui_widget_port_multiplayer_maps(&names, &last_used);
	index = ae_stage_map_playable_index(names, count, last_used);
	if (index < 0)
		index = last_used;
	if (!names || index < 0 || index >= count)
		return 0;
	snprintf(file, sizeof(file), "%s", names[index]);
	facts = facts_of(file);
	if (!facts)
	{
		ae_setup_map_id(file, id, sizeof(id));
		facts = facts_of(id);
	}
	return facts && has_tag(facts->tags, "nhe-vehicles");
}
