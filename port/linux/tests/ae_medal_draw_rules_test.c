/* ae_medal_draw_rules_test.c: the medals' slots, time line, count and the way each is shown (ae_medal_draw_rules.h). */
#include <math.h>
#include <stdio.h>
#include "ae_medal_draw_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 0.01f)

static void mode_test(void)
{
	/* one player: a picture if there is one, else the name in the holder */
	CHECK(ae_medal_show_mode(1, 1) == AE_MEDAL_SHOW_PICTURE);
	CHECK(ae_medal_show_mode(1, 0) == AE_MEDAL_SHOW_TEXT);
	CHECK(ae_medal_show_mode(0, 1) == AE_MEDAL_SHOW_PICTURE && ae_medal_show_mode(0, 0) == AE_MEDAL_SHOW_TEXT);
	/* split screen: the message list's line, with a picture or without */
	CHECK(ae_medal_show_mode(2, 1) == AE_MEDAL_SHOW_FEED && ae_medal_show_mode(2, 0) == AE_MEDAL_SHOW_FEED);
	CHECK(ae_medal_show_mode(3, 1) == AE_MEDAL_SHOW_FEED && ae_medal_show_mode(4, 0) == AE_MEDAL_SHOW_FEED);
}

static void ease_test(void)
{
	CHECK(NEAR(ae_medal_quint_out(0.0f), 0.0f) && NEAR(ae_medal_quint_out(1.0f), 1.0f));
	/* quint out: 1 - 0.5^5 at the middle, fast at first */
	CHECK(NEAR(ae_medal_quint_out(0.5f), 1.0f - 1.0f / 32.0f));
	CHECK(ae_medal_quint_out(0.1f) > 0.4f);
	/* held to 0..1 */
	CHECK(NEAR(ae_medal_quint_out(-3.0f), 0.0f) && NEAR(ae_medal_quint_out(7.0f), 1.0f));
}

static void pose_test(void)
{
	float x = -1.0f, y = -1.0f, alpha = -1.0f;

	CHECK(AE_MEDAL_LIFE_TICKS == 180);
	/* before and after: nothing, outputs left alone */
	CHECK(!ae_medal_pose(-1, 0, 0, &x, &y, &alpha) && !ae_medal_pose(180, 0, 0, &x, &y, &alpha) && x == -1.0f && y == -1.0f);
	/* the start: 156 from the holder, clear */
	CHECK(ae_medal_pose(0, 0, 0, &x, &y, &alpha) && NEAR(x, 150.0f + 156.0f) && NEAR(y, 525.0f) && NEAR(alpha, 0.0f));
	/* slot 0 settles at the holder; held 2 s fully shown */
	CHECK(ae_medal_pose(60, 0, 0, &x, &y, &alpha) && NEAR(x, 150.0f) && NEAR(alpha, 1.0f));
	CHECK(ae_medal_pose(119, 0, 0, &x, &y, &alpha) && NEAR(x, 150.0f) && NEAR(alpha, 1.0f));
	/* the fade out starts at 4 s, eases out (quickly at first), and is nearly gone at the end */
	CHECK(ae_medal_pose(120, 0, 0, &x, &y, &alpha) && NEAR(alpha, 1.0f));
	CHECK(ae_medal_pose(150, 0, 0, &x, &y, &alpha) && NEAR(alpha, 1.0f / 32.0f));
	CHECK(ae_medal_pose(135, 0, 0, &x, &y, &alpha) && alpha > 0.2f && alpha < 0.25f);
	CHECK(ae_medal_pose(179, 0, 0, &x, &y, &alpha) && alpha >= 0.0f && alpha < 0.001f);
	/* half way in, quint out: nearly all of the way (1 - 1/32) from 156 */
	CHECK(ae_medal_pose(30, 0, 0, &x, &y, &alpha) && NEAR(alpha, 1.0f - 1.0f / 32.0f) &&
		NEAR(x, 150.0f + 156.0f * (1.0f / 32.0f)));
	/* the slots: left to right, four to a row, then the next row down, 100 apart */
	CHECK(ae_medal_pose(60, 1, 0, &x, &y, &alpha) && NEAR(x, 300.0f) && NEAR(y, 525.0f));
	CHECK(ae_medal_pose(60, 3, 0, &x, &y, &alpha) && NEAR(x, 600.0f) && NEAR(y, 525.0f));
	CHECK(ae_medal_pose(60, 4, 0, &x, &y, &alpha) && NEAR(x, 150.0f) && NEAR(y, 653.0f));
	CHECK(ae_medal_pose(60, 7, 0, &x, &y, &alpha) && NEAR(x, 600.0f) && NEAR(y, 653.0f));
	CHECK(ae_medal_pose(60, 15, 0, &x, &y, &alpha) && NEAR(x, 600.0f) && NEAR(y, 653.0f));
	/* a slot past the sixteenth comes round, so the picture stays in the view's lower part: the 4th row's bottom is 525 + 300 + 80 */
	CHECK(ae_medal_pose(60, 16, 0, &x, &y, &alpha) && NEAR(x, 150.0f) && NEAR(y, 525.0f));
	CHECK(ae_medal_pose(60, 37, 0, &x, &y, &alpha) && NEAR(x, 300.0f) && NEAR(y, 653.0f));
	CHECK(ae_medal_pose(60, -5, 0, &x, &y, &alpha) && NEAR(x, 150.0f) && NEAR(y, 525.0f));
	/* a slot's x moves toward it from the start on either side of 156 */
	CHECK(ae_medal_pose(30, 3, 0, &x, &y, &alpha) && x > 150.0f + 156.0f && x < 600.0f);
	/* text: one column from the holder's left, 40 apart, eight rows */
	CHECK(ae_medal_pose(60, 0, 1, &x, &y, &alpha) && NEAR(x, 150.0f) && NEAR(y, 525.0f));
	CHECK(ae_medal_pose(60, 3, 1, &x, &y, &alpha) && NEAR(x, 150.0f) && NEAR(y, 525.0f + 120.0f));
	CHECK(ae_medal_pose(60, 9, 1, &x, &y, &alpha) && NEAR(x, 150.0f) && NEAR(y, 525.0f + 40.0f));
	CHECK(ae_medal_pose(0, 2, 1, &x, &y, &alpha) && NEAR(x, 150.0f + 156.0f));
	CHECK(NEAR(ae_medal_text_pixels(1.0f), 24.0f) && NEAR(ae_medal_text_pixels(0.5f), 16.0f) &&
		NEAR(ae_medal_text_pixels(2.0f), 48.0f) && NEAR(ae_medal_text_pixels(0.0f), 16.0f));
}

