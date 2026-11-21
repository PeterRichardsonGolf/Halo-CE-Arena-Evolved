/* The SERVICE RECORD's detail window (see the header). Measures are design units of the 1920 x 1080 record frame. */
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "math/real_math.h"
#include "../src/ae_draw.h"
#include "../src/ae_layout.h"
#include "ae_medal_pack.h"
#include "ae_pgcr_draw.h"
#include "ae_pgcr_screen_rules.h"
#include "ae_record_medals_rules.h"
#include "ae_record_columns_rules.h"
#include "ae_record_pages.h"
#include "ae_screen_record.h"
#include "ae_screen_record_window.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_test_hooks.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	WINDOW_HIT = 0x7630,
	WINDOW_CHIP_HIT = 0x7631,
	WINDOW_COLUMN_HIT = 0x7632,
	WINDOW_KIND_HIT = 0x7633,
	LEFT = 96, RIGHT = 1824, PAD = 28,
	/* the band */
	EMBLEM = 84, NAME_X = 204, TABS_CY = 196, CHIP_H = 34,
	/* the pages' grid */
	TOP = 232, BOTTOM = 968, STRIP_CY = 252, MODES_TOP = 292,
	COLUMNS_MAX = 6
};

#define RING_OUT 6.0f

static struct
{
	int on_stack;
	struct ae_rw_source source;
	int page;
	/* the ringed column (an index into the page's columns), -1 for none; for MODES the kind it is on (AE_RC_RING_NOT_PLAYED on
	NOT PLAYED YET), kept over steps, and the first column of the run shown */
	int column, ring_kind, first;
	int columns;
	int mouse, chip_hover, column_hover, kind_hover;
	/* MODES' kind chips as drawn: each chip's kind (a click rings its column), and the kind last logged */
	int chip_kind[AE_RS_KINDS];
	int chip_count, logged_kind;
	struct ae_prompt prompts[5];
	short prompt_count;
} window;

/* the record as last filled (static: the view's weapons are pointed at by its input) */
static struct ae_rw_view view;

static struct ae_screen_class const window_class;

int ae_screen_record_window_open(short owner, struct ae_rw_source const *source, int page, int column)
{
	if (window.on_stack || !source || !source->fill)
		return 0;
	memset(&window, 0, sizeof(window));
	window.source = *source;
	window.page = page >= 0 && page < AE_RC_PAGES ? page : AE_RC_MODES;
	window.column = column;
	window.ring_kind = -1;
	window.chip_hover = -1;
	window.column_hover = -1;
	window.kind_hover = -1;
	window.logged_kind = -1;
	if (!ae_ui_push(&window_class, owner, &window))
		return 0;
	window.on_stack = 1;
	memset(&view, 0, sizeof(view));
	window.source.fill(&view);
	platform_log("ae record: window %s %s", view.name, ae_rc_page_name(window.page));
	/* (debug.capture_on_event's "record_window": the tests' frame of the window) */
	ae_test_capture("record_window", 500);
	return 1;
}

int ae_screen_record_window_open_kind(short owner, struct ae_rw_source const *source, int kind)
{
	if (!ae_screen_record_window_open(owner, source, AE_RC_MODES, -1))
		return 0;
	window.ring_kind = kind;
	return 1;
}

int ae_screen_record_window_ring_kind(int kind)
{
	if (!window.on_stack || window.page != AE_RC_MODES)
		return 0;
	window.ring_kind = kind;
	window.column = -1;
	return 1;
}

int ae_screen_record_window_is_open(void)
{
	return window.on_stack;
}

void ae_screen_record_window_remove(void)
{
	if (window.on_stack)
		ae_ui_remove(&window);
}

static void window_leave(struct ae_screen *screen)
{
	(void)screen;
	window.on_stack = 0;
}

static void go_to_page(int page)
{
	if (page == window.page || page < 0 || page >= AE_RC_PAGES)
		return;
	window.page = page;
	window.column = -1;
	window.ring_kind = -1;
	window.first = 0;
	window.logged_kind = -1;
	ae_sound_request(AE_SOUND_CURSOR, 0);
	platform_log("ae record: window page %s", ae_rc_page_name(page));
	ae_test_capture("record_window", 500);
}

static void step(int delta)
{
	if (!window.source.step || !window.source.step(delta))
		return;
	ae_sound_request(AE_SOUND_CURSOR, 0);
	memset(&view, 0, sizeof(view));
	window.source.fill(&view);
	platform_log("ae record: window %s %s", view.name, ae_rc_page_name(window.page));
	ae_test_capture("record_window", 500);
}

