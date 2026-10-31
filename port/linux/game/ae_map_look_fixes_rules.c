/*
AE_MAP_LOOK_FIXES_RULES.C

AE's look fixes' table and rules (ae_map_look_fixes_rules.h).
*/

#include "ae_map_look_fixes_rules.h"

#include <ctype.h>
#include <string.h>

/* ---------- constants */

#define AE_GROUP(a, b, c, d) \
	(((unsigned long)(a) << 24) | ((unsigned long)(b) << 16) | ((unsigned long)(c) << 8) | (unsigned long)(d))

enum
{
	/* shader_transparent_chicago: framebuffer_blend_function, a short after the shader's 0x28 bytes, the counter
	limit, the flags and first_map_type (rasterizer_xbox_transparent_geometry.c's
	shader_transparent_chicago_definition) */
	CHICAGO_FRAMEBUFFER_BLEND_FUNCTION_OFFSET = 0x2C,
	FRAMEBUFFER_BLEND_ALPHA_BLEND = 0,
	FRAMEBUFFER_BLEND_ADD = 3,
	/* an object's model and collision model references (object_definitions.h's _object_definition) */
	OBJECT_MODEL_OFFSET = 0x28,
	OBJECT_COLLISION_MODEL_OFFSET = 0x70,
	/* a tag reference: the group, the name, the name's length, the tag index (tag_groups.h) */
	TAG_REFERENCE_SIZE = 0x10,
	TAG_REFERENCE_INDEX_OFFSET = 0xC
};

/* ---------- globals */

/* h2d_coagulation (H2 Demakes, an Xbox map): the teleporter's energy sheet is an alpha-blended chicago shader
whose four maps (the field generator's dust, the sheet's streak) are all opaque DXT1, so its alpha is 1
everywhere and the sheet draws as an opaque black card behind the blue streak (on the Xbox too: the stock
engine's chicago path, which this is). The streak's bitmap asks for DXT3 and holds DXT1: its alpha was lost when
the map was built. Added, black adds nothing and the streak glows on its own, as the sheet was meant to look.
h2d_headlong's and h2d_warlock's teleporters have the same sheet (the same four maps, their "power" streak
opaque DXT1 too; warlock's alpha functions add and multiply, still 1 everywhere), seen with the same card in box
games. (h2d_elongation's alike sheet is on a vehicle the map never places: not fixed.) */
struct ae_map_look_fix const ae_map_look_fixes[] =
{
	{ "h2d_coagulation", AE_GROUP('s', 'c', 'h', 'i'), "scenery\\protected 24\\shaders\\base part1",
		CHICAGO_FRAMEBUFFER_BLEND_FUNCTION_OFFSET, FRAMEBUFFER_BLEND_ALPHA_BLEND, FRAMEBUFFER_BLEND_ADD,
		"the teleporter's sheet added, not alpha-blended (its maps have no alpha: a black card)" },
	{ "h2d_headlong", AE_GROUP('s', 'c', 'h', 'i'), "scenery\\h2_teleporter\\shaders\\generator shield",
		CHICAGO_FRAMEBUFFER_BLEND_FUNCTION_OFFSET, FRAMEBUFFER_BLEND_ALPHA_BLEND, FRAMEBUFFER_BLEND_ADD,
		"the teleporters' sheet added, not alpha-blended (its maps have no alpha: a black card)" },
	{ "h2d_warlock", AE_GROUP('s', 'c', 'h', 'i'), "scenery\\h2_teleporter\\shaders\\generator shield",
		CHICAGO_FRAMEBUFFER_BLEND_FUNCTION_OFFSET, FRAMEBUFFER_BLEND_ALPHA_BLEND, FRAMEBUFFER_BLEND_ADD,
		"the teleporters' sheet added, not alpha-blended (its maps have no alpha: a black card)" },
};

unsigned int const ae_map_look_fix_count = sizeof(ae_map_look_fixes) / sizeof(ae_map_look_fixes[0]);

/* h2d_salicreek (H2 Demakes, an Xbox map): its overshield's equipment (item collection "over shield") is drawn
with the model named "over shield", which in Halo's own tags is the active camouflage's (Bungie's names are
swapped: the stock overshield equipment uses "active camoflage"'s model and collision model, the camouflage
"over shield"'s), so the pickup looks like the camouflage sphere. The map has the stock pair too (the stock
overshield equipment, which no item collection places): the equipment is given it, model and collision model
together as the stock overshield has them (both models one node, one region, one geometry, three shaders). */
struct ae_map_look_retarget const ae_map_look_retargets[] =
{
	{ "h2d_salicreek", AE_GROUP('e', 'q', 'i', 'p'), "powerups\\over shield\\over shield", OBJECT_MODEL_OFFSET,
		AE_GROUP('m', 'o', 'd', 'e'), "powerups\\over shield\\over shield",
		"powerups\\active camoflage\\active camoflage",
		"the overshield drawn with the stock overshield's model, not the camouflage's" },
	{ "h2d_salicreek", AE_GROUP('e', 'q', 'i', 'p'), "powerups\\over shield\\over shield",
		OBJECT_COLLISION_MODEL_OFFSET, AE_GROUP('c', 'o', 'l', 'l'), "powerups\\over shield\\over shield",
		"powerups\\active camoflage\\active camoflage",
		"the overshield given the stock overshield's collision model, to match its model" },
};

