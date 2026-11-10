/*
AE_MCC_PLATFORM.C

The game's use of MCC's Custom Edition resource maps (ae_mcc.h): the
detection run once a process, the first time a resource map is looked for or
Settings shows MAP FILES (game.mcc_path, HALO_MCC_PATH, then Steam's roots:
Linux's folders, Windows' registry: win32_ae_mcc.c), its line in debug.txt,
the "mcc:\<name>.map" paths map_family_resource hands the engine (and
platform_translate_path turns into the file in MCC's folder), the answers to
the menus' question, and Settings' BROWSE.

Nothing here writes in the MCC folder: it is only read, by the engine's own
read-only opens. Its path stays here and in config.toml (game.mcc_path): the
engine, the netcode, the saves and the map cache see only "mcc:\<name>.map".
*/

#include "platform.h"
#include "port_config.h"
#include "ae_mcc.h"
#include "ae_mcc_workshop.h"
#include "ae_private_files.h"
#include "ae_workshop_collections.h"
#include "update.h"
#include "ae_platform.h"
#include "halo_product.h"
#include "build_identity.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID) && !defined(__EMSCRIPTEN__)
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_misc.h>
#define AE_MCC_FOLDER_DIALOG 1
#endif

#ifndef HALO_SERVER
#include <SDL3/SDL_timer.h>
#endif

#ifdef _WIN32
/* (port/windows/src/win32_ae_mcc.c: the registry's SteamPath, the
InstallPath fallback, the default folder) */
int ae_mcc_windows_steam_roots(char (*roots)[AE_MCC_PATH_SIZE], int maximum);
#endif

/* the prefix of the paths the engine opens MCC's files by */
#define MCC_PREFIX "mcc:\\" /* (ae_mcc.c's ae_mcc_virtual_path_type reads it back) */

static pthread_mutex_t mcc_lock = PTHREAD_MUTEX_INITIALIZER;

static struct
{
	int detected;
	struct ae_mcc_result result;
	/* map_family_resource asked for the player's own folders only */
	int probing;
	/* the log's lines said once */
	int ask_logged;
	int read_logged[3];
	/* BROWSE: a folder the dialog gave, waiting for the menus' poll; the
	last result's words (0: none yet) */
	int dialog_open;
	int chosen;
	int dialog_failed;
	char chosen_path[AE_MCC_PATH_SIZE];
	char browse_message[512];
	/* the status line, UTF-8 and wide */
	char status[600];
	unsigned short status_wide[600];
} mcc;

/* ---------- the detection */

static int folder_exists(const char *path)
{
	return platform_folder_exists(path);
}

static const char *source_name(int source)
{
	switch (source)
	{
	case AE_MCC_SOURCE_CONFIG: return "game.mcc_path";
	case AE_MCC_SOURCE_ENVIRONMENT: return "HALO_MCC_PATH";
	case AE_MCC_SOURCE_STEAM: return "Steam";
	case AE_MCC_SOURCE_BROWSE: return "BROWSE";
	default: return "nowhere";
	}
}

static void log_result(const char *when)
{
	switch (mcc.result.state)
	{
	case AE_MCC_FOUND:
		platform_log("mcc: %s: Custom Edition files found in %s (from %s: %s); game.mcc_use %s", when,
			mcc.result.root, source_name(mcc.result.source), mcc.result.reason, config_string("game.mcc_use"));
		break;
	case AE_MCC_INCOMPLETE:
		platform_log("mcc: %s: MCC found, but not its Custom Edition files: %s", when, mcc.result.reason);
		break;
	default:
		platform_log("mcc: %s: not found: %s", when, mcc.result.reason);
		break;
	}
}

/* the Steam roots to look in (none on Android) */
static int steam_roots(char (*roots)[AE_MCC_PATH_SIZE])
{
	int count = 0;

#if defined(_WIN32)
	count = ae_mcc_windows_steam_roots(roots, AE_MCC_MAXIMUM_ROOTS);
#elif !defined(HALO_ANDROID)
	count = ae_mcc_linux_steam_roots(getenv("XDG_DATA_HOME"), getenv("HOME"), roots, AE_MCC_MAXIMUM_ROOTS);
#else
	(void)roots;
#endif
	return count;
}

/* (with the lock held) */
static void detect_locked(void)
{
	char roots[AE_MCC_MAXIMUM_ROOTS][AE_MCC_PATH_SIZE];
	const char *root_pointers[AE_MCC_MAXIMUM_ROOTS];
	struct ae_mcc_inputs inputs;
	int count, index;

	if (mcc.detected)
		return;
	mcc.detected = 1;
	count = steam_roots(roots);
	for (index = 0; index < count; index++)
		root_pointers[index] = roots[index];
	memset(&inputs, 0, sizeof(inputs));
	inputs.config_path = config_string("game.mcc_path");
	inputs.environment_path = getenv("HALO_MCC_PATH");
	inputs.steam_roots = root_pointers;
	inputs.steam_root_count = count;
	inputs.directory_exists = folder_exists;
	ae_mcc_detect(&inputs, &mcc.result);
	log_result("detected");
	/* (a library moved, or MCC installed again elsewhere: the folder in use kept as it is now) */
	if (mcc.result.state == AE_MCC_FOUND && mcc.result.source == AE_MCC_SOURCE_STEAM &&
		ae_mcc_use_from_text(config_string("game.mcc_use")) == AE_MCC_USE_YES &&
		strcmp(config_string("game.mcc_path"), mcc.result.root) && ae_mcc_path_storable(mcc.result.root))
	{
		if (config_write("game.mcc_path", mcc.result.root))
			platform_log("mcc: game.mcc_path is now %s", mcc.result.root);
	}
	/* (0.7.9) an earlier version's "ask" with MCC found: its files used now, as a new install's are (HALO_MCC_USE's run
	keeps its own word) */
	if (!getenv("HALO_MCC_USE") && ae_mcc_use_migration(mcc.result.state, ae_mcc_use_from_text(config_string("game.mcc_use")),
		config_boolean("game.mcc_asked_again")) == AE_MCC_MIGRATE_TURN_ON)
	{
		config_write("game.mcc_use", "yes");
		if (mcc.result.source != AE_MCC_SOURCE_ENVIRONMENT && ae_mcc_path_storable(mcc.result.root) &&
			strcmp(config_string("game.mcc_path"), mcc.result.root))
		{
			config_write("game.mcc_path", mcc.result.root);
		}
		platform_log("mcc: found in %s and game.mcc_use was \"ask\": turned on (\"yes\")", mcc.result.root);
	}
}

static void detect(void)
{
	pthread_mutex_lock(&mcc_lock);
	detect_locked();
	pthread_mutex_unlock(&mcc_lock);
}

