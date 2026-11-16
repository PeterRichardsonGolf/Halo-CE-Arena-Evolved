/*
AE_SCREEN_PGCR.C

The post-game report's board and pane (ae_screen_pgcr.h). Every measure below is the approved mockup's, in its 1920 x 1080
design units, drawn times the frame's width over 1920 (a 4:3 window's frame is narrower).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "math/real_math.h"
#include "../src/ae_draw.h"
#include "../src/ae_layout.h"
#include "ae_glue_pgcr.h"
#include "ae_glue_service_record.h"
#include "ae_hooks.h"
#include "ae_medal_pack.h"
#include "ae_pgcr_draw.h"
#include "ae_screen_record.h"
#include "ae_screen_pgcr.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_test_hooks.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

#define DASH "\xE2\x80\x94"

enum
{
	PGCR_HIT = 0x7600,
	/* (PGCR_HIT + 1 is the reopen's prompts) the player's card: its rows, and the scrim behind it */
	CARD_HIT = PGCR_HIT + 2,
	/* the design (1920 x 1080) */
	BANNER_X = 96, BANNER_Y = 72, BANNER_W = 1728, BANNER_H = 72,
	META_CY = 168,
	BOARD_X = 96, BOARD_W = 1101,
	HEADS_CY = 207, LIST_TOP = 225, LIST_BOTTOM = 936,
	PANE_X = 1236, PANE_W = 588, PANE_Y = 197, PANE_LIMIT = 990,
	PANE_PAD = 28,
	FOOTER_CY = 1020,
	/* the board's rows: full, then compact (more than 16 players) */
	ROW_H = 40, ROW_PITCH = 44, BAR_H = 46, BAR_GAP = 6, BLOCK_GAP = 22,
	ROW_H_COMPACT = 30, ROW_PITCH_COMPACT = 32, BAR_H_COMPACT = 36, BAR_GAP_COMPACT = 4, BLOCK_GAP_COMPACT = 14,
	/* the columns' right edges and the strip */
	COLUMN_K = 879, COLUMN_D = 959, COLUMN_A = 1039, COLUMN_SCORE = 1180,
	STRIP_ICON = 28, STRIP_PITCH = 36,
	WHEEL_STEP = 88
};

/* ink the mockups' betrayal and suicide numbers use (a light red: never the team's, never amber) */
#define LIGHT_RED 0xF2877DFFu
#define PANE_FILL 0x10161CF2u

/* ---------- the view */

void ae_pgcr_view_open(struct ae_pgcr_view *view, struct ae_pgr_report const *report)
{
	memset(view, 0, sizeof(*view));
	view->report = *report;
	view->cursor = ae_pgr_cursor_start(&view->report);
	platform_log("ae pgcr: shown (%d players, %s, summaries %s)", view->report.rows, view->report.teams ? "teams" : "ffa",
		view->report.have_summaries ? "yes" : "no");
}

void ae_pgcr_view_refresh(struct ae_pgcr_view *view, struct ae_pgr_report const *report)
{
	int slot = view->cursor >= 0 && view->cursor < view->report.rows ? view->report.row[view->cursor].slot : -1;
	int row;

	view->report = *report;
	view->cursor = ae_pgr_cursor_step(&view->report, view->cursor, 0);
	for (row = 0; slot >= 0 && row < view->report.rows; row++)
	{
		if (view->report.row[row].slot == slot)
		{
			view->cursor = row;
			break;
		}
	}
}

static void cursor_to(struct ae_pgcr_view *view, int row)
{
	if (row == view->cursor)
		return;
	view->cursor = row;
	if (row >= 0 && row < view->report.rows)
		platform_log("ae pgcr: cursor %d %s", row, view->report.row[row].name);
}

/* ---------- a player's card */

enum { CARD_RIVAL, CARD_RECORD };

static struct ae_pgr_row const *card_row(struct ae_pgcr_view const *view)
{
	int row;

	for (row = 0; row < view->report.rows; row++)
	{
		if (view->report.row[row].slot == view->card_slot)
			return &view->report.row[row];
	}
	return NULL;
}

static int row_can_rival(struct ae_pgcr_view const *view, struct ae_pgr_row const *row)
{
	return !row->me && !row->bot && ae_pgr_owner_name(&view->report)[0];
}

static void card_rows_read(struct ae_pgcr_view *view)
{
	struct ae_pgr_row const *row = card_row(view);

	view->card_count = 0;
	if (!row)
		return;
	if (row_can_rival(view, row))
		view->card_rows[view->card_count++] = CARD_RIVAL;
	view->card_rows[view->card_count++] = CARD_RECORD;
	if (view->card_focus >= view->card_count)
		view->card_focus = view->card_count - 1;
	if (view->card_focus < 0)
		view->card_focus = 0;
}

int ae_pgcr_view_card_is_open(struct ae_pgcr_view const *view)
{
	return view->card_open;
}

int ae_pgcr_view_can_open_card(struct ae_pgcr_view const *view)
{
	return view->cursor >= 0 && view->cursor < view->report.rows && !view->report.row[view->cursor].bot;
}

static void card_close(struct ae_pgcr_view *view)
{
	if (!view->card_open)
		return;
	view->card_open = 0;
	view->card_note[0] = 0;
	platform_log("ae pgcr: card closed");
}

static void card_open(struct ae_pgcr_view *view)
{
	if (!ae_pgcr_view_can_open_card(view))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	view->card_slot = view->report.row[view->cursor].slot;
	view->card_focus = 0;
	view->card_note[0] = 0;
	card_rows_read(view);
	if (!view->card_count)
		return;
	view->card_open = 1;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("ae pgcr: card %s", view->report.row[view->cursor].name);
	/* (debug.capture_on_event's "pgcr_card": the tests' frame of the card) */
	ae_test_capture("pgcr_card", 500);
}

static void card_pick(struct ae_pgcr_view *view)
{
	struct ae_pgr_row const *row = card_row(view);
	char name[AE_PGR_NAME], owner[AE_PGR_NAME];

	if (!row || view->card_focus < 0 || view->card_focus >= view->card_count)
	{
		card_close(view);
		return;
	}
	snprintf(name, sizeof(name), "%s", row->name);
	snprintf(owner, sizeof(owner), "%s", ae_pgr_owner_name(&view->report));
	switch (view->card_rows[view->card_focus])
	{
	case CARD_RIVAL:
		if (ae_record_store_rival(owner, name, !row->rival))
		{
			ae_sound_request(AE_SOUND_FORWARD, 0);
			card_close(view);
			/* (debug.capture_on_event's "pgcr_rival": the tests' frame of the board with the rival marked) */
			ae_test_capture("pgcr_rival", 600);
		}
		else
		{
			snprintf(view->card_note, sizeof(view->card_note), "Your list of rivals is full: take one off first.");
			ae_sound_request(AE_SOUND_FAILURE, 0);
		}
		break;
	case CARD_RECORD:
		ae_sound_request(AE_SOUND_FORWARD, 0);
		card_close(view);
		ae_screen_record_open(AE_OWNER_PARENT, name, owner);
		break;
	}
}

