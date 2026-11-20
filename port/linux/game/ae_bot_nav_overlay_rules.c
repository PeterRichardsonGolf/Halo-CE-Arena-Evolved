/* The shapes of the bot navigation overlay (ae_bot_nav_overlay_rules.h) */
#include "ae_bot_nav_overlay_rules.h"

#include "ae_bot_nav_rules.h"

#include <math.h>
#include <string.h>

struct ae_nav_colour const ae_bot_nav_overlay_palette[AE_NAV_COLOURS] =
{
	{ 0x5F, 0xC8, 0xEC, 0.85f },
	{ 0x3A, 0x8F, 0xB5, 0.85f },
	{ 0xF2, 0xC1, 0x4E, 0.85f },
	{ 0xEC, 0x80, 0x74, 0.85f },
	{ 0xF2, 0x9A, 0x4E, 0.90f },
	{ 0xB0, 0x7C, 0xE8, 0.90f },
	{ 0xFF, 0xFF, 0xFF, 0.90f },
	{ 0x5F, 0xC8, 0xEC, 0.45f },
};

void ae_nav_mesh_init(struct ae_nav_mesh *mesh, float (*point)[3], unsigned char *colour, long capacity)
{
	mesh->point = point;
	mesh->colour = colour;
	mesh->capacity = capacity - capacity % 3;
	mesh->count = 0;
	mesh->dropped = 0;
}

void ae_nav_mesh_clear(struct ae_nav_mesh *mesh)
{
	mesh->count = 0;
	mesh->dropped = 0;
}

int ae_nav_route_colour(int kinds)
{
	return kinds == AE_BOT_NAV_SCORPION ? AE_NAV_COLOUR_BLUE : AE_NAV_COLOUR_ICE;
}

float ae_nav_segment_distance(float const *a, float const *b, float x, float y)
{
	float dx = b[0] - a[0], dy = b[1] - a[1], length = dx * dx + dy * dy, t = 0.0f, px, py;

	if (length > 1e-12f)
	{
		t = ((x - a[0]) * dx + (y - a[1]) * dy) / length;
		t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
	}
	px = a[0] + dx * t - x;
	py = a[1] + dy * t - y;
	return (float)sqrt(px * px + py * py);
}

int ae_nav_point_near(float const *p, float x, float y, float range)
{
	float dx = p[0] - x, dy = p[1] - y;

	return dx * dx + dy * dy <= range * range;
}

int ae_nav_segment_near(float const *a, float const *b, float x, float y, float range)
{
	return ae_nav_segment_distance(a, b, x, y) <= range;
}

/* one triangle, or nothing and a count when the buffer is full */
static int triangle(struct ae_nav_mesh *mesh, float const *a, float const *b, float const *c, int colour)
{
	long at = mesh->count;

	if (at + 3 > mesh->capacity)
	{
		mesh->dropped++;
		return 0;
	}
	memcpy(mesh->point[at], a, 3 * sizeof(float));
	memcpy(mesh->point[at + 1], b, 3 * sizeof(float));
	memcpy(mesh->point[at + 2], c, 3 * sizeof(float));
	mesh->colour[at] = mesh->colour[at + 1] = mesh->colour[at + 2] = (unsigned char)colour;
	mesh->count = at + 3;
	return 1;
}

/* a quad from four corners in order, as two triangles: both or none */
static int quad(struct ae_nav_mesh *mesh, float const *a, float const *b, float const *c, float const *d, int colour)
{
	if (mesh->count + 6 > mesh->capacity)
	{
		mesh->dropped += 2;
		return 0;
	}
	triangle(mesh, a, b, c, colour);
	triangle(mesh, a, c, d, colour);
	return 1;
}

int ae_nav_line(struct ae_nav_mesh *mesh, float const *a, float const *b, float width, int colour)
{
	float dx = b[0] - a[0], dy = b[1] - a[1], length = (float)sqrt(dx * dx + dy * dy), nx, ny;
	float p0[3], p1[3], p2[3], p3[3];

	if (length < 1e-6f)
	{
		/* (straight up or no length: a short cross-wise stub, not a line) */
		nx = 0.0f;
		ny = 0.5f * width;
	}
	else
	{
		nx = -dy / length * 0.5f * width;
		ny = dx / length * 0.5f * width;
	}
	p0[0] = a[0] + nx; p0[1] = a[1] + ny; p0[2] = a[2];
	p1[0] = a[0] - nx; p1[1] = a[1] - ny; p1[2] = a[2];
	p2[0] = b[0] - nx; p2[1] = b[1] - ny; p2[2] = b[2];
	p3[0] = b[0] + nx; p3[1] = b[1] + ny; p3[2] = b[2];
	/* (counter-clockwise from above: facing up) */
	return quad(mesh, p0, p1, p2, p3, colour);
}

