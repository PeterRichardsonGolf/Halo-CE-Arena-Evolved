/*
AE_SCREEN_RECORD_MEDALS.C

SERVICE RECORD's MEDALS page (ae_screen_record_medals.h). Its measures are the record page's 1920 x 1080 design units; what
shows and where the cursor goes is ae_record_medals_rules.c's.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "math/real_math.h"
#include "../src/ae_draw.h"
#include "ae_medal_pack.h"
#include "ae_pgcr_draw.h"
#include "ae_pgcr_screen_rules.h"
#include "ae_record_medals_rules.h"
#include "ae_screen_record.h"
#include "ae_screen_record_medals.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_test_hooks.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

#define PANE_FILL 0x10161CF2u

enum
{
	MEDALS_HIT = 0x7640,
	PROMPTS_HIT = 0x7641,
	LEFT = 96, TOP = 232, PANE_W = 1728, PANE_H = 736, PANE_PAD = 28,
	INNER_LEFT = LEFT + PANE_PAD, INNER_RIGHT = LEFT + PANE_W - PANE_PAD,
	CHIPS_Y = 300, CHIP_H = 34, CHIP_GAP = 10, CHIP_SIDE = 16, CHIP_DOT = 8,
	GRID_Y = 354, CELL_W = 398, CELL_H = 52, GAP_X = 16, GAP_Y = 8,
	GRID_RIGHT = INNER_LEFT + AE_RM_COLUMNS * CELL_W + (AE_RM_COLUMNS - 1) * GAP_X,
	GRID_BOTTOM = GRID_Y + AE_RM_ROWS * CELL_H + (AE_RM_ROWS - 1) * GAP_Y,
	STRIP_W = 24, TRACK_PAD = 6, TRACK_MOUSE = 10, THUMB_MINIMUM = 40,
	MORE_CY = GRID_BOTTOM + 20, RULE_Y = 878, DETAIL_CY = 900, DETAIL_LINE_CY = 926,
	/* the hit index of the sort word */
	SORT_INDEX = 0
};

/* a kind not earned yet: its content at this much */
#define DIM_ALPHA AE_DISABLED_ROW_ALPHA

static struct
{
	int on_stack;
	/* whether the cursor, category, sort and scroll below are the player's (kept while the record page stays) */
	int kept;
	struct ae_service_record const *record;
	char name[AE_SR_NAME];
	char viewer[AE_SR_NAME];
	int sort, category, cursor, first;
	/* the cells of the category in the sort, as last built */
	int ids[AE_MEDAL_COUNT];
	int count;
	/* the pointer: its look (after a mouse move, until pad or key input), the cell under it (-1), whether the cursor was put
	by a click (the mouse's white cell), the thumb being dragged and where it was grabbed; the chip and sort word under it */
	int mouse, hover, clicked, dragging, chip_hover, sort_hover;
	float drag_grab;
	/* (as last drawn, layout units: the track and the thumb, and the grid's rectangle for the wheel) */
	float track_top, track_height, thumb_height;
	struct ae_rect grid;
	struct ae_prompt prompts[3];
	short prompt_count;
} page;

static struct ae_screen_class const page_class;

static unsigned long const *counts_of(void)
{
	static unsigned long const none[AE_MEDAL_COUNT];
	char key[AE_SR_KEY];
	struct ae_sr_player const *player;

	ae_sr_key(page.name, key, sizeof(key));
	player = page.record ? ae_sr_find(page.record, key) : NULL;
	return player ? player->medals : none;
}

static struct ae_sr_player const *player_of(void)
{
	char key[AE_SR_KEY];

	ae_sr_key(page.name, key, sizeof(key));
	return page.record ? ae_sr_find(page.record, key) : NULL;
}

/* the cells again (the category or the sort changed, or the record); the window held to its ends, but not moved to the
cursor: the wheel and the thumb scroll it away from the cursor */
static void build(void)
{
	page.count = ae_rm_list(counts_of(), page.sort, page.category, page.ids);
	if (page.cursor >= page.count)
		page.cursor = page.count > 0 ? page.count - 1 : 0;
	page.first = ae_rm_scroll(page.count, page.first, 0);
}

