/*
AE_ITEM_SETS_DATA.C

The made-up ITEM SETS data of DEV MODE's `menu maps sets` (ae_item_sets_data.h): four sets, a dozen maps, the kinds of one map.
Everything is kept in memory: a set made, played, added to or removed from changes this table alone, and no file is read.
*/

#include <stdio.h>
#include <string.h>
#include "ae_item_sets_data.h"

#define MB (1024ull * 1024ull)

struct st_set { char name[48], folder[64]; int used; char body[3][256]; };
/* an item's settings, by the keys of the set's file */
struct st_options { char name[48], faction[16], klass[16], type[16], group[32], hud[24], replace[8]; };
struct st_entry { int set; struct ae_is_entry e; long next_tag; struct st_options options; };

static struct st_set sets[AE_IS_SETS_MOST];
enum { ST_ENTRIES_MOST = 128 };
static struct st_entry entries[ST_ENTRIES_MOST];
static int set_count, entry_count, playing = -1, open_map = -1, serial;

struct seed_item { int kind; char const *path; char const *map; int on; char const *failed; unsigned long long bytes; };

static struct seed_item const spirit[] =
{
	{ 0, "cmt\\weapons\\evolved_h1-spirit\\pistol\\pistol_spirit_variant_mp", "tsce_multiplayerv1", 1, "", 2100 * 1024ull },
	{ 0, "cmt\\weapons\\evolved_h1-spirit\\assault_rifle\\assault_rifle_mp", "tsce_multiplayerv1", 1, "", 3800 * 1024ull },
	{ 0, "cmt\\weapons\\evolved_h1-spirit\\battle_rifle\\battle_rifle_mp", "tsce_multiplayerv1", 0, "", 4600 * 1024ull },
	{ 0, "cmt\\weapons\\evolved_h1-spirit\\rocket_pod\\rocket_pod", "warthog_run", 0, "too large", 1400 * 1024ull },
	{ 1, "cmt\\characters\\evolved_h1-spirit\\brute\\brute_captain", "tsce_multiplayerv1", 1, "", 9ull * MB },
	{ 1, "cmt\\characters\\evolved_h1-spirit\\brute\\brute_major", "tsce_multiplayerv1", 1, "", 8ull * MB },
	{ 3, "cmt\\vehicles\\evolved_h1-spirit\\ghost\\ghost_mp", "warthog_run", 1, "", 7ull * MB },
	{ 4, "cmt\\characters\\evolved_h1-spirit\\cyborg\\bipeds\\cyborg_mp", "tsce_multiplayerv1", 1, "", 6ull * MB + 100 * 1024ull },
	{ 5, "cmt\\scenery\\evolved_h1-spirit\\crate\\crate_large", "tsce_multiplayerv1", 1, "", 900 * 1024ull },
};
static struct seed_item const armory[] =
{
	{ 0, "cmt\\weapons\\covenant\\plasma_rifle\\plasma_rifle_mp", "covenant_outpost", 1, "", 3ull * MB },
	{ 0, "cmt\\weapons\\covenant\\needler\\needler_mp", "covenant_outpost", 1, "", 3ull * MB },
	{ 0, "cmt\\weapons\\covenant\\carbine\\carbine_mp", "covenant_outpost", 1, "", 3ull * MB },
	{ 1, "cmt\\characters\\covenant\\elite\\elite_zealot", "covenant_outpost", 1, "", 4ull * MB },
	{ 4, "cmt\\characters\\covenant\\elite\\elite_mp", "covenant_outpost", 1, "", 5ull * MB },
	{ 1, "cmt\\characters\\covenant\\grunt\\grunt_heavy", "tsce_multiplayerv1", 1, "", 3ull * MB },
};
static struct seed_item const garage[] =
{
	{ 3, "cmt\\vehicles\\warthog\\warthog_mp", "warthog_run", 1, "", 20ull * MB },
	{ 3, "cmt\\vehicles\\scorpion\\scorpion_mp", "warthog_run", 1, "", 18ull * MB },
	{ 3, "cmt\\vehicles\\banshee\\banshee_mp", "warthog_run", 1, "", 4ull * MB },
	{ 0, "cmt\\weapons\\turret\\chain_gun", "warthog_run", 1, "", 4ull * MB },
};

static void add_seed(int set, struct seed_item const *seed, int count)
{
	int each;

	for (each = 0; each < count && entry_count < ST_ENTRIES_MOST; each++)
	{
		struct st_entry *entry = &entries[entry_count++];

		memset(entry, 0, sizeof(*entry));
		entry->set = set;
		entry->e.tag = ++serial;
		snprintf(entry->e.path, sizeof(entry->e.path), "%s", seed[each].path);
		snprintf(entry->e.map, sizeof(entry->e.map), "%s", seed[each].map);
		entry->e.kind = seed[each].kind;
		entry->e.on = seed[each].on;
		snprintf(entry->e.failed, sizeof(entry->e.failed), "%s", seed[each].failed);
		entry->e.bytes = seed[each].bytes;
		entry->e.works = 1;
		entry->e.fits_player = seed[each].kind == 4;
	}
}

