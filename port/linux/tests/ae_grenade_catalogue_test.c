#include <stdio.h>
#include <string.h>
#include "ae_grenade_catalogue.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) ((a) - (b) < 0.0001f && (b) - (a) < 0.0001f)

int main(void)
{
	struct ae_grenade_slot_map map;
	struct ae_grenade_entry const *emp = ae_grenade_catalogue_entry(_ae_grenade_emp);
	struct ae_grenade_entry const *smoke = ae_grenade_catalogue_entry(_ae_grenade_smoke);
	int slot;

	/* the rows: ids are permanent, EMP 0 and SMOKE 1; SMOKE reserved until its own tags ship */
	CHECK(emp && !strcmp(emp->name, "emp") && !strcmp(emp->display, "EMP GRENADE"));
	CHECK(smoke && !strcmp(smoke->name, "smoke"));
	CHECK(ae_grenade_catalogue_entry(-1) == NULL && ae_grenade_catalogue_entry(2) == NULL &&
		ae_grenade_catalogue_entry(AE_GRENADE_CATALOGUE_IDS) == NULL);
	CHECK(ae_grenade_catalogue_id("emp") == 0 && ae_grenade_catalogue_id("smoke") == 1 &&
		ae_grenade_catalogue_id("frag") == -1 && ae_grenade_catalogue_id(NULL) == -1);
	CHECK(emp->available && !smoke->available && ae_grenade_catalogue_available() == 1u);
	CHECK(emp->effects == (AE_GRENADE_FX_STALL | AE_GRENADE_FX_SCRAMBLE | AE_GRENADE_FX_POWERUP_SHORT));
	CHECK(smoke->effects == AE_GRENADE_FX_SMOKE_VOLUME);
	CHECK(emp->maximum == 2 && emp->spawn_default == 0 && emp->map_rule == _ae_grenade_map_near_vehicles &&
		emp->respawn_seconds == 30 && !emp->bots && emp->glyph == _ae_grenade_glyph_bolt);
	CHECK(emp->layer_version == 1 && smoke->layer_version == 2 && AE_GRENADE_LAYER_VERSION == 1);
	CHECK(emp->throwing_effect == NULL && emp->field_effect && !strcmp(emp->projectile, "emp\\emp_grenade"));

	/* the slot map: a Custom Edition map (own 2) with the EMP: frag, plasma, EMP in slot 2 */
	ae_grenade_slot_map_make(&map, 2, 0x01);
	CHECK(map.type_count == 3 && map.own_count == 2 && map.ids[0] == -1 && map.ids[1] == -1 && map.ids[2] == 0);
	CHECK(map.placed == 1u && map.refused == 0 && map.no_slot == 0 && ae_grenade_slot_of(&map, 0) == 2);
	CHECK(ae_grenade_slot_of(&map, 1) == -1 && ae_grenade_slot_of(&map, -1) == -1);
	/* mask 0: the stock pair alone (the layer stays off on such a map) */
	ae_grenade_slot_map_make(&map, 2, 0);
	CHECK(map.type_count == 2 && map.placed == 0 && map.refused == 0);
	for (slot = 0; slot < AE_GRENADE_SLOTS; slot++)
		CHECK(map.ids[slot] == -1);
	/* fewer than 2 own types read as 2 */
	ae_grenade_slot_map_make(&map, 0, 0x01);
	CHECK(map.type_count == 3 && map.own_count == 2 && map.ids[2] == 0);
	/* an MCC map with 4 own types: the EMP at slot 4 */
	ae_grenade_slot_map_make(&map, 4, 0x01);
	CHECK(map.type_count == 5 && map.ids[2] == -1 && map.ids[3] == -1 && map.ids[4] == 0 && ae_grenade_slot_of(&map, 0) == 4);
	/* an MCC map, no mask: its own slots as today */
	ae_grenade_slot_map_make(&map, 4, 0);
	CHECK(map.type_count == 4 && map.placed == 0);
	/* SMOKE named: refused (not available), the EMP placed; ids with no row refused too */
	ae_grenade_slot_map_make(&map, 2, 0x03);
	CHECK(map.type_count == 3 && map.placed == 1u && map.refused == 2u && map.no_slot == 0);
	ae_grenade_slot_map_make(&map, 2, 0xFC);
	CHECK(map.type_count == 2 && map.placed == 0 && map.refused == 0xFCu && map.no_slot == 0);
	/* bits past the 8 ids are not ids */
	ae_grenade_slot_map_make(&map, 2, 0x101);
	CHECK(map.type_count == 3 && map.refused == 0);
	/* a map whose own types fill the table: the EMP has no free slot */
	ae_grenade_slot_map_make(&map, 10, 0x01);
	CHECK(map.type_count == 10 && map.placed == 0 && map.refused == 1u && map.no_slot == 1u);
	ae_grenade_slot_map_make(&map, 9, 0x01);
	CHECK(map.type_count == 10 && map.ids[9] == 0 && map.placed == 1u);
	ae_grenade_slot_map_make(&map, 14, 0x01);
	CHECK(map.type_count == 10 && map.no_slot == 1u);
	/* every machine the same: the map is a function of (own count, mask) only */
	{
		struct ae_grenade_slot_map other;
		ae_grenade_slot_map_make(&map, 3, 0x01);
		ae_grenade_slot_map_make(&other, 3, 0x01);
		CHECK(!memcmp(&map, &other, sizeof(map)) && map.ids[3] == 0 && map.type_count == 4);
	}

	/* the join: the version a mask needs, and the refusal */
	/* (only ids with an available row count: SMOKE is reserved, 2-7 unknown) */
	CHECK(ae_grenade_layer_version_needed(0) == 0);
	CHECK(ae_grenade_layer_version_needed(0x01) == 1);
	CHECK(ae_grenade_layer_version_needed(0x03) == 1);
	CHECK(ae_grenade_layer_version_needed(0x02) == 0);
	CHECK(ae_grenade_layer_version_needed(0x80) == 0 && ae_grenade_layer_version_needed(0xFE) == 0);
	CHECK(ae_grenade_layer_version_needed(0x81) == 1);
	CHECK(ae_grenade_layer_version_told(1) == AE_GRENADE_LAYER_VERSION && ae_grenade_layer_version_told(0) == 0);
	/* the bits: layer, version, the pack hash's top nibble */
	CHECK(ae_grenade_join_bits(0, 0xAB) == 0 && ae_grenade_join_bits(1, 0xAB) == 0xA3 && ae_grenade_join_bits(2, 0x0F) == 0x05);
	CHECK(ae_grenade_join_bits(9, 0xF0) == 0xFF);
	CHECK(ae_grenade_join_bits_version(0xA3) == 1 && ae_grenade_join_bits_pack(0xA3) == 0xA && ae_grenade_join_bits_version(0) == 0 &&
		ae_grenade_join_bits_pack(0) == -1 && ae_grenade_join_bits_version(0xA2) == 0);
	{
		unsigned host = ae_grenade_join_bits(1, 0xED), same = host, other_pack = ae_grenade_join_bits(1, 0x1D),
			later = ae_grenade_join_bits(2, 0xED);

		/* mask 0, or a mask of ids no machine of this build plays: nobody refused */
		CHECK(!ae_grenade_join_refused(0, host, 0, 0) && !ae_grenade_join_refused(0x80, host, 0, 0) &&
			!ae_grenade_join_refused(0x02, host, 0, 0));
		/* the EMP: the host's own version and pack, nothing else */
		CHECK(!ae_grenade_join_refused(0x01, host, same, 0));
		CHECK(ae_grenade_join_refused(0x01, host, 0, 0));
		CHECK(ae_grenade_join_refused(0x01, host, other_pack, 0));
		CHECK(ae_grenade_join_refused(0x01, host, later, 0));
		CHECK(!ae_grenade_join_refused(0x01, host, 0, 1));
		/* (a machine without the pack tells nothing: refused) */
		CHECK(ae_grenade_join_refused(0x01, host, ae_grenade_join_bits(ae_grenade_layer_version_told(0), 0xED), 0));
		/* a host without the layer (its pack missing) plays frag and plasma: refuses nobody for it */
		CHECK(!ae_grenade_join_refused(0x01, 0, 0, 0) && !ae_grenade_join_refused(0x01, 0, same, 0));
		/* (an unknown bit beside the EMP does not raise what is needed) */
		CHECK(!ae_grenade_join_refused(0x81, host, same, 0));
	}

	/* the falloff: 1 to 2.0 wu, 0.40 at 3.5 wu, 0 past it */
	CHECK(NEAR(ae_grenade_falloff(&emp->numbers, 0.0f), 1.0f));
	CHECK(NEAR(ae_grenade_falloff(&emp->numbers, 2.0f), 1.0f));
	CHECK(NEAR(ae_grenade_falloff(&emp->numbers, 2.75f), 0.70f));
	CHECK(NEAR(ae_grenade_falloff(&emp->numbers, 3.5f), 0.40f));
	CHECK(ae_grenade_falloff(&emp->numbers, 3.51f) == 0.0f);
	CHECK(ae_grenade_falloff(NULL, 1.0f) == 0.0f);
	/* the effects' ticks: stall 4.0 s full (120), 1.6 s at the edge (48); scramble 5.0 / 2.0 s */
	CHECK(ae_grenade_effect_ticks(emp->numbers.stall_seconds, emp->numbers.stall_edge_seconds,
		emp->numbers.edge_fraction, 1.0f) == 120);
	CHECK(ae_grenade_effect_ticks(emp->numbers.stall_seconds, emp->numbers.stall_edge_seconds,
		emp->numbers.edge_fraction, 0.40f) == 48);
	CHECK(ae_grenade_effect_ticks(emp->numbers.stall_seconds, emp->numbers.stall_edge_seconds,
		emp->numbers.edge_fraction, 0.70f) == 84);
	CHECK(ae_grenade_effect_ticks(emp->numbers.scramble_seconds, emp->numbers.scramble_edge_seconds,
		emp->numbers.edge_fraction, 1.0f) == 150);
	CHECK(ae_grenade_effect_ticks(emp->numbers.scramble_seconds, emp->numbers.scramble_edge_seconds,
		emp->numbers.edge_fraction, 0.40f) == 60);
	CHECK(ae_grenade_effect_ticks(4.0f, 1.6f, 0.4f, 0.0f) == 0 && ae_grenade_effect_ticks(0.0f, 0.0f, 0.4f, 1.0f) == 0);

	/* the stall: re-hits refresh, never stack; immunity after one ends */
	CHECK(ae_grenade_stall_refresh(100, 0, 0, 120) == 220);
	CHECK(ae_grenade_stall_refresh(150, 220, 0, 120) == 270);
	CHECK(ae_grenade_stall_refresh(150, 220, 0, 48) == 220);
	CHECK(ae_grenade_stall_immunity_end(220, emp->numbers.immunity_seconds) == 280);
	CHECK(ae_grenade_stall_refresh(230, 220, 280, 120) == 220);
	CHECK(ae_grenade_stall_refresh(280, 220, 280, 120) == 400);
	CHECK(ae_grenade_stall_refresh(100, 50, 0, 0) == 50);

	CHECK(AE_GRENADE_STORED_IDS == 8);

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_grenade_catalogue: all passed\n");
	return failures != 0;
}
