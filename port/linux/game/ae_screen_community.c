/*
AE_SCREEN_COMMUNITY.C

AE's COMMUNITY SERVICES page (ae_screen_community.h): the title and its crumb, then on the left (the rows' width) what
each service does in plain words, the LIST GAMES I HOST row under SERVER BROWSER, GAME STATS, and DECLINE / ACCEPT side
by side as the primary action's look (the selection bar when focused, a menu blue frame at rest); on the right, in the
panel's place, a card: who runs the services (the operator and its address), what works either way, where the answer
is changed. The right column is information only: nothing in it takes the focus.

- Focus: DECLINE first (an opt-in is never answered by a stray A). Up / down between the row and the buttons, left /
  right between the buttons or (on the row) OFF / ON.
- A on the row turns it OFF / ON; A on DECLINE or ACCEPT answers. B, BACK and START are DECLINE. A press in the first
  half second is not taken (the press that closed the screen before it).
- The answer is written at once (ae_community_write_answer: ACCEPT turns the game list and the stats on and lists the
  games the player hosts as the row says; DECLINE turns everything off); the page closes and AE's main menu comes.
- Sizes are the rows' (spec units); when the page does not fit its room (UI SCALE 130 %) the content, not the title or
  the footer, is drawn a little smaller so every line stays on screen.

Its log: "ae community: open", "ae community: answered <accept|decline> (list games I host: on|off)", "closed".
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_community.h"
#include "../src/ae_draw.h"
#include "ae_hooks.h"
#include "ae_motion.h"
#include "ae_screen_community.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	COMMUNITY_HIT = 0x7660,
	HIT_ROWS = COMMUNITY_HIT + 1, HIT_PROMPTS
};

/* the focusable things, top to bottom, left to right */
enum { FOCUS_LIST, FOCUS_DECLINE, FOCUS_ACCEPT, FOCUS_COUNT };

/* the operator and its address (ae_community.h): consent has to say who receives the data */
#define OPERATOR "ChupathingyCE"
#define SITE "halo.milenko.org"
#define WHERE "SETTINGS > ONLINE > COMMUNITY SERVICES"

static struct
{
	struct ae_flow_screen flow;
	short focus;
	short hover;
	int list_on;
	unsigned long opened_at;
	/* the room the page was last drawn in */
	struct ae_rect rects[FOCUS_COUNT];
} page;

static struct ae_screen_class const community_class;

/* ---------- the answer */

static int input_open(void)
{
	return ae_community_input_open(page.opened_at, ae_motion_now());
}

static void answer(int use)
{
	if (!input_open())
		return;
	ae_community_write_answer(use, page.list_on);
	platform_log("ae community: answered %s (list games I host: %s)", use ? "accept" : "decline",
		page.list_on ? "on" : "off");
	ae_sound_request(use ? AE_SOUND_FORWARD : AE_SOUND_BACK, 0);
	ae_ui_pop();
}

static void list_toggle(int repeat)
{
	if (!input_open())
		return;
	page.list_on = !page.list_on;
	ae_sound_request(AE_SOUND_CURSOR, repeat);
}

static void focus_set(short focus, int repeat)
{
	if (focus < 0 || focus >= FOCUS_COUNT || focus == page.focus)
		return;
	page.focus = focus;
	ae_sound_request(AE_SOUND_CURSOR, repeat);
}

/* ---------- drawing */

/* a line of text with the menus' small drop shadow, its baseline at baseline */
static void put(struct ae_density const *d, int font, float size, float x, float baseline, unsigned int color,
	char const *text, float tracking)
{
	float top = baseline - ae_draw_cap_height(font, size), shadow = 1.5f * d->unit;

	if (tracking > 0.0f)
	{
		ae_draw_text_tracked(font, size, tracking, x + shadow, top + shadow, AE_ALIGN_LEFT, 0x000000AAu, text);
		ae_draw_text_tracked(font, size, tracking, x, top, AE_ALIGN_LEFT, color, text);
	}
	else
	{
		ae_draw_text(font, size, x + shadow, top + shadow, AE_ALIGN_LEFT, 0x000000AAu, text);
		ae_draw_text(font, size, x, top, AE_ALIGN_LEFT, color, text);
	}
}

