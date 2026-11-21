/* ae_test_run_lines.c: Arena Evolved TEST RUN, the order of a test's setup lines (see ae_test_run_lines.h). No engine
includes. */

#include <string.h>

#include "ae_test_run_lines.h"

void ae_test_run_lines_start(struct ae_test_run_lines *lines, short count, int settle, unsigned long now)
{
	memset(lines, 0, sizeof(*lines));
	lines->count = count > 0 ? count : 0;
	lines->settle = settle != 0;
	lines->since = now;
}

int ae_test_run_lines_pending(struct ae_test_run_lines const *lines)
{
	return !lines->finished;
}

static enum ae_test_run_lines_action fail(struct ae_test_run_lines *lines, enum ae_test_run_lines_why why, short at,
	short *line)
{
	lines->finished = AE_TEST_RUN_LINES_FAILED;
	lines->why = (short)why;
	lines->next = at;
	*line = at;
	return AE_TEST_RUN_LINES_FAILED;
}

enum ae_test_run_lines_action ae_test_run_lines_step(struct ae_test_run_lines *lines, int settled, int answered, int ok,
	unsigned long now, short *line)
{
	*line = lines->next;
	if (lines->finished)
		return (enum ae_test_run_lines_action)lines->finished;
	if (lines->out)
	{
		if (!answered)
		{
			if (now - lines->sent_at > AE_TEST_RUN_LINES_ANSWER_MS)
				return fail(lines, AE_TEST_RUN_LINES_NO_ANSWER, lines->next, line);
			return AE_TEST_RUN_LINES_WAIT;
		}
		lines->out = 0;
		if (ok > 0)
		{
			lines->next++;
			lines->tries = 0;
		}
		else if (now - lines->first_at >= (ok < 0 ? AE_TEST_RUN_LINES_NOT_YET_FOR_MS : AE_TEST_RUN_LINES_RETRY_FOR_MS))
			return fail(lines, AE_TEST_RUN_LINES_REFUSED, lines->next, line);
	}
	/* (the game settled once: the lines after the first follow as each is accepted) */
	if (lines->settle)
	{
		if (!settled)
		{
			if (now - lines->since > AE_TEST_RUN_LINES_SETTLE_MS)
				return fail(lines, AE_TEST_RUN_LINES_NOT_SETTLED, -1, line);
			return AE_TEST_RUN_LINES_WAIT;
		}
		lines->settle = 0;
	}
	if (lines->next >= lines->count)
	{
		lines->finished = AE_TEST_RUN_LINES_DONE;
		*line = lines->count;
		return AE_TEST_RUN_LINES_DONE;
	}
	/* (a refused line again, a little later) */
	if (lines->tries && now - lines->sent_at < AE_TEST_RUN_LINES_RETRY_MS)
		return AE_TEST_RUN_LINES_WAIT;
	if (!lines->tries)
		lines->first_at = now;
	lines->tries++;
	lines->out = 1;
	lines->sent_at = now;
	*line = lines->next;
	return AE_TEST_RUN_LINES_SEND;
}
