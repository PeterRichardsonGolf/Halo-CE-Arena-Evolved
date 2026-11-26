/* THE SEAT PANEL's pure rules: the name cut to the bar, the pips, the seats' places. */

#include <math.h>
#include <stdio.h>
#include <wchar.h>

#include "ae_seat_panel_rules.h"

static int failures;
#define CHECK(condition) do { if (!(condition)) { printf("%s:%d %s\n", __FILE__, __LINE__, #condition); failures++; } } while (0)

static float letter_width;

static float measure(wchar_t const *text)
{
	return (float)wcslen(text) * letter_width;
}

static void seat_name_clip_test(void)
{
	wchar_t out[48];

	letter_width = 8.6f;
	ae_seat_name_clip(L"PASSENGER", L"Christopher", AE_SEAT_BAR_WIDTH, measure, out, 48);
	CHECK(!wcscmp(out, L"PASSENGER Christ..."));
	letter_width = 8.6f;
	ae_seat_name_clip(L"GUNNER", L"Christopherson", AE_SEAT_BAR_WIDTH, measure, out, 48);
	CHECK(!wcscmp(out, L"GUNNER Christoph..."));
	/* short: unchanged */
	ae_seat_name_clip(L"GUNNER", L"Kojo", AE_SEAT_BAR_WIDTH, measure, out, 48);
	CHECK(!wcscmp(out, L"GUNNER Kojo"));
	/* an AI rider: the role alone */
	ae_seat_name_clip(L"GUNNER", L"", AE_SEAT_BAR_WIDTH, measure, out, 48);
	CHECK(!wcscmp(out, L"GUNNER"));
	/* the role is never cut, however narrow */
	letter_width = 40.0f;
	ae_seat_name_clip(L"PASSENGER", L"Christopher", AE_SEAT_BAR_WIDTH, measure, out, 48);
	CHECK(!wcscmp(out, L"PASSENGER"));
	/* a small buffer stays a string */
	letter_width = 1.0f;
	ae_seat_name_clip(L"PASSENGER", L"Christopher", AE_SEAT_BAR_WIDTH, measure, out, 5);
	CHECK(!wcscmp(out, L"PASS"));
}

static void seat_pips_test(void)
{
	CHECK(ae_seat_pips_lit(0.0f) == 0);
	CHECK(ae_seat_pips_lit(-1.0f) == 0);
	CHECK(ae_seat_pips_lit(NAN) == 0);
	CHECK(ae_seat_pips_lit(0.01f) == 1);
	CHECK(ae_seat_pips_lit(0.25f) == 2);
	CHECK(ae_seat_pips_lit(0.26f) == 3);
	CHECK(ae_seat_pips_lit(1.0f) == 8);
	CHECK(ae_seat_pips_lit(3.0f) == 8);
	CHECK(ae_seat_pips_red(0) && ae_seat_pips_red(2) && !ae_seat_pips_red(3));
}

static void seat_layout_test(void)
{
	float x, y;

	/* side by side at 180 */
	CHECK(ae_seat_columns(2, 1.0f, 143.0f, 1920.0f) == 2);
	CHECK(ae_seat_columns(4, 1.0f, 143.0f, 1920.0f) == 4);
	ae_seat_cell(1, 2, 143.0f, 82.0f, 50.0f, &x, &y);
	CHECK(x == 323.0f && y == 82.0f);
	/* a quarter view: two rows from 40 */
	CHECK(ae_seat_columns(4, 0.75f, 99.0f, 960.0f) == 2);
	CHECK(ae_seat_panel_top(46.0f, 4, 0.75f) == 40.0f);
	ae_seat_cell(2, 2, 99.0f, 40.0f, 50.0f, &x, &y);
	CHECK(x == 99.0f && y == 90.0f);
	ae_seat_cell(3, 2, 99.0f, 40.0f, 50.0f, &x, &y);
	CHECK(x == 279.0f && y == 90.0f);
	/* two seats there, and every seat elsewhere, keep the ammo panel's top */
	CHECK(ae_seat_columns(2, 0.75f, 99.0f, 960.0f) == 2);
	CHECK(ae_seat_panel_top(46.0f, 2, 0.75f) == 46.0f);
	CHECK(ae_seat_panel_top(82.0f, 4, 1.0f) == 82.0f);
	CHECK(ae_seat_panel_top(46.0f, 4, 0.85f) == 46.0f);
	/* never more than fit, never none */
	CHECK(ae_seat_columns(8, 1.0f, 143.0f, 720.0f) == 3);
	CHECK(ae_seat_columns(2, 1.0f, 700.0f, 720.0f) == 1);
}

