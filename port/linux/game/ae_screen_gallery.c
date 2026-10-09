/*
AE_SCREEN_GALLERY.C

The AE menus' widget gallery (ae_screen_gallery.h): every M2 widget with the
mockups' sample content (notes/specs/2026-10-07-ae-menus-M2-widgets.md, mockups
01, 04-07, 24, 25), for the contact sheets and the owner's design gate. A debug
screen: debug.ae_test_screen 11-15 (one view, pages 1-5), 21 / 23 (two views,
pages 1-2 / 3-4), 41 (four views, pages 1-4). It replaces upstream's menus (they
stay closed after it: quit the game).

FULL pages: 1 lists (04), 2 lobby (05), 3 text entry (06), 4 dialog (07), 5 the
main menu with the old menu off (01, spec 8). Every FULL position is a fraction
of the frame (ae_frame_compute), every size u. Split views (synthetic, as M1's
test screen: halves stacked, quarters) each draw an in-view panel at VIEW density
with that view's page.

Input: the gallery is anyone's. LB / RB switch pages (one view); up / down move
the focus; A on a picker row opens it; Enter or a click on a field starts typing
into it (the whole text selected, so typing replaces it; Enter keeps, Esc
restores), a pad's A opens AE's keyboard for it; B closes a popover, then the
gallery. Page 4 opens its dialog as it shows.

Its log (tools/test_ae_gallery.py): "ae gallery: open (value N)", one "ae
gallery: view V page P density FULL|VIEW s= panel= text= minor= row=" per view
as it opens and when the window changes, "ae gallery: focus P/N", "ae gallery:
field '<text>'", "ae gallery: keyboard open|closed", "ae gallery: picked
<value>", "ae gallery: dialog <choice>", "ae gallery: item <name>", "ae
gallery: closed".
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_hooks.h"
#include "ae_screen_gallery.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

/* the gallery's hits: GALLERY_HIT + view x 16 + the part's own number */
enum
{
	GALLERY_HIT = 0x7C00,
	HIT_LIST = 1, HIT_TABS, HIT_ROWS, HIT_CHIPS, HIT_HELP, HIT_PROMPTS, HIT_FIELDS, HIT_CARDS, HIT_DOTS, HIT_PICKER_SAMPLE,
	PAGES = 5,
	VIEWS_MAXIMUM = 4
};

/* the mockups' FULL positions (fractions of the frame: 1920 x 1080 at 100 %) */
#define TITLE_Y (70.0f / 1080.0f)
#define TABS_Y (148.0f / 1080.0f)
#define CONTENT_Y (232.0f / 1080.0f)
#define LIST_WIDTH 0.52f
#define LIST_BOTTOM (995.0f / 1080.0f)   /* the list's "N more" line just above the footer, as 04 */
#define HELP_X (1143.0f / 1920.0f)
#define HELP_BOTTOM (958.0f / 1080.0f)
#define FOOTER_ABOVE_U 62.0f
/* sizes, u */
#define TITLE_U 60.0f
#define MAIN_TITLE_U 72.0f
#define CRUMB_U 20.0f
#define CRUMB_TRACKING 0.09f
#define KICKER_TRACKING 0.12f
#define RULE_U 1.0f
#define SEARCH_WIDTH_U 300.0f
#define LOBBY_ROWS_WIDTH (690.0f / 1920.0f)   /* widths are fractions of the frame too (mockups 09, 10) */
#define FIELDS_WIDTH (768.0f / 1920.0f)
#define ROSTER_BOTTOM (723.0f / 1080.0f)
#define FOOTER_ROOM_U 40.0f                    /* the footer row's room above its middle */
#define FIELD_GAP_U 30.0f
#define MAIN_PANEL_Y_U 330.0f
#define MAIN_PANEL_WIDTH_U 600.0f
#define MAIN_PANEL_PAD_U 28.0f
#define ACCENT_RULE_WIDTH_U 120.0f
#define ACCENT_RULE_U 4.0f
#define VERSION_U 16.0f

/* ---------- the sample content (the mockups') */

struct sample_row { const char *label, *value, *base, *reason; unsigned int flags; int group; };

/* page 1: Settings > VIDEO (04) */
static struct sample_row const video_rows[] =
{
	{ "DISPLAY MODE", "BORDERLESS", NULL, NULL, 0, 0 },
	{ "RESOLUTION", "NATIVE", NULL, NULL, AE_ROW_HOVER, 0 },
	{ "V-SYNC", NULL, NULL, "Needs DISPLAY MODE: FULLSCREEN", AE_ROW_DISABLED, 0 },
	{ "FRAME RATE LIMIT", "165", "AUTO", NULL, AE_ROW_CHANGED, 0 },
	{ "SMOOTH MOTION", "ON", NULL, NULL, 0, 0 },
	{ "GRAPHICS", NULL, NULL, NULL, 0, 1 },
	{ "ANTI-ALIASING", "SMAA", "OFF", NULL, AE_ROW_CHANGED, 0 },
	{ "SHADOW RESOLUTION", "512", NULL, NULL, 0, 0 },
	{ "PERFORMANCE OVERLAY", "FPS", "OFF", NULL, AE_ROW_CHANGED | AE_ROW_AE, 0 },
	{ "POSITION", "TOP LEFT", NULL, NULL, AE_ROW_AE, 0 },
	{ "HIGH-RES TEXT", "ON", NULL, NULL, 0, 0 },
	{ "INSTANT AIM", "ON", NULL, NULL, 0, 0 },
	{ "PER-PIXEL LIGHTING", "ON", NULL, NULL, 0, 0 },
	{ "AMBIENT OCCLUSION", "ON", NULL, NULL, 0, 0 },
	{ "TEXTURE FILTER", "16X", NULL, NULL, 0, 0 },
	{ "GAMMA", "1.0", NULL, NULL, 0, 0 },
	{ "BRIGHTNESS", "50", NULL, NULL, 0, 0 },
};
enum { VIDEO_ROWS = sizeof(video_rows) / sizeof(video_rows[0]), VIDEO_FIRST_FOCUS = 3 };
static struct ae_tab const settings_tabs[] =
{
	{ "GAME", 1, 0, NULL }, { "HUD", 0, 0, NULL }, { "VIDEO", 1, 0, NULL }, { "AUDIO", 0, 0, NULL },
	{ "CONTROLS", 0, 0, NULL }, { "NETWORK", 0, 0, NULL }, { "PROFILE", 0, 0, NULL },
};
static const char *const frame_rate_values[] = { "AUTO", "30", "60", "120", "144", "165", "240", "NONE" };

/* page 2: the Custom Games lobby (05) */
enum { LOBBY_START, LOBBY_MAP, LOBBY_TYPE, LOBBY_NETWORK, LOBBY_OPTIONS, LOBBY_PRIVACY, LOBBY_INVITE, LOBBY_ROWS };
static const char *const lobby_labels[LOBBY_ROWS] =
	{ "START GAME", "CHANGE MAP", "CHANGE GAME TYPE", "NETWORK", "GAME OPTIONS", "PRIVACY", "INVITE" };
static const char *const map_values[] = { "BLOOD GULCH", "BATTLE CREEK", "PRISONER", "HANG 'EM HIGH", "DERELICT" };
static const char *const type_values[] = { "AE FFA SLAY", "AE TEAM SLAY", "AE CTF", "AE KING" };
static const char *const network_values[] = { "LOCAL", "LAN", "ONLINE" };
static const char *const privacy_values[] = { "PUBLIC", "FRIENDS", "INVITE ONLY" };
static struct ae_chip const mode_chips[] =
{
	{ "SLAYER", 24, AE_CHIP_ON }, { "CTF", 11, AE_CHIP_ON | AE_CHIP_FOCUSED }, { "ODDBALL", 9, 0 },
	{ "KING", 7, AE_CHIP_HOVER }, { "RACE", -1, AE_CHIP_UNSUPPORTED }, { "TEAM", -1, AE_CHIP_DISABLED },
};
#define RED 0xC0392BFFu
#define BLUE 0x2E86DEFFu
#define GREEN 0x7FA035FFu
#define YELLOW 0xE6B422FFu
static unsigned int const player_colors[VIEWS_MAXIMUM] = { RED, BLUE, GREEN, YELLOW };