int ae_pgcr_view_handle(struct ae_pgcr_view *view, int action)
{
	int before = view->cursor, next = before;

	if (view->card_open)
	{
		card_rows_read(view);
		switch (action)
		{
		case AE_ACTION_UP:
		case AE_ACTION_DOWN:
			if (view->card_count > 1)
			{
				view->card_focus = (view->card_focus + (action == AE_ACTION_DOWN ? 1 : view->card_count - 1)) % view->card_count;
				ae_sound_request(AE_SOUND_CURSOR, 0);
			}
			break;
		case AE_ACTION_ACCEPT:
			card_pick(view);
			break;
		case AE_ACTION_BACK:
			ae_sound_request(AE_SOUND_BACK, 0);
			card_close(view);
			break;
		}
		return 1;
	}
	switch (action)
	{
	case AE_ACTION_UP: next = ae_pgr_cursor_step(&view->report, before, -1); break;
	case AE_ACTION_DOWN: next = ae_pgr_cursor_step(&view->report, before, 1); break;
	case AE_ACTION_TAB_PREVIOUS: next = ae_pgr_cursor_jump(&view->report, before, -1); break;
	case AE_ACTION_TAB_NEXT: next = ae_pgr_cursor_jump(&view->report, before, 1); break;
	case AE_ACTION_PAGE_UP: next = ae_pgr_cursor_step(&view->report, before, -AE_PGR_PAGE); break;
	case AE_ACTION_PAGE_DOWN: next = ae_pgr_cursor_step(&view->report, before, AE_PGR_PAGE); break;
	case AE_ACTION_X:
		card_open(view);
		return 1;
	case AE_ACTION_ACCEPT:
		if (!view->accept_opens_card)
			return 0;
		card_open(view);
		return 1;
	default: return 0;
	}
	if (next != before)
	{
		cursor_to(view, next);
		ae_sound_request(AE_SOUND_CURSOR, 0);
	}
	return 1;
}

void ae_pgcr_view_pointer(struct ae_pgcr_view *view, struct ae_pointer const *pointer)
{
	struct ae_hit hit;
	int found;

	if (view->card_open)
	{
		found = ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == CARD_HIT;
		if (found && hit.part == AE_PART_ROW && pointer->moved && hit.index >= 0 && hit.index < view->card_count &&
			hit.index != view->card_focus)
		{
			view->card_focus = hit.index;
			ae_sound_request(AE_SOUND_CURSOR, 0);
		}
		if (pointer->left_clicks)
		{
			if (found && hit.part == AE_PART_ROW && hit.index >= 0 && hit.index < view->card_count)
			{
				view->card_focus = hit.index;
				card_pick(view);
			}
			else if (!found || hit.part == AE_PART_OUTSIDE)
				card_close(view);
		}
		return;
	}
	if (pointer->wheel_steps)
		view->scroll -= (float)pointer->wheel_steps * WHEEL_STEP;
	found = ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == PGCR_HIT && hit.part == AE_PART_ROW &&
		hit.index >= 0 && hit.index < view->report.rows;
	if (pointer->moved && found && hit.index != view->cursor)
	{
		cursor_to(view, hit.index);
		ae_sound_request(AE_SOUND_CURSOR, 0);
	}
	if (pointer->left_clicks && found)
	{
		cursor_to(view, hit.index);
		card_open(view);
	}
}

void ae_pgcr_view_footer(struct ae_frame const *frame, float *center_y, float *x, float *right)
{
	float m = frame->rect.width / 1920.0f;

	*center_y = frame->rect.y + FOOTER_CY * m;
	*x = frame->rect.x + BANNER_X * m;
	*right = frame->rect.x + (BANNER_X + BANNER_W) * m;
}

/* ---------- the board */

/* a colour at half its light, for names on the focused (white) row: the mockup's 0.62 gives the multikill blue 3.9:1 there, half
 gives every kind 5:1 or more */
static unsigned int darkened(unsigned int rgba)
{
	unsigned int r = ((rgba >> 24) & 0xFF) * 50 / 100, g = ((rgba >> 16) & 0xFF) * 50 / 100, b = ((rgba >> 8) & 0xFF) * 50 / 100;

	return (r << 24) | (g << 16) | (b << 8) | 0xFF;
}

struct strip_ink
{
	unsigned int color;
	unsigned int muted;
};

/* a medal strip: pictures where every medal shown has one, else the names in their family's colour, then "+N" */
static void draw_strip(struct frame_metrics const *f, struct ae_pgr_row const *row, float x, float right, float cy,
	struct strip_ink const *ink, int focused)
{
	int medals[AE_PGCR_SUMMARY_MEDALS], counts[AE_PGCR_SUMMARY_MEDALS];
	int count = ae_pgr_medals_sorted(&row->summary, medals, counts), shown = 0, index, pictures = 1;
	char rest[16];
	float at = x;

