#include <stdio.h>
#include <string.h>
#include "ae_match_film_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define MB (1024LL * 1024LL)

int main(void)
{
	long long sizes[4];
	int out[4], count;
	char name[96];

	/* the setting: people, all (on too), off; empty and junk are off */
	CHECK(ae_match_film_rules_mode("people") == AE_MATCH_FILM_PEOPLE);
	CHECK(ae_match_film_rules_mode("all") == AE_MATCH_FILM_ALL);
	CHECK(ae_match_film_rules_mode("on") == AE_MATCH_FILM_ALL);
	CHECK(ae_match_film_rules_mode("off") == AE_MATCH_FILM_OFF);
	CHECK(ae_match_film_rules_mode("") == AE_MATCH_FILM_OFF);
	CHECK(ae_match_film_rules_mode("junk") == AE_MATCH_FILM_OFF);
	CHECK(ae_match_film_rules_mode("People") == AE_MATCH_FILM_OFF);
	CHECK(ae_match_film_rules_mode(NULL) == AE_MATCH_FILM_OFF);
	/* the names */
	CHECK(ae_match_film_name_is_film("film-20261108-101500-bloodgulch-ctf.txt"));
	CHECK(!ae_match_film_name_is_film("events-20261108-101500.jsonl"));
	CHECK(!ae_match_film_name_is_film("film-x.txt"));
	CHECK(!ae_match_film_name_is_film("film-20261108-101500-.txt"));
	CHECK(!ae_match_film_name_is_film("film-20261108-101500-a.txt.bak"));
	CHECK(!ae_match_film_name_is_film("film-2026110x-101500-a-b.txt"));
	CHECK(!ae_match_film_name_is_film(NULL));
	CHECK(ae_match_film_name_is_film("film-20261108-101500-a.txt"));
	CHECK(ae_match_film_file_name(name, sizeof(name), "20261108-101500", "blood gulch", "ctf", "4242") &&
		!strcmp(name, "film-20261108-101500-blood_gulch-ctf-4242.txt"));
	CHECK(ae_match_film_name_is_film(name));
	CHECK(ae_match_film_file_name(name, sizeof(name), "20261108-101500", "a", "b", "4242_2") &&
		!strcmp(name, "film-20261108-101500-a-b-4242_2.txt"));
	CHECK(!ae_match_film_file_name(name, 20, "20261108-101500", "bloodgulch", "ctf", "1"));
	/* the order: by the stamp, then the name */
	CHECK(ae_match_film_name_compare("film-20261108-101500-a-b-9.txt", "film-20261108-101501-a-b-1.txt") < 0);
	CHECK(ae_match_film_name_compare("film-20261109-000000-a-b-1.txt", "film-20261108-235959-z-b-1.txt") > 0);
	CHECK(ae_match_film_name_compare("film-20261108-101500-a-b-1.txt", "film-20261108-101500-a-b-2.txt") < 0);
	CHECK(ae_match_film_name_compare("film-20261108-101500-a-b-1.txt", "film-20261108-101500-a-b-1.txt") == 0);
	/* the budget has a floor */
	CHECK(ae_match_film_budget_mb(0) == 100 && ae_match_film_budget_mb(-5) == 100 && ae_match_film_budget_mb(99) == 100);
	CHECK(ae_match_film_budget_mb(100) == 100 && ae_match_film_budget_mb(2000) == 2000);
	/* the budget: the oldest go first, the current film never */
	sizes[0] = 500 * MB; sizes[1] = 400 * MB; sizes[2] = 300 * MB; sizes[3] = 200 * MB;
	count = ae_match_film_pick_removals(sizes, 4, 1000 * MB, 3, out);
	CHECK(count == 1 && out[0] == 0);
	count = ae_match_film_pick_removals(sizes, 4, 1500 * MB, 3, out);
	CHECK(count == 0);
	count = ae_match_film_pick_removals(sizes, 4, 600 * MB, 3, out);
	CHECK(count == 2 && out[0] == 0 && out[1] == 1);
	count = ae_match_film_pick_removals(sizes, 4, 100 * MB, 1, out);
	CHECK(count == 3 && out[0] == 0 && out[1] == 2 && out[2] == 3);
	sizes[0] = 5000 * MB;
	count = ae_match_film_pick_removals(sizes, 1, 1000 * MB, 0, out);
	CHECK(count == 0);
	count = ae_match_film_pick_removals(sizes, 1, 1000 * MB, -1, out);
	CHECK(count == 1 && out[0] == 0);
	if (!failures)
		printf("ae_match_film_rules_test: ok\n");
	return failures != 0;
}