static void seat_roll_test(void)
{
	struct ae_seat_kind seats[8];
	struct ae_seat_line lines[8];
	wchar_t text[16];
	int count, i;

	/* up to 3 seats: every seat its own line, open or not */
	for (i = 0; i < 8; i++)
		seats[i].gunner = seats[i].open = 0;
	seats[0].gunner = 1;
	seats[0].open = seats[1].open = seats[2].open = 1;
	count = ae_seat_roll(3, seats, lines);
	CHECK(count == 3 && lines[0].seat == 0 && lines[0].open == 1 && lines[1].seat == 1 && lines[1].open == 1 && lines[2].seat == 2);
	/* the Warthog's two: untouched */
	count = ae_seat_roll(2, seats, lines);
	CHECK(count == 2 && lines[1].open == 1);
	/* past 3: the open seats of one kind are one line at the first of them: a gunner, then 6 passengers, all open */
	for (i = 1; i < 7; i++)
		seats[i].gunner = 0;
	for (i = 0; i < 7; i++)
		seats[i].open = 1;
	count = ae_seat_roll(7, seats, lines);
	CHECK(count == 2 && lines[0].seat == 0 && lines[0].open == 1 && lines[1].seat == 1 && lines[1].open == 6);
	/* an occupied seat keeps its own line, in its place; the open ones of its kind roll after it where the first is */
	seats[2].open = 0;
	count = ae_seat_roll(7, seats, lines);
	CHECK(count == 3 && lines[0].seat == 0 && lines[0].open == 1 && lines[1].seat == 1 && lines[1].open == 5 &&
		lines[2].seat == 2 && lines[2].open == 0);
	/* a second open gunner joins the first's line (4 seats: past 3) */
	seats[3].gunner = 1;
	seats[3].open = 1;
	count = ae_seat_roll(4, seats, lines);
	CHECK(count == 3 && lines[0].seat == 0 && lines[0].open == 2 && lines[1].seat == 1 && lines[1].open == 1 &&
		lines[2].seat == 2 && lines[2].open == 0);
	/* a taken gunner seat is its own line and does not take the open gunners with it */
	seats[3].open = 0;
	seats[2].gunner = 1;
	count = ae_seat_roll(4, seats, lines);
	CHECK(count == 4 && lines[0].open == 1 && lines[1].open == 1 && lines[2].open == 0 && lines[3].open == 0);
	/* all occupied: nothing rolls */
	for (i = 0; i < 8; i++)
		seats[i].open = 0;
	count = ae_seat_roll(8, seats, lines);
	CHECK(count == 8 && lines[7].seat == 7 && lines[7].open == 0);
	/* none, and more than the lines hold */
	CHECK(ae_seat_roll(0, seats, lines) == 0 && ae_seat_roll(-1, seats, lines) == 0);
	CHECK(ae_seat_roll(AE_SEAT_ROLL_MOST + 3, seats, lines) == AE_SEAT_ROLL_MOST);
	/* the open line's words */
	ae_seat_open_text(1, text, 16);
	CHECK(!wcscmp(text, L"OPEN"));
	ae_seat_open_text(6, text, 16);
	CHECK(!wcscmp(text, L"OPEN x6"));
	ae_seat_open_text(12, text, 16);
	CHECK(!wcscmp(text, L"OPEN x12"));
	ae_seat_open_text(0, text, 16);
	CHECK(!wcscmp(text, L"OPEN"));
	ae_seat_open_text(6, text, 7);
	CHECK(!wcscmp(text, L"OPEN x") && text[6] == 0);
}

int main(void)
{
	seat_name_clip_test();
	seat_pips_test();
	seat_layout_test();
	seat_roll_test();
	if (failures)
		printf("%d failure(s)\n", failures);
	return failures != 0;
}
