#include <stdio.h>
#include <string.h>
#include "ae_grenade_wire_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_grenade_wire_entry entry, taken;
	unsigned char bytes[AE_GRENADE_WIRE_ENTRY_SIZE], bad[AE_GRENADE_WIRE_ENTRY_SIZE];
	static unsigned char const expected[AE_GRENADE_WIRE_ENTRY_SIZE] =
	{
		1, 3, 0x01, 0, 0x2A, 0x00, 0x34, 0xE1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x96, 0x00
	};

	/* a player's unit with 2 EMPs (slot 2) and a full scramble (150 ticks), on a Custom Edition map with the EMP */
	memset(&entry, 0, sizeof(entry));
	entry.kind = AE_GRENADE_WIRE_UNIT;
	entry.type_count = 3;
	entry.mask = 0x01;
	entry.handle = 0xE134002AUL;
	entry.counts[0] = 2;
	entry.scramble_ticks = 150;
	ae_grenade_wire_put(bytes, &entry);
	CHECK(!memcmp(bytes, expected, sizeof(bytes)));
	CHECK(ae_grenade_wire_take(bytes, &taken) && taken.kind == AE_GRENADE_WIRE_UNIT && taken.type_count == 3 &&
		taken.mask == 0x01 && taken.handle == 0xE134002AUL && taken.counts[0] == 2 && taken.counts[7] == 0 &&
		taken.scramble_ticks == 150 && taken.stall_ticks == 0);

	/* a vehicle's stall: 120 ticks; past 65535 is 65535 */
	memset(&entry, 0, sizeof(entry));
	entry.kind = AE_GRENADE_WIRE_VEHICLE;
	entry.type_count = 10;
	entry.mask = 0x3F;
	entry.handle = 0x0001FFFFUL;
	entry.stall_ticks = 120;
	ae_grenade_wire_put(bytes, &entry);
	CHECK(bytes[16] == 120 && bytes[17] == 0 && bytes[4] == 0xFF && bytes[5] == 0xFF && bytes[6] == 1 && bytes[7] == 0);
	CHECK(ae_grenade_wire_take(bytes, &taken) && taken.kind == AE_GRENADE_WIRE_VEHICLE && taken.stall_ticks == 120 &&
		taken.type_count == 10 && taken.mask == 0x3F);
	entry.stall_ticks = 100000;
	ae_grenade_wire_put(bytes, &entry);
	CHECK(ae_grenade_wire_take(bytes, &taken) && taken.stall_ticks == 65535);

	/* counts past 127 are 127 written; read: refused */
	memset(&entry, 0, sizeof(entry));
	entry.kind = AE_GRENADE_WIRE_UNIT;
	entry.type_count = 4;
	entry.counts[1] = 200;
	ae_grenade_wire_put(bytes, &entry);
	CHECK(bytes[9] == 127 && ae_grenade_wire_take(bytes, &taken) && taken.counts[1] == 127);

	/* bytes no host writes */
	memcpy(bad, bytes, sizeof(bad)); bad[9] = 128;
	CHECK(!ae_grenade_wire_take(bad, &taken));
	memcpy(bad, bytes, sizeof(bad)); bad[0] = 3;
	CHECK(!ae_grenade_wire_take(bad, &taken));
	memcpy(bad, bytes, sizeof(bad)); bad[0] = 0;
	CHECK(!ae_grenade_wire_take(bad, &taken));
	memcpy(bad, bytes, sizeof(bad)); bad[3] = 1;
	CHECK(!ae_grenade_wire_take(bad, &taken));
	memcpy(bad, bytes, sizeof(bad)); bad[1] = 1;
	CHECK(!ae_grenade_wire_take(bad, &taken));
	memcpy(bad, bytes, sizeof(bad)); bad[1] = 11;
	CHECK(!ae_grenade_wire_take(bad, &taken));
	/* (a unit with a stall; a vehicle with counts or a scramble) */
	memcpy(bad, bytes, sizeof(bad)); bad[16] = 1;
	CHECK(!ae_grenade_wire_take(bad, &taken));
	memcpy(bad, bytes, sizeof(bad)); bad[0] = AE_GRENADE_WIRE_VEHICLE;
	CHECK(!ae_grenade_wire_take(bad, &taken));
	memset(bad, 0, sizeof(bad)); bad[0] = AE_GRENADE_WIRE_VEHICLE; bad[1] = 3; bad[18] = 1;
	CHECK(!ae_grenade_wire_take(bad, &taken));
	bad[18] = 0;
	CHECK(ae_grenade_wire_take(bad, &taken));
	CHECK(!ae_grenade_wire_take(NULL, &taken) && !ae_grenade_wire_take(bad, NULL));
	ae_grenade_wire_put(bytes, NULL);
	CHECK(!bytes[0] && !ae_grenade_wire_take(bytes, &taken));

	/* the handle: 32 bits, sign-extended as the game's */
	CHECK(ae_grenade_wire_handle(0xE134002AUL) == (long)(int)0xE134002A && ae_grenade_wire_handle(0xE134002AUL) < 0);
	CHECK(ae_grenade_wire_handle(0x0001FFFFUL) == 0x0001FFFFL && ae_grenade_wire_handle(0xFFFFFFFFUL) == -1L);
	CHECK(ae_grenade_wire_handle(0x80000000UL) == -2147483647L - 1L);
	/* the slot map: the host's type count and mask, else the entry is not taken */
	memset(&entry, 0, sizeof(entry));
	entry.kind = AE_GRENADE_WIRE_UNIT;
	entry.type_count = 3;
	entry.mask = 0x01;
	CHECK(ae_grenade_wire_slot_map_matches(&entry, 3, 0x01));
	CHECK(!ae_grenade_wire_slot_map_matches(&entry, 5, 0x01) && !ae_grenade_wire_slot_map_matches(&entry, 3, 0x03));
	CHECK(!ae_grenade_wire_slot_map_matches(NULL, 3, 0x01));
	/* the round trip guard: another machine's unit always; one's own: a lower host count, no own throw, or a throw older
	than the round trip + 5 ticks */
	CHECK(ae_grenade_wire_count_taken(2, 1, 0, -1, 100, 6.0f));
	CHECK(ae_grenade_wire_count_taken(0, 1, 1, 98, 100, 6.0f));
	CHECK(!ae_grenade_wire_count_taken(2, 1, 1, 98, 100, 6.0f));
	CHECK(ae_grenade_wire_count_taken(2, 1, 1, -1, 100, 6.0f));
	CHECK(!ae_grenade_wire_count_taken(2, 1, 1, 89, 100, 6.0f) && ae_grenade_wire_count_taken(2, 1, 1, 88, 100, 6.0f));
	CHECK(!ae_grenade_wire_count_taken(2, 1, 1, 89, 100, 0.0f) && ae_grenade_wire_count_taken(2, 1, 1, 34, 100, 500.0f));
	CHECK(!ae_grenade_wire_count_taken(2, 1, 1, 90, 100, 5.0f) && ae_grenade_wire_count_taken(2, 1, 1, 89, 100, 5.0f));
	CHECK(!ae_grenade_wire_count_taken(2, 1, 1, 89, 100, 5.5f) && ae_grenade_wire_count_taken(2, 1, 1, 88, 100, 5.5f));
	/* an effect's ticks here: less half the round trip */
	CHECK(ae_grenade_wire_effect_ticks(120, 6.0f) == 117 && ae_grenade_wire_effect_ticks(120, 0.0f) == 117);
	CHECK(ae_grenade_wire_effect_ticks(2, 6.0f) == 0 && ae_grenade_wire_effect_ticks(0, 6.0f) == 0);
	CHECK(ae_grenade_wire_effect_ticks(120, 200.0f) == 90);

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_grenade_wire_rules: all passed\n");
	return failures != 0;
}
