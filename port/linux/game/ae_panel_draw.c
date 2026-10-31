/*
AE_PANEL_DRAW.C

HUD STYLE AE's panels' primitives (ae_panel_draw.h; spec 14.15). Shapes are
screen quads with the rasterizer's default texture (ae_hud_text.c's
ae_hud_fill_quad); CE's art is the map's own HUD bitmaps drawn as hud_draw.c
draws a sprite (the same blend as the HUD's numbers).
*/

#include <math.h>
#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmap_color_conversion.h"
#include "cache/texture_cache.h"
#include "interface/hud_draw.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "tag_files/tag_groups.h"

#include "ae_hud_digits.h"
#include "ae_hud_text.h"
#include "ae_panel_draw.h"

boolean config_boolean(const char *name);
unsigned long config_changes(void);
void platform_log(char const *format, ...);
/* the high-res HUD's (port/linux/src/hud_hires.c) */
long hud_hires_asset_count(void);
char const *hud_hires_asset_tag(long asset);
long hud_hires_asset_bitmap(long asset);
void hud_hires_register_placeholder(long asset, const unsigned long *texture);
unsigned int hud_hires_placeholder_texture(unsigned long data, unsigned long *levels);
boolean rasterizer_bitmap_new(struct bitmap_data *bitmap);

#define AE_BLEND_ALPHA_MULTIPLY_ADD 7  /* the HUD's, as ae_hud_digits.c */
#define AE_BITMAP_GROUP_INTERFACE 4    /* _bitmap_group_type_interface_bitmaps */
#define KEYLINE_ALPHA 0.90f
#define LINE_ALPHA 0.85f
#define CORNER_RADIUS 3.0f
#define CORNER_STEPS 4
#define LIT_ALPHA 0.94f
#define SPENT_ALPHA 0.28f
/* a loaded round: full strength with a light keyline, as the shield bar's fill reads (owner 10-15: thin rounds in a dark
keyline read grey) */
#define ROUND_LIT_ALPHA 1.0f
#define ROUND_LIT_KEYLINE 0.40f
#define MAXIMUM_POINTS 64
#define AE_PI 3.14159265f
/* the score strip's recipe (ae_score_strip.c outline4 / draw_bar), which every panel follows (owner 10-15: "match the
rest to the strip") */
#define STRIP_UNDER 3.6f        /* the dark under-stroke's width, px x scale */
#define STRIP_UNDER_MINIMUM 2.4f
#define STRIP_UNDER_ALPHA 0.78f
#define STRIP_LINE 1.6f         /* the line's width, px x scale */
#define STRIP_LINE_MINIMUM 1.2f
#define TRACK_ALPHA 0.35f       /* the dark track under a bar, a box, a lost cell */
#define FILL_ALPHA 0.92f
#define STRIP_DIVIDER_ALPHA 0.59f
#define HALO_OFFSET 1.5f        /* the strip digits' glow: 4 passes this far out at half alpha */
#define HALO_ALPHA 0.45f
#define FEATHER 0.5f        /* half of the 1 px soft edge on every slanted or curved edge (no stair steps) */
#define MAXIMUM_RING_STEPS 128

static const real tones[NUMBER_OF_AE_TONES][3] = {
	{ 229.0f, 240.0f, 255.0f }, { 178.0f, 224.0f, 255.0f }, { 188.0f, 198.0f, 212.0f }, { 6.0f, 10.0f, 18.0f },
	{ 255.0f, 77.0f, 77.0f }, { 120.0f, 255.0f, 160.0f }, { 214.0f, 255.0f, 226.0f }, { 198.0f, 86.0f, 123.0f },
	{ 111.0f, 208.0f, 78.0f },
};

/* the 8 directions of a keyline's passes */
static const real around[8][2] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f },
	{ 0.7071f, 0.7071f }, { -0.7071f, 0.7071f }, { 0.7071f, -0.7071f }, { -0.7071f, -0.7071f } };

void ae_panel_tone_rgb(short tone, real rgb[3])
{
	short i;

	if (tone < 0 || tone >= NUMBER_OF_AE_TONES)
		tone = _ae_tone_blue;
	for (i = 0; i < 3; i++)
		rgb[i] = tones[tone][i] / 255.0f;
}

/* every tone's alpha x this while a group blinks (ae_panel_draw_set_fade) */
static real draw_fade = 1.0f;

void ae_panel_draw_set_fade(real fade)
{
	draw_fade = fade < 0.0f ? 0.0f : fade > 1.0f ? 1.0f : fade;
}

static void tone_color(short tone, real alpha, real_argb_color *color)
{
	real rgb[3];

	ae_panel_tone_rgb(tone, rgb);
	alpha *= draw_fade;
	color->alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	color->red = rgb[0];
	color->green = rgb[1];
	color->blue = rgb[2];
}

/* ---------- shapes */

static void triangle(const real a[2], const real b[2], const real c[2], const real_argb_color *color)
{
	real points[4][2];

	points[0][0] = a[0]; points[0][1] = a[1];
	points[1][0] = b[0]; points[1][1] = b[1];
	points[2][0] = c[0]; points[2][1] = c[1];
	points[3][0] = c[0]; points[3][1] = c[1];
	ae_hud_fill_quad_color(points, color);
}

static void fan(const real (*points)[2], short count, const real_argb_color *color, real dx, real dy)
{
	real centre[2] = { 0.0f, 0.0f };
	short i;

	if (count < 3)
		return;
	for (i = 0; i < count; i++)
	{
		centre[0] += points[i][0];
		centre[1] += points[i][1];
	}
	centre[0] = centre[0] / (real)count + dx;
	centre[1] = centre[1] / (real)count + dy;
	for (i = 0; i < count; i++)
	{
		real a[2], b[2];

		a[0] = points[i][0] + dx; a[1] = points[i][1] + dy;
		b[0] = points[(i + 1) % count][0] + dx; b[1] = points[(i + 1) % count][1] + dy;
		triangle(centre, a, b, color);
	}
}

static void clear_of(const real_argb_color *color, real_argb_color *clear)
{
	*clear = *color;
	clear->alpha = 0.0f;
}

/* a straight line with soft long sides: a solid core, then a 1 px ramp to nothing on each side (a line under 1 px:
the two ramps alone, at a lower peak, so its weight stays) */
static void line_soft(real x0, real y0, real x1, real y1, real width, const real_argb_color *color)
{
	real dx = x1 - x0, dy = y1 - y0, length = (real)sqrt(dx * dx + dy * dy);
	real_argb_color peak = *color, clear, colors[4];
	real nx, ny, core, outer;
	short side, k;

	if (length <= 0.0f || width <= 0.0f)
		return;
	nx = -dy / length;
	ny = dx / length;
	if (width >= 2.0f * FEATHER)
	{
		core = width / 2.0f - FEATHER;
		outer = width / 2.0f + FEATHER;
	}
	else
	{
		core = 0.0f;
		outer = width / 2.0f + FEATHER;
		peak.alpha *= width / (2.0f * FEATHER);
	}
	clear_of(&peak, &clear);
	if (core > 0.0f)
	{
		/* (the core's corners by the same arithmetic as the ramps' inner corners, so their seams meet exactly) */
		real points[4][2];

		points[0][0] = x0 + nx * core; points[0][1] = y0 + ny * core;
		points[1][0] = x1 + nx * core; points[1][1] = y1 + ny * core;
		points[2][0] = x1 - nx * core; points[2][1] = y1 - ny * core;
		points[3][0] = x0 - nx * core; points[3][1] = y0 - ny * core;
		ae_hud_fill_quad_color(points, &peak);
	}
	for (side = -1; side <= 1; side += 2)
	{
		real points[4][2];

		points[0][0] = x0 + nx * core * (real)side; points[0][1] = y0 + ny * core * (real)side;
		points[1][0] = x1 + nx * core * (real)side; points[1][1] = y1 + ny * core * (real)side;
		points[2][0] = x1 + nx * outer * (real)side; points[2][1] = y1 + ny * outer * (real)side;
		points[3][0] = x0 + nx * outer * (real)side; points[3][1] = y0 + ny * outer * (real)side;
		for (k = 0; k < 4; k++)
			colors[k] = k < 2 ? peak : clear;
		ae_hud_fill_quad(points, colors);
	}
}

