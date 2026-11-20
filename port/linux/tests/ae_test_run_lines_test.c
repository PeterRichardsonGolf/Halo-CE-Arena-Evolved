#include <stdio.h>

#include "ae_test_run_lines.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_test_run_lines lines;
static short line;

static enum ae_test_run_lines_action step(int settled, int answered, int ok, unsigned long now)
{
	return ae_test_run_lines_step(&lines, settled, answered, ok, now, &line);
}

/* three lines go in order, each only when the one before was accepted */
static void order_tests(void)
{
	ae_test_run_lines_start(&lines, 3, 1, 1000);
	CHECK(ae_test_run_lines_pending(&lines));
	CHECK(step(1, 0, 0, 1000) == AE_TEST_RUN_LINES_SEND && line == 0);
	/* (no answer yet: nothing else is sent, however often it is asked) */
	CHECK(step(1, 0, 0, 1010) == AE_TEST_RUN_LINES_WAIT);
	CHECK(step(1, 0, 0, 1020) == AE_TEST_RUN_LINES_WAIT && line == 0);
	/* (accepted: the next line at once) */
	CHECK(step(1, 1, 1, 1033) == AE_TEST_RUN_LINES_SEND && line == 1);
	CHECK(step(1, 0, 0, 1040) == AE_TEST_RUN_LINES_WAIT);
	CHECK(step(1, 1, 1, 1066) == AE_TEST_RUN_LINES_SEND && line == 2);
	CHECK(step(1, 1, 1, 1099) == AE_TEST_RUN_LINES_DONE && line == 3);
	CHECK(!ae_test_run_lines_pending(&lines));
	CHECK(step(1, 0, 0, 5000) == AE_TEST_RUN_LINES_DONE);
	/* (a line answered in the same call it was sent in, as a command that needs no tick is: the caller asks again) */
	ae_test_run_lines_start(&lines, 2, 0, 0);
	CHECK(step(0, 0, 0, 0) == AE_TEST_RUN_LINES_SEND && line == 0);
	CHECK(step(0, 1, 1, 0) == AE_TEST_RUN_LINES_SEND && line == 1);
	CHECK(step(0, 1, 1, 0) == AE_TEST_RUN_LINES_DONE);
	/* (no lines and nothing to settle: done at once) */
	ae_test_run_lines_start(&lines, 0, 0, 0);
	CHECK(step(0, 0, 0, 0) == AE_TEST_RUN_LINES_DONE && line == 0);
}

/* the first line waits for the game to settle: the reset done, the player alive */
static void settle_tests(void)
{
	ae_test_run_lines_start(&lines, 2, 1, 1000);
	CHECK(step(0, 0, 0, 1000) == AE_TEST_RUN_LINES_WAIT);
	CHECK(step(0, 0, 0, 4000) == AE_TEST_RUN_LINES_WAIT);
	CHECK(step(1, 0, 0, 4500) == AE_TEST_RUN_LINES_SEND && line == 0);
	/* (settled once: a game no longer "settled" while the lines run, as one with a dummy on its way is, holds nothing) */
	CHECK(step(0, 1, 1, 4533) == AE_TEST_RUN_LINES_SEND && line == 1);
	CHECK(step(0, 1, 1, 4566) == AE_TEST_RUN_LINES_DONE);
	/* (no lines, but a game to settle: done only then) */
	ae_test_run_lines_start(&lines, 0, 1, 0);
	CHECK(step(0, 0, 0, 100) == AE_TEST_RUN_LINES_WAIT);
	CHECK(step(1, 0, 0, 200) == AE_TEST_RUN_LINES_DONE);
	/* (never settled: failed before any line, none sent) */
	ae_test_run_lines_start(&lines, 2, 1, 1000);
	CHECK(step(0, 0, 0, 1000 + AE_TEST_RUN_LINES_SETTLE_MS) == AE_TEST_RUN_LINES_WAIT);
	CHECK(step(0, 0, 0, 1001 + AE_TEST_RUN_LINES_SETTLE_MS) == AE_TEST_RUN_LINES_FAILED && line == -1 &&
		lines.why == AE_TEST_RUN_LINES_NOT_SETTLED);
	CHECK(step(1, 0, 0, 1002 + AE_TEST_RUN_LINES_SETTLE_MS) == AE_TEST_RUN_LINES_FAILED && !ae_test_run_lines_pending(&lines));
}

