/* ae_range_rules_test.c: the firing range applies to the map asked for, alone, and never in a network game */

#include <stdio.h>

#include "ae_range_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char const *map = "levels\\test\\bloodgulch\\bloodgulch";

	CHECK(ae_range_rules_same_path(map, "levels\\test\\bloodgulch\\bloodgulch"));
	CHECK(ae_range_rules_same_path(map, "LEVELS/test/BloodGulch/bloodgulch"));
	CHECK(!ae_range_rules_same_path(map, "bloodgulch"));
	CHECK(!ae_range_rules_same_path(map, "levels\\test\\bloodgulch\\bloodgulch2"));
	CHECK(!ae_range_rules_same_path(map, "mods\\x\\levels\\test\\bloodgulch\\bloodgulch"));
	CHECK(!ae_range_rules_same_path(map, "") && !ae_range_rules_same_path(map, NULL) && !ae_range_rules_same_path(NULL, map));
	/* requested, the path, alone: the range */
	CHECK(ae_range_rules_applies(1, 1, 0, 0, 0));
	/* not asked for, another path, or any network game: not */
	CHECK(!ae_range_rules_applies(0, 1, 0, 0, 0));
	CHECK(!ae_range_rules_applies(1, 0, 0, 0, 0));
	CHECK(!ae_range_rules_applies(1, 1, 1, 0, 0));
	CHECK(!ae_range_rules_applies(1, 1, 0, 1, 0));
	CHECK(!ae_range_rules_applies(1, 1, 0, 0, 1));
	if (!failures)
		printf("ae_range_rules_test: all passed\n");
	return failures ? 1 : 0;
}
