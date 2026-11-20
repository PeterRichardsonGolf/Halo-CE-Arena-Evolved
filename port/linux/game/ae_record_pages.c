/* The SERVICE RECORD pages' shared panes (see the header). */
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "math/real_math.h"
#include "../src/ae_draw.h"
#include "ae_hooks.h"
#include "ae_medal_pack.h"
#include "ae_pgcr_draw.h"
#include "ae_pgcr_screen_rules.h"
#include "ae_record_focus.h"
#include "ae_record_pages.h"
#include "ae_record_medals_rules.h"
#include "ae_style.h"
#include "ae_widgets.h"

#define PANE_FILL 0x10161CF2u
#define RING_OUT 6.0f

enum
{
	CHIP_H = 34, CHIP_GAP = 12, CHIP_SIDE = 20, CHIP_RIGHT = 1824, CHIP_CY = 150
};

char const *ae_rp_page_name(int page)
{
	return page == AE_RP_COMBAT ? "COMBAT" : page == AE_RP_MODES ? "MODES & MAPS" : "OVERVIEW";
}

char const *ae_rp_crumb(int page)
{
	return page == AE_RP_COMBAT ? "COMBAT" : page == AE_RP_MODES ? "MODES & MAPS" : NULL;
}

int ae_rp_next(int page)
{
	return (page + 1) % AE_RP_PAGES;
}

void ae_rp_chips(struct frame_metrics const *f, int current, int mouse, int hover, int hit_id)
{
	float widths[AE_RP_PAGES], x;
	int page;

	for (page = 0, x = (float)CHIP_RIGHT; page < AE_RP_PAGES; page++)
	{
		widths[page] = CHIP_SIDE * 2.0f + width_of(f, AE_FONT_BODY, 15, ae_rp_page_name(page)) + 1.4f *
			(float)strlen(ae_rp_page_name(page));
		x -= widths[page] + (page ? CHIP_GAP : 0.0f);
	}
	for (page = 0; page < AE_RP_PAGES; page++)
	{
		float top = (float)CHIP_CY - CHIP_H * 0.5f;
		int is_current = page == current;

		if (is_current)
		{
			rect(f, x, top, widths[page], CHIP_H, CHIP_H * 0.5f, with_alpha(AE_COLOR_ACCENT, 0.16f));
			ae_draw_outline(tx(f, x), ty(f, top), widths[page] * f->m, CHIP_H * f->m, CHIP_H * 0.5f * f->m, 2.0f * f->m,
				AE_COLOR_ACCENT);
		}
		else
		{
			if (mouse && hover == page)
				rect(f, x, top, widths[page], CHIP_H, CHIP_H * 0.5f, AE_COLOR_HOVER);
			ae_draw_outline(tx(f, x), ty(f, top), widths[page] * f->m, CHIP_H * f->m, CHIP_H * 0.5f * f->m, 2.0f * f->m,
				with_alpha(AE_COLOR_MUTED, 0.45f));
		}
		put_tracked(f, AE_FONT_BODY, 15, 0.10f, x + CHIP_SIDE, (float)CHIP_CY, AE_ALIGN_LEFT,
			is_current ? AE_COLOR_TITLE : AE_COLOR_TEXT, ae_rp_page_name(page));
		ae_hit_add(tx(f, x), ty(f, top), widths[page] * f->m, CHIP_H * f->m, hit_id, AE_PART_CHIP, (short)page);
		x += widths[page] + CHIP_GAP;
	}
}

void ae_rp_pane(struct frame_metrics const *f, float x, float y, float w, float h)
{
	rect(f, x, y, w, h, 3.0f, PANE_FILL);
	rect(f, x, y, w, 4.0f, 0.0f, AE_COLOR_ACCENT);
}

float ae_rp_label(struct frame_metrics const *f, float x, float y, char const *label, char const *tail)
{
	float w = put_tracked(f, AE_FONT_BODY, 16, 0.14f, x, y + 12.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);

	if (tail && tail[0])
	{
		/* (a drawn 4 u dot, then the tail dimmer than the label) */
		rect(f, x + w + 12.0f, y + 10.0f, 4.0f, 4.0f, 2.0f, with_alpha(AE_COLOR_MUTED, 0.7f));
		put_tracked(f, AE_FONT_BODY, 14, 0.12f, x + w + 28.0f, y + 12.0f, AE_ALIGN_LEFT, with_alpha(AE_COLOR_MUTED, 0.7f), tail);
	}
	return y + 34.0f;
}

