/*
AE_MAP_SHARE_CARD.C

Arena Evolved: the map downloads' card, drawn with AE's renderer (ae_map_share_card.h). Its numbers are the mockups'
(map-offer SPEC.md, look A): the lobby map card's own fill, edge and radius, the menus' Overpass with the menus' drop
shadow, and the 8 px of clearance round everything.
*/

#include "ae_map_share_card.h"
#include "../src/ae_draw.h"

#include <stdio.h>
#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

int ae_map_share_card_available(
	void)
{
	return ae_draw_available();
}

/* ---------- drawing */

enum
{
	CARD_X = 900,
	CARD_Y = 8,
	CARD_WIDTH = 495,
	CARD_HEIGHT = 140,
	CARD_EDGE = 4,
	CARD_RADIUS = 18,
	CARD_PADDING = 18,
	CARD_CAP_SMALL = 21,
	CARD_CAP_LARGE = 27,
	CARD_BAR_HEIGHT = 12,
	CARD_TEXT_GAP = 16,
};

#define CARD_FILL 0x000E23F0u
#define CARD_BLUE 0x2894FEFFu
#define CARD_EDGE_COLOR 0x2995FFFFu
#define CARD_GREY 0xC6C6C6FFu
#define CARD_WHITE 0xFFFFFFFFu
#define CARD_TRACK 0x002244FFu
#define CARD_ICE 0xB8D6FFFFu
#define CARD_SHADOW 0x000000A6u


static float card_size_for_cap(
	float cap)
{
	return cap / (ae_draw_cap_height(AE_FONT_BODY, 100.0f) / 100.0f);
}

static float card_text_width(
	float cap,
	char const *text)
{
	return ae_draw_text_width(AE_FONT_BODY, card_size_for_cap(cap), text);
}

/* text on a baseline, with the menus' drop shadow */
static void card_text(
	float cap,
	float x,
	float baseline,
	int align,
	unsigned int rgba,
	char const *text)
{
	float size = card_size_for_cap(cap);
	float top = baseline - ae_draw_cap_height(AE_FONT_BODY, size);

	ae_draw_text(AE_FONT_BODY, size, x + 2.0f, top + 2.0f, align, CARD_SHADOW, text);
	ae_draw_text(AE_FONT_BODY, size, x, top, align, rgba, text);
}

/* text that fits width at cap height, ending in "..." when it does not */
static void card_fit(
	float cap,
	float width,
	char const *text,
	char *out,
	int size)
{
	long length;

	strncpy(out, text, size - 1);
	out[size - 1] = 0;
	if (card_text_width(cap, out) <= width)
		return;
	length = (long)strlen(out);
	while (length > 0)
	{
		out[--length] = 0;
		while (length > 0 && out[length - 1] == ' ')
			out[--length] = 0;
		strcat(out, "...");
		if (card_text_width(cap, out) <= width)
			return;
		out[length] = 0;
	}
	strncpy(out, "...", size - 1);
}

/* a done card's line of names: as many as fit, then "+N" for the rest ("H2 Coagulation, H2 Warlock +1") */
static void card_names_line(
	struct ae_map_share_card const *card,
	float width,
	char *out,
	int size)
{
	long count;

	for (count = card->name_count; count >= 1; count--)
	{
		long index;
		long more = card->name_total - count;

		out[0] = 0;
		for (index = 0; index < count; index++)
		{
			if (index)
				strncat(out, ", ", size - strlen(out) - 1);
			strncat(out, card->names[index], size - strlen(out) - 1);
		}
		if (more > 0)
		{
			char tail[16];

			snprintf(tail, sizeof(tail), " +%ld", more);
			strncat(out, tail, size - strlen(out) - 1);
		}
		if (card_text_width(CARD_CAP_SMALL, out) <= width)
			return;
	}
	card_fit(CARD_CAP_SMALL, width, card->names[0], out, size);
}

