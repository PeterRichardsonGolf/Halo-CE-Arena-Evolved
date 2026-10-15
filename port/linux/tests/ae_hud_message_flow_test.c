#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "ae_hud_message_flow.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* one pixel a character */
static real measure(const wchar_t *text, long count, void *context)
{
	(void)text;
	(void)context;
	return (real)count;
}

/* a line laid out the way ae_hud_messages.c lays a piece: the runs drawn, with their line numbers */
struct run { wchar_t text[64]; short line; real x; };
static struct run runs[32];
static int run_count;

static void piece(struct ae_hud_message_flow *flow, const wchar_t *text)
{
	long length = (long)wcslen(text);

	while (length > 0)
	{
		long skip, n;

		if (*text == L'\n')
		{
			ae_hud_message_flow_new_line(flow);
			text++;
			length--;
			continue;
		}
		ae_hud_message_flow_line(flow);
		n = ae_hud_message_flow_fit(flow, text, length, measure, NULL, &skip);
		text += skip;
		length -= skip;
		if (n == 0)
		{
			if (length > 0 && *text != L'\n')
				ae_hud_message_flow_new_line(flow);
			continue;
		}
		if (run_count < 32)
		{
			wcsncpy(runs[run_count].text, text, (size_t)n);
			runs[run_count].text[n] = 0;
			runs[run_count].line = flow->lines;
			runs[run_count].x = flow->x;
			run_count++;
		}
		flow->x += measure(text, n, NULL);
		flow->line_empty = FALSE;
		text += n;
		length -= n;
		if (length > 0 && *text != L'\n')
			ae_hud_message_flow_new_line(flow);
	}
}

int main(void)
{
	struct ae_hud_message_flow flow;
	long skip;
	real x;

	/* everything fits: one run, the pen after its trailing space */
	ae_hud_message_flow_start(&flow, 10.0f, 48.0f, 100.0f, 18.0f);
	run_count = 0;
	piece(&flow, L"Press ");
	CHECK(run_count == 1 && flow.lines == 1 && !wcscmp(runs[0].text, L"Press ") && runs[0].x == 10.0f);
	CHECK(flow.x == 16.0f && flow.baseline == 100.0f);

	/* a picture after it, then text on the same line */
	CHECK(!ae_hud_message_flow_picture(&flow, 8.0f, 1.0f, &x) && x == 16.0f && flow.x == 25.0f);
	piece(&flow, L" to enter");
	CHECK(run_count == 2 && runs[1].line == 1 && runs[1].x == 25.0f && flow.x == 34.0f);

	/* a piece too long: broken between words, the second line's leading space dropped */
	piece(&flow, L" driver seat of Scorpion");
	CHECK(flow.lines == 2 && flow.baseline == 118.0f);
	CHECK(run_count == 4 && !wcscmp(runs[2].text, L" driver seat ") && runs[2].line == 1);
	CHECK(!wcscmp(runs[3].text, L"of Scorpion") && runs[3].line == 2 && runs[3].x == 10.0f);

	/* a picture that does not fit starts a new line; one on an empty line always goes */
	ae_hud_message_flow_start(&flow, 0.0f, 20.0f, 50.0f, 10.0f);
	ae_hud_message_flow_line(&flow);
	flow.x = 15.0f;
	flow.line_empty = 0;
	CHECK(ae_hud_message_flow_picture(&flow, 8.0f, 0.0f, &x) && x == 0.0f && flow.lines == 2 && flow.baseline == 60.0f);
	ae_hud_message_flow_new_line(&flow);
	CHECK(!ae_hud_message_flow_picture(&flow, 40.0f, 0.0f, &x) && x == 0.0f && flow.lines == 3);

	/* a word wider than the line is cut between characters on an empty line, never left out */
	ae_hud_message_flow_start(&flow, 0.0f, 4.0f, 0.0f, 10.0f);
	ae_hud_message_flow_line(&flow);
	CHECK(ae_hud_message_flow_fit(&flow, L"abcdefgh", 8, measure, NULL, &skip) == 4 && skip == 0);
	flow.line_empty = 0;
	flow.x = 1.0f;
	CHECK(ae_hud_message_flow_fit(&flow, L"abcdefgh", 8, measure, NULL, &skip) == 0);
	run_count = 0;
	ae_hud_message_flow_start(&flow, 0.0f, 4.0f, 0.0f, 10.0f);
	piece(&flow, L"abcdefgh");
	CHECK(run_count == 2 && !wcscmp(runs[0].text, L"abcd") && !wcscmp(runs[1].text, L"efgh") && flow.lines == 2);

	/* spaces only move the pen; a line break starts a new line */
	ae_hud_message_flow_start(&flow, 0.0f, 100.0f, 0.0f, 10.0f);
	ae_hud_message_flow_line(&flow);
	flow.x = 5.0f;
	flow.line_empty = 0;
	CHECK(ae_hud_message_flow_fit(&flow, L"  ", 2, measure, NULL, &skip) == 2);
	run_count = 0;
	ae_hud_message_flow_start(&flow, 0.0f, 100.0f, 0.0f, 10.0f);
	piece(&flow, L"one\ntwo");
	CHECK(run_count == 2 && runs[0].line == 1 && runs[1].line == 2 && !wcscmp(runs[1].text, L"two"));

	/* the first new line of an unstarted flow is its first line */
	ae_hud_message_flow_start(&flow, 3.0f, 100.0f, 40.0f, 10.0f);
	ae_hud_message_flow_new_line(&flow);
	CHECK(flow.lines == 1 && flow.baseline == 40.0f && flow.x == 3.0f);

	/* nothing to fit */
	CHECK(ae_hud_message_flow_fit(&flow, L"", 0, measure, NULL, &skip) == 0 && skip == 0);

	if (failures)
		return 1;
	printf("ae_hud_message_flow: ok\n");
	return 0;
}