	if (!row->summary.valid)
	{
		put(f, AE_FONT_ROW, 18, x, cy, AE_ALIGN_LEFT, ink->muted, DASH);
		return;
	}
	if (!count)
		return;
	for (index = 0; index < count && index < AE_PGR_STRIP_MEDALS; index++)
	{
		if (!ae_medal_pack_menu_image(medals[index]))
			pictures = 0;
	}
	if (pictures)
	{
		for (index = 0; index < count && index < AE_PGR_STRIP_MEDALS; index++, shown++)
		{
			ae_draw_image(ae_medal_pack_menu_image(medals[index]), tx(f, at), ty(f, cy - STRIP_ICON * 0.5f),
				STRIP_ICON * f->m, STRIP_ICON * f->m, 0xFFFFFFFFu);
			at += STRIP_PITCH;
		}
	}
	else
	{
		float room = right - 52.0f;

		for (index = 0; index < count && index < AE_PGR_STRIP_MEDALS; index++)
		{
			char name[AE_PGR_NAME], word[AE_PGR_NAME + 8];
			unsigned int color = ae_pgr_family_color(ae_pgr_medal_family(medals[index]));
			float w;

			ae_pgr_medal_name(medals[index], name, sizeof(name));
			if (counts[index] > 1)
				snprintf(word, sizeof(word), "%s x%d", name, counts[index]);
			else
				snprintf(word, sizeof(word), "%s", name);
			w = ae_draw_text_tracked_width(AE_FONT_BODY, 16.0f * f->m, 0.06f, word) / f->m + 20.0f;
			if (at + w > room && shown > 0)
				break;
			/* (the focused row is white: the names darkened) */
			put_tracked(f, AE_FONT_BODY, 16, 0.06f, at, cy, AE_ALIGN_LEFT, focused ? darkened(color) : color, word);
			at += w;
			shown++;
		}
	}
	index = ae_pgr_strip_rest(&row->summary, shown);
	if (index > 0)
	{
		snprintf(rest, sizeof(rest), "+%d", index);
		put(f, AE_FONT_BODY, 16, at + 4.0f, cy, AE_ALIGN_LEFT, ink->muted, rest);
	}
}

static void draw_row(struct ae_pgcr_view *view, struct frame_metrics const *f, int index, float y, float h, int compact)
{
	struct ae_pgr_row const *row = &view->report.row[index];
	int focused = index == view->cursor;
	int ffa = !view->report.teams;
	float cy = y + h * 0.5f, chip = compact ? 20.0f : 26.0f;
	float place_x = ffa ? 112.0f : 114.0f, chip_x = ffa ? 162.0f : 140.0f, name_x = ffa ? 200.0f : 179.0f;
	float strip_x = ffa ? 426.0f : 396.0f;
	unsigned int ink = focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TITLE, soft = focused ? AE_COLOR_SELECTION_MUTED : AE_COLOR_MUTED;
	struct strip_ink strip;
	char text[AE_PGR_NAME + 8], name[AE_PGR_NAME];
	float name_w, name_room;

	rect(f, BOARD_X, y, BOARD_W, h, 2.0f, focused ? AE_COLOR_SELECTION : AE_COLOR_ROW);
	if (focused)
		rect(f, BOARD_X, y, 5.0f, h, 0.0f, AE_COLOR_ACCENT);
	if (row->rival)
		rect(f, BOARD_X + BOARD_W - 5.0f, y, 5.0f, h, 0.0f, focused ? AE_PGCR_RIVAL_INK_ON_LIGHT : AE_PGCR_RIVAL_INK);
	ae_hit_add(tx(f, BOARD_X), ty(f, y), BOARD_W * f->m, h * f->m, PGCR_HIT, AE_PART_ROW, (short)index);
	put(f, AE_FONT_BODY, compact ? 14 : 16, place_x, cy, AE_ALIGN_LEFT, soft, row->place);
	rect(f, chip_x, cy - chip * 0.5f, chip, chip, 2.0f, row->chip);
	ae_draw_outline(tx(f, chip_x), ty(f, cy - chip * 0.5f), chip * f->m, chip * f->m, 2.0f * f->m, f->m > 0.5f ? 1.0f : 0.5f, AE_COLOR_RULE);
	name_room = (ffa ? 400.0f : 360.0f) - (row->me ? 54.0f : row->rival ? 76.0f : 0.0f);
	fit(f, AE_FONT_ROW, compact ? 18 : 21, row->name, name_room, name, sizeof(name));
	name_w = put(f, AE_FONT_ROW, compact ? 18 : 21, name_x, cy, AE_ALIGN_LEFT, row->quit ? soft : ink, name);
	if (row->me)
		draw_you_tag(f, name_x + name_w + 12.0f, cy, focused);
	else if (row->rival)
		draw_rival_tag(f, name_x + name_w + 12.0f, cy, focused);
	strip.color = ink;
	strip.muted = soft;
	draw_strip(f, row, strip_x, COLUMN_K - 52.0f, cy, &strip, focused);
	snprintf(text, sizeof(text), "%ld", row->kills);
	put(f, AE_FONT_ROW, compact ? 19 : 22, COLUMN_K, cy, AE_ALIGN_RIGHT, ink, text);
	snprintf(text, sizeof(text), "%ld", row->deaths);
	put(f, AE_FONT_ROW, compact ? 19 : 22, COLUMN_D, cy, AE_ALIGN_RIGHT, ink, text);
	snprintf(text, sizeof(text), "%ld", row->assists);
	put(f, AE_FONT_ROW, compact ? 19 : 22, COLUMN_A, cy, AE_ALIGN_RIGHT, ink, text);
	put(f, AE_FONT_ROW, compact ? 19 : 22, COLUMN_SCORE, cy, AE_ALIGN_RIGHT, ink, row->score);
}

static void draw_bar(struct frame_metrics const *f, struct ae_pgr_team const *team, float y, float h)
{
	char text[24];
	float cy = y + h * 0.5f;

	rect(f, BOARD_X, y, BOARD_W, h, 2.0f, with_alpha(team->color, 235.0f / 255.0f));
	put_tracked(f, AE_FONT_ROW, h > 40 ? 24 : 20, 0.04f, 114.0f, cy, AE_ALIGN_LEFT, 0xFFFFFFFFu, team->name);
	snprintf(text, sizeof(text), "%ld", team->kills);
	put(f, AE_FONT_ROW, h > 40 ? 22 : 19, COLUMN_K, cy, AE_ALIGN_RIGHT, 0xFFFFFFFFu, text);
	snprintf(text, sizeof(text), "%ld", team->deaths);
	put(f, AE_FONT_ROW, h > 40 ? 22 : 19, COLUMN_D, cy, AE_ALIGN_RIGHT, 0xFFFFFFFFu, text);
	snprintf(text, sizeof(text), "%ld", team->assists);
	put(f, AE_FONT_ROW, h > 40 ? 22 : 19, COLUMN_A, cy, AE_ALIGN_RIGHT, 0xFFFFFFFFu, text);
	put(f, AE_FONT_ROW, h > 40 ? 32 : 26, COLUMN_SCORE, cy, AE_ALIGN_RIGHT, 0xFFFFFFFFu, team->score);
}

struct board_measure
{
	float row_h, pitch, bar_h, bar_gap, block_gap;
	int compact;
};