static int add_set(char const *name)
{
	struct st_set *set;

	if (set_count >= AE_IS_SETS_MOST)
		return -1;
	set = &sets[set_count];
	memset(set, 0, sizeof(*set));
	snprintf(set->name, sizeof(set->name), "%s", name);
	snprintf(set->folder, sizeof(set->folder), "%s", name);
	set->used = 1;
	return set_count++;
}

static int set_of(char const *folder)
{
	int each;

	for (each = 0; each < set_count; each++)
		if (sets[each].used && !strcmp(sets[each].folder, folder))
			return each;
	return -1;
}

static int staged_sets(struct ae_is_set *out, int maximum)
{
	int each, kept = 0, e;

	for (each = 0; each < set_count && kept < maximum; each++)
	{
		struct ae_is_set *set = &out[kept];
		char seen[8][48];
		int seen_count = 0;

		if (!sets[each].used)
			continue;
		memset(set, 0, sizeof(*set));
		snprintf(set->name, sizeof(set->name), "%s", sets[each].name);
		snprintf(set->folder, sizeof(set->folder), "%s", sets[each].folder);
		set->playing = playing == each;
		for (e = 0; e < entry_count; e++)
		{
			int s;

			if (entries[e].set != each)
				continue;
			set->items++;
			set->on += entries[e].e.on != 0;
			set->bytes += entries[e].e.bytes;
			if (entries[e].e.kind >= 0 && entries[e].e.kind < AE_IS_KINDS)
				set->kinds[entries[e].e.kind]++;
			for (s = 0; s < seen_count && strcmp(seen[s], entries[e].e.map); s++)
				;
			if (s == seen_count && seen_count < 8)
				snprintf(seen[seen_count++], sizeof(seen[0]), "%s", entries[e].e.map);
			if (!set->first_map[0])
				snprintf(set->first_map, sizeof(set->first_map), "%s", entries[e].e.map);
		}
		set->maps = seen_count;
		kept++;
	}
	return kept;
}

static int staged_play(char const *folder)
{
	int set = folder && folder[0] ? set_of(folder) : -1;

	if (folder && folder[0] && set < 0)
		return 0;
	playing = set;
	return 1;
}

static int staged_create(char const *name, char *folder, int size)
{
	int set = add_set(name);

	if (set < 0)
		return 0;
	snprintf(folder, (size_t)size, "%s", sets[set].folder);
	return 1;
}

static int staged_copy(char const *folder, char const *name, char *new_folder, int size)
{
	int from = set_of(folder), to, each, count = entry_count;

	if (from < 0 || (to = add_set(name)) < 0)
		return 0;
	for (each = 0; each < count && entry_count < ST_ENTRIES_MOST; each++)
		if (entries[each].set == from)
		{
			entries[entry_count] = entries[each];
			entries[entry_count].set = to;
			entries[entry_count].e.tag = ++serial;
			entry_count++;
		}
	snprintf(new_folder, (size_t)size, "%s", sets[to].folder);
	return 1;
}

static int staged_rename(char const *folder, char const *name, char *new_folder, int size)
{
	int set = set_of(folder);

	if (set < 0)
		return 0;
	snprintf(sets[set].name, sizeof(sets[set].name), "%s", name);
	snprintf(sets[set].folder, sizeof(sets[set].folder), "%s", name);
	snprintf(new_folder, (size_t)size, "%s", sets[set].folder);
	return 1;
}

static int staged_remove_set(char const *folder)
{
	int set = set_of(folder), each;

	if (set < 0)
		return 0;
	sets[set].used = 0;
	if (playing == set)
		playing = -1;
	for (each = 0; each < entry_count; each++)
		if (entries[each].set == set)
			entries[each].set = -1;
	return 1;
}

static int staged_entries(char const *folder, struct ae_is_entry *out, int maximum)
{
	int set = set_of(folder), kind, each, kept = 0;

	if (set < 0)
		return 0;
	for (kind = 0; kind < AE_IS_KINDS; kind++)
		for (each = 0; each < entry_count && kept < maximum; each++)
			if (entries[each].set == set && entries[each].e.kind == kind)
			{
				out[kept] = entries[each].e;
				snprintf(out[kept].name, sizeof(out[kept].name), "%s", entries[each].options.name);
				kept++;
			}
	return kept;
}

