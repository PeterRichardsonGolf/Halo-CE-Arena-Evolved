/*
AE_SERVER_PREVIEW.C

The server's start-up summary (ae_server_preview.h).
*/

#include <stdio.h>
#include <string.h>

#include "cseries/cseries.h"
#include "halo_map_families.h"
#include "ae_server_preview.h"
#include "ae_server_bots.h"
#include "../src/ae_mcc.h"
#include "../src/ae_platform.h"
#include "../src/server_log.h"

void platform_translate_path(const char *xbox_path, char *host_path, unsigned long host_path_size);

static void log_found(void)
{
	char root[AE_MCC_PATH_SIZE];
	int items = 0, maps = 0, hidden = 0;

	if (!ae_mcc_copy_root(root, sizeof(root)))
	{
		server_log(_server_log_info, "server", "MCC folder: not found. @mcc and @ws maps need it: set HALO_MCC_PATH to the folder "
			"that holds halo1/ (Steam: steamapps/common/Halo The Master Chief Collection)");
		return;
	}
	server_log(_server_log_info, "server", "MCC folder: %s", root);
	ae_mcc_workshop_counts(&items, &maps, &hidden);
	server_log(_server_log_info, "server", "Workshop items found: %d, with %d maps%s. A new subscription needs a server restart",
		items, maps, hidden ? " (some hidden: see mcc workshop hidden lines in debug.txt)" : "");
}

int ae_server_preview_boot(void)
{
	if (!ae_mcc_boot_without_xbox())
		return 0;
	server_log(_server_log_info, "server", "no Xbox ui.map: booting from MCC's files");
	ae_mcc_workshop_log();
	log_found();
	return 1;
}

void ae_server_preview_scan(void)
{
	ae_mcc_workshop_log();
	log_found();
}

void ae_server_preview_rotation(struct server_rotation const *rotation)
{
	int index, playing = 0;

	for (index = 0; index < rotation->count; index++)
	{
		if (rotation->entries[index].rejected[0])
			continue;
		playing++;
		server_log(_server_log_info, "server", "rotation, will play %d: %s", playing, rotation->entries[index].text);
	}
	if (!playing)
		server_log(_server_log_warning, "server", "rotation: nothing to play (server.rotation, HALO_SERVER_ROTATION)");
	ae_server_bots_summary();
}

int ae_server_preview_ce_find(const char *file, char *path, int size)
{
	/* a Custom Edition map where the game looks for it (maps_ce, maps/ce, custom_maps, maps/<name>@ce.map...) */
	char game_path[1024];

	if (!map_family_find(_map_family_custom_edition, file, game_path, sizeof(game_path)))
		return 0;
	/* (the game's own form of the path, d:\maps_ce\<file>.map, as the host's files are named) */
	platform_translate_path(game_path, path, (unsigned long)size);
	return 1;
}