/* page 3: Save as new (06) */
enum { FIELD_NAME, FIELD_DISPLAY, FIELD_DESCRIPTION, FIELD_TAKEN, FIELD_BASED_ON, FIELDS };
static const char *const field_labels[FIELDS] = { "GAME TYPE NAME (11 characters: what other builds see)",
	"DISPLAY NAME", "DESCRIPTION", "GAME TYPE NAME", "BASED ON" };

/* page 4: Game Options > ARENA under a dialog (07) */
static struct sample_row const arena_rows[] =
{
	{ "HEALTH", "HALO 2", "REACH", NULL, AE_ROW_CHANGED | AE_ROW_AE, 0 },
	{ "NO SPREAD", "FULL", NULL, NULL, AE_ROW_AE, 0 },
	{ "FALL DAMAGE", "OFF", "ON", NULL, AE_ROW_CHANGED | AE_ROW_AE, 0 },
	{ "PRE-GAME COUNTDOWN", "ON", NULL, NULL, AE_ROW_AE, 0 },
	{ "NHE MODE", "OFF", NULL, NULL, AE_ROW_AE, 0 },
};
static struct ae_tab const options_tabs[] =
{
	{ "SLAYER", 1, 0, NULL }, { "PLAYERS", 0, 0, NULL }, { "ITEMS", 0, 0, NULL }, { "VEHICLES", 0, 0, NULL },
	{ "INDICATORS", 0, 0, NULL }, { "TEAMS", 0, 0, NULL }, { "ARENA", 1, 0, NULL }, { "TRAINING OPTIONS", 0, 0, NULL },
};

/* page 5: the main menu (01) */
static const char *const main_items[] = { "CAMPAIGN", "CUSTOM GAMES", "SERVER BROWSER", "SETTINGS" };
enum { MAIN_ITEMS = 4 };

/* the split views' pages: YOUR SETTINGS (a list), TRAINING (rows), MY HUD (a field and AE's keyboard), GAME (rows
under a dialog) */
static const char *const view_page_names[] = { "YOUR SETTINGS", "TRAINING", "MY HUD", "GAME" };
static struct sample_row const settings_view_rows[] =
{
	{ "CONTROLLER LAYOUT", "RECLAIMER", NULL, NULL, 0, 0 },
	{ "LOOK SENSITIVITY", "5", NULL, NULL, 0, 0 },
	{ "INVERT LOOK", "OFF", NULL, NULL, 0, 0 },
	{ "VIBRATION", "ON", NULL, NULL, 0, 0 },
	{ "COLOUR", "BLUE", "RED", NULL, AE_ROW_CHANGED, 0 },
	{ "AIM ASSIST", "STANDARD", NULL, NULL, 0, 0 },
	{ "CROUCH", "HOLD", NULL, NULL, 0, 0 },
	{ "ZOOM", "TOGGLE", NULL, NULL, 0, 0 },
};
static struct sample_row const training_view_rows[] =
{
	{ "SPAWN HEAT", "MINE", NULL, NULL, AE_ROW_AE, 0 },
	{ "PRACTICE MODE", NULL, NULL, "set by the host", AE_ROW_DISABLED, 0 },
	{ "TIMERS", NULL, NULL, "set by the host", AE_ROW_DISABLED, 0 },
};
static struct sample_row const game_view_rows[] =
{
	{ "RESUME", NULL, NULL, NULL, 0, 0 }, { "LEAVE GAME", NULL, NULL, NULL, 0, 0 },
	{ "SCOREBOARD", NULL, NULL, NULL, 0, 0 },
};
static const char *const view_help[] =
{
	"Colour applies from the next game. Saved to this player's own profile at once.",
	"Your own spawn heat view: MINE, ENEMY or OFF. The host turned it on.",
	"A pad's A on a field opens the keyboard; a physical keyboard types at any time.",
	"Leaving takes every player on this PC.",
};

/* (callback contexts: a row's or field's number, by pointer) */
static short const indexes[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };

/* ---------- the state */

struct view_state
{
	int page;                       /* 1-4 */
	struct ae_list_view list;
	short focus;
	struct ae_text text;
	struct ae_rect view;            /* layout units */
	struct ae_rect content;         /* the panel's content, layout units (from the last draw) */
	struct ae_rect panel;           /* the panel, layout units: its popovers' bounds */
	struct ae_rect field;           /* MY HUD's field well, layout units */
	struct ae_density density;
	int drawn, popover_opened;
	float logged_width, logged_height, logged_scale;
};

static struct
{
	int value, views, page;
	struct view_state view[VIEWS_MAXIMUM];
	/* FULL */
	struct ae_density density;
	struct ae_frame frame;
	float logged_width, logged_height, logged_scale, logged_ui;
	struct ae_prompt footer_prompts[4];
	short footer_count;
	int drawn, dialog_shown;
	/* page 1 */
	struct ae_tabs tabs;
	struct ae_list_view list;
	/* page 2 */
	short lobby_focus, map, type, network, privacy;
	struct ae_rect lobby_rows[LOBBY_ROWS];         /* layout units */
	/* page 3 */
	short field_focus;
	struct ae_text texts[FIELDS];
	struct ae_rect wells[FIELDS];                  /* layout units */
	struct ae_field_edit edit;
	short editing;                                 /* the field being typed into, -1 */
	int keyboard_open, field_logged;
	unsigned long caret_since;
	/* page 4 */
	struct ae_tabs options;
	int dialog_open;
	/* page 5 */
	short main_focus;
} gallery;

static struct ae_screen_class const gallery_class;

/* ---------- small helpers */

static float u(float spec_units)
{
	return spec_units * gallery.frame.unit;
}

static float left_x(void)
{
	return gallery.frame.rect.x + gallery.frame.margin;
}

static float right_x(void)
{
	return gallery.frame.rect.x + gallery.frame.rect.width - gallery.frame.margin;
}

static float frame_y(float fraction)
{
	return ae_frame_y(&gallery.frame, fraction);
}

static float cap_top(int font, float size, float center_y)
{
	return center_y - ae_draw_cap_height(font, size) * 0.5f;
}

static void sound_cursor(int moved, int repeat)
{
	if (moved)
		ae_sound_request(AE_SOUND_CURSOR, repeat);
}

static void log_focus(short focus, short count)
{
	platform_log("ae gallery: focus %d/%d", focus + 1, count);
}

static void row_of(struct sample_row const *sample, unsigned int flags, struct ae_row *row)
{
	memset(row, 0, sizeof(*row));
	row->label = sample->label;
	row->value = sample->value;
	row->base_value = sample->base;
	row->reason = sample->reason;
	row->flags = sample->flags | flags;
}

/* the FULL density and frame for the window now (UI SCALE from the settings) */
static void full_density(void)
{
	struct ae_layout layout;
	float scale = ae_settings_ui_scale();

	ae_draw_current_layout(&layout);
	ae_density_full(layout.height * layout.scale, scale, &gallery.density);
	ae_frame_compute(layout.width, scale, &gallery.frame);
	if (layout.width != gallery.logged_width || layout.scale != gallery.logged_scale || scale != gallery.logged_ui ||
		gallery.logged_height != layout.height)
	{
		gallery.logged_ui = scale;
		gallery.logged_width = layout.width;
		gallery.logged_height = layout.height;
		gallery.logged_scale = layout.scale;
		platform_log("ae gallery: view 1 page %d density FULL s=%.2f panel=%.0fx%.0f text=%.1fpx minor=%.1fpx row=%.1fpx",
			gallery.page, gallery.density.s, gallery.frame.rect.width * layout.scale,
			gallery.frame.rect.height * layout.scale, ae_size_text(&gallery.density) / gallery.density.pixel,
			ae_size_minor(&gallery.density) / gallery.density.pixel, ae_size_row(&gallery.density) / gallery.density.pixel);
	}
}