void ae_rp_cell(struct frame_metrics const *f, float left, float right, float cy, struct ae_rv_cell const *cell)
{
	char fitted[64];
	float w = put_tracked(f, AE_FONT_BODY, 16, 0.1f, left, cy, AE_ALIGN_LEFT, AE_COLOR_TEXT, cell->label);

	fit(f, AE_FONT_ROW, 24, cell->value, right - left - w - 20.0f, fitted, sizeof(fitted));
	put(f, AE_FONT_ROW, 24, right, cy, AE_ALIGN_RIGHT, cell->dash ? AE_COLOR_MUTED : AE_COLOR_TITLE, fitted);
}

/* a row of the 24 u kind: label and value; `pitch` between rows */
static void cell_rows(struct frame_metrics const *f, float left, float right, float y, struct ae_rv_cell const *cells, int count,
	float pitch)
{
	int index;

	for (index = 0; index < count; index++)
		ae_rp_cell(f, left, right, y + 17.0f + (float)index * pitch, &cells[index]);
}

void ae_rp_totals(struct frame_metrics const *f, struct ae_rv_in const *in)
{
	struct ae_rv_cell cells[AE_RV_TOTALS];
	float left = AE_RP_SECOND_X + AE_RP_PANE_PAD, right = AE_RP_SECOND_X + AE_RP_SECOND_W - AE_RP_PANE_PAD;
	float middle = AE_RP_SECOND_X + AE_RP_SECOND_W * 0.5f, y;

	ae_rv_totals(in, cells);
	ae_rp_pane(f, AE_RP_SECOND_X, AE_RP_TOP, AE_RP_SECOND_W, AE_RP_PANE_H);
	y = ae_rp_label(f, left, AE_RP_TOP + AE_RP_PANE_PAD, "CAREER TOTALS", NULL);
	cell_rows(f, left, middle - 24.0f, y, cells, 8, 46.0f);
	cell_rows(f, middle + 14.0f, right, y, cells + 8, 8, 46.0f);
}

/* ---------- COMBAT */

static void draw_accuracy(struct frame_metrics const *f, struct ae_rv_in const *in)
{
	struct ae_rv_cell cells[AE_RV_ACCURACY];
	char tail[AE_RV_TAIL];
	float y;

	ae_rv_accuracy(in, cells, tail);
	ae_rp_pane(f, AE_RP_LEFT, AE_RP_TOP, AE_RP_FIRST_W, AE_RP_PANE_H);
	y = ae_rp_label(f, AE_RP_LEFT + AE_RP_PANE_PAD, AE_RP_TOP + AE_RP_PANE_PAD, "ACCURACY", tail);
	cell_rows(f, AE_RP_LEFT + AE_RP_PANE_PAD, AE_RP_LEFT + AE_RP_FIRST_W - AE_RP_PANE_PAD, y, cells, AE_RV_ACCURACY, 46.0f);
}

static void draw_kill_kinds(struct frame_metrics const *f, struct ae_rv_in const *in)
{
	struct ae_rv_share rows[AE_RV_KILL_KINDS];
	char tail[AE_RV_TAIL];
	float left = AE_RP_SECOND_X + AE_RP_PANE_PAD, right = AE_RP_SECOND_X + AE_RP_SECOND_W - AE_RP_PANE_PAD, y;
	int index;

	ae_rv_kill_kinds(in, rows, tail);
	ae_rp_pane(f, AE_RP_SECOND_X, AE_RP_TOP, AE_RP_SECOND_W, AE_RP_PANE_H);
	y = ae_rp_label(f, left, AE_RP_TOP + AE_RP_PANE_PAD, "KILL KINDS", tail);
	put_tracked(f, AE_FONT_BODY, 13, 0.14f, right - 88.0f, y + 8.0f, AE_ALIGN_RIGHT, AE_COLOR_MUTED, "KILLS");
	put_tracked(f, AE_FONT_BODY, 13, 0.14f, right, y + 8.0f, AE_ALIGN_RIGHT, AE_COLOR_MUTED, "SHARE");
	y += 28.0f;
	for (index = 0; index < AE_RV_KILL_KINDS; index++)
	{
		float cy = y + 17.0f + (float)index * 46.0f;

		put_tracked(f, AE_FONT_ROW, 20, 0.04f, left, cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, rows[index].label);
		put(f, AE_FONT_ROW, 20, right - 88.0f, cy, AE_ALIGN_RIGHT, strcmp(rows[index].count, AE_RV_DASH) ? AE_COLOR_TITLE :
			AE_COLOR_MUTED, rows[index].count);
		put(f, AE_FONT_ROW, 20, right, cy, AE_ALIGN_RIGHT, strcmp(rows[index].share, AE_RV_DASH) ? AE_COLOR_TITLE :
			AE_COLOR_MUTED, rows[index].share);
	}
}

