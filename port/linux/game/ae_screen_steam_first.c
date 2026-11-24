/*
AE_SCREEN_STEAM_FIRST.C

The first start's MCC FROM STEAM page (ae_screen_steam_first.h), built as a copy of COMMUNITY SERVICES' page
(ae_screen_community.c): the title and its crumb, on the left what it offers, the I HAVE MCC INSTALLED row and
NOT NOW / SIGN IN WITH STEAM side by side as the primary action's look; on the right a card (what you get, where the
password goes, where to change it). Information only on the right.

- Focus: NOT NOW first (an opt-in is never answered by a stray A). Up / down between the row and the buttons, left /
  right between the buttons. B, Esc and START are NOT NOW. A press in the first half second is not taken.
- NOT NOW answers; SIGN IN WITH STEAM answers, AE's main menu comes and STEAM ACCOUNT opens over it (its B goes back to
  the menu); I HAVE MCC INSTALLED opens the system's folder dialog (ae_mcc_browse): MCC found answers and the menu comes,
  anything else says why in the row's value and the page stays.
- Every answer is written at once (game.steam_first_start_asked).
- The content is drawn a little smaller, in 4 % steps down to 60 %, when it does not fit (UI SCALE 130 %).

Its log: "ae steam first start: asked", "answered not now | sign in | mcc folder (found | not mcc)", "closed".
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_community_rules.h"
#include "../src/ae_draw.h"
#include "../src/ae_mcc.h"
#include "../src/ae_steam_client.h"
#include "ae_hooks.h"
#include "ae_motion.h"
#include "ae_screen_main_menu.h"
#include "ae_screen_steam.h"
#include "ae_screen_steam_first.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_steam_rules.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);
/* port_config.c */
int config_boolean(const char *name);
int config_write_boolean(const char *name, int value);

#define ASKED_KEY "game.steam_first_start_asked"

enum
{
	FIRST_HIT = 0x7a80,
	HIT_ROWS = FIRST_HIT + 1, HIT_PROMPTS
};

/* the focusable things: the row, then the two buttons */
enum { FOCUS_MCC, FOCUS_NOT_NOW, FOCUS_SIGN_IN, FOCUS_COUNT };

static struct
{
	struct ae_flow_screen flow;
	short focus;
	short hover;
	unsigned long opened_at;
	int browsed;                     /* the folder dialog was asked for: the row says how it went */
	int staged;
	struct ae_rect rects[FOCUS_COUNT];
} page;

static struct ae_screen_class const first_class;

static int system_kind(void)
{
#if defined(__APPLE__)
	return AE_STEAM_SYSTEM_MACOS;
#elif defined(_WIN32)
	return AE_STEAM_SYSTEM_WINDOWS;
#else
	return AE_STEAM_SYSTEM_LINUX;
#endif
}

/* ---------- when it shows */

int ae_steam_first_start_asking(void)
{
	struct ae_steam_status status;

	if (!ae_steam_client_built() || config_boolean(ASKED_KEY) || ae_mcc_state() != AE_MCC_NOT_FOUND)
		return 0;
	/* (the client starts here, so a kept sign-in is known) */
	ae_steam_client_open();
	ae_steam_client_frame();
	ae_steam_client_status(&status);
	return ae_steam_first_start_asks(1, 0, 1, status.session != AE_STEAM_SESSION_OUT);
}

/* ---------- the answers */

static int input_open(void)
{
	return ae_community_input_open(page.opened_at, ae_motion_now());
}

static void asked_write(void)
{
	if (!page.staged)
		config_write_boolean(ASKED_KEY, 1);
}

static void answer_not_now(void)
{
	if (!input_open())
		return;
	asked_write();
	platform_log("ae steam first start: answered not now");
	ae_sound_request(AE_SOUND_BACK, 0);
	ae_ui_pop();
}

static void answer_sign_in(void)
{
	if (!input_open())
		return;
	asked_write();
	platform_log("ae steam first start: answered sign in");
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_ui_pop();
	/* (the menu under it, STEAM ACCOUNT over: its B comes back to the menu) */
	ae_screen_main_menu_open();
	/* (a staged page leads to a staged one: the live screen would start the client, and sign in with a saved token) */
	if (page.staged)
		(void)ae_screen_steam_open_staged(AE_STEAM_STAGE_SIGNED_OUT, 0);
	else
		(void)ae_screen_steam_open("FIRST START");
}