/* the title (OpenCE 60 u) top left and a crumb (Overpass 750 20 u, tracked, muted) top right, on its middle */
static void page_title(const char *title, const char *crumb)
{
	float size = u(TITLE_U), top = frame_y(TITLE_Y), crumb_size = u(CRUMB_U);
	float middle = top + ae_draw_cap_height(AE_FONT_TITLE, size) * 0.5f;

	ae_draw_text(AE_FONT_TITLE, size, left_x(), top, AE_ALIGN_LEFT, AE_COLOR_TITLE, title);
	if (crumb)
		ae_draw_text_tracked(AE_FONT_BODY, crumb_size, CRUMB_TRACKING, right_x(), cap_top(AE_FONT_BODY, crumb_size, middle),
			AE_ALIGN_RIGHT, AE_COLOR_MUTED, crumb);
}

/* the content's bottom: above the footer's row (at 130 % parts that don't fit there are left out) */
static float content_bottom(void)
{
	return gallery.frame.rect.height - u(FOOTER_ABOVE_U) - u(FOOTER_ROOM_U);
}

static void footer(struct ae_prompt const *prompts, short count, const char *status)
{
	/* (kept for the pointer: a click on a prompt is its action) */
	gallery.footer_count = count < 4 ? count : 4;
	memcpy(gallery.footer_prompts, prompts, sizeof(prompts[0]) * (size_t)gallery.footer_count);
	ae_widget_prompts(&gallery.density, left_x(), gallery.frame.rect.height - u(FOOTER_ABOVE_U), prompts, count, status,
		right_x(), -1, GALLERY_HIT + HIT_PROMPTS);
}

static struct ae_prompt prompt_of(int button, const char *label, int action, const char *key)
{
	struct ae_prompt prompt;

	prompt.button = button;
	prompt.label = label;
	prompt.key = key;
	prompt.action = action;
	return prompt;
}

/* ---------- page 1: lists (04) */

struct list_context { struct ae_density const *density; struct sample_row const *rows; short hit_id; };

static void list_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct list_context const *list = context;
	struct ae_row row;

	/* (a group header in the list: its label at the slot's foot, not focusable) */
	if (list->rows[item].group)
	{
		float group = list->density->metrics->group_height * list->density->unit;

		ae_widget_group(list->density, x, y + height - group, width, list->rows[item].label);
		return;
	}
	row_of(&list->rows[item], flags, &row);
	/* (the hover sample only off the bar) */
	if (flags & (AE_ROW_FOCUSED | AE_ROW_ON_BAR | AE_ROW_UNDER_BAR))
		row.flags &= ~(unsigned int)AE_ROW_HOVER;
	ae_widget_row(list->density, x, y, width, &row, list->hit_id, item);
}

/* a list's focus never rests on a group header: on past it the same way, else back */
static void skip_groups(struct ae_list *list, struct sample_row const *rows, short direction)
{
	if (list->focus < 0 || !rows[list->focus].group)
		return;
	ae_list_move(list, direction, 0);
	if (rows[list->focus].group)
		ae_list_move(list, (short)-direction, 0);
}

static void draw_lists(void)
{
	struct ae_density const *d = &gallery.density;
	struct list_context context;
	struct ae_rect help_rect, preview;
	struct ae_help help;
	struct ae_field search;
	struct ae_text search_text;
	struct ae_prompt prompts[4];
	float left = left_x(), width = gallery.frame.rect.width * LIST_WIDTH, y, top = frame_y(CONTENT_Y), tabs_height;

	page_title("SETTINGS", "PROFILE: PETER");
	gallery.tabs.tabs = settings_tabs;
	gallery.tabs.count = (short)(sizeof(settings_tabs) / sizeof(settings_tabs[0]));
	tabs_height = ae_widget_tabs(d, &gallery.tabs, left, frame_y(TABS_Y), width, GALLERY_HIT + HIT_TABS);
	/* the search field at the right, on the tabs' row */
	ae_text_init(&search_text, "", 40, 0);
	memset(&search, 0, sizeof(search));
	search.placeholder = "Search settings";
	search.text = &search_text;
	ae_widget_field(d, right_x() - u(SEARCH_WIDTH_U), frame_y(TABS_Y), u(SEARCH_WIDTH_U), &search, 0x7C8F);
	ae_draw_rect(left, frame_y(TABS_Y) + tabs_height, right_x() - left, u(RULE_U), 0.0f, AE_COLOR_RULE);
	/* the list: DISPLAY's header over it, GRAPHICS's inside */
	y = top + ae_widget_group(d, left, top, width, "DISPLAY");
	context.density = d;
	context.rows = video_rows;
	context.hit_id = GALLERY_HIT + HIT_LIST;
	ae_widget_list(d, &gallery.list, left, y, width + u(24.0f), frame_y(LIST_BOTTOM) - y, list_item, &context,
		GALLERY_HIT + HIT_LIST);
	/* the help panel: FRAME RATE LIMIT's */
	help_rect.x = gallery.frame.rect.x + gallery.frame.rect.width * HELP_X;
	help_rect.y = top;
	help_rect.width = right_x() - help_rect.x;
	help_rect.height = frame_y(HELP_BOTTOM) - top;
	memset(&help, 0, sizeof(help));
	help.title = "Frame rate limit";
	help.body = "With V-Sync off, the most frames a second. AUTO is twice the display's refresh rate.";
	help.values = frame_rate_values;
	help.value_count = (short)(sizeof(frame_rate_values) / sizeof(frame_rate_values[0]));
	help.current = 5;
	help.default_value = "AUTO";
	help.changed_from = "AUTO";
	help.preview = 1;
	ae_widget_help(d, &help_rect, &help, GALLERY_HIT + HIT_HELP, &preview);
	if (preview.width > 0.0f)
	{
		float minor = ae_size_minor(d), text = ae_size_text(d);

		ae_draw_text_tracked(AE_FONT_BODY, minor * 0.8f, 0.12f, preview.x + u(16.0f), preview.y + u(16.0f), AE_ALIGN_LEFT,
			AE_COLOR_MUTED, "LIVE PREVIEW");
		/* (two literals: the 64-bit build's source rewrite takes "% l" in one for a printf length and drops the l) */
		ae_draw_text(AE_FONT_ROW, text * 0.85f, preview.x + preview.width * 0.5f,
			cap_top(AE_FONT_ROW, text * 0.85f, preview.y + preview.height * 0.5f), AE_ALIGN_CENTER, AE_COLOR_ACCENT,
			"165 FPS \xE2\x80\xA2 6.1 ms \xE2\x80\xA2 1%" " low 142");
	}
	prompts[0] = prompt_of(AE_BUTTON_A, "All values", AE_ACTION_ACCEPT, NULL);
	prompts[1] = prompt_of(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK, NULL);
	prompts[2] = prompt_of(AE_BUTTON_Y, "Reset row", AE_ACTION_Y, NULL);
	prompts[3] = prompt_of(AE_BUTTON_X, "Search", AE_ACTION_X, NULL);
	footer(prompts, 4, "Changes apply as you make them");
}

/* ---------- page 2: lobby (05) */

static const char *lobby_value(short row)
{
	switch (row)
	{
	case LOBBY_MAP: return map_values[gallery.map];
	case LOBBY_TYPE: return type_values[gallery.type];
	case LOBBY_NETWORK: return network_values[gallery.network];
	case LOBBY_PRIVACY: return privacy_values[gallery.privacy];
	default: return NULL;
	}
}

