#include <math.h>
#include <stdio.h>
#include "../src/ae_layout.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabs((a) - (b)) < 0.01)

/* a pointer round trip: layout point -> menu point -> floored to a whole menu pixel, as halo_ui_pointer reports
it (d3d8_gl.c ui_point_from_window) -> layout: within half a menu pixel of where it started */
static void pointer_round_trip(int screen_width, float layout_width)
{
	float menu_x, menu_y, x, y;
	float half_x = 0.5f * layout_width / (float)screen_width + 0.01f, half_y = 0.5f * 1080.0f / 480.0f + 0.01f;
	int i, j;

	for (i = 0; i <= 16; i++)
		for (j = 0; j <= 16; j++)
		{
			/* (a grid over the whole picture, its edges' last pixels included) */
			float px = layout_width * (float)i / 16.0f - (i == 16 ? 0.001f : 0.0f);
			float py = 1080.0f * (float)j / 16.0f - (j == 16 ? 0.001f : 0.0f);

			ae_layout_to_menu_point(px, py, screen_width, layout_width, &menu_x, &menu_y);
			ae_layout_from_menu_point((short)floorf(menu_x), (short)floorf(menu_y), screen_width, layout_width, &x, &y);
			CHECK(fabs(x - px) <= half_x && fabs(y - py) <= half_y);
		}
}

/* the footer's fit rule: lowest priority dropped first, never past the room */
static void prompts_fit(void)
{
	float widths[4] = { 100, 80, 60, 90 };
	short rank[4] = { 0, 1, 2, 3 };
	unsigned char keep[4];

	CHECK(ae_prompts_fit(widths, rank, 4, 10, 1000, keep) == 4 && keep[0] && keep[1] && keep[2] && keep[3]);
	/* 100+80+60+90 + 30 = 360: exactly the room fits; a unit less drops the lowest (rank 3) */
	CHECK(ae_prompts_fit(widths, rank, 4, 10, 360, keep) == 4);
	CHECK(ae_prompts_fit(widths, rank, 4, 10, 359, keep) == 3 && keep[0] && keep[1] && keep[2] && !keep[3]);
	CHECK(ae_prompts_fit(widths, rank, 4, 10, 250, keep) == 2 && keep[0] && keep[1] && !keep[2] && !keep[3]);
	CHECK(ae_prompts_fit(widths, rank, 4, 10, 100, keep) == 1 && keep[0]);
	/* not even the highest fits: none, and the caller shortens it */
	CHECK(ae_prompts_fit(widths, rank, 4, 10, 99, keep) == 0 && !keep[0] && !keep[1] && !keep[2] && !keep[3]);
	/* the priority is not the order: rank 0 on the last one keeps it over the earlier ones */
	{
		short ranks[4] = { 2, 1, 3, 0 };

		CHECK(ae_prompts_fit(widths, ranks, 4, 10, 190, keep) == 2 && keep[3] && keep[1] && !keep[0] && !keep[2]);
	}
	CHECK(ae_prompts_fit(widths, rank, 0, 10, 0, keep) == 0);
}

