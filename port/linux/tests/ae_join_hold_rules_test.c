/* ae_join_hold_rules.c: the lobby keeps a playerless machine whose map transfer is moving */
#include <stdio.h>

#include "ae_join_hold_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* open, serving, sent lately: kept */
	CHECK(ae_join_hold_transfer_alive(1, 0, 19922944, 0));
	CHECK(ae_join_hold_transfer_alive(1, 0, 1, AE_JOIN_HOLD_MOVED_MS));
	/* nothing sent for a while, nothing sent yet, closed, or the host's own download: not kept */
	CHECK(!ae_join_hold_transfer_alive(1, 0, 19922944, AE_JOIN_HOLD_MOVED_MS + 1));
	CHECK(!ae_join_hold_transfer_alive(1, 0, 0, 0));
	CHECK(!ae_join_hold_transfer_alive(1, 0, 100, -1));
	CHECK(!ae_join_hold_transfer_alive(0, 0, 100, 0));
	CHECK(!ae_join_hold_transfer_alive(1, 1, 100, 0));
	/* the event: once per join */
	CHECK(ae_join_hold_say_event(0, 5000));
	CHECK(!ae_join_hold_say_event(5000, 5000));
	CHECK(ae_join_hold_say_event(5000, 9000));
	if (failures)
		return 1;
	printf("ae_join_hold_rules: ok\n");
	return 0;
}