static void draw_lobby(void)
{
	struct ae_density const *d = &gallery.density;
	struct ae_prompt prompts[4];
	struct ae_roster_card cards[4];
	struct ae_rect bounds;
	float left = left_x(), width = gallery.frame.rect.width * LOBBY_ROWS_WIDTH, y = frame_y(200.0f / 1080.0f);
	float gap = u(d->metrics->gap), chips_height, roster_bottom = frame_y(ROSTER_BOTTOM);
	float roster_x = gallery.frame.rect.x + gallery.frame.rect.width * (1095.0f / 1920.0f), roster_y = y;
	float roster_width = right_x() - roster_x, inset = u(23.0f), card_y, box_y;
	short row, index;

	page_title("CUSTOM GAMES", "LOBBY \xE2\x80\xA2 LAN \xE2\x80\xA2 INVITE ONLY");
	/* the rows: an action, the value pickers, a disabled one */
	for (row = 0; row < LOBBY_ROWS; row++)
	{
		struct ae_row sample;

		memset(&sample, 0, sizeof(sample));
		sample.label = lobby_labels[row];
		sample.value = lobby_value(row);
		if (row == LOBBY_INVITE)
		{
			sample.flags |= AE_ROW_DISABLED;
			sample.reason = "ONLINE only";
		}
		if (row == gallery.lobby_focus)
			sample.flags |= AE_ROW_FOCUSED;
		gallery.lobby_rows[row].x = left;
		gallery.lobby_rows[row].y = y;
		gallery.lobby_rows[row].width = width;
		gallery.lobby_rows[row].height = ae_widget_row(d, left, y, width, &sample, GALLERY_HIT + HIT_ROWS, row);
		y += gallery.lobby_rows[row].height + gap;
	}
	/* the mode filter's chips and YOUR SETTINGS' page dots in a panel, each where it fits (not at 130 %: 09) */
	y += u(20.0f);
	{
		struct ae_rect rects[8];

		chips_height = ae_chips_layout(d, width, mode_chips, (short)(sizeof(mode_chips) / sizeof(mode_chips[0])), rects);
	}
	if (y + u(d->metrics->group_height) + chips_height <= content_bottom())
	{
		y += ae_widget_group(d, left, y, width, "MODE FILTER (Change Game Type rail)");
		y += ae_widget_chips(d, left, y, width, mode_chips, (short)(sizeof(mode_chips) / sizeof(mode_chips[0])),
			GALLERY_HIT + HIT_CHIPS) + u(28.0f);
	}
	box_y = y;
	if (box_y + u(110.0f) <= content_bottom())
	{
		ae_draw_rect(left, box_y, width, u(110.0f), u(d->metrics->corner), AE_COLOR_PANEL);
		ae_draw_outline(left, box_y, width, u(110.0f), u(d->metrics->corner), u(RULE_U), AE_COLOR_RULE);
		ae_widget_page_dots(d, left + width * 0.5f, box_y + u(26.0f), "YOUR SETTINGS", 6, 1, AE_COLOR_ACCENT,
			GALLERY_HIT + HIT_DOTS);
	}
	/* the roster: its header, the players that fit and the open card ("N more" for the rest) */
	ae_draw_rect(roster_x, roster_y, roster_width, roster_bottom - roster_y, u(d->metrics->corner), AE_COLOR_PANEL);
	ae_draw_outline(roster_x, roster_y, roster_width, roster_bottom - roster_y, u(d->metrics->corner), u(RULE_U),
		AE_COLOR_RULE);
	{
		float size = ae_size_minor(d), center = roster_y + u(29.0f);

		ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, roster_x + inset, cap_top(AE_FONT_BODY, size, center), AE_ALIGN_LEFT,
			AE_COLOR_MUTED, "PLAYERS  4 / 16");
		ae_draw_text(AE_FONT_BODY, size * 0.9f, roster_x + roster_width - inset, cap_top(AE_FONT_BODY, size * 0.9f, center),
			AE_ALIGN_RIGHT, AE_COLOR_MUTED, "LB / RB on your pad: team");
	}
	memset(cards, 0, sizeof(cards));
	cards[0].name = "Peter"; cards[0].slot = 1; cards[0].color = RED; cards[0].sub_line = "Keyboard and mouse \xE2\x80\xA2 this PC";
	cards[0].team_name = "RED"; cards[0].team_color = RED; cards[0].flags = AE_CARD_TEAM | AE_CARD_HOST;
	cards[1].name = "Player 2"; cards[1].slot = 2; cards[1].color = BLUE;
	cards[1].sub_line = "Controller 2 \xE2\x80\xA2 editing Your Settings";
	cards[1].team_name = "BLUE"; cards[1].team_color = BLUE; cards[1].flags = AE_CARD_TEAM | AE_CARD_EDITING;
	cards[2].name = "Guest"; cards[2].slot = 3; cards[2].color = GREEN; cards[2].sub_line = "Controller 3";
	cards[2].team_name = "RED"; cards[2].team_color = RED; cards[2].flags = AE_CARD_TEAM | AE_CARD_GUEST | AE_CARD_FOCUSED;
	cards[3].name = "Chief117"; cards[3].slot = 4; cards[3].color = YELLOW; cards[3].sub_line = NULL;
	cards[3].reason = "Disconnected \xE2\x80\xA2 12 s";
	cards[3].team_name = "BLUE"; cards[3].team_color = BLUE; cards[3].flags = AE_CARD_TEAM | AE_CARD_AWAY;
	card_y = roster_y + u(56.0f);
	{
		float pitch = u(d->metrics->card) + u(8.0f), room = roster_bottom - u(14.0f) - card_y;
		/* (the cards that fit with the open card under them; else as many as fit with an "N more" line) */
		short fit = (short)(room / pitch), shown = fit - 1 >= 4 ? 4 : (short)(fit - 1);

		if (shown < 0)
			shown = 0;
		for (index = 0; index < shown; index++)
			card_y += ae_widget_roster_card(d, roster_x + inset, card_y, roster_width - 2.0f * inset, &cards[index],
				GALLERY_HIT + HIT_CARDS, index) + u(8.0f);
		if (shown == 4)
			ae_widget_open_card(d, roster_x + inset, card_y, roster_width - 2.0f * inset, 0, GALLERY_HIT + HIT_CARDS);
		else
		{
			char more[32];
			float size = ae_size_minor(d);

			snprintf(more, sizeof(more), "\xE2\x96\xBC %d more", 5 - shown);
			ae_draw_text(AE_FONT_BODY, size, roster_x + roster_width * 0.5f, cap_top(AE_FONT_BODY, size, card_y + u(20.0f)),
				AE_ALIGN_CENTER, AE_COLOR_MUTED, more);
		}
	}
	/* the map and game type, the picture only where it fits (not at 130 %: 09) */
	{
		float map_y = roster_bottom + u(25.0f), title = u(28.0f), minor = ae_size_minor(d), picture = u(92.0f);
		float text_height = u(100.0f);

		if (map_y + picture + text_height > content_bottom())
			picture = 0.0f;
		if (map_y + picture + text_height <= content_bottom())
		{
			ae_draw_rect(roster_x, map_y, roster_width, picture + text_height, u(d->metrics->corner), AE_COLOR_PANEL);
			if (picture > 0.0f)
			{
				ae_draw_rect(roster_x, map_y, roster_width, picture, 0.0f, 0x4A5A3AC0u);
				ae_draw_text(AE_FONT_BODY, minor, roster_x + roster_width * 0.5f, cap_top(AE_FONT_BODY, minor,
					map_y + picture * 0.5f), AE_ALIGN_CENTER, AE_COLOR_MUTED, "map picture");
			}
			ae_draw_text(AE_FONT_ROW, title, roster_x + inset, cap_top(AE_FONT_ROW, title, map_y + picture + u(34.0f)),
				AE_ALIGN_LEFT, AE_COLOR_TITLE, map_values[gallery.map]);
			ae_draw_text(AE_FONT_ROW, minor, roster_x + inset, cap_top(AE_FONT_ROW, minor, map_y + picture + u(69.0f)),
				AE_ALIGN_LEFT, AE_COLOR_ACCENT, "AE FFA SLAY \xE2\x80\xA2 score 25 \xE2\x80\xA2 10 min");
		}
	}
	/* the open-state sample (preflight P15): PRIVACY's list drawn open, a picture: the real picker is NETWORK's */
	bounds = gallery.frame.rect;
	ae_widget_picker_sample(d, &gallery.lobby_rows[LOBBY_PRIVACY], &bounds, privacy_values, 3, gallery.privacy,
		(short)((gallery.privacy + 2) % 3), GALLERY_HIT + HIT_PICKER_SAMPLE);
	prompts[0] = prompt_of(AE_BUTTON_A, ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT, NULL);
	prompts[1] = prompt_of(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK, NULL);
	prompts[2] = prompt_of(AE_BUTTON_X, "Game options", AE_ACTION_X, NULL);
	prompts[3] = prompt_of(AE_BUTTON_Y, "Network", AE_ACTION_Y, NULL);
	footer(prompts, 4, NULL);
}

