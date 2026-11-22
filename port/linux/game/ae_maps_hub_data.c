/*
AE_MAPS_HUB_DATA.C

The lists MAPS & MODS' INSTALLED and REQUIRED FILES pages read (ae_maps_hub_data.h).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ae_mcc_workshop.h"
#include "../src/ae_steam_client.h"
#include "ae_map_library.h"
#include "ae_maps_hub_data.h"
#include "ae_decimal.h"

void mcc_maps_rescan(void);

int ae_hub_data_library(void)
{
	return 1;
}

/* the list's generation: it changes when a scan begins (the list it replaces the shown one with is a new one), when the count
changes, and with each delete or move made through this layer, so a view or a total kept from an older one is asked again */
static unsigned long changes_made, scans_seen;
static int was_scanning;

static void generation_poll(void)
{
	int scanning = ae_map_library_scanning();

	if (scanning && !was_scanning)
		scans_seen++;
	was_scanning = scanning;
}

unsigned long ae_hub_data_generation(void)
{
	generation_poll();
	return (unsigned long)ae_map_library_count() * 2654435761ul + scans_seen * 131ul + changes_made * 7919ul +
		(ae_map_library_scanning() ? 1ul : 0ul);
}

void ae_hub_data_update(void)
{
	ae_map_library_update();
	generation_poll();
}

int ae_hub_data_scanning(void)
{
	return ae_map_library_scanning();
}

int ae_hub_data_ready(void)
{
	return ae_map_library_ready();
}

static int source_of(int library_source)
{
	switch (library_source)
	{
	case AE_MAP_SOURCE_STOCK: return AE_HUB_SOURCE_STOCK;
	case AE_MAP_SOURCE_CUSTOM_EDITION: return AE_HUB_SOURCE_CUSTOM_EDITION;
	case AE_MAP_SOURCE_XBOX_CUSTOM: return AE_HUB_SOURCE_XBOX;
	case AE_MAP_SOURCE_DOWNLOADED: return AE_HUB_SOURCE_HOST;
	case AE_MAP_SOURCE_MAP_PACK: return AE_HUB_SOURCE_MAP_PACK;
	case AE_MAP_SOURCE_MCC: return AE_HUB_SOURCE_MCC;
	}
	return AE_HUB_SOURCE_WORKSHOP_STEAM;
}

static unsigned int source_bits(int show)
{
	unsigned int bits = 0;
	int source;

	for (source = 0; source < NUMBER_OF_AE_MAP_SOURCES; source++)
		/* (the library's downloaded maps are the host's and the archive's together: the page tells them apart per map) */
		if (ae_hub_show_has(show, source_of(source)) ||
			(source == AE_MAP_SOURCE_DOWNLOADED && ae_hub_show_has(show, AE_HUB_SOURCE_ARCHIVE)))
			bits |= AE_MAP_SOURCE_BIT(source);
	return show == AE_HUB_SHOW_ALL ? 0 : bits;
}

int ae_hub_data_query(int show, char const *text, int sort, int *indexes, int maximum)
{
	struct ae_map_filter filter;

	memset(&filter, 0, sizeof(filter));
	snprintf(filter.text, sizeof(filter.text), "%s", text);
	filter.sources = source_bits(show);
	return ae_map_library_query(&filter, sort == 1 ? AE_MAP_SORT_SIZE_LARGEST : sort == 2 ? AE_MAP_SORT_SOURCE : AE_MAP_SORT_NAME,
		indexes, maximum);
}

unsigned long long ae_hub_data_total_count(void)
{
	return (unsigned long long)ae_map_library_count();
}

unsigned long long ae_hub_data_total_bytes(void)
{
	static unsigned long long bytes;
	static unsigned long counted;
	static int have;
	unsigned long generation = ae_hub_data_generation();

	if (!have || generation != counted)
	{
		int count = ae_map_library_count(), each;

		bytes = 0;
		for (each = 0; each < count; each++)
			bytes += ae_map_library_entry(each)->size;
		counted = generation;
		have = 1;
	}
	return bytes;
}

int ae_hub_data_workshop_count(void)
{
	static unsigned long counted;
	static int have, maps;
	unsigned long generation = ae_hub_data_generation();

	if (!have || generation != counted)
	{
		int count = ae_map_library_count(), each;

		maps = 0;
		for (each = 0; each < count; each++)
			maps += ae_map_library_entry(each)->source == AE_MAP_SOURCE_WORKSHOP;
		counted = generation;
		have = 1;
	}
	return maps;
}