/* a paragraph wrapped at width: lines drawn from baseline at pitch; returns the last line's baseline */
static float paragraph(struct ae_density const *d, int draw, float x, float baseline, float width, float size,
	float pitch, unsigned int color, char const *text, int maximum)
{
	char lines[6][160];
	int count = ae_wrap_text(AE_FONT_BODY, size, text, width, lines, maximum > 6 ? 6 : maximum), index;

	for (index = 0; index < count; index++)
	{
		if (draw)
			put(d, AE_FONT_BODY, size, x, baseline + (float)index * pitch, color, lines[index], 0.0f);
	}
	return baseline + (float)(count > 0 ? count - 1 : 0) * pitch;
}

/* a section's label: small capitals, tracked, muted */
static void label(struct ae_density const *d, int draw, float x, float baseline, char const *text)
{
	if (draw)
		put(d, AE_FONT_BODY, d->unit * 14.5f, x, baseline, AE_COLOR_MUTED, text, 0.14f);
}

/* the left column from y0: returns its bottom. draw 0 only measures (the buttons' and the row's places are still
set). Spec units from the page's own steps */
static float left_column(struct ae_density const *d, int draw, float x, float y0, float width,
	unsigned int const focus_flags[])
{
	float unit = d->unit, body = unit * 21.0f, note = unit * 18.0f, pitch = unit * 31.0f, column = width * 0.74f;
	float baseline, row_height = ae_size_row(d), button_width = (width - unit * 10.0f) * 0.5f, bottom;
	struct ae_row row;
	short index;

	baseline = paragraph(d, draw, x, y0 + unit * 24.0f, column, body, pitch, AE_COLOR_TITLE,
		"Two online services, run by the " OPERATOR " team. Both stay off unless you turn them on.", 3);
	baseline += unit * 57.0f;
	label(d, draw, x, baseline, "SERVER BROWSER");
	baseline = paragraph(d, draw, x, baseline + unit * 35.0f, column, body, pitch, AE_COLOR_TEXT,
		"Public internet games show up in SERVER BROWSER. The list's server sees your IP address.", 3);
	page.rects[FOCUS_LIST].x = x;
	page.rects[FOCUS_LIST].y = baseline + unit * 48.0f;
	page.rects[FOCUS_LIST].width = width;
	page.rects[FOCUS_LIST].height = row_height;
	if (draw)
	{
		memset(&row, 0, sizeof(row));
		row.label = "LIST GAMES I HOST";
		row.value = page.list_on ? "ON" : "OFF";
		row.flags = focus_flags[FOCUS_LIST];
		ae_widget_row(d, x, page.rects[FOCUS_LIST].y, width, &row, HIT_ROWS, FOCUS_LIST);
	}
	baseline = page.rects[FOCUS_LIST].y + row_height + unit * 26.0f;
	if (draw)
		put(d, AE_FONT_BODY, note, x, baseline, AE_COLOR_MUTED,
			"On: anyone can find and join the games you host. Only with ACCEPT.", 0.0f);
	baseline += unit * 62.0f;
	label(d, draw, x, baseline, "GAME STATS");
	baseline = paragraph(d, draw, x, baseline + unit * 35.0f, column, body, pitch, AE_COLOR_TEXT,
		"Games you host or join send their stats to the site's match pages: player names, scores, kills, medals, where "
		"players moved, the map, the time and a player ID.", 4);
	bottom = baseline + unit * 72.0f;
	for (index = FOCUS_DECLINE; index <= FOCUS_ACCEPT; index++)
	{
		page.rects[index].x = x + (float)(index - FOCUS_DECLINE) * (button_width + unit * 10.0f);
		page.rects[index].y = bottom;
		page.rects[index].width = button_width;
		page.rects[index].height = row_height;
		if (draw)
		{
			memset(&row, 0, sizeof(row));
			row.label = index == FOCUS_DECLINE ? "DECLINE" : "ACCEPT";
			row.flags = focus_flags[index] | AE_ROW_PRIMARY;
			ae_widget_row(d, page.rects[index].x, bottom, button_width, &row, HIT_ROWS, index);
		}
	}
	return bottom + row_height;
}