static void slot_test(void)
{
	struct ae_medal_holder holder;
	int i;

	ae_medal_holder_reset(&holder);
	/* the counter goes up for each medal, 0 first */
	CHECK(ae_medal_holder_add(&holder, 10, 100) == 0);
	CHECK(ae_medal_holder_add(&holder, 11, 100) == 1);
	CHECK(ae_medal_holder_add(&holder, 12, 110) == 2);
	CHECK(holder.count == 3 && holder.active == 3);
	/* the first two start their fade at 100 + 120: not counted from then; the third still is */
	ae_medal_holder_step(&holder, 100 + 119);
	CHECK(holder.active == 3);
	ae_medal_holder_step(&holder, 100 + 120);
	CHECK(holder.active == 1 && holder.count == 3);
	/* a new medal while one is still active carries on from the counter */
	CHECK(ae_medal_holder_add(&holder, 13, 100 + 125) == 3);
	CHECK(holder.active == 2);
	/* the first two are gone at 100 + 180 */
	ae_medal_holder_step(&holder, 100 + 179);
	CHECK(holder.count == 4);
	ae_medal_holder_step(&holder, 100 + 180);
	CHECK(holder.count == 2 && holder.shown[0].medal == 12 && holder.shown[1].medal == 13);
	/* none active: the counter is back to 0 for the next */
	ae_medal_holder_step(&holder, 225 + 120 + 5);
	CHECK(holder.active == 0 && holder.count == 1);
	CHECK(ae_medal_holder_add(&holder, 14, 225 + 120 + 6) == 0);
	/* a long wait with no step: all gone, the counter back at 0 */
	ae_medal_holder_step(&holder, 5000);
	CHECK(holder.count == 0 && holder.active == 0);
	/* the clock going back is a new game */
	ae_medal_holder_add(&holder, 1, 6000);
	ae_medal_holder_add(&holder, 2, 6001);
	ae_medal_holder_step(&holder, 10);
	CHECK(holder.count == 0 && holder.active == 0 && holder.last == 10);
	ae_medal_holder_add(&holder, 3, 10);
	CHECK(holder.count == 1 && holder.shown[0].slot == 0);

	/* sixteen at once: the 17th takes the oldest's place, the array never passes its end */
	ae_medal_holder_reset(&holder);
	for (i = 0; i < AE_MEDAL_MOST; i++)
		CHECK(ae_medal_holder_add(&holder, i, 1000) == i);
	CHECK(holder.count == AE_MEDAL_MOST && holder.active == AE_MEDAL_MOST);
	/* full: the oldest goes, and its place is taken */
	CHECK(ae_medal_holder_add(&holder, 99, 1000) == 0);
	CHECK(holder.count == AE_MEDAL_MOST && holder.active == AE_MEDAL_MOST && holder.shown[0].medal == 1 &&
		holder.shown[AE_MEDAL_MOST - 1].medal == 99);
	/* a great many, steadily: the count stays within the array and the active count with it */
	for (i = 0; i < 5000; i++)
	{
		ae_medal_holder_add(&holder, i, 1000 + i / 4);
		CHECK(holder.count >= 0 && holder.count <= AE_MEDAL_MOST && holder.active >= 0 && holder.active <= holder.count);
	}
	/* all faded and removed: it empties, and starts at slot 0 again */
	ae_medal_holder_step(&holder, 1000 + 5000 + 1000);
	CHECK(holder.count == 0 && holder.active == 0);
}

