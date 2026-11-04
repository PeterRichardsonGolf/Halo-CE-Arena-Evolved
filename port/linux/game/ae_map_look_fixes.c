/*
AE_MAP_LOOK_FIXES.C

AE's look fixes on a map's loaded tags (ae_map_look_fixes.h). Each fix in the
table (ae_map_look_fixes_rules.c) names its map, its tag by group and name,
and a 16-bit field with the value the map holds and the one it gets (or a tag
reference, with the names of the tag it is to and the one it gets): the field
is changed only when it holds the first, so another version of the map (or one
its author fixed) is left as it is, and said so in the log. A fix changes how a
tag is drawn (an item's collision model with its model: where an item rests is
the host's game's to say), nothing a game's machines must agree on: every
machine makes it, and the map's file and checksum are its own.

A portal fix (ae_map_portal_fixes) changes the BSP's loaded cluster portals: a
portal's two clusters, and a cluster's list of its portals given one more (a
new list, from the game's heap as a layout's lists are, that the BSP points
at until it is loaded again); only for the map's own file (its hash) and a BSP
as the fix expects it.
*/

#include "cseries.h"
#include "scenario/scenario.h"
#include "structures/structure_bsp_definitions.h"
#include "tag_files/tag_groups.h"

#include "ae_map_look_fixes.h"
#include "ae_map_look_fixes_rules.h"

#include <stdint.h>
#include <stdio.h>

void platform_log(char const *format, ...);

/* ---------- constants */

enum
{
	/* the bytes of a fix's tag the field may lie in: the fixes' tags' fixed part, at least (a chicago shader's
	is 0x6C bytes) */
	LOOK_FIX_TAG_BYTES = 0x6C,
	/* the bytes of a reference fix's tag the reference may lie in: an object's fixed part, at least (its
	_object_definition, 0x17C bytes) */
	LOOK_FIX_TAG_BYTES_OBJECT = 0x17C
};

/* ---------- structures */

/* (structure_visibility.c's private cluster and portal views: a cluster's list of its portals; a portal's two
clusters) */
struct look_fix_cluster
{
	byte unused[0x5C];
	struct tag_block portal_indices;
};

struct look_fix_portal
{
	short cluster_indices[2];
	byte unused[0x3C];
};

typedef char look_fix_cluster_size_assert[sizeof(struct look_fix_cluster) == 0x68 ? 1 : -1];
typedef char look_fix_portal_size_assert[sizeof(struct look_fix_portal) == 0x40 ? 1 : -1];

/* ---------- globals */

static struct ae_bitmap_type_errors bitmap_type_errors;

/* the map's hidden shaders' tag data (the table's, found as its tags loaded), NULL past those it has */
static void const *hidden_shaders[4];

/* the portal fixes' new lists (the game's heap), each the BSP's until it is loaded again */
static short *portal_lists[4];
static long portal_list_counts[4];

/* ---------- public code */