static void lobby_picked(short index, void *context)
{
	short row = *(short const *)context;
	const char *value = NULL;

	switch (row)
	{
	case LOBBY_MAP: gallery.map = index; value = map_values[index]; break;
	case LOBBY_TYPE: gallery.type = index; value = type_values[index]; break;
	case LOBBY_NETWORK: gallery.network = index; value = network_values[index]; break;
	case LOBBY_PRIVACY: gallery.privacy = index; value = privacy_values[index]; break;
	default: return;
	}
	platform_log("ae gallery: picked %s", value);
}

static void lobby_accept(short row)
{
	struct ae_picker_spec spec;

	memset(&spec, 0, sizeof(spec));
	switch (row)
	{
	case LOBBY_MAP: spec.values = map_values; spec.count = 5; spec.current = gallery.map; break;
	case LOBBY_TYPE: spec.values = type_values; spec.count = 4; spec.current = gallery.type; break;
	case LOBBY_NETWORK: spec.values = network_values; spec.count = 3; spec.current = gallery.network; break;
	case LOBBY_PRIVACY: spec.values = privacy_values; spec.count = 3; spec.current = gallery.privacy; break;
	case LOBBY_INVITE:
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	default:
		platform_log("ae gallery: item %s", lobby_labels[row]);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	}
	spec.row = gallery.lobby_rows[row];
	spec.bounds = gallery.frame.rect;
	spec.density = gallery.density;
	spec.picked = lobby_picked;
	spec.context = (void *)&indexes[row];
	ae_picker_open(&spec, AE_OWNER_ANY);
}

/* ---------- page 3: text entry (06) */

static void log_field(short field)
{
	if (field >= 0 && field < FIELDS)
		platform_log("ae gallery: field '%s'", gallery.texts[field].text);
}

/* the field's typing ended from outside (another field took the keys, a reset, the guard) */
static void field_done(int keep, void *context)
{
	(void)keep;
	log_field(*(short const *)context);
	gallery.editing = -1;
}