/* the polygon's outline moved d px out (in: d < 0), each corner mitred */
static void offset_polygon(const real (*points)[2], short count, real d, real (*out)[2])
{
	real area = 0.0f, sign;
	short i;

	for (i = 0; i < count; i++)
		area += points[i][0] * points[(i + 1) % count][1] - points[(i + 1) % count][0] * points[i][1];
	sign = area > 0.0f ? 1.0f : -1.0f;
	for (i = 0; i < count; i++)
	{
		const real *p = points[(i + count - 1) % count], *q = points[i], *n = points[(i + 1) % count];
		real ax = q[0] - p[0], ay = q[1] - p[1], bx = n[0] - q[0], by = n[1] - q[1];
		real la = (real)sqrt(ax * ax + ay * ay), lb = (real)sqrt(bx * bx + by * by);
		real n0x, n0y, n1x, n1y, dot;

		if (la <= 0.0f)
			la = 1.0f;
		if (lb <= 0.0f)
			lb = 1.0f;
		n0x = sign * ay / la; n0y = -sign * ax / la;
		n1x = sign * by / lb; n1y = -sign * bx / lb;
		/* (a miter limit of 2: a chevron's sharp feet would otherwise grow long spikes) */
		dot = n0x * n1x + n0y * n1y;
		if (dot < -0.5f)
			dot = -0.5f;
		out[i][0] = q[0] + d * (n0x + n1x) / (1.0f + dot);
		out[i][1] = q[1] + d * (n0y + n1y) / (1.0f + dot);
	}
}

void ae_panel_fill_polygon_soft(const real (*points)[2], short count, real grow, const union real_argb_color *color)
{
	real inner[MAXIMUM_POINTS][2], outer[MAXIMUM_POINTS][2];
	real_argb_color clear;
	short i;

	if (count < 3 || count > MAXIMUM_POINTS)
		return;
	offset_polygon(points, count, grow - FEATHER, inner);
	offset_polygon(points, count, grow + FEATHER, outer);
	clear_of(color, &clear);
	/* the inner shape as a fan from its middle, then a 1 px ramp across every edge */
	fan((const real (*)[2])inner, count, color, 0.0f, 0.0f);
	for (i = 0; i < count; i++)
	{
		real quad[4][2];
		real_argb_color colors[4];
		short j = (short)((i + 1) % count);

		quad[0][0] = inner[i][0]; quad[0][1] = inner[i][1];
		quad[1][0] = inner[j][0]; quad[1][1] = inner[j][1];
		quad[2][0] = outer[j][0]; quad[2][1] = outer[j][1];
		quad[3][0] = outer[i][0]; quad[3][1] = outer[i][1];
		colors[0] = colors[1] = *color;
		colors[2] = colors[3] = clear;
		ae_hud_fill_quad(quad, colors);
	}
}

void ae_panel_draw_polygon(const real (*points)[2], short count, short tone, real alpha)
{
	real_argb_color color;

	tone_color(tone, alpha, &color);
	fan(points, count, &color, 0.0f, 0.0f);
}

/* a polygon with the strip's halo (a soft dark rim 1.5 px out), then the fill; both with soft edges */
static void polygon_keyed(const real (*points)[2], short count, short tone, real alpha)
{
	real_argb_color dark, fill;

	tone_color(_ae_tone_dark, 0.60f, &dark);
	tone_color(tone, alpha, &fill);
	ae_panel_fill_polygon_soft(points, count, HALO_OFFSET, &dark);
	ae_panel_fill_polygon_soft(points, count, 0.0f, &fill);
}

/* the corners of a leaning box, clockwise on the screen: bottom-left, top-left, top-right, bottom-right */
static void para_corners(const struct ae_panel_para *p, real corners[4][2])
{
	real shift = p->lean * (p->y1 - p->y0);

	corners[0][0] = p->x0; corners[0][1] = p->y1;
	corners[1][0] = p->x0 + shift; corners[1][1] = p->y0;
	corners[2][0] = p->x1 + shift; corners[2][1] = p->y0;
	corners[3][0] = p->x1; corners[3][1] = p->y1;
}

/* the rounded box's outline offset by d (out: +), as a polygon of 4 x (CORNER_STEPS + 1) points: each corner an arc
of radius (radius + d) round the corner's centre (the box inset by the radius) */
static short rounded(const real corners[4][2], real radius, real d, real out[][2])
{
	real normals[4][2];
	short i, count = 0;

	for (i = 0; i < 4; i++)
	{
		const real *a = corners[i], *b = corners[(i + 1) % 4];
		real dx = b[0] - a[0], dy = b[1] - a[1], length = (real)sqrt(dx * dx + dy * dy);

		if (length <= 0.0f)
			length = 1.0f;
		normals[i][0] = dy / length;   /* outward, for a clockwise polygon with y down */
		normals[i][1] = -dx / length;
	}
	for (i = 0; i < 4; i++)
	{
		const real *before = normals[(i + 3) % 4], *after = normals[i];
		real dot = before[0] * after[0] + before[1] * after[1];
		real inset = radius > -d ? radius : -d;     /* deeper than the radius: the corner sharp, the box inset */
		real k = inset / (1.0f + dot);
		real centre[2], from, to, r = radius + d;
		short step;

		centre[0] = corners[i][0] - k * (before[0] + after[0]);
		centre[1] = corners[i][1] - k * (before[1] + after[1]);
		if (r < 0.0f)
			r = 0.0f;
		from = (real)atan2(before[1], before[0]);
		to = (real)atan2(after[1], after[0]);
		while (to < from)
			to += 2.0f * AE_PI;
		while (to - from > 2.0f * AE_PI)
			to -= 2.0f * AE_PI;
		for (step = 0; step <= CORNER_STEPS; step++)
		{
			real angle = from + (to - from) * (real)step / (real)CORNER_STEPS;

			out[count][0] = centre[0] + r * (real)cos(angle);
			out[count][1] = centre[1] + r * (real)sin(angle);
			count++;
		}
	}
	return count;
}

/* the band between two offsets of the rounded box, colour c0 at offset d0 shading to c1 at d1 */
static void rounded_band_ramp(const real corners[4][2], real radius, real d0, real d1, const real_argb_color *c0,
	const real_argb_color *c1)
{
	real a[MAXIMUM_POINTS][2], b[MAXIMUM_POINTS][2];
	short count = rounded(corners, radius, d0, a), i;

	rounded(corners, radius, d1, b);
	ae_hud_quads_begin();
	for (i = 0; i < count; i++)
	{
		real points[4][2];
		real_argb_color colors[4];
		short j = (short)((i + 1) % count);

		points[0][0] = a[i][0]; points[0][1] = a[i][1];
		points[1][0] = a[j][0]; points[1][1] = a[j][1];
		points[2][0] = b[j][0]; points[2][1] = b[j][1];
		points[3][0] = b[i][0]; points[3][1] = b[i][1];
		colors[0] = colors[1] = *c0;
		colors[2] = colors[3] = *c1;
		ae_hud_fill_quad(points, colors);
	}
	ae_hud_quads_end();
}

static void rounded_band(const real corners[4][2], real radius, real d0, real d1, const real_argb_color *color)
{
	rounded_band_ramp(corners, radius, d0, d1, color, color);
}

/* the band d0..d1 of the rounded box with soft edges: solid inside, a 1 px ramp across each edge */
static void rounded_band_soft(const real corners[4][2], real radius, real d0, real d1, const real_argb_color *color)
{
	real_argb_color clear;

	if (d1 - d0 <= 0.0f)
		return;
	clear_of(color, &clear);
	rounded_band_ramp(corners, radius, d0 - FEATHER, d0 + FEATHER, &clear, color);
	if (d1 - d0 > 2.0f * FEATHER)
		rounded_band(corners, radius, d0 + FEATHER, d1 - FEATHER, color);
	rounded_band_ramp(corners, radius, d1 - FEATHER, d1 + FEATHER, color, &clear);
}

