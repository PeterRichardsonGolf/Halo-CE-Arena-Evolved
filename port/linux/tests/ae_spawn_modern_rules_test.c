#include <stdio.h>
#include "ae_spawn_modern_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	long window = 12 * 30;
	long table[4] = { 0, 0, 0, 0 };
	int index;

	/* recent(now, death_tick, window): a death in this game within the window */
	CHECK(ae_spawn_recent_death(1000, 900, window) == 1);
	CHECK(ae_spawn_recent_death(1000, 1000, window) == 1);
	CHECK(ae_spawn_recent_death(1000 + window - 1, 1000, window) == 1);
	CHECK(ae_spawn_recent_death(1000 + window, 1000, window) == 0);
	CHECK(ae_spawn_recent_death(1000, 0, window) == 0);
	/* (a death tick later than now is the previous game's: the game clock starts over at each game) */
	CHECK(ae_spawn_recent_death(40, 54000, window) == 0);
	CHECK(ae_spawn_recent_death(1, 2, window) == 0);

	/* the table at a new game: every slot's death forgotten */
	table[0] = 54000;
	table[3] = 120;
	ae_spawn_deaths_forget(table, 4);
	for (index = 0; index < 4; index++)
		CHECK(table[index] == 0);
	CHECK(ae_spawn_recent_death(130, table[3], window) == 0);

	if (failures)
		return 1;
	printf("ae_spawn_modern_rules_test: all passed\n");
	return 0;
}