static int staged_entry_enable(char const *folder, long tag, int on)
{
	int set = set_of(folder), each;

	for (each = 0; set >= 0 && each < entry_count; each++)
		if (entries[each].set == set && entries[each].e.tag == tag)
		{
			entries[each].e.on = on;
			if (on)
				entries[each].e.failed[0] = 0;
			return 1;
		}
	return 0;
}

static int staged_entry_remove(char const *folder, long tag)
{
	int set = set_of(folder), each;

	for (each = 0; set >= 0 && each < entry_count; each++)
		if (entries[each].set == set && entries[each].e.tag == tag)
		{
			entries[each].set = -1;
			return 1;
		}
	return 0;
}

struct map_seed { char const *name, *file, *where; int source, campaign; unsigned long long size; };
static struct map_seed const map_seeds[] =
{
	{ "Sidewinder Night", "sidewinder_night", "maps_ce \xE2\x80\xBA sidewinder", AE_IS_SOURCE_CE, 0, 96 * MB },
	{ "Sniper Canyon", "sniper_canyon", "maps_ce \xE2\x80\xBA sniper", AE_IS_SOURCE_CE, 0, 88 * MB },
	{ "tsce_multiplayerv1", "tsce_multiplayerv1", "maps_ce \xE2\x80\xBA tsce", AE_IS_SOURCE_CE, 0, 112 * MB },
	{ "tsce_campaign_a10", "tsce_campaign_a10", "maps_ce \xE2\x80\xBA tsce", AE_IS_SOURCE_CE, 1, 140 * MB },
	{ "Twin Peaks", "twin_peaks", "maps_ce \xE2\x80\xBA comp", AE_IS_SOURCE_CE, 0, 71 * MB },
	{ "Valley Run", "valley_run", "maps_ce \xE2\x80\xBA comp", AE_IS_SOURCE_CE, 0, 64 * MB },
	{ "warthog_run", "warthog_run", "maps_ce", AE_IS_SOURCE_CE, 0, 82 * MB },
	{ "covenant_outpost", "covenant_outpost", "maps_ce", AE_IS_SOURCE_CE, 0, 77 * MB },
	{ "Coldwater Pass", "coldwater_pass@mcc", "", AE_IS_SOURCE_MCC, 0, 52 * MB },
	{ "Delta Ruins", "delta_ruins@ws1", "", AE_IS_SOURCE_WORKSHOP, 0, 60 * MB },
};

static int staged_maps(char const *folder, struct ae_is_map *out, int maximum)
{
	int each, e, set = folder ? set_of(folder) : -1, kept = 0;

	for (each = 0; each < (int)(sizeof(map_seeds) / sizeof(map_seeds[0])) && kept < maximum; each++)
	{
		struct ae_is_map *map = &out[kept++];

		memset(map, 0, sizeof(*map));
		snprintf(map->name, sizeof(map->name), "%s", map_seeds[each].name);
		snprintf(map->file, sizeof(map->file), "%s", map_seeds[each].file);
		snprintf(map->level, sizeof(map->level), "%s", map_seeds[each].file);
		snprintf(map->where, sizeof(map->where), "%s", map_seeds[each].where);
		map->source = map_seeds[each].source;
		map->campaign = map_seeds[each].campaign;
		map->size = map_seeds[each].size;
		for (e = 0; set >= 0 && e < entry_count; e++)
			if (entries[e].set == set && !strcmp(entries[e].e.map, map_seeds[each].file))
				map->in_set++;
	}
	return kept;
}

/* a Custom Edition map can't open while one is played: the staged page shows it for the map named so (cant-open scene) */
static int block_ce;

static char const *staged_maps_reason(int source)
{
	if (block_ce && source == AE_IS_SOURCE_CE)
		return "A Custom Edition map is being played. While one is, no other Custom Edition map can be opened here.";
	return NULL;
}

static int staged_map_open(int map, char *reason, int size)
{
	if (map < 0 || map >= (int)(sizeof(map_seeds) / sizeof(map_seeds[0])))
	{
		snprintf(reason, (size_t)size, "The map can't be read again: open it again.");
		return 0;
	}
	if (block_ce && map_seeds[map].source == AE_IS_SOURCE_CE)
	{
		snprintf(reason, (size_t)size, "%s", staged_maps_reason(AE_IS_SOURCE_CE));
		return 0;
	}
	open_map = map;
	return 1;
}

static void staged_map_close(void)
{
	open_map = -1;
}

static char const *const weapon_paths[] = { "cmt\\weapons\\evolved_h1-spirit\\assault_rifle\\assault_rifle_mp",
	"cmt\\weapons\\evolved_h1-spirit\\battle_rifle\\battle_rifle_mp", "cmt\\weapons\\flamethrower\\flamethrower",
	"cmt\\weapons\\turret\\gun_turret_mounted", "cmt\\weapons\\needler\\needler",
	"cmt\\weapons\\evolved_h1-spirit\\pistol\\pistol_spirit_variant_mp",
	"cmt\\weapons\\covenant\\plasma_rifle\\plasma_rifle_brute_variant_multiplayer_final", "cmt\\weapons\\rocket_launcher\\rocket_launcher" };