static void field_edit_begin(short field)
{
	if (field == FIELD_BASED_ON)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	gallery.edit.done = field_done;
	gallery.edit.context = (void *)&indexes[field];
	gallery.edit.holder = &gallery;
	ae_field_edit_begin(&gallery.edit, &gallery.texts[field]);
	/* (the whole text selected: typing replaces it) */
	ae_text_select_all(&gallery.texts[field]);
	gallery.editing = field;
	gallery.caret_since = ae_motion_now();
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void field_edit_end(int keep)
{
	short field = gallery.editing;

	if (field < 0)
		return;
	gallery.editing = -1;
	ae_field_edit_end(&gallery.edit, keep);
	ae_sound_request(keep ? AE_SOUND_FORWARD : AE_SOUND_BACK, 0);
	log_field(field);
}

static void keyboard_done(int keep, void *context)
{
	short field = *(short const *)context;

	(void)keep;
	gallery.keyboard_open = 0;
	platform_log("ae gallery: keyboard closed");
	/* (a physical key closed it into the field's own typing: that goes on) */
	if (gallery.edit.active && gallery.edit.text == &gallery.texts[field])
	{
		gallery.editing = field;
		gallery.caret_since = ae_motion_now();
		return;
	}
	log_field(field);
}

static void keyboard_open(short field)
{
	struct ae_keyboard_spec spec;

	if (field == FIELD_BASED_ON)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	memset(&spec, 0, sizeof(spec));
	spec.text = &gallery.texts[field];
	spec.field = gallery.wells[field];
	spec.bounds = gallery.frame.rect;
	spec.density = gallery.density;
	spec.done = keyboard_done;
	spec.context = (void *)&indexes[field];
	gallery.edit.done = field_done;
	gallery.edit.context = (void *)&indexes[field];
	gallery.edit.holder = &gallery;
	spec.edit = &gallery.edit;
	if (ae_keyboard_open(&spec, AE_OWNER_ANY))
	{
		gallery.keyboard_open = 1;
		platform_log("ae gallery: keyboard open");
	}
}

static void draw_text_entry(void)
{
	struct ae_density const *d = &gallery.density;
	struct ae_prompt prompts[3];
	struct ae_layout layout;
	float left = left_x(), width = gallery.frame.rect.width * FIELDS_WIDTH, y = frame_y(CONTENT_Y);
	short field;

	page_title("SAVE AS NEW", "GAME OPTIONS \xE2\x80\xA2 BASED ON AE FFA SLAY");
	ae_draw_current_layout(&layout);
	for (field = 0; field < FIELDS; field++)
	{
		struct ae_field sample;
		float height;

		memset(&sample, 0, sizeof(sample));
		sample.label = field_labels[field];
		sample.text = &gallery.texts[field];
		sample.caret_since = gallery.caret_since;
		if (field == FIELD_DESCRIPTION)
		{
			sample.placeholder = "Optional: one line";
			sample.flags |= AE_FIELD_HOVER;
		}
		if (field == FIELD_TAKEN)
		{
			sample.flags |= AE_FIELD_ERROR;
			sample.error = "A game type with this name exists";
		}
		if (field == FIELD_BASED_ON)
			sample.flags |= AE_FIELD_DISABLED;
		/* (the focused field shows its caret, as the mockup: typing or not) */
		if (field == gallery.field_focus)
			sample.flags = (sample.flags & ~(unsigned int)AE_FIELD_HOVER) | AE_FIELD_FOCUSED |
				(field != FIELD_BASED_ON ? AE_FIELD_EDITING : 0);
		/* (a field that doesn't fit above the footer is left out: BASED ON at 130 %, as 10) */
		{
			struct ae_rect well;

			ae_field_well(d, left, y, width, &sample, &well);
			if (well.y + well.height + (sample.error ? ae_size_minor(d) * 1.6f : 0.0f) > content_bottom())
			{
				gallery.wells[field].width = 0.0f;
				continue;
			}
		}
		height = ae_widget_field(d, left, y, width, &sample, (short)(0x7C80 + field));
		ae_field_well(d, left, y, width, &sample, &gallery.wells[field]);
		y += height + u(FIELD_GAP_U);
	}
	if (!gallery.field_logged)
	{
		gallery.field_logged = 1;
		platform_log("ae gallery: field 1 at %.0f,%.0f %.0fx%.0f", gallery.wells[0].x * layout.scale + layout.origin_x,
			gallery.wells[0].y * layout.scale + layout.origin_y, gallery.wells[0].width * layout.scale,
			gallery.wells[0].height * layout.scale);
	}
	/* the note beside, under where the keyboard opens */
	{
		char lines[3][160];
		float size = ae_size_minor(d), x = gallery.frame.rect.x + gallery.frame.rect.width * (934.0f / 1920.0f);
		int count = ae_wrap_text(AE_FONT_BODY, size, "A physical keyboard types at any time; the on-screen keyboard "
			"opens only when a pad presses A on a field.", right_x() - x - u(200.0f), lines, 3), index;

		for (index = 0; index < count; index++)
			/* (under where AE's keyboard opens beside the first field: its 5 rows, pads and strip are about 350 u) */
			ae_draw_text(AE_FONT_BODY, size, x, gallery.wells[0].y + u(390.0f) + (float)index * size * 1.45f, AE_ALIGN_LEFT,
				AE_COLOR_TEXT, lines[index]);
	}
	prompts[0] = prompt_of(AE_BUTTON_START, ae_string(AE_STR_SAVE), AE_ACTION_START, NULL);
	prompts[1] = prompt_of(AE_BUTTON_A, ae_string(AE_STR_TYPE), AE_ACTION_ACCEPT, NULL);
	prompts[2] = prompt_of(AE_BUTTON_B, ae_string(AE_STR_CANCEL), AE_ACTION_BACK, NULL);
	footer(prompts, 3, "Mouse: every key is clickable");
}

/* ---------- page 4: a dialog over Game Options (07) */

static void dialog_picked(short choice, void *context)
{
	(void)context;
	gallery.dialog_open = 0;
	platform_log("ae gallery: dialog %d", choice);
}

static void dialog_open_full(void)
{
	struct ae_dialog_spec spec;

	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "Unsaved changes";
	spec.body = "TEAM AE PRO SLAYER has 3 changes. Save them, save them as a new game type, or leave them?";
	spec.choices[0] = "SAVE";
	spec.choices[1] = "SAVE AS NEW";
	spec.choices[2] = "DISCARD CHANGES";
	spec.choice_count = 3;
	/* (the safe choice SAVE AS NEW, as the mockup; B / Esc, the cancel choice, picks it: the dialog ruling) */
	spec.safe_choice = 1;
	spec.cancel_choice = 1;
	spec.has_cancel = 1;
	spec.density = gallery.density;
	spec.bounds = gallery.frame.rect;
	spec.picked = dialog_picked;
	if (ae_dialog_open(&spec, AE_OWNER_ANY))
		gallery.dialog_open = 1;
}

static void draw_dialog_page(void)
{
	struct ae_density const *d = &gallery.density;
	struct ae_prompt prompts[3];
	float left = left_x(), width = gallery.frame.rect.width * LIST_WIDTH, y, gap = u(d->metrics->gap);
	short index;

	page_title("GAME OPTIONS", "EDITING TEAM AE PRO SLAYER \xE2\x80\xA2 3 CHANGES");
	gallery.options.tabs = options_tabs;
	gallery.options.count = (short)(sizeof(options_tabs) / sizeof(options_tabs[0]));
	gallery.options.active = 6;
	ae_widget_tabs(d, &gallery.options, left, frame_y(TABS_Y), right_x() - left, GALLERY_HIT + HIT_TABS);
	y = frame_y(CONTENT_Y);
	for (index = 0; index < (short)(sizeof(arena_rows) / sizeof(arena_rows[0])); index++)
	{
		struct ae_row row;

		row_of(&arena_rows[index], 0, &row);
		y += ae_widget_row(d, left, y, width, &row, GALLERY_HIT + HIT_ROWS, index) + gap;
	}
	prompts[0] = prompt_of(AE_BUTTON_START, ae_string(AE_STR_SAVE), AE_ACTION_START, ae_string(AE_STR_KEY_CTRL_S));
	prompts[1] = prompt_of(AE_BUTTON_Y, "Save as new", AE_ACTION_Y, ae_string(AE_STR_KEY_CTRL_SHIFT_S));
	prompts[2] = prompt_of(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK, NULL);
	footer(prompts, 3, NULL);
}

/* ---------- page 5: the main menu, the old menu off (01, spec 8) */

static void draw_main_menu(void)
{
	struct ae_density const *d = &gallery.density;
	struct ae_prompt prompts[3], switch_prompt;
	struct ae_roster_card card;
	float left = left_x(), kicker = u(CRUMB_U), title = u(MAIN_TITLE_U), y = frame_y(62.0f / 1080.0f);
	float pad = u(MAIN_PANEL_PAD_U), panel_y = u(MAIN_PANEL_Y_U), panel_width = u(MAIN_PANEL_WIDTH_U), x, gap = u(d->metrics->gap);
	float rows_y, panel_height, card_height;
	short index;

	/* the kicker, the title and its accent rule */
	ae_draw_text_tracked(AE_FONT_BODY, kicker, KICKER_TRACKING, left, y, AE_ALIGN_LEFT, AE_COLOR_MUTED,
		ae_string(AE_STR_KICKER));
	y += ae_draw_cap_height(AE_FONT_BODY, kicker) + u(22.0f);
	ae_draw_text(AE_FONT_TITLE, title, left, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, ae_string(AE_STR_AE_TITLE));
	y += ae_draw_cap_height(AE_FONT_TITLE, title) + u(18.0f);
	ae_draw_rect(left, y, u(ACCENT_RULE_WIDTH_U), u(ACCENT_RULE_U), 0.0f, AE_COLOR_ACCENT);
	/* the panel: the four items, a rule, the profile card */
	card_height = u(d->metrics->card);
	panel_height = pad * 2.0f + (float)MAIN_ITEMS * (ae_size_row(d) + gap) + u(14.0f) + card_height;
	ae_draw_rect(left, panel_y, panel_width, panel_height, u(d->metrics->corner), AE_COLOR_PANEL);
	ae_draw_outline(left, panel_y, panel_width, panel_height, u(d->metrics->corner), u(RULE_U), AE_COLOR_RULE);
	x = left + pad;
	rows_y = panel_y + pad;
	for (index = 0; index < MAIN_ITEMS; index++)
	{
		struct ae_row row;

		memset(&row, 0, sizeof(row));
		row.label = main_items[index];
		row.flags = index == gallery.main_focus ? AE_ROW_FOCUSED : 0;
		rows_y += ae_widget_row(d, x, rows_y, panel_width - 2.0f * pad, &row, GALLERY_HIT + HIT_ROWS, index) + gap;
	}
	ae_draw_rect(x, rows_y + u(4.0f), panel_width - 2.0f * pad, u(RULE_U), 0.0f, AE_COLOR_RULE);
	rows_y += u(14.0f);
	memset(&card, 0, sizeof(card));
	card.name = "Peter";
	card.slot = 1;
	card.color = RED;
	card.sub_line = "Profile \xE2\x80\xA2 Keyboard and mouse";
	ae_widget_roster_card(d, x, rows_y, panel_width - 2.0f * pad, &card, GALLERY_HIT + HIT_CARDS, 0);
	switch_prompt = prompt_of(AE_BUTTON_X, ae_string(AE_STR_SWITCH), AE_ACTION_X, NULL);
	ae_widget_prompts(d, x + (panel_width - 2.0f * pad) * 0.64f, rows_y + card_height * 0.5f, &switch_prompt, 1, NULL,
		left + panel_width - pad, -1, GALLERY_HIT + HIT_PROMPTS + 0x40);
	prompts[0] = prompt_of(AE_BUTTON_A, ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT, NULL);
	prompts[1] = prompt_of(AE_BUTTON_X, ae_string(AE_STR_PROFILE), AE_ACTION_X, NULL);
	prompts[2] = prompt_of(AE_BUTTON_B, ae_string(AE_STR_QUIT), AE_ACTION_BACK, NULL);
	footer(prompts, 3, NULL);
	/* the version, bottom right, muted 16 u */
	{
		float size = u(VERSION_U);

		ae_draw_text(AE_FONT_BODY, size, right_x(), cap_top(AE_FONT_BODY, size, gallery.frame.rect.height -
			u(FOOTER_ABOVE_U)), AE_ALIGN_RIGHT, AE_COLOR_MUTED, "ARENA EVOLVED 0.2.0-dev \xE2\x80\xA2 widget gallery");
	}
}

/* ---------- the split views */

static void view_rectangle(int view, struct ae_rect *rect)
{
	struct ae_layout layout;

	ae_draw_current_layout(&layout);
	rect->x = 0.0f;
	rect->y = 0.0f;
	rect->width = layout.width;
	rect->height = layout.height;
	if (gallery.views == 2)
	{
		rect->y = view ? layout.height * 0.5f : 0.0f;
		rect->height = layout.height * 0.5f;
	}
	else if (gallery.views == 4)
	{
		rect->x = view % 2 ? layout.width * 0.5f : 0.0f;
		rect->y = view / 2 ? layout.height * 0.5f : 0.0f;
		rect->width = layout.width * 0.5f;
		rect->height = layout.height * 0.5f;
	}
}

static struct sample_row const *view_rows(int page, short *count)
{
	switch (page)
	{
	case 1: *count = (short)(sizeof(settings_view_rows) / sizeof(settings_view_rows[0])); return settings_view_rows;
	case 2: *count = (short)(sizeof(training_view_rows) / sizeof(training_view_rows[0])); return training_view_rows;
	case 4: *count = (short)(sizeof(game_view_rows) / sizeof(game_view_rows[0])); return game_view_rows;
	default: *count = 0; return NULL;
	}
}

static void draw_view(int view)
{
	struct view_state *v = &gallery.view[view];
	struct ae_layout layout;
	struct ae_rect content, panel;
	struct ae_prompt prompts[3];
	float scale = ae_settings_ui_scale(), width_px, height_px, help_y, prompts_y, view_scale;
	short id = (short)(GALLERY_HIT + 0x100 + view * 16), count, index;
	struct sample_row const *rows = view_rows(v->page, &count);

	ae_draw_current_layout(&layout);
	view_rectangle(view, &v->view);
	width_px = v->view.width * layout.scale;
	height_px = v->view.height * layout.scale;
	ae_density_view(width_px, height_px, scale, &v->density);
	ae_view_panel_rect(width_px, height_px, scale, &panel);
	v->panel.x = v->view.x + panel.x / layout.scale;
	v->panel.y = v->view.y + panel.y / layout.scale;
	v->panel.width = panel.width / layout.scale;
	v->panel.height = panel.height / layout.scale;
	if (v->logged_width != width_px || v->logged_height != height_px || v->logged_scale != scale)
	{
		v->logged_width = width_px;
		v->logged_height = height_px;
		v->logged_scale = scale;
		platform_log("ae gallery: view %d page %d density VIEW s=%.2f panel=%.0fx%.0f text=%.1fpx minor=%.1fpx row=%.1fpx",
			view + 1, v->page, v->density.s, panel.width, panel.height, ae_size_text(&v->density) / v->density.pixel,
			ae_size_minor(&v->density) / v->density.pixel, ae_size_row(&v->density) / v->density.pixel);
	}
	ae_draw_view(v->view.x, v->view.y, v->view.width, v->view.height);
	ae_widget_view_panel(&v->density, width_px, height_px, (short)view, player_colors[view],
		view_page_names[v->page - 1], (short)(v->page - 1), 6, &content);
	view_scale = v->view.height / (float)AE_LAYOUT_HEIGHT;
	v->content.x = v->view.x + content.x * view_scale;
	v->content.y = v->view.y + content.y * view_scale;
	v->content.width = content.width * view_scale;
	v->content.height = content.height * view_scale;
	if (v->page == 1)
	{
		struct list_context context;

		context.density = &v->density;
		context.rows = rows;
		context.hit_id = id;
		ae_widget_list(&v->density, &v->list, content.x, content.y, content.width, content.height, list_item, &context, id);
	}
	else if (v->page == 3)
	{
		struct ae_field field;
		struct ae_rect well;

		memset(&field, 0, sizeof(field));
		field.label = "HUD PRESET NAME";
		field.text = &v->text;
		field.flags = AE_FIELD_FOCUSED | AE_FIELD_EDITING;
		ae_widget_field(&v->density, content.x, content.y, content.width, &field, (short)(id + 1));
		ae_field_well(&v->density, content.x, content.y, content.width, &field, &well);
		v->field.x = v->view.x + well.x * view_scale;
		v->field.y = v->view.y + well.y * view_scale;
		v->field.width = well.width * view_scale;
		v->field.height = well.height * view_scale;
	}
	else
	{
		float y = content.y, gap = v->density.metrics->gap * v->density.unit;

		for (index = 0; index < count; index++)
		{
			struct ae_row row;

			row_of(&rows[index], index == v->focus ? AE_ROW_FOCUSED : 0, &row);
			y += ae_widget_row(&v->density, content.x, y, content.width, &row, id, index) + gap;
		}
	}
	ae_view_panel_footer(&v->density, width_px, height_px, &help_y, &prompts_y);
	ae_widget_help_strip(&v->density, content.x, help_y, content.width, view_help[v->page - 1]);
	prompts[0] = prompt_of(AE_BUTTON_A, v->page == 4 ? ae_string(AE_STR_SELECT) : ae_string(AE_STR_CHANGE),
		AE_ACTION_ACCEPT, NULL);
	prompts[1] = prompt_of(AE_BUTTON_B, "Resume", AE_ACTION_BACK, NULL);
	prompts[2] = prompt_of(AE_BUTTON_LEFT_SHOULDER, ae_string(AE_STR_PAGES), AE_ACTION_TAB_PREVIOUS, NULL);
	ae_widget_prompts(&v->density, content.x, prompts_y, prompts, 3, NULL, content.x + content.width, -1, (short)(id + 2));
	v->drawn = 1;
}

/* a view's popover (MY HUD: AE's keyboard; GAME: a dialog), once its panel has been drawn */
static void view_popover(int view)
{
	struct view_state *v = &gallery.view[view];

	if (v->popover_opened || !v->drawn)
		return;
	v->popover_opened = 1;
	if (v->page == 3)
	{
		struct ae_keyboard_spec spec;

		memset(&spec, 0, sizeof(spec));
		spec.text = &v->text;
		spec.field = v->field;
		spec.bounds = v->panel;
		spec.density = v->density;
		spec.view = v->view;
		if (ae_keyboard_open(&spec, (short)view))
			platform_log("ae gallery: keyboard open");
	}
	else if (v->page == 4)
	{
		struct ae_dialog_spec spec;

		memset(&spec, 0, sizeof(spec));
		spec.kind = AE_DIALOG_CONFIRM;
		spec.title = "Leave the game?";
		spec.body = "Players 1-4 on this PC leave together.";
		spec.choices[0] = ae_string(AE_STR_STAY);
		spec.choices[1] = ae_string(AE_STR_LEAVE);
		spec.choice_count = 2;
		/* (B: STAY, the safe choice) */
		spec.cancel_choice = 0;
		spec.has_cancel = 1;
		spec.density = v->density;
		/* (the panel under its header: the page it hides, mockup 25) */
		spec.bounds = v->panel;
		spec.bounds.y = v->content.y;
		spec.bounds.height = v->panel.y + v->panel.height - v->content.y;
		/* (the whole panel goes dim, its header and page dots too: task 17) */
		spec.dim = v->panel;
		spec.view = v->view;
		spec.picked = dialog_picked;
		ae_dialog_open(&spec, (short)view);
	}
}

/* ---------- the screen */

static void gallery_draw(struct ae_screen *screen)
{
	int view;

	(void)screen;
	if (gallery.views > 1)
	{
		for (view = 0; view < gallery.views; view++)
			draw_view(view);
		ae_draw_view_full();
		return;
	}
	ae_draw_view_full();
	full_density();
	switch (gallery.page)
	{
	case 1: draw_lists(); break;
	case 2: draw_lobby(); break;
	case 3: draw_text_entry(); break;
	case 4: draw_dialog_page(); break;
	default: draw_main_menu(); break;
	}
	gallery.drawn = 1;
}

static void show_page(int page)
{
	if (gallery.editing >= 0)
		field_edit_end(0);
	gallery.page = page;
	gallery.dialog_shown = 0;
	platform_log("ae gallery: page %d", page);
}

static void gallery_update(struct ae_screen *screen)
{
	int view;

	(void)screen;
	if (gallery.views > 1)
	{
		for (view = 0; view < gallery.views; view++)
			view_popover(view);
		return;
	}
	/* (typing into a field: this frame's keys) */
	if (gallery.editing >= 0 && gallery.edit.active && !ae_field_edit_keys(&gallery.edit))
		ae_sound_request(AE_SOUND_FAILURE, 0);
	/* (page 4's dialog: the page's own, opened as it shows, once drawn: it is placed in the page's frame) */
	if (gallery.page == 4 && gallery.drawn && !gallery.dialog_shown)
	{
		gallery.dialog_shown = 1;
		dialog_open_full();
	}
}

/* up / down on a list of count items without wrapping; returns whether it moved */
static int step_focus(short *focus, short count, short direction, int repeat)
{
	short before = *focus;

	*focus = ae_value_step(count, *focus, direction);
	sound_cursor(*focus != before, repeat);
	if (*focus != before)
		log_focus(*focus, count);
	return *focus != before;
}

static int handle_view(struct ae_event const *event)
{
	int view = event->player >= 0 && event->player < gallery.views ? event->player : 0;
	struct view_state *v = &gallery.view[view];
	short count;

	view_rows(v->page, &count);
	if (event->action == AE_ACTION_BACK)
		return 0;
	if (v->page == 1)
	{
		short before = v->list.list.focus;

		if (ae_list_view_event(&v->list, event) && v->list.list.focus != before)
			log_focus(v->list.list.focus, v->list.list.count);
		return 1;
	}
	if (event->action == AE_ACTION_UP || event->action == AE_ACTION_DOWN)
		step_focus(&v->focus, count, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
	return 1;
}

static int gallery_handle(struct ae_screen *screen, struct ae_event const *event)
{
	(void)screen;
	if (gallery.views > 1)
		return handle_view(event);
	/* typing into a field: Enter (START while typing) keeps, Esc (B) restores; the rest is the typing's */
	if (gallery.editing >= 0)
	{
		if (event->action == AE_ACTION_START || event->action == AE_ACTION_ACCEPT)
			field_edit_end(1);
		else if (event->action == AE_ACTION_BACK)
			field_edit_end(0);
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		show_page(ae_page_step(PAGES, (short)(gallery.page - 1), event->action == AE_ACTION_TAB_NEXT ? 1 : -1) + 1);
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		return 1;
	case AE_ACTION_BACK:
		return 0;
	default:
		break;
	}
	switch (gallery.page)
	{
	case 1:
	{
		short before = gallery.list.list.focus;
		short direction = event->action == AE_ACTION_UP || event->action == AE_ACTION_PAGE_UP ? -1 : 1;

		if (event->action == AE_ACTION_ACCEPT)
		{
			platform_log("ae gallery: item %s", video_rows[gallery.list.list.focus].label);
			ae_sound_request(AE_SOUND_FORWARD, 0);
			return 1;
		}
		if (ae_list_view_event(&gallery.list, event))
		{
			skip_groups(&gallery.list.list, video_rows, direction);
			if (gallery.list.list.focus != before)
				log_focus(gallery.list.list.focus, VIDEO_ROWS);
		}
		return 1;
	}
	case 2:
		if (event->action == AE_ACTION_UP || event->action == AE_ACTION_DOWN)
			step_focus(&gallery.lobby_focus, LOBBY_ROWS, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		else if (event->action == AE_ACTION_ACCEPT)
			lobby_accept(gallery.lobby_focus);
		return 1;
	case 3:
		if (event->action == AE_ACTION_UP || event->action == AE_ACTION_DOWN)
		{
			if (step_focus(&gallery.field_focus, FIELDS, event->action == AE_ACTION_UP ? -1 : 1, event->repeat))
				gallery.caret_since = ae_motion_now();
		}
		else if (event->action == AE_ACTION_ACCEPT)
		{
			/* (P17: Enter starts typing, a pad's A opens AE's keyboard) */
			if (event->device == AE_DEVICE_KEYBOARD_MOUSE)
				field_edit_begin(gallery.field_focus);
			else
				keyboard_open(gallery.field_focus);
		}
		return 1;
	case 4:
		if (event->action == AE_ACTION_ACCEPT && !gallery.dialog_open)
			dialog_open_full();
		return 1;
	default:
		if (event->action == AE_ACTION_UP || event->action == AE_ACTION_DOWN)
			step_focus(&gallery.main_focus, MAIN_ITEMS, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		else if (event->action == AE_ACTION_ACCEPT || event->action == AE_ACTION_X)
		{
			platform_log("ae gallery: item %s", event->action == AE_ACTION_X ? "PROFILE" : main_items[gallery.main_focus]);
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		return 1;
	}
}

static void gallery_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct ae_hit hit;
	struct ae_event event;
	int over;

	(void)screen;
	if (gallery.views > 1)
		return;
	over = ae_hit_at(pointer->x, pointer->y, 0, &hit);
	/* (a click on a prompt: its action; B closes the gallery as the stack's BACK would) */
	if (ae_prompts_pointer(gallery.footer_prompts, gallery.footer_count, pointer, GALLERY_HIT + HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK && gallery.editing < 0)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			gallery_handle(screen, &event);
		return;
	}
	if (gallery.page == 1)
	{
		short before = gallery.list.list.focus, item = ae_list_view_pointer(&gallery.list, pointer, GALLERY_HIT + HIT_LIST);

		skip_groups(&gallery.list.list, video_rows, 1);
		if (gallery.list.list.focus != before)
			log_focus(gallery.list.list.focus, VIDEO_ROWS);
		if (item >= 0 && !video_rows[item].group)
		{
			platform_log("ae gallery: item %s", video_rows[item].label);
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		if (ae_tabs_pointer(&gallery.tabs, pointer, GALLERY_HIT + HIT_TABS))
			return;
		return;
	}
	if (!over)
		return;
	if (gallery.page == 2 && hit.id == GALLERY_HIT + HIT_ROWS && hit.index >= 0 && hit.index < LOBBY_ROWS)
	{
		if (pointer->moved && hit.index != gallery.lobby_focus)
		{
			gallery.lobby_focus = hit.index;
			log_focus(hit.index, LOBBY_ROWS);
		}
		if (pointer->left_clicks)
			lobby_accept(hit.index);
	}
	else if (gallery.page == 3 && hit.id >= 0x7C80 && hit.id < 0x7C80 + FIELDS && pointer->left_clicks)
	{
		/* (a click on a field starts typing into it) */
		if (gallery.editing >= 0)
			field_edit_end(1);
		gallery.field_focus = (short)(hit.id - 0x7C80);
		field_edit_begin(gallery.field_focus);
	}
	else if (gallery.page == 5 && hit.id == GALLERY_HIT + HIT_ROWS && hit.index >= 0 && hit.index < MAIN_ITEMS)
	{
		if (pointer->moved && hit.index != gallery.main_focus)
		{
			gallery.main_focus = hit.index;
			log_focus(hit.index, MAIN_ITEMS);
		}
		if (pointer->left_clicks)
		{
			event.player = 0;
			event.action = AE_ACTION_ACCEPT;
			event.device = AE_DEVICE_KEYBOARD_MOUSE;
			event.repeat = 0;
			gallery_handle(screen, &event);
		}
	}
}

static void gallery_leave(struct ae_screen *screen)
{
	(void)screen;
	if (gallery.editing >= 0)
		field_edit_end(0);
	platform_log("ae gallery: closed");
}

static struct ae_screen_class const gallery_class =
{
	.name = "gallery", .leave = gallery_leave, .handle = gallery_handle, .draw = gallery_draw,
	.pointer = gallery_pointer, .update = gallery_update,
};

int ae_screen_gallery_open(int value)
{
	int view;

	if (!(value >= 11 && value <= 15) && value != 21 && value != 23 && value != 41)
		return 0;
	memset(&gallery, 0, sizeof(gallery));
	gallery.value = value;
	gallery.views = value == 21 || value == 23 ? 2 : value == 41 ? 4 : 1;
	gallery.page = value >= 11 && value <= 15 ? value - 10 : 1;
	gallery.editing = -1;
	gallery.lobby_focus = 0;
	gallery.network = 1;
	gallery.privacy = 2;
	gallery.main_focus = 1;
	gallery.logged_width = -1.0f;
	ae_list_view_init(&gallery.list, VIDEO_ROWS, 10);
	ae_list_set_focus(&gallery.list.list, VIDEO_FIRST_FOCUS);
	gallery.list.previous_focus = VIDEO_FIRST_FOCUS;
	gallery.tabs.active = 2;
	ae_text_init(&gallery.texts[FIELD_NAME], "SLAYER PRO", 11, 1);
	ae_text_init(&gallery.texts[FIELD_DISPLAY], "Team AE Pro Slayer", 32, 0);
	ae_text_init(&gallery.texts[FIELD_DESCRIPTION], "", 64, 0);
	ae_text_init(&gallery.texts[FIELD_TAKEN], "AE FFA SLAY", 11, 1);
	ae_text_init(&gallery.texts[FIELD_BASED_ON], "AE FFA SLAY", 11, 1);
	for (view = 0; view < VIEWS_MAXIMUM; view++)
	{
		struct view_state *v = &gallery.view[view];

		v->page = value == 23 ? view + 3 : view + 1;
		v->logged_width = -1.0f;
		ae_list_view_init(&v->list, (short)(sizeof(settings_view_rows) / sizeof(settings_view_rows[0])), 6);
		/* (the focus on LOOK SENSITIVITY, as mockup 26: the list at its top) */
		ae_list_set_focus(&v->list.list, 1);
		v->list.previous_focus = 1;
		ae_text_init(&v->text, "PRO HUD", 11, 1);
	}
	if (!ae_ui_push(&gallery_class, AE_OWNER_ANY, &gallery))
		return 0;
	platform_log("ae gallery: open (value %d)", value);
	/* (an AE screen that replaces the menus: upstream's closed) */
	ae_ui_replace_menus();
	(void)ae_settings_ui_scale();
	return 1;
}