int ae_screen_record_medals_open(short owner, struct ae_service_record const *record, char const *name, char const *viewer)
{
	unsigned long const *counts;

	if (page.on_stack || !record || !name || !name[0])
		return 0;
	if (!page.kept || page.record != record || strcmp(page.name, name))
	{
		memset(&page, 0, sizeof(page));
		page.sort = AE_RM_SORT_RAREST;
		page.category = AE_RM_ALL;
		page.kept = 1;
	}
	page.record = record;
	snprintf(page.name, sizeof(page.name), "%s", name);
	snprintf(page.viewer, sizeof(page.viewer), "%s", viewer ? viewer : "");
	page.mouse = 0;
	page.hover = page.chip_hover = page.sort_hover = -1;
	page.clicked = page.dragging = 0;
	build();
	page.first = ae_rm_keep_in_view(page.count, page.cursor, page.first);
	if (!ae_ui_push(&page_class, owner, &page))
		return 0;
	page.on_stack = 1;
	counts = counts_of();
	platform_log("ae record: medals page (%d of %d kinds)", ae_rm_earned(counts, AE_RM_ALL), ae_rm_kinds(AE_RM_ALL));
	/* (debug.capture_on_event's "record_medals": the tests' frame of the page) */
	ae_test_capture("record_medals", 500);
	return 1;
}

void ae_screen_record_medals_forget(void)
{
	if (!page.on_stack)
		page.kept = 0;
}

void ae_screen_record_medals_remove(void)
{
	if (page.on_stack)
		ae_ui_remove(&page);
	page.kept = 0;
}

static void page_leave(struct ae_screen *screen)
{
	(void)screen;
	page.on_stack = 0;
	page.dragging = 0;
	platform_log("ae record: medals page closed");
}

/* ---------- input */

/* (the window back to the cursor even when it stays: the wheel may have scrolled it away) */
static void cursor_to(int cursor, int repeat)
{
	int moved = cursor != page.cursor;

	page.cursor = cursor;
	page.first = ae_rm_keep_in_view(page.count, page.cursor, page.first);
	if (moved)
		ae_sound_request(AE_SOUND_CURSOR, repeat);
}