int ae_nav_square(struct ae_nav_mesh *mesh, float const *c, float half, int colour)
{
	float p0[3], p1[3], p2[3], p3[3];

	p0[0] = c[0] - half; p0[1] = c[1] - half; p0[2] = c[2];
	p1[0] = c[0] + half; p1[1] = c[1] - half; p1[2] = c[2];
	p2[0] = c[0] + half; p2[1] = c[1] + half; p2[2] = c[2];
	p3[0] = c[0] - half; p3[1] = c[1] + half; p3[2] = c[2];
	return quad(mesh, p0, p1, p2, p3, colour);
}

int ae_nav_diamond(struct ae_nav_mesh *mesh, float const *c, float radius, int colour)
{
	float p0[3], p1[3], p2[3], p3[3];

	p0[0] = c[0] - radius; p0[1] = c[1]; p0[2] = c[2];
	p1[0] = c[0]; p1[1] = c[1] - radius; p1[2] = c[2];
	p2[0] = c[0] + radius; p2[1] = c[1]; p2[2] = c[2];
	p3[0] = c[0]; p3[1] = c[1] + radius; p3[2] = c[2];
	return quad(mesh, p0, p1, p2, p3, colour);
}

int ae_nav_diamond_outline(struct ae_nav_mesh *mesh, float const *c, float radius, float width, int colour)
{
	float p[4][3];
	int index, ok = 1;

	for (index = 0; index < 4; index++)
	{
		p[index][0] = c[0] + (index == 0 ? -radius : index == 2 ? radius : 0.0f);
		p[index][1] = c[1] + (index == 1 ? -radius : index == 3 ? radius : 0.0f);
		p[index][2] = c[2];
	}
	for (index = 0; index < 4; index++)
		ok &= ae_nav_line(mesh, p[index], p[(index + 1) % 4], width, colour);
	return ok;
}

int ae_nav_chevron(struct ae_nav_mesh *mesh, float const *a, float const *b, float size, float width, int colour)
{
	float dx = b[0] - a[0], dy = b[1] - a[1], length = (float)sqrt(dx * dx + dy * dy), ux, uy;
	float tip[3], left[3], right[3];

	if (length < 1e-6f)
		return 0;
	ux = dx / length;
	uy = dy / length;
	tip[0] = (a[0] + b[0]) * 0.5f + ux * size * 0.5f;
	tip[1] = (a[1] + b[1]) * 0.5f + uy * size * 0.5f;
	tip[2] = (a[2] + b[2]) * 0.5f;
	/* (the wings go back from the tip and out to each side) */
	left[0] = tip[0] - ux * size - uy * size * 0.6f;
	left[1] = tip[1] - uy * size + ux * size * 0.6f;
	left[2] = tip[2];
	right[0] = tip[0] - ux * size + uy * size * 0.6f;
	right[1] = tip[1] - uy * size - ux * size * 0.6f;
	right[2] = tip[2];
	return ae_nav_line(mesh, left, tip, width, colour) & ae_nav_line(mesh, right, tip, width, colour);
}

int ae_nav_ring(struct ae_nav_mesh *mesh, float x, float y, float z, float radius, int segments, float width, int dashed,
	int colour)
{
	int index, ok = 1;

	for (index = 0; index < segments; index++)
	{
		float angle0 = 6.2831853f * (float)index / (float)segments, angle1 = 6.2831853f * (float)(index + 1) / (float)segments;
		float a[3], b[3];

		if (dashed && (index & 1))
			continue;
		a[0] = x + radius * (float)cos(angle0); a[1] = y + radius * (float)sin(angle0); a[2] = z;
		b[0] = x + radius * (float)cos(angle1); b[1] = y + radius * (float)sin(angle1); b[2] = z;
		ok &= ae_nav_line(mesh, a, b, width, colour);
	}
	return ok;
}

int ae_nav_outline(struct ae_nav_mesh *mesh, float const (*points)[3], int count, float width, int colour)
{
	int index, ok = 1;

	if (count < 3)
		return 0;
	for (index = 0; index < count; index++)
		ok &= ae_nav_line(mesh, points[index], points[(index + 1) % count], width, colour);
	return ok;
}