int ae_hub_data_fill(int index, struct ae_hub_map *map, int with_picture)
{
	struct ae_map_entry const *entry = ae_map_library_entry(index);
	char const *slash;
	size_t length;

	if (!entry)
		return 0;
	memset(map, 0, sizeof(*map));
	snprintf(map->name, sizeof(map->name), "%s", entry->title[0] ? entry->title : entry->name);
	snprintf(map->file, sizeof(map->file), "%s.map", entry->name);
	map->source = source_of(entry->source);
	/* (a downloaded map in custom_maps is the map archive's; one at the top of maps_ce was sent by a host) */
	if (entry->source == AE_MAP_SOURCE_DOWNLOADED && ae_map_library_entry_at_top_of(entry, "d:\\custom_maps\\"))
		map->source = AE_HUB_SOURCE_ARCHIVE;
	snprintf(map->folder, sizeof(map->folder), "%s", entry->folder);
	map->size = entry->size;
	map->kind_campaign = (entry->serves & AE_MAP_SERVES_CAMPAIGN) != 0 && !(entry->serves & AE_MAP_SERVES_MULTIPLAYER);
	snprintf(map->plays, sizeof(map->plays), "%s", entry->serves & AE_MAP_SERVES_MULTIPLAYER ? "Multiplayer" :
		entry->serves & AE_MAP_SERVES_CAMPAIGN ? "Campaign" : "");
	map->in_use = ae_map_library_entry_in_use(index);
	map->library_index = index;
	map->serves = entry->serves;
	ae_map_library_list_name(entry, map->list_name, sizeof(map->list_name));
	if (entry->source == AE_MAP_SOURCE_WORKSHOP)
	{
		map->workshop_item = ae_decimal_u64(entry->pack);
		/* (a Workshop map in the game's own mcc_maps folder is one the downloader may have put there; Steam's folder is Steam's) */
		if (strstr(entry->path, "/mcc_maps/") || strstr(entry->path, "\\mcc_maps\\"))
		{
			struct ae_steam_collection_item client;

			map->source = AE_HUB_SOURCE_WORKSHOP_INSTALLED;
			(void)ae_steam_client_collection_item(map->workshop_item, &client);
			map->client_open = ae_steam_client_opened();
			map->downloaded = client.recorded;
		}
	}
	snprintf(map->path, sizeof(map->path), "%s", entry->path);
	slash = strrchr(map->path, '/');
	if (!slash)
		slash = strrchr(map->path, '\\');
	length = slash ? (size_t)(slash - map->path) : 0;
	map->path[length] = 0;
	if (with_picture)
		map->picture = ae_map_library_picture(index, &map->picture_width, &map->picture_height);
	return 1;
}

int ae_hub_data_delete(int index, char *reason, int size)
{
	int answer = ae_map_library_delete(index);

	changes_made++;
	snprintf(reason, (size_t)size, "%s", ae_map_library_refusal_text(answer));
	return answer == AE_MAP_CHANGE_ALLOWED;
}

static int removing;
static unsigned long long removing_item;

int ae_hub_data_item_maps(int index)
{
	struct ae_map_entry const *entry = ae_map_library_entry(index);

	return entry && entry->source == AE_MAP_SOURCE_WORKSHOP ?
		ae_map_library_workshop_item_maps(ae_decimal_u64(entry->pack), NULL) : 1;
}

int ae_hub_data_delete_downloaded(int index, char *reason, int size)
{
	struct ae_map_entry const *entry = ae_map_library_entry(index);
	struct ae_steam_collection_item client;
	unsigned long long item;
	int in_use = 0;

	if (!entry || entry->source != AE_MAP_SOURCE_WORKSHOP)
	{
		snprintf(reason, (size_t)size, "%s", ae_map_library_refusal_text(AE_MAP_REFUSED_UNKNOWN_MAP));
		return 0;
	}
	item = ae_decimal_u64(entry->pack);
	/* (the whole item's folder goes: a lobby or a game on any map of it keeps it) */
	(void)ae_map_library_workshop_item_maps(item, &in_use);
	if (in_use || ae_map_library_entry_in_use(index))
	{
		snprintf(reason, (size_t)size, "%s", ae_map_library_refusal_text(AE_MAP_REFUSED_IN_USE));
		return 0;
	}
	(void)ae_steam_client_collection_item(item, &client);
	/* (the same removal as WORKSHOP > COLLECTIONS: only a folder the downloader recorded) */
	if (!item || !client.recorded || !ae_steam_client_collection_remove(&item, 1))
	{
		snprintf(reason, (size_t)size, "The game did not download this map, or it is being downloaded.");
		return 0;
	}
	removing = 1;
	removing_item = item;
	return 1;
}