static void measure_board(struct board_measure *m, int compact)
{
	m->compact = compact;
	m->row_h = compact ? ROW_H_COMPACT : ROW_H;
	m->pitch = compact ? ROW_PITCH_COMPACT : ROW_PITCH;
	m->bar_h = compact ? BAR_H_COMPACT : BAR_H;
	m->bar_gap = compact ? BAR_GAP_COMPACT : BAR_GAP;
	m->block_gap = compact ? BLOCK_GAP_COMPACT : BLOCK_GAP;
}

/* the content's height, and the top of a row in it */
static float board_content(struct ae_pgr_report const *report, struct board_measure const *m, int cursor, float *cursor_top)
{
	float content = 0.0f;
	int group, index;

	*cursor_top = 0.0f;
	if (report->teams)
	{
		for (group = 0; group < AE_PGR_TEAMS; group++)
		{
			struct ae_pgr_team const *team = &report->team[group];

			if (!team->count)
				continue;
			if (content > 0.0f)
				content += m->block_gap;
			content += m->bar_h + m->bar_gap;
			for (index = team->first; index < team->first + team->count; index++, content += m->pitch)
			{
				if (index == cursor)
					*cursor_top = content;
			}
			content -= m->pitch - m->row_h;
		}
		return content;
	}
	for (index = 0; index < report->rows; index++, content += m->pitch)
	{
		if (index == cursor)
			*cursor_top = content;
	}
	return report->rows ? content - (m->pitch - m->row_h) : 0.0f;
}

/* the board: the column heads, the bars and the rows (full rows, compact ones when the full do not fit, a scroll when
neither does); returns its foot (design units) */
static float draw_board(struct ae_pgcr_view *view, struct frame_metrics const *f)
{
	struct ae_pgr_report const *report = &view->report;
	struct board_measure m;
	float y, cursor_top, content, room = LIST_BOTTOM - LIST_TOP;
	int group, index;

	measure_board(&m, 0);
	content = board_content(report, &m, view->cursor, &cursor_top);
	if (content > room)
	{
		measure_board(&m, 1);
		content = board_content(report, &m, view->cursor, &cursor_top);
	}
	/* the heads, drawn once above the first bar */
	put_tracked(f, AE_FONT_BODY, 13, 0.14f, report->teams ? 396.0f : 426.0f, HEADS_CY, AE_ALIGN_LEFT, AE_COLOR_MUTED, "MEDALS");
	put_tracked(f, AE_FONT_BODY, 13, 0.14f, COLUMN_K + 6.0f, HEADS_CY, AE_ALIGN_RIGHT, AE_COLOR_MUTED, "K");
	put_tracked(f, AE_FONT_BODY, 13, 0.14f, COLUMN_D + 6.0f, HEADS_CY, AE_ALIGN_RIGHT, AE_COLOR_MUTED, "D");
	put_tracked(f, AE_FONT_BODY, 13, 0.14f, COLUMN_A + 6.0f, HEADS_CY, AE_ALIGN_RIGHT, AE_COLOR_MUTED, "A");
	put_tracked(f, AE_FONT_BODY, 13, 0.14f, COLUMN_SCORE, HEADS_CY, AE_ALIGN_RIGHT, AE_COLOR_MUTED, "SCORE");
	/* (the cursor's row kept in view; the scroll held to the content) */
	if (cursor_top < view->scroll)
		view->scroll = cursor_top - (report->teams ? m.bar_h + m.bar_gap : 0.0f);
	if (cursor_top + m.row_h > view->scroll + room)
		view->scroll = cursor_top + m.row_h - room;
	if (view->scroll > content - room)
		view->scroll = content - room;
	if (view->scroll < 0.0f)
		view->scroll = 0.0f;
	ae_draw_clip_push(tx(f, BOARD_X - 4.0f), ty(f, LIST_TOP), (BOARD_W + 8.0f) * f->m, room * f->m);
	y = LIST_TOP - view->scroll;
	if (report->teams)
	{
		int first = 1;

		for (group = 0; group < AE_PGR_TEAMS; group++)
		{
			struct ae_pgr_team const *team = &report->team[group];

			if (!team->count)
				continue;
			if (!first)
				y += m.block_gap;
			first = 0;
			draw_bar(f, team, y, m.bar_h);
			y += m.bar_h + m.bar_gap;
			for (index = team->first; index < team->first + team->count; index++, y += m.pitch)
				draw_row(view, f, index, y, m.row_h, m.compact);
			y -= m.pitch - m.row_h;
		}
	}
	else
	{
		for (index = 0; index < report->rows; index++, y += m.pitch)
			draw_row(view, f, index, y, m.row_h, m.compact);
	}
	ae_draw_clip_pop();
	return LIST_TOP + (content < room ? content : room);
}

/* ---------- the pane */

struct cell
{
	char label[AE_PGR_WORD];
	char value[24];
	unsigned int color;
};

static int add_cell(struct cell *cells, int count, char const *label, char const *value, unsigned int color)
{
	snprintf(cells[count].label, sizeof(cells[count].label), "%s", label);
	snprintf(cells[count].value, sizeof(cells[count].value), "%s", value);
	cells[count].color = color;
	return count + 1;
}

static char const *row_name_of_slot(struct ae_pgr_report const *report, int slot)
{
	int row;

	for (row = 0; row < report->rows; row++)
	{
		if (report->row[row].slot == slot)
			return report->row[row].name;
	}
	return NULL;
}

/* a section's label at its top y: returns where its content starts (the label's line is 34 high) */
static float section_label(struct frame_metrics const *f, float x, float y, char const *label, char const *tail)
{
	float w = put_tracked(f, AE_FONT_BODY, 16, 0.14f, x, y + 12.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);

	if (tail && tail[0])
		put_tracked(f, AE_FONT_BODY, 16, 0.14f, x + w + 14.0f, y + 12.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, tail);
	return y + 34.0f;
}

