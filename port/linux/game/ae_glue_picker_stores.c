/*
AE_GLUE_PICKER_STORES.C

The pickers' saved data. See ae_glue_picker_stores.h.
*/

#include <stdio.h>
#include <string.h>

#include "../src/ae_private_files.h"
#include "ae_glue_picker_stores.h"

void platform_log(char const *format, ...);
int config_boolean(char const *name);

/* (display.arena_menus_fold_on_start, owner 0.7.8: every section starts folded each time the game starts) the folds are then
this session's only: the section store holds the sections opened, nothing is read or written */
static int fold_on_start(void)
{
	static int read, on;

	if (!read)
	{
		read = 1;
		on = config_boolean("display.arena_menus_fold_on_start") != 0;
	}
	return on;
}

static const char *const folder_files[AE_PF_KINDS] = { "map-folders.txt", "gametype-folders.txt" };
#define RECENT_FILE "recent-games.txt"
/* (RECENT folded per picker, apart from the games: ae_recent_folds_write) */
#define FOLDS_FILE "recent-folds.txt"
/* (the sections of CHANGE MAP's folders that are folded: ae_section_folds.h) */
#define SECTION_FOLDS_FILE "section-folds.txt"

static struct
{
	/* (about 1.1 MB: static) */
	struct ae_picker_folders folders[AE_PF_KINDS];
	int folders_read[AE_PF_KINDS];
	struct ae_recent_games recent;
	int recent_read;
	struct ae_section_folds sections;
	int sections_read;
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
	if (fold_on_start())
	{
		stores.recent.maps_folded = 1;
		stores.recent.gametypes_folded = 1;
		return &stores.recent;
	}
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
	if (fold_on_start())
		return;
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

static struct ae_section_folds *section_store(void)
{
	long length;

	if (stores.sections_read)
		return &stores.sections;
	stores.sections_read = 1;
	ae_section_folds_init(&stores.sections);
	if (fold_on_start())
		return &stores.sections;
	length = ae_private_file_read(SECTION_FOLDS_FILE, stores.text, sizeof(stores.text));
	if (length >= 0 && !ae_section_folds_read(&stores.sections, stores.text))
		ae_section_folds_init(&stores.sections);
	return &stores.sections;
}

int ae_section_store_folded(const char *picker, const char *key)
{
	int marked = ae_section_folds_get(section_store(), picker, key);

	/* (fold on start: the store marks the sections opened) */
	return fold_on_start() ? !marked : marked;
}

void ae_section_store_set_folded(const char *picker, const char *key, int folded)
{
	static char text[AE_SECTION_FOLDS_TEXT];
	long length;

	if (!ae_section_folds_set(section_store(), picker, key, fold_on_start() ? !folded : folded) || fold_on_start())
		return;
	length = ae_section_folds_write(&stores.sections, text, sizeof(text));
	if (length < 0 || !ae_private_file_write(SECTION_FOLDS_FILE, text, length))
		platform_log("ae pickers: %s could not be written", SECTION_FOLDS_FILE);
}