static void caption_test(void)
{
	unsigned char pixels[16 * 4];
	float color[3];
	int i;

	/* a dark blue picture with a grey corner: its blue, made light enough to read */
	for (i = 0; i < 16; i++)
	{
		pixels[i * 4] = 20;
		pixels[i * 4 + 1] = 30;
		pixels[i * 4 + 2] = 160;
		pixels[i * 4 + 3] = 255;
	}
	pixels[0] = pixels[1] = pixels[2] = 128;
	ae_medal_caption_color(pixels, 4, 4, color);
	CHECK(color[2] > color[0] && color[2] > color[1]);
	CHECK(0.299f * color[0] + 0.587f * color[1] + 0.114f * color[2] >= AE_MEDAL_CAPTION_LIGHT);
	/* grey, or clear: white */
	for (i = 0; i < 16; i++)
		pixels[i * 4] = pixels[i * 4 + 1] = pixels[i * 4 + 2] = 128;
	ae_medal_caption_color(pixels, 4, 4, color);
	CHECK(color[0] == 1.0f && color[1] == 1.0f && color[2] == 1.0f);
	ae_medal_caption_color(NULL, 0, 0, color);
	CHECK(color[0] == 1.0f);
	/* the floor in a small view */
	CHECK(ae_medal_caption_pixels(1.0f) == 18.0f && ae_medal_caption_pixels(0.5f) == 16.0f);
}

static void shrink_test(void)
{
	float x = 0.0f, y = 0.0f, alpha = 0.0f;

	/* up to four medals: as they are; a fifth: all pictures 56 across */
	CHECK(NEAR(ae_medal_shrink(0), 1.0f) && NEAR(ae_medal_shrink(4), 1.0f));
	CHECK(NEAR(ae_medal_shrink(5), 0.7f) && NEAR(ae_medal_shrink(8), 0.7f));
	CHECK(NEAR(AE_MEDAL_SHRUNK_PICTURE, AE_MEDAL_PICTURE * ae_medal_shrink(5)));
	/* the shrunk pose's pitches shrink with the factor (the call stays); the holder stays where it is */
	CHECK(ae_medal_pose_shrunk(60, 0, 0, 0.7f, &x, &y, &alpha) && NEAR(x, 150.0f) && NEAR(y, 525.0f));
	CHECK(ae_medal_pose_shrunk(60, 1, 0, 0.7f, &x, &y, &alpha) && NEAR(x, 150.0f + 105.0f) && NEAR(y, 525.0f));
	CHECK(ae_medal_pose_shrunk(60, 4, 0, 0.7f, &x, &y, &alpha) && NEAR(x, 150.0f) && NEAR(y, 525.0f + 89.6f));
	/* the two rows' rule: the 9th comes round to the first row */
	CHECK(ae_medal_pose_shrunk(60, 8, 0, 0.7f, &x, &y, &alpha) && NEAR(y, 525.0f));
	/* a name's line is not shrunk */
	CHECK(ae_medal_pose_shrunk(60, 3, 1, 0.7f, &x, &y, &alpha) && NEAR(y, 525.0f + 3.0f * 40.0f));
	/* the unshrunk pose is the same call with 1 */
	{
		float x2, y2, a2;

		CHECK(ae_medal_pose(60, 5, 0, &x2, &y2, &a2) && ae_medal_pose_shrunk(60, 5, 0, 1.0f, &x, &y, &alpha) && NEAR(x, x2) &&
			NEAR(y, y2) && NEAR(alpha, a2));
	}
	/* past four medals the places keep the full pitch (150 by 128): the 56 pictures sit where 80 ones do, the first place's left
	edge the same, the 8th's at 3 columns and the second row */
	CHECK(ae_medal_pose(60, 7, 0, &x, &y, &alpha) && NEAR(x, 150.0f + 450.0f) && NEAR(y, 525.0f + AE_MEDAL_ROW_PITCH));
	/* the block with two caption lines on the second row ends clear of the sensor's top (812 at 1080) by 8 px or more */
	CHECK(525.0f + AE_MEDAL_ROW_PITCH + AE_MEDAL_SHRUNK_PICTURE + AE_MEDAL_CAPTION_GAP + 0.70f * AE_MEDAL_CAPTION_SIZE +
		AE_MEDAL_CAPTION_LINE_PITCH < 812.0f - 8.0f);
}