static void answer_folder(void)
{
	if (!input_open())
		return;
	page.browsed = 1;
	platform_log("ae steam first start: mcc folder asked");
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_mcc_browse();
}

static void focus_set(short focus, int repeat)
{
	if (focus < 0 || focus >= FOCUS_COUNT || focus == page.focus)
		return;
	page.focus = focus;
	ae_sound_request(AE_SOUND_CURSOR, repeat);
}

static void activate(short focus)
{
	if (focus == FOCUS_MCC)
		answer_folder();
	else if (focus == FOCUS_SIGN_IN)
		answer_sign_in();
	else
		answer_not_now();
}

/* ---------- drawing (the community page's numbers) */

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

static float paragraph(struct ae_density const *d, int draw, float x, float baseline, float width, float size,
	float pitch, unsigned int color, char const *text, int maximum)
{
	char lines[6][160];
	int count = ae_wrap_text(AE_FONT_BODY, size, text, width, lines, maximum > 6 ? 6 : maximum), index;

	for (index = 0; index < count; index++)
		if (draw)
			put(d, AE_FONT_BODY, size, x, baseline + (float)index * pitch, color, lines[index], 0.0f);
	return baseline + (float)(count > 0 ? count - 1 : 0) * pitch;
}

static void label(struct ae_density const *d, int draw, float x, float baseline, char const *text)
{
	if (draw)
		put(d, AE_FONT_BODY, d->unit * 14.5f, x, baseline, AE_COLOR_MUTED, text, 0.14f);
}

static float left_column(struct ae_density const *d, int draw, float x, float y0, float width,
	unsigned int const focus_flags[])
{
	float unit = d->unit, body = unit * 21.0f, note = unit * 18.0f, pitch = unit * 31.0f, column = width * 0.74f;
	float baseline, row_height = ae_size_row(d), button_width = (width - unit * 10.0f) * 0.5f, bottom;
	struct ae_row row;
	short index;

	baseline = paragraph(d, draw, x, y0 + unit * 24.0f, column, body, pitch, AE_COLOR_TITLE,
		"No MCC install was found on this PC. Sign in with Steam, and AE downloads MCC's shared map files and Halo 1 "
		"Workshop maps.", 3);
	baseline += unit * 57.0f;
	label(d, draw, x, baseline, "SIGN IN WITH STEAM");
	baseline = paragraph(d, draw, x, baseline + unit * 35.0f, column, body, pitch, AE_COLOR_TEXT,
		"It needs a Steam account that owns Halo: The Master Chief Collection and Halo: Combat Evolved Anniversary. The "
		"next page asks for its name, password and Steam Guard.", 4);
	baseline += unit * 57.0f;
	label(d, draw, x, baseline, "MCC IS ON THIS PC");
	page.rects[FOCUS_MCC].x = x;
	page.rects[FOCUS_MCC].y = baseline + unit * 26.0f;
	page.rects[FOCUS_MCC].width = width;
	page.rects[FOCUS_MCC].height = row_height;
	if (draw)
	{
		memset(&row, 0, sizeof(row));
		row.label = "I HAVE MCC INSTALLED";
		row.value = page.browsed ? ae_mcc_status_text() : "CHOOSE ITS FOLDER";
		row.flags = focus_flags[FOCUS_MCC] | AE_ROW_LINK | AE_ROW_MARQUEE;
		ae_widget_row(d, x, page.rects[FOCUS_MCC].y, width, &row, HIT_ROWS, FOCUS_MCC);
	}
	baseline = page.rects[FOCUS_MCC].y + row_height + unit * 26.0f;
	if (draw)
		put(d, AE_FONT_BODY, note, x, baseline, AE_COLOR_MUTED, "AE reads MCC's files where they are and never changes them.",
			0.0f);
	bottom = baseline + unit * 51.0f;
	for (index = FOCUS_NOT_NOW; index <= FOCUS_SIGN_IN; index++)
	{
		page.rects[index].x = x + (float)(index - FOCUS_NOT_NOW) * (button_width + unit * 10.0f);
		page.rects[index].y = bottom;
		page.rects[index].width = button_width;
		page.rects[index].height = row_height;
		if (draw)
		{
			memset(&row, 0, sizeof(row));
			row.label = index == FOCUS_NOT_NOW ? "NOT NOW" : "SIGN IN WITH STEAM";
			row.flags = focus_flags[index] | AE_ROW_PRIMARY;
			ae_widget_row(d, page.rects[index].x, bottom, button_width, &row, HIT_ROWS, index);
		}
	}
	return bottom + row_height;
}