static void draw_assists(struct frame_metrics const *f, struct ae_rv_in const *in)
{
	struct ae_rv_cell cells[AE_RV_ASSISTS];
	float y;

	ae_rv_assists(in, cells);
	ae_rp_pane(f, AE_RP_THIRD_X, AE_RP_TOP, AE_RP_THIRD_W, AE_RP_PANE_H);
	y = ae_rp_label(f, AE_RP_THIRD_X + AE_RP_PANE_PAD, AE_RP_TOP + AE_RP_PANE_PAD, "ASSISTS", NULL);
	cell_rows(f, AE_RP_THIRD_X + AE_RP_PANE_PAD, AE_RP_THIRD_X + AE_RP_THIRD_W - AE_RP_PANE_PAD, y, cells, AE_RV_ASSISTS, 46.0f);
}

static void draw_damage(struct frame_metrics const *f, struct ae_rv_in const *in)
{
	struct ae_rv_cell cells[AE_RV_DAMAGE];
	char tail[AE_RV_TAIL];
	float y;

	ae_rv_damage(in, cells, tail);
	ae_rp_pane(f, AE_RP_LEFT, AE_RP_ROW_B_Y, 1104.0f, AE_RP_ROW_B_H);
	y = ae_rp_label(f, AE_RP_LEFT + AE_RP_PANE_PAD, AE_RP_ROW_B_Y + AE_RP_PANE_PAD, "DAMAGE", tail);
	cell_rows(f, AE_RP_LEFT + AE_RP_PANE_PAD, AE_RP_LEFT + 528.0f, y, cells, 3, 46.0f);
	cell_rows(f, AE_RP_LEFT + 576.0f, AE_RP_LEFT + 1104.0f - AE_RP_PANE_PAD, y, cells + 3, 3, 46.0f);
}

static void draw_per_game(struct frame_metrics const *f, struct ae_rv_in const *in)
{
	struct ae_rv_cell cells[AE_RV_PER_GAME];
	char tail[AE_RV_TAIL];
	float y;

	ae_rv_per_game(in, cells, tail);
	ae_rp_pane(f, 1224.0f, AE_RP_ROW_B_Y, 600.0f, AE_RP_ROW_B_H);
	y = ae_rp_label(f, 1224.0f + AE_RP_PANE_PAD, AE_RP_ROW_B_Y + AE_RP_PANE_PAD, "PER GAME", tail);
	cell_rows(f, 1224.0f + AE_RP_PANE_PAD, 1224.0f + 600.0f - AE_RP_PANE_PAD, y, cells, AE_RV_PER_GAME, 46.0f);
}

void ae_rp_combat(struct frame_metrics const *f, struct ae_rv_in const *in)
{
	draw_accuracy(f, in);
	draw_kill_kinds(f, in);
	draw_assists(f, in);
	draw_damage(f, in);
	draw_per_game(f, in);
}

/* ---------- MODES & MAPS */

static void draw_objectives(struct frame_metrics const *f, struct ae_rv_in const *in)
{
	struct ae_rv_cell cells[AE_RV_OBJECTIVES];
	char tail[AE_RV_TAIL];
	float y;

	ae_rv_objectives(in, cells, tail);
	ae_rp_pane(f, AE_RP_LEFT, AE_RP_TOP, AE_RP_FIRST_W, AE_RP_PANE_H);
	y = ae_rp_label(f, AE_RP_LEFT + AE_RP_PANE_PAD, AE_RP_TOP + AE_RP_PANE_PAD, "OBJECTIVES", tail);
	/* (ten rows at a 36 u pitch: the pane's limit, the last ends at 645 of 672) */
	cell_rows(f, AE_RP_LEFT + AE_RP_PANE_PAD, AE_RP_LEFT + AE_RP_FIRST_W - AE_RP_PANE_PAD, y, cells, AE_RV_OBJECTIVES, 36.0f);
}

