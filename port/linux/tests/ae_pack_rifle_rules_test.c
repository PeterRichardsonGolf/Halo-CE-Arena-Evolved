#include <stdio.h>
#include <string.h>
#include "ae_pack_rifle_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* a fake reference: its tag index (the game's struct tag_reference holds more, the redirect reads only this) */
struct fake_reference
{
	char const *kind;
	long index;
};

int main(void)
{
	/* which weapon tags are a map's own battle rifle: the name holds "battle_rifle" or "battle rifle", any case */
	{
		static struct
		{
			char const *name;
			int rifle;
		} const samples[] =
		{
			{ "halo2\\weapons\\human\\battle rifle\\battle rifle", 1 },
			{ "dw2\\weapons\\battle_rifle\\battle_rifle", 1 },
			{ "weapons\\Battle Rifle\\BATTLE RIFLE", 1 },
			{ "ae\\packs\\battle_rifle\\battle_rifle", 1 },
			{ "weapons\\br\\br", 0 },
			{ "weapons\\assault rifle\\assault rifle", 0 },
			{ "weapons\\battle\\rifle", 0 },
			{ "weapons\\battlerifle\\battlerifle", 0 },
			{ "", 0 },
		};
		unsigned int index;

		for (index = 0; index < sizeof(samples) / sizeof(samples[0]); index++)
			CHECK(ae_pack_rifle_name_is_battle_rifle(samples[index].name) == samples[index].rifle);
		CHECK(!ae_pack_rifle_name_is_battle_rifle(NULL));
	}

	/* a name longer than the rule reads: only its first AE_PACK_RIFLE_NAME_MAXIMUM characters are asked */
	{
		char name[400];

		memset(name, 'x', sizeof(name));
		memcpy(name + 300, "battle_rifle", 12);
		name[sizeof(name) - 1] = 0;
		CHECK(!ae_pack_rifle_name_is_battle_rifle(name));
		memcpy(name + 10, "battle rifle", 12);
		CHECK(ae_pack_rifle_name_is_battle_rifle(name));
	}

	/* the redirect on a small fake table: two own battle rifles (a map with a variant), references to them, to
	other weapons and none; the pack's index is past the map's */
	{
		long const own[] = { 0x00120034, 0x00130035 };
		long const pack_index = 0x0400051A;
		struct fake_reference references[] =
		{
			{ "scnr weapon_palette.reference", 0x00120034 },
			{ "scnr starting_profile.primary_weapon", 0x00120034 },
			{ "scnr starting_profile.secondary_weapon", 0x00100030 },
			{ "itmc item_permutation.item", 0x00130035 },
			{ "itmc item_permutation.item", -1 },
			{ "matg weapon_list.weapon", 0x00120035 },
		};
		long const expected[] = { pack_index, pack_index, 0x00100030, pack_index, -1, 0x00120035 };
		int const expected_own[] = { 0, 0, -1, 1, -1, -1 };
		unsigned int index;
		long changed = 0;

		for (index = 0; index < sizeof(references) / sizeof(references[0]); index++)
		{
			int got = ae_pack_rifle_redirect(&references[index].index, own, 2, pack_index);

			CHECK(got == expected_own[index]);
			CHECK(references[index].index == expected[index]);
			changed += got >= 0;
		}
		CHECK(changed == 3);
		/* (the same index with another salt is another tag: 0x00120035 above stays) */
		CHECK(ae_pack_rifle_own_of(0x00140034, own, 2) == -1);
		/* (a second pass changes nothing: the pack's index is not the map's) */
		for (index = 0; index < sizeof(references) / sizeof(references[0]); index++)
			CHECK(ae_pack_rifle_redirect(&references[index].index, own, 2, pack_index) == -1);
		/* (no own battle rifle, no pack index, no reference: nothing) */
		{
			long value = 0x00120034;

			CHECK(ae_pack_rifle_redirect(&value, own, 0, pack_index) == -1 && value == 0x00120034);
			CHECK(ae_pack_rifle_redirect(&value, own, 2, -1) == -1 && value == 0x00120034);
			CHECK(ae_pack_rifle_redirect(NULL, own, 2, pack_index) == -1);
		}
	}

	/* the log's kinds: one per referencing group, structure, field and own battle rifle; past the room, the last
	counts the rest */
	{
		static char const scenario_palette[] = "scenario_object_palette_entry", reference[] = "reference",
			permutation[] = "item_permutation", item[] = "item";
		struct ae_pack_rifle_kind kinds[AE_PACK_RIFLE_KINDS_MAXIMUM];
		int count = 0, index;
		long total = 0;

		memset(kinds, 0, sizeof(kinds));
		count = ae_pack_rifle_kind_add(kinds, count, 0x73636E72, scenario_palette, reference, 0, "scenario");
		count = ae_pack_rifle_kind_add(kinds, count, 0x69746D63, permutation, item, 0, "itmc a");
		count = ae_pack_rifle_kind_add(kinds, count, 0x69746D63, permutation, item, 0, "itmc a");
		count = ae_pack_rifle_kind_add(kinds, count, 0x69746D63, permutation, item, 1, "itmc b");
		CHECK(count == 3);
		CHECK(kinds[0].count == 1 && kinds[1].count == 2 && kinds[2].count == 1 && kinds[2].own == 1);
		CHECK(!strcmp(kinds[1].first, "itmc a") && !strcmp(kinds[2].first, "itmc b"));
		for (index = 0; index < 40; index++)
			count = ae_pack_rifle_kind_add(kinds, count, (unsigned long)(0x100 + index), permutation, item, 0, "itmc a");
		CHECK(count == AE_PACK_RIFLE_KINDS_MAXIMUM);
		for (index = 0; index < count; index++)
			total += kinds[index].count;
		CHECK(total == 44);
	}

	if (failures)
		return 1;
	printf("ae_pack_rifle_rules: all passed\n");
	return 0;
}