/* the strip's line and its under-stroke widths at a scale */
static void strip_widths(real scale, real *line, real *under)
{
	*line = STRIP_LINE * scale;
	*under = STRIP_UNDER * scale;
	if (*line < STRIP_LINE_MINIMUM)
		*line = STRIP_LINE_MINIMUM;
	if (*under < STRIP_UNDER_MINIMUM)
		*under = STRIP_UNDER_MINIMUM;
}

/* the outline rule = the strip's outline4: a wide soft dark under-stroke, then the line over its middle; the line's
outer edge on the shape's edge */
static void outline(const real corners[4][2], real scale, short line_tone, real line_alpha)
{
	real_argb_color dark, line;
	real w, under, radius = CORNER_RADIUS * scale, centre;

	strip_widths(scale, &w, &under);
	centre = -w / 2.0f;
	tone_color(_ae_tone_dark, STRIP_UNDER_ALPHA, &dark);
	tone_color(line_tone, line_alpha, &line);
	rounded_band_soft(corners, radius, centre - under / 2.0f, centre + under / 2.0f, &dark);
	rounded_band_soft(corners, radius, centre - w / 2.0f, centre + w / 2.0f, &line);
}

void ae_panel_draw_box(const struct ae_panel_para *box, real scale, real fill_alpha, short line_tone)
{
	real corners[4][2];
	real w, under;

	strip_widths(scale, &w, &under);
	para_corners(box, corners);
	if (fill_alpha > 0.0f)
	{
		/* the fill: the strip's dark track (any fill asked for draws it) */
		real inner[MAXIMUM_POINTS][2];
		real_argb_color track;

		tone_color(_ae_tone_dark, TRACK_ALPHA, &track);
		fan((const real (*)[2])inner, rounded(corners, CORNER_RADIUS * scale, -w, inner), &track, 0.0f, 0.0f);
	}
	outline(corners, scale, line_tone, line_tone == _ae_tone_red ? 1.0f : LINE_ALPHA);
}

/* a leaning band inside the bar: from fraction f0 to f1 of its inner length, rows t0 .. t1 of its inner height */
static void bar_band(const struct ae_panel_para *bar, real w, real f0, real f1, real t0, real t1,
	const real_argb_color *color)
{
	real inner_x0 = bar->x0 + w, inner_length = (bar->x1 - bar->x0) - 2.0f * w;
	real top = bar->y0 + w, bottom = bar->y1 - w, height = bottom - top;
	real y0 = top + height * t0, y1 = top + height * t1;
	real points[4][2];
	real_argb_color colors[4], clear;
	real a = inner_x0 + inner_length * f0, b = inner_x0 + inner_length * f1;
	/* the slanted leading edge's 1 px soft ramp, as an x offset: 0.5 px across the lean */
	real soft = FEATHER * (real)sqrt(1.0f + bar->lean * bar->lean);
	real solid_end = b - soft > a ? b - soft : a;
	short k;

	if (b - a < 0.3f || height <= 0.0f)
		return;
	points[0][0] = a + bar->lean * (bar->y1 - w - y1); points[0][1] = y1;
	points[1][0] = a + bar->lean * (bar->y1 - w - y0); points[1][1] = y0;
	points[2][0] = solid_end + bar->lean * (bar->y1 - w - y0); points[2][1] = y0;
	points[3][0] = solid_end + bar->lean * (bar->y1 - w - y1); points[3][1] = y1;
	if (solid_end > a)
		ae_hud_fill_quad_color(points, color);
	points[0][0] = points[3][0]; points[1][0] = points[2][0];
	points[2][0] = b + soft + bar->lean * (bar->y1 - w - y0);
	points[3][0] = b + soft + bar->lean * (bar->y1 - w - y1);
	clear_of(color, &clear);
	for (k = 0; k < 4; k++)
		colors[k] = k < 2 ? *color : clear;
	ae_hud_fill_quad(points, colors);
}

static void draw_bar(const struct ae_panel_para *bar, real scale, real fill, short fill_tone, short line_tone,
	real overshield1, real overshield2, real fade, boolean from_right, real trail, real trail_alpha);

void ae_panel_draw_bar(const struct ae_panel_para *bar, real scale, real fill, short fill_tone, short line_tone,
	real overshield1, real overshield2)
{
	draw_bar(bar, scale, fill, fill_tone, line_tone, overshield1, overshield2, 1.0f, FALSE, 0.0f, 0.0f);
}

/* (the shield's bar: it fills from its right end and drains toward it, as CE's shield meter does, its overshield
layers too; owner 10-15) */
void ae_panel_draw_bar_faded(const struct ae_panel_para *bar, real scale, real fill, short fill_tone, short line_tone,
	real overshield1, real overshield2, real fade, real trail, real trail_alpha)
{
	draw_bar(bar, scale, fill, fill_tone, line_tone, overshield1, overshield2, fade, TRUE, trail, trail_alpha);
}

/* a band from the bar's left end (or its right end) over this much of its length */
static void bar_span(const struct ae_panel_para *bar, real w, real amount, boolean from_right, real t0, real t1,
	const real_argb_color *color)
{
	if (amount > 1.0f)
		amount = 1.0f;
	if (from_right)
		bar_band(bar, w, 1.0f - amount, 1.0f, t0, t1, color);
	else
		bar_band(bar, w, 0.0f, amount, t0, t1, color);
}

static void draw_bar(const struct ae_panel_para *bar, real scale, real fill, short fill_tone, short line_tone,
	real overshield1, real overshield2, real fade, boolean from_right, real trail, real trail_alpha)
{
	real corners[4][2];
	real w, under;
	real_argb_color color, dark;
	short cell;

	strip_widths(scale, &w, &under);
	para_corners(bar, corners);
	/* the strip's bar: the dark track, the fill, the dividers, the outline */
	tone_color(_ae_tone_dark, TRACK_ALPHA, &color);
	bar_band(bar, w, 0.0f, 1.0f, 0.0f, 1.0f, &color);
	if (fill > 0.0f)
	{
		tone_color(fill_tone, FILL_ALPHA * fade, &color);
		bar_span(bar, w, fill, from_right, 0.0f, 1.0f, &color);
	}
	/* CE's hit trail: the part a hit just took, in white, held then fading (ae_panel_trail_step) */
	if (trail_alpha > 0.0f && trail > fill)
	{
		real end = trail > 1.0f ? 1.0f : trail;

		tone_color(_ae_tone_white, FILL_ALPHA * 0.85f * trail_alpha, &color);
		if (from_right)
			bar_band(bar, w, 1.0f - end, 1.0f - (fill > 0.0f ? fill : 0.0f), 0.0f, 1.0f, &color);
		else
			bar_band(bar, w, fill > 0.0f ? fill : 0.0f, end, 0.0f, 1.0f, &color);
	}
	/* the overshield's layers in the classic HUD's colours (its first layer pink, the second grass green over it; the
	game's own red/green, as the shipped HUD tags shade them), each over the one below, a light edge on top (owner
	10-15: the plain red read as a low shield) */
	if (overshield1 > 0.0f)
	{
		real height = (bar->y1 - bar->y0) - 2.0f * w;

		tone_color(_ae_tone_overshield1, FILL_ALPHA, &color);
		bar_span(bar, w, overshield1, from_right, 0.0f, 1.0f, &color);
		tone_color(_ae_tone_white, FILL_ALPHA * 0.6f, &color);
		bar_span(bar, w, overshield1, from_right, 0.0f, height > 0.0f ? 1.0f / height : 0.0f, &color);
	}
	if (overshield2 > 0.0f)
	{
		real height = (bar->y1 - bar->y0) - 2.0f * w;

		tone_color(_ae_tone_overshield2, FILL_ALPHA, &color);
		bar_span(bar, w, overshield2, from_right, 0.0f, 1.0f, &color);
		tone_color(_ae_tone_white, FILL_ALPHA * 0.6f, &color);
		bar_span(bar, w, overshield2, from_right, 0.0f, height > 0.0f ? 1.0f / height : 0.0f, &color);
	}
	/* the dividers between 5 cells */
	tone_color(_ae_tone_dark, STRIP_DIVIDER_ALPHA, &dark);
	for (cell = 1; cell < 5; cell++)
	{
		real inner_length = (bar->x1 - bar->x0) - 2.0f * w;
		real x = bar->x0 + w + inner_length * (real)cell / 5.0f;
		real top = bar->y0 + w, bottom = bar->y1 - w;

		line_soft(x + bar->lean * (bottom - top), top, x, bottom, w, &dark);
	}
	outline(corners, scale, line_tone, (line_tone == _ae_tone_red ? 1.0f : LINE_ALPHA) * fade);
}

