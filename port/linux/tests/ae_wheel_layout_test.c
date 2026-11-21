#include <math.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "ae_wheel_layout.h"
#include "ae_wheel_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b, tolerance) do { double x_ = (a), y_ = (b); if (fabs(x_ - y_) > (tolerance)) { \
	printf("FAIL %s:%d %s = %f, wanted %f\n", __FILE__, __LINE__, #a, x_, y_); failures++; } } while (0)

#define DEGREES_TO_RADIANS 0.017453292f

static float direction_x(int entry)
{
	return sinf((float)entry * 45.0f * DEGREES_TO_RADIANS);
}

static float direction_y(int entry)
{
	return -cosf((float)entry * 45.0f * DEGREES_TO_RADIANS);
}

/* a point's distance from a segment */
static float segment_distance(float px, float py, float ax, float ay, float bx, float by)
{
	float dx = bx - ax, dy = by - ay;
	float t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy);

	t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
	return hypotf(px - (ax + dx * t), py - (ay + dy * t));
}

/* the least distance between two outlines */
static float outline_distance(float const (*a)[2], int a_count, float const (*b)[2], int b_count)
{
	float least = 1.0e9f;
	int i, j;

	for (i = 0; i < a_count; i++)
	{
		for (j = 0; j < b_count; j++)
		{
			float d = segment_distance(a[i][0], a[i][1], b[j][0], b[j][1], b[(j + 1) % b_count][0],
				b[(j + 1) % b_count][1]);
			float e = segment_distance(b[j][0], b[j][1], a[i][0], a[i][1], a[(i + 1) % a_count][0],
				a[(i + 1) % a_count][1]);

			least = fminf(least, fminf(d, e));
		}
	}
	return least;
}

static void one_view_test(void)
{
	struct ae_wheel_layout layout;

	ae_wheel_layout_get(0, 1080.0f, &layout);
	CHECK(!layout.split);
	NEAR(layout.scale, 1.0f, 0.0001);
	NEAR(layout.inner, 104.0f, 0.0001);
	NEAR(layout.outer, 224.0f, 0.0001);
	NEAR(layout.gap, 12.0f, 0.0001);
	NEAR(layout.corner, 5.0f, 0.0001);
	NEAR(layout.grow, 10.0f, 0.0001);
	NEAR(layout.icon, 44.0f, 0.0001);
	NEAR(layout.ride, 60.0f, 0.0001);
	NEAR(layout.sprite_width, 62.0f, 0.0001);
	NEAR(layout.sprite_height, 24.0f, 0.0001);
	NEAR(layout.rifle_boost, 2.5f, 0.0001);
	NEAR(layout.caption_px, 17.0f, 0.0001);
	NEAR(layout.wedge_tip, 90.0f, 0.0001);
	NEAR(layout.wedge_base, 76.0f, 0.0001);
	NEAR(layout.wedge_half, 11.0f, 0.0001);
	NEAR(layout.wedge_notch, 5.0f, 0.0001);
	NEAR(layout.word_height, 40.0f, 0.0001);
	NEAR(layout.word_px, 22.0f, 0.0001);
	NEAR(layout.word_past, 12.0f, 0.0001);
	NEAR(layout.word_pad, 44.0f, 0.0001);
	CHECK(!layout.word_above);
	NEAR(layout.line, 2.0f, 0.0001);
	NEAR(layout.keyline, 4.0f, 0.0001);
	CHECK(layout.legend);
	NEAR(layout.legend_y, 322.0f, 0.0001);
	NEAR(layout.legend_px, 17.0f, 0.0001);
	NEAR(layout.chip_height, 24.0f, 0.0001);
	NEAR(layout.chip_gap, 8.0f, 0.0001);
	NEAR(layout.legend_gap, 22.0f, 0.0001);
	NEAR(layout.died_y, 152.0f, 0.0001);
	NEAR(layout.died_px, 17.0f, 0.0001);
	NEAR(ae_wheel_outer_radius(0, 1080.0f), 234.0f, 0.0001);
}