unsigned int const ae_map_look_retarget_count = sizeof(ae_map_look_retargets) / sizeof(ae_map_look_retargets[0]);

/* scenery\spawn_marker's shader on a map's own scenery model, as Fallout's green rings at its spawns (on its
fallout_randoms; Weld's weld_randoms too), always drawn: a Training look (the maps of Halo 1: NHE name theirs spawn_marker_nhe and show them by script, in
their Training only), so not drawn; TRAINING draws Arena Evolved's own (render_spawn_markers.c). Owner 2026-10-20. */
struct ae_map_look_hidden_shader const ae_map_look_hidden_shaders[] =
{
	{ AE_GROUP('s', 'c', 'h', 'i'), "scenery\\spawn_marker\\shaders\\spawn_marker",
		"the spawn rings on the map's scenery not drawn (TRAINING draws AE's own)" },
	/* (Halo 1: NHE's and Patch Edition's own, spawn_marker_nhe: their scripts leave them in every game, 2026-10-29) */
	{ AE_GROUP('s', 'c', 'h', 'i'), "scenery\\spawn_marker_nhe\\shaders\\spawn_marker_nhe",
		"NHE's spawn rings not drawn (TRAINING draws AE's own)" },
};

unsigned int const ae_map_look_hidden_shader_count =
	sizeof(ae_map_look_hidden_shaders) / sizeof(ae_map_look_hidden_shaders[0]);

/* cyberdyneb2 (The Pit Beta, Zteam's Custom Edition remake of The Pit; owner 2026-10-26: "one corner draws
grey"): its BSP's portal 4, the opening at y -12.27 (x 8.70 to 12.56) between the alley (cluster 2) and the room
behind it (cluster 4), joins cluster 2 to itself, and cluster 4's list has only its other portal (1, at x 5.31). The
portal walk never goes through it: from the alley the room (and the outside past it) draws empty grey, and from the
room so does the alley. Its mirror on the map's other half, portal 3 (y -13.76), joins its room (cluster 1) to the
outside (0) with the room first, the side its plane faces away from: portal 4 is given the room and the alley the
same way (its plane, y up, d -12.27, faces the alley) and joins cluster 4's list. The clusters' visibility (PVS)
has them all visible from each other already. Found with a cluster log in box games (2026-10-28): from the alley
spot 9.3 -12.1 the camera is in cluster 2, 9.3 -14 is in cluster 4; the BSP has 6 clusters and 12 portals. */
struct ae_map_portal_fix const ae_map_portal_fixes[] =
{
	{ "cyberdyneb2", 0x7ab68ae9UL, 6, 12, 4, { 2, 2 }, { 4, 2 }, 4, 1, { 1 },
		"the alley's opening into the room joins the room (it joined the alley to itself: both drew grey)" },
};

unsigned int const ae_map_portal_fix_count = sizeof(ae_map_portal_fixes) / sizeof(ae_map_portal_fixes[0]);

/* ---------- public code */

/* whether a fix's map name is the header's (at most AE_MAP_LOOK_FIX_MAP_NAME_SIZE bytes of it, any case) */
static int map_name_matches(char const *fix_name, char const *map_name)
{
	unsigned int index;

	if (!fix_name || !map_name)
		return 0;
	for (index = 0; index < AE_MAP_LOOK_FIX_MAP_NAME_SIZE; index++)
	{
		unsigned char want = (unsigned char)fix_name[index];
		unsigned char have = (unsigned char)map_name[index];

		if (tolower(want) != tolower(have))
			return 0;
		if (!want)
			return 1;
	}
	/* (a name of the header's whole 32 bytes: the fix's must end there too) */
	return fix_name[AE_MAP_LOOK_FIX_MAP_NAME_SIZE] == '\0';
}

int ae_map_look_fix_for_map(struct ae_map_look_fix const *fix, char const *map_name)
{
	return fix && map_name_matches(fix->map_name, map_name);
}

int ae_map_look_retarget_for_map(struct ae_map_look_retarget const *fix, char const *map_name)
{
	return fix && map_name_matches(fix->map_name, map_name);
}