/* a refused line is sent again for a short while; the line after it never goes before it is accepted */
static void retry_tests(void)
{
	unsigned long now;
	int sends = 0;

	ae_test_run_lines_start(&lines, 2, 0, 0);
	CHECK(step(1, 0, 0, 0) == AE_TEST_RUN_LINES_SEND && line == 0);
	/* (refused: not at once again, and not the next line) */
	CHECK(step(1, 1, 0, 33) == AE_TEST_RUN_LINES_WAIT && line == 0);
	CHECK(step(1, 0, 0, AE_TEST_RUN_LINES_RETRY_MS - 1) == AE_TEST_RUN_LINES_WAIT);
	CHECK(step(1, 0, 0, AE_TEST_RUN_LINES_RETRY_MS) == AE_TEST_RUN_LINES_SEND && line == 0);
	/* (accepted the second time: the next line) */
	CHECK(step(1, 1, 1, AE_TEST_RUN_LINES_RETRY_MS + 33) == AE_TEST_RUN_LINES_SEND && line == 1);
	CHECK(step(1, 1, 1, AE_TEST_RUN_LINES_RETRY_MS + 66) == AE_TEST_RUN_LINES_DONE);
	/* (refused every time: sent again until the time is up, then failed at that line, the later line never sent) */
	ae_test_run_lines_start(&lines, 3, 0, 0);
	CHECK(step(1, 0, 0, 0) == AE_TEST_RUN_LINES_SEND && line == 0);
	CHECK(step(1, 1, 1, 33) == AE_TEST_RUN_LINES_SEND && line == 1);
	for (now = 66; now < 20000 && ae_test_run_lines_pending(&lines); now += 33)
	{
		enum ae_test_run_lines_action action = step(1, lines.out, 0, now);

		CHECK(action != AE_TEST_RUN_LINES_DONE);
		if (action == AE_TEST_RUN_LINES_SEND)
		{
			sends++;
			CHECK(line == 1);
		}
	}
	CHECK(!ae_test_run_lines_pending(&lines) && lines.why == AE_TEST_RUN_LINES_REFUSED && line == 1);
	CHECK(sends >= 5 && sends <= AE_TEST_RUN_LINES_RETRY_FOR_MS / AE_TEST_RUN_LINES_RETRY_MS + 1);
	CHECK(now >= 33 + AE_TEST_RUN_LINES_RETRY_FOR_MS && now < 33 + AE_TEST_RUN_LINES_RETRY_FOR_MS + 400);
	CHECK(step(1, 1, 1, now) == AE_TEST_RUN_LINES_FAILED && line == 1);
}

/* a line answered not yet (a replay loading or seeking) is sent again for longer than a refused one, then accepted */
static void not_yet_tests(void)
{
	unsigned long now;

	ae_test_run_lines_start(&lines, 2, 0, 0);
	CHECK(step(1, 0, 0, 0) == AE_TEST_RUN_LINES_SEND && line == 0);
	CHECK(step(1, 1, 1, 33) == AE_TEST_RUN_LINES_SEND && line == 1);
	/* (not yet, past a refused line's time: still sent again) */
	for (now = 66; now < AE_TEST_RUN_LINES_RETRY_FOR_MS + 2000; now += 33)
		CHECK(step(1, lines.out, -1, now) != AE_TEST_RUN_LINES_FAILED);
	CHECK(ae_test_run_lines_pending(&lines));
	while (!lines.out && ae_test_run_lines_pending(&lines) && now < 60000)
		step(1, 0, -1, now += 33);
	CHECK(step(1, 1, 1, now += 33) == AE_TEST_RUN_LINES_DONE);
	/* (not yet every time: failed once its own time is up, refused, at that line) */
	ae_test_run_lines_start(&lines, 1, 0, 0);
	CHECK(step(1, 0, 0, 0) == AE_TEST_RUN_LINES_SEND);
	for (now = 33; now < 60000 && ae_test_run_lines_pending(&lines); now += 33)
		step(1, lines.out, -1, now);
	CHECK(!ae_test_run_lines_pending(&lines) && lines.why == AE_TEST_RUN_LINES_REFUSED && line == 0);
	CHECK(now >= AE_TEST_RUN_LINES_NOT_YET_FOR_MS && now < AE_TEST_RUN_LINES_NOT_YET_FOR_MS + 400);
}

/* a line with no answer fails the setup at that line */
static void answer_tests(void)
{
	ae_test_run_lines_start(&lines, 2, 0, 0);
	CHECK(step(1, 0, 0, 0) == AE_TEST_RUN_LINES_SEND);
	CHECK(step(1, 0, 0, AE_TEST_RUN_LINES_ANSWER_MS) == AE_TEST_RUN_LINES_WAIT);
	CHECK(step(1, 0, 0, AE_TEST_RUN_LINES_ANSWER_MS + 1) == AE_TEST_RUN_LINES_FAILED && line == 0 &&
		lines.why == AE_TEST_RUN_LINES_NO_ANSWER);
	/* (an answer that comes late, a line that waited for a dummy to join, is still an answer) */
	ae_test_run_lines_start(&lines, 1, 0, 0);
	CHECK(step(1, 0, 0, 0) == AE_TEST_RUN_LINES_SEND);
	CHECK(step(1, 1, 1, 4000) == AE_TEST_RUN_LINES_DONE);
}

int main(void)
{
	order_tests();
	settle_tests();
	retry_tests();
	answer_tests();
	not_yet_tests();
	if (failures)
		return 1;
	printf("ae_test_run_lines: ok\n");
	return 0;
}
