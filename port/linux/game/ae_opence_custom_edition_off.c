/*
AE_OPENCE_CUSTOM_EDITION_OFF.C

Arena Evolved: OpenCE's Custom Edition maps' loader and lists (build-145,
custom_edition_*.c, cache_file_formats.c, bmp_files.c, stb_vorbis.c) are not
built into the game (tools/linux_build.py, OPENCE_CUSTOM_EDITION_SOURCES).
Arena Evolved plays Halo Custom Edition and HaloMD maps with
ChupathingyCE's loader (ce_*.c), as <file>@ce and <file>@md from maps/ce and
md_maps (halo_map_families.h). The game's calls into OpenCE's loader, which
upstream's code makes as it is, are answered here: no OpenCE Custom Edition
map is ever loaded, so every one finds none.

The one name OpenCE gives such a map on the network, custom_maps\<name>
(network version 22), is still told apart (custom_edition_level_name), so
that a client of an OpenCE host on one is told what it is
(cache_files_map_present) and never plays the Xbox map of that file name in
its place.
*/

#include "cseries.h"

#include "custom_edition_cache.h"
#include "custom_edition_maps.h"

#include <stdio.h>
#include <string.h>

/* ---------- custom_edition_cache.h */

boolean custom_edition_cache_stock_tag(
	long tag_index)
{
	(void)tag_index;
	return FALSE;
}

boolean custom_edition_cache_refuse(
	void const *header,
	char const *build,
	char const *path)
{
	(void)header;
	(void)build;
	(void)path;
	return FALSE;
}

boolean custom_edition_level_name(
	char const *level_name)
{
	return level_name &&
		!_strnicmp(level_name, CUSTOM_EDITION_LEVEL_NAME_PREFIX, strlen(CUSTOM_EDITION_LEVEL_NAME_PREFIX));
}

boolean custom_edition_cache_playable(
	char const *level_name)
{
	(void)level_name;
	return FALSE;
}

boolean custom_edition_map_file_present(
	char const *map_name)
{
	(void)map_name;
	return FALSE;
}

boolean custom_edition_cache_present(
	char const *level_name,
	char *message,
	long message_size)
{
	char const *name = level_name ? level_name + strlen(CUSTOM_EDITION_LEVEL_NAME_PREFIX) : "";

	if (message && message_size > 0)
	{
		snprintf(message, (size_t)message_size,
			"The host's map %.64s is a Custom Edition map named as OpenCE names them. Arena Evolved plays "
			"Custom Edition maps from maps/ce, as ChupathingyCE does: a host of either plays them with it.",
			custom_edition_level_name(level_name) ? name : "");
	}
	return FALSE;
}

boolean custom_edition_cache_campaign(
	char const *file_name)
{
	(void)file_name;
	return FALSE;
}

boolean custom_edition_cache_multiplayer(
	char const *file_name)
{
	(void)file_name;
	return FALSE;
}

struct cache_file_tag_header *custom_edition_cache_tags_load(
	char const *map_name,
	void *header)
{
	(void)map_name;
	(void)header;
	return NULL;
}

boolean custom_edition_install_present(
	void)
{
	return FALSE;
}

boolean custom_edition_cache_tags_loaded(
	void)
{
	return FALSE;
}

boolean custom_edition_structure_bsp_reference_valid(
	struct scenario_structure_bsp_reference const *reference)
{
	(void)reference;
	return FALSE;
}

boolean custom_edition_cache_relies_on(
	short behaviour)
{
	(void)behaviour;
	return FALSE;
}

void custom_edition_cache_tags_unload(
	void)
{
	return;
}

void custom_edition_cache_read(
	long tag_index,
	long offset,
	long size,
	void *buffer)
{
	(void)tag_index;
	(void)offset;
	if (buffer && size > 0)
		memset(buffer, 0, (size_t)size);
	return;
}

boolean custom_edition_vehicles_by_placement(
	void)
{
	return FALSE;
}

boolean custom_edition_vehicle_placement_allowed(
	struct scenario_object_datum const *placement)
{
	(void)placement;
	return TRUE;
}

short custom_edition_part_palette(
	struct vertex_buffer const *vertex_buffer,
	byte const **nodes)
{
	(void)vertex_buffer;
	if (nodes)
		*nodes = NULL;
	return 0;
}

boolean custom_edition_structure_bsp_load(
	struct structure_bsp *structure_bsp)
{
	(void)structure_bsp;
	return FALSE;
}

void custom_edition_structure_bsp_unload(
	void)
{
	return;
}

/* ---------- custom_edition_maps.h */

char **custom_edition_maps_level_list(
	char **xbox_levels,
	short xbox_level_count,
	short *level_count)
{
	if (level_count)
		*level_count = xbox_level_count;
	return xbox_levels;
}

void custom_edition_maps_look_again(
	void)
{
	return;
}

short custom_edition_maps_level_display_index(
	short level_index)
{
	return level_index;
}

short custom_edition_maps_display_index(
	char const *level_name)
{
	(void)level_name;
	return NONE;
}

boolean custom_edition_maps_campaign(
	short display_index)
{
	(void)display_index;
	return FALSE;
}

short custom_edition_maps_campaign_level(
	short display_index)
{
	(void)display_index;
	return NONE;
}

boolean custom_edition_maps_level_campaign(
	char const *level_name)
{
	(void)level_name;
	return FALSE;
}

short custom_edition_maps_count(
	boolean campaign)
{
	(void)campaign;
	return 0;
}

short custom_edition_maps_display_index_of(
	boolean campaign,
	short index)
{
	(void)campaign;
	(void)index;
	return NONE;
}

char const *custom_edition_maps_level_name(
	short display_index)
{
	(void)display_index;
	return NULL;
}

boolean custom_edition_maps_stock(
	short display_index)
{
	(void)display_index;
	return FALSE;
}

wchar_t *custom_edition_maps_name(
	short display_index)
{
	(void)display_index;
	return NULL;
}

wchar_t *custom_edition_maps_description(
	short display_index)
{
	(void)display_index;
	return NULL;
}

struct bitmap_data *custom_edition_maps_picture(
	long bitmap_tag_index,
	short *frame_index)
{
	(void)bitmap_tag_index;
	(void)frame_index;
	return NULL;
}
