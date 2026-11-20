/*
AE_COLLECTIONS_DATA.C

The collections page's model. See ae_collections_data.h.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ae_mcc.h"
#include "../src/ae_mcc_workshop.h"
#include "../src/ae_steam_client.h"
#include "ae_collections_data.h"
#include "ae_motion.h"
#include "ae_map_library.h"

/* the two collections the offline playlists play (their Steam ids) */
static char const *const playlist_ids[] = { "2899609719", "3001370390" };

char const *ae_coll_sort_name(int sort)
{
	static char const *const names[AE_COLL_SORTS] = { "PLAYLISTS FIRST", "NAME", "SIZE" };

	return sort >= 0 && sort < AE_COLL_SORTS ? names[sort] : "";
}

static int is_playlist(char const *id)
{
	int index;

	for (index = 0; index < (int)(sizeof(playlist_ids) / sizeof(playlist_ids[0])); index++)
		if (!strcmp(id, playlist_ids[index]))
			return 1;
	return 0;
}

static unsigned long long coll_bytes(struct ae_coll const *coll)
{
	struct ae_coll_summary summary;

	ae_coll_summarize(coll, &summary);
	return summary.total_bytes;
}

static int order_before(struct ae_coll const *a, struct ae_coll const *b, int sort)
{
	/* (the playlists' two first, in Steam's order of them: Social, then Pro Edition) */
	if (a->playlist != b->playlist)
		return a->playlist > b->playlist;
	if (a->playlist && b->playlist && sort == AE_COLL_SORT_PLAYLISTS)
		return strcmp(a->id, b->id) < 0;
	if (sort == AE_COLL_SORT_SIZE && coll_bytes(a) != coll_bytes(b))
		return coll_bytes(a) > coll_bytes(b);
	if (sort == AE_COLL_SORT_PLAYLISTS && !a->playlist)
		return 0;
	return strcmp(a->name, b->name) < 0;
}

static void sort_model(struct ae_coll_model *model, int sort)
{
	static struct ae_coll held;
	int outer, inner;

	for (outer = 1; outer < model->count; outer++)
		for (inner = outer; inner > 0 && order_before(&model->colls[inner], &model->colls[inner - 1], sort); inner--)
		{
			held = model->colls[inner];
			model->colls[inner] = model->colls[inner - 1];
			model->colls[inner - 1] = held;
		}
}

/* ---------- the real model */

static struct ae_mcc_workshop_map *catalog;
enum { CATALOG_MOST = 1024 };

static int in_games_folder(char const *path)
{
	return strstr(path, "/mcc_maps/") != NULL || strstr(path, "\\mcc_maps\\") != NULL;
}

static int catalog_find(int count, char const *id)
{
	int index;

	for (index = 0; index < count; index++)
		if (!strcmp(catalog[index].item_id, id))
			return index;
	return -1;
}

