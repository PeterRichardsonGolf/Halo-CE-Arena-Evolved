/*
AE_GLUE_ITEM_SETS.C

The ITEM SETS page's engine side (ae_item_sets_data.h): the sets are the tag injector's profiles (port_tag_profiles.c), a map is
opened and its items taken as Content Evolved's own Tag Injector page does (cev/tag_injector.c), and a set plays through the two
settings the randomizer's ITEM SET choice writes (game.randomizer_tag_profile names its folder, game.tag_injection turns it on).
Those two settings are read where a campaign level played alone loads its tags and nowhere else (see ae_item_sets_rules.h).
*/

#include "cev/cev_names.h"

#include "cseries.h"
#include "tag_files/tag_groups.h"
#include "cev/tag_injector.h"
#include "ae_tag_inject.h"
#include "ae_maps_hub_data.h"
#include "ae_item_sets_data.h"
#include "../src/port_tag_profiles.h"
#include "../src/port_config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);
int config_write(const char *name, const char *value);

#define PROFILE_KEY "game.randomizer_tag_profile"
#define SWITCH_KEY "game.tag_injection"
/* DEV MODE's own profile (ae_tag_inject.c) is not a set of a person's */
#define DEV_PROFILE "ae_dev"

static int same_text(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

static int kind_of_group(unsigned long group)
{
	int kind;

	for (kind = 0; kind < AE_IS_KINDS; kind++)
		if (tag_injector_group_in_category(group, (short)kind))
			return kind;
	return -1;
}

static long profile_of(char const *folder)
{
	port_tag_profiles_scan();
	return folder && folder[0] ? port_tag_profiles_find(folder) : -1;
}

static int played_folder(char *folder, int size)
{
	char const *named = config_string(PROFILE_KEY);

	folder[0] = 0;
	if (!config_boolean(SWITCH_KEY) || !named || !named[0])
		return 0;
	snprintf(folder, (size_t)size, "%s", named);
	return 1;
}

static int engine_sets(struct ae_is_set *sets, int maximum)
{
	long count, each;
	int kept = 0;
	char played[64];

	port_tag_profiles_scan();
	count = port_tag_profiles_count();
	played_folder(played, sizeof(played));
	for (each = 0; each < count && kept < maximum; each++)
	{
		struct ae_is_set *set = &sets[kept];
		long tags = port_tag_profiles_tag_count(each), tag;
		char seen[8][48];
		int seen_count = 0;

		if (same_text(port_tag_profiles_folder(each), DEV_PROFILE))
			continue;
		memset(set, 0, sizeof(*set));
		snprintf(set->name, sizeof(set->name), "%s", port_tag_profiles_name(each));
		snprintf(set->folder, sizeof(set->folder), "%s", port_tag_profiles_folder(each));
		set->playing = played[0] && same_text(played, set->folder);
		set->items = tags;
		set->bytes = port_tag_profiles_size(each);
		for (tag = 0; tag < tags; tag++)
		{
			char const *map = port_tag_profiles_tag_map(each, tag);
			int kind = kind_of_group(port_tag_profiles_tag_group(each, tag)), s;

			set->on += port_tag_profiles_tag_enabled(each, tag) != 0;
			if (kind >= 0)
				set->kinds[kind]++;
			for (s = 0; s < seen_count && !same_text(seen[s], map); s++)
				;
			if (s == seen_count && seen_count < 8)
				snprintf(seen[seen_count++], sizeof(seen[0]), "%s", map);
			if (!set->first_map[0])
				snprintf(set->first_map, sizeof(set->first_map), "%s", map);
		}
		set->maps = seen_count;
		kept++;
	}
	return kept;
}

static int engine_play(char const *folder)
{
	if (!folder || !folder[0])
		return config_write(SWITCH_KEY, "false") && config_write(PROFILE_KEY, "");
	if (profile_of(folder) < 0)
		return 0;
	platform_log("item sets: %s plays", folder);
	return config_write(PROFILE_KEY, folder) && config_write(SWITCH_KEY, "true");
}

static int engine_create(char const *name, char *folder, int size)
{
	long profile;

	port_tag_profiles_scan();
	profile = port_tag_profiles_create(name);
	if (profile < 0)
		return 0;
	snprintf(folder, (size_t)size, "%s", port_tag_profiles_folder(profile));
	return 1;
}

static int engine_copy(char const *folder, char const *name, char *new_folder, int size)
{
	long profile = profile_of(folder), made;

	if (profile < 0)
		return 0;
	made = port_tag_profiles_duplicate(profile, name);
	if (made < 0)
		return 0;
	snprintf(new_folder, (size_t)size, "%s", port_tag_profiles_folder(made));
	return 1;
}

/* a set's name changed as the engine has no rename: a copy under the new name, then the old one deleted (a copy that can't
be made whole leaves the old one as it was); a set that plays keeps playing under its new folder */
static int engine_rename(char const *folder, char const *name, char *new_folder, int size)
{
	char played[64];
	int was_playing, kept;
	long old;

	if (!engine_copy(folder, name, new_folder, size))
		return 0;
	was_playing = played_folder(played, sizeof(played)) && same_text(played, folder);
	old = profile_of(folder);
	kept = old >= 0 && port_tag_profiles_delete(old);
	if (!kept)
		platform_log("item sets: %s was copied as %s but could not be deleted", folder, new_folder);
	if (was_playing)
		engine_play(new_folder);
	return 1;
}

static int engine_remove_set(char const *folder)
{
	char played[64];
	long profile = profile_of(folder);

	if (profile < 0)
		return 0;
	if (played_folder(played, sizeof(played)) && same_text(played, folder))
		engine_play(NULL);
	return port_tag_profiles_delete(profile);
}

static int engine_entries(char const *folder, struct ae_is_entry *out, int maximum)
{
	long profile = profile_of(folder), tags, tag;
	int kind, kept = 0;

	if (profile < 0)
		return 0;
	tags = port_tag_profiles_tag_count(profile);
	for (kind = 0; kind < AE_IS_KINDS; kind++)
		for (tag = 0; tag < tags && kept < maximum; tag++)
		{
			struct ae_is_entry *entry = &out[kept];

			if (kind_of_group(port_tag_profiles_tag_group(profile, tag)) != kind)
				continue;
			memset(entry, 0, sizeof(*entry));
			entry->tag = tag;
			snprintf(entry->path, sizeof(entry->path), "%s", port_tag_profiles_tag_name(profile, tag));
			snprintf(entry->map, sizeof(entry->map), "%s", port_tag_profiles_tag_map(profile, tag));
			entry->kind = kind;
			entry->on = port_tag_profiles_tag_enabled(profile, tag) != 0;
			snprintf(entry->failed, sizeof(entry->failed), "%s", port_tag_profiles_tag_option(profile, tag, "failed"));
			entry->bytes = port_tag_profiles_tag_size(profile, tag);
			entry->works = entry->bytes > 0;
			kept++;
		}
	return kept;
}

static int engine_entry_enable(char const *folder, long tag, int on)
{
	long profile = profile_of(folder);

	return profile >= 0 && port_tag_profiles_tag_set_enabled(profile, tag, on);
}

static int engine_entry_remove(char const *folder, long tag)
{
	long profile = profile_of(folder);

	return profile >= 0 && port_tag_profiles_tag_remove(profile, tag);
}

/* ---------- the maps */

static void utf8_of(wchar_t const *wide, char *out, int size)
{
	int length = 0;

	for (; wide && *wide && length < size - 4; wide++)
	{
		unsigned long c = (unsigned long)*wide;

		if (c < 0x80)
			out[length++] = (char)c;
		else if (c < 0x800)
		{
			out[length++] = (char)(0xC0 | (c >> 6));
			out[length++] = (char)(0x80 | (c & 0x3F));
		}
		else
		{
			out[length++] = (char)(0xE0 | (c >> 12));
			out[length++] = (char)(0x80 | ((c >> 6) & 0x3F));
			out[length++] = (char)(0x80 | (c & 0x3F));
		}
	}
	out[length] = 0;
}

static char listed_levels[AE_IS_MAPS_MOST][96];
static int listed_count;

static int engine_maps(char const *folder, struct ae_is_map *out, int maximum)
{
	static int indexes[AE_IS_MAPS_MOST];
	struct ae_is_entry *entries = malloc(sizeof(*entries) * AE_IS_ENTRIES_MOST);
	int count = ae_hub_data_query(AE_HUB_SHOW_ALL, "", 0, indexes, AE_IS_MAPS_MOST), each, kept = 0, entry_count;
	struct ae_hub_map *hub = malloc(sizeof(*hub));

	entry_count = entries ? engine_entries(folder, entries, AE_IS_ENTRIES_MOST) : 0;
	for (each = 0; hub && each < count && kept < maximum; each++)
	{
		struct ae_is_map *map = &out[kept];
		char stem[64], *dot;
		int e, source;

		if (!ae_hub_data_fill(indexes[each], hub, 0) || hub->unreadable)
			continue;
		source = hub->source == AE_HUB_SOURCE_CUSTOM_EDITION ? AE_IS_SOURCE_CE : hub->source == AE_HUB_SOURCE_MCC ? AE_IS_SOURCE_MCC :
			hub->source == AE_HUB_SOURCE_WORKSHOP_STEAM || hub->source == AE_HUB_SOURCE_WORKSHOP_INSTALLED ? AE_IS_SOURCE_WORKSHOP : -1;
		if (source < 0)
			continue;
		snprintf(stem, sizeof(stem), "%s", hub->file);
		dot = strrchr(stem, '.');
		if (dot)
			*dot = 0;
		memset(map, 0, sizeof(*map));
		snprintf(map->name, sizeof(map->name), "%s", hub->name);
		snprintf(map->file, sizeof(map->file), "%s", hub->file);
		if (source == AE_IS_SOURCE_CE)
			snprintf(map->level, sizeof(map->level), "%s", stem);
		else
			snprintf(map->level, sizeof(map->level), "%s", strchr(hub->list_name, '@') ? hub->list_name : stem);
		ae_hub_folder_label(hub->path, map->where, sizeof(map->where));
		map->source = source;
		map->campaign = hub->kind_campaign;
		map->size = hub->size;
		for (e = 0; e < entry_count; e++)
			if (same_text(entries[e].map, map->level))
				map->in_set++;
		snprintf(listed_levels[kept], sizeof(listed_levels[0]), "%s", map->level);
		kept++;
	}
	listed_count = kept;
	free(hub);
	free(entries);
	return kept;
}

static char const *engine_maps_reason(int source)
{
	static char reason[200];

	if (!(source == AE_IS_SOURCE_CE ? tag_injector_custom_edition_maps_available(reason, sizeof(reason)) :
		tag_injector_mcc_maps_available(reason, sizeof(reason))))
		return reason[0] ? reason : "Maps of this kind can't be opened now.";
	return NULL;
}

static int engine_map_open(int map, char *reason, int size)
{
	short found;

	if (map < 0 || map >= listed_count)
	{
		snprintf(reason, (size_t)size, "The map can't be read again: open it again.");
		return 0;
	}
	if (!tag_injector_maps_available(reason, size))
		return 0;
	found = ae_tag_inject_map_of_level(listed_levels[map]);
	if (found == NONE || !tag_injector_map_open(found))
	{
		snprintf(reason, (size_t)size, "The map can't be opened (the log says why).");
		platform_log("item sets: %s cannot be opened", listed_levels[map]);
		return 0;
	}
	return 1;
}

static void engine_map_close(void)
{
	tag_injector_map_close();
}

static long engine_kind_count(int kind)
{
	return tag_injector_map_opened() == NONE ? 0 : tag_injector_category_count((short)kind);
}

static char const *name_part(char const *path)
{
	char const *last = path, *each;

	for (each = path; each && *each; each++)
		if (*each == '\\' || *each == '/')
			last = each + 1;
	return last;
}

static int compare_items(void const *a, void const *b)
{
	struct ae_is_item const *x = a, *y = b;
	char const *p = name_part(x->path), *q = name_part(y->path);

	for (; *p && *q; p++, q++)
		if (tolower((unsigned char)*p) != tolower((unsigned char)*q))
			return tolower((unsigned char)*p) - tolower((unsigned char)*q);
	return tolower((unsigned char)*p) - tolower((unsigned char)*q);
}

static int engine_kind_items(int kind, char const *folder, struct ae_is_item *items, int maximum)
{
	long count = engine_kind_count(kind), each, profile = profile_of(folder);
	int kept = 0;

	for (each = 0; each < count && kept < maximum; each++)
	{
		char const *path = tag_injector_category_tag((short)kind, each);
		struct ae_is_item *item = &items[kept++];

		memset(item, 0, sizeof(*item));
		item->index = each;
		snprintf(item->path, sizeof(item->path), "%s", path ? path : "");
		item->made_up = tag_injector_tag_name_made((short)kind, each) != 0;
		item->in_set = profile >= 0 && tag_injector_tag_in_profile(profile, (short)kind, each) != NONE;
		item->parts = -1;
	}
	qsort(items, (size_t)kept, sizeof(*items), compare_items);
	return kept;
}

static void engine_item_look(int kind, int index, struct ae_is_item *item)
{
	char reason[200];
	long parts = tag_injector_tag_size((short)kind, index, reason, sizeof(reason));

	item->reason[0] = 0;
	item->refused = parts == NONE;
	item->parts = parts == NONE ? 0 : parts;
	item->bytes = 0;
	item->works = tag_injector_map_tag_usable((short)kind, item->path) != 0;
	if (item->refused)
		snprintf(item->reason, sizeof(item->reason), "%s", reason);
}

static int engine_kind_addable(int kind)
{
	return tag_injector_category_addable((short)kind) != 0;
}

static void engine_item_add(char const *folder, int kind, int index, struct ae_is_added *added)
{
	long profile = profile_of(folder), carried = 0;
	unsigned long long before;

	memset(added, 0, sizeof(*added));
	if (profile < 0)
	{
		snprintf(added->reason, sizeof(added->reason), "Make a profile first.");
		return;
	}
	before = port_tag_profiles_size(profile);
	if (!tag_injector_tag_add(profile, (short)kind, index, &carried, added->reason, sizeof(added->reason)) &&
		tag_injector_tag_in_profile(profile, (short)kind, index) == NONE)
	{
		if (!added->reason[0])
			snprintf(added->reason, sizeof(added->reason), "It can't be added (the log says why).");
		return;
	}
	added->ok = 1;
	added->carried = carried;
	if (carried > 0)
		snprintf(added->carried_names, sizeof(added->carried_names), "%s", kind == 1 ? "its weapons and equipment" : "other items it needs");
	added->kept = port_tag_profiles_size(profile) > before ? port_tag_profiles_size(profile) - before : 0;
}

static int engine_item_remove(char const *folder, int kind, int index)
{
	long profile = profile_of(folder), tag;

	if (profile < 0)
		return 0;
	tag = tag_injector_tag_in_profile(profile, (short)kind, index);
	return tag != NONE && port_tag_profiles_tag_remove(profile, tag);
}

static int engine_randomizer_on(void)
{
	return config_boolean("game.randomizer") != 0;
}

static struct ae_is_backend const engine =
{
	engine_sets, engine_play, engine_create, engine_copy, engine_rename, engine_remove_set, engine_entries, engine_entry_enable,
	engine_entry_remove, engine_maps, engine_maps_reason, engine_map_open, engine_map_close, engine_kind_count, engine_kind_items,
	engine_item_look, engine_kind_addable, engine_item_add, engine_item_remove, engine_randomizer_on,
};

struct ae_is_backend const *ae_is_backend_engine(void)
{
	return &engine;
}