/* a stand-in for the text's measure: 10 px a character at 18 px */
static float fake_measure(wchar_t const *text, float px)
{
	return (float)wcslen(text) * 10.0f * px / 18.0f;
}

static void caption_split_test(void)
{
	wchar_t first[64], second[64];

	/* a name that fits (142 px: 14 characters here) is one line, whole */
	CHECK(ae_medal_caption_lines(L"KILLING SPREE", fake_measure, 18.0f, AE_MEDAL_CAPTION_WIDTH, first, second, 64) == 1 &&
		!wcscmp(first, L"KILLING SPREE") && second[0] == 0);
	CHECK(ae_medal_caption_lines(L"GUNNER", fake_measure, 18.0f, AE_MEDAL_CAPTION_WIDTH, first, second, 64) == 1);
	/* too wide: it breaks at the space, two lines */
	CHECK(ae_medal_caption_lines(L"AUTOPILOT ENGAGED", fake_measure, 18.0f, AE_MEDAL_CAPTION_WIDTH, first, second, 64) == 2 &&
		!wcscmp(first, L"AUTOPILOT") && !wcscmp(second, L"ENGAGED"));
	CHECK(ae_medal_caption_lines(L"FLAG CAPTURED", fake_measure, 14.0f, 100.0f, first, second, 64) == 2 &&
		!wcscmp(first, L"FLAG") && !wcscmp(second, L"CAPTURED"));
	/* three words: the break that leaves the narrower wider line */
	CHECK(ae_medal_caption_lines(L"ONE TWO THREE FOUR", fake_measure, 18.0f, 100.0f, first, second, 64) == 2 &&
		!wcscmp(first, L"ONE TWO") && !wcscmp(second, L"THREE FOUR"));
	CHECK(ae_medal_caption_lines(L"A VERY LONG MEDAL NAME", fake_measure, 18.0f, 120.0f, first, second, 64) == 2 &&
		!wcscmp(first, L"A VERY LONG") && !wcscmp(second, L"MEDAL NAME"));
	/* one word wider than the limit has no word to break: one line, as it is */
	CHECK(ae_medal_caption_lines(L"SCATTERGUNNER", fake_measure, 18.0f, 100.0f, first, second, 64) == 1 &&
		!wcscmp(first, L"SCATTERGUNNER"));
	/* the one fixed entry */
	CHECK(ae_medal_caption_lines(L"UNFRIGGENBELIEVABLE", fake_measure, 18.0f, AE_MEDAL_CAPTION_WIDTH, first, second, 64) == 2 &&
		!wcscmp(first, L"UNFRIGGEN") && !wcscmp(second, L"BELIEVABLE"));
	/* no room, or no name: nothing written past it */
	CHECK(ae_medal_caption_lines(L"AUTOPILOT ENGAGED", fake_measure, 18.0f, 100.0f, first, second, 6) == 1);
	CHECK(ae_medal_caption_lines(NULL, fake_measure, 18.0f, 100.0f, first, second, 64) == 0);
	CHECK(ae_medal_caption_lines(L"", fake_measure, 18.0f, 100.0f, first, second, 64) == 0 && first[0] == 0 && second[0] == 0);
}

int main(void)
{
	caption_test();
	mode_test();
	ease_test();
	pose_test();
	slot_test();
	shrink_test();
	caption_split_test();
	if (!failures)
		printf("ae_medal_draw_rules_test: ok\n");
	return failures != 0;
}