int main(void)
{
	struct ae_layout l;
	struct ae_view v;
	float x, y, px, py;

	ae_draw_layout(0, 0, 1920, 1080, &l);          /* 16:9 */
	CHECK(NEAR(l.height, 1080) && NEAR(l.width, 1920) && NEAR(l.scale, 1.0));
	ae_draw_layout(0, 0, 1280, 720, &l);
	CHECK(NEAR(l.height, 1080) && NEAR(l.width, 1920) && NEAR(l.scale, 720.0 / 1080.0));
	ae_draw_layout(160, 0, 1440, 1080, &l);        /* 4:3 picture in a 16:9 window */
	CHECK(NEAR(l.width, 1440) && NEAR(l.origin_x, 160));
	ae_layout_to_pixels(&l, 0, 0, &px, &py);       /* the layout's left edge is the picture's, not the window's */
	CHECK(NEAR(px, 160) && NEAR(py, 0));
	ae_layout_to_pixels(&l, 1440, 1080, &px, &py);
	CHECK(NEAR(px, 1600) && NEAR(py, 1080));
	ae_draw_layout(0, 0, 3440, 1440, &l);          /* 21:9 */
	CHECK(NEAR(l.height, 1080) && NEAR(l.width, 3440.0 * 1080.0 / 1440.0));
	ae_draw_layout(960, 540, 960, 540, &l);        /* a quarter view */
	CHECK(NEAR(l.height, 1080) && NEAR(l.scale, 0.5) && NEAR(l.origin_x, 960));
	ae_layout_to_pixels(&l, 100, 200, &px, &py);
	ae_layout_from_pixels(&l, px, py, &x, &y);
	CHECK(NEAR(px, 1010) && NEAR(py, 640) && NEAR(x, 100) && NEAR(y, 200));
	ae_draw_layout(0, 0, 0, 0, &l);                /* no picture yet: no division by zero */
	CHECK(NEAR(l.scale, 1.0) && NEAR(l.width, 0));

	/* views: the whole layout, and the 2p / 4p split-screen parts of a 16:9 layout */
	ae_layout_view(0, 0, 1920, 1080, &v);
	CHECK(NEAR(v.scale, 1.0) && NEAR(ae_view_width(&v), 1920));
	ae_layout_view(0, 540, 1920, 540, &v);         /* 2p: the lower half */
	CHECK(NEAR(v.scale, 0.5) && NEAR(ae_view_width(&v), 3840));
	ae_view_to_layout(&v, 0, 0, &x, &y);
	CHECK(NEAR(x, 0) && NEAR(y, 540));
	ae_view_to_layout(&v, 3840, 1080, &x, &y);
	CHECK(NEAR(x, 1920) && NEAR(y, 1080));
	ae_layout_view(960, 540, 960, 540, &v);        /* 4p: the lower right quarter */
	CHECK(NEAR(v.scale, 0.5) && NEAR(ae_view_width(&v), 1920));
	ae_view_to_layout(&v, 1920, 1080, &x, &y);
	CHECK(NEAR(x, 1920) && NEAR(y, 1080));
	ae_view_from_layout(&v, 1440, 810, &x, &y);    /* the quarter's middle is its own space's middle */
	CHECK(NEAR(x, 960) && NEAR(y, 540));
	ae_layout_view(0, 0, 0, 0, &v);
	CHECK(NEAR(v.scale, 1.0));

	/* the menus' pointer: 640 columns centred in the picture, 480 lines */
	/* (a menu pixel maps to its middle: 2.25 layout units a pixel, so half of one is 1.125) */
	ae_layout_from_menu_point(0, 0, 640, 1440, &x, &y);       /* 4:3: the columns are the picture */
	CHECK(NEAR(x, 1.125) && NEAR(y, 1.125));
	ae_layout_from_menu_point(639, 479, 640, 1440, &x, &y);   /* its last pixel */
	CHECK(NEAR(x, 1440 - 1.125) && NEAR(y, 1080 - 1.125));
	ae_layout_from_menu_point(-107, 240, 854, 1921.5f, &x, &y); /* 16:9 (854 wide): the left margin */
	CHECK(NEAR(x, 1.125) && NEAR(y, 541.125));
	ae_layout_from_menu_point(320, 0, 1120, 2520, &x, &y);    /* 21:9 (1120 wide): the middle */
	CHECK(NEAR(x, 1261.125));
	ae_layout_to_menu_point(1261.125f, 541.125f, 1120, 2520, &x, &y);
	CHECK(NEAR(x, 320.5) && NEAR(y, 240.5));
	ae_layout_to_menu_point(1440, 1080, 640, 0, &x, &y);      /* no picture yet: no division by zero */
	CHECK(NEAR(x, 640) && NEAR(y, 480));
	pointer_round_trip(640, 1440);
	pointer_round_trip(854, 1921.5f);
	pointer_round_trip(1120, 2520);
	pointer_round_trip(800, 1800);                            /* 16:10 */
	pointer_round_trip(1280, 2880);                           /* 32:9 */
	prompts_fit();
	if (failures)
		return 1;
	printf("ae_draw_layout: ok\n");
	return 0;
}
