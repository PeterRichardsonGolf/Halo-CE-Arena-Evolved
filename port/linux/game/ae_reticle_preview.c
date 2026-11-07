/*
AE_RETICLE_PREVIEW.C

The live preview card (ae_reticle_preview.h): the crosshair as the game draws it (ae_crosshair.c paints both), 1:1 in the 1080
tall layout, over a plain swatch like a colour chart's; a 4x magnifier of its middle in the corner, where thickness and gaps are
judged; under it NORMAL / ENEMY / ALLY (the colours the game keeps) and AT REST / MOVING / FIRING (the motion the lines make).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"

#include <math.h>

#include "../src/ae_draw.h"
#include "ae_motion.h"
#include "ae_crosshair.h"
#include "ae_crosshair_edit.h"
#include "ae_reticle_preview.h"
#include "ae_style.h"

static const struct { const char *name; unsigned int rgba; } backgrounds[] =
{
	{ "DARK", 0x2C3238FFu }, { "LIGHT", 0xCED4D8FFu }, { "SKY", 0x7AA8D6FFu }, { "GRASS", 0x587840FFu },
	{ "SAND", 0xC4AA78FFu }, { "SNOW", 0xE8EEF2FFu }, { "METAL", 0x787E84FFu }
};
#define BACKGROUNDS ((int)(sizeof(backgrounds) / sizeof(backgrounds[0])))

int ae_reticle_preview_backgrounds(void)
{
	return BACKGROUNDS;
}

const char *ae_reticle_preview_background_name(int index)
{
	return index >= 0 && index < BACKGROUNDS ? backgrounds[index].name : "";
}

static unsigned long rgb_of(const struct ae_crosshair *xh)
{
	return ((unsigned long)xh->color[0] << 16) | ((unsigned long)xh->color[1] << 8) | xh->color[2];
}

/* (0.7.7) the loops the MOVING and FIRING tiles play: the motion the HUD gives the crosshair, asked for at the loop's time on
the menus' clock (ae_motion_now). MOVING: 1.2 s, out to +3 px and back (still, then moving, then still); FIRING: 0.6 s, a shot
kicks +6 px and eases back in 0.3 s, then holds 0.3 s. REDUCE MOTION holds the tile on a mid frame */
enum { LOOP_MOVING_MS = 1200, LOOP_FIRING_MS = 600 };

static float loop_phase(int ms)
{
	return (float)(ae_motion_now() % (unsigned long)ms) / (float)ms;
}

static struct ae_xh_motion loop_motion(int kind, float *progress)
{
	struct ae_xh_motion motion = { 0.0f, 0.0f, 0.0f };
	float phase;

	*progress = -1.0f;
	if (kind == 1)
	{
		phase = loop_phase(LOOP_MOVING_MS);
		*progress = phase;
		motion.move = ae_motion_reduced() ? 3.0f : 3.0f * sinf(3.14159265f * phase);
	}
	else if (kind == 2)
	{
		phase = loop_phase(LOOP_FIRING_MS);
		*progress = phase;
		if (ae_motion_reduced())
			motion.kick = 4.0f;
		else if (phase < 0.5f)
		{
			float left = 1.0f - phase * 2.0f;

			motion.kick = 6.0f * left * left;
		}
	}
	return motion;
}

/* (0.7.7) the scale a crosshair is drawn at in a swatch (a state tile, the 4X magnifier): its resting look takes about 60 % of the
swatch's half and the widest bloom (a shot's kick, 6 px at scale 1) stays within 85 %, so every state has a margin; never more than
cap (2x in a tile, 4x in the magnifier, times the screen's unit). half: the room from the middle to the nearest edge */
static float swatch_scale(const struct ae_crosshair *xh, float half, float cap)
{
	float extent = ae_xh_extent(xh), rest, bloom;

	if (extent < 0.5f)
		return cap;
	rest = 0.60f * half / extent;
	bloom = 0.85f * half / (extent + 6.0f);
	if (bloom < rest)
		rest = bloom;
	return rest < cap ? rest : cap;
}

