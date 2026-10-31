/* ae_map_kind.c: a map's kind by its identity (AE's name for it in games and lists) */

#include <stdio.h>
#include <string.h>

#include "ae_map_kind.h"

static int failures;

#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char text[300], file[64];

	/* the Xbox's maps, Halo PC's families, packs */
	CHECK(ae_map_kind_of("bloodgulch") == AE_MAP_KIND_XBOX);
	CHECK(ae_map_kind_of("levels\\test\\bloodgulch\\bloodgulch") == AE_MAP_KIND_XBOX);
	CHECK(ae_map_kind_of("custom_maps\\hugeass") == AE_MAP_KIND_CE && ae_map_kind_of("bloodgulch@ce") == AE_MAP_KIND_CE &&
		ae_map_kind_of("x@pc") == AE_MAP_KIND_CE && ae_map_kind_of("x@md") == AE_MAP_KIND_CE);
	CHECK(ae_map_kind_of("bloodgulch@nhe") == AE_MAP_KIND_PACK);
	/* MCC's stock multiplayer maps, read in place: <name>@mcc, a stock name only */
	CHECK(ae_map_kind_of("bloodgulch@mcc") == AE_MAP_KIND_MCC_STOCK && ae_map_kind_of("Wizard@MCC") == AE_MAP_KIND_MCC_STOCK);
	CHECK(ae_map_kind_of("a10@mcc") == AE_MAP_KIND_INVALID && ae_map_kind_of("@mcc") == AE_MAP_KIND_INVALID &&
		ae_map_kind_of("../x@mcc") == AE_MAP_KIND_INVALID && ae_map_kind_of("x\\bloodgulch@mcc") == AE_MAP_KIND_INVALID);
	/* MCC Workshop maps: <name>@ws<item id>, and GulchCE's package folders (mcc_maps\...) */
	CHECK(ae_map_kind_of("pat_x@ws976730123") == AE_MAP_KIND_MCC_WORKSHOP);
	CHECK(ae_map_kind_of("mcc_maps\\pat_beavercreek") == AE_MAP_KIND_MCC_WORKSHOP &&
		ae_map_kind_of("MCC_MAPS/pack/x") == AE_MAP_KIND_MCC_WORKSHOP);
	CHECK(ae_map_kind_of("x@ws12a") == AE_MAP_KIND_INVALID && ae_map_kind_of("x@ws") == AE_MAP_KIND_INVALID &&
		ae_map_kind_of("@ws12") == AE_MAP_KIND_INVALID && ae_map_kind_of("x@ws123456789012345678901") == AE_MAP_KIND_INVALID &&
		ae_map_kind_of("../x@ws12") == AE_MAP_KIND_INVALID);
	/* a Workshop identity's map name and item id */
	{
		char name[64], id[24];

		CHECK(ae_map_kind_workshop_parts("pat_beavercreek@ws2899342285", name, sizeof(name), id, sizeof(id)) &&
			!strcmp(name, "pat_beavercreek") && !strcmp(id, "2899342285"));
		CHECK(ae_map_kind_workshop_parts("Some Map@WS12", name, sizeof(name), id, sizeof(id)) &&
			!strcmp(name, "Some Map") && !strcmp(id, "12"));
		CHECK(!ae_map_kind_workshop_parts("x@ws12a", name, sizeof(name), id, sizeof(id)) &&
			!ae_map_kind_workshop_parts("bloodgulch@mcc", name, sizeof(name), id, sizeof(id)) &&
			!ae_map_kind_workshop_parts("mcc_maps\\x", name, sizeof(name), id, sizeof(id)) &&
			!ae_map_kind_workshop_parts("x@ws12", name, 1, id, sizeof(id)));
	}
	/* an MCC identity of 64 characters or more: invalid (the loader's level names hold 63) */
	memset(text, 'a', 51);
	strcpy(text + 51, "@ws2899342285");
	CHECK(strlen(text) == 64 && ae_map_kind_of(text) == AE_MAP_KIND_INVALID);
	text[50] = 0;
	strcat(text, "@ws2899342285");
	CHECK(strlen(text) == 63 && ae_map_kind_of(text) == AE_MAP_KIND_MCC_WORKSHOP);
	/* no identity, or one longer than a game record's map name holds */
	CHECK(ae_map_kind_of(NULL) == AE_MAP_KIND_INVALID && ae_map_kind_of("") == AE_MAP_KIND_INVALID);
	memset(text, 'a', sizeof(text) - 1);
	text[sizeof(text) - 1] = 0;
	CHECK(ae_map_kind_of(text) == AE_MAP_KIND_INVALID);
	/* a stock MCC map's file in MCC's halo1/maps */
	CHECK(ae_map_kind_mcc_stock_file("bloodgulch@mcc", file, sizeof(file)) && !strcmp(file, "bloodgulch"));
	/* (MCC's file names are lower case: the canonical name, whatever the identity's case) */
	CHECK(ae_map_kind_mcc_stock_file("Wizard@mcc", file, sizeof(file)) && !strcmp(file, "wizard"));
	CHECK(ae_map_kind_mcc_stock_file("BloodGulch@MCC", file, sizeof(file)) && !strcmp(file, "bloodgulch"));
	CHECK(!ae_map_kind_mcc_stock_file("bloodgulch", file, sizeof(file)) && !ae_map_kind_mcc_stock_file("a10@mcc", file, sizeof(file)) &&
		!ae_map_kind_mcc_stock_file("bloodgulch@mcc", file, 4));

	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_map_kind: all passed\n");
	return 0;
}