/* ---------- CE's art */

/* one round of a hud_ammo_alphas sheet (measured on port/assets/hud by the art table tool: a middle round's ink box,
and the slanted strip round it that holds it and no neighbour: its centre's x at the ink's top and bottom and its
half width), all in the PNG's texels */
struct art_strip { short bitmap; short width, height; real ink_x0, ink_x1, ink_y0, ink_y1, top_x, bottom_x, half; };
static const struct art_strip art_strips[] = {
	{ 1, 1024, 256, 104, 147, 4, 64, 142.41f, 114.61f, 12.24f },   /* AR */
	{ 5, 1024, 128, 66, 142, 0, 117, 130.18f, 85.45f, 27.28f },    /* pistol */
	{ 7, 1024, 128, 72, 175, 0, 128, 155.75f, 96.08f, 26.29f },    /* shell */
	{ 4, 1024, 128, 42, 122, 0, 119, 111.60f, 50.38f, 17.04f },    /* needle */
	{ 6, 1024, 256, 40, 424, 0, 72, 232.00f, 232.00f, 193.00f },   /* sniper (lying: its box) */
	{ 2, 1024, 128, 0, 428, 0, 128, 214.00f, 214.00f, 215.00f },   /* rocket (lying: its box) */
};

/* a glyph's box in its sheet (texels of the PNG) */
struct glyph_box { short bitmap; short width, height; real x0, x1, y0, y1; };
static const struct glyph_box round_box = { 0, 256, 128, 96, 160, 0, 80 };   /* hud_ammo_type_icons 0's round */
static const struct glyph_box frag_box = { 2, 256, 128, 8, 73, 0, 78 };      /* hud_ammo_type_icons 2 */

/* the SOLID silhouettes (holes filled), x and y over their boxes: hud_unit_backgrounds 0's shield icon (80 x 88) and
hud_ammo_type_icons 4's battery (210 x 80) */
static const real shield_shape[][2] = { { 0.9962f, 0.7169f }, { 0.9345f, 0.9540f }, { 0.8882f, 0.9950f },
	{ 0.6286f, 0.8613f }, { 0.2032f, 0.4028f }, { 0.0162f, 0.0411f }, { 0.0578f, 0.0011f }, { 0.7905f, 0.0012f },
	{ 0.8498f, 0.0584f }, { 0.9599f, 0.3427f } };
#define SHIELD_ASPECT (80.0f / 88.0f)
static const real battery_shape[][2] = { { 0.9333f, 0.6965f }, { 0.8329f, 0.7313f }, { 0.7938f, 0.9467f },
	{ 0.7698f, 0.9990f }, { 0.0827f, 0.9981f }, { -0.0041f, 0.9702f }, { 0.1711f, 0.0029f }, { 0.8962f, 0.0023f },
	{ 0.9512f, 0.0575f }, { 0.9169f, 0.2982f }, { 0.9834f, 0.3813f } };
#define BATTERY_ASPECT (210.0f / 80.0f)

/* the three sheets AE's panels draw from */
enum { _sheet_ammo_alphas, _sheet_type_icons, _sheet_unit_backgrounds, NUMBER_OF_SHEETS };
static const char *const sheet_tags[NUMBER_OF_SHEETS] = {
	"ui\\hud\\bitmaps\\combined\\hud_ammo_alphas",
	"ui\\hud\\bitmaps\\combined\\hud_ammo_type_icons",
	"ui\\hud\\bitmaps\\combined\\hud_unit_backgrounds" };

/* their bitmap groups, looked up once a map (tag_loaded walks every tag of the map, and a magazine draws a round at a
time): forgotten when a map's tags load or go (ae_panel_draw_tags_changed) */
static long sheet_groups[NUMBER_OF_SHEETS];
static boolean sheet_groups_known = FALSE;

/* MAPS WITHOUT THE STOCK SHEETS (Custom Edition and Gearbox maps; owner 2026-10-26: "draw from that art"): the panels
draw the high-res HUD's copies of the stock sheets (port/assets/hud, Halo PC Restored's redraws), through a
placeholder bitmap the texture cache draws that texture for (ae_hud_digits.c's way). A copy is in its Xbox sheet's
layout, one bitmap to a sequence, so the art tables above read it as they read the Xbox maps' sheets. With
display.high_res_hud off, or a copy that can't be drawn, the stand-ins further down are. */
#define SHEET_COPIES 8
static struct
{
	struct bitmap_data *placeholder;
	long asset;
	boolean attempted;
} sheet_copies[NUMBER_OF_SHEETS][SHEET_COPIES];

static void sheet_copies_forget(void)
{
	short sheet, index;

	for (sheet = 0; sheet < NUMBER_OF_SHEETS; sheet++)
		for (index = 0; index < SHEET_COPIES; index++)
			if (sheet_copies[sheet][index].placeholder)
			{
				hud_hires_register_placeholder(sheet_copies[sheet][index].asset, NULL);
				bitmap_delete(sheet_copies[sheet][index].placeholder);
			}
	csmemset(sheet_copies, 0, sizeof(sheet_copies));
}

/* the placeholder drawn as bitmap `index` of a sheet's high-res copy, made when first needed; NULL for none */
static struct bitmap_data const *sheet_copy(short sheet, short index)
{
	long asset;

	if (sheet < 0 || sheet >= NUMBER_OF_SHEETS || index < 0 || index >= SHEET_COPIES ||
		!config_boolean("display.high_res_hud"))
	{
		return NULL;
	}
	if (sheet_copies[sheet][index].attempted)
		return sheet_copies[sheet][index].placeholder;
	sheet_copies[sheet][index].attempted = TRUE;
	for (asset = 0; asset < hud_hires_asset_count(); asset++)
	{
		struct bitmap_data *bitmap;
		unsigned long const *texture;
		unsigned long levels = 0;

		if (_stricmp(hud_hires_asset_tag(asset), sheet_tags[sheet]) || hud_hires_asset_bitmap(asset) != index)
			continue;
		/* (a small square a8r8g8b8 bitmap: the quads take the texture's coordinates 0 to 1, whatever its size) */
		bitmap = bitmap_2d_new(64, 64, 0, 11);
		if (bitmap && !rasterizer_bitmap_new(bitmap))
		{
			bitmap_delete(bitmap);
			bitmap = NULL;
		}
		if (!bitmap)
			break;
		texture = (unsigned long const *)xbox_pointer(bitmap->hardware_format);
		hud_hires_register_placeholder(asset, texture);
		/* (the placeholders' table full, or the PNG not decoded: the stand-ins, not an empty quad) */
		if (!hud_hires_placeholder_texture(texture[1], &levels))
		{
			hud_hires_register_placeholder(asset, NULL);
			bitmap_delete(bitmap);
			platform_log("ae panel: no high-res copy of %s %d drawn; using procedural stand-ins", sheet_tags[sheet],
				index);
			break;
		}
		sheet_copies[sheet][index].placeholder = bitmap;
		sheet_copies[sheet][index].asset = asset;
		platform_log("ae panel: sheet %s %d drawn from its high-res copy", sheet_tags[sheet], index);
		break;
	}
	return sheet_copies[sheet][index].placeholder;
}

void ae_panel_draw_tags_changed(void)
{
	sheet_groups_known = FALSE;
	sheet_copies_forget();
}

