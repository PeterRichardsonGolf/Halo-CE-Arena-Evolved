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
	printf("ae_weapon_rules: %u fake triggers, %d failed\n", index, failures);
	return failures ? 1 : 0;
}