static float right_card(struct ae_density const *d, int draw, float x, float y0, float width)
{
	float unit = d->unit, inner = width - unit * 56.0f, tx = x + unit * 28.0f, baseline, small = unit * 15.0f;
	float body = unit * 19.0f, pitch = unit * 28.0f, rule_x = x + unit * 20.0f, rule_width = width - unit * 40.0f;
	char password[300];

	snprintf(password, sizeof(password), "Goes to Steam only, encrypted with Steam's own key. Arena Evolved never saves it. %s",
		ae_steam_first_store_text(system_kind(), ae_steam_client_store()));
	baseline = y0 + unit * 46.0f;
	if (draw)
		put(d, AE_FONT_BODY, small, tx, baseline, AE_COLOR_MUTED, "WHAT YOU GET", 0.14f);
	baseline = paragraph(d, draw, tx, baseline + unit * 38.0f, inner * 0.92f, body, pitch, AE_COLOR_TEXT,
		"MCC's three shared map files, then any Halo 1 Workshop map you pick. AE lists them with MCC's maps.", 3);
	baseline += unit * 40.0f;
	if (draw)
		ae_draw_rect(rule_x, baseline - unit * 8.0f, rule_width, 1.0f, 0.0f, AE_COLOR_RULE);
	baseline += unit * 30.0f;
	if (draw)
		put(d, AE_FONT_BODY, small, tx, baseline, AE_COLOR_MUTED, "YOUR PASSWORD", 0.14f);
	baseline = paragraph(d, draw, tx, baseline + unit * 38.0f, inner * 0.92f, body, pitch, AE_COLOR_TEXT, password, 5);
	baseline += unit * 40.0f;
	if (draw)
		ae_draw_rect(rule_x, baseline - unit * 8.0f, rule_width, 1.0f, 0.0f, AE_COLOR_RULE);
	baseline += unit * 30.0f;
	if (draw)
	{
		put(d, AE_FONT_BODY, small, tx, baseline, AE_COLOR_MUTED, "CHANGE IT LATER", 0.14f);
		put(d, AE_FONT_ROW, unit * 18.0f, tx, baseline + unit * 32.0f, AE_COLOR_TITLE,
			"SETTINGS > SYSTEM > MCC > STEAM ACCOUNT", 0.0f);
	}
	return baseline + unit * 32.0f + unit * 48.0f - y0;
}

static char const *help_for(short focus)
{
	if (focus == FOCUS_SIGN_IN)
		return "Opens STEAM ACCOUNT: your Steam account name, your password, then Steam Guard.";
	if (focus == FOCUS_MCC)
		return "Opens a folder picker: choose the folder that holds halo1 (Halo The Master Chief Collection).";
	return "Play without them for now. SETTINGS > SYSTEM > MCC > STEAM ACCOUNT signs in any time.";
}