static void draw_kinds(struct frame_metrics const *f, struct ae_rv_in const *in)
{
	struct ae_rv_kind_row rows[AE_RS_KINDS];
	float left = AE_RP_SECOND_X + AE_RP_PANE_PAD, right = AE_RP_SECOND_X + AE_RP_SECOND_W - AE_RP_PANE_PAD, y;
	/* (right edges of the number columns, as the record page's BY KIND always had) */
	float const columns[6] = { right - 340.0f, right - 268.0f, right - 196.0f, right - 130.0f, right - 68.0f, right };
	static char const *const heads[6] = { "GAMES", "WINS", "K", "D", "A", "K/D" };
	int count, index;
	char tail[AE_RV_TAIL];

	count = ae_rv_kinds(in, rows);
	/* (a bot's kinds are counted with the new numbers: its tail; a player's cover every game) */
	if (in->spree_from_record)
		ae_rs_from_games(in->x ? in->x->counted : 0, in->games, tail, sizeof(tail));
	else
		tail[0] = 0;
	ae_rp_pane(f, AE_RP_SECOND_X, AE_RP_TOP, AE_RP_SECOND_W, AE_RP_PANE_H);
	y = ae_rp_label(f, left, AE_RP_TOP + AE_RP_PANE_PAD, "BY KIND OF GAME", tail);
	for (index = 0; index < 6; index++)
		put_tracked(f, AE_FONT_BODY, 13, 0.14f, columns[index], y + 8.0f, AE_ALIGN_RIGHT, AE_COLOR_MUTED, heads[index]);
	y += 28.0f;
	for (index = 0; index < count; index++)
	{
		float cy = y + 17.0f + (float)index * 46.0f;
		char const *const values[6] = { rows[index].games, rows[index].wins, rows[index].kills, rows[index].deaths,
			rows[index].assists, rows[index].kd };
		int column;

		put_tracked(f, AE_FONT_ROW, 20, 0.04f, left, cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, rows[index].name);
		for (column = 0; column < 6; column++)
			put(f, AE_FONT_ROW, 20, columns[column], cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, values[column]);
	}
	if (!count)
		put(f, AE_FONT_ROW, 24, left, y + 17.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, AE_RV_DASH);
}

/* the pane's last line: "+26 more", a dot, then ALL MEDALS (the page it opens); ALL MEDALS alone when nothing is hidden */
static void draw_medals_link(struct frame_metrics const *f, float right, float cy, int hidden)
{
	char text[24];
	float w = put_tracked(f, AE_FONT_BODY, 16, 0.10f, right, cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, "ALL MEDALS"), dot;

	if (hidden <= 0)
		return;
	dot = right - w - 12.0f;
	rect(f, dot - 2.0f, cy - 2.0f, 4.0f, 4.0f, 2.0f, AE_COLOR_MUTED);
	snprintf(text, sizeof(text), "+%d more", hidden);
	put(f, AE_FONT_BODY, 16, dot - 12.0f, cy, AE_ALIGN_RIGHT, AE_COLOR_MUTED, text);
}