int ae_hub_data_delete_poll(int *removed)
{
	struct ae_steam_collection_item client;
	int result;

	if (!removing)
		return 0;
	/* (this removal's own result: not the totals of a batch the collections page may have running) */
	result = ae_steam_client_removal_result(removing_item);
	if (!result)
		return 0;
	removing = 0;
	if (result < 0)
	{
		/* (the batch forgot it: it is done when the client holds no record of it) */
		(void)ae_steam_client_collection_item(removing_item, &client);
		result = client.recorded ? 2 : 1;
	}
	*removed = result == 1;
	mcc_maps_rescan();
	ae_mcc_workshop_rescan();
	ae_map_library_refresh();
	changes_made++;
	return 1;
}

int ae_hub_data_move(int index, char const *folder, char *reason, int size)
{
	int answer = ae_map_library_move(index, folder);

	changes_made++;
	snprintf(reason, (size_t)size, "%s", ae_map_library_refusal_text(answer));
	return answer == AE_MAP_CHANGE_ALLOWED;
}

int ae_hub_data_folder_name_ok(char const *name)
{
	return name && name[0] && ae_map_library_folder_name_ok(name);
}

int ae_hub_data_folders(int index, char names[][48], int *counts, int maximum)
{
	struct ae_map_entry const *entry = ae_map_library_entry(index);
	int each, count = 0, listed, total = ae_map_library_count();

	if (!entry)
		return 0;
	for (each = 0; each < total; each++)
	{
		struct ae_map_entry const *other = ae_map_library_entry(each);

		if (!other || other->root != entry->root || !other->folder[0])
			continue;
		for (listed = 0; listed < count && strcmp(names[listed], other->folder); listed++)
			;
		if (listed == count && count < maximum)
		{
			snprintf(names[count], 48, "%s", other->folder);
			counts[count++] = 0;
		}
		if (listed < count)
			counts[listed]++;
	}
	return count;
}

static void file_set(struct ae_hub_file *file, char const *name, int state, char const *group, char const *group_value,
	char const *from, unsigned long long size, char const *unlocks, int door);

int ae_hub_data_read_files(struct ae_hub_files *files, int discs)
{
	struct ae_map_required_file ce[AE_MAP_REQUIRED_MAXIMUM], mcc[AE_MAP_REQUIRED_MAXIMUM];
	int ce_count = ae_map_library_missing(AE_MAP_REQUIRED_CUSTOM_EDITION, ce, AE_MAP_REQUIRED_MAXIMUM);
	int mcc_count = ae_map_library_missing(AE_MAP_REQUIRED_MCC, mcc, AE_MAP_REQUIRED_MAXIMUM);
	int each, count = 0, maps = 0, total = ae_map_library_count();
	char text[40], upper[40], where[AE_MAP_LIBRARY_PATH_SIZE];

	memset(files, 0, sizeof(*files));
	for (each = 0; each < total; each++)
		if (ae_map_library_entry(each)->source == AE_MAP_SOURCE_CUSTOM_EDITION)
			maps++;
	for (each = 0; each < ce_count && count < AE_HUB_FILES_MOST - 2; each++, count++)
	{
		int at;

		for (at = 0; ce[each].file[at] && at < 39; at++)
			upper[at] = ce[each].file[at] >= 'a' && ce[each].file[at] <= 'z' ? (char)(ce[each].file[at] - 32) : ce[each].file[at];
		upper[at] = 0;
		snprintf(text, sizeof(text), "%d ON THIS PC", maps);
		snprintf(where, sizeof(where), "%s", ce[each].found ? ce[each].where : "");
		file_set(&files->files[count], upper, ce[each].found ? AE_HUB_FILE_FOUND : AE_HUB_FILE_MISSING, each ? "" : "CUSTOM EDITION MAPS",
			each ? "" : text, where, 0, ce[each].unlocks ? ce[each].unlocks : "", ce[each].found ? 0 : 1);
		if (!ce[each].found)
			files->missing++;
	}
	for (each = 0; each < mcc_count && count < AE_HUB_FILES_MOST - 1; each++, count++)
	{
		int at;

		for (at = 0; mcc[each].file[at] && at < 39; at++)
			upper[at] = mcc[each].file[at] >= 'a' && mcc[each].file[at] <= 'z' ? (char)(mcc[each].file[at] - 32) : mcc[each].file[at];
		upper[at] = 0;
		snprintf(where, sizeof(where), "%s", mcc[each].found ? mcc[each].where : "");
		file_set(&files->files[count], upper, mcc[each].found ? AE_HUB_FILE_FOUND : AE_HUB_FILE_ABSENT, each ? "" : "MCC MAPS", "", where,
			0, mcc[each].unlocks ? mcc[each].unlocks : "", 1);
	}
	file_set(&files->files[count], "MODDED DISCS", AE_HUB_FILE_COUNT_ONLY, "MAP PACKS AND DISCS", "", "", 0,
		"A modded disc's maps replace the stock maps it has.", 2);
	snprintf(files->files[count].value, sizeof(files->files[count].value), "%d FOUND", discs);
	count++;
	files->count = count;
	return count;
}