static void split_test(void)
{
	struct ae_wheel_layout layout;

	/* (a split view of 540 lines, on a 1080 screen) */
	ae_wheel_layout_get(1, 540.0f, &layout);
	CHECK(layout.split);
	NEAR(layout.scale, 0.68f, 0.0001);
	NEAR(layout.inner, 70.7f, 0.05);
	NEAR(layout.outer, 152.3f, 0.05);
	NEAR(layout.gap, 9.0f, 0.0001);
	NEAR(layout.corner, 3.4f, 0.0001);
	NEAR(layout.grow, 4.1f, 0.05);
	NEAR(layout.icon, 30.0f, 0.0001);
	NEAR(layout.ride, 43.0f, 0.0001);
	/* (the sprite does not shrink with the ring: 62 wide, 24 tall) */
	NEAR(layout.sprite_width, 62.0f, 0.0001);
	NEAR(layout.sprite_height, 24.0f, 0.0001);
	NEAR(layout.rifle_boost, 1.4f, 0.0001);
	NEAR(layout.caption_px, 16.0f, 0.0001);
	NEAR(layout.wedge_tip, 62.0f, 0.0001);
	NEAR(layout.wedge_base, 52.5f, 0.0001);
	NEAR(layout.wedge_half, 7.5f, 0.0001);
	NEAR(layout.word_height, 28.0f, 0.0001);
	NEAR(layout.word_px, 16.0f, 0.0001);
	NEAR(layout.word_past, 8.2f, 0.05);
	CHECK(layout.word_above);
	NEAR(layout.word_nudge, 1.0f, 0.0001);
	NEAR(layout.line, 1.5f, 0.0001);
	NEAR(layout.keyline, 3.0f, 0.0001);
	CHECK(!layout.legend);
}

static void height_test(void)
{
	struct ae_wheel_layout one, tall, split, split_tall, wide;

	ae_wheel_layout_get(0, 1080.0f, &one);
	ae_wheel_layout_get(0, 1440.0f, &tall);
	NEAR(tall.scale, 1440.0f / 1080.0f, 0.0001);
	NEAR(tall.inner, one.inner * 4.0f / 3.0f, 0.0001);
	NEAR(tall.outer, one.outer * 4.0f / 3.0f, 0.0001);
	NEAR(tall.gap, 16.0f, 0.0001);
	NEAR(tall.icon, 59.0f, 0.0001);
	NEAR(tall.caption_px, 17.0f * 4.0f / 3.0f, 0.0001);
	NEAR(tall.legend_y, 322.0f * 4.0f / 3.0f, 0.0001);
	NEAR(ae_wheel_outer_radius(0, 1440.0f), 234.0f * 4.0f / 3.0f, 0.0001);
	/* (a split view of 720 lines on a 1440 screen) */
	ae_wheel_layout_get(1, 540.0f, &split);
	ae_wheel_layout_get(1, 720.0f, &split_tall);
	NEAR(split_tall.scale, 0.68f * 4.0f / 3.0f, 0.0001);
	NEAR(split_tall.outer, split.outer * 4.0f / 3.0f, 0.0001);
	NEAR(split_tall.gap, 12.0f, 0.0001);
	NEAR(split_tall.sprite_width, 62.0f * 4.0f / 3.0f, 0.5);
	/* (the floors are screen pixels: 17 x 0.68 x 4/3 is 15.4, under the caption's 16; the word's cell 40 x 0.907 is over
	its 28) */
	NEAR(split_tall.caption_px, 16.0f, 0.0001);
	NEAR(split_tall.word_height, 40.0f * 0.68f * 4.0f / 3.0f, 0.0001);
	/* width changes nothing: the layout has none, and the ring's centre is the view's middle, whole pixels */
	{
		float x, y, x2, y2;

		ae_wheel_centre(1920.0f, 1080.0f, &x, &y);
		NEAR(x, 960.0f, 0.0001);
		NEAR(y, 540.0f, 0.0001);
		ae_wheel_centre(2560.0f, 1080.0f, &x2, &y2);
		NEAR(x2, 1280.0f, 0.0001);
		NEAR(y2, 540.0f, 0.0001);
		ae_wheel_centre(1921.0f, 1081.0f, &x, &y);
		NEAR(x, floorf(x), 0.0);
		NEAR(y, floorf(y), 0.0);
		NEAR(x, 960.0f, 0.0001);
		NEAR(y, 540.0f, 0.0001);
		ae_wheel_layout_get(0, 1080.0f, &wide);
		CHECK(!memcmp(&wide, &one, sizeof(wide)));
	}
}

