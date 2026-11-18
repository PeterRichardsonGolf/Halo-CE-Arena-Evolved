#include <stdio.h>
#include <string.h>
#include "ae_cache_config_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	static struct ae_cache_set_config const table[] = { { 1, 1, 0x01000007 }, { 2, 1, 0x01000009 }, { 1, 9, 0x0100000A }, { 0, 0, 0 } };
	struct ae_weapon_selection selection;

	ae_weapon_selection_clear(&selection);
	/* the shipped table is empty: no set names a config */
	CHECK(ae_cache_config_pick(&selection, 1, AE_CACHE_CONFIG_MAP_COPY, ae_cache_set_configs) == 0);
	CHECK(ae_cache_config_pick(&selection, 1, AE_CACHE_CONFIG_PRIMARY, ae_cache_set_configs) == 0);
	/* NULL selection or weapon 0: config 0 */
	CHECK(ae_cache_config_pick(NULL, 1, 0, table) == 0);
	CHECK(ae_cache_config_pick(&selection, 0, 0, table) == 0);

	/* rule 3: no START, casual set, a table without casual rows */
	CHECK(ae_cache_config_pick(&selection, 1, AE_CACHE_CONFIG_MAP_COPY, table) == 0);
	/* rule 2: the set's row, by set and weapon, on map copies and loadouts */
	selection.ae_weapons = 1;
	selection.set_id = 1;
	CHECK(ae_cache_config_pick(&selection, 1, AE_CACHE_CONFIG_MAP_COPY, table) == 0x01000007);
	CHECK(ae_cache_config_pick(&selection, 1, AE_CACHE_CONFIG_SECONDARY, table) == 0x01000007);
	CHECK(ae_cache_config_pick(&selection, 9, AE_CACHE_CONFIG_MAP_COPY, table) == 0x0100000A);
	CHECK(ae_cache_config_pick(&selection, 2, AE_CACHE_CONFIG_MAP_COPY, table) == 0);
	selection.set_id = 2;
	CHECK(ae_cache_config_pick(&selection, 1, AE_CACHE_CONFIG_MAP_COPY, table) == 0x01000009);
	CHECK(ae_cache_config_pick(&selection, 9, AE_CACHE_CONFIG_MAP_COPY, table) == 0);

	/* rule 1: a START pair that names the weapon with a config wins on its own loadout slot only */
	selection.set_id = 1;
	selection.start[0].weapon = 1;
	selection.start[0].config = 0x01000003;
	CHECK(ae_cache_config_pick(&selection, 1, AE_CACHE_CONFIG_PRIMARY, table) == 0x01000003);
	CHECK(ae_cache_config_pick(&selection, 1, AE_CACHE_CONFIG_SECONDARY, table) == 0x01000007);
	CHECK(ae_cache_config_pick(&selection, 1, AE_CACHE_CONFIG_MAP_COPY, table) == 0x01000007);
	CHECK(ae_cache_config_pick(&selection, 9, AE_CACHE_CONFIG_PRIMARY, table) == 0x0100000A);
	/* a START of the weapon with config 0 leaves the set's config in force */
	selection.start[0].config = 0;
	CHECK(ae_cache_config_pick(&selection, 1, AE_CACHE_CONFIG_PRIMARY, table) == 0x01000007);
	/* a bad loadout slot is a map copy */
	selection.start[1].weapon = 1;
	selection.start[1].config = 0x01000004;
	CHECK(ae_cache_config_pick(&selection, 1, 5, table) == 0x01000007);
	CHECK(ae_cache_config_pick(&selection, 1, AE_CACHE_CONFIG_SECONDARY, table) == 0x01000004);
	if (failures)
		return 1;
	printf("ae_cache_config_rules_test: ok\n");
	return 0;
}
