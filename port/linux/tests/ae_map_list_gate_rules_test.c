/* ae_map_list_gate_rules_test.c: the menus' map list gate (ae_map_list_gate_rules.h) */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_map_list_gate_rules.h"

static int failures;

#define CHECK(condition) \
	do { if (!(condition)) { printf("%s:%d: %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

static struct ae_map_list_gate gate;

/* a folder's times: modified, then created (a few at a time, for one call) */
static struct ae_map_list_gate_times const *T(unsigned long long modified, unsigned long modified_nanoseconds,
	unsigned long long created, unsigned long created_nanoseconds)
{
	static struct ae_map_list_gate_times times[4];
	static int next;
	struct ae_map_list_gate_times *t = &times[next++ % 4];

	t->modified_seconds = modified;
	t->modified_nanoseconds = modified_nanoseconds;
	t->created_seconds = created;
	t->created_nanoseconds = created_nanoseconds;
	return t;
}
static struct ae_map_list_missing missing;

int main(void)
{
	char name[80];
	int index;

	/* a making whose folders had settled: valid; each folder once; a folder unchanged is the same, a newer time or a
	folder made or gone is not */
	ae_map_list_gate_rules_begin(&gate, 1000);
	CHECK(!gate.valid);
	ae_map_list_gate_rules_note(&gate, "/data/maps", 1, T(900, 5, 0, 0));
	ae_map_list_gate_rules_note(&gate, "/data/maps", 1, T(900, 5, 0, 0));
	ae_map_list_gate_rules_note(&gate, "/data/maps_dev", 0, T(0, 0, 0, 0));
	CHECK(gate.count == 2);
	ae_map_list_gate_rules_end(&gate, 42, 5000);
	CHECK(gate.valid && gate.inputs == 42 && !gate.recording);
	CHECK(ae_map_list_gate_rules_folder_same(&gate.folders[0], 1, T(900, 5, 0, 0)));
	CHECK(!ae_map_list_gate_rules_folder_same(&gate.folders[0], 1, T(900, 6, 0, 0)));
	CHECK(!ae_map_list_gate_rules_folder_same(&gate.folders[0], 1, T(901, 5, 0, 0)));
	CHECK(!ae_map_list_gate_rules_folder_same(&gate.folders[0], 0, T(0, 0, 0, 0)));
	CHECK(ae_map_list_gate_rules_folder_same(&gate.folders[1], 0, T(0, 0, 0, 0)));
	CHECK(!ae_map_list_gate_rules_folder_same(&gate.folders[1], 1, T(950, 0, 0, 0)));
	/* (nothing noted outside a making) */
	ae_map_list_gate_rules_note(&gate, "/data/other", 1, T(1, 0, 0, 0));
	CHECK(gate.count == 2);

	/* the creation time counts too: a copy that kept the modification time still moves it */
	ae_map_list_gate_rules_begin(&gate, 1000);
	ae_map_list_gate_rules_note(&gate, "/data/maps", 1, T(900, 5, 910, 7));
	ae_map_list_gate_rules_end(&gate, 1, 5000);
	CHECK(gate.valid);
	CHECK(ae_map_list_gate_rules_folder_same(&gate.folders[0], 1, T(900, 5, 910, 7)));
	CHECK(!ae_map_list_gate_rules_folder_same(&gate.folders[0], 1, T(900, 5, 911, 7)));
	CHECK(!ae_map_list_gate_rules_folder_same(&gate.folders[0], 1, T(900, 5, 910, 8)));
	CHECK(!ae_map_list_gate_rules_folder_same(&gate.folders[0], 1, NULL));
	/* (created within 2 s of the start, modified long before: not trusted) */
	ae_map_list_gate_rules_begin(&gate, 1000);
	ae_map_list_gate_rules_note(&gate, "/data/maps", 1, T(500, 0, 999, 0));
	ae_map_list_gate_rules_end(&gate, 1, 5000);
	CHECK(!gate.valid);

	/* the age: young below the maximum (milliseconds since the making's end), across the clock's wrap; never during
	a making or before the first */
	ae_map_list_gate_rules_begin(&gate, 1000);
	CHECK(!ae_map_list_gate_rules_young(&gate, 5000, 10000));
	ae_map_list_gate_rules_note(&gate, "/data/maps", 1, T(900, 0, 900, 0));
	ae_map_list_gate_rules_end(&gate, 1, 5000);
	CHECK(ae_map_list_gate_rules_young(&gate, 5000, 10000));
	CHECK(ae_map_list_gate_rules_young(&gate, 14999, 10000));
	CHECK(!ae_map_list_gate_rules_young(&gate, 15000, 10000));
	CHECK(ae_map_list_gate_rules_young(&gate, 6999, AE_MAP_LIST_MISSING_UNTRUSTED_MILLISECONDS));
	CHECK(!ae_map_list_gate_rules_young(&gate, 7000, AE_MAP_LIST_MISSING_UNTRUSTED_MILLISECONDS));
	ae_map_list_gate_rules_begin(&gate, 1000);
	ae_map_list_gate_rules_end(&gate, 1, ULONG_MAX - 0xFFul);
	CHECK(ae_map_list_gate_rules_young(&gate, 0x100ul, 10000));
	CHECK(AE_MAP_LIST_GATE_MAXIMUM_AGE_MILLISECONDS == 10000);

	/* a folder modified within 2 s of the start: not trusted; 3 s before: trusted */
	ae_map_list_gate_rules_begin(&gate, 1000);
	ae_map_list_gate_rules_note(&gate, "/data/maps", 1, T(998, 0, 0, 0));
	ae_map_list_gate_rules_end(&gate, 1, 5000);
	CHECK(!gate.valid);
	ae_map_list_gate_rules_begin(&gate, 1000);
	ae_map_list_gate_rules_note(&gate, "/data/maps", 1, T(997, 999999999, 0, 0));
	ae_map_list_gate_rules_end(&gate, 1, 5000);
	CHECK(gate.valid);
	/* (a folder that did not open has no time to settle) */
	ae_map_list_gate_rules_begin(&gate, 1000);
	ae_map_list_gate_rules_note(&gate, "/data/none", 0, T(0, 0, 0, 0));
	ae_map_list_gate_rules_end(&gate, 1, 5000);
	CHECK(gate.valid);

	/* a folder whose time could not be read: untrusted; not outside a making */
	ae_map_list_gate_rules_begin(&gate, 1000);
	ae_map_list_gate_rules_note(&gate, "/data/maps", 1, T(10, 0, 0, 0));
	ae_map_list_gate_rules_untrusted(&gate);
	ae_map_list_gate_rules_end(&gate, 1, 5000);
	CHECK(!gate.valid);
	ae_map_list_gate_rules_begin(&gate, 1000);
	ae_map_list_gate_rules_note(&gate, "/data/maps", 1, T(10, 0, 0, 0));
	ae_map_list_gate_rules_end(&gate, 1, 5000);
	ae_map_list_gate_rules_untrusted(&gate);
	CHECK(gate.valid);

	/* too many folders, or a path too long: untrusted */
	ae_map_list_gate_rules_begin(&gate, 1000);
	for (index = 0; index <= AE_MAP_LIST_GATE_FOLDERS; index++)
	{
		snprintf(name, sizeof(name), "/data/maps/%d", index);
		ae_map_list_gate_rules_note(&gate, name, 1, T(10, 0, 0, 0));
	}
	ae_map_list_gate_rules_end(&gate, 1, 5000);
	CHECK(!gate.valid && gate.count == AE_MAP_LIST_GATE_FOLDERS);
	{
		char long_path[AE_MAP_LIST_GATE_PATH + 8];

		memset(long_path, 'a', sizeof(long_path) - 1);
		long_path[sizeof(long_path) - 1] = 0;
		ae_map_list_gate_rules_begin(&gate, 1000);
		ae_map_list_gate_rules_note(&gate, long_path, 1, T(10, 0, 0, 0));
		ae_map_list_gate_rules_end(&gate, 1, 5000);
		CHECK(!gate.valid && gate.count == 0);
	}

	/* the digest: text and number both count; the text's end too */
	CHECK(ae_map_list_gate_rules_digest(0, "a", 1) == ae_map_list_gate_rules_digest(0, "a", 1));
	CHECK(ae_map_list_gate_rules_digest(0, "a", 1) != ae_map_list_gate_rules_digest(0, "a", 2));
	CHECK(ae_map_list_gate_rules_digest(0, "a", 1) != ae_map_list_gate_rules_digest(0, "b", 1));
	CHECK(ae_map_list_gate_rules_digest(0, "a1", 0) != ae_map_list_gate_rules_digest(0, "a", 1));
	CHECK(ae_map_list_gate_rules_digest(0, NULL, 0) == ae_map_list_gate_rules_digest(0, "", 0));
	CHECK(ae_map_list_gate_rules_digest(1, "a", 1) != ae_map_list_gate_rules_digest(2, "a", 1));

	/* missing names: kept once each, the oldest let go past the last 64; empty, NULL and too long never kept */
	ae_map_list_missing_rules_clear(&missing);
	ae_map_list_missing_rules_add(&missing, "levels\\test\\a10\\a10");
	ae_map_list_missing_rules_add(&missing, "levels\\test\\a10\\a10");
	CHECK(missing.count == 1 && ae_map_list_missing_rules_has(&missing, "levels\\test\\a10\\a10"));
	CHECK(!ae_map_list_missing_rules_has(&missing, "a10@ce") && !ae_map_list_missing_rules_has(&missing, NULL));
	ae_map_list_missing_rules_add(&missing, "");
	ae_map_list_missing_rules_add(&missing, NULL);
	memset(name, 'x', sizeof(name) - 1);
	name[sizeof(name) - 1] = 0;
	ae_map_list_missing_rules_add(&missing, name);
	CHECK(missing.count == 1);
	for (index = 0; index < AE_MAP_LIST_MISSING_NAMES; index++)
	{
		snprintf(name, sizeof(name), "map%d", index);
		ae_map_list_missing_rules_add(&missing, name);
	}
	CHECK(missing.count == AE_MAP_LIST_MISSING_NAMES);
	CHECK(!ae_map_list_missing_rules_has(&missing, "levels\\test\\a10\\a10"));
	CHECK(ae_map_list_missing_rules_has(&missing, "map0") && ae_map_list_missing_rules_has(&missing, "map63"));
	ae_map_list_missing_rules_clear(&missing);
	CHECK(!ae_map_list_missing_rules_has(&missing, "map0"));

	if (failures)
	{
		printf("ae_map_list_gate_rules_test: %d failed\n", failures);
		return 1;
	}
	printf("ae_map_list_gate_rules_test: all passed\n");
	return 0;
}