static char const *const ai_paths[] = { "cmt\\characters\\covenant\\elite\\elite_zealot", "cmt\\characters\\covenant\\grunt\\grunt_heavy",
	"cmt\\characters\\evolved_h1-spirit\\brute\\brute_major" };
static char const *const equipment_paths[] = { "cmt\\equipment\\frag_grenade\\frag_grenade", "cmt\\equipment\\overshield\\overshield",
	"cmt\\equipment\\health_pack\\health_pack" };
static char const *const vehicle_paths[] = { "cmt\\vehicles\\warthog\\warthog_mp", "cmt\\vehicles\\ghost\\ghost_mp" };
static char const *const biped_paths[] = { "cmt\\characters\\evolved_h1-spirit\\cyborg\\bipeds\\cyborg_mp",
	"cmt\\characters\\covenant\\elite\\elite_mp" };
static char const *const device_paths[] = { "cmt\\devices\\door\\door_large", "cmt\\devices\\lift\\lift_small" };
static char const *const sky_paths[] = { "cmt\\skies\\sky_night\\sky_night" };
static char const *const hands_paths[] = { "cmt\\characters\\evolved_h1-spirit\\cyborg\\fp\\fp" };

static int paths_of(int kind, char const *const **paths, int *count)
{
#define LIST(array) do { *paths = array; *count = (int)(sizeof(array) / sizeof(array[0])); return 1; } while (0)
	switch (kind)
	{
	case 0: LIST(weapon_paths);
	case 1: LIST(ai_paths);
	case 2: LIST(equipment_paths);
	case 3: LIST(vehicle_paths);
	case 4: LIST(biped_paths);
	case 6: LIST(device_paths);
	case 7: LIST(sky_paths);
	case 8: LIST(hands_paths);
	}
#undef LIST
	*paths = NULL;
	*count = 0;
	return 0;
}

/* scenery: a dozen made-up props (the search line is for kinds like this one) */
enum { SCENERY_COUNT = 12 };

static long staged_kind_count(int kind)
{
	char const *const *paths;
	int count;

	if (open_map < 0)
		return 0;
	if (kind == 5)
		return SCENERY_COUNT;
	paths_of(kind, &paths, &count);
	return count;
}

static void item_path(int kind, int index, char *path, int size)
{
	char const *const *paths;
	int count;

	if (kind == 5)
		snprintf(path, (size_t)size, "cmt\\scenery\\props\\crate\\crate_%02d", index + 1);
	else if (paths_of(kind, &paths, &count) && index >= 0 && index < count)
		snprintf(path, (size_t)size, "%s", paths[index]);
	else
		path[0] = 0;
}

static int in_set_path(int set, char const *path)
{
	int each;

	for (each = 0; set >= 0 && each < entry_count; each++)
		if (entries[each].set == set && !strcmp(entries[each].e.path, path))
			return 1;
	return 0;
}

static void item_fill_lookup(struct ae_is_item *item)
{
	unsigned int hash = 7, at;

	for (at = 0; item->path[at]; at++)
		hash = hash * 31u + (unsigned char)item->path[at];
	item->parts = 12 + (long)(hash % 40u);
	item->bytes = (1400ull + (unsigned long long)(hash % 4600u)) * 1024ull;
	item->refused = strstr(item->path, "gun_turret_mounted") != NULL;
	item->works = strstr(item->path, "rocket_launcher") == NULL;
	item->reason[0] = 0;
	if (item->refused)
	{
		snprintf(item->reason, sizeof(item->reason), "It has no first-person model or animations: its map never lets a player hold it, "
			"and it cannot be added.");
		item->parts = 0;
		item->bytes = 0;
	}
}

static int staged_kind_items(int kind, char const *folder, struct ae_is_item *items, int maximum)
{
	int count = (int)staged_kind_count(kind), each, set = folder ? set_of(folder) : -1;

	for (each = 0; each < count && each < maximum; each++)
	{
		memset(&items[each], 0, sizeof(items[each]));
		item_path(kind, each, items[each].path, sizeof(items[each].path));
		items[each].index = each;
		items[each].in_set = in_set_path(set, items[each].path);
		items[each].parts = -1;
	}
	return count < maximum ? count : maximum;
}

static void staged_item_look(int kind, int index, struct ae_is_item *item)
{
	(void)kind;
	(void)index;
	item_fill_lookup(item);
}