/* the stats cells of a player: the game's kind first, then the numbers every game has */
static int stat_cells(struct ae_pgr_report const *report, struct ae_pgr_row const *row, struct cell *cells)
{
	int count = 0, index, has_summary = row->summary.valid;
	char number[24];

	if (report->mode == AE_PGR_MODE_BALL || report->mode == AE_PGR_MODE_HILL)
		count = add_cell(cells, count, report->score_label, row->score, AE_COLOR_TITLE);
	for (index = 0; index < AE_PGR_OBJECTIVES; index++)
	{
		if (!report->objective_label[index][0])
			continue;
		ae_pgr_number_text(row->objective[index], number, sizeof(number));
		count = add_cell(cells, count, report->objective_label[index], number, AE_COLOR_TITLE);
	}
	ae_pgr_number_text(row->kills, number, sizeof(number));
	count = add_cell(cells, count, "KILLS", number, AE_COLOR_TITLE);
	ae_pgr_number_text(row->deaths, number, sizeof(number));
	count = add_cell(cells, count, "DEATHS", number, AE_COLOR_TITLE);
	ae_pgr_number_text(row->assists, number, sizeof(number));
	count = add_cell(cells, count, "ASSISTS", number, AE_COLOR_TITLE);
	ae_pgr_kd_text(row->kills, row->deaths, number, sizeof(number));
	count = add_cell(cells, count, "K/D", number, AE_COLOR_TITLE);
	ae_pgr_number_text(has_summary ? row->summary.best_spree : AE_PGR_UNKNOWN, number, sizeof(number));
	count = add_cell(cells, count, "BEST SPREE", number, AE_COLOR_TITLE);
	ae_pgr_number_text(has_summary ? row->summary.multikills : AE_PGR_UNKNOWN, number, sizeof(number));
	count = add_cell(cells, count, "MULTIKILLS", number, AE_COLOR_TITLE);
	ae_pgr_number_text(row->betrayals, number, sizeof(number));
	count = add_cell(cells, count, "BETRAYALS", number, row->betrayals > 0 ? LIGHT_RED : AE_COLOR_TITLE);
	ae_pgr_number_text(row->suicides, number, sizeof(number));
	count = add_cell(cells, count, "SUICIDES", number, row->suicides > 0 ? LIGHT_RED : AE_COLOR_TITLE);
	ae_pgr_number_text(has_summary ? row->summary.headshots : AE_PGR_UNKNOWN, number, sizeof(number));
	count = add_cell(cells, count, "HEADSHOTS", number, AE_COLOR_TITLE);
	ae_pgr_accuracy_text(row->shots_fired, row->shots_hit, number, sizeof(number));
	return add_cell(cells, count, "ACCURACY", number, AE_COLOR_TITLE);
}

enum { PICTURE_PITCH = 76, PICTURE_ROW = 68, PICTURES_PER_ROW = 7, MOST_PICTURES = 14, MOST_NAMES = 10, NAME_ROW = 32 };