struct made_up { char const *name, *file; int source; unsigned int megabytes; char const *plays, *added, *folder; };

static struct made_up const made_up_maps[] =
{
	{ "Arcadia Ridge", "arcadia_ridge", AE_HUB_SOURCE_CUSTOM_EDITION, 41, "Slayer, CTF", "From the map archive, today", "comp" },
	{ "Basin Outpost", "basin_outpost", AE_HUB_SOURCE_CUSTOM_EDITION, 29, "Slayer, King", "From a host, 2 weeks ago", "" },
	{ "Blood Gulch", "bloodgulch", AE_HUB_SOURCE_STOCK, 31, "Slayer, CTF, King, Oddball", "With the game", "" },
	{ "Coldwater", "coldwater", AE_HUB_SOURCE_CUSTOM_EDITION, 38, "Slayer, CTF, King, Oddball", "From a host, 3 days ago", "comp" },
	{ "Delta Ruins", "delta_ruins", AE_HUB_SOURCE_WORKSHOP_INSTALLED, 52, "Slayer, CTF", "Workshop, 2 weeks ago", "" },
	{ "Echo Yard", "echo_yard", AE_HUB_SOURCE_MCC, 64, "Slayer, Oddball", "With MCC", "" },
	{ "Fallow Bridge", "fallow_bridge", AE_HUB_SOURCE_CUSTOM_EDITION, 27, "Slayer, CTF", "From the map archive, 5 days ago", "" },
	{ "Glasshouse", "glasshouse", AE_HUB_SOURCE_HOST, 22, "Slayer", "From a host, yesterday", "" },
	{ "Harbor Light", "harbor_light", AE_HUB_SOURCE_MAP_PACK, 45, "Slayer, CTF", "With a map pack", "" },
	{ "Ironwood", "ironwood", AE_HUB_SOURCE_CUSTOM_EDITION, 33, "Slayer, King", "From a host, 1 week ago", "" },
	{ "Juniper Pass", "juniper_pass", AE_HUB_SOURCE_WORKSHOP_STEAM, 48, "Slayer, CTF", "Workshop, 3 days ago", "" },
	{ "Kestrel Court", "kestrel_court", AE_HUB_SOURCE_CUSTOM_EDITION, 19, "Slayer", "From the map archive, today", "" },
	{ "H2d Midship", "h2d_midship", AE_HUB_SOURCE_XBOX, 38, "Slayer, CTF", "Your own file, 2 weeks ago", "H2 Demakes" },
	{ "Tinkered", "tinkered", AE_HUB_SOURCE_XBOX, 36, "Slayer", "Your own file, 2 weeks ago", "" },
	{ "Grifball", "grifball", AE_HUB_SOURCE_CUSTOM_EDITION, 41, "Oddball", "From a host, 3 days ago", "ce" },
};