static int staged_kind_addable(int kind)
{
	return kind == 0 || kind == 1 || kind == 2 || kind == 3 || kind == 4;
}

static void staged_item_add(char const *folder, int kind, int index, struct ae_is_added *added)
{
	int set = set_of(folder);
	struct ae_is_item item;
	struct st_entry *entry;

	memset(added, 0, sizeof(*added));
	memset(&item, 0, sizeof(item));
	item_path(kind, index, item.path, sizeof(item.path));
	item_fill_lookup(&item);
	if (set < 0 || open_map < 0)
	{
		snprintf(added->reason, sizeof(added->reason), "Make a profile first.");
		return;
	}
	if (item.refused)
	{
		snprintf(added->reason, sizeof(added->reason), "%s", item.reason);
		return;
	}
	if (in_set_path(set, item.path) || entry_count >= ST_ENTRIES_MOST)
	{
		added->ok = in_set_path(set, item.path);
		return;
	}
	entry = &entries[entry_count++];
	memset(entry, 0, sizeof(*entry));
	entry->set = set;
	entry->e.tag = ++serial;
	snprintf(entry->e.path, sizeof(entry->e.path), "%s", item.path);
	snprintf(entry->e.map, sizeof(entry->e.map), "%s", map_seeds[open_map].file);
	entry->e.kind = kind;
	entry->e.on = 1;
	entry->e.works = 1;
	entry->e.fits_player = kind == 4;
	entry->e.bytes = item.bytes * 6 / 10;
	added->ok = 1;
	added->read = item.bytes;
	added->kept = entry->e.bytes;
	if (kind == 4)
	{
		added->carried = 1;
		snprintf(added->carried_names, sizeof(added->carried_names), "its first-person hands");
	}
	else if (kind == 1)
	{
		added->carried = 2;
		snprintf(added->carried_names, sizeof(added->carried_names), "plasma_rifle, frag_grenade");
	}
}

static int staged_item_remove(char const *folder, int kind, int index)
{
	int set = set_of(folder), each;
	char path[256];

	item_path(kind, index, path, sizeof(path));
	for (each = 0; set >= 0 && each < entry_count; each++)
		if (entries[each].set == set && !strcmp(entries[each].e.path, path))
		{
			entries[each].set = -1;
			return 1;
		}
	return 0;
}

static int staged_randomizer_on(void)
{
	return 1;
}

static struct st_entry *entry_of(char const *folder, long tag)
{
	int set = set_of(folder), each;

	for (each = 0; set >= 0 && each < entry_count; each++)
		if (entries[each].set == set && entries[each].e.tag == tag)
			return &entries[each];
	return NULL;
}

static char *option_field(struct st_options *options, char const *key, int *size)
{
	struct { char const *key; char *field; int size; } const table[] = {
		{ "name", options->name, (int)sizeof(options->name) }, { "faction", options->faction, (int)sizeof(options->faction) },
		{ "class", options->klass, (int)sizeof(options->klass) }, { "type", options->type, (int)sizeof(options->type) },
		{ "custom_type", options->group, (int)sizeof(options->group) }, { "hud", options->hud, (int)sizeof(options->hud) },
		{ "replace", options->replace, (int)sizeof(options->replace) } };
	int each;

	for (each = 0; each < 7; each++)
		if (!strcmp(table[each].key, key))
		{
			*size = table[each].size;
			return table[each].field;
		}
	return NULL;
}

static int staged_entry_option(char const *folder, long tag, char const *key, char *value, int size)
{
	struct st_entry *entry = entry_of(folder, tag);
	int field_size;
	char *field = entry ? option_field(&entry->options, key, &field_size) : NULL;

	if (!field)
		return 0;
	snprintf(value, (size_t)size, "%s", field);
	return 1;
}

static int staged_entry_set_option(char const *folder, long tag, char const *key, char const *value)
{
	struct st_entry *entry = entry_of(folder, tag);
	int field_size;
	char *field = entry ? option_field(&entry->options, key, &field_size) : NULL;

	if (!field)
		return 0;
	snprintf(field, (size_t)field_size, "%s", value);
	return 1;
}

static char *body_field(char const *folder, char const *key)
{
	int set = set_of(folder);

	if (set < 0)
		return NULL;
	return !strcmp(key, "player_biped") ? sets[set].body[0] : !strcmp(key, "player_hands") ? sets[set].body[1] :
		!strcmp(key, "player_movement") ? sets[set].body[2] : NULL;
}

static int staged_body_option(char const *folder, char const *key, char *value, int size)
{
	char *field = body_field(folder, key);

	if (!field)
		return 0;
	snprintf(value, (size_t)size, "%s", field);
	return 1;
}

static int staged_body_set_option(char const *folder, char const *key, char const *value)
{
	char *field = body_field(folder, key);

	if (!field)
		return 0;
	snprintf(field, 256, "%s", value);
	return 1;
}

