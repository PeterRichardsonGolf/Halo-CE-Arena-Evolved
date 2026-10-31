/* CE's counter digits laid out at any size (port/linux/game/ae_hud_digit_cells.c): frames, advances, quads, the
ink bounds callers centre a block by */
#include <math.h>
#include <stdio.h>
#include <wchar.h>

#include "ae_hud_digit_cells.h"

static int failures;
#define AE_CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabsf((float)(a) - (float)(b)) < 0.01f)

static void test_frames(void)
{
	AE_CHECK(ae_digit_frame(L'0') == 0 && ae_digit_frame(L'9') == 9);
	AE_CHECK(ae_digit_frame(L'.') == 10 && ae_digit_frame(L':') == 11 && ae_digit_frame(L'-') == 12);
	AE_CHECK(ae_digit_frame(L'N') == -1 && ae_digit_frame(L' ') == -1 && ae_digit_frame(0) == -1);
}

static void test_advances_and_measure(void)
{
	AE_CHECK(NEAR(ae_digit_advance(L'1', 24.0f, 0.0f), 19.2f) && NEAR(ae_digit_advance(L':', 24.0f, 0.0f), 12.0f));
	AE_CHECK(NEAR(ae_digit_advance(L' ', 20.0f, 0.0f), 10.0f) && NEAR(ae_digit_advance(L'X', 20.0f, 0.0f), 16.0f));
	/* (a fixed cell: every character the same) */
	AE_CHECK(NEAR(ae_digit_advance(L':', 24.0f, 21.0f), 21.0f));
	AE_CHECK(NEAR(ae_digits_measure_cells(L"1:42", 20.0f, 0.0f), 16.0f * 3.0f + 10.0f));
	AE_CHECK(NEAR(ae_digits_measure_cells(L"040", 24.0f, 21.0f), 63.0f));
	AE_CHECK(NEAR(ae_digits_measure_cells(L"", 24.0f, 0.0f), 0.0f));
	AE_CHECK(NEAR(ae_digits_measure_cells(NULL, 24.0f, 0.0f), 0.0f));
	/* (the clock's width is the same for every time of one length: digits in fixed cells) */
	AE_CHECK(NEAR(ae_digits_measure_cells(L"1:11", 20.0f, 0.0f), ae_digits_measure_cells(L"8:08", 20.0f, 0.0f)));
}

static void test_quads(void)
{
	struct ae_digit_quad q;
	real left, right;

	/* (the cell is 1.2 x the cap tall, the cap's rows 1/12 .. 11/12 of it: the cap sits on the baseline) */
	ae_digit_quad(L"0", 0, 100.0f, 200.0f, 24.0f, 0.0f, FALSE, 1.5f, &q);
	AE_CHECK(q.visible && NEAR(q.y1 - q.y0, 28.8f) && NEAR(q.y0 + 28.8f / 12.0f, 176.0f) &&
		NEAR(q.y1 - 28.8f / 12.0f, 200.0f));
	AE_CHECK(NEAR(q.x1 - q.x0, 43.2f));
	/* (its ink centred in its 19.2 px cell) */
	ae_digit_ink(L'0', &left, &right);
	AE_CHECK(NEAR(q.x0 + (left + right) / 2.0f * 43.2f, 109.6f));
	/* (ink at the cell's left) */
	ae_digit_quad(L"0", 0, 100.0f, 200.0f, 24.0f, 0.0f, TRUE, 1.5f, &q);
	AE_CHECK(NEAR(q.x0 + left * 43.2f, 100.0f));
	/* (the second character starts one advance on; a space draws nothing) */
	ae_digit_quad(L"1 7", 2, 0.0f, 50.0f, 20.0f, 0.0f, FALSE, 1.5f, &q);
	ae_digit_ink(L'7', &left, &right);
	AE_CHECK(q.visible && NEAR(q.x0 + (left + right) / 2.0f * (q.x1 - q.x0), 16.0f + 10.0f + 8.0f));
	ae_digit_quad(L"1 7", 1, 0.0f, 50.0f, 20.0f, 0.0f, FALSE, 1.5f, &q);
	AE_CHECK(!q.visible);
	ae_digit_quad(L"1", 5, 0.0f, 50.0f, 20.0f, 0.0f, FALSE, 1.5f, &q);
	AE_CHECK(!q.visible);
	/* (no aspect from the sprite: the sheet's own, 1.5; the 4 is wider) */
	ae_digit_quad(L"4", 0, 0.0f, 50.0f, 20.0f, 0.0f, FALSE, 0.0f, &q);
	AE_CHECK(NEAR((q.x1 - q.x0) / (q.y1 - q.y0), 19.0f / 12.0f));
}