void ae_coll_data_read(struct ae_coll_model *model, int sort, int force)
{
	static unsigned long last;
	static int valid;
	struct ae_steam_status status;
	char ids[AE_COLL_ITEMS + 1][24];
	char title[64];
	unsigned long now = ae_motion_now();
	int total, index, catalog_count, asked = 0;

	if (valid && !force && now - last < 1000ul)
		return;
	valid = 1;
	last = now;
	ae_mcc_workshop_collections_ask();
	memset(model, 0, sizeof(*model));
	ae_steam_client_status(&status);
	model->signed_in = status.session == AE_STEAM_SESSION_IN;
	model->free_bytes = ae_mcc_workshop_free_bytes();
	model->free_known = model->free_bytes != 0;
	if (!catalog)
		catalog = malloc(sizeof(*catalog) * CATALOG_MOST);
	catalog_count = catalog ? ae_mcc_workshop_catalog_copy(catalog, CATALOG_MOST) : 0;
	total = ae_mcc_workshop_collection_count();
	model->loading = total == 0 && !ae_mcc_workshop_collections_finished();
	model->empty = total == 0 && !model->loading;
	for (index = 0; index < total && model->count < AE_COLL_MOST; index++)
	{
		struct ae_coll *coll = &model->colls[model->count];
		unsigned long long asked_ids[AE_COLL_ITEMS];
		int count, item, unknown = 0;

		memset(coll, 0, sizeof(*coll));
		count = ae_mcc_workshop_collection_items(index, coll->id, AE_COLL_ID, ids, AE_COLL_ITEMS + 1);
		if (!coll->id[0] || count <= 0)
			continue;
		/* (more maps than the view holds: the collection is listed as TOO MANY MAPS and not opened) */
		if (count > AE_COLL_ITEMS)
		{
			count = AE_COLL_ITEMS;
			coll->cut = 1;
		}
		ae_mcc_workshop_collection_name(index, title, (int)sizeof(title));
		ae_coll_display_name(title, coll->name, AE_COLL_NAME);
		coll->playlist = is_playlist(coll->id);
		for (item = 0; item < count; item++)
		{
			struct ae_coll_item *entry = &coll->items[coll->count];
			struct ae_steam_collection_item client;
			int place = catalog_find(catalog_count, ids[item]);

			memset(entry, 0, sizeof(*entry));
			snprintf(entry->id, AE_COLL_ID, "%s", ids[item]);
			asked_ids[coll->count] = strtoull(entry->id, NULL, 10);
			(void)ae_steam_client_collection_item(asked_ids[coll->count], &client);
			if (client.known)
			{
				/* (a size Steam gives is bounded: 64 GiB a map at the most) */
				entry->size = client.size < (64ull << 30) ? client.size : (64ull << 30);
				snprintf(entry->title, sizeof(entry->title), "%s", client.title);
			}
			else
			{
				unknown++;
				entry->failed = client.failed;
			}
			if (place >= 0)
			{
				entry->state = client.outdated ? AE_COLL_ITEM_UPDATE : AE_COLL_ITEM_PRESENT;
				/* (only a package the game's own downloader recorded can be removed here: Steam's folder, or one put in the
				game's folder by hand, stays) */
				entry->steam_owned = client.recorded ? 0 : in_games_folder(catalog[place].path) ? 2 : 1;
				entry->failed = 0;
				snprintf(entry->picture_name, sizeof(entry->picture_name), "%s@ws%s", catalog[place].name, entry->id);
				if (!entry->title[0])
					snprintf(entry->title, sizeof(entry->title), "%s", catalog[place].item_title[0] ? catalog[place].item_title :
						catalog[place].title);
			}
			if (client.getting)
			{
				entry->state = AE_COLL_ITEM_GETTING;
				entry->percent = client.percent;
			}
			else if (client.queued)
				entry->state = AE_COLL_ITEM_WAITING;
			if (!entry->title[0])
				snprintf(entry->title, sizeof(entry->title), "Workshop item %s", entry->id);
			coll->count++;
		}
		if (unknown && model->signed_in && !asked)
		{
			ae_steam_client_collection_ask(asked_ids, coll->count);
			asked = 1;
		}
		model->count++;
	}
	sort_model(model, sort);
}

/* ---------- removing */

/* whether a map of this Workshop item is in use now (a lobby or a game on it): it is not removed under the engine */
static int item_in_use(unsigned long long item)
{
	int in_use = 0;

	(void)ae_map_library_workshop_item_maps(item, &in_use);
	return in_use;
}

int ae_coll_data_remove_start(char (*ids)[AE_COLL_ID], int count, int *in_use)
{
	unsigned long long numbers[AE_COLL_ITEMS];
	int index, kept = 0;

	*in_use = 0;
	for (index = 0; index < count && index < AE_COLL_ITEMS; index++)
	{
		unsigned long long number = strtoull(ids[index], NULL, 10);

		if (item_in_use(number))
			(*in_use)++;
		else
			numbers[kept++] = number;
	}
	return ae_steam_client_collection_remove(numbers, kept);
}

void ae_coll_data_remove_status(int *pending, int *done, int *failed)
{
	ae_steam_client_collection_remove_status(pending, done, failed);
}

/* ---------- the made-up model */

static void stage_item(struct ae_coll *coll, char const *title, unsigned long long megabytes, int state)
{
	struct ae_coll_item *item = &coll->items[coll->count];

	memset(item, 0, sizeof(*item));
	snprintf(item->id, AE_COLL_ID, "%llu", 2899500000ull + (unsigned long long)coll->count * 997ull +
		(unsigned long long)(unsigned char)coll->id[3]);
	snprintf(item->title, sizeof(item->title), "Halo CE - %s - SMP", title);
	item->size = megabytes * 1000000ull;
	item->state = state;
	/* (a map the PC has borrows a stock map's picture) */
	if (state == AE_COLL_ITEM_PRESENT || state == AE_COLL_ITEM_UPDATE)
	{
		static char const *const stock[] = { "bloodgulch", "dangercanyon", "carousel", "chillout", "hangemhigh", "icefields",
			"infinity", "prisoner", "wizard", "longest" };

		snprintf(item->picture_name, sizeof(item->picture_name), "%s", stock[coll->count % 10]);
	}
	coll->count++;
}