static long sheet_group(short sheet)
{
	if (!sheet_groups_known)
	{
		short i;

		for (i = 0; i < NUMBER_OF_SHEETS; i++)
		{
			sheet_groups[i] = tag_loaded(BITMAP_GROUP_TAG, sheet_tags[i]);
			if (sheet_groups[i] == NONE)
				platform_log("ae panel: sheet %s missing; its high-res copy, else stand-ins", sheet_tags[i]);
		}
		sheet_groups_known = TRUE;
	}
	return sheet >= 0 && sheet < NUMBER_OF_SHEETS ? sheet_groups[sheet] : NONE;
}

/* a sheet of a HUD bitmap group, its texture ready: sequence `index`, and its clip (texels for an interface group,
else 0..1) into which the PNG's fractions map. On the Xbox maps the sequence has no sprites: its bitmap is the
whole of it (the PNG's layout). Halo PC's sheets (a Custom Edition map's, from bitmaps.map) pack the same art as
sprites of one bitmap: *sprite TRUE, the clip the sprite's. */
static struct bitmap_data const *sheet_bitmap(short sheet, short index, real_rectangle2d *clip, boolean *sprite)
{
	long group_index = sheet_group(sheet);
	struct bitmap_group const *group;
	struct bitmap_group_sequence const *sequence;
	struct bitmap_data const *bitmap = NULL;

	*sprite = FALSE;
	if (group_index == NONE)
	{
		bitmap = sheet_copy(sheet, index);
		if (bitmap)
		{
			clip->x0 = 0.0f;
			clip->y0 = 0.0f;
			clip->x1 = 1.0f;
			clip->y1 = 1.0f;
		}
		return bitmap;
	}
	group = bitmap_group_get(group_index);
	if (!group || index < 0 || index >= group->sequences.count)
		return NULL;
	sequence = TAG_BLOCK_GET_ELEMENT(&group->sequences, index, struct bitmap_group_sequence);
	if (sequence->sprites.count > 0)
	{
		real_rectangle2d const *sprite_clip = NULL;

		hud_retrieve_bitmap_and_bounding_rect(group_index, index, 0, &bitmap, &sprite_clip);
		if (!bitmap || !sprite_clip)
			return NULL;
		*clip = *sprite_clip;
		*sprite = TRUE;
	}
	else
	{
		boolean interface = (boolean)(group->type == AE_BITMAP_GROUP_INTERFACE);

		if (sequence->first_bitmap_index < 0 || sequence->first_bitmap_index >= group->bitmaps.count)
			return NULL;
		bitmap = TAG_BLOCK_GET_ELEMENT(&group->bitmaps, sequence->first_bitmap_index, struct bitmap_data);
		clip->x0 = 0.0f;
		clip->y0 = 0.0f;
		clip->x1 = interface ? (real)bitmap->width : 1.0f;
		clip->y1 = interface ? (real)bitmap->height : 1.0f;
	}
	if (bitmap->width <= 0 || bitmap->height <= 0 ||
		!_texture_cache_bitmap_get_hardware_format((struct bitmap_data *)bitmap, FALSE, TRUE))
	{
		return NULL;
	}
	return bitmap;
}

/* Halo PC's bullet sprites hold the Xbox art at about twice its size with a margin: each art's ink box as fractions
of the Xbox bitmap and of the PC sprite (Custom Edition's bitmaps.map, MCC's), so an Xbox fraction maps across */
struct pc_art_box { short sequence; real xbox_x0, xbox_x1, xbox_y0, xbox_y1, pc_x0, pc_x1, pc_y0, pc_y1; };
static const struct pc_art_box pc_art_boxes[] = {
	{ 1, 0.0000f, 0.8691f, 0.0156f, 0.7812f, 0.0044f, 0.9912f, 0.0377f, 0.9623f },
	{ 5, 0.0000f, 0.8271f, 0.0000f, 0.9141f, 0.0093f, 0.9953f, 0.0606f, 0.9394f },
	{ 7, 0.0020f, 0.8535f, 0.0000f, 1.0000f, 0.0077f, 0.8456f, 0.0286f, 0.9714f },
	{ 4, 0.0010f, 0.8477f, 0.0000f, 0.9297f, 0.0045f, 0.9910f, 0.0303f, 0.9697f },
	{ 6, 0.0000f, 0.8203f, 0.0000f, 0.6250f, 0.0845f, 0.9577f, 0.0465f, 0.9535f },
	{ 2, 0.0000f, 0.8359f, 0.0000f, 1.0000f, 0.0415f, 0.9677f, 0.0286f, 0.9429f },
};

static void pc_art_map(short sequence, real uv[4][2])
{
	short i, k;

	for (i = 0; i < (short)NUMBEROF(pc_art_boxes); i++)
		if (pc_art_boxes[i].sequence == sequence)
		{
			const struct pc_art_box *b = &pc_art_boxes[i];

			for (k = 0; k < 4; k++)
			{
				uv[k][0] = b->pc_x0 + (uv[k][0] - b->xbox_x0) * (b->pc_x1 - b->pc_x0) / (b->xbox_x1 - b->xbox_x0);
				uv[k][1] = b->pc_y0 + (uv[k][1] - b->xbox_y0) * (b->pc_y1 - b->pc_y0) / (b->xbox_y1 - b->xbox_y0);
			}
			return;
		}
}

boolean ae_panel_art_found(short *alphas, short *icons, short *backgrounds)
{
	long group_index;

	group_index = sheet_group(_sheet_ammo_alphas);
	*alphas = (short)(group_index == NONE ? -1 : bitmap_group_get(group_index)->sequences.count);
	group_index = sheet_group(_sheet_type_icons);
	*icons = (short)(group_index == NONE ? -1 : bitmap_group_get(group_index)->sequences.count);
	group_index = sheet_group(_sheet_unit_backgrounds);
	*backgrounds = (short)(group_index == NONE ? -1 : bitmap_group_get(group_index)->sequences.count);
	return (boolean)(*alphas > 6 && *icons > 2);
}

/* a textured quad: points clockwise from the top-left, texture coordinates as fractions of the PNG (the sheet's
layout) */
static void sheet_quad(struct bitmap_data const *bitmap, const real_rectangle2d *clip, const real points[4][2],
	const real uv[4][2], pixel32 color)
{
	struct dynamic_screen_vertex vertices[4];
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	real units = 1.0f / ae_hud_pixels_per_unit();
	short i;

	for (i = 0; i < 4; i++)
	{
		vertices[i].position.x = points[i][0] * units;
		vertices[i].position.y = points[i][1] * units;
		vertices[i].texture_coordinates.x = clip->x0 + uv[i][0] * (clip->x1 - clip->x0);
		vertices[i].texture_coordinates.y = clip->y0 + uv[i][1] * (clip->y1 - clip->y0);
		vertices[i].color = color;
	}
	csmemset(&parameters, 0, sizeof(parameters));
	parameters.map_texture_scale[0].i = 1.0f;
	parameters.map_texture_scale[0].j = 1.0f;
	parameters.map_scale[0].i = 1.0f;
	parameters.map_scale[0].j = 1.0f;
	parameters.framebuffer_blend_function = AE_BLEND_ALPHA_MULTIPLY_ADD;
	parameters.map[0] = (struct bitmap_data *)bitmap;
	rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
}

/* a sheet's colour: premultiplied by its alpha (the HUD's blend, alpha multiply add, adds the colour to what is under
it by 1 - alpha; measured: a 0.28 round drew white on grey without it) */
static pixel32 tone_pixel(short tone, real alpha)
{
	real_argb_color color;

	tone_color(tone, alpha, &color);
	color.red *= color.alpha;
	color.green *= color.alpha;
	color.blue *= color.alpha;
	return real_argb_color_to_pixel32(&color);
}

static void draw_shape(const real (*shape)[2], short count, real aspect, real x, real y, real height, short tone,
	real alpha)
{
	real points[16][2];
	short i;

	for (i = 0; i < count && i < 16; i++)
	{
		points[i][0] = x + shape[i][0] * height * aspect;
		points[i][1] = y + shape[i][1] * height;
	}
	polygon_keyed((const real (*)[2])points, i, tone, alpha);
}

