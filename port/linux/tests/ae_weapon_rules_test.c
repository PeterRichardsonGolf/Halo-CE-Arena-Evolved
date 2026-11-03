#include <stdio.h>
#include "ae_weapon_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* a fake trigger: its magazine index (the tag's), and the weapon's magazine count (0-2) */
struct fake_trigger
{
	short magazine_index;
	short magazine_count;
	short expected;
};

int main(void)
{
	static struct fake_trigger const triggers[] =
	{
		/* (no magazine: a map's charging weapon without one; never the slot before the magazines) */
		{ -1, 0, -1 }, { -1, 1, -1 }, { -1, 2, -1 },
		/* (the weapon's own magazines) */
		{ 0, 1, 0 }, { 0, 2, 0 }, { 1, 2, 1 },
		/* (a map's index past the weapon's magazines, or below none) */
		{ 1, 1, -1 }, { 2, 2, -1 }, { 0, 0, -1 }, { -2, 2, -1 }, { 32767, 2, -1 }, { -32768, 2, -1 },
	};
	unsigned int index;

	for (index = 0; index < sizeof(triggers) / sizeof(triggers[0]); index++)
	{
		short got = ae_weapon_charged_magazine_index(triggers[index].magazine_index, triggers[index].magazine_count);

		if (got != triggers[index].expected)
			printf("trigger %u: magazine %d of %d read %d, not %d\n", index, triggers[index].magazine_index,
				triggers[index].magazine_count, got, triggers[index].expected);
		CHECK(got == triggers[index].expected);
		/* (whatever it reads is a magazine the weapon has, or none: never out of the array) */
		CHECK(got == -1 || (got >= 0 && got < triggers[index].magazine_count));
	}
	/* INFINITE AMMO and BOTTOMLESS CLIP (seed revision 16): bits 5 and 6 of pack_loadout, the pack's bits 0-4 and
	bit 7 kept and never read as them */
	{
		unsigned int byte;

		for (byte = 0; byte < 256; byte++)
		{
			unsigned char both = ae_weapon_ammo_byte_set((unsigned char)byte, 1, 1);
			unsigned char none = ae_weapon_ammo_byte_set((unsigned char)byte, 0, 0);

			CHECK(ae_weapon_infinite_ammo((unsigned char)byte) == ((byte & 0x20u) != 0));
			CHECK(ae_weapon_bottomless_clip((unsigned char)byte) == ((byte & 0x40u) != 0));
			CHECK((both & 0x9Fu) == (byte & 0x9Fu) && ae_weapon_infinite_ammo(both) && ae_weapon_bottomless_clip(both));
			CHECK((none & 0x9Fu) == (byte & 0x9Fu) && !ae_weapon_infinite_ammo(none) && !ae_weapon_bottomless_clip(none));
			CHECK(ae_weapon_ammo_byte_set((unsigned char)byte, 1, 0) == ((byte & 0x9Fu) | 0x20u));
			CHECK(ae_weapon_ammo_byte_set((unsigned char)byte, 0, 1) == ((byte & 0x9Fu) | 0x40u));
		}
		/* (the AE BR set's byte, the pack in the primary slot: neither) */
		CHECK(!ae_weapon_infinite_ammo(0x01) && !ae_weapon_bottomless_clip(0x01 | 0x10));
	}
	/* MAP GRENADES (seed revision 18): bit 7 of pack_loadout, set = OFF; the other bits kept and never read as it */
	{
		unsigned int byte;

		for (byte = 0; byte < 256; byte++)
		{
			unsigned char off = ae_map_grenades_byte_set((unsigned char)byte, 1);
			unsigned char on = ae_map_grenades_byte_set((unsigned char)byte, 0);

			CHECK(ae_map_grenades_off((unsigned char)byte) == ((byte & 0x80u) != 0));
			CHECK((off & 0x7Fu) == (byte & 0x7Fu) && ae_map_grenades_off(off));
			CHECK((on & 0x7Fu) == (byte & 0x7Fu) && !ae_map_grenades_off(on));
			/* (the ammo bits and the pack's neither read it nor lose it) */
			CHECK(ae_map_grenades_off(ae_weapon_ammo_byte_set(off, 1, 1)) && !ae_map_grenades_off(ae_weapon_ammo_byte_set(on, 1, 1)));
			CHECK(ae_weapon_infinite_ammo(off) == ae_weapon_infinite_ammo(on));
			/* (only a grenade pickup is skipped, and only with it off: weapons, powerups, health, no equipment never) */
			CHECK(ae_map_item_skipped((unsigned char)byte, 1, 1) == ae_map_grenades_off((unsigned char)byte));
			CHECK(!ae_map_item_skipped((unsigned char)byte, 1, 0));
			CHECK(!ae_map_item_skipped((unsigned char)byte, 0, 0));
			CHECK(!ae_map_item_skipped((unsigned char)byte, 0, 1));
		}
		/* (every gametype from before has 0 there: MAP GRENADES on; the AE BR set's byte too) */
		CHECK(!ae_map_grenades_off(0x00) && !ae_map_grenades_off(0x01) && !ae_map_grenades_off(0x7F));
		CHECK(ae_map_item_skipped(0x80, 1, 1) && !ae_map_item_skipped(0x7F, 1, 1));
	}
	printf("ae_weapon_rules: %u fake triggers, %d failed\n", index, failures);
	return failures ? 1 : 0;
}