const char *ae_mcc_root(void)
{
	detect();
	return mcc.result.state == AE_MCC_FOUND ? mcc.result.root : NULL;
}

int ae_mcc_copy_root(char *root, size_t size)
{
	int found;

	pthread_mutex_lock(&mcc_lock);
	detect_locked();
	found = mcc.result.state == AE_MCC_FOUND;
	snprintf(root, size, "%s", found ? mcc.result.root : "");
	pthread_mutex_unlock(&mcc_lock);
	return found;
}

void ae_mcc_probe_own_files(int on)
{
	mcc.probing = on;
}

int ae_mcc_state(void)
{
	detect();
	return mcc.result.state;
}

int ae_mcc_use(void)
{
	return ae_mcc_use_from_text(config_string("game.mcc_use"));
}

/* ---------- the Workshop catalog (ae_mcc_workshop.h), logged once at start */

#define WORKSHOP_MAXIMUM_MAPS 1024

/* the catalog the start's scan found, kept for the maps' reads (ae_mcc_workshop_map_path; under mcc_lock) */
static struct ae_mcc_workshop_map *workshop_catalog;
static int workshop_catalog_count;

/* (0.7.9) a space and an underscore are the same: a Workshop map file named with spaces ("frigid_keep dbug flycam mcc
pm.map") can be written in a server rotation, whose entries end at a space, as frigid_keep_dbug_flycam_mcc_pm@ws<item> */
static int workshop_same_name(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
	{
		int x = *a >= 'A' && *a <= 'Z' ? *a - 'A' + 'a' : *a == ' ' ? '_' : *a;
		int y = *b >= 'A' && *b <= 'Z' ? *b - 'A' + 'a' : *b == ' ' ? '_' : *b;

		if (x != y)
			return 0;
	}
	return !*a && !*b;
}

int ae_mcc_workshop_map_path(const char *item_id, const char *name, char *path, size_t size)
{
	int index, found = 0;

	if (!item_id || !name || !path || !size)
		return 0;
	pthread_mutex_lock(&mcc_lock);
	for (index = 0; index < workshop_catalog_count && !found; index++)
	{
		const struct ae_mcc_workshop_map *map = &workshop_catalog[index];

		if (map->status == AE_MCC_WORKSHOP_PLAYABLE && !strcmp(map->item_id, item_id) && workshop_same_name(map->name, name))
			found = snprintf(path, size, "%s", map->path) < (int)size;
	}
	pthread_mutex_unlock(&mcc_lock);
	return found;
}

int ae_mcc_workshop_map_pictures(const char *item_id, const char *name, char *large, char *thumbnail, size_t size)
{
	int index, found = 0;

	if (!item_id || !name || !large || !thumbnail || !size)
		return 0;
	large[0] = thumbnail[0] = 0;
	pthread_mutex_lock(&mcc_lock);
	for (index = 0; index < workshop_catalog_count && !found; index++)
	{
		const struct ae_mcc_workshop_map *map = &workshop_catalog[index];

		if (map->status == AE_MCC_WORKSHOP_PLAYABLE && !strcmp(map->item_id, item_id) && workshop_same_name(map->name, name))
		{
			found = 1;
			if (snprintf(large, size, "%s", map->large) >= (int)size)
				large[0] = 0;
			if (snprintf(thumbnail, size, "%s", map->thumbnail) >= (int)size)
				thumbnail[0] = 0;
		}
	}
	pthread_mutex_unlock(&mcc_lock);
	return found;
}

/* milliseconds, for the scan's timing in the log (the server has no SDL: 0) */
static unsigned long long workshop_ticks(void)
{
#ifndef HALO_SERVER
	return (unsigned long long)SDL_GetTicks();
#else
	return 0;
#endif
}

/* the libraries and item folders the last full scan saw (the main thread's: a rescan compares the folders) */
static char (*workshop_libraries)[AE_MCC_PATH_SIZE];
static int workshop_library_count;
static char (*workshop_ids)[24];
static int workshop_id_count;
#define WORKSHOP_MAXIMUM_ITEMS 4096

/* a whole scan of the libraries: the catalog replaced (under mcc_lock), the item folders kept; how many maps */
static int workshop_full_scan(int *items, int *hidden)
{
	const char *library_pointers[AE_MCC_MAXIMUM_LIBRARIES];
	struct ae_mcc_workshop_map *maps, *old;
	int count, index, other;

	maps = malloc(sizeof(*maps) * WORKSHOP_MAXIMUM_MAPS);
	if (!maps)
		return -1;
	for (index = 0; index < workshop_library_count; index++)
		library_pointers[index] = workshop_libraries[index];
	*hidden = 0;
	count = ae_mcc_workshop_scan(library_pointers, workshop_library_count, maps, WORKSHOP_MAXIMUM_MAPS, hidden);
	workshop_id_count = ae_mcc_workshop_item_ids(library_pointers, workshop_library_count, workshop_ids,
		WORKSHOP_MAXIMUM_ITEMS);
	for (index = 0, *items = 0; index < count; index++)
	{
		for (other = 0; other < index && strcmp(maps[other].item_id, maps[index].item_id); other++)
			;
		if (other == index)
			(*items)++;
	}
	/* (kept for the maps' reads: <name>@ws<item id>, ae_mcc_boot.c; and the menus' list) */
	pthread_mutex_lock(&mcc_lock);
	old = workshop_catalog;
	workshop_catalog = maps;
	workshop_catalog_count = count;
	pthread_mutex_unlock(&mcc_lock);
	free(old);
	return count;
}

static void collections_start(void);

static int preview_items, preview_maps, preview_hidden;

void ae_mcc_workshop_counts(int *items, int *maps, int *hidden)
{
	*items = preview_items;
	*maps = preview_maps;
	*hidden = preview_hidden;
}