static void sector_shape_test(void)
{
	struct ae_wheel_layout layout;
	float points[AE_WHEEL_SECTOR_POINTS][2], next[AE_WHEEL_SECTOR_POINTS][2];
	int split, entry;

	for (split = 0; split < 2; split++)
	{
		ae_wheel_layout_get(split, split ? 540.0f : 1080.0f, &layout);
		for (entry = 0; entry < AE_WHEEL_COUNT; entry++)
		{
			int count = ae_wheel_sector_polygon(&layout, entry, 0, points);
			int grown_count, i, other;
			float grown[AE_WHEEL_SECTOR_POINTS][2];
			float least = 1.0e9f, most = 0.0f;

			CHECK(count >= 8 && count <= AE_WHEEL_SECTOR_POINTS);
			for (i = 0; i < count; i++)
			{
				float r = hypotf(points[i][0], points[i][1]);

				least = fminf(least, r);
				most = fmaxf(most, r);
			}
			/* (the cut corners keep the extremes to the arcs' middles: the inner and outer radii exactly) */
			NEAR(least, layout.inner, 0.05);
			NEAR(most, layout.outer, 0.05);
			/* (mirrored about its own axis: the points' mean across the axis is 0) */
			{
				float across = 0.0f, along = 0.0f;

				for (i = 0; i < count; i++)
				{
					across += points[i][0] * direction_y(entry) * -1.0f + points[i][1] * direction_x(entry);
					along += points[i][0] * direction_x(entry) + points[i][1] * direction_y(entry);
				}
				/* (the arcs are sampled evenly, so each side balances) */
				NEAR(across / (float)count, 0.0f, 0.02);
				CHECK(along / (float)count > layout.inner && along / (float)count < layout.outer);
			}
			/* (the focused one grows its outer arc by grow, nothing else) */
			grown_count = ae_wheel_sector_polygon(&layout, entry, 1, grown);
			CHECK(grown_count == count);
			most = 0.0f;
			least = 1.0e9f;
			for (i = 0; i < grown_count; i++)
			{
				float r = hypotf(grown[i][0], grown[i][1]);

				least = fminf(least, r);
				most = fmaxf(most, r);
			}
			NEAR(least, layout.inner, 0.05);
			NEAR(most, layout.outer + layout.grow, 0.05);
			/* (the gap: parallel from the hub to the rim, ink to ink the table's: every sector clears its neighbour by
			the gap, less nothing, at the closest) */
			other = (entry + 1) % AE_WHEEL_COUNT;
			{
				int next_count = ae_wheel_sector_polygon(&layout, other, 0, next);

				NEAR(outline_distance((float const (*)[2])points, count, (float const (*)[2])next, next_count),
					layout.gap, 0.05);
			}
		}
	}
}

/* the sides are parallel to the boundary between two sectors, a half gap from it: the two corners of a side, one at the
outer cut and one at the inner, are the same distance from the boundary's line */
static void sector_gap_parallel_test(void)
{
	struct ae_wheel_layout layout;
	float points[AE_WHEEL_SECTOR_POINTS][2];
	float boundary_x = sinf(22.5f * DEGREES_TO_RADIANS), boundary_y = -cosf(22.5f * DEGREES_TO_RADIANS);
	int split, count;

	for (split = 0; split < 2; split++)
	{
		ae_wheel_layout_get(split, split ? 540.0f : 1080.0f, &layout);
		count = ae_wheel_sector_polygon(&layout, AE_WHEEL_SMART, 0, points);
		/* (the outer arc's 21 points, then the right side's outer corner and inner corner) */
		CHECK(count == 38);
		{
			float outer_corner = fabsf(points[21][0] * boundary_y - points[21][1] * boundary_x);
			float inner_corner = fabsf(points[22][0] * boundary_y - points[22][1] * boundary_x);

			NEAR(outer_corner, layout.gap / 2.0f, 0.001);
			NEAR(inner_corner, layout.gap / 2.0f, 0.001);
			/* (the corners are the cut's: the outer one cut short of the rim, the inner one past the hub) */
			NEAR(hypotf(points[21][0], points[21][1]), layout.outer - layout.corner, 0.001);
			NEAR(hypotf(points[22][0], points[22][1]), layout.inner + layout.corner, 0.001);
		}
	}
}

