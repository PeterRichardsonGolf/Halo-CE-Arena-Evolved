/*
AE_GLUE_PICKER_STORES.C

The pickers' saved data. See ae_glue_picker_stores.h.
*/

#include <stdio.h>
#include <string.h>

#include "../src/ae_private_files.h"
#include "ae_glue_picker_stores.h"

void platform_log(char const *format, ...);

static const char *const folder_files[AE_PF_KINDS] = { "map-folders.txt", "gametype-folders.txt" };
#define RECENT_FILE "recent-games.txt"
/* (RECENT folded per picker, apart from the games: ae_recent_folds_write) */
#define FOLDS_FILE "recent-folds.txt"

static struct
{
	/* (about 1.1 MB: static) */
	struct ae_picker_folders folders[AE_PF_KINDS];
	int folders_read[AE_PF_KINDS];
	struct ae_recent_games recent;
	int recent_read;
	char text[AE_PF_TEXT];
} stores;

/* a refused file kept aside, said once */
static void set_aside(const char *file)
{
	if (ae_private_file_set_aside(file))
		platform_log("ae pickers: %s refused: kept as %s.bad, a fresh one started", file, file);
	else
		platform_log("ae pickers: %s refused (not kept aside), a fresh one started", file);
}

struct ae_picker_folders *ae_picker_store(int kind)
{
	struct ae_picker_folders *store;
	long length;

	if (kind < 0 || kind >= AE_PF_KINDS)
		kind = AE_PF_MAPS;
	store = &stores.folders[kind];
	if (stores.folders_read[kind])
		return store;
	stores.folders_read[kind] = 1;
	length = ae_private_file_read(folder_files[kind], stores.text, sizeof(stores.text));
	if (length < 0 && length != -2)
	{
		ae_pf_init(store, kind);
		platform_log("ae pickers: %s: none yet", folder_files[kind]);
		return store;
	}
	if (length == -2 || !ae_pf_read(store, kind, stores.text))
	{
		ae_pf_init(store, kind);
		set_aside(folder_files[kind]);
		return store;
	}
	platform_log("ae pickers: %s: %d folders, %d entries (%d lines dropped)", folder_files[kind], store->folder_count,
		store->entry_count, store->dropped);
	return store;
}

int ae_picker_store_save(int kind)
{
	long length;

	if (kind < 0 || kind >= AE_PF_KINDS || !stores.folders_read[kind])
		return 0;
	length = ae_pf_write(&stores.folders[kind], stores.text, sizeof(stores.text));
	if (length < 0 || !ae_private_file_write(folder_files[kind], stores.text, length))
	{
		platform_log("ae pickers: %s could not be written", folder_files[kind]);
		return 0;
	}
	platform_log("ae pickers: %s written (%d folders, %d entries)", folder_files[kind], stores.folders[kind].folder_count,
		stores.folders[kind].entry_count);
	return 1;
}

struct ae_recent_games *ae_recent_store(void)
{
	long length;

	if (stores.recent_read)
		return &stores.recent;
	stores.recent_read = 1;
	length = ae_private_file_read(RECENT_FILE, stores.text, sizeof(stores.text));
	if (length < 0 && length != -2)
	{
		ae_recent_games_init(&stores.recent);
		platform_log("ae pickers: %s: none yet", RECENT_FILE);
	}
	else if (length == -2 || !ae_recent_games_read(&stores.recent, stores.text))
	{
		ae_recent_games_init(&stores.recent);
		set_aside(RECENT_FILE);
	}
	else
		platform_log("ae pickers: %s: %d games", RECENT_FILE, stores.recent.count);
	length = ae_private_file_read(FOLDS_FILE, stores.text, sizeof(stores.text));
	if (length >= 0)
	{
		struct ae_recent_games folds;

		if (ae_recent_games_read(&folds, stores.text))
		{
			stores.recent.maps_folded = folds.maps_folded;
			stores.recent.gametypes_folded = folds.gametypes_folded;
		}
	}
	return &stores.recent;
}

int ae_recent_store_folded(int by)
{
	struct ae_recent_games *recent = ae_recent_store();

	return by == AE_RECENT_BY_GAMETYPE ? recent->gametypes_folded : recent->maps_folded;
}

void ae_recent_store_set_folded(int by, int folded)
{
	struct ae_recent_games *recent = ae_recent_store();
	short *flag = by == AE_RECENT_BY_GAMETYPE ? &recent->gametypes_folded : &recent->maps_folded;
	long length;

	folded = folded != 0;
	if (*flag == folded)
		return;
	*flag = (short)folded;
	length = ae_recent_folds_write(recent, stores.text, sizeof(stores.text));
	if (length < 0 || !ae_private_file_write(FOLDS_FILE, stores.text, length))
		platform_log("ae pickers: %s could not be written", FOLDS_FILE);
}

void ae_recent_store_record(const char *map, const char *gametype)
{
	struct ae_recent_games *recent = ae_recent_store();
	long length;

	if (!ae_recent_games_add(recent, map, gametype))
	{
		platform_log("ae pickers: recent game not kept (map '%s', gametype '%s')", map ? map : "",
			gametype ? gametype : "");
		return;
	}
	platform_log("ae pickers: recent game '%s' | '%s'", map, gametype);
	length = ae_recent_games_write(recent, stores.text, sizeof(stores.text));
	if (length < 0 || !ae_private_file_write(RECENT_FILE, stores.text, length))
		platform_log("ae pickers: %s could not be written", RECENT_FILE);
}