/* a piece of a sheet drawn with its keyline; texels are the PNG's; the quad's 4 texel corners map onto the screen
by origin + (texel - texel_origin) x scale */
static void sheet_piece(struct bitmap_data const *bitmap, const real_rectangle2d *clip, short pc_sequence,
	short width, short height,
	const real texels[4][2], real origin_x, real origin_y, real texel_x, real texel_y, real scale, short tone,
	real alpha, real keyline_alpha)
{
	real points[4][2], uv[4][2];
	short i, pass;

	for (i = 0; i < 4; i++)
	{
		points[i][0] = origin_x + (texels[i][0] - texel_x) * scale;
		points[i][1] = origin_y + (texels[i][1] - texel_y) * scale;
		uv[i][0] = texels[i][0] / (real)width;
		uv[i][1] = texels[i][1] / (real)height;
	}
	if (pc_sequence >= 0)
		pc_art_map(pc_sequence, uv);
	if (keyline_alpha > 0.0f)
	{
		/* the strip digits' glow: a soft halo, 8 passes HALO_OFFSET out at a share of the keyline's alpha */
		pixel32 dark = tone_pixel(_ae_tone_dark, keyline_alpha * HALO_ALPHA);

		for (pass = 0; pass < 8; pass++)
		{
			real moved[4][2];

			for (i = 0; i < 4; i++)
			{
				moved[i][0] = points[i][0] + around[pass][0] * HALO_OFFSET;
				moved[i][1] = points[i][1] + around[pass][1] * HALO_OFFSET;
			}
			sheet_quad(bitmap, clip, moved, uv, dark);
		}
	}
	sheet_quad(bitmap, clip, points, uv, tone_pixel(tone, alpha));
}

/* Stand-ins for maps without the stock HUD sheets. They occupy the measured ink boxes above: standing ammunition
leans with the panel, while the sniper cartridge and rocket lie along their rows. The stock sheet path never reaches
these silhouettes. */
static const real fallback_ar[][2] = {
	{ 0.94f, 0.00f }, { 1.00f, 0.13f }, { 0.49f, 0.82f }, { 0.43f, 1.00f },
	{ 0.00f, 0.82f }, { 0.10f, 0.66f }, { 0.68f, 0.08f } };
static const real fallback_pistol[][2] = {
	{ 0.84f, 0.00f }, { 1.00f, 0.12f }, { 0.57f, 0.82f }, { 0.48f, 1.00f },
	{ 0.00f, 0.79f }, { 0.11f, 0.62f }, { 0.65f, 0.07f } };
static const real fallback_shell[][2] = {
	{ 0.74f, 0.00f }, { 1.00f, 0.10f }, { 0.55f, 0.92f }, { 0.39f, 1.00f },
	{ 0.00f, 0.84f }, { 0.09f, 0.65f }, { 0.62f, 0.04f } };
static const real fallback_needle[][2] = {
	{ 1.00f, 0.00f }, { 0.73f, 0.31f }, { 0.50f, 0.84f }, { 0.30f, 1.00f },
	{ 0.00f, 0.78f }, { 0.34f, 0.45f } };
static const real fallback_sniper[][2] = {
	{ 0.00f, 0.19f }, { 0.67f, 0.19f }, { 0.78f, 0.03f }, { 1.00f, 0.50f },
	{ 0.78f, 0.97f }, { 0.67f, 0.81f }, { 0.00f, 0.81f }, { 0.05f, 0.50f } };
static const real fallback_rocket[][2] = {
	{ 0.00f, 0.30f }, { 0.16f, 0.30f }, { 0.27f, 0.08f }, { 0.78f, 0.19f },
	{ 1.00f, 0.50f }, { 0.78f, 0.81f }, { 0.27f, 0.92f }, { 0.16f, 0.70f }, { 0.00f, 0.70f } };

static void draw_art_fallback(short art, const struct art_strip *strip, real x, real y, real height, boolean lit,
	short tone)
{
	const real (*shape)[2] = fallback_ar;
	short count = (short)NUMBEROF(fallback_ar);
	real aspect = (strip->ink_x1 - strip->ink_x0) / (strip->ink_y1 - strip->ink_y0);

	switch (art)
	{
	case _ae_art_pistol: shape = fallback_pistol; count = (short)NUMBEROF(fallback_pistol); break;
	case _ae_art_shell: shape = fallback_shell; count = (short)NUMBEROF(fallback_shell); break;
	case _ae_art_needle: shape = fallback_needle; count = (short)NUMBEROF(fallback_needle); break;
	case _ae_art_sniper: shape = fallback_sniper; count = (short)NUMBEROF(fallback_sniper); break;
	case _ae_art_rocket: shape = fallback_rocket; count = (short)NUMBEROF(fallback_rocket); break;
	default: break;
	}
	draw_shape(shape, count, aspect, x, y, height, lit ? tone : _ae_tone_blue,
		lit ? ROUND_LIT_ALPHA : SPENT_ALPHA);
}

void ae_panel_draw_art(short art, real x, real y, real height, boolean lit, short tone)
{
	const struct art_strip *strip;
	struct bitmap_data const *bitmap;
	real_rectangle2d clip;
	boolean sprite;
	real texels[4][2], scale;

	if (art < 0 || art >= (short)NUMBEROF(art_strips) || height <= 0.0f)
		return;
	strip = &art_strips[art];
	bitmap = sheet_bitmap(_sheet_ammo_alphas, strip->bitmap, &clip, &sprite);
	if (!bitmap)
	{
		draw_art_fallback(art, strip, x, y, height, lit, tone);
		return;
	}
	scale = height / (strip->ink_y1 - strip->ink_y0);
	texels[0][0] = strip->top_x - strip->half; texels[0][1] = strip->ink_y0;
	texels[1][0] = strip->top_x + strip->half; texels[1][1] = strip->ink_y0;
	texels[2][0] = strip->bottom_x + strip->half; texels[2][1] = strip->ink_y1;
	texels[3][0] = strip->bottom_x - strip->half; texels[3][1] = strip->ink_y1;
	sheet_piece(bitmap, &clip, sprite ? strip->bitmap : -1, strip->width, strip->height, texels, x, y, strip->ink_x0,
		strip->ink_y0, scale,
		lit ? tone : _ae_tone_blue, lit ? ROUND_LIT_ALPHA : SPENT_ALPHA, lit ? ROUND_LIT_KEYLINE : KEYLINE_ALPHA *
		(SPENT_ALPHA + 0.15f));
}

static boolean draw_box_glyph(short sheet, const struct glyph_box *box, real x, real y, real height, short tone,
	real alpha)
{
	struct bitmap_data const *bitmap;
	real_rectangle2d clip;
	boolean sprite;
	real texels[4][2];

	bitmap = sheet_bitmap(sheet, box->bitmap, &clip, &sprite);
	if (!bitmap)
		return FALSE;
	texels[0][0] = box->x0; texels[0][1] = box->y0;
	texels[1][0] = box->x1; texels[1][1] = box->y0;
	texels[2][0] = box->x1; texels[2][1] = box->y1;
	texels[3][0] = box->x0; texels[3][1] = box->y1;
	sheet_piece(bitmap, &clip, -1, box->width, box->height, texels, x, y, box->x0, box->y0,
		height / (box->y1 - box->y0), tone, alpha, KEYLINE_ALPHA);
	return TRUE;
}

real ae_panel_glyph_width(short glyph, real height)
{
	switch (glyph)
	{
	case _ae_glyph_round: return height * (round_box.x1 - round_box.x0) / (round_box.y1 - round_box.y0);
	case _ae_glyph_frag: return height * (frag_box.x1 - frag_box.x0) / (frag_box.y1 - frag_box.y0);
	case _ae_glyph_battery: return height * BATTERY_ASPECT;
	case _ae_glyph_shield: return height * SHIELD_ASPECT;
	default: return height;
	}
}