/* the card on the right from y0: returns its height; draw 0 only measures. The card's text stands 28 units in, its
rows between rules */
static float right_card(struct ae_density const *d, int draw, float x, float y0, float width)
{
	float unit = d->unit, inner = width - unit * 56.0f, tx = x + unit * 28.0f, baseline, small = unit * 15.0f;
	float body = unit * 19.0f, pitch = unit * 28.0f, rule_x = x + unit * 20.0f, rule_width = width - unit * 40.0f;
	float height;

	baseline = y0 + unit * 46.0f;
	if (draw)
	{
		put(d, AE_FONT_BODY, small, tx, baseline, AE_COLOR_MUTED, "RUN BY", 0.14f);
		put(d, AE_FONT_ROW, unit * 30.0f, tx, baseline + unit * 38.0f, AE_COLOR_TITLE, OPERATOR, 0.0f);
		put(d, AE_FONT_BODY, body, tx, baseline + unit * 68.0f, AE_COLOR_ACCENT, SITE, 0.0f);
	}
	baseline = paragraph(d, draw, tx, baseline + unit * 110.0f, inner * 0.85f, body, pitch, AE_COLOR_TEXT,
		"Arena Evolved does not run these services, so it cannot promise they are safe, private or always online.", 4);
	baseline += unit * 40.0f;
	if (draw)
		ae_draw_rect(rule_x, baseline, rule_width, 1.0f, 0.0f, AE_COLOR_RULE);
	baseline += unit * 38.0f;
	if (draw)
	{
		put(d, AE_FONT_BODY, small, tx, baseline, AE_COLOR_MUTED, "WORKS EITHER WAY", 0.14f);
		put(d, AE_FONT_BODY, body, tx, baseline + unit * 32.0f, AE_COLOR_TEXT,
			"LAN games, invites and JOIN BY ADDRESS.", 0.0f);
	}
	baseline += unit * 62.0f;
	if (draw)
		ae_draw_rect(rule_x, baseline - unit * 8.0f, rule_width, 1.0f, 0.0f, AE_COLOR_RULE);
	baseline += unit * 30.0f;
	if (draw)
	{
		put(d, AE_FONT_BODY, small, tx, baseline, AE_COLOR_MUTED, "CHANGE IT LATER", 0.14f);
		put(d, AE_FONT_ROW, unit * 18.0f, tx, baseline + unit * 32.0f, AE_COLOR_TITLE, WHERE, 0.0f);
	}
	height = baseline + unit * 32.0f + unit * 48.0f - y0;
	return height;
}

/* the line under the content: what the focused thing does */
static char const *help_for(short focus)
{
	if (focus == FOCUS_LIST)
		return page.list_on ? "Anyone can find and join the games you host, once you ACCEPT." :
			"Your games stay off the public list. Turn it on to let anyone find and join them.";
	if (focus == FOCUS_ACCEPT)
		return page.list_on ? "Turns on the server browser, the game stats and listing your games." :
			"Turns on the server browser and the game stats. Your games stay unlisted.";
	return "Keep both off. You can turn them on later in SETTINGS.";
}