void ae_hub_data_stage_maps(struct ae_hub_maps *maps, int state)
{
	int each, count = (int)(sizeof(made_up_maps) / sizeof(made_up_maps[0]));

	memset(maps, 0, sizeof(*maps));
	maps->total_count = 1284;
	maps->total_bytes = 23190ull * 1024ull * 1024ull;
	if (state == AE_HUB_STATE_LOADING)
	{
		maps->loading = 1;
		return;
	}
	if (state == AE_HUB_STATE_EMPTY)
	{
		/* a source with nothing: the SHOW value is the page's; no maps in the list */
		maps->total_count = 0;
		maps->total_bytes = 0;
		return;
	}
	for (each = 0; each < count; each++)
	{
		struct ae_hub_map *map = &maps->maps[each];

		snprintf(map->name, sizeof(map->name), "%s", made_up_maps[each].name);
		snprintf(map->file, sizeof(map->file), "%s.map", made_up_maps[each].file);
		map->source = made_up_maps[each].source;
		map->library_index = -1;
		map->downloaded = 1;
		map->client_open = 1;
		map->size = (unsigned long long)made_up_maps[each].megabytes * 1024ull * 1024ull;
		snprintf(map->plays, sizeof(map->plays), "%s", made_up_maps[each].plays);
		snprintf(map->added, sizeof(map->added), "%s", made_up_maps[each].added);
		snprintf(map->folder, sizeof(map->folder), "%s", made_up_maps[each].folder);
		snprintf(map->path, sizeof(map->path), "/staged/%s%s%s", map->source == AE_HUB_SOURCE_XBOX ? "maps" : "maps_ce",
			map->folder[0] ? "/" : "", map->folder);
		if (map->source == AE_HUB_SOURCE_WORKSHOP_INSTALLED || map->source == AE_HUB_SOURCE_WORKSHOP_STEAM)
			map->workshop_item = 1234567890ull + (unsigned long long)each;
		if (!strcmp(made_up_maps[each].file, "coldwater"))
			snprintf(map->description, sizeof(map->description), "Two bases across a frozen river; tunnels under the ice.");
		if (!strcmp(made_up_maps[each].file, "echo_yard"))
			map->unreadable = state == AE_HUB_STATE_UNREADABLE;
		if (!strcmp(made_up_maps[each].file, "coldwater"))
		{
			map->in_use = state == AE_HUB_STATE_IN_USE;
			map->unreadable = state == AE_HUB_STATE_UNREADABLE;
		}
	}
	maps->count = count;
}

static void file_set(struct ae_hub_file *file, char const *name, int state, char const *group, char const *group_value,
	char const *from, unsigned long long size, char const *unlocks, int door)
{
	memset(file, 0, sizeof(*file));
	snprintf(file->name, sizeof(file->name), "%s", name);
	file->state = state;
	snprintf(file->group, sizeof(file->group), "%s", group);
	snprintf(file->group_value, sizeof(file->group_value), "%s", group_value);
	snprintf(file->from, sizeof(file->from), "%s", from);
	file->size = size;
	snprintf(file->unlocks, sizeof(file->unlocks), "%s", unlocks);
	file->door = door;
}

void ae_hub_data_stage_files(struct ae_hub_files *files, int state)
{
	int missing = state == AE_HUB_STATE_MISSING;
	char const *sounds_from = "MCC (Steam library)";

	memset(files, 0, sizeof(*files));
	file_set(&files->files[0], "BITMAPS.MAP", AE_HUB_FILE_FOUND, "CUSTOM EDITION MAPS", "214 ON THIS PC", sounds_from,
		95ull * 1024ull * 1024ull, "The pictures every Custom Edition map shares.", 0);
	file_set(&files->files[1], "SOUNDS.MAP", missing ? AE_HUB_FILE_MISSING : AE_HUB_FILE_FOUND, "", "", missing ? "" :
		sounds_from, 134ull * 1024ull * 1024ull, "The sounds every Custom Edition map shares. Without it, the 214 Custom "
		"Edition maps on this PC are left out of the map lists.", 1);
	file_set(&files->files[2], "LOC.MAP", AE_HUB_FILE_FOUND, "", "", "maps folder", 3ull * 1024ull * 1024ull,
		"The text every Custom Edition map shares.", 0);
	file_set(&files->files[3], "MCC ON THIS PC", AE_HUB_FILE_FOUND, "MCC MAPS", "STEAM", "MCC (Steam library)", 0,
		"MCC's own maps and its Workshop maps need it.", 0);
	file_set(&files->files[4], "MCC'S SHARED FILES", AE_HUB_FILE_FOUND, "", "", "MCC (Steam library)", 0,
		"The files MCC's and the Workshop's maps share.", 1);
	file_set(&files->files[5], "MODDED DISCS", AE_HUB_FILE_COUNT_ONLY, "MAP PACKS AND DISCS", "", "", 0,
		"A modded disc's maps replace the stock maps it has.", 2);
	snprintf(files->files[5].value, sizeof(files->files[5].value), "2 FOUND");
	files->count = 6;
	files->missing = missing ? 1 : 0;
}
