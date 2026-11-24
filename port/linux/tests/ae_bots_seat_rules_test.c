#include <stdio.h>
#include <string.h>

#include "ae_bots_seat_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	static struct ae_seat_table table;
	int shooting = -9, movement = -9;

	/* (no pair: the settings) */
	ae_seat_clear(&table);
	CHECK(!ae_seat_levels(&table, 0, &shooting, &movement) && shooting == -9 && ae_seat_count(&table) == 0);
	/* (RECRUIT shooting, NORMAL movement: levels 0 and 2) */
	CHECK(ae_seat_set(&table, 3, 1, 3) && ae_seat_levels(&table, 3, &shooting, &movement) && shooting == 0 &&
		movement == 2);
	CHECK(ae_seat_set(&table, AE_SEAT_MOST - 1, 7, 7) && ae_seat_count(&table) == 2);
	/* (out of range: nothing changes) */
	CHECK(!ae_seat_set(&table, 3, 0, 3) && !ae_seat_set(&table, 3, 8, 3) && !ae_seat_set(&table, 3, 2, 0));
	CHECK(!ae_seat_set(&table, -1, 2, 2) && !ae_seat_set(&table, AE_SEAT_MOST, 2, 2));
	CHECK(ae_seat_levels(&table, 3, &shooting, &movement) && shooting == 0 && movement == 2);
	CHECK(!ae_seat_levels(&table, -1, &shooting, &movement) && !ae_seat_levels(&table, AE_SEAT_MOST, &shooting, &movement));
	/* (one seat back to the settings; all back) */
	ae_seat_unset(&table, 3);
	CHECK(!ae_seat_levels(&table, 3, &shooting, &movement) && ae_seat_count(&table) == 1);
	ae_seat_unset(&table, 999);
	ae_seat_clear(&table);
	CHECK(ae_seat_count(&table) == 0);
	/* (names and numbers) */
	CHECK(ae_seat_step_of_name("recruit") == 1 && ae_seat_step_of_name("Legendary") == 6 && ae_seat_step_of_name("DEMON") == 7);
	CHECK(ae_seat_step_of_name("4") == 4 && !ae_seat_step_of_name("8") && !ae_seat_step_of_name("0"));
	CHECK(!ae_seat_step_of_name("legend") && !ae_seat_step_of_name("") && !ae_seat_step_of_name(NULL) &&
		!ae_seat_step_of_name("12"));
	CHECK(!strcmp(ae_seat_step_name(5), "ELITE") && !strcmp(ae_seat_level_name(0), "RECRUIT") &&
		!strcmp(ae_seat_step_name(0), "?") && !strcmp(ae_seat_level_name(7), "?"));
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_bots_seat_rules: ok\n");
	return failures != 0;
}
