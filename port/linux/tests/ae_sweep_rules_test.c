/* the 2026-10-16 sweep's pure rules: the lobby's SHUFFLE deal (ae_team_shuffle_rules.c) and the map list's WARM-UP
group (ae_map_warm_up.c) */
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "ae_team_shuffle.h"
#include "ae_map_warm_up.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* every count is dealt half and half (red the odd one), each player once, and seeds differ in who goes where */
static void test_deal(void)
{
	signed char teams[AE_TEAM_SHUFFLE_MAXIMUM];
	signed char first[16];
	int count, index, red, differs = 0, extra_red = 0, extra_blue = 0;
	unsigned long seed;

	for (count = 1; count <= 20; count++)
	{
		for (seed = 0; seed < 50; seed++)
		{
			memset(teams, 9, sizeof(teams));
			ae_team_shuffle_deal(count, seed * 2654435761ul, teams);
			red = 0;
			for (index = 0; index < count; index++)
			{
				CHECK(teams[index] == 0 || teams[index] == 1);
				red += teams[index] == 0;
			}
			/* (half each; an odd one out on either side, at random: one person alone is red or blue) */
			CHECK(red == count / 2 || red == (count + 1) / 2);
			if (count % 2)
			{
				extra_red += red > count / 2;
				extra_blue += red == count / 2;
			}
			CHECK(teams[count] == 9);
		}
	}
	CHECK(extra_red > 100 && extra_blue > 100);
	ae_team_shuffle_deal(16, 1, first);
	for (seed = 2; seed < 20; seed++)
	{
		ae_team_shuffle_deal(16, seed * 7919ul, teams);
		differs += memcmp(first, teams, 16) != 0;
	}
	CHECK(differs >= 15);
	/* (none, and more than the most, are safe) */
	ae_team_shuffle_deal(0, 5, teams);
	ae_team_shuffle_deal(-3, 5, teams);
}

struct row
{
	char name[16];
	int order;
};

static void test_warm_up(void)
{
	struct row rows[6] = { { "cex_abyss", 0 }, { "octagon_b", 1 }, { "decidia", 2 }, { "OCTAGON", 3 },
		{ "zz", 4 }, { "octagonal", 5 } };
	const wchar_t *text;

	CHECK(ae_map_warm_up("octagon"));
	CHECK(ae_map_warm_up("Octagon_B"));
	CHECK(ae_map_warm_up("maps\\\\octagon.map"));
	CHECK(ae_map_warm_up("mods/x/maps/octagon_b.map"));
	CHECK(!ae_map_warm_up("octagonal"));
	CHECK(!ae_map_warm_up("octagon_c"));
	CHECK(!ae_map_warm_up(""));
	CHECK(!ae_map_warm_up(NULL));
	text = ae_map_warm_up_name("octagon", L"Octagon", 1);
	CHECK(!wcscmp(text, L"Octagon [WARM-UP]"));
	text = ae_map_warm_up_name("octagon", L"Octagon", 0);
	CHECK(!wcscmp(text, L"Octagon\r\n[WARM-UP]"));
	CHECK(!wcscmp(ae_map_warm_up_name("decidia", L"Decidia", 1), L"Decidia"));
	CHECK(!wcscmp(ae_map_warm_up_description("decidia", L"Community map"), L"Community map"));
	CHECK(!wcscmp(ae_map_warm_up_description("octagon_b", L"Community map"), L"A warm-up map\r\nfor Slayer"));
	ae_map_warm_up_first(rows, 6, sizeof(rows[0]), 0);
	CHECK(rows[0].order == 1 && rows[1].order == 3);
	CHECK(rows[2].order == 0 && rows[3].order == 2 && rows[4].order == 4 && rows[5].order == 5);
}

int main(void)
{
	test_deal();
	test_warm_up();
	printf("%d checks, %d failures\n", checks, failures);
	return failures != 0;
}