void ae_panel_draw_glyph(short glyph, real x, real y, real height, short tone, real alpha)
{
	static const real round_fallback[][2] = {
		{ 0.86f, 0.00f }, { 1.00f, 0.14f }, { 0.58f, 0.82f }, { 0.48f, 1.00f },
		{ 0.00f, 0.78f }, { 0.12f, 0.60f }, { 0.67f, 0.07f } };
	static const real frag_fallback[][2] = {
		{ 0.47f, 0.00f }, { 0.78f, 0.00f }, { 0.82f, 0.14f }, { 1.00f, 0.29f },
		{ 0.91f, 0.45f }, { 0.94f, 0.78f }, { 0.72f, 1.00f }, { 0.20f, 0.92f },
		{ 0.00f, 0.66f }, { 0.12f, 0.35f }, { 0.36f, 0.19f } };
	switch (glyph)
	{
	case _ae_glyph_round:
		if (!draw_box_glyph(_sheet_type_icons, &round_box, x, y, height, tone, alpha))
			draw_shape(round_fallback, (short)NUMBEROF(round_fallback),
			(round_box.x1 - round_box.x0) / (round_box.y1 - round_box.y0), x, y, height, tone, alpha);
		break;
	case _ae_glyph_frag:
		if (!draw_box_glyph(_sheet_type_icons, &frag_box, x, y, height, tone, alpha))
			draw_shape(frag_fallback, (short)NUMBEROF(frag_fallback),
			(frag_box.x1 - frag_box.x0) / (frag_box.y1 - frag_box.y0), x, y, height, tone, alpha);
		break;
	case _ae_glyph_battery:
		draw_shape(battery_shape, (short)NUMBEROF(battery_shape), BATTERY_ASPECT, x, y, height, tone, alpha);
		break;
	case _ae_glyph_shield:
		draw_shape(shield_shape, (short)NUMBEROF(shield_shape), SHIELD_ASPECT, x, y, height, tone, alpha);
		break;
	default:
		break;
	}
}

/* ---------- drawn shapes */

static short circle(real cx, real cy, real radius, real points[][2], short steps)
{
	short i;

	for (i = 0; i < steps; i++)
	{
		real angle = 2.0f * AE_PI * (real)i / (real)steps;

		points[i][0] = cx + radius * (real)cos(angle);
		points[i][1] = cy + radius * (real)sin(angle);
	}
	return steps;
}

/* enough segments that a circle's chords stay under a tenth of a pixel off the curve */
static short ring_steps(real radius)
{
	short steps = (short)(radius * 0.75f);

	return steps < 24 ? 24 : steps > MAXIMUM_RING_STEPS ? MAXIMUM_RING_STEPS : steps;
}

/* the band r0..r1 round (cx, cy), colour c0 at r0 shading to c1 at r1 */
static void ring_band_ramp(real cx, real cy, real r0, real r1, short steps, const real_argb_color *c0,
	const real_argb_color *c1)
{
	real a[MAXIMUM_RING_STEPS][2], b[MAXIMUM_RING_STEPS][2];
	short count, i;

	count = circle(cx, cy, r0 > 0.0f ? r0 : 0.0f, a, steps);
	circle(cx, cy, r1 > 0.0f ? r1 : 0.0f, b, steps);
	ae_hud_quads_begin();
	for (i = 0; i < count; i++)
	{
		real points[4][2];
		real_argb_color colors[4];
		short j = (short)((i + 1) % count);

		points[0][0] = a[i][0]; points[0][1] = a[i][1];
		points[1][0] = a[j][0]; points[1][1] = a[j][1];
		points[2][0] = b[j][0]; points[2][1] = b[j][1];
		points[3][0] = b[i][0]; points[3][1] = b[i][1];
		colors[0] = colors[1] = *c0;
		colors[2] = colors[3] = *c1;
		ae_hud_fill_quad(points, colors);
	}
	ae_hud_quads_end();
}

/* the band r0..r1 with soft edges: solid inside, a 1 px ramp across each edge (a band under 1 px: the two ramps
alone at a lower peak, so its weight stays) */
static void ring_band(real cx, real cy, real r0, real r1, const real_argb_color *color)
{
	real_argb_color peak = *color, clear;
	real w = r1 - r0;
	/* (one segment count for every band of the ring: bands that share a circle must share its corners exactly, or
	the seams between them show as specks) */
	short steps = ring_steps(r1 + FEATHER);

	if (w <= 0.0f)
		return;
	if (w >= 2.0f * FEATHER)
	{
		clear_of(color, &clear);
		ring_band_ramp(cx, cy, r0 - FEATHER, r0 + FEATHER, steps, &clear, color);
		if (w > 2.0f * FEATHER)
			ring_band_ramp(cx, cy, r0 + FEATHER, r1 - FEATHER, steps, color, color);
		ring_band_ramp(cx, cy, r1 - FEATHER, r1 + FEATHER, steps, color, &clear);
	}
	else
	{
		real mid = (r0 + r1) / 2.0f, half = w / 2.0f + FEATHER;

		peak.alpha *= w / (2.0f * FEATHER);
		clear_of(&peak, &clear);
		ring_band_ramp(cx, cy, mid - half, mid, steps, &clear, &peak);
		ring_band_ramp(cx, cy, mid, mid + half, steps, &peak, &clear);
	}
}

/* a filled circle with a soft 1 px edge */
static void disc_soft(real cx, real cy, real radius, const real_argb_color *color)
{
	real points[MAXIMUM_RING_STEPS][2];
	real_argb_color clear;
	real inner = radius - FEATHER;
	short steps = ring_steps(radius + FEATHER);

	if (radius <= 0.0f)
		return;
	clear_of(color, &clear);
	if (inner > 0.0f)
		fan((const real (*)[2])points, circle(cx, cy, inner, points, steps), color, 0.0f, 0.0f);
	ring_band_ramp(cx, cy, inner > 0.0f ? inner : 0.0f, radius + FEATHER, steps, color, &clear);
}

void ae_panel_fill_disc(real cx, real cy, real radius, const union real_argb_color *color)
{
	disc_soft(cx, cy, radius, color);
}

void ae_panel_draw_disc(real cx, real cy, real radius, short tone, real alpha, boolean keyline)
{
	real_argb_color color;

	if (radius <= 0.0f)
		return;
	if (keyline)
	{
		tone_color(_ae_tone_dark, KEYLINE_ALPHA, &color);
		disc_soft(cx, cy, radius + 1.0f, &color);
	}
	tone_color(tone, alpha, &color);
	disc_soft(cx, cy, radius, &color);
}

void ae_panel_draw_ring(real cx, real cy, real radius, real width, short tone, real alpha, boolean keyline)
{
	real_argb_color color;

	if (keyline)
	{
		/* the strip's under-stroke: 2.25 x the line's width */
		real under = width * STRIP_UNDER / STRIP_LINE;

		tone_color(_ae_tone_dark, STRIP_UNDER_ALPHA, &color);
		ring_band(cx, cy, radius - under / 2.0f, radius + under / 2.0f, &color);
	}
	tone_color(tone, alpha, &color);
	ring_band(cx, cy, radius - width / 2.0f, radius + width / 2.0f, &color);
}

void ae_panel_draw_plasma(real cx, real cy, real diameter, short tone, real alpha)
{
	real r = diameter / 2.0f;
	real_argb_color dark;

	ae_panel_draw_disc(cx, cy, r, tone, alpha, TRUE);
	tone_color(_ae_tone_dark, 0.85f, &dark);
	ring_band(cx, cy, r * 0.46f - 0.5f, r * 0.46f + 0.5f, &dark);
}

void ae_panel_draw_line(real x0, real y0, real x1, real y1, real width, short tone, real alpha, boolean keyline)
{
	real_argb_color color;

	if (keyline)
	{
		real dx = x1 - x0, dy = y1 - y0, length = (real)sqrt(dx * dx + dy * dy);
		real ux = length > 0.0f ? dx / length : 0.0f, uy = length > 0.0f ? dy / length : 0.0f;
		real under = width * STRIP_UNDER / STRIP_LINE, ends = (under - width) / 2.0f;

		tone_color(_ae_tone_dark, STRIP_UNDER_ALPHA, &color);
		line_soft(x0 - ux * ends, y0 - uy * ends, x1 + ux * ends, y1 + uy * ends, under, &color);
	}
	tone_color(tone, alpha, &color);
	line_soft(x0, y0, x1, y1, width, &color);
}