enum ae_map_look_fix_result ae_map_look_retarget_apply(struct ae_map_look_retarget const *fix, unsigned char *data,
	unsigned int size, int32_t expected_index, int32_t new_index)
{
	uint32_t group;
	int32_t index;

	if (!fix || !data || size < TAG_REFERENCE_SIZE || fix->offset > size - TAG_REFERENCE_SIZE)
		return AE_MAP_LOOK_FIX_OUTSIDE;
	/* (-1 is none: a tag's index has its salt in the high bits, so most are negative) */
	if (expected_index == -1 || new_index == -1)
		return AE_MAP_LOOK_FIX_MISSING;
	memcpy(&group, data + fix->offset, sizeof(group));
	memcpy(&index, data + fix->offset + TAG_REFERENCE_INDEX_OFFSET, sizeof(index));
	if (group != (uint32_t)fix->target_group)
		return AE_MAP_LOOK_FIX_UNEXPECTED;
	if (index == new_index)
		return AE_MAP_LOOK_FIX_ALREADY;
	if (index != expected_index)
		return AE_MAP_LOOK_FIX_UNEXPECTED;
	memcpy(data + fix->offset + TAG_REFERENCE_INDEX_OFFSET, &new_index, sizeof(new_index));
	return AE_MAP_LOOK_FIX_APPLIED;
}

enum ae_map_look_fix_result ae_map_look_fix_apply(struct ae_map_look_fix const *fix, unsigned char *data,
	unsigned int size)
{
	short value;

	if (!fix || !data || size < sizeof(value) || fix->offset > size - sizeof(value))
		return AE_MAP_LOOK_FIX_OUTSIDE;
	memcpy(&value, data + fix->offset, sizeof(value));
	if (value == fix->new_value)
		return AE_MAP_LOOK_FIX_ALREADY;
	if (value != fix->old_value)
		return AE_MAP_LOOK_FIX_UNEXPECTED;
	value = fix->new_value;
	memcpy(data + fix->offset, &value, sizeof(value));
	return AE_MAP_LOOK_FIX_APPLIED;
}

int ae_map_portal_fix_for_map(struct ae_map_portal_fix const *fix, char const *map_file, unsigned long map_hash)
{
	return fix && map_name_matches(fix->map_file, map_file) && (map_hash & 0xFFFFFFFFUL) == fix->map_hash;
}

enum ae_map_look_fix_result ae_map_portal_fix_apply(struct ae_map_portal_fix const *fix, long cluster_count,
	long portal_count, short clusters[2], short const *list, long list_count, short *new_list, long new_list_size,
	long *new_list_count)
{
	long index;
	int listed = 0;

	if (!fix || !clusters || (list_count && !list) || list_count < 0 || fix->portal_index < 0 ||
		fix->portal_index >= portal_count || fix->list_cluster < 0 || fix->list_cluster >= cluster_count ||
		fix->old_list_count < 0 || fix->old_list_count > AE_MAP_PORTAL_FIX_LIST_SIZE)
		return AE_MAP_LOOK_FIX_OUTSIDE;
	for (index = 0; index < list_count; index++)
	{
		if (list[index] == fix->portal_index)
			listed = 1;
	}
	if (clusters[0] == fix->new_clusters[0] && clusters[1] == fix->new_clusters[1] && listed)
		return AE_MAP_LOOK_FIX_ALREADY;
	if (cluster_count != fix->cluster_count || portal_count != fix->portal_count ||
		clusters[0] != fix->old_clusters[0] || clusters[1] != fix->old_clusters[1] ||
		list_count != fix->old_list_count || listed)
		return AE_MAP_LOOK_FIX_UNEXPECTED;
	for (index = 0; index < list_count; index++)
	{
		if (list[index] != fix->old_list[index])
			return AE_MAP_LOOK_FIX_UNEXPECTED;
	}
	if (!new_list || !new_list_count || new_list_size < list_count + 1)
		return AE_MAP_LOOK_FIX_OUTSIDE;
	for (index = 0; index < list_count; index++)
		new_list[index] = list[index];
	new_list[list_count] = fix->portal_index;
	*new_list_count = list_count + 1;
	clusters[0] = fix->new_clusters[0];
	clusters[1] = fix->new_clusters[1];
	return AE_MAP_LOOK_FIX_APPLIED;
}

void ae_bitmap_type_errors_reset(struct ae_bitmap_type_errors *errors)
{
	if (errors)
		errors->count = 0;
}

int ae_bitmap_type_errors_first(struct ae_bitmap_type_errors *errors, uintptr_t bitmap, short type)
{
	unsigned int index;

	if (!errors)
		return 0;
	for (index = 0; index < errors->count && index < AE_BITMAP_TYPE_ERRORS_KEPT; index++)
	{
		if (errors->bitmaps[index] == bitmap && errors->types[index] == type)
			return 0;
	}
	if (errors->count >= AE_BITMAP_TYPE_ERRORS_KEPT)
		return 0;
	errors->bitmaps[errors->count] = bitmap;
	errors->types[errors->count] = type;
	errors->count++;
	return 1;
}
