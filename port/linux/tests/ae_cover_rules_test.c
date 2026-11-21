/*
AE_COVER_RULES_TEST.C

A picture drawn to fill a box (port/linux/src/ae_cover_rules.c): the part that shows, and whether a picture needs
cutting.
*/

#include <math.h>
#include <stdio.h>

#include "../src/ae_cover_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static int near_to(float a, float b)
{
	return fabsf(a - b) < 0.0005f;
}

/* the shown part's proportions in pixels equal the box's */
static void shape_is(int w, int h, float box_w, float box_h)
{
	float uv[4], shown_w, shown_h;

	ae_cover_uv(w, h, box_w, box_h, uv);
	shown_w = (uv[2] - uv[0]) * (float)w;
	shown_h = (uv[3] - uv[1]) * (float)h;
	CHECK(near_to(shown_w / shown_h, box_w / box_h));
	/* (centred, inside the picture) */
	CHECK(near_to(uv[0], 1.0f - uv[2]) && near_to(uv[1], 1.0f - uv[3]));
	CHECK(uv[0] >= 0.0f && uv[1] >= 0.0f && uv[2] <= 1.0f && uv[3] <= 1.0f);
}

/* the whole picture inside the box: its proportions kept, centred, touching two sides */
static void fits_whole(int w, int h, float box_w, float box_h)
{
	float rect[4];

	ae_cover_fit_rect(w, h, box_w, box_h, rect);
	CHECK(near_to(rect[2] / rect[3], (float)w / (float)h));
	CHECK(rect[0] >= 0.0f && rect[1] >= 0.0f && rect[0] + rect[2] <= box_w + 0.001f && rect[1] + rect[3] <= box_h + 0.001f);
	CHECK(near_to(rect[0], box_w - rect[0] - rect[2]) && near_to(rect[1], box_h - rect[1] - rect[3]));
	CHECK(near_to(rect[2], box_w) || near_to(rect[3], box_h));
}

int main(void)
{
	float uv[4];

	/* (whole: a 16:9 Workshop picture in a stock preview's box shows full width, room above and below) */
	fits_whole(2048, 1152, 140.0f, 114.0f);
	fits_whole(668, 292, 100.0f, 100.0f);
	fits_whole(560, 456, 160.0f, 90.0f);
	{
		float rect[4];

		ae_cover_fit_rect(2048, 1152, 140.0f, 114.0f, rect);
		CHECK(near_to(rect[0], 0.0f) && near_to(rect[2], 140.0f) && near_to(rect[3], 140.0f * 1152.0f / 2048.0f));
		ae_cover_fit_rect(560, 456, 560.0f, 456.0f, rect);
		CHECK(near_to(rect[0], 0.0f) && near_to(rect[1], 0.0f) && near_to(rect[2], 560.0f) && near_to(rect[3], 456.0f));
		ae_cover_fit_rect(0, 456, 140.0f, 114.0f, rect);
		CHECK(near_to(rect[2], 140.0f) && near_to(rect[3], 114.0f));
		ae_cover_fit_rect(560, 456, 0.0f, 114.0f, rect);
		CHECK(near_to(rect[2], 0.0f));
	}

	/* (a stock preview, 560 by 456, in a 16:9 box: wider box, the top and bottom cut) */
	ae_cover_uv(560, 456, 160.0f, 90.0f, uv);
	CHECK(near_to(uv[0], 0.0f) && near_to(uv[2], 1.0f));
	CHECK(uv[1] > 0.0f && near_to(uv[3], 1.0f - uv[1]));
	CHECK(near_to(uv[3] - uv[1], (560.0f / 456.0f) / (160.0f / 90.0f)));
	shape_is(560, 456, 160.0f, 90.0f);
	/* (a Workshop image, 2048 by 1152, in a 4:3 box: narrower box, the sides cut to 75 percent) */
	ae_cover_uv(2048, 1152, 140.0f, 105.0f, uv);
	CHECK(near_to(uv[1], 0.0f) && near_to(uv[3], 1.0f));
	CHECK(near_to(uv[2] - uv[0], 0.75f));
	shape_is(2048, 1152, 140.0f, 105.0f);
	/* (a wide one, 668 by 292, in a square box: 292 / 668 of its width shows) */
	ae_cover_uv(668, 292, 100.0f, 100.0f, uv);
	CHECK(near_to(uv[2] - uv[0], 292.0f / 668.0f) && near_to(uv[1], 0.0f) && near_to(uv[3], 1.0f));
	shape_is(668, 292, 100.0f, 100.0f);
	/* (the same proportions: the whole picture) */
	ae_cover_uv(560, 456, 140.0f, 114.0f, uv);
	CHECK(near_to(uv[0], 0.0f) && near_to(uv[1], 0.0f) && near_to(uv[2], 1.0f) && near_to(uv[3], 1.0f));
	ae_cover_uv(1920, 1080, 16.0f, 9.0f, uv);
	CHECK(near_to(uv[0], 0.0f) && near_to(uv[1], 0.0f) && near_to(uv[2], 1.0f) && near_to(uv[3], 1.0f));
	/* (zero or negative sizes: the whole picture, no division by zero) */
	ae_cover_uv(0, 456, 140.0f, 114.0f, uv);
	CHECK(uv[0] == 0.0f && uv[1] == 0.0f && uv[2] == 1.0f && uv[3] == 1.0f);
	ae_cover_uv(560, 0, 140.0f, 114.0f, uv);
	CHECK(uv[2] == 1.0f && uv[3] == 1.0f);
	ae_cover_uv(560, 456, 0.0f, 114.0f, uv);
	CHECK(uv[0] == 0.0f && uv[2] == 1.0f);
	ae_cover_uv(560, 456, 140.0f, 0.0f, uv);
	CHECK(uv[1] == 0.0f && uv[3] == 1.0f);
	ae_cover_uv(-5, -5, -1.0f, -1.0f, uv);
	CHECK(uv[0] == 0.0f && uv[3] == 1.0f);

	/* (needs no cutting: within a percent of the box's proportions) */
	CHECK(ae_cover_matches(560, 456, 140, 114));
	CHECK(ae_cover_matches(1120, 912, 140, 114));
	CHECK(!ae_cover_matches(2048, 1152, 140, 114));
	CHECK(!ae_cover_matches(668, 292, 140, 114));
	CHECK(ae_cover_matches(0, 10, 140, 114) && ae_cover_matches(10, 10, 0, 0));

	if (!failures)
		printf("ae_cover_rules_test: ok\n");
	return failures ? 1 : 0;
}
