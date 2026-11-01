/*
AE_MCC_BOOT.C

(Arena Evolved) booting with no Xbox data (ae_mcc_boot.h).
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "cseries/cseries_windows.h"

#include "ae_mcc_boot.h"
#include "ae_map_kind.h"
#include "../src/ae_mcc.h"

#include <stdio.h>
#include <string.h>

/* (cache_files.c's) */
char const *cache_files_map_directory(void);

static int ae_mcc_boot_source_decided = -1;

int ae_mcc_boot_from_mcc(void)
{
	if (ae_mcc_boot_source_decided < 0)
	{
		char path[256], root[AE_MCC_PATH_SIZE];
		FILE *file;
		int xbox_ui_present, mcc_found;

		/* (the game's fopen: d:\maps\ui.map under the data root) */
		snprintf(path, sizeof(path), "%sui.map", cache_files_map_directory());
		file = fopen(path, "rb");
		xbox_ui_present = file != NULL;
		if (file)
			fclose(file);
		/* (MCC looked for only when it can decide: no Xbox ui.map and game.mcc_use not "no", which reads nothing of
		Steam's) */
		mcc_found = !xbox_ui_present && ae_mcc_use() != AE_MCC_USE_NO && ae_mcc_copy_root(root, sizeof(root));
		ae_mcc_boot_source_decided = ae_mcc_boot_source(xbox_ui_present, mcc_found, ae_mcc_use());
		if (ae_mcc_boot_source_decided == AE_MCC_BOOT_MCC)
			error(_error_silent, "mcc boot: ui from %s/halo1/maps/ui.map (MCC v13)", root);
		else if (ae_mcc_boot_source_decided == AE_MCC_BOOT_XBOX)
			error(_error_silent, "mcc boot: ui from %s (Xbox)", path);
		else
			error(_error_silent, "mcc boot: no Halo CE data: install Halo CE in MCC or add an Xbox dump");
	}
	return ae_mcc_boot_source_decided == AE_MCC_BOOT_MCC;
}

int ae_mcc_boot_ui(char const *name)
{
	/* (the level's name, or its scenario's path, as the cache files ask: never mcc_maps\ui, a map so named) */
	return name && (!_stricmp(name, "ui") || !_stricmp(name, "levels\\ui\\ui") || !_stricmp(name, "levels/ui/ui")) &&
		ae_mcc_boot_from_mcc();
}

/* an identity of MCC's own maps, <name>@mcc or <name>@ws<item id>, from a level name: itself, or the leaf of the
scenario's path the cache files ask for (levels\\test\\<identity>\\<identity>); NULL for none */
static char const *ae_mcc_own_identity(char const *name)
{
	char const *leaf = name, *cursor;
	int kind;

	if (!name)
		return NULL;
	for (cursor = name; *cursor; cursor++)
	{
		if (*cursor == '\\' || *cursor == '/')
			leaf = cursor + 1;
	}
	if (leaf != name && _strnicmp(name, "levels\\test\\", 12) && _strnicmp(name, "levels/test/", 12))
		return NULL;
	kind = ae_map_kind_of(leaf);
	/* (GulchCE's mcc_maps\ folders are its own levels, not these) */
	return kind == AE_MAP_KIND_MCC_STOCK || (kind == AE_MAP_KIND_MCC_WORKSHOP && strchr(leaf, '@')) ? leaf : NULL;
}

/* a Workshop identity's map in the catalog the start's scan found (ae_mcc_workshop_map_path) */
static int ae_mcc_workshop_found(char const *identity)
{
	char name[64], id[24], path[AE_MCC_PATH_SIZE];

	return ae_map_kind_workshop_parts(identity, name, sizeof(name), id, sizeof(id)) &&
		ae_mcc_workshop_map_path(id, name, path, sizeof(path));
}

int ae_mcc_install_level(char const *name)
{
	char root[AE_MCC_PATH_SIZE];
	char const *identity = ae_mcc_own_identity(name);

	if (ae_mcc_boot_ui(name))
		return 1;
	if (!identity || ae_mcc_use() == AE_MCC_USE_NO || !ae_mcc_copy_root(root, sizeof(root)))
		return 0;
	return ae_map_kind_of(identity) == AE_MAP_KIND_MCC_STOCK || ae_mcc_workshop_found(identity);
}

char const *ae_mcc_install_level_identity(char const *name)
{
	return ae_mcc_boot_ui(name) ? "ui" : ae_mcc_own_identity(name);
}

int ae_mcc_install_level_path(char const *name, char *path, unsigned long size)
{
	char file[64], id[24];
	char const *identity = ae_mcc_own_identity(name);
	int length;

	if (ae_mcc_boot_ui(name))
		length = snprintf(path, size, "mcc:\\halo1\\ui.map");
	else if (identity && ae_map_kind_mcc_stock_file(identity, file, sizeof(file)))
		length = snprintf(path, size, "mcc:\\halo1\\%s.map", file);
	else if (identity && ae_map_kind_workshop_parts(identity, file, sizeof(file), id, sizeof(id)))
		length = snprintf(path, size, "mcc:\\ws\\%s\\%s.map", id, file);
	else
		return 0;
	return length > 0 && (unsigned long)length < size;
}

void ae_mcc_stock_missing_message(char const *file, char *message, long size)
{
	char stock[64], id[24];

	if (!message || size <= 0)
		return;
	if (ae_map_kind_workshop_parts(file, stock, sizeof(stock), id, sizeof(id)))
	{
		if (ae_mcc_use() == AE_MCC_USE_NO)
			snprintf(message, (size_t)size, "This game plays %s from Halo: MCC Workshop item %s, and Settings > MAP FILES "
				"has MCC's files turned off. Turn them on to join.", stock, id);
		else
			snprintf(message, (size_t)size, "This game plays %s from Halo: MCC Workshop item %s. Subscribe to it in "
				"Steam (with Halo: The Master Chief Collection installed) to join.", stock, id);
		return;
	}
	if (!ae_map_kind_mcc_stock_file(file, stock, sizeof(stock)))
		return;
	if (ae_mcc_use() == AE_MCC_USE_NO)
		snprintf(message, (size_t)size, "This game plays %s from Halo: MCC's own files, which Settings > MAP FILES has "
			"turned off. Turn them on to join.", stock);
	else
		snprintf(message, (size_t)size, "This game plays %s from Halo: MCC's own files. Install Halo: The Master Chief "
			"Collection in Steam, or choose its folder in Settings > MAP FILES, to join.", stock);
}

void ae_mcc_install_level_problem(char const *name, int missing, char *reason, unsigned long size)
{
	char const *identity = ae_mcc_install_level_identity(name);
	char file[64], id[24];

	if (!reason || !size || !identity || !ae_mcc_install_level(name))
		return;
	if (ae_map_kind_workshop_parts(identity, file, sizeof(file), id, sizeof(id)))
		snprintf(reason, size, missing ? "%s.map of Workshop item %s is missing. Check the item in Steam." :
			"%s.map of Workshop item %s can't be read: Steam may be updating it.", file, id);
	else
	{
		if (!ae_map_kind_mcc_stock_file(identity, file, sizeof(file)))
			snprintf(file, sizeof(file), "ui");
		snprintf(reason, size, missing ? "MCC's %s.map is missing. Verify MCC's files in Steam." :
			"MCC's %s.map can't be read: Steam may be updating it.", file);
	}
}

