/*
AE_BOT_NAV_OVERLAY_RULES.H

Arena Evolved: the shapes of the DEV MODE bot navigation overlay (ae_bot_nav_overlay.h), as plain triangles in a fixed
buffer: a thin flat line, a square, a diamond, a chevron, a ring (whole or dashed) and an outline. Each vertex carries a
palette number; the overlay turns the palette into pixels. A full buffer drops what does not fit and counts it. Pure
(no engine types): unit tested (tests/ae_bot_nav_overlay_rules_test.c).
*/

#ifndef __AE_BOT_NAV_OVERLAY_RULES_H
#define __AE_BOT_NAV_OVERLAY_RULES_H

/* the palette: the colours of the overlay (rgb 0-255, alpha 0-1), in this order in ae_bot_nav_overlay_palette */
enum
{
	AE_NAV_COLOUR_ICE = 0,   /* routes of warthogs, ghosts and any kind */
	AE_NAV_COLOUR_BLUE,      /* routes of scorpions */
	AE_NAV_COLOUR_AMBER,     /* keepouts */
	AE_NAV_COLOUR_RED,       /* scenery rings, learnt circles, closed ground, crashes, learnt wedges */
	AE_NAV_COLOUR_ORANGE,    /* wedges */
	AE_NAV_COLOUR_PURPLE,    /* stuck */
	AE_NAV_COLOUR_WHITE,     /* a driver's next waypoint */
	AE_NAV_COLOUR_ICE_THIN,  /* a driver's remaining path (the ice, fainter) */
	AE_NAV_COLOURS
};

struct ae_nav_colour
{
	unsigned char red, green, blue;
	float alpha;
};
extern struct ae_nav_colour const ae_bot_nav_overlay_palette[AE_NAV_COLOURS];

/* the buffer: capacity vertices, a multiple of 3 */
struct ae_nav_mesh
{
	float (*point)[3];
	unsigned char *colour;
	long capacity;
	long count;
	long dropped;	/* triangles that did not fit */
};

void ae_nav_mesh_init(struct ae_nav_mesh *mesh, float (*point)[3], unsigned char *colour, long capacity);
void ae_nav_mesh_clear(struct ae_nav_mesh *mesh);

/* the palette number of a route's kinds (ae_bot_nav_rules.h's bits): scorpion alone is the darker blue, any other the ice */
int ae_nav_route_colour(int kinds);
/* the distance across the ground (x, y) from a point to the segment a-b */
float ae_nav_segment_distance(float const *a, float const *b, float x, float y);
/* whether a point / a segment is within range across the ground of (x, y) */
int ae_nav_point_near(float const *p, float x, float y, float range);
int ae_nav_segment_near(float const *a, float const *b, float x, float y, float range);

/* the triangles added; 0 when they did not fit (and are counted in dropped) */
/* a flat quad along a-b, width wide, lying in the plane of the segment's height and its ground-side normal (facing up) */
int ae_nav_line(struct ae_nav_mesh *mesh, float const *a, float const *b, float width, int colour);
/* a filled square of half-width half, flat, at c */
int ae_nav_square(struct ae_nav_mesh *mesh, float const *c, float half, int colour);
/* a filled diamond (a square turned 45 degrees) of radius radius, flat */
int ae_nav_diamond(struct ae_nav_mesh *mesh, float const *c, float radius, int colour);
/* the diamond's outline, its lines width wide */
int ae_nav_diamond_outline(struct ae_nav_mesh *mesh, float const *c, float radius, float width, int colour);
/* a chevron (two lines back from its tip) at the middle of a-b, pointing from a to b, size long */
int ae_nav_chevron(struct ae_nav_mesh *mesh, float const *a, float const *b, float size, float width, int colour);
/* a ring of segments segments round (x, y) at height z; dashed leaves out every other segment */
int ae_nav_ring(struct ae_nav_mesh *mesh, float x, float y, float z, float radius, int segments, float width, int dashed,
	int colour);
/* a closed outline through count points (3 or more) */
int ae_nav_outline(struct ae_nav_mesh *mesh, float const (*points)[3], int count, float width, int colour);

#endif