/* a state tile: the swatch with the crosshair at 2x at most, the label under it on the panel (readable on every background), and for
MOVING and FIRING a thin loop bar along the swatch's foot; a motion the crosshair has off shows faded, OFF in its label.
strip: the label's band, text its size (the card's 20 and 12; the CROSSHAIR screen's in its units) */
static void tile_sized(float x, float y, float width, float height, const struct ae_crosshair *xh, unsigned long rgb, int kind,
	int background, const char *label, int off, float strip, float text, float bar, float unit)
{
	float swatch = height - strip, progress = -1.0f;
	struct ae_xh_motion motion = { 0.0f, 0.0f, 0.0f };
	char words[32];

	if (kind)
		motion = loop_motion(kind, &progress);
	ae_draw_rect(x, y, width, swatch, 0.0f, backgrounds[background].rgba);
	ae_draw_outline(x, y, width, swatch, 0.0f, 1.0f, 0xC8D6E040u);
	ae_draw_clip_push(x, y, width, swatch);
	ae_crosshair_paint(xh, &motion, rgb, x + width / 2.0f, y + swatch / 2.0f,
		swatch_scale(xh, (width < swatch ? width : swatch) * 0.5f, 2.0f * unit), 1.0f, off ? 0.3f : 1.0f);
	ae_draw_clip_pop();
	if (progress >= 0.0f && !off)
	{
		ae_draw_rect(x, y + swatch - bar, width, bar, 0.0f, 0x10161C80u);
		ae_draw_rect(x, y + swatch - bar, width * (progress > 1.0f ? 1.0f : progress), bar, 0.0f, AE_COLOR_ACCENT);
	}
	ae_draw_rect(x, y + swatch, width, strip, 0.0f, AE_COLOR_WELL);
	snprintf(words, sizeof(words), off ? "%s  OFF" : "%s", label);
	/* (a narrow tile's label is drawn smaller rather than cut: "MOVING  OFF" at 130 %) */
	{
		float room = width - 8.0f, wide = ae_draw_text_tracked_width(AE_FONT_BODY, text, 0.1f, words);

		if (wide > room && wide > 0.0f)
			text *= room / wide;
	}
	ae_draw_text_tracked(AE_FONT_BODY, text, 0.1f, x + width / 2.0f, y + swatch + (strip - ae_draw_cap_height(AE_FONT_BODY,
		text)) * 0.5f, AE_ALIGN_CENTER, off ? AE_COLOR_DISABLED_TEXT : AE_COLOR_TEXT, words);
}

static void tile(float x, float y, float width, float height, const struct ae_crosshair *xh, unsigned long rgb, int kind,
	int background, const char *label, int off)
{
	tile_sized(x, y, width, height, xh, rgb, kind, background, label, off, 20.0f, 12.0f, 2.0f, 1.0f);
}

unsigned int ae_reticle_preview_background_rgba(int index)
{
	return backgrounds[index >= 0 && index < BACKGROUNDS ? index : 0].rgba;
}

/* a small dark badge with tracked words ("4X", "1:1"), its left at x, top at y; returns its width */
static float badge(float x, float y, float unit, const char *words)
{
	float size = 15.0f * unit, height = 30.0f * unit, width = ae_draw_text_tracked_width(AE_FONT_BODY, size, 0.08f, words) +
		24.0f * unit;

	ae_draw_rect(x, y, width, height, height * 0.5f, 0x10161CE6u);
	ae_draw_text_tracked(AE_FONT_BODY, size, 0.08f, x + width * 0.5f, y + (height - ae_draw_cap_height(AE_FONT_BODY, size)) * 0.5f,
		AE_ALIGN_CENTER, AE_COLOR_TITLE, words);
	return width;
}

void ae_reticle_preview_stage(float x, float y, float width, float height, const struct ae_crosshair *xh, int background,
	float unit)
{
	struct ae_xh_motion rest = { 0.0f, 0.0f, 0.0f };
	float pad = 16.0f * unit, glass;

	if (background < 0 || background >= BACKGROUNDS)
		background = 0;
	ae_draw_rect(x, y, width, height, 0.0f, backgrounds[background].rgba);
	ae_draw_outline(x, y, width, height, 0.0f, 1.0f, 0xC8D6E046u);
	if (!xh)
		return;
	ae_crosshair_paint(xh, &rest, rgb_of(xh), x + width * 0.5f, y + height * 0.5f, 1.0f, 1.0f, 1.0f);
	/* (the magnifier: the middle at 4x in the top-left corner, framed, "4X" on its foot; a stage too short goes without) */
	glass = height * 0.5f;
	if (glass > 200.0f * unit)
		glass = 200.0f * unit;
	if (glass >= 96.0f * unit && glass < width * 0.45f)
	{
		float gx = x + pad, gy = y + pad;

		ae_draw_rect(gx, gy, glass, glass, 0.0f, backgrounds[background].rgba);
		ae_draw_rect(gx, gy, glass, glass, 0.0f, 0x10161C1Au);
		ae_draw_outline(gx, gy, glass, glass, 0.0f, 2.0f * unit, 0x10161C99u);
		ae_draw_clip_push(gx, gy, glass, glass);
		ae_crosshair_paint(xh, &rest, rgb_of(xh), gx + glass * 0.5f, gy + glass * 0.5f, swatch_scale(xh, glass * 0.5f, 4.0f * unit),
			1.0f, 1.0f);
		ae_draw_clip_pop();
		badge(gx + 8.0f * unit, gy + glass - 38.0f * unit, unit, "4X");
	}
	{
		float width_11 = ae_draw_text_tracked_width(AE_FONT_BODY, 15.0f * unit, 0.08f, "1:1") + 24.0f * unit;

		badge(x + width - pad - width_11, y + pad, unit, "1:1");
	}
}

