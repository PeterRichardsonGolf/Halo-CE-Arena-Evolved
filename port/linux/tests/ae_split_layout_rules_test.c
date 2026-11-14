#include <math.h>
#include <stdio.h>

#include "ae_split_layout_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, e) (fabs((double)(a) - (double)(b)) <= (e))

static double degrees(double radians) { return radians * 180.0 / 3.14159265358979; }
static double radians(double degrees_) { return degrees_ * 3.14159265358979 / 180.0; }

int main(void)
{
	/* 1920x1080 is 853 x 480 of the game's units; 2560x1080 1137 x 480; 5120x1440 (32:9) 1706 x 480 */
	struct ae_split_rect screen = { 0, 0, 853, 480 }, frame = { 63, 36, 790, 444 };
	struct ae_split_rect top = { 0, 0, 853, 240 }, wide_top = { 0, 0, 1137, 240 }, narrow = { 0, 0, 640, 240 };
	struct ae_split_rect left = { 0, 0, 426, 480 }, safe, quads[AE_SPLIT_MAXIMUM_QUADS];
	long columns, rows;
	double fov85_vertical = 2.0 * atan(tan(radians(85.0) * 0.5) * 9.0 / 16.0);
	float factor, widened;
	int count;

	/* the setting's text */
	CHECK(ae_split_two_players_value("auto") == AE_SPLIT_TWO_AUTO);
	CHECK(ae_split_two_players_value("top_and_bottom") == AE_SPLIT_TWO_TOP_AND_BOTTOM);
	CHECK(ae_split_two_players_value("side_by_side") == AE_SPLIT_TWO_SIDE_BY_SIDE);
	CHECK(ae_split_two_players_value("sideways") == AE_SPLIT_TWO_AUTO);
	CHECK(ae_split_two_players_value(NULL) == AE_SPLIT_TWO_AUTO);

	/* AUTO: side by side from 2.2:1 (16:9 and 16:10 top and bottom; 2560x1080 and 3440x1440 side by side) */
	CHECK(!ae_split_side_by_side(AE_SPLIT_TWO_AUTO, 853, 480));
	CHECK(!ae_split_side_by_side(AE_SPLIT_TWO_AUTO, 768, 480));
	CHECK(ae_split_side_by_side(AE_SPLIT_TWO_AUTO, 1137, 480));
	CHECK(ae_split_side_by_side(AE_SPLIT_TWO_AUTO, 3440, 1440));
	CHECK(ae_split_side_by_side(AE_SPLIT_TWO_AUTO, 1056, 480));
	CHECK(!ae_split_side_by_side(AE_SPLIT_TWO_AUTO, 1055, 480));
	CHECK(!ae_split_side_by_side(AE_SPLIT_TWO_AUTO, 0, 0));
	/* the fixed values whatever the screen */
	CHECK(ae_split_side_by_side(AE_SPLIT_TWO_SIDE_BY_SIDE, 640, 480));
	CHECK(!ae_split_side_by_side(AE_SPLIT_TWO_TOP_AND_BOTTOM, 1706, 480));

	/* the counts: two side by side are 2 x 1; two top and bottom, three and four stay the game's */
	columns = 1, rows = 2;
	ae_split_counts(2, 1, &columns, &rows);
	CHECK(columns == 2 && rows == 1);
	columns = 1, rows = 2;
	ae_split_counts(2, 0, &columns, &rows);
	CHECK(columns == 1 && rows == 2);
	columns = 2, rows = 2;
	ae_split_counts(3, 1, &columns, &rows);
	CHECK(columns == 2 && rows == 2);
	ae_split_counts(4, 1, &columns, &rows);
	CHECK(columns == 2 && rows == 2);

	/* SIDE BARS: 16:9's views 640 wide, centred (106 units = 240 px each side at 1080p); 21:9's 640 too (248 units,
	558 px); a view already no wider than 8:3 stays */
	CHECK(ae_split_bars_narrow(&top));
	CHECK(top.x0 == 106 && top.x1 == 746 && top.y0 == 0 && top.y1 == 240);
	CHECK(ae_split_bars_narrow(&wide_top));
	CHECK(wide_top.x0 == 248 && wide_top.x1 == 888);
	CHECK(!ae_split_bars_narrow(&narrow));
	CHECK(narrow.x0 == 0 && narrow.x1 == 640);

	/* the safe frames: a narrowed view's is a 4:3 screen's half view's (48 x 18 units in); a side-by-side view's
	takes a quarter view's height: 32 x 18 units, as a four-player view's */
	CHECK(ae_split_safe_frame(&top, 240, &safe));
	CHECK(safe.x0 == 154 && safe.x1 == 698 && safe.y0 == 18 && safe.y1 == 222);
	CHECK(ae_split_safe_frame(&left, 240, &safe));
	CHECK(safe.x0 == 32 && safe.x1 == 394 && safe.y0 == 18 && safe.y1 == 462);
	/* (centred: the reticle at the view's middle) */
	CHECK(safe.x0 + safe.x1 == left.x0 + left.x1 && safe.y0 + safe.y1 == left.y0 + left.y1);
	{
		struct ae_split_rect tiny = { 5, 5, 5, 6 };

		safe.x0 = -7;
		CHECK(!ae_split_safe_frame(&tiny, 240, &safe));
		CHECK(safe.x0 == -7);
	}

	/* the quads: side by side's divider down the middle; top and bottom's across, with the two bars */
	count = ae_split_quads(&screen, &frame, 1, NULL, quads);
	CHECK(count == 1);
	CHECK(quads[0].x0 == 425 && quads[0].x1 == 427 && quads[0].y0 == 0 && quads[0].y1 == 480);
	count = ae_split_quads(&screen, &frame, 0, NULL, quads);
	CHECK(count == 1);
	CHECK(quads[0].x0 == 0 && quads[0].x1 == 853 && quads[0].y0 == 239 && quads[0].y1 == 241);
	count = ae_split_quads(&screen, &frame, 0, &top, quads);
	CHECK(count == 3);
	CHECK(quads[1].x0 == 0 && quads[1].x1 == 106 && quads[1].y0 == 0 && quads[1].y1 == 480);
	CHECK(quads[2].x0 == 746 && quads[2].x1 == 853 && quads[2].y0 == 0 && quads[2].y1 == 480);

	/* the field of view: the angle spans a view's window (the title-safe frame on one player's screen, 408 of 480
	lines); at display.fov 85, 16:9's vertical angle over that window is 54.54 degrees, and one player's whole screen
	shows 94.3 degrees across (tangent 1.0779) */
	CHECK(NEAR(degrees(fov85_vertical), 54.54, 0.01));
	CHECK(NEAR(tan(fov85_vertical * 0.5) * (480.0 * 16.0 / 9.0) / 408.0, 1.0779, 0.0005));
	/* a side-by-side view takes the middle ground: its tangent across is the geometric mean of the game's rule's (one
	player's vertical angle kept) and one player's width's, at FOV 70, 85 and 110, at 1920x1080 (426 x 480 units a
	view, its window 444: a quarter view's insets), 2560x1080 (568) and 32:9's halves (16:9) */
	{
		static float const fovs[3] = { 70.0f, 85.0f, 110.0f };
		static float const widths[3] = { 426.0f, 568.0f, 853.33f };
		/* (degrees across and top to bottom, [fov][width]) */
		static double const across[3][3] = { { 60.40, 67.81, 78.96 }, { 74.59, 82.66, 94.30 }, { 99.78, 107.78, 118.48 } };
		static double const upright[3][3] = { { 66.52, 59.19, 49.72 }, { 81.28, 73.24, 62.46 }, { 106.44, 98.40, 86.77 } };
		int f, w;

		for (f = 0; f < 3; f++)
		{
			double one = tan(radians(fovs[f]) * 0.5) * 9.0 / 16.0;
			double one_up = one * 480.0 / 408.0, one_across = one_up * 16.0 / 9.0;

			for (w = 0; w < 3; w++)
			{
				double t, narrow, wide;

				factor = ae_split_fov_tangent_factor(widths[w], 480.0f, 444.0f);
				widened = ae_split_fov_widen((float)(2.0 * atan(one)), factor);
				t = tan(widened * 0.5);
				narrow = one_up * widths[w] / 480.0;
				wide = one_across;
				CHECK(NEAR(t * widths[w] / 444.0, sqrt(narrow * wide), 0.0005));
				CHECK(NEAR(2.0 * degrees(atan(t * widths[w] / 444.0)), across[f][w], 0.02));
				CHECK(NEAR(2.0 * degrees(atan(t * 480.0 / 444.0)), upright[f][w], 0.02));
			}
		}
	}
	/* (32:9's halves are 16:9: one player's view, both rules agree) */
	CHECK(NEAR(ae_split_fov_tangent_factor(853.33f, 480.0f, 444.0f), ae_split_fov_upright_factor(480.0f, 444.0f),
		0.0005));
	CHECK(NEAR(ae_split_fov_upright_factor(480.0f, 444.0f), 444.0 / 408.0, 0.0001));
	/* a view that is no side-by-side view: no change */
	CHECK(ae_split_fov_tangent_factor(0.0f, 480.0f, 444.0f) == 1.0f);
	CHECK(ae_split_fov_tangent_factor(426.0f, 480.0f, 0.0f) == 1.0f);
	CHECK(ae_split_fov_widen(0.0f, 2.0f) == 0.0f);
	/* a scope's narrow angle widens by the same factor */
	factor = ae_split_fov_tangent_factor(426.0f, 480.0f, 444.0f);
	widened = ae_split_fov_widen((float)radians(10.0), factor);
	CHECK(NEAR(tan(widened * 0.5) / tan(radians(10.0) * 0.5), factor, 0.0005));

	/* the first-person weapon's band: one player's whole screen, sized to keep one player's weapon against the world
	(the view's height x one player's vertical tangent / the view's), centred across, on the view's foot */
	{
		struct ae_split_rect band;
		float bx0, bx1, by0, by1, up = 480.0f / 408.0f, band_height, scale;

		band_height = ae_split_viewmodel_band_height(480.0f, ae_split_fov_upright_factor(480.0f, 444.0f),
			ae_split_fov_tangent_factor(426.0f, 480.0f, 444.0f));
		CHECK(NEAR(band_height, 339.1, 0.1));
		/* (1080p: 763 x 1357 px for a 958 x 1080 view: 200 px off each side) */
		ae_split_viewmodel_band(426.0f, 480.0f, band_height, &band);
		CHECK(band.y1 == 480 && band.y0 == 141 && band.x0 == -88 && band.x1 == 514);
		/* the weapon against the world: the band's pixels per tangent are the world's, as in one player */
		factor = ae_split_fov_tangent_factor(426.0f, 480.0f, 444.0f);
		scale = (band_height / 2.0f) / up;
		CHECK(NEAR(scale, (480.0f / 2.0f) / (factor * 480.0f / 444.0f), 0.05));
		/* the bounds: one player's screen's tangents over the band, from the view's foot */
		ae_split_viewmodel_bounds(426.0f, 480.0f, band_height, &bx0, &bx1, &by0, &by1);
		CHECK(NEAR(by0, -up, 0.0001));
		CHECK(NEAR(by0 + (by1 - by0) * band_height / 480.0f, up, 0.0005));
		/* (across: the band's half width is one player's 2.0915; the view shows 426 of its 602.9) */
		CHECK(NEAR(bx1 * 426.0 / 480.0, 2.0915 * 426.0 / (band_height * 16.0 / 9.0), 0.0005) && NEAR(bx0, -bx1, 0.00001));
		/* the weapon's middle (its tangent 0) is the band's: 159 px under the view's middle at 1080p */
		CHECK(NEAR(1080.0 * by1 / (by1 - by0) - 540.0, 540.0 - 1080.0 * band_height / 480.0 / 2.0, 0.6));
		CHECK(NEAR(1080.0 * by1 / (by1 - by0) - 540.0, 158.5, 1.0));
		/* 2560x1080: the same rule */
		band_height = ae_split_viewmodel_band_height(480.0f, ae_split_fov_upright_factor(480.0f, 444.0f),
			ae_split_fov_tangent_factor(568.0f, 480.0f, 444.0f));
		CHECK(NEAR(band_height, 391.6, 0.1));
		/* a 16:9 view: the band is the whole view, one player's bounds exactly */
		band_height = ae_split_viewmodel_band_height(480.0f, ae_split_fov_upright_factor(480.0f, 444.0f),
			ae_split_fov_tangent_factor(853.33f, 480.0f, 444.0f));
		ae_split_viewmodel_bounds(853.33f, 480.0f, band_height, &bx0, &bx1, &by0, &by1);
		CHECK(NEAR(by1, up, 0.001) && NEAR(bx1, 16.0 / 9.0 * up * 480.0 / 853.33, 0.001));
		/* no view: the full bounds */
		ae_split_viewmodel_bounds(0.0f, 480.0f, 300.0f, &bx0, &bx1, &by0, &by1);
		CHECK(bx0 == -1.0f && bx1 == 1.0f && by0 == -1.0f && by1 == 1.0f);
	}

	/* the weapon's markers: a point by the weapon moves so the world's projection puts it where the weapon's draws it
	(1080p side by side at FOV 85, one player's weapon angle 54.54 degrees: the band's bounds against the view's) */
	{
		struct ae_split_view_map map;
		float world[4], weapon[4], world_t, weapon_t, aspect = 426.0f / 480.0f, band_height;
		static float const points[4][3] = { { 0.0f, 0.0f, 1.0f }, { 0.12f, -0.2f, 0.35f }, { -0.05f, -0.4f, 0.6f },
			{ 0.3f, 0.1f, 0.5f } };
		int p;

		factor = ae_split_fov_tangent_factor(426.0f, 480.0f, 444.0f);
		world_t = (float)tan(fov85_vertical * 0.5) * factor;
		weapon_t = (float)tan(fov85_vertical * 0.5);
		/* (the world's bounds: the 426 x 480 view over its 444-line window) */
		world[0] = -480.0f / 444.0f;
		world[1] = 480.0f / 444.0f;
		world[2] = -480.0f / 444.0f;
		world[3] = 480.0f / 444.0f;
		band_height = ae_split_viewmodel_band_height(480.0f, ae_split_fov_upright_factor(480.0f, 444.0f), factor);
		ae_split_viewmodel_bounds(426.0f, 480.0f, band_height, &weapon[0], &weapon[1], &weapon[2], &weapon[3]);
		ae_split_viewmodel_map(aspect, world_t, world, weapon_t, weapon, &map);
		/* (across: both centred, a pure scale; up: the band's offset) */
		CHECK(NEAR(map.cx, 0.0, 0.00001));
		CHECK(map.cy < 0.0f);
		for (p = 0; p < 4; p++)
		{
			double x = points[p][0], y = points[p][1], z = points[p][2];
			double x2 = map.ax * x + map.cx * z, y2 = map.ay * y + map.cy * z;
			/* (the viewport place, 0 to 1, under each projection) */
			double weapon_u = (x / z / (aspect * weapon_t) - weapon[0]) / (weapon[1] - weapon[0]);
			double weapon_v = (y / z / weapon_t - weapon[2]) / (weapon[3] - weapon[2]);
			double world_u = (x2 / z / (aspect * world_t) - world[0]) / (world[1] - world[0]);
			double world_v = (y2 / z / world_t - world[2]) / (world[3] - world[2]);

			CHECK(NEAR(weapon_u, world_u, 0.00001) && NEAR(weapon_v, world_v, 0.00001));
		}
		/* (the weapon's middle, straight ahead, lands at the band's middle: 159 px under the view's at 1080p) */
		CHECK(NEAR(1080.0 * (1.0 - (map.cy / world_t - world[2]) / (world[3] - world[2])) - 540.0, 158.5, 1.0));
		/* bad input: no move */
		ae_split_viewmodel_map(aspect, 0.0f, world, weapon_t, weapon, &map);
		CHECK(map.ax == 1.0f && map.ay == 1.0f && map.cx == 0.0f && map.cy == 0.0f);
	}

	/* the HUD's share: side by side a quarter, side bars the half they were, else the view's own */
	CHECK(ae_split_hud_share_factor(1, 0, 853.0f, 426.0f) == 0.5f);
	CHECK(NEAR(ae_split_hud_share_factor(0, 1, 853.0f, 640.0f), 853.0 / 640.0, 0.0001));
	CHECK(ae_split_hud_share_factor(0, 0, 853.0f, 853.0f) == 1.0f);
	CHECK(ae_split_hud_share_factor(0, 1, 640.0f, 640.0f) == 1.0f);
	/* (the score strip's view factor, 0.5 + 0.5 sqrt(share): a side-by-side view's 0.75, a four-player view's) */
	CHECK(NEAR(0.5 + 0.5 * sqrt(0.5 * (426.0 * 480.0) / (853.0 * 480.0)), 0.75, 0.001));
	/* (and a narrowed view's 0.85, a top-and-bottom view's) */
	CHECK(NEAR(0.5 + 0.5 * sqrt((853.0 / 640.0) * (640.0 * 240.0) / (853.0 * 480.0)), 0.8536, 0.001));

	if (failures)
		return 1;
	printf("ae_split_layout_rules: ok\n");
	return 0;
}
