#include <stdio.h>
#include "ae_dev_packs_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* a map's tag table as the packs grow it: each pack added keeps the table it found (its original) and leaves a bigger
one; let go, it puts its original back and frees its own. Every table a release puts back must be one still alive (the
map's, or one a pack still added made): never one a pack let go before freed. TRUE when the order is sound */
static int release_sound(const short *added, int count, const short *release)
{
	int original[16], made[16], freed[16], alive_table, step, slot;

	for (slot = 0; slot < 16; slot++)
		freed[slot] = 0;
	alive_table = 1000;
	for (step = 0; step < count; step++)
	{
		original[added[step]] = alive_table;
		made[added[step]] = alive_table + 100 + added[step];
		alive_table = made[added[step]];
	}
	for (step = 0; step < count; step++)
	{
		int table = original[release[step]], other;

		freed[release[step]] = 1;
		for (other = 0; other < count; other++)
		{
			if (freed[added[other]] && made[added[other]] == table)
				return 0;
		}
		alive_table = table;
	}
	return alive_table == 1000;
}

int main(void)
{
	unsigned int gametype, installed, loaded;
	/* (slots 1-5 the gametype's, 8 the vehicle pack, 9 the bomb pack, then DEV MODE's extras 6 and 7) */
	static const short added[] = { 1, 2, 3, 4, 5, 8, 9, 6, 7 };
	static const short reverse_slots[] = { 9, 8, 7, 6, 5, 4, 3, 2, 1 };
	short release[9];
	int index;

	/* (add, then let go: the exact reverse of the adding, which is sound; reverse slot order is not, with the extras
	after the vehicle and bomb packs) */
	CHECK(ae_dev_packs_release_order(added, 9, release) == 9);
	for (index = 0; index < 9; index++)
		CHECK(release[index] == added[8 - index]);
	CHECK(release_sound(added, 9, release));
	CHECK(!release_sound(added, 9, reverse_slots));
	/* (packs added in slot order, as without DEV MODE: both orders the same, both sound) */
	{
		static const short in_slot_order[] = { 1, 3, 8 };
		static const short back[] = { 8, 3, 1 };

		CHECK(ae_dev_packs_release_order(in_slot_order, 3, release) == 3);
		CHECK(release[0] == 8 && release[1] == 3 && release[2] == 1);
		CHECK(release_sound(in_slot_order, 3, back));
	}
	CHECK(ae_dev_packs_release_order(added, 0, release) == 0);
	CHECK(ae_dev_packs_release_order(NULL, 3, release) == 0);

	/* (the played mask is the loaded packs the gametype names, for every gametype mask and every set of loaded packs) */
	for (gametype = 0; gametype < 128; gametype++)
	{
		for (installed = 0; installed < 128; installed += 5)
		{
			loaded = gametype & installed;
			CHECK(ae_dev_packs_played(loaded, gametype) == loaded);
		}
	}
	CHECK(ae_dev_packs_played(0x1F | 0x60, 0x1F) == 0x1F);
	CHECK(ae_dev_packs_played(0x13 | 0x20, 0x20) == 0x20);
	if (failures)
		printf("%d failures\n", failures);
	return failures != 0;
}