static void test_ink_bounds(void)
{
	real left, right, l0, r0, l1, r1;

	AE_CHECK(!ae_digits_ink_bounds_cells(L"", 24.0f, 0.0f, FALSE, &left, &right));
	AE_CHECK(!ae_digits_ink_bounds_cells(L"  ", 24.0f, 0.0f, FALSE, &left, &right));
	AE_CHECK(ae_digits_ink_bounds_cells(L"180", 24.0f, 21.0f, FALSE, &left, &right));
	ae_digit_ink(L'1', &l0, &r0);
	ae_digit_ink(L'0', &l1, &r1);
	/* (the 1's ink centred in the first 21 px cell, the 0's in the third) */
	AE_CHECK(NEAR(left, 10.5f - (r0 - l0) * 43.2f / 2.0f) && NEAR(right, 52.5f + (r1 - l1) * 43.2f / 2.0f));
	/* (narrower for a 1 than a 0: the slanted digits' ink is wider than their cells, so it is the ink a block is
	centred by) */
	ae_digits_ink_bounds_cells(L"0", 24.0f, 21.0f, FALSE, &l0, &r0);
	ae_digits_ink_bounds_cells(L"1", 24.0f, 21.0f, FALSE, &l1, &r1);
	AE_CHECK(r1 - l1 < r0 - l0);
}

/* the clock's colon (spec 14.15 part 7, "even gaps like 4:12"): only the colon moves in its cell, by
(CL[left digit] - CR[right digit]) / 2 x the cap height; the clock's width never changes */
static void test_colon_shift(void)
{
	static const struct { const wchar_t *time; real shift; } cases[] = {
		{ L"1:42", (0.081f - 0.375f) / 2.0f }, { L"4:12", (0.206f - 0.269f) / 2.0f },
		{ L"0:07", (0.312f - 0.500f) / 2.0f }, { L"11:41", (0.081f - 0.375f) / 2.0f },
		{ L"10:00", (0.312f - 0.500f) / 2.0f }, { L"0:11", (0.312f - 0.269f) / 2.0f } };
	unsigned i;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
	{
		const wchar_t *time = cases[i].time;
		long colon = (long)(wcschr(time, L':') - time);
		struct ae_digit_quad shifted, centred;
		real left, right;

		AE_CHECK(NEAR(ae_digits_colon_shift(time[colon - 1], time[colon + 1]), cases[i].shift));
		/* (drawn: the colon's ink centre moved from its cell's centre by the shift x the cap; the digits before it
		16 px each at 20 px cap, the colon's cell 10) */
		ae_digit_quad(time, colon, 0.0f, 0.0f, 20.0f, 0.0f, FALSE, 1.5f, &shifted);
		ae_digit_ink(L':', &left, &right);
		AE_CHECK(NEAR(shifted.x0 + (shifted.x1 - shifted.x0) * (left + right) / 2.0f,
			(real)colon * 16.0f + 5.0f + cases[i].shift * 20.0f));
		/* (fixed cells: no shift) */
		ae_digit_quad(time, colon, 0.0f, 0.0f, 20.0f, 16.0f, FALSE, 1.5f, &centred);
		AE_CHECK(NEAR(centred.x0 + (centred.x1 - centred.x0) * (left + right) / 2.0f, (real)colon * 16.0f + 8.0f));
	}
	/* (every time of one length is as wide) */
	AE_CHECK(NEAR(ae_digits_measure_cells(L"1:42", 20.0f, 0.0f), ae_digits_measure_cells(L"4:12", 20.0f, 0.0f)));
	AE_CHECK(NEAR(ae_digits_measure_cells(L"11:41", 20.0f, 0.0f), ae_digits_measure_cells(L"10:00", 20.0f, 0.0f)));
	/* (no shift beside a non-digit, nor in fixed cells) */
	AE_CHECK(NEAR(ae_digits_colon_shift(L' ', L'1'), 0.0f));
}

int main(void)
{
	test_colon_shift();
	test_frames();
	test_advances_and_measure();
	test_quads();
	test_ink_bounds();
	printf("%s\n", failures ? "FAILED" : "ok");
	return failures != 0;
}