void ae_panel_draw_x(real x, real baseline_y, real size, real scale, short tone)
{
	/* two strokes in a leaning size x size square: (u, v) = (x + (u + (1 - v) lean) size, top + v size) */
	real top = baseline_y - size, w = 2.2f * scale;
	real ax = x + (0.0f + AE_PANEL_LEAN) * size, bx = x + 0.86f * size;
	real cx = x + (0.86f + AE_PANEL_LEAN) * size, dx = x;

	if (w < 1.2f)
		w = 1.2f;
	ae_panel_draw_line(ax, top, bx, baseline_y, w + 2.0f, _ae_tone_dark, KEYLINE_ALPHA, FALSE);
	ae_panel_draw_line(cx, top, dx, baseline_y, w + 2.0f, _ae_tone_dark, KEYLINE_ALPHA, FALSE);
	ae_panel_draw_line(ax, top, bx, baseline_y, w, tone, 1.0f, FALSE);
	ae_panel_draw_line(cx, top, dx, baseline_y, w, tone, 1.0f, FALSE);
}

void ae_panel_draw_cell(const struct ae_panel_para *cell, boolean lit, short tone)
{
	real corners[4][2];
	real_argb_color color, clear;
	real inner[MAXIMUM_POINTS][2];
	real under = STRIP_UNDER * (cell->y1 - cell->y0) / 10.0f;   /* (a cell is 10 px tall at scale 1) */

	para_corners(cell, corners);
	/* as the strip's round-mode cells: lit = the fill, lost = the dark track; the soft dark under-stroke round it */
	if (under < STRIP_UNDER_MINIMUM)
		under = STRIP_UNDER_MINIMUM;
	tone_color(_ae_tone_dark, STRIP_UNDER_ALPHA, &color);
	rounded_band_soft(corners, 0.0f, -under / 2.0f, under / 2.0f, &color);
	if (lit)
		tone_color(tone, FILL_ALPHA, &color);
	else
		tone_color(_ae_tone_dark, TRACK_ALPHA, &color);
	clear_of(&color, &clear);
	fan((const real (*)[2])inner, rounded(corners, 0.0f, -FEATHER, inner), &color, 0.0f, 0.0f);
	rounded_band_ramp(corners, 0.0f, -FEATHER, FEATHER, &color, &clear);
	if (!lit)
	{
		tone_color(tone == _ae_tone_red ? _ae_tone_red : _ae_tone_blue, LINE_ALPHA, &color);
		rounded_band_soft(corners, 0.0f, -1.0f, 0.0f, &color);
	}
}

static void rect(real x0, real y0, real x1, real y1, const real_argb_color *color)
{
	real points[4][2];

	points[0][0] = x0; points[0][1] = y1;
	points[1][0] = x0; points[1][1] = y0;
	points[2][0] = x1; points[2][1] = y0;
	points[3][0] = x1; points[3][1] = y1;
	ae_hud_fill_quad_color(points, color);
}

void ae_panel_draw_cross(real x, real y, real size, short tone)
{
	real arm = floorf(size / 3.0f + 0.5f);
	real_argb_color color;

	tone_color(_ae_tone_dark, KEYLINE_ALPHA, &color);
	rect(x + arm - 1.0f, y - 1.0f, x + 2.0f * arm + 1.0f, y + size + 1.0f, &color);
	rect(x - 1.0f, y + arm - 1.0f, x + size + 1.0f, y + 2.0f * arm + 1.0f, &color);
	tone_color(tone, 0.96f, &color);
	rect(x + arm, y, x + 2.0f * arm, y + size, &color);
	rect(x, y + arm, x + arm, y + 2.0f * arm, &color);
	rect(x + 2.0f * arm, y + arm, x + size, y + 2.0f * arm, &color);
}

/* ---------- debug.ae_panel_primitives */

void ae_panel_draw_debug(short local_player_index)
{
	static short logged_counts[3] = { -2, -2, -2 };
	struct ae_panel_para box = { 60.0f, 209.0f, 120.0f, 156.0f, AE_PANEL_LEAN };
	struct ae_panel_para bar = { 60.0f, 361.0f, 200.0f, 212.0f, AE_PANEL_LEAN };
	struct ae_panel_para cell = { 400.0f, 415.0f, 200.0f, 210.0f, -AE_PANEL_LEAN };
	real_argb_color grey;
	static unsigned long read_at = (unsigned long)-1;
	static boolean on = FALSE;

	/* (read again only when a setting changed) */
	if (read_at != config_changes())
	{
		read_at = config_changes();
		on = config_boolean("debug.ae_panel_primitives");
	}
	if (local_player_index != 0 || !on)
		return;
	/* the plate: flat mid grey, so the checks measure the primitives on a known ground */
	grey.alpha = 1.0f; grey.red = grey.green = grey.blue = 0.40f;
	rect(30.0f, 90.0f, 820.0f, 330.0f, &grey);
	ae_panel_draw_ring(700.0f, 210.0f, 92.0f - 0.8f, 1.6f, _ae_tone_blue, 0.85f, TRUE);
	ae_panel_draw_ring(700.0f, 210.0f, 56.7f, 1.6f, _ae_tone_blue, 0.70f, TRUE);
	ae_panel_draw_line(700.0f, 210.0f, 765.0f, 145.0f, 1.6f, _ae_tone_blue, 0.85f, TRUE);
	ae_panel_draw_box(&box, 1.0f, 0.08f, _ae_tone_blue);
	ae_panel_draw_bar(&bar, 1.0f, 0.6f, _ae_tone_blue, _ae_tone_blue, 0.0f, 0.0f);
	ae_panel_draw_art(_ae_art_ar, 260.0f, 120.0f, 23.6f, TRUE, _ae_tone_blue);
	ae_panel_draw_art(_ae_art_ar, 280.0f, 120.0f, 23.6f, FALSE, _ae_tone_blue);
	ae_panel_draw_plasma(340.0f, 132.0f, 20.0f, _ae_tone_white, 1.0f);
	ae_panel_draw_x(380.0f, 144.0f, 12.0f, 1.0f, _ae_tone_blue);
	ae_panel_draw_glyph(_ae_glyph_round, 410.0f, 121.0f, 23.0f, _ae_tone_blue, 1.0f);
	ae_panel_draw_glyph(_ae_glyph_frag, 450.0f, 120.0f, 24.0f, _ae_tone_white, 0.55f);
	ae_panel_draw_glyph(_ae_glyph_shield, 490.0f, 120.0f, 25.0f, _ae_tone_blue, 1.0f);
	ae_panel_draw_glyph(_ae_glyph_battery, 260.0f, 260.0f, 20.0f, _ae_tone_blue, 1.0f);
	ae_panel_draw_cell(&cell, TRUE, _ae_tone_blue);
	cell.x0 += 22.0f; cell.x1 += 22.0f;
	ae_panel_draw_cell(&cell, FALSE, _ae_tone_blue);
	ae_panel_draw_cross(460.0f, 200.0f, 12.0f, _ae_tone_blue);
	{
		short alphas, icons, backgrounds;
		boolean found = ae_panel_art_found(&alphas, &icons, &backgrounds);

		/* (once per map: the menus' map has the icons only) */
		if (alphas == logged_counts[0] && icons == logged_counts[1] && backgrounds == logged_counts[2])
			return;
		logged_counts[0] = alphas;
		logged_counts[1] = icons;
		logged_counts[2] = backgrounds;
		platform_log("ae panel primitives: plate 30,90..820,330 rings 700,210 r 91.2 + 56.7, line 700,210..765,145 box 60..209,120..156 bar 60..361,200..212 "
			"ar_lit 260,120 h 23.6 ar_spent 280,120 plasma 340,132 d 20 x 380,144 s 12 round 410,121 h 23 "
			"frag 450,120 h 24 a 0.55 shield 490,120 h 25 battery 260,260 h 20 cell 400..415,200..210 "
			"cross 460,200 s 12; art %s (alphas %d, icons %d, backgrounds %d)", found ? "found" : "missing", alphas,
			icons, backgrounds);
	}
}