static void contains_test(void)
{
	struct ae_wheel_layout layout;
	int entry;

	ae_wheel_layout_get(0, 1080.0f, &layout);
	for (entry = 0; entry < AE_WHEEL_COUNT; entry++)
	{
		float middle = (layout.inner + layout.outer) / 2.0f;
		float x = direction_x(entry) * middle, y = direction_y(entry) * middle;

		CHECK(ae_wheel_sector_contains(&layout, entry, 0, x, y));
		CHECK(!ae_wheel_sector_contains(&layout, (entry + 1) % AE_WHEEL_COUNT, 0, x, y));
		/* (the hub is open, and past the rim is outside) */
		CHECK(!ae_wheel_sector_contains(&layout, entry, 0, direction_x(entry) * (layout.inner - 2.0f),
			direction_y(entry) * (layout.inner - 2.0f)));
		CHECK(!ae_wheel_sector_contains(&layout, entry, 0, direction_x(entry) * (layout.outer + 2.0f),
			direction_y(entry) * (layout.outer + 2.0f)));
		/* (the focused one reaches further) */
		CHECK(!ae_wheel_sector_contains(&layout, entry, 0, direction_x(entry) * (layout.outer + 5.0f),
			direction_y(entry) * (layout.outer + 5.0f)));
		CHECK(ae_wheel_sector_contains(&layout, entry, 1, direction_x(entry) * (layout.outer + 5.0f),
			direction_y(entry) * (layout.outer + 5.0f)));
		/* (the gap between two sectors is in neither) */
		{
			float boundary_x = sinf(((float)entry * 45.0f + 22.5f) * DEGREES_TO_RADIANS);
			float boundary_y = -cosf(((float)entry * 45.0f + 22.5f) * DEGREES_TO_RADIANS);

			CHECK(!ae_wheel_sector_contains(&layout, entry, 0, boundary_x * middle, boundary_y * middle));
			CHECK(!ae_wheel_sector_contains(&layout, (entry + 1) % AE_WHEEL_COUNT, 0, boundary_x * middle,
				boundary_y * middle));
		}
	}
}

/* the fan the soft fill draws, from the corners' centroid, must see every corner: each triangle the same way round */
static int fan_ok(float const (*points)[2], int count)
{
	float cx = 0.0f, cy = 0.0f, sign = 0.0f;
	int i;

	for (i = 0; i < count; i++)
	{
		cx += points[i][0];
		cy += points[i][1];
	}
	cx /= (float)count;
	cy /= (float)count;
	for (i = 0; i < count; i++)
	{
		float ax = points[i][0] - cx, ay = points[i][1] - cy;
		float bx = points[(i + 1) % count][0] - cx, by = points[(i + 1) % count][1] - cy;
		float cross = ax * by - ay * bx;

		if (sign == 0.0f)
			sign = cross;
		if (cross * sign <= 0.0f)
			return 0;
	}
	return 1;
}

static void fan_test(void)
{
	struct ae_wheel_layout layout;
	float points[AE_WHEEL_SECTOR_POINTS][2], wedge[AE_WHEEL_WEDGE_POINTS][2];
	int split, entry;

	for (split = 0; split < 2; split++)
	{
		ae_wheel_layout_get(split, split ? 540.0f : 1080.0f, &layout);
		for (entry = 0; entry < AE_WHEEL_COUNT; entry++)
		{
			int count = ae_wheel_sector_polygon(&layout, entry, 0, points);

			CHECK(fan_ok((float const (*)[2])points, count));
			count = ae_wheel_sector_polygon(&layout, entry, 1, points);
			CHECK(fan_ok((float const (*)[2])points, count));
			CHECK(ae_wheel_wedge_polygon(&layout, entry, wedge) == AE_WHEEL_WEDGE_POINTS);
			CHECK(fan_ok((float const (*)[2])wedge, AE_WHEEL_WEDGE_POINTS));
		}
	}
}

