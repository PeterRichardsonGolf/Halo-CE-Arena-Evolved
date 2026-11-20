/* The revive HUD's rules (game/ae_revive_hud_rules.c) */
#include <stdio.h>
#include <wchar.h>

#include "../game/ae_revive_hud_rules.h"

static int failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); failures++; } } while (0)

int main(void)
{
	float from, to;
	wchar_t text[40];
	int index;

	/* the ring: six spans in order, clockwise from the top, none overlapping, the chin's gap kept */
	for (index = 0; index < AE_REVIVE_SEGMENTS; index++)
	{
		CHECK(ae_revive_segment_span(index, &from, &to));
		CHECK(from < to && from >= 0.0f && to <= 360.0f);
	}
	CHECK(!ae_revive_segment_span(-1, &from, &to) && !ae_revive_segment_span(6, &from, &to));
	ae_revive_segment_span(2, &from, &to);
	CHECK(to < 160.0f);
	ae_revive_segment_span(3, &from, &to);
	CHECK(from > 200.0f);

	/* progress fills whole segments: 0 none, a half three, full six */
	CHECK(ae_revive_white_segments(0) == 0 && ae_revive_white_segments(-5) == 0);
	CHECK(ae_revive_white_segments(42) == 0 && ae_revive_white_segments(43) == 1);
	CHECK(ae_revive_white_segments(128) == 3 && ae_revive_white_segments(240) == 5);
	CHECK(ae_revive_white_segments(255) == 6 && ae_revive_white_segments(300) == 6);

	/* bleed-out: all lit at the start, one dark a sixth, the last second still shows one */
	CHECK(ae_revive_bleed_lit(30, 30) == 6 && ae_revive_bleed_lit(31, 30) == 6);
	CHECK(ae_revive_bleed_lit(25, 30) == 5 && ae_revive_bleed_lit(24, 30) == 5);
	CHECK(ae_revive_bleed_lit(15, 30) == 3 && ae_revive_bleed_lit(1, 30) == 1);
	CHECK(ae_revive_bleed_lit(0, 30) == 0 && ae_revive_bleed_lit(5, 0) == 0);
	CHECK(!ae_revive_segment_is_lit(0, 5) && ae_revive_segment_is_lit(1, 5) && ae_revive_segment_is_lit(5, 1));
	CHECK(!ae_revive_segment_is_lit(4, 1) && !ae_revive_segment_is_lit(-1, 6) && !ae_revive_segment_is_lit(6, 6));
	for (index = 0; index < AE_REVIVE_SEGMENTS; index++)
		CHECK(ae_revive_segment_is_lit(index, 6) && !ae_revive_segment_is_lit(index, 0));

	/* distances: a world unit is 3.048 m */
	CHECK(ae_revive_metres(0.0f) == 0 && ae_revive_metres(-3.0f) == 0 && ae_revive_metres(4.0f) == 12);
	CHECK(ae_revive_metres(1.0f) == 3 && ae_revive_metres(1e9f) == 9999);

	/* reach: the host's rule */
	CHECK(ae_revive_in_reach(0.5f, 0.5f, 1.0f) && !ae_revive_in_reach(1.0f, 0.5f, 0.0f) && !ae_revive_in_reach(0.0f, 0.0f, 1.6f));
	CHECK(ae_revive_in_reach(0.0f, 1.0f, -1.5f));

	/* the words */
	ae_revive_bleed_text(9, text, 40);
	CHECK(!wcscmp(text, L"BLEED-OUT 0:09"));
	ae_revive_bleed_text(75, text, 40);
	CHECK(!wcscmp(text, L"BLEED-OUT 1:15"));
	ae_revive_bleed_text(-3, text, 40);
	CHECK(!wcscmp(text, L"BLEED-OUT 0:00"));
	ae_revive_nearest_text(12, text, 40);
	CHECK(!wcscmp(text, L"NEAREST TEAMMATE 12m"));
	ae_revive_distance_text(24, text, 40);
	CHECK(!wcscmp(text, L"24m"));
	ae_revive_bleed_text(9, text, 6);
	CHECK(!wcscmp(text, L"BLEED"));
	CHECK(ae_revive_self_mode(255) == AE_REVIVE_SELF_WAITING && ae_revive_self_mode(3) == AE_REVIVE_SELF_REVIVING);

	if (failures)
		return 1;
	printf("ae_revive_hud_rules_test: ok\n");
	return 0;
}
