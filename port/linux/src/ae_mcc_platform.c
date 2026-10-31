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
#include "ae_platform.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(HALO_SERVER) && !defined(HALO_ANDROID) && !defined(__EMSCRIPTEN__)
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_keyboard.h>
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

static int workshop_same_name(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
	{
		int x = *a >= 'A' && *a <= 'Z' ? *a - 'A' + 'a' : *a, y = *b >= 'A' && *b <= 'Z' ? *b - 'A' + 'a' : *b;

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

int ae_mcc_boot_without_xbox(void)
{
#if !defined(HALO_SERVER) && !defined(HALO_ANDROID) && !defined(__EMSCRIPTEN__)
	char maps[AE_MCC_PATH_SIZE];

	/* (with "no" nothing of Steam's is read: no detection) */
	if (ae_mcc_use() == AE_MCC_USE_NO || ae_mcc_boot_source(0, ae_mcc_root() != NULL, ae_mcc_use()) != AE_MCC_BOOT_MCC)
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
		return ae_mcc_use() == AE_MCC_USE_ASK ? ". MCC found: turn on the main menu's Settings > MAP FILES to use its files." : "";
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
