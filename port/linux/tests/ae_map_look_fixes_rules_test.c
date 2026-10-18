/*
AE_MAP_LOOK_FIXES_RULES_TEST.C

AE's look fixes (port/linux/game/ae_map_look_fixes_rules.c): a fix's map by
name, the field changed only from the value the fix expects, never outside the
tag's bytes; the table's own entries; and the bitmap type errors said once for
each bitmap and type until the reset.
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
