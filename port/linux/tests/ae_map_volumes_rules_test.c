/* ae_map_volumes_rules_test.c: MAP VOLUMES' names (ae_map_volumes_rules.h) */

#include <stdio.h>
#include <string.h>

#include "ae_map_volumes_rules.h"

static int failures, checks;
#define CHECK(x) do { checks++; if (!(x)) { failures++; printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x); } } while (0)

int main(void)
{
	char exit_name[8];

	/* (Dredwerkz: Hell kills, TPA lifts to TPB; TPB itself is only an exit) */
	CHECK(ae_map_volume_kind("Hell") == AE_MAP_VOLUME_KILL);
	CHECK(ae_map_volume_kind("KILL_ZONE") == AE_MAP_VOLUME_KILL);
	CHECK(ae_map_volume_kind("lava") == AE_MAP_VOLUME_KILL);
	CHECK(ae_map_volume_kind("TPA") == AE_MAP_VOLUME_LIFT);
	CHECK(ae_map_volume_kind("tpc") == AE_MAP_VOLUME_LIFT);
	CHECK(ae_map_volume_kind("TPB") == AE_MAP_VOLUME_NONE);
	CHECK(ae_map_volume_kind("TPAB") == AE_MAP_VOLUME_NONE);
	CHECK(ae_map_volume_kind("TP") == AE_MAP_VOLUME_NONE);
	CHECK(ae_map_volume_kind("hellgate") == AE_MAP_VOLUME_NONE);
	CHECK(ae_map_volume_kind("kill_marines") == AE_MAP_VOLUME_NONE);
	CHECK(ae_map_volume_kind("") == AE_MAP_VOLUME_NONE);
	CHECK(ae_map_volume_kind(NULL) == AE_MAP_VOLUME_NONE);
	CHECK(ae_map_volume_lift_exit("TPA", exit_name, sizeof(exit_name)) && !strcmp(exit_name, "TPB"));
	CHECK(ae_map_volume_lift_exit("tpc", exit_name, sizeof(exit_name)) && !strcmp(exit_name, "tpD"));
	CHECK(!ae_map_volume_lift_exit("Hell", exit_name, sizeof(exit_name)));
	CHECK(!ae_map_volume_lift_exit("TPA", exit_name, 3));
	CHECK(ae_map_volume_same_name("TPB", "tpb") && !ae_map_volume_same_name("TPB", "TPC") && !ae_map_volume_same_name(NULL, "a"));
	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
