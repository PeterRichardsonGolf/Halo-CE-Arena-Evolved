#include <stdio.h>
#include <string.h>

#include "ae_dev_ttk_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_ttk_table table;

/* a kill's time runs from the killer's first damage to the victim, however many hits came after it */
static void time_tests(void)
{
	struct ae_ttk_result result;

	ae_ttk_clear(&table);
	CHECK(ae_ttk_hit(&table, 1, 2, 77, 100, 0));
	CHECK(ae_ttk_hit(&table, 1, 2, 77, 130, 1));
	CHECK(ae_ttk_hit(&table, 1, 2, 77, 160, 0));
	CHECK(ae_ttk_kill(&table, 1, 2, 77, 190, &result));
	CHECK(result.ticks == 90 && result.hits == 3 && result.head == 1);
	/* (the row is cleared by the kill: the victim's next life starts over) */
	CHECK(!ae_ttk_kill(&table, 1, 2, 77, 200, &result) && result.hits == 0);
	CHECK(ae_ttk_hit(&table, 1, 2, 78, 400, 0) && ae_ttk_kill(&table, 1, 2, 78, 410, &result) && result.ticks == 10 &&
		result.hits == 1 && result.head == 0);
}

/* the killer's own damage counts: a hit by another player first does not start the killer's clock */
static void killer_tests(void)
{
	struct ae_ttk_result result;

	ae_ttk_clear(&table);
	CHECK(ae_ttk_hit(&table, 3, 2, 5, 50, 0));
	CHECK(ae_ttk_hit(&table, 1, 2, 5, 120, 0));
	CHECK(ae_ttk_kill(&table, 1, 2, 5, 150, &result) && result.ticks == 30 && result.hits == 1);
	/* (a kill by a player who never hurt the victim: none, and the row goes) */
	CHECK(ae_ttk_hit(&table, 3, 2, 6, 10, 0) && !ae_ttk_kill(&table, 1, 2, 6, 40, &result));
	CHECK(!ae_ttk_kill(&table, 3, 2, 6, 40, &result));
	/* (a death with no killer clears the row) */
	CHECK(ae_ttk_hit(&table, 3, 2, 8, 10, 0) && !ae_ttk_kill(&table, -1, 2, 8, 40, &result));
	CHECK(ae_ttk_hit(&table, 3, 2, 8, 100, 0) && ae_ttk_kill(&table, 3, 2, 8, 130, &result) && result.ticks == 30);
}

/* a row counted for another life starts over (a death with no damage event: the kill command's) */
static void life_tests(void)
{
	struct ae_ttk_result result;

	ae_ttk_clear(&table);
	CHECK(ae_ttk_hit(&table, 1, 2, 10, 100, 0));
	CHECK(ae_ttk_hit(&table, 1, 2, 11, 900, 1));
	CHECK(ae_ttk_kill(&table, 1, 2, 11, 930, &result) && result.ticks == 30 && result.hits == 1 && result.head == 1);
	/* (a kill read for a life the row was not counted in) */
	CHECK(ae_ttk_hit(&table, 1, 2, 12, 5, 0) && !ae_ttk_kill(&table, 1, 2, 13, 50, &result));
	ae_ttk_clear_victim(&table, 2);
	CHECK(!ae_ttk_kill(&table, 1, 2, 12, 50, &result));
}

/* slots out of range, oneself, and the format */
static void edge_tests(void)
{
	struct ae_ttk_result result = { 150, 5, 2 };
	char text[120];

	ae_ttk_clear(&table);
	CHECK(!ae_ttk_hit(&table, 2, 2, 1, 1, 0) && !ae_ttk_hit(&table, -1, 2, 1, 1, 0) && !ae_ttk_hit(&table, 1, -1, 1, 1, 0) &&
		!ae_ttk_hit(&table, AE_TTK_PLAYERS, 2, 1, 1, 0) && !ae_ttk_hit(&table, 1, AE_TTK_PLAYERS, 1, 1, 0));
	CHECK(!ae_ttk_kill(&table, 2, 2, 1, 5, &result) && !ae_ttk_kill(&table, 1, AE_TTK_PLAYERS, 1, 5, &result));
	result.ticks = 150;
	result.hits = 5;
	result.head = 2;
	ae_ttk_format(text, sizeof text, "Johnson", "Hall", "ar_modern", &result);
	CHECK(!strcmp(text, "ttk: Johnson -> Hall ar_modern 5.00s, 5 hits (2 head)"));
	result.ticks = 31;
	ae_ttk_format(text, sizeof text, "a", "b", "c", &result);
	CHECK(!strcmp(text, "ttk: a -> b c 1.03s, 5 hits (2 head)"));
}

/* the short line of the killer's HUD message list */
static void short_format_tests(void)
{
	char text[96];
	struct ae_ttk_result result = { 24, 3, 3 };

	ae_ttk_short_format(text, sizeof text, "pistol", &result);
	CHECK(!strcmp(text, "TTK 0.80s - 3 hits (3 head) - pistol"));
	result.hits = 1;
	result.head = 0;
	result.ticks = 45;
	ae_ttk_short_format(text, sizeof text, "battle_rifle_with_a_long_name", &result);
	CHECK(!strcmp(text, "TTK 1.50s - 1 hit - battle_rifle_wit"));
	ae_ttk_short_format(text, 12, "x", &result);
	CHECK(strlen(text) == 11);
	ae_ttk_short_format(text, sizeof text, NULL, &result);
	CHECK(strstr(text, "- ?"));
}

int main(void)
{
	time_tests();
	killer_tests();
	life_tests();
	edge_tests();
	short_format_tests();
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_dev_ttk_rules: ok\n");
	return failures != 0;
}