/* the facts the made-up bipeds are checked by (ae_biped_pack_rules.h's rule, as the engine runs it): the stock player, a biped of
about its size that holds every weapon and rides every seat, and an elite that is not built for a player's physics and lacks three seats */
static void staged_entry_fit(char const *folder, long tag, struct ae_is_fit *fit)
{
	struct st_entry *entry = entry_of(folder, tag);
	struct ae_biped_facts stock, biped;

	memset(fit, 0, sizeof(*fit));
	if (!entry || entry->e.kind != 4)
		return;
	memset(&stock, 0, sizeof(stock));
	stock.radius = 0.70f;
	stock.standing_height = 0.70f;
	stock.standing_eye = 0.64f;
	stock.crouching_eye = 0.40f;
	stock.has_animation_graph = 1;
	stock.uses_player_physics = 1;
	stock.seats_stock = 14;
	stock.labels_checked = 11;
	biped = stock;
	if (strstr(entry->e.path, "elite"))
	{
		biped.radius = 0.71f;
		biped.standing_height = 0.72f;
		biped.standing_eye = 0.66f;
		biped.crouching_eye = 0.41f;
		biped.uses_player_physics = 0;
		biped.seats_missing = 3;
		snprintf(biped.seat_first, sizeof(biped.seat_first), "w-driver");
	}
	else
	{
		biped.standing_height = 0.714f;
		biped.standing_eye = 0.65f;
	}
	ae_is_fit_from_facts(&stock, &biped, fit);
}

static int staged_replace_known(void)
{
	return 1;
}

/* ---------- the campaign swaps, the lobby's choice and the firing range */

struct st_option { char key[24], value[256]; };
struct st_swap { char name[48], file[64]; int used; struct st_option option[14]; int option_count; };
struct st_list { int swap; char section[12], key[40], path[256]; };
enum { ST_LIST_MOST = 256 };
static struct st_swap swaps_made[AE_IS_SWAPS_MOST];
static struct st_list lists_made[ST_LIST_MOST];
static int swap_count, list_made;
/* the lobby's choice: AE_IS_PICK_*, and its swap file or set folder */
static int picked_kind;
static char picked_id[64];

static int swap_of(char const *file)
{
	int each;

	for (each = 0; each < swap_count; each++)
		if (swaps_made[each].used && !strcmp(swaps_made[each].file, file))
			return each;
	return -1;
}

static char const *swap_value(int swap, char const *key)
{
	int each;

	for (each = 0; swap >= 0 && each < swaps_made[swap].option_count; each++)
		if (!strcmp(swaps_made[swap].option[each].key, key))
			return swaps_made[swap].option[each].value;
	return "";
}

static int swap_write(int swap, char const *key, char const *value)
{
	int each;

	if (swap < 0)
		return 0;
	for (each = 0; each < swaps_made[swap].option_count; each++)
		if (!strcmp(swaps_made[swap].option[each].key, key))
		{
			snprintf(swaps_made[swap].option[each].value, sizeof(swaps_made[swap].option[0].value), "%s", value);
			return 1;
		}
	if (swaps_made[swap].option_count >= 14)
		return 0;
	snprintf(swaps_made[swap].option[each].key, sizeof(swaps_made[swap].option[0].key), "%s", key);
	snprintf(swaps_made[swap].option[each].value, sizeof(swaps_made[swap].option[0].value), "%s", value);
	swaps_made[swap].option_count++;
	return 1;
}

static int swap_add(char const *name, char const *set_folder)
{
	struct st_swap *swap;

	if (swap_count >= AE_IS_SWAPS_MOST)
		return -1;
	swap = &swaps_made[swap_count];
	memset(swap, 0, sizeof(*swap));
	swap->used = 1;
	snprintf(swap->name, sizeof(swap->name), "%s", name);
	snprintf(swap->file, sizeof(swap->file), "%s", name);
	swap_write(swap_count, "profile", set_folder);
	swap_write(swap_count, "weapon_rotation", "add");
	swap_write(swap_count, "weapon_replace", "random");
	swap_write(swap_count, "enemy_rotation", "add");
	swap_write(swap_count, "enemy_replace", "random");
	return swap_count++;
}

static void list_add(int swap, char const *section, char const *key, char const *path)
{
	struct st_list *list;

	if (list_made >= ST_LIST_MOST)
		return;
	list = &lists_made[list_made++];
	list->swap = swap;
	snprintf(list->section, sizeof(list->section), "%s", section);
	snprintf(list->key, sizeof(list->key), "%s", key);
	snprintf(list->path, sizeof(list->path), "%s", path);
}