void ae_map_look_fixes_tags_loaded(
	char const *map_name)
{
	/* (the header's name: 32 bytes, its terminator the header's to keep) */
	char name[AE_MAP_LOOK_FIX_MAP_NAME_SIZE + 1];
	unsigned int index;

	ae_bitmap_type_errors_reset(&bitmap_type_errors);
	csmemset(hidden_shaders, 0, sizeof(hidden_shaders));
	if (!map_name)
		return;
	snprintf(name, sizeof(name), "%.32s", map_name);
	for (index = 0; index < ae_map_look_hidden_shader_count && index < NUMBEROF(hidden_shaders); index++)
	{
		struct ae_map_look_hidden_shader const *hidden = &ae_map_look_hidden_shaders[index];
		long tag_index = tag_loaded((long)hidden->group, hidden->tag_name);

		if (tag_index == NONE)
			continue;
		hidden_shaders[index] = tag_get((long)hidden->group, tag_index);
		platform_log("look fixes: %s's '%s': %s", name, hidden->tag_name, hidden->what);
	}
	for (index = 0; index < ae_map_look_fix_count; index++)
	{
		struct ae_map_look_fix const *fix = &ae_map_look_fixes[index];
		long tag_index;

		if (!ae_map_look_fix_for_map(fix, name))
			continue;
		tag_index = tag_loaded((long)fix->group, fix->tag_name);
		if (tag_index == NONE)
		{
			platform_log("look fixes: %s has no tag '%s': not fixed", name, fix->tag_name);
			continue;
		}
		switch (ae_map_look_fix_apply(fix, (unsigned char *)tag_get((long)fix->group, tag_index),
			LOOK_FIX_TAG_BYTES))
		{
			case AE_MAP_LOOK_FIX_APPLIED:
				platform_log("look fixes: %s's '%s': %s (%d to %d)", name, fix->tag_name, fix->what,
					(int)fix->old_value, (int)fix->new_value);
				break;
			case AE_MAP_LOOK_FIX_ALREADY:
				platform_log("look fixes: %s's '%s' is fixed in the map already", name, fix->tag_name);
				break;
			case AE_MAP_LOOK_FIX_UNEXPECTED:
				platform_log("look fixes: %s's '%s' is not as the fix expects (another version of the map): not fixed",
					name, fix->tag_name);
				break;
			default:
				platform_log("look fixes: %s's '%s': its field is not in the tag: not fixed", name, fix->tag_name);
				break;
		}
	}
	for (index = 0; index < ae_map_look_retarget_count; index++)
	{
		struct ae_map_look_retarget const *fix = &ae_map_look_retargets[index];
		long tag_index;

		if (!ae_map_look_retarget_for_map(fix, name))
			continue;
		tag_index = tag_loaded((long)fix->group, fix->tag_name);
		if (tag_index == NONE)
		{
			platform_log("look fixes: %s has no tag '%s': not fixed", name, fix->tag_name);
			continue;
		}
		/* (the tags it names found by their group and name: of the group the reference is to) */
		switch (ae_map_look_retarget_apply(fix, (unsigned char *)tag_get((long)fix->group, tag_index),
			LOOK_FIX_TAG_BYTES_OBJECT, (int32_t)tag_loaded((long)fix->target_group, fix->expected_name),
			(int32_t)tag_loaded((long)fix->target_group, fix->new_name)))
		{
			case AE_MAP_LOOK_FIX_APPLIED:
				platform_log("look fixes: %s's '%s': %s ('%s' for '%s')", name, fix->tag_name, fix->what,
					fix->new_name, fix->expected_name);
				break;
			case AE_MAP_LOOK_FIX_ALREADY:
				platform_log("look fixes: %s's '%s' is fixed in the map already", name, fix->tag_name);
				break;
			case AE_MAP_LOOK_FIX_MISSING:
				platform_log("look fixes: %s lacks '%s' or '%s': '%s' not fixed", name, fix->expected_name,
					fix->new_name, fix->tag_name);
				break;
			case AE_MAP_LOOK_FIX_UNEXPECTED:
				platform_log("look fixes: %s's '%s' is not as the fix expects (another version of the map): not fixed",
					name, fix->tag_name);
				break;
			default:
				platform_log("look fixes: %s's '%s': its reference is not in the tag: not fixed", name, fix->tag_name);
				break;
		}
	}
	return;
}

