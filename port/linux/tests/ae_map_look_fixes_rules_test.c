/*
AE_MAP_LOOK_FIXES_RULES_TEST.C

AE's look fixes (port/linux/game/ae_map_look_fixes_rules.c): a fix's map by
name, the field changed only from the value the fix expects, never outside the
tag's bytes; the table's own entries; a portal fix by its map's file and hash,
made only on a BSP as it expects (cyberdyneb2's), once; and the bitmap type
errors said once for each bitmap and type until the reset.
*/

#include <stdio.h>
#include <string.h>

#include "../game/ae_map_look_fixes_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static short field(unsigned char const *data, unsigned int offset)
{
	short value;

	memcpy(&value, data + offset, sizeof(value));
	return value;
}

static void set_field(unsigned char *data, unsigned int offset, short value)
{
	memcpy(data + offset, &value, sizeof(value));
}

int main(void)
{
	struct ae_map_look_fix const fix = { "h2d_coagulation", 0x73636869UL, "a\\b", 0x2C, 0, 3, "test" };
	unsigned char data[0x6C];
	unsigned char before[0x6C];
	static struct ae_bitmap_type_errors errors;
	unsigned int index;
	char header_name[AE_MAP_LOOK_FIX_MAP_NAME_SIZE];

	/* (the map by its header's name, any case; no other) */
	CHECK(ae_map_look_fix_for_map(&fix, "h2d_coagulation"));
	CHECK(ae_map_look_fix_for_map(&fix, "H2D_Coagulation"));
	CHECK(!ae_map_look_fix_for_map(&fix, "h2d_coagulation2"));
	CHECK(!ae_map_look_fix_for_map(&fix, "h2d_coagulatio"));
	CHECK(!ae_map_look_fix_for_map(&fix, "h2_coagulation"));
	CHECK(!ae_map_look_fix_for_map(&fix, ""));
	CHECK(!ae_map_look_fix_for_map(&fix, NULL));
	CHECK(!ae_map_look_fix_for_map(NULL, "h2d_coagulation"));
	/* (a header's name of the whole 32 bytes, no terminator: read no further, matches no shorter fix) */
	memset(header_name, 'x', sizeof(header_name));
	CHECK(!ae_map_look_fix_for_map(&fix, header_name));
	{
		struct ae_map_look_fix const full = { "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 0, "", 0, 0, 1, "" };
		struct ae_map_look_fix const longer = { "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxy", 0, "", 0, 0, 1, "" };

		CHECK(ae_map_look_fix_for_map(&full, header_name));
		CHECK(!ae_map_look_fix_for_map(&longer, header_name));
	}

	/* (the field changed from the expected value only; the other bytes left) */
	memset(data, 0x5A, sizeof(data));
	set_field(data, 0x2C, 0);
	memcpy(before, data, sizeof(data));
	CHECK(ae_map_look_fix_apply(&fix, data, sizeof(data)) == AE_MAP_LOOK_FIX_APPLIED);
	CHECK(field(data, 0x2C) == 3);
	CHECK(!memcmp(data, before, 0x2C) && !memcmp(data + 0x2E, before + 0x2E, sizeof(data) - 0x2E));
	/* (again: fixed already, left) */
	CHECK(ae_map_look_fix_apply(&fix, data, sizeof(data)) == AE_MAP_LOOK_FIX_ALREADY);
	CHECK(field(data, 0x2C) == 3);
	/* (another value: another version of the map, left) */
	set_field(data, 0x2C, 1);
	CHECK(ae_map_look_fix_apply(&fix, data, sizeof(data)) == AE_MAP_LOOK_FIX_UNEXPECTED);
	CHECK(field(data, 0x2C) == 1);
	/* (outside the tag's bytes, or no bytes: nothing read) */
	CHECK(ae_map_look_fix_apply(&fix, data, 0x2D) == AE_MAP_LOOK_FIX_OUTSIDE);
	CHECK(ae_map_look_fix_apply(&fix, data, 0x2E) == AE_MAP_LOOK_FIX_UNEXPECTED);
	CHECK(ae_map_look_fix_apply(&fix, data, 1) == AE_MAP_LOOK_FIX_OUTSIDE);
	CHECK(ae_map_look_fix_apply(&fix, data, 0) == AE_MAP_LOOK_FIX_OUTSIDE);
	CHECK(ae_map_look_fix_apply(&fix, NULL, sizeof(data)) == AE_MAP_LOOK_FIX_OUTSIDE);
	CHECK(ae_map_look_fix_apply(NULL, data, sizeof(data)) == AE_MAP_LOOK_FIX_OUTSIDE);
	{
		struct ae_map_look_fix const far = { "m", 0, "t", 0xFFFFFFFFu, 0, 3, "" };

		CHECK(ae_map_look_fix_apply(&far, data, sizeof(data)) == AE_MAP_LOOK_FIX_OUTSIDE);
	}

	/* (the table: h2d_coagulation's teleporter sheet, alpha_blend to add, in a chicago shader's fixed part;
	every entry names its map and tag, changes something, and lies in a chicago shader's 0x6C bytes) */
	CHECK(ae_map_look_fix_count >= 1);
	{
		int coagulation = 0;

		for (index = 0; index < ae_map_look_fix_count; index++)
		{
			struct ae_map_look_fix const *entry = &ae_map_look_fixes[index];

			CHECK(entry->map_name && entry->map_name[0] && strlen(entry->map_name) <= AE_MAP_LOOK_FIX_MAP_NAME_SIZE);
			CHECK(entry->tag_name && entry->tag_name[0]);
			CHECK(entry->what && entry->what[0]);
			CHECK(entry->old_value != entry->new_value);
			CHECK(entry->offset + sizeof(short) <= 0x6C);
			if (!strcmp(entry->map_name, "h2d_coagulation") &&
				!strcmp(entry->tag_name, "scenery\\protected 24\\shaders\\base part1"))
			{
				coagulation = 1;
				CHECK(entry->group == 0x73636869UL); /* 'schi' */
				CHECK(entry->offset == 0x2C);
				CHECK(entry->old_value == 0 && entry->new_value == 3);
			}
		}
		CHECK(coagulation);
	}

	/* (a reference's fix: pointed at the new tag only from the expected one, with the target group; its other
	bytes, the reference's name and the tag's other bytes left) */
	{
		struct ae_map_look_retarget const retarget = { "h2d_salicreek", 0x65716970UL, "e\\q", 0x28, 0x6D6F6465UL,
			"old", "new", "test" };
		unsigned char object[0x17C];
		unsigned char kept[0x17C];
		uint32_t group = 0x6D6F6465UL;
		int32_t index_value = (int32_t)0xE55403E0;
		int32_t value;
		uint32_t name = 0x803BF778UL;

		CHECK(ae_map_look_retarget_for_map(&retarget, "h2d_salicreek"));
		CHECK(ae_map_look_retarget_for_map(&retarget, "H2D_SALICREEK"));
		CHECK(!ae_map_look_retarget_for_map(&retarget, "h2d_salicreek2"));
		CHECK(!ae_map_look_retarget_for_map(&retarget, NULL));
		CHECK(!ae_map_look_retarget_for_map(NULL, "h2d_salicreek"));

		memset(object, 0x5A, sizeof(object));
		memcpy(object + 0x28, &group, sizeof(group));
		memcpy(object + 0x2C, &name, sizeof(name));
		memcpy(object + 0x34, &index_value, sizeof(index_value));
		memcpy(kept, object, sizeof(object));
		/* (the expected or the new tag not the map's: left) */
		CHECK(ae_map_look_retarget_apply(&retarget, object, sizeof(object), -1, (int32_t)0xE68C0518) ==
			AE_MAP_LOOK_FIX_MISSING);
		CHECK(ae_map_look_retarget_apply(&retarget, object, sizeof(object), (int32_t)0xE55403E0, -1) ==
			AE_MAP_LOOK_FIX_MISSING);
		CHECK(!memcmp(object, kept, sizeof(object)));
		/* (to another tag than the expected one: another version of the map, left) */
		CHECK(ae_map_look_retarget_apply(&retarget, object, sizeof(object), (int32_t)0xE55B03E7, (int32_t)0xE68C0518) ==
			AE_MAP_LOOK_FIX_UNEXPECTED);
		CHECK(!memcmp(object, kept, sizeof(object)));
		/* (a reference of another group: left) */
		{
			unsigned char other[0x17C];
			uint32_t coll = 0x636F6C6CUL;

			memcpy(other, object, sizeof(other));
			memcpy(other + 0x28, &coll, sizeof(coll));
			CHECK(ae_map_look_retarget_apply(&retarget, other, sizeof(other), (int32_t)0xE55403E0,
				(int32_t)0xE68C0518) == AE_MAP_LOOK_FIX_UNEXPECTED);
			CHECK(!memcmp(other + 0x2C, object + 0x2C, 12));
		}
		/* (outside the tag: nothing read) */
		CHECK(ae_map_look_retarget_apply(&retarget, object, 0x37, (int32_t)0xE55403E0, (int32_t)0xE68C0518) ==
			AE_MAP_LOOK_FIX_OUTSIDE);
		CHECK(ae_map_look_retarget_apply(&retarget, NULL, sizeof(object), (int32_t)0xE55403E0, (int32_t)0xE68C0518) ==
			AE_MAP_LOOK_FIX_OUTSIDE);
		CHECK(ae_map_look_retarget_apply(NULL, object, sizeof(object), (int32_t)0xE55403E0, (int32_t)0xE68C0518) ==
			AE_MAP_LOOK_FIX_OUTSIDE);
		CHECK(!memcmp(object, kept, sizeof(object)));
		/* (applied: the index alone changes; again, fixed already) */
		CHECK(ae_map_look_retarget_apply(&retarget, object, 0x38, (int32_t)0xE55403E0, (int32_t)0xE68C0518) ==
			AE_MAP_LOOK_FIX_APPLIED);
		memcpy(&value, object + 0x34, sizeof(value));
		CHECK(value == (int32_t)0xE68C0518);
		CHECK(!memcmp(object, kept, 0x34) && !memcmp(object + 0x38, kept + 0x38, sizeof(object) - 0x38));
		CHECK(ae_map_look_retarget_apply(&retarget, object, sizeof(object), (int32_t)0xE55403E0, (int32_t)0xE68C0518) ==
			AE_MAP_LOOK_FIX_ALREADY);
		memcpy(&value, object + 0x34, sizeof(value));
		CHECK(value == (int32_t)0xE68C0518);
	}

	/* (the references' table: h2d_salicreek's overshield given the stock overshield's model and collision model;
	every entry names its map, tags and target group, changes something and lies in an object's 0x17C bytes) */
	{
		int model = 0;
		int collision = 0;

		for (index = 0; index < ae_map_look_retarget_count; index++)
		{
			struct ae_map_look_retarget const *entry = &ae_map_look_retargets[index];

			CHECK(entry->map_name && entry->map_name[0] && strlen(entry->map_name) <= AE_MAP_LOOK_FIX_MAP_NAME_SIZE);
			CHECK(entry->tag_name && entry->tag_name[0]);
			CHECK(entry->expected_name && entry->expected_name[0] && entry->new_name && entry->new_name[0]);
			CHECK(strcmp(entry->expected_name, entry->new_name));
			CHECK(entry->what && entry->what[0]);
			CHECK(entry->offset + 0x10 <= 0x17C);
			if (!strcmp(entry->map_name, "h2d_salicreek") && entry->group == 0x65716970UL && /* 'eqip' */
				!strcmp(entry->tag_name, "powerups\\over shield\\over shield") &&
				!strcmp(entry->expected_name, "powerups\\over shield\\over shield") &&
				!strcmp(entry->new_name, "powerups\\active camoflage\\active camoflage"))
			{
				if (entry->offset == 0x28 && entry->target_group == 0x6D6F6465UL) /* 'mode' */
					model = 1;
				if (entry->offset == 0x70 && entry->target_group == 0x636F6C6CUL) /* 'coll' */
					collision = 1;
			}
		}
		CHECK(model && collision);
	}

	/* (a portal fix: its map by file name, any case, and hash prefix; the BSP as it expects it, cyberdyneb2's) */
	{
		struct ae_map_portal_fix const *pit = NULL;
		short clusters[2], list[3], new_list[4];
		long new_count;

		for (index = 0; index < ae_map_portal_fix_count; index++)
		{
			if (ae_map_portal_fix_for_map(&ae_map_portal_fixes[index], "cyberdyneb2", 0x7ab68ae9UL))
				pit = &ae_map_portal_fixes[index];
		}
		CHECK(pit != NULL);
		if (pit)
		{
			CHECK(ae_map_portal_fix_for_map(pit, "CYBERDYNEB2", 0x7ab68ae9UL));
			CHECK(!ae_map_portal_fix_for_map(pit, "cyberdyneb2", 0x7ab68ae8UL));
			CHECK(!ae_map_portal_fix_for_map(pit, "cyberdyneb", 0x7ab68ae9UL));
			CHECK(!ae_map_portal_fix_for_map(pit, NULL, 0x7ab68ae9UL));
			CHECK(!ae_map_portal_fix_for_map(NULL, "cyberdyneb2", 0x7ab68ae9UL));
			/* (the map's own: portal 4 joins the alley to itself; the room lists only portal 1) */
			clusters[0] = 2;
			clusters[1] = 2;
			list[0] = 1;
			new_count = 0;
			CHECK(ae_map_portal_fix_apply(pit, 6, 12, clusters, list, 1, new_list, 4, &new_count) ==
				AE_MAP_LOOK_FIX_APPLIED);
			CHECK(clusters[0] == 4 && clusters[1] == 2);
			CHECK(new_count == 2 && new_list[0] == 1 && new_list[1] == 4);
			/* (fixed: again, nothing changes) */
			list[1] = 4;
			new_count = 0;
			CHECK(ae_map_portal_fix_apply(pit, 6, 12, clusters, list, 2, new_list, 4, &new_count) ==
				AE_MAP_LOOK_FIX_ALREADY);
			CHECK(new_count == 0 && clusters[0] == 4 && clusters[1] == 2);
			/* (another version of the map: other counts, clusters or list; nothing written) */
			clusters[0] = 2;
			clusters[1] = 2;
			CHECK(ae_map_portal_fix_apply(pit, 7, 12, clusters, list, 1, new_list, 4, &new_count) ==
				AE_MAP_LOOK_FIX_UNEXPECTED);
			CHECK(ae_map_portal_fix_apply(pit, 6, 13, clusters, list, 1, new_list, 4, &new_count) ==
				AE_MAP_LOOK_FIX_UNEXPECTED);
			clusters[1] = 3;
			CHECK(ae_map_portal_fix_apply(pit, 6, 12, clusters, list, 1, new_list, 4, &new_count) ==
				AE_MAP_LOOK_FIX_UNEXPECTED);
			CHECK(clusters[0] == 2 && clusters[1] == 3);
			clusters[1] = 2;
			list[0] = 0;
			CHECK(ae_map_portal_fix_apply(pit, 6, 12, clusters, list, 1, new_list, 4, &new_count) ==
				AE_MAP_LOOK_FIX_UNEXPECTED);
			list[0] = 1;
			list[1] = 0;
			CHECK(ae_map_portal_fix_apply(pit, 6, 12, clusters, list, 2, new_list, 4, &new_count) ==
				AE_MAP_LOOK_FIX_UNEXPECTED);
			/* (the portal listed though its clusters are the old ones: not as expected either) */
			list[1] = 4;
			CHECK(ae_map_portal_fix_apply(pit, 6, 12, clusters, list, 2, new_list, 4, &new_count) ==
				AE_MAP_LOOK_FIX_UNEXPECTED);
			CHECK(new_count == 0 && clusters[0] == 2 && clusters[1] == 2);
			/* (a BSP without the portal or cluster; no room for the new list; no list given) */
			CHECK(ae_map_portal_fix_apply(pit, 6, 4, clusters, list, 1, new_list, 4, &new_count) ==
				AE_MAP_LOOK_FIX_OUTSIDE);
			CHECK(ae_map_portal_fix_apply(pit, 4, 12, clusters, list, 1, new_list, 4, &new_count) ==
				AE_MAP_LOOK_FIX_OUTSIDE);
			CHECK(ae_map_portal_fix_apply(pit, 6, 12, clusters, list, 1, new_list, 1, &new_count) ==
				AE_MAP_LOOK_FIX_OUTSIDE);
			CHECK(ae_map_portal_fix_apply(pit, 6, 12, clusters, NULL, 1, new_list, 4, &new_count) ==
				AE_MAP_LOOK_FIX_OUTSIDE);
			CHECK(ae_map_portal_fix_apply(pit, 6, 12, NULL, list, 1, new_list, 4, &new_count) ==
				AE_MAP_LOOK_FIX_OUTSIDE);
			CHECK(new_count == 0 && clusters[0] == 2 && clusters[1] == 2);
		}
		/* (every fix's own: its old list fits, its portal is not in it, the clusters change) */
		for (index = 0; index < ae_map_portal_fix_count; index++)
		{
			struct ae_map_portal_fix const *entry = &ae_map_portal_fixes[index];
			int listed = 0, list_index;

			CHECK(entry->map_file && entry->what);
			CHECK(entry->old_list_count >= 0 && entry->old_list_count < AE_MAP_PORTAL_FIX_LIST_SIZE);
			for (list_index = 0; list_index < entry->old_list_count; list_index++)
				listed |= entry->old_list[list_index] == entry->portal_index;
			CHECK(!listed);
			CHECK(entry->old_clusters[0] != entry->new_clusters[0] || entry->old_clusters[1] != entry->new_clusters[1]);
			CHECK(entry->new_clusters[0] == entry->list_cluster || entry->new_clusters[1] == entry->list_cluster);
		}
	}

	/* (a bitmap and type said once until the reset; another type or bitmap is its own) */
	ae_bitmap_type_errors_reset(&errors);
	CHECK(ae_bitmap_type_errors_first(&errors, 0x1000, 0));
	CHECK(!ae_bitmap_type_errors_first(&errors, 0x1000, 0));
	CHECK(ae_bitmap_type_errors_first(&errors, 0x1000, 2));
	CHECK(ae_bitmap_type_errors_first(&errors, 0x2000, 0));
	CHECK(!ae_bitmap_type_errors_first(&errors, 0x2000, 0));
	CHECK(!ae_bitmap_type_errors_first(&errors, 0x1000, 2));
	ae_bitmap_type_errors_reset(&errors);
	CHECK(ae_bitmap_type_errors_first(&errors, 0x1000, 0));
	/* (full: none said, the kept ones still known) */
	ae_bitmap_type_errors_reset(&errors);
	for (index = 0; index < AE_BITMAP_TYPE_ERRORS_KEPT; index++)
		CHECK(ae_bitmap_type_errors_first(&errors, 0x10000 + index, 1));
	CHECK(!ae_bitmap_type_errors_first(&errors, 0x99999, 1));
	CHECK(!ae_bitmap_type_errors_first(&errors, 0x10000, 1));
	CHECK(errors.count == AE_BITMAP_TYPE_ERRORS_KEPT);
	CHECK(!ae_bitmap_type_errors_first(NULL, 0x1000, 0));

	if (!failures)
		printf("ae_map_look_fixes_rules: all passed\n");
	return failures ? 1 : 0;
}
