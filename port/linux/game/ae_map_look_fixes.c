/*
AE_MAP_LOOK_FIXES.C

AE's look fixes on a map's loaded tags (ae_map_look_fixes.h). Each fix in the
table (ae_map_look_fixes_rules.c) names its map, its tag by group and name,
and a 16-bit field with the value the map holds and the one it gets: the field
is changed only when it holds the first, so another version of the map (or one
its author fixed) is left as it is, and said so in the log. A fix changes how a
tag is drawn, nothing a game's machines must agree on: every machine makes it,
and the map's file and checksum are its own.
*/

#include "cseries.h"
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
	LOOK_FIX_TAG_BYTES = 0x6C
};

/* ---------- globals */

static struct ae_bitmap_type_errors bitmap_type_errors;

/* ---------- public code */

void ae_map_look_fixes_tags_loaded(
	char const *map_name)
{
	/* (the header's name: 32 bytes, its terminator the header's to keep) */
	char name[AE_MAP_LOOK_FIX_MAP_NAME_SIZE + 1];
	unsigned int index;

	ae_bitmap_type_errors_reset(&bitmap_type_errors);
	if (!map_name)
		return;
	snprintf(name, sizeof(name), "%.32s", map_name);
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
	return;
}

boolean ae_map_look_bitmap_type_error_first(
	void const *bitmap,
	short type)
{
	return (boolean)ae_bitmap_type_errors_first(&bitmap_type_errors, (uintptr_t)bitmap, type);
}