static int draw_medals(struct frame_metrics const *f, struct ae_rv_in const *in)
{
	static unsigned long const none[AE_MEDAL_COUNT];
	unsigned long const *counts = in->medals ? in->medals : none;
	/* (the MEDALS page's own order, RAREST FIRST: its first cells are this pane's rows) */
	int medals[AE_MEDAL_COUNT], count, shown, index;
	float left = AE_RP_THIRD_X + AE_RP_PANE_PAD, right = AE_RP_THIRD_X + AE_RP_THIRD_W - AE_RP_PANE_PAD, y;
	char text[24];

	ae_rm_list(counts, AE_RM_SORT_RAREST, AE_RM_ALL, medals);
	count = ae_rm_earned(counts, AE_RM_ALL);
	ae_rp_pane(f, AE_RP_THIRD_X, AE_RP_TOP, AE_RP_THIRD_W, AE_RP_PANE_H);
	snprintf(text, sizeof(text), "%d KINDS", count);
	{
		float w = put_tracked(f, AE_FONT_BODY, 16, 0.14f, left, AE_RP_TOP + AE_RP_PANE_PAD + 12.0f, AE_ALIGN_LEFT,
			AE_COLOR_MUTED, "MEDALS");

		if (count)
			put_tracked(f, AE_FONT_BODY, 16, 0.14f, left + w + 14.0f, AE_RP_TOP + AE_RP_PANE_PAD + 12.0f, AE_ALIGN_LEFT,
				AE_COLOR_MUTED, text);
		y = AE_RP_TOP + AE_RP_PANE_PAD + 34.0f;
	}
	shown = count < AE_RP_MEDAL_LINES ? count : AE_RP_MEDAL_LINES;
	for (index = 0; index < shown; index++)
	{
		char name[AE_PGR_NAME];
		float cy = y + 17.0f + (float)index * 34.0f;
		unsigned int color = ae_pgr_family_color(ae_pgr_medal_family(medals[index]));

		ae_pgr_medal_name(medals[index], name, sizeof(name));
		snprintf(text, sizeof(text), "%lu", counts[medals[index]]);
		if (ae_medal_pack_menu_image(medals[index]))
		{
			ae_draw_image(ae_medal_pack_menu_image(medals[index]), tx(f, left), ty(f, cy - 14.0f), 28.0f * f->m, 28.0f * f->m,
				0xFFFFFFFFu);
			put_tracked(f, AE_FONT_BODY, 17, 0.06f, left + 40.0f, cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, name);
		}
		else
			put_tracked(f, AE_FONT_BODY, 17, 0.06f, left, cy, AE_ALIGN_LEFT, color, name);
		put(f, AE_FONT_ROW, 20, right, cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, text);
	}
	if (count > 0)
		draw_medals_link(f, right, AE_RP_TOP + AE_RP_PANE_H - 28.0f, count - shown);
	else
		put(f, AE_FONT_ROW, 24, left, y + 17.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, AE_RV_DASH);
	return count;
}

static void draw_maps(struct frame_metrics const *f, struct ae_rv_in const *in)
{
	struct ae_rv_map_row rows[AE_RV_MAPS];
	char tail[AE_RV_TAIL], more_text[24];
	int more = 0, count, index;
	float left = AE_RP_LEFT + AE_RP_PANE_PAD, y;
	/* (two columns of five: each a name and the numbers' right edges) */
	float const offsets[2] = { 0.0f, 548.0f };
	float const games_right = 352.0f, wins_right = 422.0f, kd_right = 500.0f;

	count = ae_rv_maps(in, rows, &more, tail);
	ae_rp_pane(f, AE_RP_LEFT, AE_RP_ROW_B_Y, 1104.0f, AE_RP_ROW_B_H);
	y = ae_rp_label(f, left, AE_RP_ROW_B_Y + AE_RP_PANE_PAD, "BY MAP", tail);
	for (index = 0; index < 2; index++)
	{
		float x = left + offsets[index];

		put_tracked(f, AE_FONT_BODY, 13, 0.14f, x + games_right, AE_RP_ROW_B_Y + AE_RP_PANE_PAD + 12.0f, AE_ALIGN_RIGHT,
			AE_COLOR_MUTED, "GAMES");
		put_tracked(f, AE_FONT_BODY, 13, 0.14f, x + wins_right, AE_RP_ROW_B_Y + AE_RP_PANE_PAD + 12.0f, AE_ALIGN_RIGHT,
			AE_COLOR_MUTED, "WINS");
		put_tracked(f, AE_FONT_BODY, 13, 0.14f, x + kd_right, AE_RP_ROW_B_Y + AE_RP_PANE_PAD + 12.0f, AE_ALIGN_RIGHT,
			AE_COLOR_MUTED, "K/D");
		/* (the second column's head only when it has a row) */
		if (count <= 5)
			break;
	}
	for (index = 0; index < count; index++)
	{
		float x = left + offsets[index / 5], cy = y + 22.0f + (float)(index % 5) * 34.0f;
		char name[AE_RS_MAP_NAME];

		fit(f, AE_FONT_ROW, 20, rows[index].name, games_right - 110.0f, name, sizeof(name));
		put_tracked(f, AE_FONT_ROW, 20, 0.04f, x, cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, name);
		put(f, AE_FONT_ROW, 20, x + games_right, cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, rows[index].games);
		put(f, AE_FONT_ROW, 20, x + wins_right, cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, rows[index].wins);
		put(f, AE_FONT_ROW, 20, x + kd_right, cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, rows[index].kd);
	}
	if (!count)
		put(f, AE_FONT_ROW, 24, left, y + 22.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, AE_RV_DASH);
	if (more > 0)
	{
		snprintf(more_text, sizeof(more_text), "+%d more", more);
		put(f, AE_FONT_BODY, 16, AE_RP_LEFT + 1104.0f - AE_RP_PANE_PAD, AE_RP_ROW_B_Y + AE_RP_ROW_B_H - 26.0f, AE_ALIGN_RIGHT,
			AE_COLOR_MUTED, more_text);
	}
}