void ae_mcc_workshop_log(void)
{
	static int logged;
	char roots[AE_MCC_MAXIMUM_ROOTS][AE_MCC_PATH_SIZE];
	const char *root_pointers[AE_MCC_MAXIMUM_ROOTS];
	char root[AE_MCC_PATH_SIZE];
	int root_count, count, hidden = 0, items = 0, index;
	unsigned long long started;
	struct ae_mcc_workshop_map *maps;

	if (logged)
		return;
	logged = 1;
	/* (nothing for NOT_FOUND or INCOMPLETE, nor with game.mcc_use "no": nothing of Steam's is read then) */
	if (ae_mcc_use() == AE_MCC_USE_NO || !ae_mcc_copy_root(root, sizeof(root)))
		return;
	root_count = steam_roots(roots);
	for (index = 0; index < root_count; index++)
		root_pointers[index] = roots[index];
	workshop_libraries = malloc(sizeof(*workshop_libraries) * AE_MCC_MAXIMUM_LIBRARIES);
	workshop_ids = malloc(sizeof(*workshop_ids) * WORKSHOP_MAXIMUM_ITEMS);
	if (!workshop_libraries || !workshop_ids)
	{
		free(workshop_libraries);
		free(workshop_ids);
		workshop_libraries = NULL;
		workshop_ids = NULL;
		return;
	}
	workshop_library_count = ae_mcc_libraries(root_pointers, root_count, root, workshop_libraries,
		AE_MCC_MAXIMUM_LIBRARIES);
	started = workshop_ticks();
	count = workshop_full_scan(&items, &hidden);
	if (count < 0)
		return;
	platform_log("mcc workshop: %d items, %d maps, %d hidden, %d ms", items, count, hidden,
		(int)(workshop_ticks() - started));
	preview_items = items;
	preview_maps = count;
	preview_hidden = hidden;
	pthread_mutex_lock(&mcc_lock);
	maps = workshop_catalog;
	for (index = 0; index < count; index++)
	{
		if (maps[index].status == AE_MCC_WORKSHOP_HIDDEN)
			platform_log("mcc workshop hidden: %s %s: %s", maps[index].item_id, maps[index].name, maps[index].reason);
	}
	pthread_mutex_unlock(&mcc_lock);
	if (count >= WORKSHOP_MAXIMUM_MAPS)
		platform_log("mcc workshop: the list stops at %d maps", WORKSHOP_MAXIMUM_MAPS);
	/* (only a player with Workshop maps: Steam is asked which collections they are in) */
	if (count > 0)
		collections_start();
}

void ae_mcc_workshop_rescan(void)
{
	const char *library_pointers[AE_MCC_MAXIMUM_LIBRARIES];
	char (*ids)[24];
	int index, id_count, ready = 0, items, hidden, count;
	unsigned long long started = workshop_ticks();

	/* (none before the start's scan, or with nothing of Steam's to read) */
	if (!workshop_libraries || !workshop_ids)
		return;
	ids = malloc(sizeof(*ids) * WORKSHOP_MAXIMUM_ITEMS);
	if (!ids)
		return;
	for (index = 0; index < workshop_library_count; index++)
		library_pointers[index] = workshop_libraries[index];
	id_count = ae_mcc_workshop_item_ids(library_pointers, workshop_library_count, ids, WORKSHOP_MAXIMUM_ITEMS);
	if (!ae_mcc_workshop_same_items(ids, id_count, workshop_ids, workshop_id_count))
	{
		/* (an item added or removed: everything again) */
		count = workshop_full_scan(&items, &hidden);
		platform_log("mcc workshop: rescan, items changed: %d items, %d maps, %d hidden, %d ms", items, count, hidden,
			(int)(workshop_ticks() - started));
	}
	else
	{
		/* (the same items: only the maps Steam was writing are looked at again) */
		pthread_mutex_lock(&mcc_lock);
		ready = ae_mcc_workshop_recheck(workshop_catalog, workshop_catalog_count);
		pthread_mutex_unlock(&mcc_lock);
		platform_log("mcc workshop: rescan, %d maps updated by Steam now ready, %d ms", ready,
			(int)(workshop_ticks() - started));
	}
	free(ids);
}

int ae_mcc_workshop_catalog_copy(struct ae_mcc_workshop_map *maps, int maximum)
{
	int count;

	if (!maps || maximum <= 0)
		return 0;
	pthread_mutex_lock(&mcc_lock);
	count = workshop_catalog_count < maximum ? workshop_catalog_count : maximum;
	if (count > 0)
		memcpy(maps, workshop_catalog, sizeof(*maps) * (size_t)count);
	pthread_mutex_unlock(&mcc_lock);
	return count;
}

/* ---------- the Workshop collections (ae_workshop_collections.h): Steam's answer, kept a day beside the saves */

/* Steam is asked on Linux, Windows and Mac only (the browser build has no threads; Android has no Steam and its
guest links no update_https_request) */
#if !defined(__EMSCRIPTEN__) && !defined(HALO_ANDROID)
#define AE_WSC_ASKS_STEAM 1
#else
#define AE_WSC_ASKS_STEAM 0
#endif

#define COLLECTIONS_CACHE "workshop-collections.cache"
#define COLLECTIONS_API "https://api.steampowered.com"
/* (a test's own server: HALO_AE_WORKSHOP_API=http://127.0.0.1:<port>, update_https_request's plain http to this machine) */
#define COLLECTIONS_API_ENVIRONMENT "HALO_AE_WORKSHOP_API"
#define COLLECTIONS_ANSWER_MAXIMUM (1024ul * 1024ul)

/* the table the menus read (under mcc_lock): the cache's, then what Steam answered */
static struct ae_wsc_table *collections_table;
static int collections_started;

static void collections_cache_load(struct ae_wsc_table *table)
{
	long length;
	char *text = malloc(AE_WSC_CACHE_TEXT);

	ae_wsc_table_init(table);
	if (!text)
		return;
	length = ae_private_file_read(COLLECTIONS_CACHE, text, AE_WSC_CACHE_TEXT);
	if (length > 0)
		ae_wsc_cache_read(table, text);
	free(text);
}

static void collections_cache_save(struct ae_wsc_table const *table)
{
	char *text = malloc(AE_WSC_CACHE_TEXT);
	long length;

	if (!text)
		return;
	length = ae_wsc_cache_write(table, text, AE_WSC_CACHE_TEXT);
	if (length < 0 || !ae_private_file_write(COLLECTIONS_CACHE, text, length))
		platform_log("mcc workshop: collections cache not written");
	free(text);
}

#if AE_WSC_ASKS_STEAM
/* one POST to the web API: its answer (malloc'd by the request; freed by the caller), NULL for none */
static int collections_post(const char *method, const char *form, struct update_response *response)
{
	char url[256], error[256];
	const char *base = getenv(COLLECTIONS_API_ENVIRONMENT);

	snprintf(url, sizeof(url), "%s/ISteamRemoteStorage/%s/v1/", base && base[0] ? base : COLLECTIONS_API, method);
	if (!update_https_request("POST", url, "Content-Type: application/x-www-form-urlencoded\r\n", form,
		(unsigned long)strlen(form), COLLECTIONS_ANSWER_MAXIMUM, response, error, sizeof(error)))
	{
		platform_log("mcc workshop: collections not asked: %s", error);
		return 0;
	}
	if (response->status != 200 || !response->body)
	{
		platform_log("mcc workshop: collections: Steam answered %d", response->status);
		free(response->body);
		response->body = NULL;
		return 0;
	}
	return 1;
}