void ae_map_share_card_draw(
	struct ae_map_share_card const *card,
	float alpha)
{
	struct ae_layout layout;
	float frame_left, x0, y0, ix0, ix1, base1, base2, top, row3, content, gap1, gap2, width;
	char text[112];
	float counter_width = 0.0f, amount_width = 0.0f, name_cap;

	ae_draw_view_full();
	ae_draw_current_layout(&layout);
	ae_draw_set_alpha(alpha);
	frame_left = (layout.width - 1440.0f) * 0.5f;
	x0 = frame_left + CARD_X;
	y0 = CARD_Y;
	ae_draw_rect(x0, y0, CARD_WIDTH, CARD_HEIGHT, CARD_RADIUS, CARD_FILL);
	ae_draw_outline(x0, y0, CARD_WIDTH, CARD_HEIGHT, CARD_RADIUS, CARD_EDGE, CARD_EDGE_COLOR);
	ix0 = x0 + CARD_EDGE + CARD_PADDING;
	ix1 = x0 + CARD_WIDTH - CARD_EDGE - CARD_PADDING;
	width = ix1 - ix0;
	/* rows centred in the card: label, name, then the bar or a line (the gaps are from the font's measured ascenders
	and descenders: 16 / 20 with the bar, 14 / 21 with a line) */
	gap1 = card->has_bar ? 16.0f : 14.0f;
	gap2 = card->has_bar ? 20.0f : 21.0f;
	content = CARD_CAP_SMALL + gap1 + CARD_CAP_LARGE + gap2 + (card->has_bar ? CARD_BAR_HEIGHT : CARD_CAP_SMALL + 9);
	top = y0 + CARD_EDGE + (float)(int)(((CARD_HEIGHT - 2 * CARD_EDGE) - content) * 0.5f);
	base1 = top + CARD_CAP_SMALL;
	base2 = base1 + gap1 + CARD_CAP_LARGE;
	row3 = base2 + gap2;
	if (card->counter[0])
	{
		counter_width = card_text_width(CARD_CAP_SMALL, card->counter);
		card_text(CARD_CAP_SMALL, ix1, base1, AE_ALIGN_RIGHT, CARD_GREY, card->counter);
	}
	card_fit(CARD_CAP_SMALL, counter_width ? width - counter_width - CARD_TEXT_GAP : width, card->label, text,
		sizeof(text));
	card_text(CARD_CAP_SMALL, ix0, base1, AE_ALIGN_LEFT, card->failed ? CARD_WHITE : CARD_BLUE, text);
	if (card->amount[0])
	{
		amount_width = card_text_width(CARD_CAP_SMALL, card->amount);
		card_text(CARD_CAP_SMALL, ix1, base2, AE_ALIGN_RIGHT, CARD_GREY, card->amount);
	}
	/* (the name at the large size; at the small size when it does not fit beside the amount; then ending in "...") */
	{
		/* (the room is what the amount leaves with every digit as the widest, so that the name's size does not jump as the
		digits change) */
		float widest = amount_width;
		float room;

		if (card->amount[0] && card->amount[0] != 'A')
		{
			char wide[48];
			int at;

			strncpy(wide, card->amount, sizeof(wide) - 1);
			wide[sizeof(wide) - 1] = 0;
			for (at = 0; wide[at]; at++)
			{
				if (wide[at] >= '1' && wide[at] <= '9')
					wide[at] = '0';
			}
			widest = MAX(widest, card_text_width(CARD_CAP_SMALL, wide));
		}
		room = widest ? width - widest - CARD_TEXT_GAP : width;

		name_cap = card_text_width(CARD_CAP_LARGE, card->name) <= room ? CARD_CAP_LARGE : CARD_CAP_SMALL;
		card_fit(name_cap, room, card->name, text, sizeof(text));
		card_text(name_cap, ix0, base2, AE_ALIGN_LEFT, CARD_WHITE, text);
	}
	if (card->has_bar)
	{
		float fill = (width - 4.0f) * card->progress;

		ae_draw_rect(ix0, row3, width, CARD_BAR_HEIGHT, CARD_BAR_HEIGHT * 0.5f, CARD_TRACK);
		if (fill >= 1.0f)
			ae_draw_rect(ix0 + 2.0f, row3 + 2.0f, fill, CARD_BAR_HEIGHT - 4.0f, MIN(4.0f, fill * 0.5f), CARD_BLUE);
		ae_draw_outline(ix0, row3, width, CARD_BAR_HEIGHT, CARD_BAR_HEIGHT * 0.5f, 2.0f, CARD_BLUE);
		/* (the moving end) */
		if (fill >= 8.0f && card->progress < 1.0f)
			ae_draw_rect(ix0 + 2.0f + fill - 6.0f, row3 + 2.0f, 6.0f, CARD_BAR_HEIGHT - 4.0f, 2.0f, CARD_ICE);
	}
	else if (card->line[0] || card->name_count)
	{
		if (card->name_count)
			card_names_line(card, width, text, sizeof(text));
		else
			card_fit(CARD_CAP_SMALL, width, card->line, text, sizeof(text));
		card_text(CARD_CAP_SMALL, ix0, row3 + CARD_CAP_SMALL, AE_ALIGN_LEFT, CARD_GREY, text);
	}
	ae_draw_set_alpha(1.0f);
}