void ae_reticle_preview_tiles(float x, float y, float width, float tile_height, float gap, int columns,
	const struct ae_crosshair *xh, int background, float unit)
{
	static const char *const labels[6] = { "NORMAL", "ENEMY", "ALLY", "AT REST", "MOVING", "FIRING" };
	float tile_width;
	int index;
	unsigned long base;

	if (!xh)
		return;
	if (columns < 1)
		columns = 6;
	if (background < 0 || background >= BACKGROUNDS)
		background = 0;
	tile_width = (width - (float)(columns - 1) * gap) / (float)columns;
	base = rgb_of(xh);
	for (index = 0; index < 6; index++)
	{
		float tx = x + (float)(index % columns) * (tile_width + gap), ty = y + (float)(index / columns) * (tile_height + gap);
		int motion = index >= 3 ? index - 3 : 0;

		tile_sized(tx, ty, tile_width, tile_height, xh, index < 3 ? ae_xh_state_rgb(base, index == 1, index == 2) : base,
			motion, background, labels[index], (motion == 1 && !xh->movement) || (motion == 2 && !xh->firing),
			26.0f * unit, 13.0f * unit, 3.0f * unit, unit);
	}
}

/* (0.7.7.1) one stock-reticle stand-in: four ticks (length 12, thick 3, gap 7 at scale 1) round (cx, cy); outline 1: only their
outlines (the 100 % reference) */
static void stock_stand_in(float cx, float cy, float scale, unsigned int color, int outline)
{
	float length = 12.0f * scale, thick = 3.0f * scale, gap = 7.0f * scale;
	float rects[4][4] =
	{
		{ cx - thick * 0.5f, cy - gap - length, thick, length }, { cx - thick * 0.5f, cy + gap, thick, length },
		{ cx - gap - length, cy - thick * 0.5f, length, thick }, { cx + gap, cy - thick * 0.5f, length, thick },
	};
	int index;

	for (index = 0; index < 4; index++)
	{
		if (outline)
			ae_draw_outline(rects[index][0], rects[index][1], rects[index][2], rects[index][3], 0.0f, 1.0f, color);
		else
			ae_draw_rect(rects[index][0], rects[index][1], rects[index][2], rects[index][3], 0.0f, color);
	}
}

float ae_reticle_preview_stock_draw(float x, float y, float width, float height, float scale, int background,
	const char *caption)
{
	float swatch_height = height - 52.0f, cx = x + width / 2.0f, cy;

	if (background < 0 || background >= BACKGROUNDS)
		background = 0;
	if (swatch_height > width * 0.72f)
		swatch_height = width * 0.72f;
	if (swatch_height < 60.0f)
		swatch_height = 60.0f;
	cy = y + swatch_height / 2.0f;
	ae_draw_outline(x - 2.0f, y - 2.0f, width + 4.0f, swatch_height + 4.0f, 0.0f, 1.0f, 0xC8D6E046u);
	ae_draw_rect(x, y, width, swatch_height, 0.0f, backgrounds[background].rgba);
	ae_draw_clip_push(x, y, width, swatch_height);
	/* (the reference first, faint; the stand-in over it, solid, so a larger one covers it and a smaller one sits inside it) */
	stock_stand_in(cx, cy, 1.0f, 0x84939FC0u, 1);
	stock_stand_in(cx, cy, scale, 0xFFFFFFFFu, 0);
	ae_draw_clip_pop();
	ae_draw_text_tracked(AE_FONT_BODY, 11.0f, 0.1f, x + 8.0f, y + swatch_height - 20.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED,
		"OUTLINE: 100 %");
	ae_draw_text_tracked(AE_FONT_BODY, 12.0f, 0.06f, x, y + swatch_height + 18.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED,
		caption ? caption : "");
	return swatch_height + 46.0f;
}

