#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ae_social_names_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int same(char const *a, char const *b)
{
	while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b))
	{
		a++;
		b++;
	}
	return !*a && !*b;
}

/* every name fits a player name, is printable ASCII, and is there once (in any case); no skill level's name */
static void pool_tests(void)
{
	static char const *const levels[] = { "recruit", "easy", "normal", "heroic", "elite", "legendary", "demon" };
	int index, other, count = ae_social_names_count();

	CHECK(count >= 400);
	CHECK(!strcmp(ae_social_names_get(0), "Stain") && ae_social_names_get(count) == NULL && ae_social_names_get(-1) == NULL);
	for (index = 0; index < count; index++)
	{
		char const *name = ae_social_names_get(index), *c;
		size_t level;

		CHECK(name && strlen(name) >= 2 && strlen(name) <= AE_SOCIAL_NAME_LENGTH);
		for (c = name; *c; c++)
			CHECK(*c > ' ' && *c < 127);
		for (other = index + 1; other < count; other++)
			CHECK(!same(name, ae_social_names_get(other)));
		for (level = 0; level < sizeof levels / sizeof levels[0]; level++)
			CHECK(!same(name, levels[level]));
	}
}

/* a lobby's pick: all different, seeded, avoids the players' names */
static void pick_tests(void)
{
	int a[16], b[16], c[16], count, index, other, differs = 0;
	char const *avoid[2];

	count = ae_social_names_pick(7, 15, NULL, 0, a);
	CHECK(count == 15);
	for (index = 0; index < count; index++)
	{
		CHECK(a[index] >= 0 && a[index] < ae_social_names_count());
		for (other = index + 1; other < count; other++)
			CHECK(a[index] != a[other]);
	}
	CHECK(ae_social_names_pick(7, 15, NULL, 0, b) == 15 && !memcmp(a, b, sizeof(int) * 15));
	CHECK(ae_social_names_pick(8, 15, NULL, 0, c) == 15);
	for (index = 0; index < 15; index++)
		differs += a[index] != c[index];
	CHECK(differs > 5);
	/* (the players' names, in any case, are passed over) */
	avoid[0] = ae_social_names_get(a[0]);
	avoid[1] = "WEEPY";
	CHECK(ae_social_names_pick(7, 15, avoid, 2, b) == 15);
	for (index = 0; index < 15; index++)
		CHECK(b[index] != a[0] && !same(ae_social_names_get(b[index]), "weepy"));
	/* (more than the pool: the whole pool, once each) */
	{
		static int all[2048];
		int total = ae_social_names_pick(1, 2000, NULL, 0, all);

		CHECK(total == ae_social_names_count());
	}
	CHECK(ae_social_names_pick(0, 0, NULL, 0, a) == 0 && ae_social_names_pick(0, 3, NULL, 0, a) == 3);
}

/* the hash: stable numbers, case does not matter */
static void hash_tests(void)
{
	CHECK(ae_social_name_hash("") == 2166136261UL);
	CHECK(ae_social_name_hash("a") == 0xE40C292CUL);
	CHECK(ae_social_name_hash("Weepy") == ae_social_name_hash("wEEPY"));
	CHECK(ae_social_name_hash("Weepy") != ae_social_name_hash("Tart"));
	CHECK(ae_social_name_hash(NULL) == 2166136261UL);
}

int main(void)
{
	pool_tests();
	pick_tests();
	hash_tests();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_social_names_rules: ok (%d names)\n", ae_social_names_count());
	return failures != 0;
}
