/*
CEV_GLUE.C

What the Custom Edition cache copy (cev/) asks of parts this build has not:
there are no mounted campaigns of Custom Edition maps here, so a map is
never one of a campaign's; and the memory a Custom Edition map's tags are
made for, which the platform reserves as the game starts.
*/

#include "cev/cev_names.h"

#include "cseries.h"

/* (xbox_memory.c: the tag cache's address and whether it was reserved) */
#define CEV_TAG_CACHE_ADDRESS 0x40440000U
#ifdef HALO_CUSTOM_EDITION
extern int platform_ce_tag_cache_ready;
#else
/* (a build without Custom Edition reserves no such memory) */
#define platform_ce_tag_cache_ready 0
#endif

void *halo_custom_edition_tag_cache(void);

#include "cev/custom_campaigns.h"
#include "cev/tag_editor.h"
#include "mcc_maps.h"
#include "ae_glue_mcc_maps.h"

#include <stdlib.h>
#include "cev_glue.h"

#include <string.h>

boolean custom_campaign_map_path(
	char const *map_name,
	boolean custom_edition,
	char *path)
{
	(void)map_name;
	(void)custom_edition;
	(void)path;

	return FALSE;
}

boolean custom_campaign_custom_edition_map(
	char const *map_name)
{
	(void)map_name;

	return FALSE;
}

boolean custom_campaign_file_path(
	char const *name,
	char *path)
{
	(void)name;
	(void)path;

	return FALSE;
}

void *halo_custom_edition_tag_cache(
	void)
{
	return platform_ce_tag_cache_ready ? xbox_pointer(CEV_TAG_CACHE_ADDRESS) : NULL;
}

/* ---------- the Custom Edition maps tags can be taken out of */

/* (the map lists of this build are not the copy's: the maps are named to it,
cev_maps_add) */
enum
{
	MAXIMUM_CEV_MAPS = 64,
	MAXIMUM_CEV_MAP_NAME = 64,
};

static char cev_map_names[MAXIMUM_CEV_MAPS][MAXIMUM_CEV_MAP_NAME];
static short cev_map_count;

boolean cev_maps_add(
	char const *level_name)
{
	short index;

	if (!level_name || !level_name[0] || strlen(level_name) >= MAXIMUM_CEV_MAP_NAME)
		return FALSE;
	for (index = 0; index < cev_map_count; index++)
	{
		if (!strcmp(cev_map_names[index], level_name))
			return TRUE;
	}
	if (cev_map_count >= MAXIMUM_CEV_MAPS)
		return FALSE;
	strcpy(cev_map_names[cev_map_count++], level_name);

	return TRUE;
}

short custom_edition_maps_count(
	boolean campaign)
{
	return campaign ? 0 : cev_map_count;
}

short custom_edition_maps_display_index_of(
	boolean campaign,
	short index)
{
	return !campaign && index >= 0 && index < cev_map_count ? index : NONE;
}

char const *custom_edition_maps_level_name(
	short display_index)
{
	return display_index >= 0 && display_index < cev_map_count ? cev_map_names[display_index] : NULL;
}

/* ---------- the texture cache */

/* (the texture cache of this build draws every bitmap's texels in the Xbox's
channel order: a bitmap whose channels are elsewhere is drawn as stored) */
void halo_custom_edition_texels_channels(
	void const *texels,
	unsigned char channel_order)
{
	(void)texels;
	(void)channel_order;
}

/* ---------- the MCC maps tags can be taken out of */

/* (this build reads MCC's maps in place and names each by its identity, <name>@mcc or <name>@ws<item id>, which
is its level name too: the list is the menus', made again at each rescan; none is listed as a campaign map) */
static struct ae_mcc_menu_map *cev_mcc_maps;
static int cev_mcc_map_count;

void mcc_maps_rescan(
	void)
{
	enum { MAXIMUM_CEV_MCC_MAPS = 1100 };

	if (!cev_mcc_maps)
		cev_mcc_maps = (struct ae_mcc_menu_map *)malloc(MAXIMUM_CEV_MCC_MAPS * sizeof(*cev_mcc_maps));
	cev_mcc_map_count = cev_mcc_maps ? ae_mcc_menu_maps(cev_mcc_maps, MAXIMUM_CEV_MCC_MAPS) : 0;
}

short mcc_maps_type_count(
	boolean campaign)
{
	return campaign ? 0 : (short)cev_mcc_map_count;
}

short mcc_maps_type_index(
	short row,
	boolean campaign)
{
	return !campaign && row >= 0 && row < cev_mcc_map_count ? row : NONE;
}

char const *mcc_maps_level_name(
	short index)
{
	return index >= 0 && index < cev_mcc_map_count && !cev_mcc_maps[index].updating ?
		cev_mcc_maps[index].identity : NULL;
}

wchar_t const *mcc_maps_name(
	short index)
{
	static wchar_t name[64];
	char const *text = index >= 0 && index < cev_mcc_map_count ? cev_mcc_maps[index].name : "";
	short character;

	for (character = 0; character < 63 && text[character]; character++)
		name[character] = (wchar_t)(unsigned char)text[character];
	name[character] = 0;

	return name;
}

/* ---------- an MCC map's tags elsewhere than at their base */

/* (the tags of an MCC map are always at the base they were written for in this build: when it is taken, by a map
being played, no bundle is made) */
unsigned long platform_mcc_shift;
unsigned long platform_mcc_linked_base;
/* (and a Custom Edition map's tag cache is never moved: cev/cache_file_formats.c reads this shift, 0) */
unsigned long platform_custom_edition_shift;

void *mcc_memory_reserve_elsewhere(
	unsigned int linked,
	unsigned int size)
{
	(void)linked;
	(void)size;

	return NULL;
}

/* ---------- the tag editor: this build has none */

void tag_editor_weapon_loaded(
	long profile,
	long weapon_datum)
{
	(void)profile;
	(void)weapon_datum;
}

void tag_editor_level_unloaded(
	void)
{
}

/* ---------- what the copy's Tag Injector and cache units ask of parts this build has not

(a build that links without dropping unreached code needs each defined: the Windows test builds) */

/* this build's Custom Edition map list keeps level names only (cev_maps_add): no display name, no category */
wchar_t *custom_edition_maps_name(
	short display_index)
{
	(void)display_index;

	return NULL;
}

char const *custom_edition_maps_category(
	boolean campaign,
	short index)
{
	(void)campaign;
	(void)index;

	return NULL;
}

/* the copy's Custom Edition script conversion is not built here: a map whose scripts need it is not taken */
boolean custom_edition_scripts_convert(
	byte *tag_cache,
	unsigned long loaded_bytes)
{
	(void)tag_cache;
	(void)loaded_bytes;

	return FALSE;
}