static void stage_social(struct ae_coll *coll, int scene)
{
	static char const *const names[19] = { "Blood Covenant", "Dredwerkz", "Hektik", "Ascetic", "Blamite Mines", "Co2",
		"Cold Shoulder", "Delta Ruined", "Fissure Fall", "Fractured Peak", "Frigid Keep", "Gothra", "Grid Iron",
		"Intensity", "Nitra", "Ruins of Alph", "Sympathy", "The Gutter", "Warren" };
	static unsigned char const megabytes[19] = { 118, 148, 91, 96, 131, 91, 118, 127, 109, 184, 102, 123, 97, 143, 88, 124,
		112, 105, 120 };
	int index, here;

	memset(coll, 0, sizeof(*coll));
	snprintf(coll->id, AE_COLL_ID, "2899609719");
	snprintf(coll->name, AE_COLL_NAME, "Social Map Pack");
	coll->playlist = 1;
	for (index = 0; index < 19; index++)
	{
		int state = AE_COLL_ITEM_ABSENT;

		if (scene == AE_COLL_SCENE_INSTALLED)
			state = index == 3 || index == 8 ? AE_COLL_ITEM_UPDATE : AE_COLL_ITEM_PRESENT;
		else if (scene == AE_COLL_SCENE_GETTING)
			state = index < 7 ? AE_COLL_ITEM_PRESENT : index == 7 ? AE_COLL_ITEM_GETTING : AE_COLL_ITEM_WAITING;
		else if (scene != AE_COLL_SCENE_NONE)
			/* (Blood Covenant, Dredwerkz, Hektik: the first three) */
			state = index < 3 ? AE_COLL_ITEM_PRESENT : AE_COLL_ITEM_ABSENT;
		stage_item(coll, names[index], megabytes[index], state);
		here = coll->count - 1;
		coll->items[here].percent = 42;
		/* (Dredwerkz and Blamite Mines came from Steam's own folder) */
		coll->items[here].steam_owned = (index == 1 || index == 4) &&
			(coll->items[here].state == AE_COLL_ITEM_PRESENT || coll->items[here].state == AE_COLL_ITEM_UPDATE);
	}
}

static void stage_filler(struct ae_coll *coll, char const *id, char const *name, int playlist, int maps, unsigned long long megabytes,
	int present)
{
	int index;

	memset(coll, 0, sizeof(*coll));
	snprintf(coll->id, AE_COLL_ID, "%s", id);
	snprintf(coll->name, AE_COLL_NAME, "%s", name);
	coll->playlist = playlist;
	for (index = 0; index < maps; index++)
	{
		char title[32];

		snprintf(title, sizeof(title), "%s %d", playlist ? "Pro Map" : "Arena Map", index + 1);
		stage_item(coll, title, megabytes / (unsigned long long)maps, index < present ? AE_COLL_ITEM_PRESENT : AE_COLL_ITEM_ABSENT);
	}
}

void ae_coll_data_stage(struct ae_coll_model *model, int scene, int sort)
{
	memset(model, 0, sizeof(*model));
	model->signed_in = scene != AE_COLL_SCENE_NONE;
	model->free_known = 1;
	model->free_bytes = scene == AE_COLL_SCENE_NO_ROOM ? 900ull * 1000000ull : 41000ull * 1000000ull;
	stage_social(&model->colls[0], scene);
	/* (the other two hold maps this PC has, so they are listed; signed out, none is here) */
	stage_filler(&model->colls[1], "3001370390", "Halo 1 Pro Edition", 1, 10, 400, 0);
	stage_filler(&model->colls[2], "3001717490", "2v2 Arena Maps", 0, 6, 700, scene == AE_COLL_SCENE_NONE ? 0 : 2);
	stage_filler(&model->colls[3], "3817409420", "Classic Remakes", 0, 12, 1400, scene == AE_COLL_SCENE_NONE ? 0 : 5);
	model->count = 4;
	sort_model(model, sort);
}