static void draw_weapons(struct frame_metrics const *f, struct ae_rv_in const *in)
{
	struct ae_rv_cell rows[AE_RS_WEAPONS];
	char tail[AE_RV_TAIL], head[AE_RV_TAIL + 16];
	float left = 1224.0f + AE_RP_PANE_PAD, right = 1224.0f + 600.0f - AE_RP_PANE_PAD, y;
	int count, index;

	count = ae_rv_weapons(in, rows, tail);
	ae_rp_pane(f, 1224.0f, AE_RP_ROW_B_Y, 600.0f, AE_RP_ROW_B_H);
	snprintf(head, sizeof(head), "%s", "TOP WEAPONS");
	/* (the head says what the numbers are: kills; the tail after it) */
	{
		float w = put_tracked(f, AE_FONT_BODY, 16, 0.14f, left, AE_RP_ROW_B_Y + AE_RP_PANE_PAD + 12.0f, AE_ALIGN_LEFT,
			AE_COLOR_MUTED, head);
		float next;

		rect(f, left + w + 12.0f, AE_RP_ROW_B_Y + AE_RP_PANE_PAD + 10.0f, 4.0f, 4.0f, 2.0f, AE_COLOR_MUTED);
		next = left + w + 28.0f;
		next += put_tracked(f, AE_FONT_BODY, 16, 0.14f, next, AE_RP_ROW_B_Y + AE_RP_PANE_PAD + 12.0f, AE_ALIGN_LEFT,
			AE_COLOR_TEXT, "KILLS");
		if (tail[0])
		{
			rect(f, next + 12.0f, AE_RP_ROW_B_Y + AE_RP_PANE_PAD + 10.0f, 4.0f, 4.0f, 2.0f, with_alpha(AE_COLOR_MUTED, 0.7f));
			put_tracked(f, AE_FONT_BODY, 14, 0.12f, next + 28.0f, AE_RP_ROW_B_Y + AE_RP_PANE_PAD + 12.0f, AE_ALIGN_LEFT,
				with_alpha(AE_COLOR_MUTED, 0.7f), tail);
		}
	}
	y = (float)AE_RP_ROW_B_Y + AE_RP_PANE_PAD + 34.0f;
	for (index = 0; index < count; index++)
	{
		float cy = y + 17.0f + (float)index * 34.0f;
		char name[AE_RS_WEAPON_NAME * 2];

		fit(f, AE_FONT_ROW, 20, rows[index].label, right - left - 90.0f, name, sizeof(name));
		put_tracked(f, AE_FONT_ROW, 20, 0.04f, left, cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, name);
		put(f, AE_FONT_ROW, 20, right, cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, rows[index].value);
	}
	if (!count)
		put(f, AE_FONT_ROW, 24, left, y + 17.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, AE_RV_DASH);
}

int ae_rp_modes(struct frame_metrics const *f, struct ae_rv_in const *in)
{
	int kinds;

	draw_objectives(f, in);
	draw_kinds(f, in);
	kinds = draw_medals(f, in);
	draw_maps(f, in);
	draw_weapons(f, in);
	return kinds;
}

void ae_rp_boxes(struct frame_metrics const *f, int page, unsigned long has, int focus, int mouse, int hover, int hit_id)
{
	int ids[AE_RF_BOXES], count = ae_rf_list(page, has, ids), index;

	for (index = 0; index < count; index++)
	{
		struct ae_rf_rect r = ae_rf_rect_of(ids[index]);

		ae_hit_add(tx(f, r.x), ty(f, r.y), r.w * f->m, r.h * f->m, hit_id, AE_PART_CARD, (short)ids[index]);
		if (mouse && hover == ids[index])
			rect(f, r.x, r.y, r.w, r.h, 3.0f, AE_COLOR_HOVER);
		else if (!mouse && focus == ids[index])
			ae_draw_outline(tx(f, r.x - RING_OUT), ty(f, r.y - RING_OUT), (r.w + 2.0f * RING_OUT) * f->m,
				(r.h + 2.0f * RING_OUT) * f->m, 3.0f * f->m, 3.0f * f->m, AE_COLOR_ACCENT);
	}
}
