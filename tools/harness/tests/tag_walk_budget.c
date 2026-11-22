#include "harness.h"
/* tag_validate.c's validate_element: a walk visits each element of a map's tags once a pass, unless tags share their
bytes (a mod or disc map), where each sharer walks the shared elements again. Elements whose blocks all name one child
block walk that child once a parent element, at every level: the walk is refused once it has visited more elements
than 4 a byte of the region, and a walk of distinct elements is never refused */

#include "config.inc"
struct tag_schema_definition { long size; long fanout; struct tag_schema_definition const *child; };
static unsigned long walked;
static void tag_validate_refuse(struct tag_validation *validation, char const *format, ...) { validation->refused = TRUE; }
static void validate_element(struct tag_validation *validation, byte *base,
	struct tag_schema_definition const *definition, char const *field_name, long element_index);
/* (the values pass walks each child block's elements, every one naming the same child block) */
static void validate_fields(struct tag_validation *validation, byte *base, struct tag_schema_definition const *definition)
{
	long index;

	if (validation->pass != _pass_values || !definition->child)
		return;
	for (index = 0; index < definition->fanout && !validation->refused; index++)
	{
		/* (a walk this long has no bound: the test stops it) */
		if (++walked > 50000000UL)
		{
			validation->refused = TRUE;
			return;
		}
		validate_element(validation, base + 1, definition->child, "child", index);
	}
}
#include "under_test.inc"

static byte region[1000];

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	struct tag_validation validation;
	struct tag_schema_definition levels[8];
	int level;

	memset(&validation, 0, sizeof(validation));
	validation.region = region;
	validation.region_size = sizeof(region);
	validation.pass = _pass_values;
	CASE("shared-chain")
	{
		/* 7 levels of 20 elements, each naming the same child block: 20^7 element walks in 1000 bytes */
		for (level = 0; level < 8; level++)
		{
			levels[level].size = 4;
			levels[level].fanout = level < 7 ? 20 : 0;
			levels[level].child = level < 7 ? &levels[level + 1] : NULL;
		}
		validate_element(&validation, region, &levels[0], "root", 0);
		CHECK(validation.refused && walked < 4 * sizeof(region) + 65536 + 64, "walked %lu elements, %s", walked,
			validation.refused ? "refused" : "not refused");
		CHECK(walked < 50000000UL, "the walk had no bound");
		return 0;
	}
	CASE("distinct-elements")
	{
		/* 999 elements of 1 byte, both passes: never refused */
		levels[0].size = 1;
		levels[0].fanout = 999;
		levels[0].child = &levels[1];
		levels[1].size = 1;
		levels[1].fanout = 0;
		levels[1].child = NULL;
		validate_element(&validation, region, &levels[0], "root", 0);
		validation.pass = _pass_checks;
		validate_element(&validation, region, &levels[0], "root", 0);
		CHECK(!validation.refused && walked == 999, "a walk of distinct elements refused (%lu)", walked);
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
