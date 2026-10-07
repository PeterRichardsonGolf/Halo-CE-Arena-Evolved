#include <math.h>
#include <stdio.h>
#include "../src/ae_layout.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabs((a) - (b)) < 0.01)

/* a pointer round trip: layout point -> menu point (as halo_ui_pointer reports it, whole numbers) -> layout */
static void pointer_round_trip(int screen_width, float layout_width)
{
	float menu_x, menu_y, x, y;
	float points[3][2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 1080.0f } };
	int index;

	points[1][0] = layout_width * 0.5f; points[1][1] = 540.0f;
	points[2][0] = layout_width;
	for (index = 0; index < 3; index++)
	{
		ae_layout_to_menu_point(points[index][0], points[index][1], screen_width, layout_width, &menu_x, &menu_y);
		ae_layout_from_menu_point((short)floorf(menu_x + 0.5f), (short)floorf(menu_y + 0.5f), screen_width, layout_width,
			&x, &y);
		/* (a menu pixel is 2.25 layout units tall; whole menu pixels lose up to half of one) */
		CHECK(fabs(x - points[index][0]) <= 1.2 * layout_width / screen_width + 0.01);
		CHECK(fabs(y - points[index][1]) <= 1.2 * 1080.0 / 480.0 + 0.01);
	}
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
	ae_layout_from_menu_point(0, 0, 640, 1440, &x, &y);       /* 4:3: the columns are the picture */
	CHECK(NEAR(x, 0) && NEAR(y, 0));
	ae_layout_from_menu_point(640, 480, 640, 1440, &x, &y);
	CHECK(NEAR(x, 1440) && NEAR(y, 1080));
	ae_layout_from_menu_point(-107, 240, 854, 1921.5f, &x, &y); /* 16:9 (854 wide): the left margin */
	CHECK(NEAR(x, 0) && NEAR(y, 540));
	ae_layout_from_menu_point(320, 0, 1120, 2520, &x, &y);    /* 21:9 (1120 wide): the middle */
	CHECK(NEAR(x, 1260));
	pointer_round_trip(640, 1440);
	pointer_round_trip(854, 1921.5f);
	pointer_round_trip(1120, 2520);
	if (failures)
		return 1;
	printf("ae_draw_layout: ok\n");
	return 0;
}