/* the worker: both questions, then the table swapped in and the cache written. Nothing shown, nothing blocked: a failure
is one line in the log and the cache (or none) stays what the menus read */
static void *collections_worker(void *argument)
{
	struct ae_wsc_table *fresh = argument;
	char form[AE_WSC_BODY];
	struct update_response response;
	int read;

	ae_wsc_table_init(fresh);
	if (!ae_wsc_children_body(form, sizeof(form)) || !collections_post("GetCollectionDetails", form, &response))
		goto done;
	read = ae_wsc_read_children(fresh, response.body, (int)response.body_size);
	free(response.body);
	if (!read || !ae_wsc_titles_body(form, sizeof(form)) || !collections_post("GetPublishedFileDetails", form, &response))
		goto done;
	ae_wsc_read_titles(fresh, response.body, (int)response.body_size);
	free(response.body);
	/* (an answer with no collection the game asks about is not kept over a cache that had some) */
	if (ae_wsc_prune(fresh) <= 0)
	{
		platform_log("mcc workshop: collections: Steam named none of them");
		goto done;
	}
	fresh->fetched = (long long)time(NULL);
	pthread_mutex_lock(&mcc_lock);
	memcpy(collections_table, fresh, sizeof(*fresh));
	pthread_mutex_unlock(&mcc_lock);
	collections_cache_save(fresh);
	platform_log("mcc workshop: collections from Steam: %d of the %d asked", fresh->count, ae_workshop_collections_id_count());
done:
	free(fresh);
	return NULL;
}
#endif

/* the start's (ae_mcc_workshop_log, with Workshop maps found): the cache read, and a day old or none, Steam asked on a
thread of its own */
static void collections_start(void)
{
	struct ae_wsc_table *fresh;
	int stale;
#if AE_WSC_ASKS_STEAM
	pthread_t thread;
	pthread_attr_t attributes;
#endif

	if (collections_started)
		return;
	collections_started = 1;
	collections_table = malloc(sizeof(*collections_table));
	if (!collections_table)
		return;
	collections_cache_load(collections_table);
	stale = ae_wsc_stale(collections_table, (long long)time(NULL));
	platform_log("mcc workshop: collections: %d in the cache%s", collections_table->count, stale ? ", asking Steam" : "");
#if AE_WSC_ASKS_STEAM
	if (!stale || !(fresh = malloc(sizeof(*fresh))))
		return;
	pthread_attr_init(&attributes);
	pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&thread, &attributes, collections_worker, fresh))
		free(fresh);
	pthread_attr_destroy(&attributes);
#else
	(void)fresh;
#endif
}

int ae_mcc_workshop_collection_count(void)
{
	int count;

	pthread_mutex_lock(&mcc_lock);
	count = collections_table ? collections_table->count : 0;
	pthread_mutex_unlock(&mcc_lock);
	return count;
}

int ae_mcc_workshop_collection_name(int index, char *name, int size)
{
	int found = 0;

	if (size > 0)
		name[0] = 0;
	pthread_mutex_lock(&mcc_lock);
	if (collections_table && size > 0 && index >= 0 && index < collections_table->count)
		found = snprintf(name, (size_t)size, "%s", collections_table->collections[index].name) < size;
	pthread_mutex_unlock(&mcc_lock);
	return found;
}

int ae_mcc_workshop_collections_of(const char *item_id, int *indices, int maximum)
{
	int count = 0;

	pthread_mutex_lock(&mcc_lock);
	if (collections_table && item_id && indices)
		count = ae_wsc_of(collections_table, item_id, indices, maximum);
	pthread_mutex_unlock(&mcc_lock);
	return count;
}

/* ---------- the engine's reads */

int ae_mcc_resource(const char *name, char *path, long size)
{
	int type = ae_mcc_resource_type(name);

	/* (with "no" nothing of Steam's is read for a map: no detection; only Settings' MAP FILES looks) */
	if (type < 0 || size <= 0 || mcc.probing || ae_mcc_use() == AE_MCC_USE_NO || ae_mcc_state() != AE_MCC_FOUND)
		return 0;
	switch (ae_mcc_use())
	{
	case AE_MCC_USE_ASK:
		/* (outside the menus' question: a test host, a joined game; ask counts as no) */
		pthread_mutex_lock(&mcc_lock);
		if (!mcc.ask_logged)
		{
			mcc.ask_logged = 1;
			platform_log("mcc: Custom Edition's %s.map is missing; MCC's in %s is not read: game.mcc_use is "
				"\"ask\" (the menus ask when a map is picked; \"yes\" reads them)", name, mcc.result.root);
		}
		pthread_mutex_unlock(&mcc_lock);
		return 0;
	default:
		break;
	}
	snprintf(path, (size_t)size, MCC_PREFIX "%s.map", ae_mcc_resource_name(type));
	pthread_mutex_lock(&mcc_lock);
	if (!mcc.read_logged[type])
	{
		mcc.read_logged[type] = 1;
		platform_log("mcc: %s.map read from MCC's folder (%s)", ae_mcc_resource_name(type), mcc.result.root);
	}
	pthread_mutex_unlock(&mcc_lock);
	return 1;
}

/* an MCC map's shared resource files in the MCC install found (mcc_resources.c reads them in place, nothing
copied); none when MCC isn't found or game.mcc_use is "no" */
int ae_mcc_install_resource_candidates(int type, char (*paths)[AE_MCC_PATH_SIZE], int maximum)
{
	char root[AE_MCC_PATH_SIZE];

	if (ae_mcc_use() == AE_MCC_USE_NO || !ae_mcc_copy_root(root, sizeof(root)))
		return 0;
	return ae_mcc_resource_candidate_paths(root, type, paths, maximum);
}

/* (0.7.9) a Workshop map's own shared resource files: a mod package keeps them in its item's halo1/maps (and
halo1/maps/custom_edition), MCC's layout, beside its maps/ folder ("45@ws3477809506": item 3477809506's maps/45.map). None
for a map that is not a Workshop one, or not in the catalog */
int ae_mcc_workshop_resource_candidates(const char *level, int type, char (*paths)[AE_MCC_PATH_SIZE], int maximum)
{
	char name[AE_MCC_PATH_SIZE], item[32], path[AE_MCC_PATH_SIZE];
	const char *at = level ? strstr(level, "@ws") : NULL;
	size_t length;
	int cut;

	if (!at || !at[3] || strlen(at + 3) >= sizeof(item) || (size_t)(at - level) >= sizeof(name))
		return 0;
	length = (size_t)(at - level);
	memcpy(name, level, length);
	name[length] = 0;
	snprintf(item, sizeof(item), "%s", at + 3);
	if (!ae_mcc_workshop_map_path(item, name, path, sizeof(path)))
		return 0;
	/* (the item's folder: the map's folder's parent) */
	for (cut = 0; cut < 2; cut++)
	{
		char *separator = strrchr(path, '/'), *back = strrchr(path, '\\');

		if (back > separator)
			separator = back;
		if (!separator)
			return 0;
		*separator = 0;
	}
	return ae_mcc_workshop_resource_paths(path, type, paths, maximum);
}