void ae_map_look_fixes_map_loaded(
	char const *map_file,
	unsigned long map_hash)
{
	struct structure_bsp *structure = global_structure_bsp_get();
	unsigned int index;

	if (!map_file || !structure)
		return;
	for (index = 0; index < ae_map_portal_fix_count && index < NUMBEROF(portal_lists); index++)
	{
		struct ae_map_portal_fix const *fix = &ae_map_portal_fixes[index];
		struct look_fix_portal *portal;
		struct look_fix_cluster *cluster;
		short clusters[2], list[AE_MAP_PORTAL_FIX_LIST_SIZE + 1];
		long list_count = 0;

		if (!ae_map_portal_fix_for_map(fix, map_file, map_hash))
			continue;
		if (fix->portal_index >= structure->cluster_portals.count || fix->list_cluster >= structure->clusters.count)
		{
			platform_log("look fixes: %s's BSP has no portal %d or cluster %d (another version of the map): not fixed",
				map_file, (int)fix->portal_index, (int)fix->list_cluster);
			continue;
		}
		portal = TAG_BLOCK_GET_ELEMENT(&structure->cluster_portals, fix->portal_index, struct look_fix_portal);
		cluster = TAG_BLOCK_GET_ELEMENT(&structure->clusters, fix->list_cluster, struct look_fix_cluster);
		clusters[0] = portal->cluster_indices[0];
		clusters[1] = portal->cluster_indices[1];
		switch (ae_map_portal_fix_apply(fix, structure->clusters.count, structure->cluster_portals.count, clusters,
			cluster->portal_indices.count > 0 ? (short const *)xbox_pointer(cluster->portal_indices.address) : NULL,
			cluster->portal_indices.count, list, NUMBEROF(list), &list_count))
		{
			case AE_MAP_LOOK_FIX_APPLIED:
			{
				/* (the last load's list: this BSP, loaded again, has its own) */
				short *copy = (short *)malloc((size_t)list_count * sizeof(*copy));

				if (portal_lists[index])
				{
					free(portal_lists[index]);
					portal_lists[index] = NULL;
					portal_list_counts[index] = 0;
				}
				if (!copy)
				{
					platform_log("look fixes: %s's portal %d: no memory: not fixed", map_file, (int)fix->portal_index);
					break;
				}
				csmemcpy(copy, list, (size_t)list_count * sizeof(*copy));
				portal_lists[index] = copy;
				portal_list_counts[index] = list_count;
				cluster->portal_indices.address = XBOX_ADDRESS(copy);
				cluster->portal_indices.count = list_count;
				portal->cluster_indices[0] = clusters[0];
				portal->cluster_indices[1] = clusters[1];
				platform_log("look fixes: %s's portal %d: %s (clusters %d-%d, cluster %d's portals %ld)", map_file,
					(int)fix->portal_index, fix->what, (int)clusters[0], (int)clusters[1], (int)fix->list_cluster,
					list_count);
				break;
			}
			case AE_MAP_LOOK_FIX_ALREADY:
				platform_log("look fixes: %s's portal %d is fixed already", map_file, (int)fix->portal_index);
				break;
			case AE_MAP_LOOK_FIX_UNEXPECTED:
				platform_log("look fixes: %s's portal %d is not as the fix expects (another version of the map): not fixed",
					map_file, (int)fix->portal_index);
				break;
			default:
				platform_log("look fixes: %s's portal %d: not in the BSP: not fixed", map_file, (int)fix->portal_index);
				break;
		}
	}
	return;
}

int ae_map_look_fixes_memory(
	unsigned long address,
	long size)
{
	unsigned int index;

	for (index = 0; index < NUMBEROF(portal_lists); index++)
	{
		unsigned long start, end;

		if (!portal_lists[index] || size <= 0)
			continue;
		start = (unsigned long)XBOX_ADDRESS(portal_lists[index]);
		end = start + (unsigned long)portal_list_counts[index] * sizeof(short);
		if (address >= start && address < end && (unsigned long)size <= end - address)
			return 1;
	}
	return 0;
}

boolean ae_map_look_bitmap_type_error_first(
	void const *bitmap,
	short type)
{
	return (boolean)ae_bitmap_type_errors_first(&bitmap_type_errors, (uintptr_t)bitmap, type);
}

boolean ae_map_look_shader_hidden(
	void const *shader)
{
	unsigned int index;

	if (!shader)
		return FALSE;
	for (index = 0; index < NUMBEROF(hidden_shaders); index++)
	{
		if (hidden_shaders[index] == shader)
			return TRUE;
	}
	return FALSE;
}