static int staged_swaps(struct ae_is_swap *out, int maximum)
{
	int each, kept = 0;

	for (each = 0; each < swap_count && kept < maximum; each++)
	{
		struct ae_is_swap *swap = &out[kept];
		int set = set_of(swap_value(each, "profile")), e;

		if (!swaps_made[each].used)
			continue;
		memset(swap, 0, sizeof(*swap));
		snprintf(swap->name, sizeof(swap->name), "%s", swaps_made[each].name);
		snprintf(swap->file, sizeof(swap->file), "%s", swaps_made[each].file);
		snprintf(swap->set_folder, sizeof(swap->set_folder), "%s", swap_value(each, "profile"));
		swap->set_found = set >= 0;
		if (set >= 0)
		{
			snprintf(swap->set_name, sizeof(swap->set_name), "%s", sets[set].name);
			for (e = 0; e < entry_count; e++)
				swap->set_items_on += entries[e].set == set && entries[e].e.on;
		}
		swap->in_use = picked_kind == AE_IS_PICK_SWAP && !strcmp(picked_id, swap->file);
		snprintf(swap->weapon_replace, sizeof(swap->weapon_replace), "%s", swap_value(each, "weapon_replace"));
		snprintf(swap->enemy_replace, sizeof(swap->enemy_replace), "%s", swap_value(each, "enemy_replace"));
		kept++;
	}
	return kept;
}

static int staged_swap_create(char const *name, char const *set_folder, char *file, int size)
{
	int swap = swap_of(name) >= 0 ? -1 : swap_add(name, set_folder);

	if (swap < 0)
		return 0;
	snprintf(file, (size_t)size, "%s", swaps_made[swap].file);
	return 1;
}

static int staged_swap_remove(char const *file)
{
	int swap = swap_of(file), each;

	if (swap < 0)
		return 0;
	swaps_made[swap].used = 0;
	for (each = 0; each < list_made; each++)
		if (lists_made[each].swap == swap)
			lists_made[each].swap = -1;
	if (picked_kind == AE_IS_PICK_SWAP && !strcmp(picked_id, file))
		picked_kind = AE_IS_PICK_NONE;
	return 1;
}

static int staged_swap_rename(char const *file, char const *name)
{
	int swap = swap_of(file);

	if (swap < 0)
		return 0;
	snprintf(swaps_made[swap].name, sizeof(swaps_made[swap].name), "%s", name);
	return 1;
}

static int staged_swap_option(char const *file, char const *key, char *value, int size)
{
	int swap = swap_of(file);

	if (swap < 0)
		return 0;
	snprintf(value, (size_t)size, "%s", !strcmp(key, "name") ? swaps_made[swap].name : swap_value(swap, key));
	return 1;
}

static int staged_swap_set_option(char const *file, char const *key, char const *value)
{
	int swap = swap_of(file);

	if (!strcmp(key, "name"))
		return staged_swap_rename(file, value);
	return swap_write(swap, key, value);
}

static int staged_swap_list_count(char const *file, char const *section, char const *key)
{
	int swap = swap_of(file), each, count = 0;

	for (each = 0; each < list_made; each++)
		count += lists_made[each].swap == swap && !strcmp(lists_made[each].section, section) && !strcmp(lists_made[each].key, key);
	return count;
}

static int staged_swap_list_get(char const *file, char const *section, char const *key, int entry, char *path, int size)
{
	int swap = swap_of(file), each, count = 0;

	for (each = 0; each < list_made; each++)
		if (lists_made[each].swap == swap && !strcmp(lists_made[each].section, section) && !strcmp(lists_made[each].key, key) && count++ == entry)
		{
			snprintf(path, (size_t)size, "%s", lists_made[each].path);
			return 1;
		}
	return 0;
}

static int staged_swap_list_has(char const *file, char const *section, char const *key, char const *path)
{
	int swap = swap_of(file), each;

	for (each = 0; each < list_made; each++)
		if (lists_made[each].swap == swap && !strcmp(lists_made[each].section, section) && !strcmp(lists_made[each].key, key) &&
			!strcmp(lists_made[each].path, path))
			return 1;
	return 0;
}

static int staged_swap_list_set(char const *file, char const *section, char const *key, char const *path, int on)
{
	int swap = swap_of(file), each;

	if (swap < 0)
		return 0;
	for (each = 0; each < list_made; each++)
		if (lists_made[each].swap == swap && !strcmp(lists_made[each].section, section) && !strcmp(lists_made[each].key, key) &&
			!strcmp(lists_made[each].path, path))
		{
			if (!on)
				lists_made[each].swap = -1;
			return 1;
		}
	if (on)
		list_add(swap, section, key, path);
	return 1;
}

static int staged_picked(char *id, int size)
{
	snprintf(id, (size_t)size, "%s", picked_kind == AE_IS_PICK_NONE ? "" : picked_id);
	return picked_kind;
}