/* a host file opened as it is named (the game's own fopen takes its paths for the Xbox's and finds them under the
data root): an MCC install's resource file, read in place; NULL with errno for any path that is not absolute */
FILE *ae_mcc_install_open(const char *path)
{
	int absolute = path && (path[0] == '/' || path[0] == '\\' ||
		(((path[0] | 0x20) >= 'a' && (path[0] | 0x20) <= 'z') && path[1] == ':' && (path[2] == '\\' || path[2] == '/')));

	if (!absolute)
	{
		errno = ENOENT;
		return NULL;
	}
	return fopen(path, "rb");
}

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID) && !defined(__EMSCRIPTEN__)
/* the maps folder the engine asks for even with MCC's maps alone (see ae_mcc_boot_without_xbox): 1 made */
static int first_start_make_maps(const char *destination)
{
	char maps[AE_MCC_PATH_SIZE];

	snprintf(maps, sizeof(maps), "%s/maps", destination);
	if (!SDL_CreateDirectory(maps))
	{
		platform_log("first start: MCC found in %s, but %s could not be made: %s", ae_mcc_root(), maps, SDL_GetError());
		return 0;
	}
	platform_log("first start: MCC found in %s: no disc question, made %s (MCC's maps play)", ae_mcc_root(), maps);
	return 1;
}
#endif

int ae_mcc_first_start_maps(const char *destination)
{
#if !defined(HALO_SERVER) && !defined(HALO_ANDROID) && !defined(__EMSCRIPTEN__)
	if (!destination || !destination[0] || ae_mcc_use() == AE_MCC_USE_NO)
		return 0;
	if (!ae_mcc_root())
	{
		platform_log("first start: no MCC found (%s): the disc question is asked, with a button for MCC's folder",
			mcc.result.reason);
		return 0;
	}
	/* (one piece of its Halo CE content missing: the question that names it, ae_mcc_first_start_incomplete) */
	if (mcc.result.missing)
	{
		platform_log("first start: MCC found in %s, but its Halo CE content is not whole (%s): the question that names it follows", ae_mcc_root(),
			mcc.result.missing_detail);
		return 0;
	}
	return first_start_make_maps(destination);
#else
	(void)destination;
	return 0;
#endif
}

int ae_mcc_boot_without_xbox(void)
{
#if !defined(HALO_SERVER) && !defined(HALO_ANDROID) && !defined(__EMSCRIPTEN__)
	char maps[AE_MCC_PATH_SIZE];

	/* (with "no" nothing of Steam's is read: no detection) */
	if (ae_mcc_use() == AE_MCC_USE_NO || ae_mcc_boot_source(0, ae_mcc_root() != NULL, ae_mcc_use()) != AE_MCC_BOOT_MCC)
		return 0;
	/* (without the Anniversary piece there is no ui.map to boot from; without the other, no stock maps: asked first) */
	if (mcc.result.missing)
		return 0;
	/* (the engine asks for its maps folder even then: cache_files_map_directory asserts with none, and the maps
	of mcc_maps, map packs and Halo PC go beside it) */
	snprintf(maps, sizeof(maps), "%s/maps", platform_data_root());
	SDL_CreateDirectory(maps);
	platform_log("maps folder: no Xbox ui.map; the main menu is read from MCC's halo1/maps/ui.map in %s", ae_mcc_root());
	return 1;
#else
	/* (the desktop's maps folder check alone asks: the server, Android and the web build have no MCC boot) */
	return 0;
#endif
}

int ae_mcc_translate_path(const char *xbox_path, char *host_path, unsigned long size)
{
	char root[AE_MCC_PATH_SIZE];
	int type = ae_mcc_virtual_path_type(xbox_path);

	if (type == AE_MCC_VIRTUAL_NOT)
		return 0;
	if (size)
		host_path[0] = 0;
	/* (any thread opens files: the folder copied under the lock, which BROWSE writes it under) */
	/* (an MCC Workshop item's map: mcc:\ws\<item id>\<name>.map, where the catalog found it) */
	if (type == AE_MCC_VIRTUAL_WORKSHOP)
	{
		char id[24], name[64];

		if (ae_mcc_workshop_virtual_parts(xbox_path, id, sizeof(id), name, sizeof(name)))
			ae_mcc_workshop_map_path(id, name, host_path, size);
		return 1;
	}
	/* (MCC's own maps: mcc:\halo1\<name>.map, the name checked) */
	if (type == AE_MCC_VIRTUAL_HALO1 && ae_mcc_copy_root(root, sizeof(root)))
		snprintf(host_path, size, "%s/halo1/maps/%s", root, xbox_path + 11);
	else if (type >= 0 && type != AE_MCC_VIRTUAL_HALO1 && ae_mcc_copy_root(root, sizeof(root)))
		ae_mcc_resource_path(root, ae_mcc_resource_name(type), host_path, size);
	return 1;
}

const char *ae_mcc_refusal_note(void)
{
	if (ae_mcc_use() == AE_MCC_USE_NO)
		return "";
	switch (ae_mcc_state())
	{
	case AE_MCC_INCOMPLETE:
		return ". MCC found, but its Custom Edition files are missing. Check the install in Steam.";
	case AE_MCC_FOUND:
		/* (a load outside the menus' question: a game joined, the Xbox menus, a test host) */
		return ae_mcc_use() == AE_MCC_USE_ASK ? ". MCC found: turn on SETTINGS > SYSTEM > FILES > MCC FILES to use its files." : "";
	default:
		return "";
	}
}

/* ---------- the menus */

void ae_mcc_answer_use(void)
{
	const char *root = ae_mcc_root();

	if (!root)
		return;
	config_write("game.mcc_use", "yes");
	/* (HALO_MCC_PATH's folder is one run's: not kept) */
	if (mcc.result.source != AE_MCC_SOURCE_ENVIRONMENT && ae_mcc_path_storable(root) &&
		strcmp(config_string("game.mcc_path"), root))
	{
		config_write("game.mcc_path", root);
	}
	platform_log("mcc: USE MCC FILES: game.mcc_use = \"yes\", reading from %s", root);
}

int ae_mcc_offer_pending(char *root, size_t size)
{
	detect();
	if (getenv("HALO_MCC_USE") || ae_mcc_use_migration(mcc.result.state, ae_mcc_use_from_text(config_string("game.mcc_use")),
		config_boolean("game.mcc_asked_again")) != AE_MCC_MIGRATE_ASK)
	{
		return 0;
	}
	if (root && size)
		snprintf(root, size, "%s", mcc.result.root);
	return 1;
}

