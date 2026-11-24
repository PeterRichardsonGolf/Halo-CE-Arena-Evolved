/* ae_join_diag_rules.c: the counters and the join diagnostics' texts */
#include <stdio.h>
#include <string.h>

#include "ae_join_diag_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_join_diag_machine machine;
	char text[200], small[16];
	int length;

	ae_join_diag_rules_reset(&machine);
	length = ae_join_diag_rules_heard_text(&machine, 5000, text, sizeof(text));
	CHECK(!strcmp(text, "datagrams 0 (none yet), stream messages 0 (none yet)"));
	CHECK(length == (int)strlen(text));

	ae_join_diag_rules_datagram(&machine, 1000, 5151);
	ae_join_diag_rules_datagram(&machine, 1100, 5152);
	ae_join_diag_rules_stream(&machine, 1050);
	ae_join_diag_rules_heard_text(&machine, 1500, text, sizeof(text));
	CHECK(!strcmp(text, "datagrams 2 (last 400 ms ago, from port 5152), stream messages 1 (last 450 ms ago)"));
	/* the clock wrapping: still the small difference */
	ae_join_diag_rules_stream(&machine, 0xFFFFFFF0UL);
	ae_join_diag_rules_heard_text(&machine, 0x10UL, text, sizeof(text));
	CHECK(strstr(text, "stream messages 2 (last 32 ms ago)") != NULL);

	/* a text bigger than its buffer is cut, ended, and its length reported */
	memset(small, 'x', sizeof(small));
	length = ae_join_diag_rules_heard_text(&machine, 1500, small, sizeof(small));
	CHECK(small[sizeof(small) - 1] == 0 && strlen(small) == sizeof(small) - 1 && length > (int)sizeof(small));
	CHECK(ae_join_diag_rules_heard_text(&machine, 1500, small, 0) == 0);

	/* a reset starts everything over */
	ae_join_diag_rules_reset(&machine);
	CHECK(!machine.datagrams && !machine.streams && !machine.datagram_port);

	/* sends outside the late start say nothing */
	CHECK(ae_join_diag_rules_late_send(&machine, 3, 12, 100, 1, text, sizeof(text)) == 0 && !text[0]);
	CHECK(machine.late_sends == 0);
	/* inside: each send a line, the failure with the bytes written before it */
	ae_join_diag_rules_late_begin(&machine);
	CHECK(ae_join_diag_rules_late_send(&machine, 3, 12, 100, 1, text, sizeof(text)) > 0);
	CHECK(!strcmp(text, "join-diag: late start of machine #3, send 1: packet type 12, 100 bytes: ok"));
	CHECK(ae_join_diag_rules_late_send(&machine, 3, 12, 250, 1, text, sizeof(text)) > 0);
	CHECK(ae_join_diag_rules_late_send(&machine, 3, 14, 40, 0, text, sizeof(text)) > 0);
	CHECK(!strcmp(text, "join-diag: late start of machine #3, send 3: packet type 14, 40 bytes: failed (350 bytes of the "
		"start in 2 sends were written before it)"));
	CHECK(machine.late_sends == 2 && machine.late_bytes == 350);
	/* the first send failing: nothing written before it */
	ae_join_diag_rules_late_begin(&machine);
	ae_join_diag_rules_late_send(&machine, 3, 12, 100, 0, text, sizeof(text));
	CHECK(strstr(text, "failed (0 bytes of the start in 0 sends") != NULL);
	/* a small buffer: cut, never over */
	ae_join_diag_rules_late_send(&machine, 3, 12, 100, 0, small, sizeof(small));
	CHECK(strlen(small) == sizeof(small) - 1);
	ae_join_diag_rules_late_end(&machine);
	CHECK(ae_join_diag_rules_late_send(&machine, 3, 12, 100, 1, text, sizeof(text)) == 0);

	/* the closed line once per join */
	CHECK(ae_join_diag_rules_closed_say(&machine));
	CHECK(!ae_join_diag_rules_closed_say(&machine));
	ae_join_diag_rules_reset(&machine);
	CHECK(ae_join_diag_rules_closed_say(&machine));

	if (failures)
		return 1;
	printf("ae_join_diag_rules: ok\n");
	return 0;
}
