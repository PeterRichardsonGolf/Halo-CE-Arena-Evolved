/*
AE_PGCR_DRAW.H

The post-game report's drawing helpers, shared by its screens (ae_screen_pgcr.c, ae_screen_record.c): every measure is in
the mockups' 1920 x 1080 design units, drawn times the frame's width over 1920.
*/

#ifndef __AE_PGCR_DRAW_H
#define __AE_PGCR_DRAW_H

#include "../src/ae_draw.h"
#include "ae_style.h"
#include "ae_widgets.h"

struct frame_metrics
{
	float m, x, y;
};

static inline float tx(struct frame_metrics const *f, float v) { return f->x + v * f->m; }
static inline float ty(struct frame_metrics const *f, float v) { return f->y + v * f->m; }

static inline unsigned int with_alpha(unsigned int rgba, float alpha)
{
	unsigned int a = (unsigned int)((float)(rgba & 0xFF) * (alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha) + 0.5f);

	return (rgba & 0xFFFFFF00u) | a;
}

/* text with its capitals centred on a design line; returns its width in design units */
static inline float put(struct frame_metrics const *f, int font, float size, float x, float cy, int align, unsigned int color,
	char const *text)
{
	float px = size * f->m;

	return ae_draw_text(font, px, tx(f, x), ty(f, cy) - ae_draw_cap_height(font, px) * 0.5f, align, color, text) / f->m;
}

static inline float put_tracked(struct frame_metrics const *f, int font, float size, float tracking, float x, float cy, int align,
	unsigned int color, char const *text)
{
	float px = size * f->m;

	return ae_draw_text_tracked(font, px, tracking, tx(f, x), ty(f, cy) - ae_draw_cap_height(font, px) * 0.5f, align, color,
		text) / f->m;
}

static inline float width_of(struct frame_metrics const *f, int font, float size, char const *text)
{
	return ae_draw_text_width(font, size * f->m, text) / f->m;
}

static inline void rect(struct frame_metrics const *f, float x, float y, float w, float h, float radius, unsigned int color)
{
	ae_draw_rect(tx(f, x), ty(f, y), w * f->m, h * f->m, radius * f->m, color);
}

static inline void fit(struct frame_metrics const *f, int font, float size, char const *text, float width, char *out, int out_size)
{
	ae_fit_text(font, size * f->m, text, width * f->m, out, out_size);
}

/* a rival's mark: a violet edge and tag (never the team's red, never the focus's ice blue), darker on the focused white row */
#define AE_PGCR_RIVAL_INK 0xB8A4F5FFu
#define AE_PGCR_RIVAL_INK_ON_LIGHT 0x5A44B8FFu

static inline void draw_you_tag(struct frame_metrics const *f, float x, float cy, int focused)
{
	float w = width_of(f, AE_FONT_ROW, 12, "YOU") + 17.0f;

	rect(f, x, cy - 10.0f, w, 20.0f, 2.0f, focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT);
	put_tracked(f, AE_FONT_ROW, 12, 0.1f, x + w * 0.5f + 0.5f, cy, AE_ALIGN_CENTER,
		focused ? AE_COLOR_SELECTION : AE_COLOR_SELECTION_TEXT, "YOU");
}

/* a rival's tag: outlined, in the rival ink */
static inline void draw_rival_tag(struct frame_metrics const *f, float x, float cy, int focused)
{
	float w = width_of(f, AE_FONT_ROW, 12, "RIVAL") + 17.0f;
	unsigned int ink = focused ? AE_PGCR_RIVAL_INK_ON_LIGHT : AE_PGCR_RIVAL_INK;

	ae_draw_outline(tx(f, x), ty(f, cy - 10.0f), w * f->m, 20.0f * f->m, 2.0f * f->m, f->m > 0.5f ? 1.5f : 1.0f, ink);
	put_tracked(f, AE_FONT_ROW, 12, 0.1f, x + w * 0.5f + 0.5f, cy, AE_ALIGN_CENTER, ink, "RIVAL");
}

#endif