static void category_to(int category)
{
	if (category == page.category)
		return;
	page.category = category;
	page.cursor = 0;
	page.first = 0;
	build();
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

/* the other sort, the cursor kept on its medal and in view */
static void sort_switch(void)
{
	int medal = page.count > 0 ? page.ids[page.cursor] : -1, at;

	page.sort = page.sort == AE_RM_SORT_RAREST ? AE_RM_SORT_MOST : AE_RM_SORT_RAREST;
	build();
	at = ae_rm_find(page.ids, page.count, medal);
	page.cursor = at >= 0 ? at : 0;
	page.first = ae_rm_keep_in_view(page.count, page.cursor, page.first);
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static int page_handle(struct ae_screen *screen, struct ae_event const *event)
{
	(void)screen;
	/* (pad or key input: the cursor's look again, no hover) */
	page.mouse = 0;
	page.hover = page.chip_hover = page.sort_hover = -1;
	page.clicked = 0;
	build();
	switch (event->action)
	{
	case AE_ACTION_LEFT: cursor_to(ae_rm_step(page.count, page.cursor, -1, 0), event->repeat); return 1;
	case AE_ACTION_RIGHT: cursor_to(ae_rm_step(page.count, page.cursor, 1, 0), event->repeat); return 1;
	case AE_ACTION_UP: cursor_to(ae_rm_step(page.count, page.cursor, 0, -1), event->repeat); return 1;
	case AE_ACTION_DOWN: cursor_to(ae_rm_step(page.count, page.cursor, 0, 1), event->repeat); return 1;
	case AE_ACTION_PAGE_UP: cursor_to(ae_rm_page(page.count, page.cursor, -1), event->repeat); return 1;
	case AE_ACTION_PAGE_DOWN: cursor_to(ae_rm_page(page.count, page.cursor, 1), event->repeat); return 1;
	case AE_ACTION_HOME: cursor_to(0, 0); return 1;
	case AE_ACTION_END: cursor_to(page.count > 0 ? page.count - 1 : 0, 0); return 1;
	case AE_ACTION_TAB_PREVIOUS: category_to(ae_rm_category_step(page.category, -1)); return 1;
	case AE_ACTION_TAB_NEXT: category_to(ae_rm_category_step(page.category, 1)); return 1;
	case AE_ACTION_X: sort_switch(); return 1;
	case AE_ACTION_BACK: return 0;
	default: return 1;
	}
}

static void page_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	int found;

	(void)screen;
	build();
	if (!pointer->touch && (pointer->moved || pointer->left_clicks || pointer->wheel_steps))
		page.mouse = 1;
	/* a thumb being dragged: the window follows it until the button is let go of */
	if (page.dragging)
	{
		if (!pointer->left_held)
			page.dragging = 0;
		else if (pointer->moved)
			page.first = ae_scrollbar_first((short)ae_rm_rows(page.count), AE_RM_ROWS, page.track_height,
				page.thumb_height, pointer->y - page.drag_grab - page.track_top);
		return;
	}
	if (pointer->wheel_steps && pointer->x >= page.grid.x && pointer->y >= page.grid.y &&
		pointer->x < page.grid.x + page.grid.width && pointer->y < page.grid.y + page.grid.height)
		page.first = ae_rm_scroll(page.count, page.first, -pointer->wheel_steps);
	found = ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == MEDALS_HIT;
	/* (on a move only, as the list widget does: after a wheel step the hits are last frame's, from before the scroll) */
	if (pointer->moved)
	{
		page.hover = found && hit.part == AE_PART_ITEM && hit.index >= 0 && hit.index < page.count ? hit.index : -1;
		page.chip_hover = found && hit.part == AE_PART_CHIP ? hit.index : -1;
		page.sort_hover = found && hit.part == AE_PART_VALUE ? SORT_INDEX : -1;
	}
	if (pointer->left_clicks && found)
	{
		switch (hit.part)
		{
		case AE_PART_ITEM:
			if (hit.index >= 0 && hit.index < page.count)
			{
				page.clicked = 1;
				cursor_to(hit.index, 0);
			}
			return;
		case AE_PART_CHIP:
			category_to(hit.index);
			return;
		case AE_PART_VALUE:
			sort_switch();
			return;
		case AE_PART_THUMB:
			page.dragging = 1;
			page.drag_grab = pointer->y - hit.rect.y;
			return;
		case AE_PART_TRACK:
		{
			float thumb_top = page.track_top, room = page.track_height - page.thumb_height;
			int last = ae_rm_last_first(page.count);

			if (last > 0)
				thumb_top += room * (float)page.first / (float)last;
			page.first = ae_rm_scroll(page.count, page.first, pointer->y < thumb_top ? -AE_RM_PAGE_ROWS : AE_RM_PAGE_ROWS);
			return;
		}
		default:
			break;
		}
	}
	if (ae_prompts_pointer(page.prompts, page.prompt_count, pointer, PROMPTS_HIT, &event))
		ae_ui_dispatch(&event);
}

/* ---------- drawing */

static unsigned int darkened(unsigned int rgba)
{
	unsigned int r = ((rgba >> 24) & 0xFF) * 50 / 100, g = ((rgba >> 16) & 0xFF) * 50 / 100, b = ((rgba >> 8) & 0xFF) * 50 / 100;

	return (r << 24) | (g << 16) | (b << 8) | 0xFF;
}

static float tracked_width(struct frame_metrics const *f, int font, float size, float tracking, char const *text)
{
	return ae_draw_text_tracked_width(font, size * f->m, tracking, text) / f->m;
}

static void icon(struct frame_metrics const *f, int medal, float x, float y, float size, float alpha)
{
	int image = ae_medal_pack_menu_image(medal);

	if (image)
		ae_draw_image(image, tx(f, x), ty(f, y), size * f->m, size * f->m, with_alpha(0xFFFFFFFFu, alpha));
}

/* the CATEGORY chips: round, the current one an accent fill at .16 with a 2 u accent edge (never white: white is the
cursor), the others a muted edge; a dot in the category's colour before its word (none on ALL), then its kinds earned */
static void draw_chips(struct frame_metrics const *f, unsigned long const *counts)
{
	float x = INNER_LEFT, cy = CHIPS_Y + CHIP_H * 0.5f;
	int category;

	for (category = 0; category < AE_RM_CATEGORIES; category++)
	{
		char text[32];
		int current = category == page.category;
		float w, inner = CHIP_SIDE;

		snprintf(text, sizeof(text), "%s  %d", ae_rm_category_name(category), ae_rm_earned(counts, category));
		w = CHIP_SIDE * 2.0f + tracked_width(f, AE_FONT_BODY, 15, 0.10f, text) + (category ? CHIP_DOT + 8.0f : 0.0f);
		if (current)
		{
			rect(f, x, CHIPS_Y, w, CHIP_H, CHIP_H * 0.5f, with_alpha(AE_COLOR_ACCENT, 0.16f));
			ae_draw_outline(tx(f, x), ty(f, CHIPS_Y), w * f->m, CHIP_H * f->m, CHIP_H * 0.5f * f->m, 2.0f * f->m,
				AE_COLOR_ACCENT);
		}
		else
		{
			if (page.mouse && page.chip_hover == category)
				rect(f, x, CHIPS_Y, w, CHIP_H, CHIP_H * 0.5f, AE_COLOR_HOVER);
			ae_draw_outline(tx(f, x), ty(f, CHIPS_Y), w * f->m, CHIP_H * f->m, CHIP_H * 0.5f * f->m, 2.0f * f->m,
				with_alpha(AE_COLOR_MUTED, 0.45f));
		}
		if (category)
		{
			rect(f, x + CHIP_SIDE, cy - CHIP_DOT * 0.5f, CHIP_DOT, CHIP_DOT, CHIP_DOT * 0.5f,
				ae_pgr_family_color((enum ae_pgr_family)(category - 1)));
			inner += CHIP_DOT + 8.0f;
		}
		put_tracked(f, AE_FONT_BODY, 15, 0.10f, x + inner, cy, AE_ALIGN_LEFT, current ? AE_COLOR_TITLE : AE_COLOR_TEXT, text);
		ae_hit_add(tx(f, x), ty(f, CHIPS_Y), w * f->m, CHIP_H * f->m, MEDALS_HIT, AE_PART_CHIP, (short)category);
		x += w + CHIP_GAP;
	}
}

static void draw_sort_word(struct frame_metrics const *f)
{
	char const *word = page.sort == AE_RM_SORT_RAREST ? "RAREST FIRST" : "MOST EARNED";
	float cy = CHIPS_Y + CHIP_H * 0.5f;
	float w = put_tracked(f, AE_FONT_BODY, 15, 0.14f, INNER_RIGHT, cy, AE_ALIGN_RIGHT,
		page.mouse && page.sort_hover == SORT_INDEX ? AE_COLOR_TITLE : AE_COLOR_MUTED, word);

	ae_hit_add(tx(f, INNER_RIGHT - w - 8.0f), ty(f, CHIPS_Y), (w + 8.0f) * f->m, CHIP_H * f->m, MEDALS_HIT, AE_PART_VALUE,
		SORT_INDEX);
}

static void draw_cell(struct frame_metrics const *f, int index, float x, float y, unsigned long const *counts)
{
	int medal = page.ids[index], earned = counts[medal] != 0, image = ae_medal_pack_menu_image(medal);
	/* (the cursor's white cell: with a pad or keys always, under the mouse only where it clicked) */
	int cursor = index == page.cursor && (!page.mouse || page.clicked);
	float alpha = earned ? 1.0f : DIM_ALPHA, cy = y + CELL_H * 0.5f, name_x = x + 62.0f, count_w;
	unsigned int family = ae_pgr_family_color(ae_pgr_medal_family(medal)), ink, count_ink;
	char name[AE_PGR_NAME], fitted[AE_PGR_NAME + 8], count[24];

	if (cursor)
	{
		rect(f, x, y, CELL_W, CELL_H, 2.0f, AE_COLOR_SELECTION);
		ink = earned ? AE_COLOR_SELECTION_TEXT : AE_COLOR_SELECTION_MUTED;
		count_ink = ink;
	}
	else
	{
		/* (a kind not earned: the row's fill faint, its name and its 0 dimmed) */
		rect(f, x, y, CELL_W, CELL_H, 2.0f, earned ? AE_COLOR_ROW : with_alpha(AE_COLOR_ROW, 0.32f));
		if (page.mouse && page.hover == index)
			rect(f, x, y, CELL_W, CELL_H, 2.0f, AE_COLOR_HOVER);
		ink = earned ? AE_COLOR_TITLE : AE_COLOR_DISABLED_TEXT;
		count_ink = earned ? AE_COLOR_TITLE : with_alpha(AE_COLOR_MUTED, 0.55f);
	}
	if (image)
		icon(f, medal, x + 10.0f, y + 6.0f, 40.0f, cursor ? (earned ? 1.0f : 0.5f) : alpha);
	else if (cursor)
	{
		/* (no picture: the name in its family's colour, darkened on the white cell (a kind not earned keeps the darkened
		ink whole: its 0 and the strip say it), kept at +62 so the column stays) */
		ink = darkened(family);
	}
	else
		ink = earned ? family : with_alpha(family, 0.45f);
	ae_rm_count_text(counts[medal], count, sizeof(count));
	count_w = put(f, AE_FONT_ROW, 24, x + CELL_W - 16.0f, cy, AE_ALIGN_RIGHT, count_ink, count);
	ae_pgr_medal_name(medal, name, sizeof(name));
	fit(f, AE_FONT_BODY, 17, name, CELL_W - 62.0f - 16.0f - count_w - 14.0f, fitted, sizeof(fitted));
	put_tracked(f, AE_FONT_BODY, 17, 0.06f, name_x, cy, AE_ALIGN_LEFT, ink, fitted);
	ae_hit_add(tx(f, x), ty(f, y), CELL_W * f->m, CELL_H * f->m, MEDALS_HIT, AE_PART_ITEM, (short)index);
}

static void draw_scrollbar(struct frame_metrics const *f)
{
	short rows = (short)ae_rm_rows(page.count);
	float track_w = page.mouse ? TRACK_MOUSE : TRACK_PAD, track_x, height = GRID_BOTTOM - GRID_Y, thumb_y, thumb_h;
	unsigned int thumb_color = page.dragging ? AE_COLOR_RING : page.mouse ? AE_COLOR_THUMB_MOUSE : AE_COLOR_MUTED;

	page.track_height = page.thumb_height = 0.0f;
	if (rows <= AE_RM_ROWS)
		return;
	track_x = INNER_RIGHT - STRIP_W * 0.5f - track_w * 0.5f;
	ae_scrollbar_thumb(rows, AE_RM_ROWS, (float)page.first, height, THUMB_MINIMUM, &thumb_y, &thumb_h);
	rect(f, track_x, GRID_Y, track_w, height, track_w * 0.5f, AE_COLOR_TRACK);
	rect(f, track_x, GRID_Y + thumb_y, track_w, thumb_h, track_w * 0.5f, thumb_color);
	ae_hit_add(tx(f, INNER_RIGHT - STRIP_W), ty(f, GRID_Y), STRIP_W * f->m, height * f->m, MEDALS_HIT, AE_PART_TRACK, -1);
	ae_hit_add(tx(f, INNER_RIGHT - STRIP_W), ty(f, GRID_Y + thumb_y), STRIP_W * f->m, thumb_h * f->m, MEDALS_HIT,
		AE_PART_THUMB, -1);
	/* (for the pointer: layout units, the whole frame's) */
	page.track_top = ty(f, GRID_Y);
	page.track_height = height * f->m;
	page.thumb_height = thumb_h * f->m;
}

/* the strip under the grid: the focused medal (the hovered one under the mouse), its category, its line, its count and
points; a kind not earned yet says so */
static void draw_detail(struct frame_metrics const *f, unsigned long const *counts)
{
	int shown = page.mouse && page.hover >= 0 ? page.hover : page.cursor, medal, earned, image;
	enum ae_pgr_family family;
	char name[AE_PGR_NAME], line[AE_MEDAL_DESC_SIZE], points[32], text[64], count[24];
	float name_x = INNER_LEFT + 56.0f, w;

	rect(f, INNER_LEFT, RULE_Y, INNER_RIGHT - INNER_LEFT, 1.0f, 0.0f, AE_COLOR_RULE);
	if (shown < 0 || shown >= page.count)
		return;
	medal = page.ids[shown];
	earned = counts[medal] != 0;
	family = ae_pgr_medal_family(medal);
	image = ae_medal_pack_menu_image(medal);
	if (image)
		icon(f, medal, INNER_LEFT, RULE_Y + 14.0f, 40.0f, earned ? 1.0f : DIM_ALPHA);
	ae_pgr_medal_name(medal, name, sizeof(name));
	w = put_tracked(f, AE_FONT_ROW, 20, 0.04f, name_x, DETAIL_CY, AE_ALIGN_LEFT,
		image ? AE_COLOR_TITLE : ae_pgr_family_color(family), name);
	put_tracked(f, AE_FONT_BODY, 13, 0.14f, name_x + w + 14.0f, DETAIL_CY, AE_ALIGN_LEFT, ae_pgr_family_color(family),
		ae_rm_category_name(1 + (int)family));
	ae_rm_points_text(ae_pgr_medal_points(medal), points, sizeof(points));
	/* (the career list's line where the install has one; else the points alone, and the right side says only earned) */
	if (ae_medal_pack_desc(medal, line, sizeof(line)))
	{
		char fitted[AE_MEDAL_DESC_SIZE + 8];

		fit(f, AE_FONT_BODY, 16, line, 1100.0f, fitted, sizeof(fitted));
		put(f, AE_FONT_BODY, 16, name_x, DETAIL_LINE_CY, AE_ALIGN_LEFT, AE_COLOR_MUTED, fitted);
		snprintf(text, sizeof(text), "%s, %s", earned ? "EARNED" : "NOT EARNED YET", points);
	}
	else
	{
		put(f, AE_FONT_BODY, 16, name_x, DETAIL_LINE_CY, AE_ALIGN_LEFT, AE_COLOR_MUTED, points);
		snprintf(text, sizeof(text), "%s", earned ? "EARNED" : "NOT EARNED YET");
	}
	ae_rm_count_text(counts[medal], count, sizeof(count));
	put(f, AE_FONT_ROW, 24, INNER_RIGHT, DETAIL_CY, AE_ALIGN_RIGHT, earned ? AE_COLOR_TITLE : AE_COLOR_MUTED, count);
	put_tracked(f, AE_FONT_BODY, 13, 0.14f, INNER_RIGHT, DETAIL_LINE_CY, AE_ALIGN_RIGHT, AE_COLOR_MUTED, text);
}

static void page_draw(struct ae_screen *screen)
{
	struct ae_frame frame;
	struct ae_density density;
	struct frame_metrics f;
	struct ae_sr_player const *player = player_of();
	unsigned long const *counts = counts_of();
	char text[96], total[24], range[32];
	int row, column, more;

	(void)screen;
	build();
	ae_record_frame_begin(&f, &frame, &density);
	ae_rm_count_text(ae_rm_total(counts), total, sizeof(total));
	snprintf(text, sizeof(text), "%s MEDAL%s IN %ld GAME%s ON THIS PC", total, ae_rm_total(counts) == 1 ? "" : "S",
		player ? player->total.games : 0L, player && player->total.games == 1 ? "" : "S");
	ae_record_head(&f, page.record, page.name, page.viewer, "MEDALS", text);

	/* the pane: its head (the kinds earned of all of them, the cells shown), the chips and the sort */
	rect(&f, LEFT, TOP, PANE_W, PANE_H, 3.0f, PANE_FILL);
	rect(&f, LEFT, TOP, PANE_W, 4.0f, 0.0f, AE_COLOR_ACCENT);
	{
		float w = put_tracked(&f, AE_FONT_BODY, 16, 0.14f, INNER_LEFT, TOP + 40.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, "MEDALS");

		snprintf(text, sizeof(text), "%d OF %d KINDS", ae_rm_earned(counts, AE_RM_ALL), ae_rm_kinds(AE_RM_ALL));
		put_tracked(&f, AE_FONT_BODY, 16, 0.14f, INNER_LEFT + w + 14.0f, TOP + 40.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
	}
	ae_rm_range_text(page.count, page.first, range, sizeof(range));
	put_tracked(&f, AE_FONT_BODY, 16, 0.14f, INNER_RIGHT, TOP + 40.0f, AE_ALIGN_RIGHT, AE_COLOR_MUTED, range);
	draw_chips(&f, counts);
	draw_sort_word(&f);

	/* the grid: eight rows from the first, the last under the fade while more follow */
	for (row = 0; row < AE_RM_ROWS; row++)
	{
		for (column = 0; column < AE_RM_COLUMNS; column++)
		{
			int index = (page.first + row) * AE_RM_COLUMNS + column;

			if (index >= page.count)
				break;
			draw_cell(&f, index, INNER_LEFT + column * (CELL_W + GAP_X), GRID_Y + row * (CELL_H + GAP_Y), counts);
		}
	}
	if (ae_rm_faded(page.count, page.first))
		ae_draw_gradient(tx(&f, INNER_LEFT), ty(&f, GRID_BOTTOM - CELL_H), (GRID_RIGHT - INNER_LEFT) * f.m, CELL_H * f.m, 0.0f,
			AE_COLOR_FADE & 0xFFFFFF00u, AE_COLOR_FADE);
	page.grid.x = tx(&f, INNER_LEFT);
	page.grid.y = ty(&f, GRID_Y);
	page.grid.width = (INNER_RIGHT - INNER_LEFT) * f.m;
	page.grid.height = (GRID_BOTTOM - GRID_Y) * f.m;
	draw_scrollbar(&f);
	more = ae_rm_more(page.count, page.first);
	if (more > 0)
	{
		snprintf(text, sizeof(text), "%d more", more);
		put(&f, AE_FONT_BODY, 16, (INNER_LEFT + GRID_RIGHT) * 0.5f, MORE_CY, AE_ALIGN_CENTER, AE_COLOR_MUTED, text);
	}
	draw_detail(&f, counts);

	/* the foot */
	page.prompt_count = 0;
	memset(page.prompts, 0, sizeof(page.prompts));
	page.prompts[page.prompt_count].button = AE_BUTTON_LEFT_SHOULDER;
	page.prompts[page.prompt_count].label = "Category";
	page.prompts[page.prompt_count].action = AE_ACTION_TAB_PREVIOUS;
	page.prompts[page.prompt_count++].pair = 1;
	page.prompts[page.prompt_count].button = AE_BUTTON_X;
	page.prompts[page.prompt_count].label = "Sort";
	page.prompts[page.prompt_count++].action = AE_ACTION_X;
	page.prompts[page.prompt_count].button = AE_BUTTON_B;
	page.prompts[page.prompt_count].label = "Back";
	page.prompts[page.prompt_count++].action = AE_ACTION_BACK;
	ae_widget_prompts(&density, frame.rect.x + LEFT * f.m, frame.rect.y + 1020.0f * f.m, page.prompts, page.prompt_count,
		"THIS PC'S VIEW", frame.rect.x + (LEFT + PANE_W) * f.m, -1, PROMPTS_HIT);
}

static struct ae_screen_class const page_class =
{
	.name = "service_record_medals", .leave = page_leave, .handle = page_handle, .draw = page_draw, .pointer = page_pointer,
};
