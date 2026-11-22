#include <stdio.h>

#include "ae_hud_safe_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_safe_rect rect(int x0, int y0, int x1, int y1)
{
	struct ae_safe_rect r;

	r.x0 = x0; r.y0 = y0; r.x1 = x1; r.y1 = y1;
	return r;
}

int main(void)
{
	struct ae_safe_rect screen = rect(0, 0, 1920, 1080);
	struct ae_safe_rect out;

	/* the value is held to 0 .. 10 */
	CHECK(ae_hud_safe_percent(-3) == 0);
	CHECK(ae_hud_safe_percent(0) == 0);
	CHECK(ae_hud_safe_percent(7) == 7);
	CHECK(ae_hud_safe_percent(10) == 10);
	CHECK(ae_hud_safe_percent(99) == 10);

	/* 0 percent: today's look, any window */
	out = ae_hud_safe_window(screen, screen, 0);
	CHECK(out.x0 == 0 && out.y0 == 0 && out.x1 == 1920 && out.y1 == 1080);

	/* one view: all four edges move, by the width's and the height's share */
	out = ae_hud_safe_window(screen, screen, 5);
	CHECK(out.x0 == 96 && out.x1 == 1824 && out.y0 == 54 && out.y1 == 1026);
	out = ae_hud_safe_window(screen, screen, 10);
	CHECK(out.x0 == 192 && out.x1 == 1728 && out.y0 == 108 && out.y1 == 972);

	/* two views side by side (left): the outer left, top and bottom move; the edge where the views meet stays */
	out = ae_hud_safe_window(screen, rect(0, 0, 960, 1080), 5);
	CHECK(out.x0 == 96 && out.x1 == 960 && out.y0 == 54 && out.y1 == 1026);
	out = ae_hud_safe_window(screen, rect(960, 0, 1920, 1080), 5);
	CHECK(out.x0 == 960 && out.x1 == 1824 && out.y0 == 54 && out.y1 == 1026);

	/* two views stacked, and four views: only the screen's own edges */
	out = ae_hud_safe_window(screen, rect(0, 0, 1920, 540), 10);
	CHECK(out.x0 == 192 && out.x1 == 1728 && out.y0 == 108 && out.y1 == 540);
	out = ae_hud_safe_window(screen, rect(960, 540, 1920, 1080), 10);
	CHECK(out.x0 == 960 && out.x1 == 1728 && out.y0 == 540 && out.y1 == 972);

	/* a window the move would empty is left whole */
	out = ae_hud_safe_window(rect(0, 0, 4, 4), rect(0, 0, 4, 4), 10);
	CHECK(out.x1 - out.x0 > 0);
	out = ae_hud_safe_window(rect(0, 0, 0, 0), rect(0, 0, 0, 0), 10);
	CHECK(out.x1 == 0 && out.y1 == 0);

	if (!failures)
		printf("ae_hud_safe_rules: all passed\n");
	return failures ? 1 : 0;
}
