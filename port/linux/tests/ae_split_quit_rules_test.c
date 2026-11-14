#include <stdio.h>
#include "ae_split_quit_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* two players, one quits: the screen is the other's */
	CHECK(ae_split_quit_leaves(1, 2, 0, 0));
	/* four players: one leaves, three remain */
	CHECK(ae_split_quit_leaves(1, 4, 0, 0));
	/* the last player keeps the list (a lone player's quit goes on to the end of the game) */
	CHECK(!ae_split_quit_leaves(1, 1, 0, 0));
	CHECK(!ae_split_quit_leaves(1, 0, 0, 0));
	/* not on the list (a bot, a remote player, one already off it) */
	CHECK(!ae_split_quit_leaves(0, 2, 0, 0));
	/* co-op's QUIT quits every controller; a film has one view */
	CHECK(!ae_split_quit_leaves(1, 2, 1, 0));
	CHECK(!ae_split_quit_leaves(1, 2, 0, 1));
	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ok\n");
	return failures ? 1 : 0;
}
