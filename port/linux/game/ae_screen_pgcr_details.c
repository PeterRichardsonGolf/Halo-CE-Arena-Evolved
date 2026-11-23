/* The post-game report's DETAILS page (see the header). */
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_pgcr_details_rules.h"
#include "ae_pgcr_draw.h"
#include "ae_record_pages.h"
#include "ae_screen_pgcr_details.h"
#include "ae_style.h"

enum { LEFT = 96, TOP = 232, PAD = 28 };

static void head(struct ae_pgr_report const *report, struct ae_pgr_row const *row, struct frame_metrics const *f)
{
	float w, x, name_w;
	char text[AE_PGR_NAME + 8], sub[4][48];
	int count = 0, index;

	w = put_tracked(f, AE_FONT_BODY, 18, 0.14f, LEFT, 96.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, "POST-GAME REPORT");
	/* (a drawn 4 u dot between, never U+00B7) */
	rect(f, LEFT + w + 12.0f, 94.0f, 4.0f, 4.0f, 2.0f, AE_COLOR_MUTED);
	put_tracked(f, AE_FONT_BODY, 18, 0.14f, LEFT + w + 28.0f, 96.0f, AE_ALIGN_LEFT, AE_COLOR_TEXT, "DETAILS");
	rect(f, LEFT, 133.0f, 34.0f, 34.0f, 2.0f, row->chip ? row->chip : AE_COLOR_ACCENT);
	fit(f, AE_FONT_ROW, 44, row->name, 900.0f, text, sizeof(text));
	name_w = put(f, AE_FONT_ROW, 44, LEFT + 50.0f, 150.0f, AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
	x = LEFT + 50.0f + name_w + 16.0f;
	if (row->bot)
		draw_outlined_tag(f, x, 150.0f, "BOT", AE_COLOR_MUTED);
	else if (row->me)
		draw_you_tag(f, x, 150.0f, 0);
	else if (row->rival)
		draw_rival_tag(f, x, 150.0f, 0);
	/* the line under it: team or place, gametype, map, time */
	snprintf(sub[count++], sizeof(sub[0]), "%s", report->teams && row->team >= 0 && row->team < AE_PGR_TEAMS ?
		report->team[row->team].name : row->place);
	snprintf(sub[count++], sizeof(sub[0]), "%s", report->gametype);
	snprintf(sub[count++], sizeof(sub[0]), "%s", report->map);
	snprintf(sub[count++], sizeof(sub[0]), "%s", report->played);
	x = LEFT;
	for (index = 0, w = 0.0f; index < count; index++)
	{
		if (!sub[index][0])
			continue;
		if (w > 0.0f)
		{
			rect(f, x + 12.0f, 194.0f, 4.0f, 4.0f, 2.0f, AE_COLOR_MUTED);
			x += 28.0f;
		}
		x += put_tracked(f, AE_FONT_BODY, 18, 0.08f, x, 196.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, sub[index]);
		w = 1.0f;
	}
}

/* rows of 24 u values at the 46 u pitch */
static void rows_big(struct frame_metrics const *f, float left, float right, float y, struct ae_rv_cell const *cells, int count)
{
	int index;

	for (index = 0; index < count; index++)
		ae_rp_cell(f, left, right, y + 17.0f + (float)index * 46.0f, &cells[index]);
}

/* rows of the second row's panes: label left, a 20 u value right, a 34 u pitch */
static void rows_small(struct frame_metrics const *f, float left, float right, float y, struct ae_rv_cell const *cells, int count)
{
	int index;

	for (index = 0; index < count; index++)
	{
		float cy = y + 17.0f + (float)index * 34.0f;

		put_tracked(f, AE_FONT_BODY, 16, 0.1f, left, cy, AE_ALIGN_LEFT, AE_COLOR_TEXT, cells[index].label);
		put(f, AE_FONT_ROW, 20, right, cy, AE_ALIGN_RIGHT, cells[index].dash ? AE_COLOR_MUTED : AE_COLOR_TITLE, cells[index].value);
	}
}

static void teams(struct ae_pgr_report const *report, struct frame_metrics const *f)
{
	struct ae_pgd_line lines[AE_PGD_LINES];
	char title[AE_PGD_TITLE];
	static char const *const heads[5] = { "K", "D", "A", "ACC", "DAMAGE" };
	float const columns[5] = { 438.0f, 508.0f, 578.0f, 668.0f, 788.0f };
	float y;
	int count, index, column;

	count = ae_pgd_lines(report, lines, title);
	ae_rp_pane(f, LEFT, 696.0f, 720.0f, 264.0f);
	y = ae_rp_label(f, LEFT + PAD, 696.0f + PAD, title, NULL);
	for (column = 0; column < 5; column++)
		put_tracked(f, AE_FONT_BODY, 13, 0.14f, columns[column], 696.0f + PAD + 12.0f, AE_ALIGN_RIGHT, AE_COLOR_MUTED, heads[column]);
	for (index = 0; index < count; index++)
	{
		float cy = y + 22.0f + (float)index * 56.0f;
		char fitted[AE_PGR_NAME];
		char const *values[5];

		values[0] = lines[index].k; values[1] = lines[index].d; values[2] = lines[index].a; values[3] = lines[index].acc;
		values[4] = lines[index].damage;
		rect(f, LEFT + PAD, cy - 13.0f, 26.0f, 26.0f, 2.0f, lines[index].chip ? lines[index].chip : AE_COLOR_ACCENT);
		fit(f, AE_FONT_ROW, 22, lines[index].name, 220.0f, fitted, sizeof(fitted));
		put_tracked(f, AE_FONT_ROW, 22, 0.04f, LEFT + PAD + 40.0f, cy, AE_ALIGN_LEFT,
			lines[index].me ? AE_COLOR_ACCENT : AE_COLOR_TITLE, fitted);
		for (column = 0; column < 5; column++)
			put(f, AE_FONT_ROW, 22, columns[column], cy, AE_ALIGN_RIGHT, strcmp(values[column], AE_RV_DASH) ? AE_COLOR_TITLE :
				AE_COLOR_MUTED, values[column]);
	}
}

void ae_pgcr_details_draw(struct ae_pgr_report const *report, int row_index, struct frame_metrics const *f)
{
	struct ae_pgr_row const *row;
	struct ae_rv_cell cells[AE_PGD_ACCURACY];
	char title[AE_PGD_TITLE];
	float y;
	int count;

	if (row_index < 0 || row_index >= report->rows)
		return;
	row = &report->row[row_index];
	head(report, row, f);
	/* THIS GAME */
	ae_rp_pane(f, LEFT, TOP, AE_RP_FIRST_W, AE_RP_PANE_H);
	y = ae_rp_label(f, LEFT + PAD, TOP + PAD, "THIS GAME", NULL);
	count = ae_pgd_this_game(row, cells);
	rows_big(f, LEFT + PAD, LEFT + AE_RP_FIRST_W - PAD, y, cells, count);
	/* ACCURACY AND DAMAGE: two columns of five */
	ae_rp_pane(f, AE_RP_SECOND_X, TOP, AE_RP_SECOND_W, AE_RP_PANE_H);
	y = ae_rp_label(f, AE_RP_SECOND_X + PAD, TOP + PAD, "ACCURACY AND DAMAGE", NULL);
	count = ae_pgd_accuracy_damage(row, cells);
	rows_big(f, AE_RP_SECOND_X + PAD, 956.0f, y, cells, 5);
	rows_big(f, 1004.0f, AE_RP_SECOND_X + AE_RP_SECOND_W - PAD, y, cells + 5, count - 5);
	/* KILL KINDS */
	ae_rp_pane(f, AE_RP_THIRD_X, TOP, AE_RP_THIRD_W, AE_RP_PANE_H);
	y = ae_rp_label(f, AE_RP_THIRD_X + PAD, TOP + PAD, "KILL KINDS", NULL);
	count = ae_pgd_kill_kinds(row, cells);
	rows_big(f, AE_RP_THIRD_X + PAD, AE_RP_THIRD_X + AE_RP_THIRD_W - PAD, y, cells, count);
	/* the second row: the teams, the kind's block, the assists */
	teams(report, f);
	ae_rp_pane(f, 840.0f, 696.0f, 480.0f, 264.0f);
	count = ae_pgd_mode_block(report, row, title, cells);
	y = ae_rp_label(f, 840.0f + PAD, 696.0f + PAD, title, NULL);
	rows_small(f, 840.0f + PAD, 840.0f + 480.0f - PAD, y, cells, count);
	ae_rp_pane(f, AE_RP_THIRD_X, 696.0f, AE_RP_THIRD_W, 264.0f);
	y = ae_rp_label(f, AE_RP_THIRD_X + PAD, 696.0f + PAD, "ASSISTS", NULL);
	count = ae_pgd_assists(row, cells);
	rows_small(f, AE_RP_THIRD_X + PAD, AE_RP_THIRD_X + AE_RP_THIRD_W - PAD, y, cells, count);
}