static void draw_pane(struct ae_pgcr_view *view, struct frame_metrics const *f)
{
	struct ae_pgr_report const *report = &view->report;
	struct ae_pgr_row const *row;
	struct cell cells[16];
	int cells_count, medals[AE_PGCR_SUMMARY_MEDALS], counts[AE_PGCR_SUMMARY_MEDALS], medal_count, index;
	int has_summary, show_points, show_rounds, show_versus, weapons, pictures = 1, medal_shown;
	long pk = 0, ps = 0, pb = 0;
	char text[64], number[24];
	float left = PANE_X + PANE_PAD, right = PANE_X + PANE_W - PANE_PAD, y = PANE_Y, height, medals_h;

	if (view->cursor < 0 || view->cursor >= report->rows)
		return;
	row = &report->row[view->cursor];
	has_summary = row->summary.valid;
	cells_count = stat_cells(report, row, cells);
	medal_count = ae_pgr_medals_sorted(&row->summary, medals, counts);
	for (index = 0; index < medal_count && index < MOST_PICTURES; index++)
	{
		if (!ae_medal_pack_menu_image(medals[index]))
			pictures = 0;
	}
	medal_shown = medal_count < (pictures ? MOST_PICTURES : MOST_NAMES) ? medal_count : (pictures ? MOST_PICTURES : MOST_NAMES);
	medals_h = pictures ? ((medal_shown + PICTURES_PER_ROW - 1) / PICTURES_PER_ROW) * (float)PICTURE_ROW :
		((medal_shown + 1) / 2) * (float)NAME_ROW;
	if (medal_count > medal_shown)
		medals_h += 24.0f;
	show_points = ae_pgr_points_add_up(row, &pk, &ps, &pb) && (ps > 0 || pb > 0);
	show_rounds = report->rounds;
	show_versus = 1;
	weapons = has_summary ? (row->summary.weapon_count < 3 ? row->summary.weapon_count : 3) : 0;
	/* the height: the player, the weapons, the stats and the medals, then the optional blocks, the lowest dropped first when
	they would not fit above the footer */
	for (;;)
	{
		height = 84.0f + 34.0f + (weapons > 1 ? 34.0f + (weapons - 1) * 28.0f : 34.0f) + 22.0f + 34.0f +
			((cells_count + 1) / 2) * 34.0f + 14.0f + 34.0f + medals_h + 14.0f + 16.0f;
		if (show_points)
			height += 34.0f + 34.0f * (1 + (ps > 0) + (pb > 0)) + 14.0f;
		if (show_rounds)
			height += 34.0f + 34.0f + 14.0f;
		if (show_versus)
			height += 34.0f + 60.0f;
		if (PANE_Y + height <= PANE_LIMIT || (!show_points && !show_rounds && !show_versus))
			break;
		if (show_points)
			show_points = 0;
		else if (show_rounds)
			show_rounds = 0;
		else
			show_versus = 0;
	}
	rect(f, PANE_X, PANE_Y, PANE_W, height, 3.0f, PANE_FILL);
	rect(f, PANE_X, PANE_Y, PANE_W, 4.0f, 0.0f, AE_COLOR_ACCENT);
	/* the player */
	{
		char name[AE_PGR_NAME];
		float cy = y + 46.0f, name_w;

		rect(f, left, cy - 18.0f, 36.0f, 36.0f, 2.0f, row->chip);
		fit(f, AE_FONT_ROW, 30, row->name, 280.0f, name, sizeof(name));
		name_w = put(f, AE_FONT_ROW, 30, left + 49.0f, cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, name);
		if (row->me)
			draw_you_tag(f, left + 49.0f + name_w + 14.0f, cy, 0);
		put_tracked(f, AE_FONT_BODY, 16, 0.1f, right, cy, AE_ALIGN_RIGHT, AE_COLOR_MUTED,
			report->teams ? report->team[row->team > 0 ? 1 : 0].name : row->place);
		y += 84.0f;
	}
	/* the weapons of choice: the first large, the next two under it */
	y = section_label(f, left, y, "WEAPON OF CHOICE", NULL);
	if (weapons > 0)
	{
		for (index = 0; index < weapons; index++)
		{
			float cy = index == 0 ? y + 17.0f : y + 34.0f + 14.0f + (index - 1) * 28.0f;
			char const *name = row->weapon_name[index][0] ? row->weapon_name[index] : "UNKNOWN";

			ae_pgr_kills_text(row->summary.weapons[index].kills, number, sizeof(number));
			put_tracked(f, AE_FONT_ROW, index == 0 ? 24 : 19, 0.04f, left, cy, AE_ALIGN_LEFT,
				index == 0 ? AE_COLOR_TITLE : AE_COLOR_TEXT, name);
			put(f, AE_FONT_BODY, 16, right, cy, AE_ALIGN_RIGHT, AE_COLOR_MUTED, number);
		}
		y += weapons > 1 ? 34.0f + (weapons - 1) * 28.0f : 34.0f;
	}
	else
	{
		put(f, AE_FONT_ROW, 24, left, y + 17.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, DASH);
		y += 34.0f;
	}
	y += 22.0f;
	/* the stats, two columns */
	y = section_label(f, left, y, "STATS", NULL);
	for (index = 0; index < cells_count; index++)
	{
		float column_x = index % 2 ? PANE_X + PANE_W * 0.5f + 14.0f : left;
		float column_right = index % 2 ? right : PANE_X + PANE_W * 0.5f - 24.0f;
		float cy = y + 17.0f + (index / 2) * 34.0f;
		unsigned int value_color = strcmp(cells[index].value, DASH) ? cells[index].color : AE_COLOR_MUTED;

		put_tracked(f, AE_FONT_BODY, 16, 0.1f, column_x, cy, AE_ALIGN_LEFT, AE_COLOR_TEXT, cells[index].label);
		put(f, AE_FONT_ROW, 24, column_right, cy, AE_ALIGN_RIGHT, value_color, cells[index].value);
	}
	y += ((cells_count + 1) / 2) * 34.0f + 14.0f;
	if (show_points)
	{
		float cy;

		y = section_label(f, left, y, "POINTS", NULL);
		cy = y + 17.0f;
		snprintf(number, sizeof(number), "+%ld", pk);
		put_tracked(f, AE_FONT_BODY, 16, 0.1f, left, cy, AE_ALIGN_LEFT, AE_COLOR_TEXT, "KILLS");
		put(f, AE_FONT_ROW, 22, right, cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, number);
		if (ps > 0)
		{
			cy += 34.0f;
			snprintf(number, sizeof(number), "-%ld", ps);
			put_tracked(f, AE_FONT_BODY, 16, 0.1f, left, cy, AE_ALIGN_LEFT, AE_COLOR_TEXT, "SUICIDES");
			put(f, AE_FONT_ROW, 22, right, cy, AE_ALIGN_RIGHT, LIGHT_RED, number);
		}
		if (pb > 0)
		{
			cy += 34.0f;
			snprintf(number, sizeof(number), "-%ld", pb);
			put_tracked(f, AE_FONT_BODY, 16, 0.1f, left, cy, AE_ALIGN_LEFT, AE_COLOR_TEXT, "BETRAYALS");
			put(f, AE_FONT_ROW, 22, right, cy, AE_ALIGN_RIGHT, LIGHT_RED, number);
		}
		y += 34.0f * (1 + (ps > 0) + (pb > 0)) + 14.0f;
	}
	if (show_rounds)
	{
		char one[40], two[40];

		y = section_label(f, left, y, "ROUNDS WON", NULL);
		snprintf(one, sizeof(one), "%s  %ld", report->team[0].name, report->rounds_won[0]);
		snprintf(two, sizeof(two), "%s  %ld", report->team[1].name, report->rounds_won[1]);
		put_tracked(f, AE_FONT_ROW, 19, 0.04f, left, y + 17.0f, AE_ALIGN_LEFT, AE_COLOR_TITLE, one);
		put_tracked(f, AE_FONT_ROW, 19, 0.04f, right, y + 17.0f, AE_ALIGN_RIGHT, AE_COLOR_TITLE, two);
		y += 34.0f + 14.0f;
	}
	/* the medals: pictures when every one shown has one, else their names in their family's colour */
	if (has_summary)
		snprintf(number, sizeof(number), "%d", ae_pgr_medal_total(&row->summary));
	else
		snprintf(number, sizeof(number), DASH);
	y = section_label(f, left, y, "MEDALS", number);
	for (index = 0; index < medal_shown; index++)
	{
		if (pictures)
		{
			float ix = left + (index % PICTURES_PER_ROW) * (float)PICTURE_PITCH, iy = y + (index / PICTURES_PER_ROW) * (float)PICTURE_ROW;

			ae_draw_image(ae_medal_pack_menu_image(medals[index]), tx(f, ix), ty(f, iy), 52.0f * f->m, 52.0f * f->m, 0xFFFFFFFFu);
			if (counts[index] > 1)
			{
				snprintf(number, sizeof(number), "x%d", counts[index]);
				put(f, AE_FONT_ROW, 15, ix + 58.0f, iy + 46.0f, AE_ALIGN_CENTER, AE_COLOR_TITLE, number);
			}
		}
		else
		{
			char name[AE_PGR_NAME];
			float cx = left + (index % 2) * (PANE_W * 0.5f - 14.0f), cy = y + 16.0f + (index / 2) * (float)NAME_ROW;
			unsigned int color = ae_pgr_family_color(ae_pgr_medal_family(medals[index]));
			float w;

			ae_pgr_medal_name(medals[index], name, sizeof(name));
			w = put_tracked(f, AE_FONT_BODY, 17, 0.06f, cx, cy, AE_ALIGN_LEFT, color, name);
			if (counts[index] > 1)
			{
				snprintf(number, sizeof(number), "x%d", counts[index]);
				put(f, AE_FONT_BODY, 16, cx + w + 8.0f, cy, AE_ALIGN_LEFT, color, number);
			}
		}
	}
	if (medal_count > medal_shown)
	{
		snprintf(number, sizeof(number), "+%d more", ae_pgr_strip_rest(&row->summary, medal_shown));
		put(f, AE_FONT_BODY, 16, right, y + medals_h - 12.0f, AE_ALIGN_RIGHT, AE_COLOR_MUTED, number);
	}
	y += medals_h + 14.0f;
	if (show_versus)
	{
		static char const *const labels[2] = { "KILLED MOST", "KILLED BY MOST" };
		int which;

		y = section_label(f, left, y, "VERSUS", NULL);
		for (which = 0; which < 2; which++)
		{
			int slot = has_summary ? (which ? row->summary.killed_by_most : row->summary.killed_most) : AE_PGCR_NO_PLAYER;
			int times = has_summary ? (which ? row->summary.killed_by_most_count : row->summary.killed_most_count) : 0;
			char const *who = slot != AE_PGCR_NO_PLAYER ? row_name_of_slot(report, slot) : NULL;
			char name[AE_PGR_NAME];
			float cy = y + 15.0f + which * 30.0f;

			put_tracked(f, AE_FONT_BODY, 16, 0.1f, left, cy, AE_ALIGN_LEFT, AE_COLOR_TEXT, labels[which]);
			if (!who)
			{
				put(f, AE_FONT_ROW, 20, right, cy, AE_ALIGN_RIGHT, AE_COLOR_MUTED, DASH);
				continue;
			}
			fit(f, AE_FONT_ROW, 20, who, 200.0f, name, sizeof(name));
			snprintf(text, sizeof(text), "%s  %d", name, times);
			put(f, AE_FONT_ROW, 20, right, cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, text);
		}
	}
}