static void wedge_test(void)
{
	struct ae_wheel_layout layout;
	float wedge[AE_WHEEL_WEDGE_POINTS][2];
	int entry;

	ae_wheel_layout_get(0, 1080.0f, &layout);
	ae_wheel_wedge_polygon(&layout, AE_WHEEL_ENEMY, wedge);
	/* (pointing right: the tip at 90, the wings at 76 and 11 either side, the notch at 81; the sides' middles between) */
	NEAR(wedge[0][0], 90.0f, 0.001);
	NEAR(wedge[0][1], 0.0f, 0.001);
	NEAR(wedge[2][0], 76.0f, 0.001);
	NEAR(fabsf(wedge[2][1]), 11.0f, 0.001);
	NEAR(wedge[3][0], 81.0f, 0.001);
	NEAR(wedge[3][1], 0.0f, 0.001);
	NEAR(wedge[4][0], 76.0f, 0.001);
	NEAR(wedge[4][1], -wedge[2][1], 0.001);
	NEAR(wedge[1][0], 83.0f, 0.001);
	NEAR(wedge[5][0], 83.0f, 0.001);
	for (entry = 0; entry < AE_WHEEL_COUNT; entry++)
	{
		ae_wheel_wedge_polygon(&layout, entry, wedge);
		NEAR(hypotf(wedge[0][0], wedge[0][1]), 90.0f, 0.001);
		NEAR(wedge[0][0] * direction_x(entry) + wedge[0][1] * direction_y(entry), 90.0f, 0.001);
	}
	/* (clear of the hub: its tip well inside the inner radius) */
	CHECK(layout.wedge_tip < layout.inner);
	ae_wheel_layout_get(1, 540.0f, &layout);
	ae_wheel_wedge_polygon(&layout, AE_WHEEL_SMART, wedge);
	NEAR(wedge[0][1], -62.0f, 0.001);
	NEAR(wedge[3][1], -(52.5f + 3.4f), 0.001);
}

static void split_offset_test(void)
{
	static int const wanted[AE_WHEEL_COUNT][2] =
	{
		{ 0, 6 },		/* SMART */
		{ -1, 8 },		/* ATTACK */
		{ 2, 13 },		/* ENEMY */
		{ 5, -7 },		/* WEAPON */
		{ 0, 18 },		/* DEFEND */
		{ 1, 19 },		/* RIDE */
		{ -3, 14 },		/* GOING */
		{ 3, 8 },		/* WATCH */
	};
	int entry;

	for (entry = 0; entry < AE_WHEEL_COUNT; entry++)
	{
		int x = 99, y = 99;

		ae_wheel_split_offset(entry, &x, &y);
		CHECK(x == wanted[entry][0]);
		CHECK(y == wanted[entry][1]);
	}
}

