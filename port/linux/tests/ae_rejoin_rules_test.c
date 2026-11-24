/* ae_rejoin_rules.c: when a lost connection is rejoined, the tries' times, the words, the host's replace rule */
#include <stdio.h>
#include <string.h>

#include "ae_rejoin_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char text[AE_REJOIN_TEXT];
	unsigned short name_a[8] = { 'T', 'e', 's', 't', 'e', 'r', 0, 0x55 };
	unsigned short name_b[8] = { 'T', 'e', 's', 't', 'e', 'r', 0, 0x77 };
	unsigned short name_c[8] = { 'T', 'e', 's', 't', 'e', 'R', 0, 0 };
	unsigned short empty[8] = { 0 };
	double seconds = 0.0;
	int silent = -1;
	unsigned long lost = 100000UL, wrap = 0xFFFFF000UL;

	/* (rejoined only with the setting on, the join armed, a lost connection, and no reason from the host) */
	CHECK(ae_rejoin_wanted(1, 1, 1, 0));
	CHECK(!ae_rejoin_wanted(0, 1, 1, 0));
	CHECK(!ae_rejoin_wanted(1, 0, 1, 0));
	CHECK(!ae_rejoin_wanted(1, 1, 0, 0));
	CHECK(!ae_rejoin_wanted(1, 1, 1, 1));

	/* (the first try 2.5 s after the drop, a later one 2 s after the last failed) */
	CHECK(ae_rejoin_next_try(lost, 0, 0) == lost + 2500UL);
	CHECK(ae_rejoin_next_try(lost, 1, lost + 9000UL) == lost + 11000UL);
	CHECK(!ae_rejoin_try_due(lost + 2499UL, lost, lost + 2500UL));
	CHECK(ae_rejoin_try_due(lost + 2500UL, lost, lost + 2500UL));
	/* (never past the window) */
	CHECK(!ae_rejoin_try_due(lost + 30000UL, lost, lost + 2500UL));
	/* (a clock that wraps between the drop and the try) */
	CHECK(ae_rejoin_try_due(wrap + 3000UL, wrap, ae_rejoin_next_try(wrap, 0, 0)));
	CHECK(!ae_rejoin_try_due(wrap + 1000UL, wrap, ae_rejoin_next_try(wrap, 0, 0)));

	/* (given up: 3 tries done and none going on, or 30 s gone) */
	CHECK(!ae_rejoin_give_up(lost + 5000UL, lost, 2, 0));
	CHECK(!ae_rejoin_give_up(lost + 5000UL, lost, 3, 1));
	CHECK(ae_rejoin_give_up(lost + 5000UL, lost, 3, 0));
	CHECK(ae_rejoin_give_up(lost + 30000UL, lost, 1, 1));
	CHECK(ae_rejoin_give_up(wrap + 30000UL, wrap, 0, 1));
	CHECK(!ae_rejoin_give_up(wrap + 29999UL, wrap, 0, 1));

	/* (the words: the server's or the host's, where, the try of 3) */
	ae_rejoin_body(text, (int)sizeof(text), 1, "play.example.org", 2);
	CHECK(!strcmp(text, "The connection to the server was lost. Rejoining play.example.org (try 2 of 3)."));
	ae_rejoin_body(text, (int)sizeof(text), 0, "", 1);
	CHECK(!strcmp(text, "The connection to the host was lost. Rejoining the game (try 1 of 3)."));
	ae_rejoin_body(text, (int)sizeof(text), 0, NULL, 9);
	CHECK(strstr(text, "(try 3 of 3)") != NULL);
	ae_rejoin_body(text, 12, 1, "x", 1);
	CHECK(strlen(text) == 11);

	/* (the host replaces the old machine: the same name, silent 2 s or more, the same machine) */
	CHECK(ae_rejoin_replaces(1, "abcd", "abcd", 0, 0, 2000UL));
	CHECK(!ae_rejoin_replaces(1, "abcd", "abcd", 0, 0, 1999UL));
	CHECK(!ae_rejoin_replaces(0, "abcd", "abcd", 0, 0, 9000UL));
	/* (both told a hardware id: another machine's is never replaced, whatever the address) */
	CHECK(!ae_rejoin_replaces(1, "abcd", "abce", 7, 7, 9000UL));
	/* (a hardware id missing on either side: the address decides, and must be known) */
	CHECK(ae_rejoin_replaces(1, "", "abcd", 7, 7, 9000UL));
	CHECK(ae_rejoin_replaces(1, NULL, NULL, 7, 7, 9000UL));
	CHECK(!ae_rejoin_replaces(1, "", "", 7, 8, 9000UL));
	CHECK(!ae_rejoin_replaces(1, "", "", 0, 0, 9000UL));

	/* (machine names: up to their ends, what follows the end (the join's marks) not compared) */
	CHECK(ae_rejoin_same_name(name_a, name_b, 8));
	CHECK(!ae_rejoin_same_name(name_a, name_c, 8));
	CHECK(!ae_rejoin_same_name(empty, empty, 8));
	CHECK(ae_rejoin_same_name(name_a, name_c, 5));
	CHECK(!ae_rejoin_same_name(NULL, name_a, 8));

	/* (the test drop's text) */
	CHECK(ae_rejoin_test_drop_parse("30:reset", &seconds, &silent) && seconds == 30.0 && silent == 0);
	CHECK(ae_rejoin_test_drop_parse("12.5:silent", &seconds, &silent) && seconds == 12.5 && silent == 1);
	CHECK(!ae_rejoin_test_drop_parse("", &seconds, &silent));
	CHECK(!ae_rejoin_test_drop_parse("30", &seconds, &silent));
	CHECK(!ae_rejoin_test_drop_parse("30:close", &seconds, &silent));
	CHECK(!ae_rejoin_test_drop_parse("0:reset", &seconds, &silent));
	CHECK(!ae_rejoin_test_drop_parse("-3:reset", &seconds, &silent));
	CHECK(!ae_rejoin_test_drop_parse("x:reset", &seconds, &silent));
	CHECK(!ae_rejoin_test_drop_parse(NULL, &seconds, &silent));

	printf("%s (%d failures)\n", failures ? "FAILED" : "ok", failures);
	return failures != 0;
}