void ae_mcc_offer_answered(int use)
{
	config_write("game.mcc_asked_again", "true");
	if (use)
		ae_mcc_answer_use();
	else
		platform_log("mcc: asked once more: NOT NOW, game.mcc_use stays \"%s\"", config_string("game.mcc_use"));
}

int ae_mcc_missing_pending(void)
{
#if defined(__APPLE__) || defined(HALO_ANDROID)
	/* (Steam's MCC runs on Windows, and on Linux through Steam: nothing to look for here) */
	return 0;
#endif
	detect();
	return mcc.result.state == AE_MCC_NOT_FOUND && !getenv("HALO_MCC_USE") &&
		ae_mcc_use_from_text(config_string("game.mcc_use")) != AE_MCC_USE_NO &&
		strcmp(config_string("game.mcc_missing_told"), build_identity_version());
}

void ae_mcc_missing_told(void)
{
	config_write("game.mcc_missing_told", build_identity_version());
}

int ae_mcc_redetect(void)
{
	int state;

	pthread_mutex_lock(&mcc_lock);
	mcc.detected = 0;
	detect_locked();
	state = mcc.result.state;
	pthread_mutex_unlock(&mcc_lock);
	return state;
}

void ae_mcc_answer_never(void)
{
	config_write("game.mcc_use", "no");
	platform_log("mcc: NEVER ASK AGAIN: game.mcc_use = \"no\"");
}

void ae_mcc_answer_not_now(void)
{
	platform_log("mcc: NOT NOW: game.mcc_use stays \"%s\"", config_string("game.mcc_use"));
}

/* the folder BROWSE chose, or one of its own folders' parents: the MCC folder
(the one holding halo1), its halo1, halo1/maps or the custom_edition folder
itself. 1 with the MCC folder in root */
static int browsed_root(const char *chosen, char *root, size_t size)
{
	char candidate[AE_MCC_PATH_SIZE], why[384], first_why[384] = "";
	int level;

	snprintf(candidate, sizeof(candidate), "%s", chosen);
	for (level = 0; level < 4; level++)
	{
		size_t length = strlen(candidate);
		char *cut;

		while (length > 1 && (candidate[length - 1] == '/' || candidate[length - 1] == '\\'))
			candidate[--length] = 0;
		if (ae_mcc_root_valid(candidate, why, sizeof(why)))
		{
			snprintf(root, size, "%s", candidate);
			return 1;
		}
		if (!level)
			snprintf(first_why, sizeof(first_why), "%s", why);
		cut = strrchr(candidate, '/');
		if (!cut || (strrchr(candidate, '\\') && strrchr(candidate, '\\') > cut))
			cut = strrchr(candidate, '\\');
		if (!cut || cut == candidate)
			break;
		*cut = 0;
	}
	platform_log("mcc: BROWSE: %s is not an MCC folder, nor in one (%s)", chosen, first_why);
	return 0;
}

/* a folder chosen, checked and kept (the menus' thread, with the lock held) */
static void browse_take_locked(const char *chosen)
{
	char root[AE_MCC_PATH_SIZE];

	if (!browsed_root(chosen, root, sizeof(root)))
	{
		snprintf(mcc.browse_message, sizeof(mcc.browse_message),
			"THAT FOLDER HAS NO CUSTOM EDITION FILES. PICK\nTHE MCC FOLDER, THE ONE THAT HOLDS halo1.");
		return;
	}
	if (!ae_mcc_path_storable(root))
	{
		snprintf(mcc.browse_message, sizeof(mcc.browse_message),
			"THAT FOLDER'S PATH IS TOO LONG TO KEEP\n(UNDER 256 CHARACTERS).");
		return;
	}
	config_write("game.mcc_path", root);
	mcc.detected = 1;
	mcc.result.state = AE_MCC_FOUND;
	mcc.result.source = AE_MCC_SOURCE_BROWSE;
	snprintf(mcc.result.root, sizeof(mcc.result.root), "%s", root);
	snprintf(mcc.result.reason, sizeof(mcc.result.reason), "the folder BROWSE chose");
	log_result("BROWSE");
	mcc.browse_message[0] = 0;
}

#ifdef AE_MCC_FOLDER_DIALOG
static void SDLCALL dialog_done(void *userdata, const char *const *files, int filter)
{
	(void)userdata;
	(void)filter;
	pthread_mutex_lock(&mcc_lock);
	mcc.dialog_open = 0;
	if (!files)
	{
		mcc.dialog_failed = 1;
		platform_log("mcc: BROWSE: the folder dialog failed: %s", SDL_GetError());
	}
	else if (files[0])
	{
		snprintf(mcc.chosen_path, sizeof(mcc.chosen_path), "%s", files[0]);
		mcc.chosen = 1;
	}
	else
	{
		platform_log("mcc: BROWSE: cancelled");
	}
	pthread_mutex_unlock(&mcc_lock);
}
#endif

void ae_mcc_browse(void)
{
	const char *test_result = getenv("HALO_MCC_BROWSE_RESULT");

	detect();
	pthread_mutex_lock(&mcc_lock);
	if (mcc.dialog_open)
	{
		pthread_mutex_unlock(&mcc_lock);
		return;
	}
	mcc.browse_message[0] = 0;
	mcc.dialog_failed = 0;
	/* (the tests' folder, chosen as the dialog would) */
	if (test_result && *test_result)
	{
		platform_log("mcc: BROWSE: HALO_MCC_BROWSE_RESULT %s", test_result);
		snprintf(mcc.chosen_path, sizeof(mcc.chosen_path), "%s", test_result);
		mcc.chosen = 1;
		pthread_mutex_unlock(&mcc_lock);
		return;
	}
#ifdef AE_MCC_FOLDER_DIALOG
	mcc.dialog_open = 1;
	pthread_mutex_unlock(&mcc_lock);
	platform_log("mcc: BROWSE: the folder dialog opened");
	SDL_ShowOpenFolderDialog(dialog_done, NULL, SDL_GetKeyboardFocus(),
		mcc.result.state == AE_MCC_FOUND ? mcc.result.root : NULL, false);
#else
	snprintf(mcc.browse_message, sizeof(mcc.browse_message), "THIS BUILD HAS NO FOLDER DIALOG:\nSET game.mcc_path.");
	pthread_mutex_unlock(&mcc_lock);
#endif
}

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID) && !defined(__EMSCRIPTEN__)
/* ---------- the first start's questions (before the game window exists) */

static struct
{
	int done;
	char path[AE_MCC_PATH_SIZE];
} first_pick;