static void first_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &page.flow;
	struct ae_layout layout;
	struct ae_density content;
	unsigned int flags[FOCUS_COUNT];
	float scale = ae_settings_ui_scale(), fit, x, width, y0, panel_x, panel_width, help_baseline, room, bottom;
	float card_height;
	short index;

	(void)instance;
	ae_flow_full_density(flow);
	ae_draw_current_layout(&layout);
	x = ae_flow_left_x(flow);
	width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH;
	y0 = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	panel_x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X + ae_flow_u(flow, AE_FLOW_INSET_U);
	panel_width = ae_flow_right_x(flow) - ae_flow_u(flow, AE_FLOW_INSET_U) - panel_x;
	ae_flow_title(flow, "MCC FROM STEAM", "BEFORE YOU PLAY");
	help_baseline = ae_flow_content_bottom(flow) - ae_flow_u(flow, 44.0f);
	room = help_baseline - ae_draw_cap_height(AE_FONT_BODY, ae_flow_u(flow, 21.0f)) - ae_flow_u(flow, 12.0f) - y0;
	for (index = 0; index < FOCUS_COUNT; index++)
		flags[index] = 0;
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
	left_column(&content, 1, x, y0, width, flags);
	card_height = right_card(&content, 0, panel_x, y0, panel_width);
	ae_draw_rect(panel_x, y0, panel_width, card_height, 0.0f, AE_COLOR_PANEL);
	ae_draw_rect(panel_x, y0, panel_width, ae_flow_u(flow, 3.0f) * fit, 0.0f, AE_COLOR_ACCENT);
	right_card(&content, 1, panel_x, y0, panel_width);
	put(&flow->density, AE_FONT_BODY, ae_flow_u(flow, 21.0f), x, help_baseline, AE_COLOR_TITLE, help_for(page.focus), 0.0f);
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Select", AE_ACTION_ACCEPT);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Not now", AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

/* ---------- input */

static int first_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_BACK:
	case AE_ACTION_START:
		answer_not_now();
		return 1;
	case AE_ACTION_ACCEPT:
		activate(page.focus);
		return 1;
	case AE_ACTION_UP:
		focus_set(page.focus == FOCUS_MCC ? FOCUS_MCC : FOCUS_MCC, event->repeat);
		return 1;
	case AE_ACTION_DOWN:
		if (page.focus == FOCUS_MCC)
			focus_set(FOCUS_NOT_NOW, event->repeat);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (page.focus != FOCUS_MCC)
			focus_set(event->action == AE_ACTION_LEFT ? FOCUS_NOT_NOW : FOCUS_SIGN_IN, event->repeat);
		return 1;
	default:
		return 1;
	}
}

static void first_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	int over;

	if (ae_prompts_pointer(page.flow.prompts, page.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		event.device = AE_DEVICE_KEYBOARD_MOUSE;
		first_handle(instance, &event);
		return;
	}
	over = ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_ROWS && hit.index >= 0 && hit.index < FOCUS_COUNT;
	if (pointer->moved)
	{
		page.hover = over ? hit.index : -1;
		if (over)
			focus_set(hit.index, 0);
	}
	if (!over || !pointer->left_clicks)
		return;
	page.focus = hit.index;
	activate(hit.index);
}

/* a folder chosen that is MCC's answers the page (polled each frame: the dialog is the system's) */
static void first_update(struct ae_screen *instance)
{
	(void)instance;
	if (!page.browsed || page.staged)
		return;
	(void)ae_mcc_status_text();
	if (ae_mcc_state() == AE_MCC_FOUND)
	{
		asked_write();
		platform_log("ae steam first start: answered mcc folder (found)");
		ae_sound_request(AE_SOUND_FORWARD, 0);
		page.browsed = 0;
		ae_ui_pop();
	}
}

static void first_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae steam first start: closed");
}

static struct ae_screen_class const first_class =
{
	.name = "mcc from steam", .leave = first_leave, .handle = first_handle, .draw = first_draw,
	.pointer = first_pointer, .update = first_update,
};

static int open_page(int staged)
{
	memset(&page, 0, sizeof(page));
	page.flow.hit_rows = HIT_ROWS;
	page.flow.hit_prompts = HIT_PROMPTS;
	page.focus = FOCUS_NOT_NOW;
	page.hover = -1;
	page.staged = staged;
	page.opened_at = ae_motion_now();
	if (!ae_ui_push(&first_class, AE_OWNER_ANY, &page))
		return 0;
	ae_ui_replace_menus();
	platform_log("ae steam first start: asked%s", staged ? " (staged)" : "");
	(void)ae_settings_ui_scale();
	return 1;
}

int ae_screen_steam_first_open(void)
{
	if (ae_ui_depth())
		return 0;
	return open_page(0);
}

int ae_screen_steam_first_open_staged(void)
{
	(void)ae_ui_remove(&page);
	return open_page(1);
}