static void community_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &page.flow;
	struct ae_layout layout;
	struct ae_density content;
	unsigned int flags[FOCUS_COUNT];
	float scale = ae_settings_ui_scale(), fit, x, width, y0, panel_x, panel_width, help_baseline, room, bottom;
	float card_height;
	short index;
	char const *help;

	(void)instance;
	ae_flow_full_density(flow);
	ae_draw_current_layout(&layout);
	x = ae_flow_left_x(flow);
	width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH;
	y0 = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	panel_x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X + ae_flow_u(flow, AE_FLOW_INSET_U);
	panel_width = ae_flow_right_x(flow) - ae_flow_u(flow, AE_FLOW_INSET_U) - panel_x;
	ae_flow_title(flow, "COMMUNITY SERVICES", "BEFORE YOU PLAY ONLINE");
	/* (the room: from the content's top to the help line's top, 8 px clear) */
	help_baseline = ae_flow_content_bottom(flow) - ae_flow_u(flow, 44.0f);
	room = help_baseline - ae_draw_cap_height(AE_FONT_BODY, ae_flow_u(flow, 21.0f)) - ae_flow_u(flow, 12.0f) - y0;
	for (index = 0; index < FOCUS_COUNT; index++)
		flags[index] = 0;
	/* (the largest size that fits: UI SCALE's, then smaller in 4 % steps down to 60 %) */
	for (fit = 1.0f; fit > 0.6f; fit -= 0.04f)
	{
		ae_density_full(layout.height * layout.scale, scale * fit, &content);
		bottom = left_column(&content, 0, x, y0, width, flags) - y0;
		card_height = right_card(&content, 0, panel_x, y0, panel_width);
		if (bottom <= room && card_height <= room)
			break;
	}
	for (index = 0; index < FOCUS_COUNT; index++)
	{
		flags[index] = 0;
		if (page.focus == index)
			flags[index] |= AE_ROW_FOCUSED;
		if (page.hover == index)
			flags[index] |= AE_ROW_HOVER;
	}
	help = help_for(page.focus);
	left_column(&content, 1, x, y0, width, flags);
	card_height = right_card(&content, 0, panel_x, y0, panel_width);
	ae_draw_rect(panel_x, y0, panel_width, card_height, 0.0f, AE_COLOR_PANEL);
	ae_draw_rect(panel_x, y0, panel_width, ae_flow_u(flow, 3.0f) * fit, 0.0f, AE_COLOR_ACCENT);
	right_card(&content, 1, panel_x, y0, panel_width);
	put(&flow->density, AE_FONT_BODY, ae_flow_u(flow, 21.0f), x, help_baseline, AE_COLOR_TITLE, help, 0.0f);
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Select", AE_ACTION_ACCEPT);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Decline", AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

/* ---------- input */

static int community_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_BACK:
	case AE_ACTION_START:
		/* (B, Esc and START are DECLINE) */
		answer(0);
		return 1;
	case AE_ACTION_ACCEPT:
		if (page.focus == FOCUS_LIST)
			list_toggle(0);
		else
			answer(page.focus == FOCUS_ACCEPT);
		return 1;
	case AE_ACTION_UP:
		if (page.focus != FOCUS_LIST)
			focus_set(FOCUS_LIST, event->repeat);
		return 1;
	case AE_ACTION_DOWN:
		if (page.focus == FOCUS_LIST)
			focus_set(FOCUS_DECLINE, event->repeat);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (page.focus == FOCUS_LIST)
			list_toggle(event->repeat);
		else
			focus_set(event->action == AE_ACTION_LEFT ? FOCUS_DECLINE : FOCUS_ACCEPT, event->repeat);
		return 1;
	default:
		return 1;
	}
}

static void community_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	int over;

	if (ae_prompts_pointer(page.flow.prompts, page.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		event.device = AE_DEVICE_KEYBOARD_MOUSE;
		community_handle(instance, &event);
		return;
	}
	over = ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_ROWS && hit.index >= 0 &&
		hit.index < FOCUS_COUNT;
	if (pointer->moved)
	{
		page.hover = over ? hit.index : -1;
		if (over)
			focus_set(hit.index, 0);
	}
	if (!over || !pointer->left_clicks)
		return;
	page.focus = hit.index;
	if (hit.index == FOCUS_LIST)
		list_toggle(0);
	else
		answer(hit.index == FOCUS_ACCEPT);
}

static void community_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae community: closed");
}

static struct ae_screen_class const community_class =
{
	.name = "community services", .leave = community_leave, .handle = community_handle, .draw = community_draw,
	.pointer = community_pointer,
};

int ae_screen_community_open(void)
{
	if (ae_ui_depth())
		return 0;
	memset(&page, 0, sizeof(page));
	page.flow.hit_rows = HIT_ROWS;
	page.flow.hit_prompts = HIT_PROMPTS;
	page.focus = FOCUS_DECLINE;
	page.hover = -1;
	page.opened_at = ae_motion_now();
	if (!ae_ui_push(&community_class, AE_OWNER_ANY, &page))
		return 0;
	platform_log("ae community: open");
	(void)ae_settings_ui_scale();
	return 1;
}