/* a test's answers to the dialogs below: HALO_MCC_FIRST_START_ANSWER, words from help, folder, disc and quit,
separated by commas, one for each time a dialog would be shown (the dialogs can't be clicked headless) */
static const char *first_start_test_answer(void)
{
	static const char *rest = NULL;
	static char word[16];
	const char *list = getenv("HALO_MCC_FIRST_START_ANSWER");
	size_t length;

	if (!rest)
		rest = list;
	if (!rest || !*rest)
		return NULL;
	length = strcspn(rest, ",");
	snprintf(word, sizeof(word), "%.*s", (int)(length < sizeof(word) - 1 ? length : sizeof(word) - 1), rest);
	rest += length + (rest[length] == ',');
	return word;
}

static void SDLCALL first_pick_done(void *userdata, const char *const *files, int filter)
{
	(void)userdata;
	(void)filter;
	pthread_mutex_lock(&mcc_lock);
	first_pick.path[0] = 0;
	if (!files)
		platform_log("first start: the folder dialog failed: %s", SDL_GetError());
	else if (files[0])
		snprintf(first_pick.path, sizeof(first_pick.path), "%s", files[0]);
	first_pick.done = 1;
	pthread_mutex_unlock(&mcc_lock);
}

/* the system's folder dialog, waited for here (its answer comes from its own thread; the events are pumped meanwhile).
1 with the folder in chosen, 0 for none (cancelled, or no dialog could be shown) */
static int first_start_pick(char *chosen, size_t size)
{
	const char *test = getenv("HALO_MCC_BROWSE_RESULT");
	int done = 0;

	if (test && *test)
	{
		platform_log("first start: HALO_MCC_BROWSE_RESULT %s", test);
		snprintf(chosen, size, "%s", test);
		return 1;
	}
	pthread_mutex_lock(&mcc_lock);
	first_pick.done = 0;
	pthread_mutex_unlock(&mcc_lock);
	platform_log("first start: the folder dialog opened");
	SDL_ShowOpenFolderDialog(first_pick_done, NULL, NULL, NULL, false);
	while (!done)
	{
		SDL_PumpEvents();
		SDL_Delay(20);
		pthread_mutex_lock(&mcc_lock);
		done = first_pick.done;
		pthread_mutex_unlock(&mcc_lock);
	}
	if (!first_pick.path[0])
	{
		platform_log("first start: no folder chosen");
		return 0;
	}
	snprintf(chosen, size, "%s", first_pick.path);
	return 1;
}

/* a dialog line in debug.txt on one line */
static void first_start_log_text(const char *what, const char *text)
{
	char line[1600];
	size_t index;

	snprintf(line, sizeof(line), "%s", text);
	for (index = 0; line[index]; index++)
	{
		if (line[index] == '\n')
			line[index] = ' ';
	}
	platform_log("first start: %s: %s", what, line);
}

static void first_start_say(int icon, const char *text)
{
	first_start_log_text("said", text);
	/* (a test's answers: nobody to read it) */
	if (!getenv("HALO_MCC_FIRST_START_ANSWER"))
		SDL_ShowSimpleMessageBox(icon, HALO_PRODUCT_NAME, text, NULL);
}

/* the names of the pieces the bits say ("A", "B", or "A and B") */
static void first_start_piece_names(int pieces, char *names, size_t size)
{
	snprintf(names, size, "%s%s%s", pieces & AE_MCC_PIECE_ANNIVERSARY ? AE_MCC_PIECE_ANNIVERSARY_NAME : "",
		pieces == (AE_MCC_PIECE_ANNIVERSARY | AE_MCC_PIECE_MULTIPLAYER) ? " and " : "",
		pieces & AE_MCC_PIECE_MULTIPLAYER ? AE_MCC_PIECE_MULTIPLAYER_NAME : "");
}

/* what to do about missing pieces, in Steam's words */
static void first_start_fix_text(int pieces, char *text, size_t size)
{
	char names[160];

	first_start_piece_names(pieces, names, sizeof(names));
	snprintf(text, size,
		"In Steam, open your Library, right-click Halo: The Master Chief Collection, choose Properties, then DLC, "
		"and tick %s%s. Let Steam download %s, then choose Installed Files > Verify integrity of game files, and "
		"start " HALO_PRODUCT_SHORT_NAME " again.",
		pieces == (AE_MCC_PIECE_ANNIVERSARY | AE_MCC_PIECE_MULTIPLAYER) ? "both: " : "", names,
		pieces == (AE_MCC_PIECE_ANNIVERSARY | AE_MCC_PIECE_MULTIPLAYER) ? "them" : "it");
}

/* a folder the player chose: the MCC folder in or above it, if it holds both pieces, is kept (game.mcc_path, and for
this run) and the maps folder made: 1. Otherwise the player is told what is wrong: 0 */
static int first_start_take(const char *chosen, const char *destination)
{
	char root[AE_MCC_PATH_SIZE], detail[400], text[1400];
	int missing;

	if (!browsed_root(chosen, root, sizeof(root)))
	{
		first_start_say(SDL_MESSAGEBOX_WARNING,
			"That folder is not Halo: The Master Chief Collection. Choose the folder that holds halo1: in your Steam "
			"library, steamapps" AE_MCC_PATH_SEPARATOR "common" AE_MCC_PATH_SEPARATOR "Halo The Master Chief Collection.");
		return 0;
	}
	missing = ae_mcc_missing_pieces(root, detail, sizeof(detail));
	if (missing)
	{
		char fix[700], names[160];

		platform_log("first start: %s lacks its Halo CE content (%s)", root, detail);
		first_start_piece_names(missing, names, sizeof(names));
		first_start_fix_text(missing, fix, sizeof(fix));
		snprintf(text, sizeof(text), "That copy of Halo: The Master Chief Collection is missing %s.\n\n%s", names, fix);
		first_start_say(SDL_MESSAGEBOX_WARNING, text);
		return 0;
	}
	pthread_mutex_lock(&mcc_lock);
	mcc.detected = 1;
	mcc.result.state = AE_MCC_FOUND;
	mcc.result.source = AE_MCC_SOURCE_BROWSE;
	mcc.result.missing = 0;
	mcc.result.missing_detail[0] = 0;
	snprintf(mcc.result.root, sizeof(mcc.result.root), "%s", root);
	snprintf(mcc.result.reason, sizeof(mcc.result.reason), "the folder chosen at the first start");
	pthread_mutex_unlock(&mcc_lock);
	/* (kept only when the settings can: else it plays this run, and the next finds it again by Steam or asks) */
	if (ae_mcc_path_storable(root) && config_write("game.mcc_path", root))
		platform_log("first start: game.mcc_path is now %s", root);
	log_result("first start");
	return first_start_make_maps(destination);
}