static void place_test(void)
{
	struct ae_wheel_layout layout;
	struct ae_wheel_place place;
	int entry;

	/* one view: the icon 11 above the sector's middle, the caption's middle 26 under it, both on the sector's axis */
	ae_wheel_layout_get(0, 1080.0f, &layout);
	for (entry = 0; entry < AE_WHEEL_COUNT; entry++)
	{
		ae_wheel_entry_place(&layout, entry, &place);
		NEAR(place.middle_x, direction_x(entry) * 164.0f, 0.001);
		NEAR(place.middle_y, direction_y(entry) * 164.0f, 0.001);
		NEAR(place.caption_y, roundf(place.middle_y + 26.0f), 0.001);
		NEAR(place.caption_x, roundf(place.middle_x), 0.001);
		CHECK(!place.caption_over_sprite);
		if (entry != AE_WHEEL_RIDE)
		{
			NEAR(place.icon_x, roundf(place.middle_x), 0.001);
			NEAR(place.icon_y, roundf(place.middle_y - 11.0f), 0.001);
		}
	}
	/* (the steering wheel's picture, about 54 of its 60 square, sits 14.5 over the caption's middle) */
	ae_wheel_entry_place(&layout, AE_WHEEL_RIDE, &place);
	NEAR(place.icon_y, roundf(place.caption_y - 14.5f - 27.0f), 0.001);
	/* (SMART at the top: icon 11 above 0 less 164, so the ring's top stays clear: the icon's top inside the sector) */
	ae_wheel_entry_place(&layout, AE_WHEEL_SMART, &place);
	NEAR(place.icon_y, -175.0f, 0.001);
	NEAR(place.caption_y, -138.0f, 0.001);

	/* split: the caption at its own offset, the icon 10 over it less half the icon, the weapon's caption over its sprite */
	ae_wheel_layout_get(1, 540.0f, &layout);
	for (entry = 0; entry < AE_WHEEL_COUNT; entry++)
	{
		int ox, oy;
		float rm = (layout.inner + layout.outer) / 2.0f;

		ae_wheel_split_offset(entry, &ox, &oy);
		ae_wheel_entry_place(&layout, entry, &place);
		NEAR(place.caption_x, roundf(direction_x(entry) * rm + (float)ox), 0.001);
		NEAR(place.caption_y, roundf(direction_y(entry) * rm + (float)oy), 0.001);
		/* (whole pixels: nothing at a half) */
		NEAR(place.caption_x, floorf(place.caption_x), 0.0);
		NEAR(place.caption_y, floorf(place.caption_y), 0.0);
		NEAR(place.icon_x, floorf(place.icon_x), 0.0);
		NEAR(place.icon_y, floorf(place.icon_y), 0.0);
		if (entry == AE_WHEEL_REGROUP)
		{
			CHECK(place.caption_over_sprite);
			/* (the sprite under the caption: 5.6 and 3 under its middle, half its height further) */
			NEAR(place.icon_y, roundf(place.caption_y + 5.6f + 3.0f + 12.0f), 1.0);
			CHECK(place.icon_y > place.caption_y);
		}
		else
		{
			CHECK(!place.caption_over_sprite);
			if (entry == AE_WHEEL_RIDE)
				NEAR(place.icon_y, roundf(place.caption_y - 11.5f - 19.35f), 1.0);
			else
				NEAR(place.icon_y, place.caption_y - 10.0f - 15.0f, 0.001);
			CHECK(place.icon_y < place.caption_y);
		}
	}
}

/* the icon's and the caption's ends of every entry stay inside its own sector, in one view */
static void inside_test(void)
{
	struct ae_wheel_layout layout;
	struct ae_wheel_place place;
	int entry;

	ae_wheel_layout_get(0, 1080.0f, &layout);
	for (entry = 0; entry < AE_WHEEL_COUNT; entry++)
	{
		float half_icon = entry == AE_WHEEL_RIDE ? layout.ride * 0.45f : entry == AE_WHEEL_REGROUP ? 12.0f :
			layout.icon / 2.0f;
		float half_wide = entry == AE_WHEEL_REGROUP ? layout.sprite_width / 2.0f : half_icon;
		float corners[8][2];
		int i;

		ae_wheel_entry_place(&layout, entry, &place);
		/* the icon's four corners and the caption's end points (a caption is about 8 px per letter at 17) */
		corners[0][0] = place.icon_x - half_wide; corners[0][1] = place.icon_y - half_icon;
		corners[1][0] = place.icon_x + half_wide; corners[1][1] = place.icon_y - half_icon;
		corners[2][0] = place.icon_x - half_wide; corners[2][1] = place.icon_y + half_icon;
		corners[3][0] = place.icon_x + half_wide; corners[3][1] = place.icon_y + half_icon;
		corners[4][0] = place.caption_x - 28.0f; corners[4][1] = place.caption_y - 6.0f;
		corners[5][0] = place.caption_x + 28.0f; corners[5][1] = place.caption_y - 6.0f;
		corners[6][0] = place.caption_x - 28.0f; corners[6][1] = place.caption_y + 6.0f;
		corners[7][0] = place.caption_x + 28.0f; corners[7][1] = place.caption_y + 6.0f;
		for (i = 0; i < 8; i++)
		{
			if (!ae_wheel_sector_contains(&layout, entry, 0, corners[i][0], corners[i][1]))
				printf("one view: entry %d corner %d (%.1f, %.1f) is outside its sector\n", entry, i, corners[i][0],
					corners[i][1]);
			CHECK(ae_wheel_sector_contains(&layout, entry, 0, corners[i][0], corners[i][1]));
		}
	}
}

