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

	/* CE's line ends (draw_string.c's): the campaign's "|n" (a10's help text), "\r\n", a lone '\r' all one '\n'; a tab
	a space; any other '|' is a character; the copy cut at the buffer, always ended */
	{
		wchar_t text[64];

		CHECK(ae_hud_message_flow_clean(L"Charged|n|nPress ", text, 64) == 15 && !wcscmp(text, L"Charged\n\nPress "));
		CHECK(ae_hud_message_flow_clean(L"a\r\nb\rc\nd\te", text, 64) == 9 && !wcscmp(text, L"a\nb\nc\nd e"));
		CHECK(ae_hud_message_flow_clean(L"x|y|", text, 64) == 4 && !wcscmp(text, L"x|y|"));
		CHECK(ae_hud_message_flow_clean(L"|N", text, 64) == 2 && !wcscmp(text, L"|N"));
		CHECK(ae_hud_message_flow_clean(L"abcdef", text, 4) == 3 && !wcscmp(text, L"abc"));
		CHECK(ae_hud_message_flow_clean(L"", text, 64) == 0 && text[0] == 0);
		CHECK(ae_hud_message_flow_clean(L"ab", text, 0) == 0);

		/* a10's look inversion question, laid out piece by piece as hud_messaging.c hands it over (text, the A
		button, text, the B button, text): three lines and a blank one between each, never two pieces on one place */
		ae_hud_message_flow_start(&flow, 10.0f, 200.0f, 100.0f, 20.0f);
		run_count = 0;
		ae_hud_message_flow_clean(L"Vertical looking is now inverted|n|nPress ", text, 64);
		piece(&flow, text);
		CHECK(run_count == 2 && runs[0].line == 1 && runs[1].line == 3 && !wcscmp(runs[1].text, L"Press "));
		CHECK(!ae_hud_message_flow_picture(&flow, 8.0f, 1.0f, &x) && x == 16.0f && flow.lines == 3);
		ae_hud_message_flow_clean(L" to try it the other way|n|nPress ", text, 64);
		piece(&flow, text);
		CHECK(run_count == 4 && runs[2].line == 3 && runs[2].x == 25.0f && runs[3].line == 5 && runs[3].x == 10.0f);
		CHECK(!ae_hud_message_flow_picture(&flow, 8.0f, 1.0f, &x) && x == 16.0f && flow.lines == 5);
		ae_hud_message_flow_clean(L" to leave it like it is", text, 64);
		piece(&flow, text);
		CHECK(run_count == 5 && runs[4].line == 5 && runs[4].x == 25.0f && flow.baseline == 180.0f);
	}

	/* the first new line of an unstarted flow is its first line */
	ae_hud_message_flow_start(&flow, 3.0f, 100.0f, 40.0f, 10.0f);
	ae_hud_message_flow_new_line(&flow);
	CHECK(flow.lines == 1 && flow.baseline == 40.0f && flow.x == 3.0f);

	/* nothing to fit */
	CHECK(ae_hud_message_flow_fit(&flow, L"", 0, measure, NULL, &skip) == 0 && skip == 0);

	/* sizes (campaign review F1-F3): 24 at 1080p single, 16 floor everywhere, pitch 1.3 em */
#define NEAR(a, b) ((a) > (b) - 0.01f && (a) < (b) + 0.01f)
	CHECK(NEAR(ae_hud_message_px(1.0f), 24.0f));
	CHECK(NEAR(ae_hud_message_px(0.854f), 20.496f));      /* 1080p half view */
	CHECK(NEAR(ae_hud_message_px(0.6667f), 16.0f));       /* 720p single: 16, not 24 x 0.667 */
	CHECK(NEAR(ae_hud_message_px(0.75f * 0.6667f), 16.0f)); /* 720p quarter: the same floor */
	CHECK(NEAR(ae_hud_message_px(2.0f), 48.0f));
	/* the bigger screen never has the smaller text (review I5): 1080p half >= 720p half */
	CHECK(ae_hud_message_px(0.854f) >= ae_hud_message_px(0.854f * 0.6667f));
	CHECK(NEAR(ae_hud_message_pitch(24.0f), 31.2f));
	/* no ammo group: 26 em from the left (624 at 1080p), not the view's edge */
	CHECK(NEAR(ae_hud_message_wrap_right(100.0f, 1920.0f, 0.0f, 24.0f, 1.0f), 724.0f));
	/* under a group: twice the group */
	CHECK(NEAR(ae_hud_message_wrap_right(100.0f, 1920.0f, 251.5f, 24.0f, 1.0f), 603.0f));
	/* a wide group: capped at 26 em */
	CHECK(NEAR(ae_hud_message_wrap_right(100.0f, 1920.0f, 400.0f, 24.0f, 1.0f), 724.0f));
	/* a narrow view: the edge less 16 x scale */
	CHECK(NEAR(ae_hud_message_wrap_right(100.0f, 600.0f, 0.0f, 24.0f, 1.0f), 584.0f));
	/* never under 8 em */
	CHECK(NEAR(ae_hud_message_wrap_right(100.0f, 200.0f, 0.0f, 24.0f, 1.0f), 292.0f));
	/* pictures (the HUD sheet's real sizes, a10): the A button (24x30) 1.9 x the capitals; a stick with its label
	(36x40) CE's 40/30 of that, 2.53; the d-pad (31x28) and the small black / white (19x22) a button's 1.9 at least;
	the 53x53 one and a weapon (122x26, 2:1 or wider) 2.6 at most */
	CHECK(NEAR(ae_hud_message_picture_height(16.8f, 24.0f, 30.0f, 30.0f), 31.92f));
	CHECK(NEAR(ae_hud_message_picture_height(16.8f, 36.0f, 40.0f, 30.0f), 42.56f));
	CHECK(NEAR(ae_hud_message_picture_height(16.8f, 31.0f, 28.0f, 30.0f), 31.92f));
	CHECK(NEAR(ae_hud_message_picture_height(16.8f, 19.0f, 22.0f, 30.0f), 31.92f));
	CHECK(NEAR(ae_hud_message_picture_height(16.8f, 53.0f, 53.0f, 30.0f), 43.68f));
	CHECK(NEAR(ae_hud_message_picture_height(16.8f, 122.0f, 26.0f, 30.0f), 43.68f));
	/* not the HUD's sheet (the menus' small icons, a weapon pack's): a button's 1.9, a wide one 2.6 */
	CHECK(NEAR(ae_hud_message_picture_height(16.8f, 16.0f, 20.0f, 0.0f), 31.92f));
	CHECK(NEAR(ae_hud_message_picture_height(16.8f, 128.0f, 64.0f, 0.0f), 43.68f));
	CHECK(NEAR(ae_hud_message_picture_height(16.8f, 0.0f, 64.0f, 30.0f), 31.92f));

	if (failures)
		return 1;
	printf("ae_hud_message_flow: ok\n");
	return 0;
}