int ae_mcc_first_start_choose(const char *destination)
{
	char chosen[AE_MCC_PATH_SIZE];

	return first_start_pick(chosen, sizeof(chosen)) && first_start_take(chosen, destination);
}

int ae_mcc_first_start_incomplete(const char *destination)
{
	static const SDL_MessageBoxButtonData buttons[] =
	{
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Open the help page" },
		{ 0, 2, "Choose MCC folder" },
		{ 0, 3, "Use a disc image" },
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit" },
	};
	static int chose_disc = 0;
	char message[1800], fix[700], names[160];
	const char *test_answer = getenv("HALO_MCC_FIRST_START_ANSWER") ? "" : NULL;
	int missing;

	/* (found and whole: no question; not found: the disc question's, with its own button) */
	if (chose_disc || ae_mcc_use() == AE_MCC_USE_NO || ae_mcc_state() == AE_MCC_NOT_FOUND ||
		(ae_mcc_state() == AE_MCC_FOUND && !mcc.result.missing))
	{
		return 0;
	}
	/* (not for runs nobody is watching, but a test's answers are: the log shows what a player would see) */
	if (!test_answer && (config_boolean("debug.hidden_window") || config_real("debug.exit_after") > 0.0))
		return 0;
	missing = mcc.result.missing;
	if (!test_answer && !SDL_Init(SDL_INIT_VIDEO))
		return 0;
	if (missing)
	{
		first_start_piece_names(missing, names, sizeof(names));
		first_start_fix_text(missing, fix, sizeof(fix));
		snprintf(message, sizeof(message),
			HALO_PRODUCT_SHORT_NAME " found Halo: The Master Chief Collection, but it is missing %s.\n\n%s\n\n"
			"If you have a second copy of MCC, choose its folder. No MCC? Use a disc image of Halo: Combat Evolved.",
			names, fix);
	}
	else
	{
		/* (all the files are there but Steam says it is still installing or updating them) */
		snprintf(message, sizeof(message),
			"Steam is still installing or updating Halo: The Master Chief Collection. Wait until Steam is done, then "
			"start " HALO_PRODUCT_SHORT_NAME " again.\n\nIf you have a second copy of MCC, choose its folder. No MCC? "
			"Use a disc image of Halo: Combat Evolved.");
	}
	platform_log("first start: MCC in %s is not whole (%s; %s)", mcc.result.root, mcc.result.reason,
		mcc.result.missing_detail[0] ? mcc.result.missing_detail : "no file missing");
	for (;;)
	{
		SDL_MessageBoxData question = { SDL_MESSAGEBOX_INFORMATION, NULL, HALO_PRODUCT_NAME, message, 4, buttons, NULL };
		int answer = 0;

		first_start_log_text("asked", message);
		if (test_answer)
		{
			const char *word = first_start_test_answer();

			answer = !word ? 0 : !strcmp(word, "help") ? 1 : !strcmp(word, "folder") ? 2 : !strcmp(word, "disc") ? 3 : 0;
			platform_log("first start: test answer %s", word ? word : "(none: quit)");
		}
		else if (!SDL_ShowMessageBox(&question, &answer))
		{
			answer = 0;
		}
		if (answer == 1)
		{
			platform_log("first start: opening the help page %s", AE_MCC_HELP_URL);
			if (!test_answer && !SDL_OpenURL(AE_MCC_HELP_URL))
				platform_log("first start: the help page did not open: %s", SDL_GetError());
			continue;
		}
		if (answer == 2)
		{
			if (ae_mcc_first_start_choose(destination))
				return 1;
			continue;
		}
		if (answer == 3)
		{
			platform_log("first start: the player chose a disc image");
			chose_disc = 1;
			return 0;
		}
		platform_log("first start: quitting");
		exit(EXIT_SUCCESS);
	}
}
#endif

const char *ae_mcc_status_text(void)
{
	char path[128];

	detect();
	pthread_mutex_lock(&mcc_lock);
	if (mcc.chosen)
	{
		mcc.chosen = 0;
		browse_take_locked(mcc.chosen_path);
	}
	if (mcc.dialog_failed)
	{
		mcc.dialog_failed = 0;
		snprintf(mcc.browse_message, sizeof(mcc.browse_message),
			"THE FOLDER DIALOG COULD NOT OPEN.\nSET game.mcc_path IN config.toml INSTEAD.");
	}
	if (mcc.dialog_open)
		snprintf(mcc.status, sizeof(mcc.status), "CHOOSING A FOLDER...");
	else if (mcc.browse_message[0])
		snprintf(mcc.status, sizeof(mcc.status), "%s", mcc.browse_message);
	else if (mcc.result.state == AE_MCC_FOUND)
	{
		ae_mcc_short_path(mcc.result.root, path, sizeof(path), 48);
		snprintf(mcc.status, sizeof(mcc.status), "%s\n%s",
			ae_mcc_use() == AE_MCC_USE_YES ? "IN USE:" : "FOUND (SET MCC FILES TO ON TO USE IT):", path);
	}
	else if (mcc.result.state == AE_MCC_INCOMPLETE)
		snprintf(mcc.status, sizeof(mcc.status), "MCC FOUND, BUT ITS CUSTOM EDITION FILES ARE\nMISSING. CHECK THE INSTALL IN STEAM.");
	else
		snprintf(mcc.status, sizeof(mcc.status), "NOT FOUND");
	pthread_mutex_unlock(&mcc_lock);
	return mcc.status;
}

const unsigned short *ae_mcc_status_wide(void)
{
	const unsigned char *text = (const unsigned char *)ae_mcc_status_text();
	size_t length = 0;

	/* (UTF-8 to UTF-16, the basic plane; anything else a ?) */
	while (*text && length + 1 < sizeof(mcc.status_wide) / sizeof(mcc.status_wide[0]))
	{
		unsigned long code = *text++;
		int more = code >= 0xf0 ? 3 : code >= 0xe0 ? 2 : code >= 0xc0 ? 1 : 0;

		if (more)
		{
			code &= 0x3f >> more;
			for (; more && (*text & 0xc0) == 0x80; more--)
				code = code << 6 | (*text++ & 0x3f);
			if (more || code > 0xffff)
				code = '?';
		}
		else if (code >= 0x80)
		{
			code = '?';
		}
		/* (a line break as the menus' text has it: menu_files.c's halo_menus_utf16) */
		else if (code == '\n' && length + 2 < sizeof(mcc.status_wide) / sizeof(mcc.status_wide[0]))
		{
			mcc.status_wide[length++] = '\r';
		}
		mcc.status_wide[length++] = (unsigned short)code;
	}
	mcc.status_wide[length] = 0;
	return mcc.status_wide;
}
