#include <stdio.h>

#include "ae_master_backoff_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* (0.8.2: one unreachable master held up the server's publishing to every master; AE #5 fell off the AE master for
	two minutes) a master that answered is asked on the usual round; one that failed n times in a row waits 60 s, then
	twice that each time, 15 minutes at most */
	CHECK(ae_master_backoff_seconds(0) == 0);
	CHECK(ae_master_backoff_seconds(1) == 60);
	CHECK(ae_master_backoff_seconds(2) == 120);
	CHECK(ae_master_backoff_seconds(3) == 240);
	CHECK(ae_master_backoff_seconds(5) == 900 && ae_master_backoff_seconds(40) == 900);
	CHECK(ae_master_backoff_seconds(-1) == 0);
	/* the order of a round: the masters that answered last time first, so a failing one holds up only the failing */
	CHECK(ae_master_publish_pass(0, 0) && !ae_master_publish_pass(3, 0));
	CHECK(!ae_master_publish_pass(0, 1) && ae_master_publish_pass(3, 1));
	/* (review 0.8.2) what counts as a failed answer: no answer, a 5xx even with a JSON error, an unparsed status, the
	refusals that last; a listing, a 429 (its own wait) and a clock error do not */
	CHECK(ae_master_answer_failed(1, 0, ""));
	CHECK(ae_master_answer_failed(0, 500, "internal"));
	CHECK(ae_master_answer_failed(0, 503, "netlist_unavailable"));
	CHECK(ae_master_answer_failed(0, 502, "status_502"));
	CHECK(ae_master_answer_failed(0, 403, "publisher_pending"));
	CHECK(ae_master_answer_failed(0, 403, "unknown_key"));
	CHECK(ae_master_answer_failed(0, 403, "publisher_suspended"));
	CHECK(!ae_master_answer_failed(0, 200, ""));
	CHECK(!ae_master_answer_failed(0, 429, "rate_limited"));
	CHECK(!ae_master_answer_failed(0, 400, "clock"));
	CHECK(ae_master_answer_failed(0, 200, NULL) == 0);
	if (failures)
	{
		printf("ae_master_backoff_rules_test: %d failure(s)\n", failures);
		return 1;
	}
	printf("ae_master_backoff_rules_test: ok\n");
	return 0;
}