static void word_cell_test(void)
{
	struct ae_wheel_layout layout;
	float points[8][2], x, y;

	ae_wheel_layout_get(0, 1080.0f, &layout);
	ae_wheel_word_cell(&layout, 200.0f, points, &x, &y);
	/* (below the ring: 12 past the focused sector's rim, the cell 40 high about its middle) */
	NEAR(x, 0.0f, 0.001);
	NEAR(y, 224.0f + 10.0f + 12.0f + 20.0f, 0.001);
	/* (its width: the text and 44 and the lean of 0.46 of 40) */
	{
		float least_x = 1.0e9f, most_x = -1.0e9f, least_y = 1.0e9f, most_y = -1.0e9f;
		int i;

		for (i = 0; i < 8; i++)
		{
			least_x = fminf(least_x, points[i][0]);
			most_x = fmaxf(most_x, points[i][0]);
			least_y = fminf(least_y, points[i][1]);
			most_y = fmaxf(most_y, points[i][1]);
		}
		NEAR(most_x - least_x, 200.0f + 44.0f + 0.46f * 40.0f, 0.001);
		NEAR(most_y - least_y, 40.0f, 0.001);
		NEAR(least_y, y - 20.0f, 0.001);
	}
	/* split: above, 8.2 past the rim, one pixel higher still */
	ae_wheel_layout_get(1, 540.0f, &layout);
	ae_wheel_word_cell(&layout, 100.0f, points, &x, &y);
	NEAR(y, -(152.32f + 4.08f + 8.16f + 14.0f + 1.0f), 0.05);
	CHECK(y < 0.0f);
}

static void legend_test(void)
{
	struct ae_wheel_layout layout;
	float chips[3] = { 24.0f, 24.0f, 0.0f };
	float words[3] = { 60.0f, 150.0f, 170.0f };
	float lefts[3];
	float width;

	ae_wheel_layout_get(0, 1080.0f, &layout);
	width = ae_wheel_legend_place(&layout, 3, chips, words, lefts);
	/* (each item's chip, the 8 gap and its word, 22 between items) */
	NEAR(width, (24.0f + 8.0f + 60.0f) + 22.0f + (24.0f + 8.0f + 150.0f) + 22.0f + 170.0f, 0.001);
	NEAR(lefts[0], -width / 2.0f, 0.5);
	NEAR(lefts[1], lefts[0] + 92.0f + 22.0f, 0.001);
	NEAR(lefts[2], lefts[1] + 182.0f + 22.0f, 0.001);
	/* (centred, whole pixels) */
	NEAR(lefts[0], floorf(lefts[0]), 0.0);
	/* PICK alone: one item */
	width = ae_wheel_legend_place(&layout, 1, chips, words, lefts);
	NEAR(width, 92.0f, 0.001);
	NEAR(lefts[0], -46.0f, 0.001);
}