static void move_ring(int direction)
{
	int next = window.column < 0 ? 0 : window.column + direction;

	if (window.columns <= 0 || next < 0 || next >= window.columns || next == window.column)
		return;
	window.column = next;
	window.ring_kind = -1;
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

/* MODES: a kind chip or a column clicked rings that column (a kind, or -1 and the column's index) */
static void ring_mode(int kind, int column)
{
	if ((kind >= 0 && kind == window.ring_kind) || (kind < 0 && column == window.column))
		return;
	window.ring_kind = kind;
	window.column = column;
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static int window_handle(struct ae_screen *screen, struct ae_event const *event)
{
	(void)screen;
	window.mouse = 0;
	window.chip_hover = -1;
	window.column_hover = -1;
	window.kind_hover = -1;
	switch (event->action)
	{
	case AE_ACTION_TAB_NEXT:
		go_to_page(ae_rc_page_step(window.page, 1));
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
		go_to_page(ae_rc_page_step(window.page, -1));
		return 1;
	case AE_ACTION_PAGE_DOWN:
		step(1);
		return 1;
	case AE_ACTION_PAGE_UP:
		step(-1);
		return 1;
	case AE_ACTION_LEFT:
		move_ring(-1);
		return 1;
	case AE_ACTION_RIGHT:
		move_ring(1);
		return 1;
	case AE_ACTION_ACCEPT:
		if (window.page == AE_RC_MEDALS && window.source.open_medals)
		{
			ae_sound_request(AE_SOUND_FORWARD, 0);
			window.source.open_medals(window.column >= 0 ? 1 + window.column : AE_RM_ALL);
		}
		return 1;
	default:
		break;
	}
	return event->action == AE_ACTION_BACK ? 0 : 1;
}

static void window_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	int found;

	(void)screen;
	if (!pointer->touch && (pointer->moved || pointer->left_clicks || pointer->wheel_steps))
		window.mouse = 1;
	found = ae_hit_at(pointer->x, pointer->y, 0, &hit);
	if (pointer->moved)
	{
		window.chip_hover = found && hit.id == WINDOW_CHIP_HIT ? hit.index : -1;
		window.column_hover = found && hit.id == WINDOW_COLUMN_HIT ? hit.index : -1;
		window.kind_hover = found && hit.id == WINDOW_KIND_HIT ? hit.index : -1;
	}
	if (pointer->left_clicks && found && hit.id == WINDOW_CHIP_HIT)
	{
		go_to_page(hit.index);
		return;
	}
	if (pointer->left_clicks && found && window.page == AE_RC_MODES && hit.id == WINDOW_KIND_HIT && hit.index >= 0 &&
		hit.index < window.chip_count)
	{
		ring_mode(window.chip_kind[hit.index], -1);
		return;
	}
	if (pointer->left_clicks && found && window.page == AE_RC_MODES && hit.id == WINDOW_COLUMN_HIT)
	{
		ring_mode(-1, hit.index);
		return;
	}
	if (pointer->left_clicks && found && hit.id == WINDOW_COLUMN_HIT && window.page == AE_RC_MEDALS && window.source.open_medals)
	{
		ae_sound_request(AE_SOUND_FORWARD, 0);
		window.source.open_medals(1 + hit.index);
		return;
	}
	if (ae_prompts_pointer(window.prompts, window.prompt_count, pointer, WINDOW_HIT, &event))
		ae_ui_dispatch(&event);
}

/* ---------- drawing pieces */

static float tracked_w(struct frame_metrics const *f, float size, float tracking, char const *text)
{
	return ae_draw_text_tracked_width(AE_FONT_BODY, size * f->m, tracking, text) / f->m;
}

/* a drawn 4 u dot between two words, never U+00B7 */
static void dot(struct frame_metrics const *f, float x, float cy, unsigned int color)
{
	rect(f, x - 2.0f, cy - 2.0f, 4.0f, 4.0f, 2.0f, color);
}

/* a round chip: on (accent edge, .16 accent fill), off (muted .45 edge), disabled (the disabled chip's edge and words); its width */
enum { CHIP_OFF, CHIP_ON, CHIP_DISABLED };

static float chip_w(struct frame_metrics const *f, char const *word, char const *count)
{
	float w = 36.0f + tracked_w(f, 15, 0.10f, word);

	if (count && count[0])
		w += 10.0f + tracked_w(f, 15, 0.10f, count);
	return w;
}

static float chip(struct frame_metrics const *f, float x, float cy, char const *word, char const *count, int state, int hover,
	int hit_id, int index)
{
	float side = 18.0f, w = chip_w(f, word, count), top = cy - CHIP_H * 0.5f;
	if (state == CHIP_ON)
	{
		rect(f, x, top, w, CHIP_H, CHIP_H * 0.5f, with_alpha(AE_COLOR_ACCENT, 0.16f));
		ae_draw_outline(tx(f, x), ty(f, top), w * f->m, CHIP_H * f->m, CHIP_H * 0.5f * f->m, 2.0f * f->m, AE_COLOR_ACCENT);
	}
	else
	{
		if (hover)
			rect(f, x, top, w, CHIP_H, CHIP_H * 0.5f, AE_COLOR_HOVER);
		ae_draw_outline(tx(f, x), ty(f, top), w * f->m, CHIP_H * f->m, CHIP_H * 0.5f * f->m, (state == CHIP_DISABLED ? 1.5f :
			2.0f) * f->m, state == CHIP_DISABLED ? AE_COLOR_CHIP_DISABLED : with_alpha(AE_COLOR_MUTED, 0.45f));
	}
	{
		float word_w = put_tracked(f, AE_FONT_BODY, 15, 0.10f, x + side, cy, AE_ALIGN_LEFT, state == CHIP_ON ? AE_COLOR_TITLE :
			state == CHIP_DISABLED ? AE_COLOR_CHIP_DISABLED_TEXT : AE_COLOR_TEXT, word);

		if (count && count[0])
			put_tracked(f, AE_FONT_BODY, 15, 0.10f, x + side + word_w + 10.0f, cy, AE_ALIGN_LEFT, state == CHIP_ON ?
				AE_COLOR_TEXT : AE_COLOR_MUTED, count);
	}
	if (hit_id)
		ae_hit_add(tx(f, x), ty(f, top), w * f->m, CHIP_H * f->m, hit_id, AE_PART_CHIP, (short)index);
	return w;
}

/* a bar: the track, then the fill (at least its own height while above 0) */
static void bar(struct frame_metrics const *f, float x, float cy, float w, float fill, float h)
{
	rect(f, x, cy - h * 0.5f, w, h, h * 0.5f, AE_COLOR_TRACK);
	if (fill > 0.0f)
	{
		float length = w * (fill < 1.0f ? fill : 1.0f);

		rect(f, x, cy - h * 0.5f, length > h ? length : h, h, h * 0.5f, AE_COLOR_ACCENT);
	}
}

/* a medal's picture, or the ring in its family's colour with its dot */
static void medal_icon(struct frame_metrics const *f, int medal, float x, float y, float size)
{
	int image = ae_medal_pack_menu_image(medal);
	unsigned int color = ae_pgr_family_color(ae_pgr_medal_family(medal));
	float ring = size * 0.78f, inner = size * 0.22f, cx = x + size * 0.5f, cy = y + size * 0.5f;

	if (image)
	{
		ae_draw_image(image, tx(f, x), ty(f, y), size * f->m, size * f->m, 0xFFFFFFFFu);
		return;
	}
	ae_draw_outline(tx(f, cx - ring * 0.5f), ty(f, cy - ring * 0.5f), ring * f->m, ring * f->m, ring * 0.5f * f->m, 2.5f * f->m,
		color);
	rect(f, cx - inner * 0.5f, cy - inner * 0.5f, inner, inner, inner * 0.5f, color);
}

/* words wrapped to a width into lines (at most `most`): how many */
static int wrap(struct frame_metrics const *f, float size, float tracking, char const *text, float width, char lines[][AE_RC_NOTE],
	int most)
{
	int count = 0;
	char line[AE_RC_NOTE] = "", word[AE_RC_NOTE];

	while (text && *text && count < most)
	{
		int n = 0;
		char trial[AE_RC_NOTE * 2];

		while (*text == ' ')
			text++;
		while (text[n] && text[n] != ' ' && n + 1 < (int)sizeof(word))
			n++;
		if (!n)
			break;
		memcpy(word, text, (size_t)n);
		word[n] = 0;
		text += n;
		snprintf(trial, sizeof(trial), "%s%s%s", line, line[0] ? " " : "", word);
		if (line[0] && tracked_w(f, size, tracking, trial) > width)
		{
			snprintf(lines[count++], AE_RC_NOTE, "%s", line);
			snprintf(line, sizeof(line), "%s", word);
		}
		else
			snprintf(line, sizeof(line), "%s", trial);
	}
	if (line[0] && count < most)
		snprintf(lines[count++], AE_RC_NOTE, "%s", line);
	return count;
}

/* a column: name and tag, its games line and meter, three heroes, rows, bars, medals and the foot note */
void ae_rw_draw_column(struct frame_metrics const *f, float x, float y, float w, float h, struct ae_rc_column const *c)
{
	float left = x + PAD, right = x + w - PAD, top, limit, name_w, meter_w = 0.0f;
	char lines[3][AE_RC_NOTE], fitted[64], games[AE_RC_TEXT];
	int index, note_lines = 0;

	ae_rp_pane(f, x, y, w, h);
	name_w = put(f, AE_FONT_ROW, 26, left, y + 44.0f, AE_ALIGN_LEFT, AE_COLOR_TITLE, c->name);
	if (c->tag[0])
		draw_outlined_tag(f, left + name_w + 14.0f, y + 44.0f, c->tag, AE_COLOR_ACCENT);
	/* (a games line that would come within 8 u of the meter's word keeps its first part, "109 GAMES", and the rate; the
	measure holds the tracking after the last letter, 1.8 u) */
	snprintf(games, sizeof(games), "%s", c->games);
	if (c->meter && c->meter_word[0])
		meter_w = put(f, AE_FONT_ROW, 16, right, y + 78.0f, AE_ALIGN_RIGHT, AE_COLOR_TITLE, c->meter_word);
	if (meter_w > 0.0f && tracked_w(f, 15, 0.12f, games) - 1.8f + 8.0f + meter_w > right - left && strstr(games, "  "))
		*strstr(games, "  ") = 0;
	put_tracked(f, AE_FONT_BODY, 15, 0.12f, left, y + 78.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, games);
	top = y + 100.0f;
	if (c->meter)
	{
		bar(f, left, y + 104.0f, right - left, c->fill, 8.0f);
		rect(f, left, y + 126.0f, right - left, 1.0f, 0.0f, AE_COLOR_RULE);
		top = y + 132.0f;
	}
	for (index = 0; index < c->hero_count; index++)
	{
		struct ae_rc_hero const *hero = &c->hero[index];

		put_tracked(f, AE_FONT_BODY, 15, 0.14f, left, top + 22.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, hero->label);
		put(f, AE_FONT_ROW, 44, left, top + 58.0f, AE_ALIGN_LEFT, hero->dash ? AE_COLOR_MUTED : AE_COLOR_TITLE, hero->value);
		if (hero->side[0])
			put_tracked(f, AE_FONT_BODY, 15, 0.08f, right, top + 58.0f, AE_ALIGN_RIGHT, AE_COLOR_TEXT, hero->side);
		top += 84.0f;
	}
	if (c->row_count)
	{
		float cy = top + 30.0f;

		if (c->hero_count)
			rect(f, left, top + 2.0f, right - left, 1.0f, 0.0f, AE_COLOR_RULE);
		for (index = 0; index < c->row_count; index++, cy += 32.0f)
		{
			put_tracked(f, AE_FONT_BODY, 15, 0.10f, left, cy, AE_ALIGN_LEFT, AE_COLOR_TEXT, c->row[index].label);
			put(f, AE_FONT_ROW, 20, right, cy, AE_ALIGN_RIGHT, c->row[index].dash ? AE_COLOR_MUTED : AE_COLOR_TITLE,
				c->row[index].value);
		}
		top = cy - 32.0f + 14.0f;
	}
	for (index = 0; index < c->bar_count; index++)
	{
		struct ae_rc_bar const *b = &c->bar[index];
		float share_w;

		fit(f, AE_FONT_BODY, 15, b->label, right - left - 150.0f, fitted, sizeof(fitted));
		put_tracked(f, AE_FONT_BODY, 15, 0.10f, left, top + 22.0f, AE_ALIGN_LEFT, AE_COLOR_TEXT, fitted);
		share_w = put(f, AE_FONT_ROW, 18, right, top + 22.0f, AE_ALIGN_RIGHT, strcmp(b->share, AE_RV_DASH) ? AE_COLOR_TITLE :
			AE_COLOR_MUTED, b->share);
		if (b->count[0])
			put_tracked(f, AE_FONT_BODY, 15, 0.08f, right - share_w - 14.0f, top + 22.0f, AE_ALIGN_RIGHT, AE_COLOR_MUTED, b->count);
		bar(f, left, top + 48.0f, right - left, b->length, 6.0f);
		top += 66.0f;
	}
	if (c->note[0])
		note_lines = wrap(f, 14, 0.06f, c->note, right - left, lines, 3);
	limit = y + h - PAD - (float)note_lines * 22.0f;
	if (c->medals_head[0] && top + 34.0f + 40.0f <= limit)
	{
		float cy = top + 34.0f;

		put_tracked(f, AE_FONT_BODY, 15, 0.14f, left, cy, AE_ALIGN_LEFT, AE_COLOR_MUTED, c->medals_head);
		cy += 40.0f;
		if (!c->medal_count)
		{
			char empty[3][AE_RC_NOTE];
			int count = wrap(f, 16, 0.0f, c->medals_empty, right - left, empty, 3), line;

			for (line = 0; line < count; line++)
				put(f, AE_FONT_BODY, 16, left, cy - 4.0f + (float)line * 24.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, empty[line]);
		}
		for (index = 0; index < c->medal_count && cy + 16.0f <= limit; index++, cy += 46.0f)
		{
			/* (the last row that fits says how many more there are when the column holds more than it shows) */
			if (c->medals_earned > index + 1 && cy + 46.0f + 16.0f > limit)
			{
				char more[32];

				snprintf(more, sizeof(more), "+%d MORE", c->medals_earned - index);
				put_tracked(f, AE_FONT_BODY, 14, 0.14f, left, cy, AE_ALIGN_LEFT, AE_COLOR_MUTED, more);
				break;
			}
			char name[AE_PGR_NAME], count[24];

			medal_icon(f, c->medal[index], left, cy - 18.0f, 36.0f);
			ae_pgr_medal_name(c->medal[index], name, sizeof(name));
			ae_rs_number_text((long)c->medal_n[index], count, sizeof(count));
			fit(f, AE_FONT_BODY, 14, name, right - left - 48.0f - 60.0f, fitted, sizeof(fitted));
			put_tracked(f, AE_FONT_BODY, 14, 0.06f, left + 48.0f, cy, AE_ALIGN_LEFT, AE_COLOR_TEXT, fitted);
			put(f, AE_FONT_ROW, 18, right, cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, count);
		}
	}
	else if (!c->medals_head[0] && c->medals_empty[0] && !c->bar_count)
		put(f, AE_FONT_BODY, 16, left, top + 34.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, c->medals_empty);
	for (index = 0; index < note_lines; index++)
		put_tracked(f, AE_FONT_BODY, 14, 0.06f, left, y + h - PAD - (float)(note_lines - 1 - index) * 22.0f - 6.0f, AE_ALIGN_LEFT,
			AE_COLOR_MUTED, lines[index]);
}

/* the ring 6 u out of a column (pad and keys), the hover wash under the mouse; each column a hit. MODES' ring is its kind
chips' selection: it shows with the mouse too, and the wash marks another column a click would ring */
static void column_focus(struct frame_metrics const *f, int index, float x, float y, float w, float h)
{
	int modes = window.page == AE_RC_MODES;

	ae_hit_add(tx(f, x), ty(f, y), w * f->m, h * f->m, WINDOW_COLUMN_HIT, AE_PART_CARD, (short)index);
	if (window.mouse && window.column_hover == index && (window.page == AE_RC_MEDALS || (modes && window.column != index)))
		rect(f, x, y, w, h, 3.0f, AE_COLOR_HOVER);
	else if ((!window.mouse || modes) && window.column == index)
		ae_draw_outline(tx(f, x - RING_OUT), ty(f, y - RING_OUT), (w + 2.0f * RING_OUT) * f->m, (h + 2.0f * RING_OUT) * f->m,
			3.0f * f->m, 3.0f * f->m, AE_COLOR_ACCENT);
}

/* ---------- the band */

static void draw_band(struct frame_metrics const *f, struct ae_density const *density)
{
	struct ae_rc_head head;
	static char const *const labels[4] = { "GAMES", "WIN RATE", "K/D", "BEST SPREE" };
	char const *values[4];
	float w, x, name_w;
	char text[128];
	int index;

	ae_rc_head(&view.in, view.last.has ? &view.last : NULL, view.name, &head);
	w = put_tracked(f, AE_FONT_BODY, 16, 0.14f, LEFT, 56.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, "SERVICE RECORD");
	dot(f, LEFT + w + 14.0f, 56.0f, AE_COLOR_MUTED);
	put_tracked(f, AE_FONT_BODY, 16, 0.14f, LEFT + w + 28.0f, 56.0f, AE_ALIGN_LEFT, AE_COLOR_TEXT, ae_rc_page_name(window.page));
	/* the emblem: the record's initial on the neutral square (the file keeps no colour for a player or a bot) */
	rect(f, LEFT, 80.0f, EMBLEM, EMBLEM, 6.0f, AE_COLOR_ROW);
	ae_draw_outline(tx(f, LEFT), ty(f, 80.0f), EMBLEM * f->m, EMBLEM * f->m, 6.0f * f->m, 2.0f * f->m,
		with_alpha(AE_COLOR_MUTED, 0.45f));
	put(f, AE_FONT_ROW, 44, LEFT + EMBLEM * 0.5f, 122.0f, AE_ALIGN_CENTER, AE_COLOR_TEXT, head.initial);
	fit(f, AE_FONT_ROW, 44, view.name, 760.0f, text, sizeof(text));
	name_w = put(f, AE_FONT_ROW, 44, NAME_X, 106.0f, AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
	if (view.bot)
		draw_outlined_tag(f, NAME_X + name_w + 16.0f, 106.0f, "BOT", AE_COLOR_MUTED);
	else if (view.own)
		draw_you_tag(f, NAME_X + name_w + 16.0f, 106.0f, 0);
	else if (view.rival)
		draw_rival_tag(f, NAME_X + name_w + 16.0f, 106.0f, 0);
	/* the live subtitle: what the record covers, then the last game */
	w = put_tracked(f, AE_FONT_BODY, 16, 0.08f, NAME_X, 150.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, view.sub);
	if (head.last[0])
	{
		dot(f, NAME_X + w + 14.0f, 150.0f, AE_COLOR_MUTED);
		fit(f, AE_FONT_BODY, 16, head.last, 1000.0f - w, text, sizeof(text));
		put_tracked(f, AE_FONT_BODY, 16, 0.08f, NAME_X + w + 28.0f, 150.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
	}
	/* the headline numbers, right-aligned cells 48 apart */
	values[0] = head.games; values[1] = head.win_rate; values[2] = head.kd; values[3] = head.best_spree;
	for (index = 3, x = RIGHT; index >= 0; index--)
	{
		float vw = width_of(f, AE_FONT_ROW, 32, values[index]), lw = tracked_w(f, 14, 0.14f, labels[index]);

		put(f, AE_FONT_ROW, 32, x, 132.0f, AE_ALIGN_RIGHT, strcmp(values[index], AE_RV_DASH) ? AE_COLOR_TITLE : AE_COLOR_MUTED,
			values[index]);
		put_tracked(f, AE_FONT_BODY, 14, 0.14f, x, 92.0f, AE_ALIGN_RIGHT, AE_COLOR_MUTED, labels[index]);
		x -= (vw > lw ? vw : lw) + 48.0f;
	}
	/* the tabs: the LB mark, the page chips, the RB mark; the place in the list on the right */
	x = LEFT;
	x += ae_widget_button_mark(density, AE_BUTTON_LEFT_SHOULDER, tx(f, x), ty(f, TABS_CY), 26.0f * f->m, 0, 1) / f->m + 12.0f;
	for (index = 0; index < AE_RC_PAGES; index++)
		x += chip(f, x, TABS_CY, ae_rc_page_name(index), NULL, index == window.page ? CHIP_ON : CHIP_OFF,
			window.mouse && window.chip_hover == index, WINDOW_CHIP_HIT, index) + 12.0f;
	ae_widget_button_mark(density, AE_BUTTON_RIGHT_SHOULDER, tx(f, x), ty(f, TABS_CY), 26.0f * f->m, 0, 1);
	if (view.position[0])
		put_tracked(f, AE_FONT_BODY, 16, 0.10f, RIGHT, TABS_CY, AE_ALIGN_RIGHT, AE_COLOR_MUTED, view.position);
}

/* ---------- the pages */

/* the NOT PLAYED YET column: the kinds not played as disabled chips and the sentence; nw wide (the whole room with no column) */
static void draw_not_played(struct frame_metrics const *f, float x, float nw, float h, int alone, int const *not_played,
	int missing)
{
	char sentence[AE_RC_NOTE * 2], lines[4][AE_RC_NOTE];
	float left = x + PAD, cy;
	int line_count, line, index;

	ae_draw_outline(tx(f, x), ty(f, MODES_TOP), nw * f->m, h * f->m, 3.0f * f->m, 1.5f * f->m, AE_COLOR_CHIP_DISABLED);
	put_tracked(f, AE_FONT_BODY, 16, 0.14f, left, MODES_TOP + 44.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, "NOT PLAYED YET");
	cy = MODES_TOP + 96.0f;
	for (index = 0; index < missing; index++, cy += 50.0f)
		chip(f, left, cy, ae_rc_kind_word(not_played[index]), NULL, CHIP_DISABLED, 0, 0, 0);
	ae_rc_not_played_text(view.name, view.bot, view.own, sentence, sizeof(sentence));
	line_count = wrap(f, 16, 0.0f, sentence, (alone ? 640.0f : nw) - 2.0f * PAD, lines, 4);
	for (line = 0; line < line_count; line++, cy += 26.0f)
		put(f, AE_FONT_BODY, 16, left, cy + 6.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, lines[line]);
}

static void draw_modes(struct frame_metrics const *f)
{
	static struct ae_rc_column columns[AE_RC_MODE_COLUMNS];
	struct ae_rc_chip chips[AE_RS_KINDS];
	int not_played[AE_RC_OBJECTIVE_KINDS], missing = 0, count, chip_count, index, slots, shown, form, ring_kind;
	float x, w, nw, room, run_w = 0.0f, h = (float)(BOTTOM - MODES_TOP);
	char run[40];

	chip_count = ae_rc_chips(&view.in, chips);
	count = ae_rc_mode_columns(&view.in, columns, not_played, &missing);
	slots = count + (missing > 0 ? 1 : 0);
	window.columns = slots;
	/* the ring: its kind's column (the favourite kind's at open), kept as a kind over steps; the lit chip is its kind */
	window.column = ae_rc_mode_ring(columns, count, missing > 0, window.ring_kind, window.column, ae_rc_favourite_kind(&view.in));
	ring_kind = window.column < 0 ? -1 : window.column < count ? columns[window.column].kind : AE_RC_RING_NOT_PLAYED;
	ae_rc_chips_light(chips, chip_count, ring_kind);
	shown = ae_rc_mode_shown(count, missing > 0);
	window.first = ae_rc_mode_first(slots, shown, window.first, window.column);
	/* (a record with no kind played, as while a list empties, leaves the kind kept for the next record) */
	if (count > 0)
		window.ring_kind = ring_kind;
	if (count > 0 && window.ring_kind != window.logged_kind)
	{
		window.logged_kind = window.ring_kind;
		platform_log("ae record: window kind %s (column %d of %d, %d to %d shown)", window.ring_kind == AE_RC_RING_NOT_PLAYED ?
			"NOT PLAYED YET" : ae_sr_mode_name(window.ring_kind), window.column + 1, slots, window.first + 1, window.first + shown);
	}
	/* the kinds as chips: BY KIND, then each kind played; the words drop the rate, then the games, when the strip has no room
	(for the run's place on the right too: "COLUMNS 3-7 OF 7") */
	run[0] = 0;
	if (shown < slots)
	{
		snprintf(run, sizeof(run), "COLUMNS %d-%d OF %d", window.first + 1, window.first + shown, slots);
		run_w = tracked_w(f, 15, 0.14f, run) + 24.0f;
		put_tracked(f, AE_FONT_BODY, 15, 0.14f, RIGHT, STRIP_CY, AE_ALIGN_RIGHT, AE_COLOR_MUTED, run);
	}
	x = LEFT + put_tracked(f, AE_FONT_BODY, 15, 0.14f, LEFT, STRIP_CY, AE_ALIGN_LEFT, AE_COLOR_MUTED, "BY KIND") + 20.0f;
	for (form = 0; form < 2; form++)
	{
		for (index = 0, room = 0.0f; index < chip_count; index++)
			room += chip_w(f, chips[index].name, form ? chips[index].games : chips[index].count) + 12.0f;
		if (x + room - 12.0f <= RIGHT - run_w)
			break;
	}
	window.chip_count = chip_count;
	for (index = 0; index < chip_count; index++)
	{
		char const *words = form == 0 ? chips[index].count : form == 1 ? chips[index].games : NULL;

		window.chip_kind[index] = chips[index].kind;
		x += chip(f, x, STRIP_CY, chips[index].name, words, chips[index].on ? CHIP_ON : CHIP_OFF,
			window.mouse && window.kind_hover == index && !chips[index].on, WINDOW_KIND_HIT, index) + 12.0f;
	}
	if (!chip_count)
		put(f, AE_FONT_BODY, 16, x, STRIP_CY, AE_ALIGN_LEFT, AE_COLOR_MUTED, "No games kept yet.");
	/* the columns: every one while they fit (NOT PLAYED YET at its own width), else a run of five equal ones */
	if (shown == slots)
	{
		w = ae_rc_column_width(count, missing > 0);
		nw = count ? (float)AE_RC_NOT_PLAYED_W : (float)AE_RC_ROOM;
	}
	else
		w = nw = ae_rc_column_width(shown, 0);
	for (index = window.first, x = LEFT; index < window.first + shown && index < slots; index++)
	{
		if (index < count)
		{
			ae_rw_draw_column(f, x, MODES_TOP, w, h, &columns[index]);
			column_focus(f, index, x, MODES_TOP, w, h);
			x += w + AE_RC_GAP;
		}
		else
		{
			draw_not_played(f, x, nw, h, !count, not_played, missing);
			column_focus(f, index, x, MODES_TOP, nw, h);
		}
	}
}

static void draw_four(struct frame_metrics const *f, struct ae_rc_column const *columns, int count)
{
	float w = ((float)AE_RC_ROOM - (float)AE_RC_GAP * (float)(count - 1)) / (float)count, x = LEFT;
	int index;

	window.columns = count;
	if (window.column >= count)
		window.column = count - 1;
	for (index = 0; index < count; index++, x += w + AE_RC_GAP)
	{
		ae_rw_draw_column(f, x, TOP, w, (float)(BOTTOM - TOP), &columns[index]);
		column_focus(f, index, x, TOP, w, (float)(BOTTOM - TOP));
	}
}

static void draw_weapons_maps(struct frame_metrics const *f)
{
	static struct ae_rc_column weapons;
	struct ae_rc_map_row maps[AE_RC_MAP_ROWS];
	char head[24], fitted[64];
	float wx = LEFT, ww = 560.0f, mx = LEFT + 560.0f + AE_RC_GAP, mw = (float)AE_RC_ROOM - 560.0f - AE_RC_GAP, h = BOTTOM - TOP;
	float left = mx + PAD, right = mx + mw - PAD, cy;
	int count, index;
	/* (the maps' number columns: games, the win-rate bar and its word, K/D) */
	float const games_right = right - 560.0f, bar_left = right - 500.0f, rate_right = right - 140.0f;

	ae_rc_weapons_column(&view.in, &weapons);
	ae_rw_draw_column(f, wx, TOP, ww, h, &weapons);
	column_focus(f, 0, wx, TOP, ww, h);
	count = ae_rc_maps(&view.in, maps, head, sizeof(head));
	window.columns = 2;
	ae_rp_pane(f, mx, TOP, mw, h);
	put(f, AE_FONT_ROW, 26, left, TOP + 44.0f, AE_ALIGN_LEFT, AE_COLOR_TITLE, "MAPS");
	put_tracked(f, AE_FONT_BODY, 15, 0.12f, left, TOP + 78.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, head);
	put_tracked(f, AE_FONT_BODY, 14, 0.14f, games_right, TOP + 112.0f, AE_ALIGN_RIGHT, AE_COLOR_MUTED, "GAMES");
	put_tracked(f, AE_FONT_BODY, 14, 0.14f, bar_left, TOP + 112.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, "WIN RATE");
	put_tracked(f, AE_FONT_BODY, 14, 0.14f, right, TOP + 112.0f, AE_ALIGN_RIGHT, AE_COLOR_MUTED, "K/D");
	rect(f, left, TOP + 128.0f, right - left, 1.0f, 0.0f, AE_COLOR_RULE);
	for (index = 0, cy = TOP + 156.0f; index < count && cy < BOTTOM - PAD; index++, cy += 36.0f)
	{
		fit(f, AE_FONT_ROW, 20, maps[index].name, games_right - left - 90.0f, fitted, sizeof(fitted));
		put_tracked(f, AE_FONT_ROW, 20, 0.04f, left, cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, fitted);
		put(f, AE_FONT_ROW, 20, games_right, cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, maps[index].games);
		bar(f, bar_left, cy, rate_right - 80.0f - bar_left, maps[index].won, 6.0f);
		put(f, AE_FONT_ROW, 18, rate_right, cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, maps[index].win_rate);
		put(f, AE_FONT_ROW, 20, right, cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, maps[index].kd);
	}
	if (!count)
		put(f, AE_FONT_BODY, 16, left, TOP + 156.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, "No map kept yet.");
	column_focus(f, 1, mx, TOP, mw, h);
}

static void window_draw(struct ae_screen *screen)
{
	static struct ae_rc_column columns[AE_RC_COMBAT_COLUMNS];
	struct ae_frame frame;
	struct ae_density density;
	struct frame_metrics f;

	(void)screen;
	ae_record_frame_begin(&f, &frame, &density);
	memset(&view, 0, sizeof(view));
	if (!window.source.fill(&view))
		ae_rv_from_player(NULL, &view.in);
	draw_band(&f, &density);
	switch (window.page)
	{
	case AE_RC_COMBAT:
		draw_four(&f, columns, ae_rc_combat_columns(&view.in, columns));
		break;
	case AE_RC_WEAPONS_MAPS:
		draw_weapons_maps(&f);
		break;
	case AE_RC_MEDALS:
		draw_four(&f, columns, ae_rc_medal_columns(&view.in, columns));
		break;
	default:
		draw_modes(&f);
		break;
	}
	/* the foot: B back, LT / RT the next record of the list, LB / RB the page; A every medal on MEDALS */
	window.prompt_count = 0;
	memset(window.prompts, 0, sizeof(window.prompts));
	if (window.page == AE_RC_MEDALS && window.source.open_medals)
	{
		window.prompts[window.prompt_count].button = AE_BUTTON_A;
		window.prompts[window.prompt_count].label = window.column >= 0 ? "These medals" : "All medals";
		window.prompts[window.prompt_count++].action = AE_ACTION_ACCEPT;
	}
	window.prompts[window.prompt_count].button = AE_BUTTON_B;
	window.prompts[window.prompt_count].label = "Back";
	window.prompts[window.prompt_count++].action = AE_ACTION_BACK;
	if (window.source.can_step && window.source.can_step())
	{
		window.prompts[window.prompt_count].button = AE_BUTTON_LEFT_TRIGGER;
		window.prompts[window.prompt_count].label = view.bot ? "Bot" : "Player";
		window.prompts[window.prompt_count].action = AE_ACTION_PAGE_UP;
		window.prompts[window.prompt_count++].pair = 1;
	}
	window.prompts[window.prompt_count].button = AE_BUTTON_LEFT_SHOULDER;
	window.prompts[window.prompt_count].label = "Page";
	window.prompts[window.prompt_count].action = AE_ACTION_TAB_PREVIOUS;
	window.prompts[window.prompt_count++].pair = 1;
	ae_widget_footer_band(&density, frame.rect.y + 1080.0f * f.m);
	ae_widget_prompts(&density, frame.rect.x + LEFT * f.m, frame.rect.y + 1020.0f * f.m, window.prompts, window.prompt_count,
		"THIS PC'S VIEW", frame.rect.x + RIGHT * f.m, -1, WINDOW_HIT);
}

static struct ae_screen_class const window_class =
{
	.name = "record_window", .leave = window_leave, .handle = window_handle, .draw = window_draw, .pointer = window_pointer,
};