/* ---------- the card */

enum { CARD_X = 660, CARD_W = 600, CARD_Y = 300, CARD_PAD = 28, CARD_HEAD = 84, CARD_HELP_LINES = 3 };

static void card_help(struct ae_pgcr_view const *view, struct ae_pgr_row const *row, char *text, size_t size)
{
	if (view->card_note[0])
		snprintf(text, size, "%s", view->card_note);
	else if (view->card_count <= 0)
		text[0] = 0;
	else if (view->card_rows[view->card_focus] == CARD_RIVAL)
	{
		if (row->rival)
			snprintf(text, size, "%s stops being marked on the boards, and the record stops counting your kills against them.",
				row->name);
		else
			snprintf(text, size, "%s is marked on every board from now on, and your record counts your kills against them.",
				row->name);
	}
	else
		snprintf(text, size, "What this PC has seen of %s over the games it played.", row->name);
}

static void draw_card(struct ae_pgcr_view *view, struct frame_metrics const *f)
{
	struct ae_pgr_row const *row = card_row(view);
	struct ae_density density;
	struct ae_layout layout;
	struct ae_row widgets[3];
	struct ae_row_layout lay;
	char labels[3][24], name[AE_PGR_NAME], help[200], sub[48], lines[CARD_HELP_LINES][160];
	float left = CARD_X + CARD_PAD, width = CARD_W - 2.0f * CARD_PAD, row_h, gap, rows_h, y, panel_h, name_w, help_y;
	int index, line_count;

	if (!row)
	{
		card_close(view);
		return;
	}
	card_rows_read(view);
	ae_draw_current_layout(&layout);
	ae_density_full(layout.height * layout.scale, ae_settings_ui_scale(), &density);
	ae_draw_rect(0.0f, 0.0f, layout.width, layout.height, 0.0f, 0x04070AB8u);
	ae_hit_add(0.0f, 0.0f, layout.width, layout.height, CARD_HIT, AE_PART_OUTSIDE, -1);
	memset(widgets, 0, sizeof(widgets));
	for (index = 0; index < view->card_count; index++)
	{
		if (view->card_rows[index] == CARD_RIVAL)
			snprintf(labels[index], sizeof(labels[index]), "%s", row->rival ? "REMOVE RIVAL" : "SET RIVAL");
		else
			snprintf(labels[index], sizeof(labels[index]), "SERVICE RECORD");
		widgets[index].label = labels[index];
		widgets[index].flags = index == view->card_focus ? AE_ROW_FOCUSED : 0;
	}
	ae_row_layout(&density, width * f->m, &widgets[0], &lay);
	row_h = lay.height / f->m;
	gap = density.metrics->gap * density.unit / f->m;
	rows_h = view->card_count * (row_h + gap);
	card_help(view, row, help, sizeof(help));
	line_count = help[0] ? ae_wrap_text(AE_FONT_BODY, 16.0f * f->m, help, width * f->m, lines, CARD_HELP_LINES) : 0;
	panel_h = CARD_HEAD + 12.0f + rows_h + 14.0f + CARD_HELP_LINES * 24.0f + 14.0f;
	rect(f, CARD_X, CARD_Y, CARD_W, panel_h, 3.0f, 0x10161CFFu);
	rect(f, CARD_X, CARD_Y, CARD_W, 4.0f, 0.0f, AE_COLOR_ACCENT);
	/* (the panel itself takes a click, so only the scrim around it closes the card) */
	ae_hit_add(tx(f, CARD_X), ty(f, CARD_Y), CARD_W * f->m, panel_h * f->m, CARD_HIT, AE_PART_CARD, -1);
	/* the player: chip, name, YOU / RIVAL tag; under it the team or place, tracked */
	y = CARD_Y + CARD_PAD;
	rect(f, left, y + 2.0f, 36.0f, 36.0f, 2.0f, row->chip);
	fit(f, AE_FONT_ROW, 30, row->name, width - 49.0f - 90.0f, name, sizeof(name));
	name_w = put(f, AE_FONT_ROW, 30, left + 49.0f, y + 20.0f, AE_ALIGN_LEFT, AE_COLOR_TITLE, name);
	if (row->me)
		draw_you_tag(f, left + 49.0f + name_w + 14.0f, y + 20.0f, 0);
	else if (row->rival)
		draw_rival_tag(f, left + 49.0f + name_w + 14.0f, y + 20.0f, 0);
	snprintf(sub, sizeof(sub), "%s", view->report.teams ? view->report.team[row->team > 0 ? 1 : 0].name : row->place);
	put_tracked(f, AE_FONT_BODY, 16, 0.1f, left + 49.0f, y + 50.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, sub);
	/* the rows */
	y = CARD_Y + CARD_HEAD + 12.0f;
	for (index = 0; index < view->card_count; index++)
	{
		ae_widget_row(&density, tx(f, left), ty(f, y), width * f->m, &widgets[index], CARD_HIT, (short)index);
		y += row_h + gap;
	}
	help_y = y + 14.0f;
	for (index = 0; index < line_count; index++)
		put(f, AE_FONT_BODY, 16, left, help_y + 12.0f + index * 24.0f, AE_ALIGN_LEFT,
			view->card_note[0] ? AE_COLOR_WARNING : AE_COLOR_MUTED, lines[index]);
}

/* ---------- the banner and the whole */