static void words_test(void)
{
	CHECK(!wcscmp(ae_wheel_caption(AE_WHEEL_SMART, 0), L"SMART"));
	CHECK(!wcscmp(ae_wheel_caption(AE_WHEEL_ATTACK, 0), L"ATTACK"));
	CHECK(!wcscmp(ae_wheel_caption(AE_WHEEL_ENEMY, 0), L"ENEMY"));
	CHECK(!wcscmp(ae_wheel_caption(AE_WHEEL_REGROUP, 0), L"REGROUP"));
	CHECK(!wcscmp(ae_wheel_caption(AE_WHEEL_DEFEND, 0), L"DEFEND"));
	CHECK(!wcscmp(ae_wheel_caption(AE_WHEEL_RIDE, 0), L"RIDE"));
	CHECK(!wcscmp(ae_wheel_caption(AE_WHEEL_GOING, 0), L"GOING"));
	CHECK(!wcscmp(ae_wheel_caption(AE_WHEEL_WATCH, 0), L"WATCH"));
	CHECK(!wcscmp(ae_wheel_caption(AE_WHEEL_SMART, 1), L"HELP"));
	CHECK(!wcscmp(ae_wheel_caption(AE_WHEEL_ENEMY, 1), L"ENEMY"));
	CHECK(!wcscmp(ae_wheel_caption(AE_WHEEL_GOING, 1), L"GO"));
	CHECK(!wcscmp(ae_wheel_word(AE_WHEEL_SMART, 0), L"SMART CALL"));
	CHECK(!wcscmp(ae_wheel_word(AE_WHEEL_ATTACK, 0), L"ATTACKING HERE"));
	CHECK(!wcscmp(ae_wheel_word(AE_WHEEL_ENEMY, 0), L"ENEMY SPOTTED"));
	CHECK(!wcscmp(ae_wheel_word(AE_WHEEL_REGROUP, 0), L"REGROUP"));
	CHECK(!wcscmp(ae_wheel_word(AE_WHEEL_DEFEND, 0), L"DEFENDING HERE"));
	CHECK(!wcscmp(ae_wheel_word(AE_WHEEL_RIDE, 0), L"NEED A RIDE"));
	CHECK(!wcscmp(ae_wheel_word(AE_WHEEL_GOING, 0), L"GOING HERE"));
	CHECK(!wcscmp(ae_wheel_word(AE_WHEEL_WATCH, 0), L"WATCHING HERE"));
	/* dead: the place call is an order, GO HERE, and the top says HELP ME */
	CHECK(!wcscmp(ae_wheel_word(AE_WHEEL_SMART, 1), L"HELP ME"));
	CHECK(!wcscmp(ae_wheel_word(AE_WHEEL_ENEMY, 1), L"ENEMY SPOTTED"));
	CHECK(!wcscmp(ae_wheel_word(AE_WHEEL_GOING, 1), L"GO HERE"));
	CHECK(ae_wheel_caption(99, 0)[0] == 0);
	CHECK(ae_wheel_word(-1, 0)[0] == 0);
}

static void motion_test(void)
{
	float flash, alpha;

	/* (the send flashes the sector to white over 60 ms, then the ring closes over 80) */
	CHECK(ae_wheel_motion(0, &flash, &alpha));
	NEAR(flash, 0.0f, 0.001);
	NEAR(alpha, 1.0f, 0.001);
	CHECK(ae_wheel_motion(30, &flash, &alpha));
	NEAR(flash, 0.5f, 0.001);
	NEAR(alpha, 1.0f, 0.001);
	CHECK(ae_wheel_motion(60, &flash, &alpha));
	NEAR(flash, 1.0f, 0.001);
	NEAR(alpha, 1.0f, 0.001);
	CHECK(ae_wheel_motion(100, &flash, &alpha));
	NEAR(flash, 1.0f, 0.001);
	NEAR(alpha, 0.5f, 0.001);
	CHECK(ae_wheel_motion(139, &flash, &alpha));
	CHECK(alpha > 0.0f && alpha < 0.02f);
	CHECK(!ae_wheel_motion(140, &flash, &alpha));
	CHECK(!ae_wheel_motion(5000, &flash, &alpha));
	CHECK(!ae_wheel_motion(-1, &flash, &alpha));
}

int main(void)
{
	one_view_test();
	split_test();
	height_test();
	sector_shape_test();
	sector_gap_parallel_test();
	contains_test();
	fan_test();
	wedge_test();
	split_offset_test();
	place_test();
	inside_test();
	word_cell_test();
	legend_test();
	words_test();
	motion_test();
	if (failures)
	{
		printf("%d failed\n", failures);
		return 1;
	}
	printf("ok\n");
	return 0;
}