static int staged_pick(int pick, char const *id)
{
	picked_kind = pick;
	snprintf(picked_id, sizeof(picked_id), "%s", pick == AE_IS_PICK_NONE || !id ? "" : id);
	if (pick == AE_IS_PICK_SET)
		playing = set_of(id);
	else if (pick == AE_IS_PICK_NONE)
		playing = -1;
	return 1;
}

static int staged_range_ready(void)
{
	return !block_ce;
}

static int staged_range_start(char const *folder, char *why, int size)
{
	(void)folder;
	if (block_ce)
	{
		snprintf(why, (size_t)size, "no Xbox-format bloodgulch.map in maps");
		return 0;
	}
	return 1;
}

static int staged_entry_stock(char const *folder, long tag)
{
	struct st_entry *entry = entry_of(folder, tag);

	return entry && (entry->e.kind == 0 || entry->e.kind == 1) && (strstr(entry->e.path, "pistol") || strstr(entry->e.path, "assault_rifle") ||
		strstr(entry->e.path, "grunt") || strstr(entry->e.path, "elite"));
}

static struct ae_is_backend const staged =
{
	staged_sets, staged_play, staged_create, staged_copy, staged_rename, staged_remove_set, staged_entries, staged_entry_enable,
	staged_entry_remove, staged_maps, staged_maps_reason, staged_map_open, staged_map_close, staged_kind_count, staged_kind_items,
	staged_item_look, staged_kind_addable, staged_item_add, staged_item_remove, staged_randomizer_on, staged_entry_option,
	staged_entry_set_option, staged_body_option, staged_body_set_option, staged_entry_fit, staged_replace_known,
	staged_entry_stock, staged_swaps, staged_swap_create, staged_swap_remove, staged_swap_rename, staged_swap_option, staged_swap_set_option,
	staged_swap_list_count, staged_swap_list_get, staged_swap_list_has, staged_swap_list_set, staged_picked, staged_pick, staged_range_ready,
	staged_range_start,
};

struct ae_is_backend const *ae_is_backend_staged(int empty)
{
	set_count = 0;
	entry_count = 0;
	playing = -1;
	open_map = -1;
	serial = 0;
	block_ce = 0;
	swap_count = 0;
	list_made = 0;
	picked_kind = AE_IS_PICK_NONE;
	picked_id[0] = 0;
	if (!empty)
	{
		add_set("Spirit Kit");
		add_set("Covenant Armory");
		add_set("Warthog Garage");
		add_set("Test Bench");
		add_seed(0, spirit, (int)(sizeof(spirit) / sizeof(spirit[0])));
		add_seed(1, armory, (int)(sizeof(armory) / sizeof(armory[0])));
		add_seed(2, garage, (int)(sizeof(garage) / sizeof(garage[0])));
		playing = 0;
		/* (three swaps: the one in use, one that picks enemies by enemy, and one whose set is gone) */
		{
			int run = swap_add("Spirit Run", "Spirit Kit"), brute = swap_add("Brute Invasion", "Covenant Armory"), old = swap_add("Old Run", "Gone Set");

			swap_write(run, "weapon_replace", "manual");
			swap_write(run, "enemy_rotation", "only");
			swap_write(run, "player_biped", "cmt\\characters\\evolved_h1-spirit\\cyborg\\bipeds\\cyborg_mp");
			list_add(run, "weapons", "pistol", "cmt\\weapons\\evolved_h1-spirit\\pistol\\pistol_spirit_variant_mp");
			list_add(run, "weapons", "pistol", "cmt\\weapons\\evolved_h1-spirit\\assault_rifle\\assault_rifle_mp");
			list_add(run, "weapons", "plasma_rifle", "cmt\\weapons\\evolved_h1-spirit\\battle_rifle\\battle_rifle_mp");
			list_add(run, "weapons", "shotgun", "cmt\\weapons\\evolved_h1-spirit\\assault_rifle\\assault_rifle_mp");
			swap_write(brute, "enemy_replace", "manual");
			list_add(brute, "enemies", "elite_major", "cmt\\characters\\covenant\\elite\\elite_zealot");
			list_add(brute, "enemies", "elite_major", "original");
			list_add(brute, "loadout", "elite_major", "cmt\\weapons\\covenant\\plasma_rifle\\plasma_rifle_mp");
			(void)old;
			picked_kind = AE_IS_PICK_SWAP;
			snprintf(picked_id, sizeof(picked_id), "%s", swaps_made[run].file);
		}
	}
	return &staged;
}

/* the staged table's only extra: the CAN'T OPEN scene turns the Custom Edition block on */
void ae_is_staged_block_ce(int block)
{
	block_ce = block;
}
