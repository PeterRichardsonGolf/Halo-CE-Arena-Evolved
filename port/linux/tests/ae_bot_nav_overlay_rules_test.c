/* ae_bot_nav_overlay_rules_test.c: the overlay's shapes: a line is two triangles facing up, a ring's segments and dashes, the
distance tests, and a full buffer drops and counts what does not fit. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ae_bot_nav_overlay_rules.h"
#include "ae_bot_nav_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static float points[600][3];
static unsigned char colours[600];

/* the triangle's normal's up component: positive when it winds counter-clockwise seen from above */
static float up(struct ae_nav_mesh const *mesh, long first)
{
	float const *a = mesh->point[first], *b = mesh->point[first + 1], *c = mesh->point[first + 2];

	return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}

int main(void)
{
	struct ae_nav_mesh mesh;
	float a[3] = { 0.0f, 0.0f, 1.0f }, b[3] = { 10.0f, 0.0f, 2.0f }, c[3] = { 5.0f, 5.0f, 0.0f };
	float square[4][3] = { { 0, 0, 0 }, { 2, 0, 0 }, { 2, 2, 0 }, { 0, 2, 0 } };
	long index;

	ae_nav_mesh_init(&mesh, points, colours, 600);
	/* a line: two triangles, both facing up, its width across, its ends the points */
	CHECK(ae_nav_line(&mesh, a, b, 0.08f, AE_NAV_COLOUR_ICE) == 1 && mesh.count == 6);
	CHECK(up(&mesh, 0) > 0.0f && up(&mesh, 3) > 0.0f);
	CHECK(fabsf(points[0][1] - 0.04f) < 1e-5f && fabsf(points[1][1] + 0.04f) < 1e-5f && points[2][2] == 2.0f);
	CHECK(colours[0] == AE_NAV_COLOUR_ICE && colours[5] == AE_NAV_COLOUR_ICE);
	/* the other way along it faces up too */
	ae_nav_mesh_clear(&mesh);
	ae_nav_line(&mesh, b, a, 0.08f, AE_NAV_COLOUR_ICE);
	CHECK(up(&mesh, 0) > 0.0f && up(&mesh, 3) > 0.0f);
	/* a line of no length does not blow up */
	ae_nav_mesh_clear(&mesh);
	CHECK(ae_nav_line(&mesh, a, a, 0.08f, AE_NAV_COLOUR_ICE) == 1 && mesh.count == 6);

	/* a square and a diamond: two triangles each, up */
	ae_nav_mesh_clear(&mesh);
	CHECK(ae_nav_square(&mesh, c, 0.3f, AE_NAV_COLOUR_RED) && mesh.count == 6 && up(&mesh, 0) > 0.0f && up(&mesh, 3) > 0.0f);
	ae_nav_mesh_clear(&mesh);
	CHECK(ae_nav_diamond(&mesh, c, 0.4f, AE_NAV_COLOUR_ORANGE) && mesh.count == 6 && up(&mesh, 0) > 0.0f && up(&mesh, 3) > 0.0f);
	ae_nav_mesh_clear(&mesh);
	CHECK(ae_nav_diamond_outline(&mesh, c, 0.5f, 0.08f, AE_NAV_COLOUR_RED) && mesh.count == 24);

	/* a chevron: two lines, its tip ahead of the middle along a to b */
	ae_nav_mesh_clear(&mesh);
	CHECK(ae_nav_chevron(&mesh, a, b, 0.9f, 0.08f, AE_NAV_COLOUR_ICE) && mesh.count == 12);
	CHECK(ae_nav_chevron(&mesh, a, a, 0.9f, 0.08f, AE_NAV_COLOUR_ICE) == 0 && mesh.count == 12);

	/* a ring: 24 segments of two triangles; dashed, half of them; all of it at the radius and the height */
	ae_nav_mesh_clear(&mesh);
	CHECK(ae_nav_ring(&mesh, 3.0f, 4.0f, 1.5f, 2.0f, 24, 0.08f, 0, AE_NAV_COLOUR_RED) && mesh.count == 24 * 6);
	CHECK(mesh.dropped == 0);
	for (index = 0; index < mesh.count; index++)
	{
		float dx = mesh.point[index][0] - 3.0f, dy = mesh.point[index][1] - 4.0f, r = (float)sqrt(dx * dx + dy * dy);

		CHECK(mesh.point[index][2] == 1.5f && fabsf(r - 2.0f) < 0.05f);
	}
	ae_nav_mesh_clear(&mesh);
	CHECK(ae_nav_ring(&mesh, 0.0f, 0.0f, 0.0f, 1.0f, 24, 0.08f, 1, AE_NAV_COLOUR_AMBER) && mesh.count == 12 * 6);

	/* an outline: a loop of lines; fewer than three points is none */
	ae_nav_mesh_clear(&mesh);
	CHECK(ae_nav_outline(&mesh, (float const (*)[3])square, 4, 0.05f, AE_NAV_COLOUR_RED) && mesh.count == 24);
	CHECK(ae_nav_outline(&mesh, (float const (*)[3])square, 2, 0.05f, AE_NAV_COLOUR_RED) == 0);

	/* a full buffer: what does not fit is dropped whole and counted (a line is two triangles) */
	ae_nav_mesh_init(&mesh, points, colours, 600);
	for (index = 0; index < 100; index++)
		CHECK(ae_nav_line(&mesh, a, b, 0.08f, AE_NAV_COLOUR_ICE));
	CHECK(mesh.count == 600 && mesh.dropped == 0);
	CHECK(ae_nav_line(&mesh, a, b, 0.08f, AE_NAV_COLOUR_ICE) == 0 && mesh.count == 600 && mesh.dropped == 2);
	CHECK(ae_nav_square(&mesh, c, 0.3f, AE_NAV_COLOUR_ICE) == 0 && mesh.dropped == 4);
	ae_nav_mesh_clear(&mesh);
	CHECK(mesh.count == 0 && mesh.dropped == 0);
	/* a capacity that is no multiple of 3 is cut down to one */
	ae_nav_mesh_init(&mesh, points, colours, 8);
	CHECK(mesh.capacity == 6 && ae_nav_line(&mesh, a, b, 0.08f, 0) == 1 && ae_nav_line(&mesh, a, b, 0.08f, 0) == 0);

	/* distances: a point's, a segment's (its ends, its middle, past an end) */
	CHECK(ae_nav_point_near(c, 5.0f, 5.0f, 0.1f) && !ae_nav_point_near(c, 5.0f, 46.0f, 40.0f) && ae_nav_point_near(c, 5.0f, 45.0f, 40.0f));
	CHECK(fabsf(ae_nav_segment_distance(a, b, 5.0f, 3.0f) - 3.0f) < 1e-5f);
	CHECK(fabsf(ae_nav_segment_distance(a, b, -3.0f, 4.0f) - 5.0f) < 1e-5f);
	CHECK(fabsf(ae_nav_segment_distance(a, a, 3.0f, 4.0f) - 5.0f) < 1e-5f);
	/* a long segment passing the camera is near though both its ends are far */
	{
		float far_a[3] = { -100.0f, 0.0f, 0.0f }, far_b[3] = { 100.0f, 0.0f, 0.0f };

		CHECK(ae_nav_segment_near(far_a, far_b, 0.0f, 30.0f, 40.0f) && !ae_nav_segment_near(far_a, far_b, 0.0f, 41.0f, 40.0f));
		CHECK(!ae_nav_point_near(far_a, 0.0f, 30.0f, 40.0f) && !ae_nav_point_near(far_b, 0.0f, 30.0f, 40.0f));
	}

	/* colours: a scorpion's route alone is the darker blue; every other kind is the ice */
	CHECK(ae_nav_route_colour(AE_BOT_NAV_SCORPION) == AE_NAV_COLOUR_BLUE);
	CHECK(ae_nav_route_colour(AE_BOT_NAV_WARTHOG) == AE_NAV_COLOUR_ICE && ae_nav_route_colour(AE_BOT_NAV_ANY) == AE_NAV_COLOUR_ICE &&
		ae_nav_route_colour(AE_BOT_NAV_GHOST | AE_BOT_NAV_SCORPION) == AE_NAV_COLOUR_ICE);
	CHECK(ae_bot_nav_overlay_palette[AE_NAV_COLOUR_ICE].red == 0x5F && ae_bot_nav_overlay_palette[AE_NAV_COLOUR_BLUE].blue == 0xB5 &&
		ae_bot_nav_overlay_palette[AE_NAV_COLOUR_PURPLE].red == 0xB0 && ae_bot_nav_overlay_palette[AE_NAV_COLOUR_AMBER].green == 0xC1);

	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