float ae_reticle_preview_draw(float x, float y, float width, float height, const struct ae_crosshair *xh, int background,
	const char *caption)
{
	static const char *const state_labels[3] = { "NORMAL", "ENEMY", "ALLY" };
	static const char *const motion_labels[3] = { "AT REST", "MOVING", "FIRING" };
	float swatch_height, pitch = 8.0f, tile_width = (width - 2.0f * pitch) / 3.0f, tile_height = 88.0f, cy, cx, used;
	char line[160];
	int index, rows;

	if (background < 0 || background >= BACKGROUNDS)
		background = 0;
	/* (the swatch takes what the tile rows and the caption leave; a card with too little room, a larger UI scale's, drops the
	motion row, then the colour row, rather than meet the footer) */
	{
		float room = height - 52.0f;

		rows = room >= 120.0f + 2.0f * (tile_height + pitch) ? 2 : room >= 120.0f + (tile_height + pitch) ? 1 : 0;
		swatch_height = room - (float)rows * (tile_height + pitch);
	}
	if (swatch_height > width * 0.72f)
		swatch_height = width * 0.72f;
	/* (never more than the card has: a short card's swatch is what is left, the magnifier then dropped) */
	{
		float least = height - 52.0f < 100.0f ? (height - 52.0f > 44.0f ? height - 52.0f : 44.0f) : 100.0f;

		if (swatch_height < least)
			swatch_height = least;
	}
	cx = x + width / 2.0f;
	cy = y + swatch_height / 2.0f;
	ae_draw_outline(x - 2.0f, y - 2.0f, width + 4.0f, swatch_height + 4.0f, 0.0f, 1.0f, 0xC8D6E046u);
	ae_draw_rect(x, y, width, swatch_height, 0.0f, backgrounds[background].rgba);
	if (xh)
	{
		struct ae_xh_motion rest = { 0.0f, 0.0f, 0.0f };

		ae_crosshair_paint(xh, &rest, rgb_of(xh), cx, cy, 1.0f, 1.0f, 1.0f);
		/* (the magnifier: the middle at 4x in the corner, framed; a swatch too short for it goes without) */
		if (swatch_height >= 100.0f)
		{
			ae_draw_rect(x + 8.0f, y + 8.0f, 88.0f, 88.0f, 0.0f, backgrounds[background].rgba);
			ae_draw_outline(x + 8.0f, y + 8.0f, 88.0f, 88.0f, 0.0f, 1.0f, 0x84939F90u);
			ae_draw_clip_push(x + 8.0f, y + 8.0f, 88.0f, 88.0f);
			ae_crosshair_paint(xh, &rest, rgb_of(xh), x + 52.0f, y + 52.0f, swatch_scale(xh, 44.0f, 4.0f), 1.0f, 1.0f);
			ae_draw_clip_pop();
			ae_draw_rect(x + 8.0f, y + 100.0f, 30.0f, 20.0f, 0.0f, AE_COLOR_WELL);
			ae_draw_text_tracked(AE_FONT_BODY, 11.0f, 0.1f, x + 14.0f, y + 106.0f, AE_ALIGN_LEFT, AE_COLOR_TEXT, "4X");
		}
	}
	else
		ae_draw_text(AE_FONT_BODY, 14.0f, cx, cy - 6.0f, AE_ALIGN_CENTER, AE_COLOR_TITLE, "STOCK RETICLE");
	used = swatch_height + 18.0f;
	snprintf(line, sizeof(line), "%s", caption ? caption : "");
	if (xh && !caption)
		ae_crosshair_edit_summary(xh, line, sizeof(line));
	ae_draw_text_tracked(AE_FONT_BODY, 12.0f, 0.06f, x, y + used, AE_ALIGN_LEFT, AE_COLOR_MUTED, line);
	used += 28.0f;
	if (xh && rows > 0)
	{
		unsigned long base = rgb_of(xh);

		for (index = 0; index < 3; index++)
			tile(x + (float)index * (tile_width + pitch), y + used, tile_width, tile_height, xh,
				ae_xh_state_rgb(base, index == 1, index == 2), 0, background, state_labels[index], 0);
		used += tile_height + pitch;
		for (index = 0; rows > 1 && index < 3; index++)
			tile(x + (float)index * (tile_width + pitch), y + used, tile_width, tile_height, xh, base,
				index, background, motion_labels[index],
				(index == 1 && !xh->movement) || (index == 2 && !xh->firing));
		used += tile_height;
	}
	return used;
}