static void draw_banner(struct ae_pgr_report const *report, struct frame_metrics const *f)
{
	struct ae_density density;
	struct ae_layout layout;
	char fitted[AE_PGR_NAME + 8];
	float score_w;
	char const *parts[4];
	int count = 0, index;
	float x = BANNER_X;

	rect(f, BANNER_X, BANNER_Y, BANNER_W, BANNER_H, 3.0f, with_alpha(report->banner_color, 235.0f / 255.0f));
	if (report->banner_stripe)
		rect(f, BANNER_X, BANNER_Y, 8.0f, BANNER_H, 0.0f, report->banner_stripe);
	score_w = width_of(f, AE_FONT_ROW, 44, report->banner_score);
	fit(f, AE_FONT_ROW, 44, report->banner, BANNER_W - 56.0f - score_w - 32.0f, fitted, sizeof(fitted));
	put(f, AE_FONT_ROW, 44, BANNER_X + 28.0f, BANNER_Y + BANNER_H * 0.5f, AE_ALIGN_LEFT, report->banner_text_color, fitted);
	put(f, AE_FONT_ROW, 44, BANNER_X + BANNER_W - 28.0f, BANNER_Y + BANNER_H * 0.5f, AE_ALIGN_RIGHT, report->banner_text_color,
		report->banner_score);
	/* the line under it: the gametype, the map, the time, and the viewer's place in a free for all */
	parts[count++] = report->gametype;
	parts[count++] = report->map;
	parts[count++] = report->played;
	if (report->you[0])
		parts[count++] = report->you;
	ae_draw_current_layout(&layout);
	ae_density_full(layout.height * layout.scale, ae_settings_ui_scale(), &density);
	for (index = 0; index < count; index++)
	{
		if (!parts[index][0])
			continue;
		if (index > 0)
		{
			ae_widget_separator(&density, tx(f, x + 11.0f), ty(f, META_CY), 4.0f * f->m, AE_COLOR_MUTED);
			x += 22.0f;
		}
		x += put_tracked(f, AE_FONT_BODY, 18, 0.08f, x, META_CY, AE_ALIGN_LEFT, AE_COLOR_MUTED, parts[index]);
	}
}

float ae_pgcr_view_draw(struct ae_pgcr_view *view, struct ae_frame const *frame)
{
	struct frame_metrics f;
	float foot;

	f.m = frame->rect.width / 1920.0f;
	f.x = frame->rect.x;
	f.y = frame->rect.y;
	if (!view->report.valid)
		return frame->rect.y;
	/* (the rivals now: a card's change shows at once) */
	ae_pgr_mark_rivals(&view->report, ae_record_is_rival, NULL);
	draw_banner(&view->report, &f);
	foot = draw_board(view, &f);
	draw_pane(view, &f);
	if (view->card_open)
		draw_card(view, &f);
	return ty(&f, foot);
}

/* ---------- the lobby's reopen */

#define BACKDROP 0x04070AFFu
#define HIT_LAST_PROMPTS (PGCR_HIT + 1)

static struct
{
	int on_stack;
	struct ae_pgcr_view view;
	struct ae_prompt prompts[4];
	short prompt_count;
} last;

static struct ae_screen_class const last_class;

int ae_screen_pgcr_has_last(void)
{
	return ae_glue_pgcr_kept() != NULL;
}

int ae_screen_pgcr_open_last(short owner)
{
	struct ae_pgr_report const *kept = ae_glue_pgcr_kept();

	if (!kept || last.on_stack)
		return 0;
	memset(&last, 0, sizeof(last));
	ae_pgcr_view_open(&last.view, kept);
	last.view.accept_opens_card = 1;
	if (!ae_ui_push(&last_class, owner, &last))
		return 0;
	last.on_stack = 1;
	/* (debug.capture_on_event's "pgcr_last": the tests' frame of the reopened report) */
	ae_test_capture("pgcr_last", 700);
	return 1;
}

static void last_leave(struct ae_screen *screen)
{
	(void)screen;
	last.on_stack = 0;
	platform_log("ae pgcr: closed");
}

static int last_handle(struct ae_screen *screen, struct ae_event const *event)
{
	(void)screen;
	if (event->action == AE_ACTION_BACK && !ae_pgcr_view_card_is_open(&last.view))
		return 0;
	return ae_pgcr_view_handle(&last.view, event->action);
}

static void last_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct ae_event event;

	(void)screen;
	ae_pgcr_view_pointer(&last.view, pointer);
	if (ae_prompts_pointer(last.prompts, last.prompt_count, pointer, HIT_LAST_PROMPTS, &event))
		ae_ui_dispatch(&event);
}

static void last_draw(struct ae_screen *screen)
{
	struct ae_layout layout;
	struct ae_frame frame;
	struct ae_density density;
	float center_y, x, right;

	(void)screen;
	ae_draw_view_full();
	ae_draw_current_layout(&layout);
	ae_frame_compute(layout.width, ae_settings_ui_scale(), &frame);
	ae_draw_rect(0.0f, 0.0f, layout.width, layout.height, 0.0f, BACKDROP);
	ae_pgcr_view_draw(&last.view, &frame);
	ae_pgcr_view_footer(&frame, &center_y, &x, &right);
	ae_density_full(layout.height * layout.scale, ae_settings_ui_scale(), &density);
	last.prompt_count = 0;
	memset(last.prompts, 0, sizeof(last.prompts));
	if (ae_pgcr_view_card_is_open(&last.view))
	{
		last.prompts[last.prompt_count].button = AE_BUTTON_A;
		last.prompts[last.prompt_count].label = "Select";
		last.prompts[last.prompt_count++].action = AE_ACTION_ACCEPT;
	}
	else
	{
		if (ae_pgcr_view_can_open_card(&last.view))
		{
			last.prompts[last.prompt_count].button = AE_BUTTON_A;
			last.prompts[last.prompt_count].label = "Player";
			last.prompts[last.prompt_count++].action = AE_ACTION_ACCEPT;
		}
		if (last.view.report.teams)
		{
			last.prompts[last.prompt_count].button = AE_BUTTON_LEFT_SHOULDER;
			last.prompts[last.prompt_count].label = "Team";
			last.prompts[last.prompt_count].action = AE_ACTION_TAB_PREVIOUS;
			last.prompts[last.prompt_count++].pair = 1;
		}
	}
	last.prompts[last.prompt_count].button = AE_BUTTON_B;
	last.prompts[last.prompt_count].label = "Back";
	last.prompts[last.prompt_count++].action = AE_ACTION_BACK;
	ae_widget_prompts(&density, x, center_y, last.prompts, last.prompt_count, "LAST GAME", right, -1, HIT_LAST_PROMPTS);
}

static struct ae_screen_class const last_class =
{
	.name = "pgcr_last", .leave = last_leave, .handle = last_handle, .draw = last_draw, .pointer = last_pointer,
};
