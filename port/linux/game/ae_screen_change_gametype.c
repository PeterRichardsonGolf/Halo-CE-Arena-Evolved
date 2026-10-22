/*
AE_SCREEN_CHANGE_GAMETYPE.C

AE's CHANGE GAMETYPE screen (ae_screen_change_gametype.h; the owner's v2 decisions of 2026-10-22, mockups
~/src/halo-test/mockups-037/change-gametype/*-v2-*.png and SPEC.md). CHANGE MAP's frame (ae_screen_change_map.c):
the title and the count top right; the mode filters' chips between LB and RB, the search field beside them over the
panel; the list (RECENT and FAVOURITES open, then the folders, one open in place; its header pinned on the list's
top once it scrolls off); the panel (the full name and its mode badge, folder . score to win . time, the description,
RULES, PLAYS WELL ON's maps, the maps with the mode and the name older machines list). Positions are fractions of the
frame, sizes u.

A row is a battle rifle PAIR when there is one: its label the name without the BR word, its value the start weapon of
the one shown (PISTOL START, BR START, AR START ...), grey unless focused; left / right on an AE row with a pair toggle
it. RECENT's rows show the start that was used. RECENT, FAVOURITES and search rows carry their folder's name.

Input (anyone's): up / down move; A opens or closes a folder, or picks a gametype (the caller's, or the GAME OPTIONS
edit's) and closes; right opens a folder, left closes the open one (on a pair: left / right its start); LB / RB (Q / E)
the filter; LT / RT (PgUp / PgDn) the next folder, opened; Y FAVOURITE (this session's); X (Ctrl+F) the search: typing
on a keyboard (Enter keeps it, Esc clears it), AE's keyboard on a pad; B clears a search, else back. The pointer:
hover, a click on a row as A, on its arrows the start, on a chip its filter, on the search field typing.

Its log: "ae change gametype: open (filter <name>)", "filter <name>: N of M", "folder <name> open|closed",
"focus '<stored>'" / "focus folder <name>", "start '<stored>'", "search '<text>': N", "favourite ...", "pick
'<stored>'", the glue's lines, "closed".
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_gametype_picker.h"
#include "ae_glue_gametype_picker.h"
#include "ae_glue_setup_maps.h"
#include "ae_glue_setup_options.h"
#include "ae_hooks.h"
#include "ae_motion.h"
#include "ae_screen_change_gametype.h"
#include "ae_setup_maps.h"
#include "ae_setup_options.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	GAMETYPE_HIT = 0x7D00,
	HIT_LIST = GAMETYPE_HIT + 1, HIT_CHIPS, HIT_PROMPTS, HIT_PLAYS, HIT_SEARCH, HIT_PINNED,
	ROWS_MAXIMUM = 640,
	PROMPTS = 9,
	PLAYS_MAXIMUM = 4,
	FACTS_PER_FRAME = 4
};

/* the frame's positions (fractions of 1920 x 1080 at 100 %): CHANGE MAP's */
#define TITLE_Y (70.0f / 1080.0f)
#define CHIPS_Y (150.0f / 1080.0f)
#define CONTENT_Y (238.0f / 1080.0f)
#define LIST_WIDTH 0.50f
#define PANEL_X (1143.0f / 1920.0f)
/* sizes, u */
#define TITLE_U 60.0f
#define CRUMB_U 20.0f
#define CRUMB_TRACKING 0.09f
#define RULE_U 1.0f
#define GLYPH_GAP_U 16.0f
#define INDENT_U 28.0f
#define CHEVRON_U 7.0f
#define PANEL_GAP_U 18.0f
#define NAME_SCALE 1.5f
#define FOOTER_ABOVE_U 62.0f
#define FOOTER_ROOM_U 40.0f
#define TRACK_STRIP_U 24.0f
/* (v2: the badge 28 u with 10 u sides, the facts' dots 24 u apart, the search field 42 u: the mockups' clearance
check's; the badge 30 u on the real fonts, whose capitals sit a pixel low in 28 u: 7 px under them) */
#define BADGE_HEIGHT_U 30.0f
#define BADGE_SIDE_U 10.0f
#define BADGE_GAP_U 14.0f
#define DOT_PITCH_U 24.0f
#define SEARCH_HEIGHT_U 42.0f
#define SEARCH_PAD_U 20.0f
/* (the X in the field: 24 u, 9 px above and under it in the 42 u field) */
#define SEARCH_GLYPH_U 24.0f
#define RING_U 2.0f
#define CARET_U 2.0f
#define TAG_GAP_U 24.0f
#define RULES_VALUE_U 190.0f
#define FACTS_LINE_U 40.0f

static struct ae_gtp gtp;
static struct ae_setup_maps maps;

static struct
{
	struct ae_density density;
	struct ae_frame frame;
	int filter, open_folder;
	struct ae_gtp_row rows[ROWS_MAXIMUM];
	short count;
	struct ae_gtp_row focus_row, logged_row;
	struct ae_list_view list;
	struct ae_prompt prompts[PROMPTS];
	short prompt_count;
	struct ae_chip chips[AE_GTP_FILTERS];
	char chip_labels[AE_GTP_FILTERS][32];
	short facts_left;
	short current;
	/* the search: its text, typed (a keyboard) or AE's keyboard (a pad); the query the rows were made for */
	struct ae_text search;
	struct ae_field_edit edit;
	int typing, keyboard_open;
	unsigned long caret_since;
	char query[AE_GTP_QUERY];
	struct ae_rect well;
	/* (CREATE GAME's) a pick told back instead of taken here */
	void (*picked)(long profile);
} screen;

static struct ae_screen_class const gametype_class;

/* ---------- helpers */

static float u(float spec_units)
{
	return spec_units * screen.frame.unit;
}

static float left_x(void)
{
	return screen.frame.rect.x + screen.frame.margin;
}

static float right_x(void)
{
	return screen.frame.rect.x + screen.frame.rect.width - screen.frame.margin;
}

static float panel_x(void)
{
	return screen.frame.rect.x + screen.frame.rect.width * PANEL_X;
}

static float frame_y(float fraction)
{
	return ae_frame_y(&screen.frame, fraction);
}

static float cap_top(int font, float size, float center_y)
{
	return center_y - ae_draw_cap_height(font, size) * 0.5f;
}

static struct ae_prompt prompt_of(int button, const char *label, int action, int pair)
{
	struct ae_prompt prompt;

	prompt.button = button;
	prompt.label = label;
	prompt.key = NULL;
	prompt.action = action;
	prompt.pair = pair;
	return prompt;
}

/* the glyphs' font of the device the prompts show (ae_ui_last_device: the footer's), so the chips' ends and the
search's X match the footer */
static int device_font(void)
{
	switch (ae_ui_last_device())
	{
	case AE_DEVICE_XBOX: return AE_FONT_XBOX;
	case AE_DEVICE_PLAYSTATION: return AE_FONT_PLAYSTATION;
	case AE_DEVICE_NINTENDO: return AE_FONT_NINTENDO;
	default: break;
	}
	return AE_FONT_KEYBOARD;
}

static void full_density(void)
{
	struct ae_layout layout;
	float scale = ae_settings_ui_scale();

	ae_draw_current_layout(&layout);
	ae_density_full(layout.height * layout.scale, scale, &screen.density);
	ae_frame_compute(layout.width, scale, &screen.frame);
}

static int searching(void)
{
	return screen.query[0] != 0;
}

static struct ae_gtp_row no_row(void)
{
	struct ae_gtp_row row = { -1, -1 };

	return row;
}

static struct ae_gtp_row focused_row(void)
{
	short focus = screen.list.list.focus;

	return focus >= 0 && focus < screen.count ? screen.rows[focus] : no_row();
}

static int same_row(struct ae_gtp_row a, struct ae_gtp_row b)
{
	return a.folder == b.folder && a.gametype == b.gametype;
}

static short index_of_row(struct ae_gtp_row row)
{
	short index;

	for (index = 0; index < screen.count; index++)
	{
		if (same_row(screen.rows[index], row))
			return index;
	}
	return -1;
}

/* a folder that stays open (RECENT, FAVOURITES) */
static int always_open(int folder)
{
	return folder == AE_GTP_FOLDER_RECENT || folder == AE_GTP_FOLDER_FAVOURITES;
}

/* the rows again (the filter, the open folder, the search, a favourite): the focus stays on its row, else on its
folder's header, else the first */
static void refresh(void)
{
	struct ae_gtp_row row = screen.focus_row;
	short index;

	screen.count = ae_gtp_rows(&gtp, screen.filter, screen.query, screen.open_folder, screen.rows, ROWS_MAXIMUM);
	ae_list_set_count(&screen.list.list, screen.count);
	index = index_of_row(row);
	if (index < 0 && row.gametype >= 0 && !searching())
		index = ae_gtp_header_row(screen.rows, screen.count, row.folder);
	ae_list_set_focus(&screen.list.list, index < 0 ? 0 : index);
	screen.focus_row = focused_row();
	if (!same_row(screen.focus_row, screen.logged_row) && screen.count)
	{
		short gametype = ae_gtp_row_gametype(&gtp, screen.focus_row);

		screen.logged_row = screen.focus_row;
		if (gametype < 0)
			platform_log("ae change gametype: focus folder %s", ae_gtp_folder_name(screen.focus_row.folder));
		else
			platform_log("ae change gametype: focus '%s' (%s)", gtp.gametypes[gametype].stored,
				gtp.gametypes[gametype].start);
	}
}

static void focus_taken(void)
{
	screen.focus_row = focused_row();
	refresh();
}

static void folder_set(int folder)
{
	int was = screen.open_folder;

	screen.open_folder = folder;
	if (was >= 0 && was != folder)
		platform_log("ae change gametype: folder %s closed", ae_gtp_folder_name(was));
	if (folder >= 0)
		platform_log("ae change gametype: folder %s open", ae_gtp_folder_name(folder));
	refresh();
}

static void filter_set(int filter)
{
	screen.filter = (filter % AE_GTP_FILTERS + AE_GTP_FILTERS) % AE_GTP_FILTERS;
	refresh();
	platform_log("ae change gametype: filter %s: %d of %d", ae_gtp_filter_name(screen.filter),
		ae_gtp_count(&gtp, screen.filter, screen.query), ae_gtp_count(&gtp, AE_GTF_ALL, screen.query));
}

/* the search's text as the rows' query, when it changed */
static void query_follow(void)
{
	if (!strncmp(screen.query, screen.search.text, sizeof(screen.query) - 1))
		return;
	snprintf(screen.query, sizeof(screen.query), "%s", screen.search.text);
	/* (a new search starts at its first result; a cleared one goes back to the row it was on) */
	if (searching())
		screen.focus_row = no_row();
	refresh();
	platform_log("ae change gametype: search '%s': %d", screen.query, ae_gtp_count(&gtp, screen.filter, screen.query));
}

static void search_clear(void)
{
	ae_text_init(&screen.search, "", AE_GTP_QUERY - 1, 0);
	query_follow();
}

/* the focus's folder header scrolled to the list's top (as far as the list goes) */
static void show_row_at_top(short row)
{
	struct ae_list *list = &screen.list.list;
	short last_first = list->count > list->rows ? (short)(list->count - list->rows) : 0;

	ae_list_set_focus(list, row);
	list->first = row < last_first ? row : last_first;
	screen.focus_row = focused_row();
}

/* A on a row (or a click) */
static void accept(struct ae_gtp_row row)
{
	short gametype = ae_gtp_row_gametype(&gtp, row);

	if (row.folder < 0)
		return;
	if (row.gametype < 0)
	{
		if (always_open(row.folder))
			return;
		folder_set(screen.open_folder == row.folder ? -1 : row.folder);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	}
	if (gametype < 0)
		return;
	ae_gtp_remember_pick(&gtp, gametype);
	if (screen.picked)
	{
		platform_log("ae change gametype: pick '%s' %s (for the caller)", gtp.gametypes[gametype].stored,
			gtp.gametypes[gametype].start);
		screen.picked(gtp.gametypes[gametype].profile);
	}
	else
	{
		platform_log("ae change gametype: pick '%s' %s", gtp.gametypes[gametype].stored, gtp.gametypes[gametype].start);
		if (!ae_setup_edit_begin(gtp.gametypes[gametype].profile))
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_ui_pop();
}

/* left / right on a pair: the other start */
static int toggle(struct ae_gtp_row row)
{
	short gametype;

	if (!ae_gtp_has_arrows(&gtp, ae_gtp_row_gametype(&gtp, row)) || !ae_gtp_toggle(&gtp, row))
		return 0;
	/* (RECENT's entry swapped: the rows hold the new one) */
	if (row.folder == AE_GTP_FOLDER_RECENT)
	{
		short index = screen.list.list.focus;

		screen.count = ae_gtp_rows(&gtp, screen.filter, screen.query, screen.open_folder, screen.rows, ROWS_MAXIMUM);
		if (index >= 0 && index < screen.count)
			screen.focus_row = screen.rows[index];
		refresh();
	}
	gametype = ae_gtp_row_gametype(&gtp, screen.focus_row);
	if (gametype >= 0)
		platform_log("ae change gametype: start '%s' %s", gtp.gametypes[gametype].stored, gtp.gametypes[gametype].start);
	ae_sound_request(AE_SOUND_CURSOR, 0);
	return 1;
}

/* ---------- the search's typing */

static void typing_done(int keep, void *context)
{
	(void)keep;
	(void)context;
	screen.typing = 0;
}

static void typing_begin(void)
{
	screen.edit.done = typing_done;
	screen.edit.context = NULL;
	screen.edit.holder = &screen;
	ae_field_edit_begin(&screen.edit, &screen.search);
	ae_text_end(&screen.search, 0);
	screen.typing = 1;
	screen.caret_since = ae_motion_now();
	platform_log("ae change gametype: typing");
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* Enter keeps the search (the list takes the keys again), Esc clears it */
static void typing_end(int keep)
{
	if (!screen.typing)
		return;
	screen.typing = 0;
	ae_field_edit_end(&screen.edit, 1);
	if (!keep)
		search_clear();
	ae_sound_request(keep ? AE_SOUND_FORWARD : AE_SOUND_BACK, 0);
}

static void keyboard_done(int keep, void *context)
{
	(void)context;
	screen.keyboard_open = 0;
	/* (a physical key closed it into the field's own typing: that goes on) */
	if (screen.edit.active && screen.edit.text == &screen.search)
	{
		screen.typing = 1;
		screen.caret_since = ae_motion_now();
		return;
	}
	(void)keep;
	query_follow();
}

static void keyboard_open(void)
{
	struct ae_keyboard_spec spec;

	memset(&spec, 0, sizeof(spec));
	spec.text = &screen.search;
	spec.field = screen.well;
	spec.bounds = screen.frame.rect;
	spec.density = screen.density;
	spec.done = keyboard_done;
	screen.edit.done = typing_done;
	screen.edit.context = NULL;
	screen.edit.holder = &screen;
	spec.edit = &screen.edit;
	if (ae_keyboard_open(&spec, AE_OWNER_ANY))
	{
		screen.keyboard_open = 1;
		platform_log("ae change gametype: keyboard open");
	}
}

static void search_begin(int device)
{
	if (device == AE_DEVICE_KEYBOARD_MOUSE)
		typing_begin();
	else
		keyboard_open();
}

/* ---------- drawing */

static void chevron(float x, float center_y, int open, unsigned int color)
{
	float c = u(CHEVRON_U), thickness = u(2.0f);

	if (open)
	{
		ae_draw_line(x - c, center_y - c * 0.5f, x, center_y + c * 0.5f, thickness, color);
		ae_draw_line(x, center_y + c * 0.5f, x + c, center_y - c * 0.5f, thickness, color);
	}
	else
	{
		ae_draw_line(x - c * 0.5f, center_y - c, x + c * 0.5f, center_y, thickness, color);
		ae_draw_line(x + c * 0.5f, center_y, x - c * 0.5f, center_y + c, thickness, color);
	}
}

/* a folder's header row */
static void header_row(float x, float y, float width, int folder, unsigned int flags, short item)
{
	struct ae_row row;
	float pad = u(screen.density.metrics->pad), minor = ae_size_minor(&screen.density);
	float center = y + ae_size_row(&screen.density) * 0.5f;
	int on_bar = (flags & AE_ROW_ON_BAR) != 0;
	char label[48], right[16];

	memset(&row, 0, sizeof(row));
	row.flags = flags;
	/* (room at the left for the chevron) */
	snprintf(label, sizeof(label), "     %s", ae_gtp_folder_name(folder));
	row.label = label;
	ae_widget_row(&screen.density, x, y, width, &row, item < 0 ? HIT_PINNED : HIT_LIST, item);
	chevron(x + pad + u(CHEVRON_U), center, always_open(folder) || folder == screen.open_folder,
		on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT);
	snprintf(right, sizeof(right), "%d", ae_gtp_folder_count(&gtp, folder, screen.filter, screen.query));
	ae_draw_text(AE_FONT_BODY, minor, x + width - pad, cap_top(AE_FONT_BODY, minor, center), AE_ALIGN_RIGHT,
		on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED, right);
}

/* a gametype's row: its label (the search's letters in the accent), and on the right its start (grey unless focused;
with arrows on a focused pair), after its folder's name in the flat folders (RECENT, FAVOURITES, a search) */
static void gametype_row(float x, float y, float width, struct ae_gtp_row where, unsigned int flags, short item)
{
	struct ae_density const *d = &screen.density;
	struct ae_row row;
	short gametype = ae_gtp_row_gametype(&gtp, where);
	struct ae_gtp_gametype const *g = &gtp.gametypes[gametype];
	float pad = u(d->metrics->pad), minor = ae_size_minor(d), text = ae_size_text(d), indent = u(INDENT_U);
	float center = y + ae_size_row(d) * 0.5f, right, room;
	int on_bar = (flags & AE_ROW_ON_BAR) != 0, focused = (flags & AE_ROW_FOCUSED) != 0;
	int arrows = focused && ae_gtp_has_arrows(&gtp, gametype);
	int flat = searching() || always_open(where.folder);
	int favourite = gtp.gametypes[ae_gtp_base(&gtp, gametype)].favourite;
	unsigned int ink = on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED;
	char label[160];
	int cut = 0, match;

	x += indent;
	width -= indent;
	memset(&row, 0, sizeof(row));
	row.flags = flags;
	right = x + width - pad;
	if (arrows)
	{
		/* (the row's own value: the start between its arrows, as a settings row) */
		struct ae_row_layout layout;

		row.label = g->row;
		row.value = g->start;
		ae_row_layout(d, width, &row, &layout);
		snprintf(label, sizeof(label), "%s", layout.label);
		cut = layout.label_cut;
		ae_widget_row(d, x, y, width, &row, HIT_LIST, item);
	}
	else
	{
		float value_width = ae_draw_text_width(AE_FONT_BODY, minor, g->start), tag_width = 0.0f;

		right -= value_width;
		if (flat)
		{
			const char *tag = ae_gtp_folder_name(g->folder);

			tag_width = ae_draw_text_width(AE_FONT_BODY, minor, tag);
			right -= u(TAG_GAP_U) + tag_width;
		}
		if (favourite)
			right -= u(14.0f);
		room = right - u(TAG_GAP_U) - (x + pad);
		cut = ae_fit_text(AE_FONT_ROW, text, g->row, room, label, sizeof(label));
		row.label = label;
		ae_widget_row(d, x, y, width, &row, HIT_LIST, item);
		/* the start, then (before it) the folder's name and the favourite's dot */
		right = x + width - pad;
		ae_draw_text(AE_FONT_BODY, minor, right, cap_top(AE_FONT_BODY, minor, center), AE_ALIGN_RIGHT, ink, g->start);
		right -= value_width;
		if (flat)
		{
			right -= u(TAG_GAP_U);
			ae_draw_text(AE_FONT_BODY, minor, right, cap_top(AE_FONT_BODY, minor, center), AE_ALIGN_RIGHT, ink,
				ae_gtp_folder_name(g->folder));
			right -= tag_width;
		}
		if (favourite)
			ae_widget_separator(d, right - u(14.0f), center, 8.0f, on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT);
	}
	/* the search's letters in the accent (not on the bar: its text is dark; not in a cut label) */
	match = searching() && !on_bar && !cut ? ae_gtp_match(label, screen.query) : -1;
	if (match >= 0)
	{
		char before[160], part[AE_GTP_QUERY];
		size_t length = strlen(screen.query);

		snprintf(before, sizeof(before), "%.*s", match, label);
		snprintf(part, sizeof(part), "%.*s", (int)length, label + match);
		ae_draw_text(AE_FONT_ROW, text, x + pad + ae_draw_text_width(AE_FONT_ROW, text, before),
			cap_top(AE_FONT_ROW, text, center), AE_ALIGN_LEFT, AE_COLOR_ACCENT, part);
	}
}

static void list_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct ae_gtp_row where = screen.rows[item];

	(void)context;
	(void)height;
	if (where.gametype < 0)
		header_row(x, y, width, where.folder, flags, item);
	else
		gametype_row(x, y, width, where, flags, item);
}

static void draw_header(void)
{
	char crumb[96], query[24];
	float size = u(TITLE_U), top = frame_y(TITLE_Y), crumb_size = u(CRUMB_U), middle;
	short shown = ae_gtp_count(&gtp, screen.filter, screen.query);
	int index;

	middle = top + ae_draw_cap_height(AE_FONT_TITLE, size) * 0.5f;
	ae_draw_text(AE_FONT_TITLE, size, left_x(), top, AE_ALIGN_LEFT, AE_COLOR_TITLE, "CHANGE GAMETYPE");
	for (index = 0; index < (int)sizeof(query) - 1 && screen.query[index]; index++)
		query[index] = screen.query[index] >= 'a' && screen.query[index] <= 'z' ? (char)(screen.query[index] - 32) :
			screen.query[index];
	query[index] = 0;
	if (searching())
		snprintf(crumb, sizeof(crumb), "%d GAMETYPE%s MATCH %s", shown, shown == 1 ? "" : "S", query);
	else if (screen.filter == AE_GTF_ALL)
		snprintf(crumb, sizeof(crumb), "%d GAMETYPES", shown);
	else
		snprintf(crumb, sizeof(crumb), "%d OF %d GAMETYPES ARE %s", shown, ae_gtp_count(&gtp, AE_GTF_ALL, NULL),
			ae_gtp_filter_name(screen.filter));
	ae_draw_text_tracked(AE_FONT_BODY, crumb_size, CRUMB_TRACKING, right_x(), cap_top(AE_FONT_BODY, crumb_size, middle),
		AE_ALIGN_RIGHT, AE_COLOR_TEXT, crumb);
}

/* the search field over the panel, centred on the chips: idle (X SEARCH), typing (the ring, the caret, B CLEARS) or
holding a search */
static void draw_search(float center_y)
{
	struct ae_density const *d = &screen.density;
	float x = panel_x(), width = right_x() - x, height = u(SEARCH_HEIGHT_U), top = center_y - height * 0.5f;
	float pad = u(SEARCH_PAD_U), minor = ae_size_minor(d), glyph = u(SEARCH_GLYPH_U), pen = x + pad;
	int device = ae_ui_last_device(), editing = screen.typing || screen.keyboard_open;
	const char *clears = device == AE_DEVICE_KEYBOARD_MOUSE ? "ESC CLEARS" : "B CLEARS";

	screen.well.x = x;
	screen.well.y = top;
	screen.well.width = width;
	screen.well.height = height;
	ae_draw_rect(x, top, width, height, u(screen.density.metrics->corner), AE_COLOR_WELL);
	if (editing)
		ae_draw_outline(x, top, width, height, u(screen.density.metrics->corner), u(RING_U), AE_COLOR_RING);
	if (!editing && !screen.search.length)
	{
		/* (idle: the X button, or Ctrl+F on a keyboard, and the placeholder) */
		if (device == AE_DEVICE_KEYBOARD_MOUSE)
			pen += ae_widget_key_cap(d, pen, center_y - glyph * 0.5f, glyph, ae_prompt_key(AE_BUTTON_X));
		else
			pen += ae_draw_button(device_font(), AE_BUTTON_X, glyph, pen, center_y - glyph * 0.5f,
				ae_prompt_tint(device, AE_BUTTON_X));
		ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, pen + u(10.0f), cap_top(AE_FONT_BODY, minor, center_y),
			AE_ALIGN_LEFT, AE_COLOR_MUTED, "SEARCH");
	}
	else
	{
		float clears_width = ae_draw_text_tracked_width(AE_FONT_BODY, minor * 0.85f, 0.09f, clears);
		float room = width - 3.0f * pad - clears_width, text = ae_size_text(d), drawn;
		char shown[AE_TEXT_MAXIMUM + 8];

		ae_fit_text(AE_FONT_BODY, text, screen.search.text, room, shown, sizeof(shown));
		drawn = ae_draw_text(AE_FONT_BODY, text, pen, cap_top(AE_FONT_BODY, text, center_y), AE_ALIGN_LEFT,
			AE_COLOR_TITLE, shown);
		/* (the caret: 2 u in the accent, blinking as the fields' own) */
		if (editing && ((ae_motion_now() - screen.caret_since) / 530u) % 2u == 0u)
			ae_draw_rect(pen + drawn + u(2.0f), center_y - ae_draw_cap_height(AE_FONT_BODY, text) * 0.75f, u(CARET_U),
				ae_draw_cap_height(AE_FONT_BODY, text) * 1.5f, 0.0f, AE_COLOR_ACCENT);
		ae_draw_text_tracked(AE_FONT_BODY, minor * 0.85f, 0.09f, x + width - pad,
			cap_top(AE_FONT_BODY, minor * 0.85f, center_y), AE_ALIGN_RIGHT, AE_COLOR_MUTED, clears);
	}
	ae_hit_add(x, top, width, height, HIT_SEARCH, AE_PART_FIELD, 0);
}

/* the filters between LB and RB, the search beside them; returns the rule's y under them */
static float draw_filters(void)
{
	struct ae_rect rects[AE_GTP_FILTERS];
	float glyph = ae_size_glyph(&screen.density), y = frame_y(CHIPS_Y), x = left_x(), chips_x, height, end, room;
	float rb = ae_draw_button_width(device_font(), AE_BUTTON_RIGHT_SHOULDER, glyph);
	int filter;

	for (filter = 0; filter < AE_GTP_FILTERS; filter++)
	{
		short count = ae_gtp_count(&gtp, filter, screen.query);

		snprintf(screen.chip_labels[filter], sizeof(screen.chip_labels[filter]), "%s", ae_gtp_filter_name(filter));
		screen.chips[filter].label = screen.chip_labels[filter];
		screen.chips[filter].count = count;
		screen.chips[filter].flags = filter == screen.filter ? AE_CHIP_ON : count ? 0 : AE_CHIP_DISABLED;
	}
	chips_x = x + ae_draw_button_width(device_font(), AE_BUTTON_LEFT_SHOULDER, glyph) + u(GLYPH_GAP_U);
	room = panel_x() - u(GLYPH_GAP_U) * 2.0f - rb - chips_x;
	height = ae_chips_layout(&screen.density, room, screen.chips, AE_GTP_FILTERS, rects);
	ae_draw_button(device_font(), AE_BUTTON_LEFT_SHOULDER, glyph, x, y + (rects[0].height - glyph) * 0.5f,
		AE_COLOR_KEY_CAP);
	ae_widget_chips(&screen.density, chips_x, y, room, screen.chips, AE_GTP_FILTERS, HIT_CHIPS);
	end = chips_x + rects[AE_GTP_FILTERS - 1].x + rects[AE_GTP_FILTERS - 1].width + u(GLYPH_GAP_U);
	ae_draw_button(device_font(), AE_BUTTON_RIGHT_SHOULDER, glyph, end,
		y + rects[AE_GTP_FILTERS - 1].y + (rects[AE_GTP_FILTERS - 1].height - glyph) * 0.5f, AE_COLOR_KEY_CAP);
	draw_search(y + rects[0].height * 0.5f);
	return y + height + u(14.0f);
}

/* the parts on one line, drawn dots 24 u apart; a part that does not fit is left out with the rest */
static void facts_line(char const *const *parts, int count, float x, float y, float width)
{
	float minor = ae_size_minor(&screen.density) * 1.1f, center = y + ae_draw_cap_height(AE_FONT_BODY, minor) * 0.5f;
	float end = x + width;
	int index;

	for (index = 0; index < count; index++)
	{
		float part = ae_draw_text_tracked_width(AE_FONT_BODY, minor, 0.06f, parts[index]);

		if (index && x + u(DOT_PITCH_U) + part > end)
			break;
		if (index)
		{
			ae_widget_separator(&screen.density, x + u(DOT_PITCH_U) * 0.5f, center, 5.0f, AE_COLOR_MUTED);
			x += u(DOT_PITCH_U);
		}
		x += ae_draw_text_tracked(AE_FONT_BODY, minor, 0.06f, x, y, AE_ALIGN_LEFT, AE_COLOR_TEXT, parts[index]);
	}
}

/* the full name (wrapped at a space when it and its badge do not fit) and the mode badge; returns the y under it */
static float name_and_badge(const char *name, const char *badge, float x, float y, float width)
{
	float text = ae_size_text(&screen.density) * NAME_SCALE, minor = ae_size_minor(&screen.density) * 0.85f;
	float cap = ae_draw_cap_height(AE_FONT_ROW, text), line = cap + u(PANEL_GAP_U) * 0.75f;
	float badge_width = ae_draw_text_tracked_width(AE_FONT_BODY, minor, 0.1f, badge) + 2.0f * u(BADGE_SIDE_U);
	float badge_height = u(BADGE_HEIGHT_U), last_width, center;
	char lines[2][160];
	int count, index;

	if (ae_draw_text_width(AE_FONT_ROW, text, name) + u(BADGE_GAP_U) + badge_width <= width)
	{
		snprintf(lines[0], sizeof(lines[0]), "%s", name);
		count = 1;
	}
	else
		count = ae_wrap_text(AE_FONT_ROW, text, name, width - u(BADGE_GAP_U) - badge_width, lines, 2);
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_ROW, text, x, y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_TITLE, lines[index]);
	last_width = count ? ae_draw_text_width(AE_FONT_ROW, text, lines[count - 1]) : 0.0f;
	center = y + line * (float)(count - 1) + cap * 0.5f;
	if (badge[0])
	{
		float bx = x + last_width + u(BADGE_GAP_U);

		ae_draw_outline(bx, center - badge_height * 0.5f, badge_width, badge_height, u(2.0f), u(1.0f), AE_COLOR_ACCENT);
		ae_draw_text_tracked(AE_FONT_BODY, minor, 0.1f, bx + u(BADGE_SIDE_U), cap_top(AE_FONT_BODY, minor, center),
			AE_ALIGN_LEFT, AE_COLOR_ACCENT, badge);
	}
	return y + line * (float)(count - 1) + cap + u(PANEL_GAP_U);
}

/* a muted line (the maps with the mode, the stored name), when it fits above bottom */
static float muted_line(const char *words, float x, float y, float width, float bottom)
{
	float body = u(screen.density.metrics->body), line = u(screen.density.metrics->body_line);
	char lines[2][160];
	int count = ae_wrap_text(AE_FONT_BODY, body, words, width, lines, 2), index;

	for (index = 0; index < count && y + line * (float)(index + 1) <= bottom; index++)
		ae_draw_text(AE_FONT_BODY, body, x, y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_MUTED, lines[index]);
	return y + line * (float)count;
}

static const char *mode_words(int engine)
{
	switch (engine)
	{
	case AE_SETUP_ENGINE_CTF: return "CTF flags";
	case AE_SETUP_ENGINE_KING: return "hills";
	case AE_SETUP_ENGINE_ODDBALL: return "ball spawns";
	case AE_SETUP_ENGINE_SLAYER: return "slayer spawns";
	}
	return NULL;
}

static void draw_folder_panel(int folder, float x, float y, float width)
{
	float body = u(screen.density.metrics->body), line = u(screen.density.metrics->body_line);
	float text = ae_size_text(&screen.density) * NAME_SCALE;
	char words[200], lines[3][160];
	short count = ae_gtp_folder_count(&gtp, folder, screen.filter, screen.query);
	int index, written;

	ae_draw_text(AE_FONT_ROW, text, x, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, ae_gtp_folder_name(folder));
	y += ae_draw_cap_height(AE_FONT_ROW, text) + u(PANEL_GAP_U);
	if (folder == AE_GTP_FOLDER_RECENT)
		snprintf(words, sizeof(words), "The last %d gametypes you picked, with the start you used.", count);
	else if (folder == AE_GTP_FOLDER_FAVOURITES)
		snprintf(words, sizeof(words), "%d gametype%s you starred with Y (until the game closes).", count,
			count == 1 ? "" : "s");
	else
		snprintf(words, sizeof(words), "%d gametype%s%s. A opens the folder here; LT / RT go from folder to folder.",
			count, count == 1 ? "" : "s", screen.filter == AE_GTF_ALL ? "" : " suit the filter");
	written = ae_wrap_text(AE_FONT_BODY, body, words, width, lines, 3);
	for (index = 0; index < written; index++)
		ae_draw_text(AE_FONT_BODY, body, x, y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
}

static void draw_panel(float x, float y, float width, float bottom)
{
	struct ae_density const *d = &screen.density;
	short gametype = ae_gtp_row_gametype(&gtp, screen.focus_row);
	float body = u(d->metrics->body), line = u(d->metrics->body_line), minor = ae_size_minor(d);
	struct ae_gtp_gametype const *g;
	char lines[3][160], words[160];
	char const *parts[3];
	int count, index;

	if (!screen.count || screen.focus_row.folder < 0)
		return;
	if (gametype < 0)
	{
		draw_folder_panel(screen.focus_row.folder, x, y, width);
		return;
	}
	g = &gtp.gametypes[gametype];
	y = name_and_badge(g->name, ae_gtp_filter_name(ae_gtp_filter_of_engine(g->engine)), x, y, width);
	parts[0] = ae_gtp_folder_name(g->folder);
	parts[1] = g->score;
	parts[2] = g->time;
	facts_line(parts, 3, x, y, width);
	y += u(FACTS_LINE_U);
	/* the description */
	if (g->description)
	{
		count = ae_wrap_text(AE_FONT_BODY, body, g->description, width, lines, 3);
		for (index = 0; index < count; index++)
			ae_draw_text(AE_FONT_BODY, body, x, y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
		y += line * (float)count;
	}
	/* RULES */
	{
		static const char *const labels[4] = { "WEAPONS", "SHIELDS", "MOTION SENSOR", "RESPAWN" };
		const char *values[4];

		values[0] = g->weapons;
		values[1] = g->shields;
		values[2] = g->sensor;
		values[3] = g->respawn;
		y += ae_widget_group(d, x, y, width, "RULES");
		for (index = 0; index < 4 && y + line <= bottom; index++)
		{
			float center = y + line * 0.5f;
			char fitted[160];

			ae_draw_text_tracked(AE_FONT_BODY, minor, 0.06f, x, cap_top(AE_FONT_BODY, minor, center), AE_ALIGN_LEFT,
				AE_COLOR_MUTED, labels[index]);
			ae_fit_text(AE_FONT_ROW, body, values[index], width - u(RULES_VALUE_U), fitted, sizeof(fitted));
			ae_draw_text(AE_FONT_ROW, body, x + u(RULES_VALUE_U), cap_top(AE_FONT_ROW, body, center), AE_ALIGN_LEFT,
				AE_COLOR_TEXT, fitted);
			y += line;
		}
	}
	/* PLAYS WELL ON: maps with the mode, of the size for the team; then how many maps have the mode */
	{
		short plays[PLAYS_MAXIMUM], have = 0, read = 0, shown;
		struct ae_chip chips[PLAYS_MAXIMUM];
		char names[PLAYS_MAXIMUM][AE_SETUP_MAP_TEXT];
		struct ae_rect rects[PLAYS_MAXIMUM];
		const char *mode = mode_words(g->engine);

		shown = ae_gtp_plays_well(&maps, g->engine, g->team, ae_gtp_preferred_map_groups(&maps), plays, PLAYS_MAXIMUM,
			&have, &read);
		for (index = 0; index < shown; index++)
		{
			int at;

			for (at = 0; at < AE_SETUP_MAP_TEXT - 1 && maps.maps[plays[index]].name[at]; at++)
			{
				char c = maps.maps[plays[index]].name[at];

				names[index][at] = c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
			}
			names[index][at] = 0;
			chips[index].label = names[index];
			chips[index].count = -1;
			chips[index].flags = 0;
		}
		/* (one line of chips: the last ones left out until they fit) */
		while (shown > 1 && ae_chips_layout(d, width, chips, shown, rects) > rects[0].height + 1.0f)
			shown--;
		if (shown)
		{
			y += ae_widget_group(d, x, y, width, "PLAYS WELL ON");
			y += ae_widget_chips(d, x, y, width, chips, shown, HIT_PLAYS) + u(PANEL_GAP_U);
		}
		else
			y += u(PANEL_GAP_U);
		if (!mode)
			words[0] = 0;
		else if (screen.facts_left && !read)
			snprintf(words, sizeof(words), "Reading the maps...");
		else
			snprintf(words, sizeof(words), "%d of %d maps have %s.", have, read, mode);
		if (words[0])
			y = muted_line(words, x, y, width, bottom);
	}
	if (strcmp(g->name, g->stored))
	{
		snprintf(words, sizeof(words), "Older machines list it as %s.", g->stored);
		muted_line(words, x, y, width, bottom);
	}
}

/* no gametype has the search's letters: one box at the list's top */
static void draw_empty(float x, float y, float width)
{
	struct ae_density const *d = &screen.density;
	float row = ae_size_row(d), gap = u(d->metrics->gap), pad = u(d->metrics->pad), text = ae_size_text(d);
	float body = u(d->metrics->body), height = row * 2.0f + gap;
	char words[96], query[AE_GTP_QUERY];
	int index;

	for (index = 0; index < AE_GTP_QUERY - 1 && screen.query[index]; index++)
		query[index] = screen.query[index] >= 'a' && screen.query[index] <= 'z' ? (char)(screen.query[index] - 32) :
			screen.query[index];
	query[index] = 0;
	ae_draw_rect(x, y, width, height, u(d->metrics->corner), AE_COLOR_ROW);
	snprintf(words, sizeof(words), "No gametype has %s in its name.", query);
	ae_draw_text(AE_FONT_ROW, text, x + pad, cap_top(AE_FONT_ROW, text, y + row * 0.5f + gap * 0.5f), AE_ALIGN_LEFT,
		AE_COLOR_TEXT, words);
	ae_draw_text(AE_FONT_BODY, body, x + pad, cap_top(AE_FONT_BODY, body, y + row * 1.5f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
		ae_ui_last_device() == AE_DEVICE_KEYBOARD_MOUSE ? "Check the spelling, or press Esc to clear the search." :
		"Check the spelling, or press B to clear the search.");
}

/* the open folder's header, when it scrolled off over its own rows: pinned in the list's first row slot */
static short pinned_header(void)
{
	short header;

	if (searching() || screen.open_folder < 0 || screen.focus_row.folder != screen.open_folder)
		return -1;
	header = ae_gtp_header_row(screen.rows, screen.count, screen.open_folder);
	return header >= 0 && header < screen.list.list.first ? header : -1;
}

static void draw_footer(void)
{
	struct ae_gtp_row row = screen.focus_row;
	short gametype = ae_gtp_row_gametype(&gtp, row), count = 0;

	if (!screen.count)
		;
	else if (row.gametype < 0)
	{
		if (!always_open(row.folder))
			screen.prompts[count++] = prompt_of(AE_BUTTON_A, row.folder == screen.open_folder ? "Close" : "Open",
				AE_ACTION_ACCEPT, 0);
	}
	else
		screen.prompts[count++] = prompt_of(AE_BUTTON_A, "Use this gametype", AE_ACTION_ACCEPT, 0);
	screen.prompts[count++] = prompt_of(AE_BUTTON_B, screen.typing || searching() ? "Clear search" :
		ae_string(AE_STR_BACK), AE_ACTION_BACK, 0);
	screen.prompts[count++] = prompt_of(AE_BUTTON_LEFT_SHOULDER, "Filter", AE_ACTION_TAB_PREVIOUS, 1);
	if (!searching())
		screen.prompts[count++] = prompt_of(AE_BUTTON_LEFT_TRIGGER, "Folder", AE_ACTION_PAGE_UP, 1);
	if (gametype >= 0)
		screen.prompts[count++] = prompt_of(AE_BUTTON_Y, "Favourite", AE_ACTION_Y, 0);
	if (!screen.typing)
		screen.prompts[count++] = prompt_of(AE_BUTTON_X, "Search", AE_ACTION_X, 0);
	if (ae_gtp_has_arrows(&gtp, gametype))
		screen.prompts[count++] = prompt_of(AE_BUTTON_DPAD_LEFT, "Start weapon", AE_ACTION_LEFT, 1);
	screen.prompt_count = count;
	ae_widget_prompts(&screen.density, left_x(), screen.frame.rect.height - u(FOOTER_ABOVE_U), screen.prompts, count,
		NULL, right_x(), -1, HIT_PROMPTS);
}

static void gametype_draw(struct ae_screen *instance)
{
	struct ae_list *list = &screen.list.list;
	float top, bottom, list_width, rule, row_height;
	short header;

	(void)instance;
	full_density();
	draw_header();
	rule = draw_filters();
	ae_draw_rect(left_x(), rule, right_x() - left_x(), u(RULE_U), 0.0f, AE_COLOR_RULE);
	top = rule + u(24.0f);
	if (top < frame_y(CONTENT_Y))
		top = frame_y(CONTENT_Y);
	bottom = screen.frame.rect.height - u(FOOTER_ABOVE_U) - u(FOOTER_ROOM_U);
	list_width = screen.frame.rect.width * LIST_WIDTH;
	row_height = ae_size_row(&screen.density);
	if (!screen.count && searching())
		draw_empty(left_x(), top, list_width - u(TRACK_STRIP_U));
	else
	{
		/* (the pinned header takes the first row slot: the focus never under it) */
		header = pinned_header();
		if (header >= 0 && list->focus == list->first && list->first > 0)
			list->first--;
		header = pinned_header();
		ae_widget_list(&screen.density, &screen.list, left_x(), top, list_width, bottom - top, list_item, NULL, HIT_LIST);
		if (header >= 0)
		{
			/* (over the "▲ N more" line and the first row slot, opaque so neither shows through; the header in
			its middle) */
			float rows_top = ae_list_view_rows_top(&screen.density, &screen.list, top);
			float height = rows_top + row_height - top;

			ae_draw_rect(left_x(), top, list_width - u(TRACK_STRIP_U), height, 0.0f, AE_COLOR_POPOVER);
			header_row(left_x(), top + (height - row_height) * 0.5f, list_width - u(TRACK_STRIP_U), screen.open_folder, 0,
				-1);
		}
	}
	draw_panel(panel_x(), top, right_x() - panel_x(), bottom);
	draw_footer();
}

/* ---------- input */

static void gametype_update(struct ae_screen *instance)
{
	(void)instance;
	if (screen.typing && screen.edit.active && !ae_field_edit_keys(&screen.edit))
		ae_sound_request(AE_SOUND_FAILURE, 0);
	/* (typed, or AE's keyboard: the list follows the text as it changes) */
	query_follow();
	if (!screen.facts_left)
		return;
	screen.facts_left = ae_setup_maps_read_facts(&maps, FACTS_PER_FRAME);
}

static int gametype_handle(struct ae_screen *instance, struct ae_event const *event)
{
	struct ae_gtp_row row = screen.focus_row;

	(void)instance;
	/* (typing: Enter or START keeps the search, Esc or B clears it; the rest is the typing's) */
	if (screen.typing)
	{
		if (event->action == AE_ACTION_START || event->action == AE_ACTION_ACCEPT)
			typing_end(1);
		else if (event->action == AE_ACTION_BACK)
			typing_end(0);
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		/* (a search first: B clears it) */
		if (searching() || screen.search.length)
		{
			search_clear();
			ae_sound_request(AE_SOUND_BACK, 0);
			return 1;
		}
		return 0;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		filter_set(screen.filter + (event->action == AE_ACTION_TAB_NEXT ? 1 : -1));
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		return 1;
	case AE_ACTION_PAGE_UP:
	case AE_ACTION_PAGE_DOWN:
	{
		short at;

		if (searching())
			return 1;
		at = ae_gtp_folder_step(screen.rows, screen.count, screen.list.list.focus,
			event->action == AE_ACTION_PAGE_DOWN ? 1 : -1);
		if (at < 0 || at >= screen.count)
			return 1;
		screen.focus_row = screen.rows[at];
		folder_set(screen.rows[at].folder);
		show_row_at_top(index_of_row(screen.focus_row));
		ae_sound_request(AE_SOUND_CURSOR, 0);
		return 1;
	}
	case AE_ACTION_ACCEPT:
		accept(row);
		return 1;
	case AE_ACTION_RIGHT:
		if (row.gametype >= 0)
			toggle(row);
		else if (!always_open(row.folder) && row.folder != screen.open_folder)
			accept(row);
		return 1;
	case AE_ACTION_LEFT:
		if (row.gametype >= 0 && toggle(row))
			return 1;
		if (!searching() && screen.open_folder >= 0)
		{
			screen.focus_row.folder = (short)screen.open_folder;
			screen.focus_row.gametype = -1;
			folder_set(-1);
			ae_sound_request(AE_SOUND_BACK, 0);
		}
		return 1;
	case AE_ACTION_Y:
		if (row.gametype >= 0)
		{
			ae_gtp_remember_favourite(&gtp, ae_gtp_row_gametype(&gtp, row));
			refresh();
			ae_sound_request(AE_SOUND_CURSOR, 0);
		}
		return 1;
	case AE_ACTION_X:
		search_begin(event->device);
		return 1;
	default:
		break;
	}
	if (ae_list_view_event(&screen.list, event))
		focus_taken();
	return 1;
}

static void gametype_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	short item;

	if (ae_prompts_pointer(screen.prompts, screen.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK && !screen.typing && !searching())
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			gametype_handle(instance, &event);
		return;
	}
	if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit))
	{
		if (hit.id == HIT_CHIPS && hit.index >= 0 && hit.index < AE_GTP_FILTERS)
		{
			filter_set(hit.index);
			ae_sound_request(AE_SOUND_CURSOR, 0);
			return;
		}
		if (hit.id == HIT_SEARCH && !screen.typing)
		{
			typing_begin();
			return;
		}
		if (hit.id == HIT_PINNED)
		{
			struct ae_gtp_row header;

			header.folder = (short)screen.open_folder;
			header.gametype = -1;
			screen.focus_row = header;
			accept(header);
			return;
		}
		/* (a focused pair's arrows: its start) */
		if (hit.id == HIT_LIST && (hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT) &&
			hit.index >= 0 && hit.index < screen.count)
		{
			toggle(screen.rows[hit.index]);
			return;
		}
	}
	item = ae_list_view_pointer(&screen.list, pointer, HIT_LIST);
	focus_taken();
	if (item >= 0 && item < screen.count)
		accept(screen.rows[item]);
}

static void gametype_leave(struct ae_screen *instance)
{
	(void)instance;
	if (screen.typing)
	{
		screen.typing = 0;
		ae_field_edit_end(&screen.edit, 0);
	}
	platform_log("ae change gametype: closed");
}

static struct ae_screen_class const gametype_class =
{
	.name = "change gametype", .leave = gametype_leave, .handle = gametype_handle, .draw = gametype_draw,
	.pointer = gametype_pointer, .update = gametype_update,
};

static int open_screen(int filter, const char *query, void (*picked)(long profile))
{
	short current, base;

	memset(&screen, 0, sizeof(screen));
	screen.picked = picked;
	screen.logged_row = no_row();
	screen.filter = filter >= 0 && filter < AE_GTP_FILTERS ? filter : AE_GTF_ALL;
	ae_gtp_load(&gtp, &current);
	screen.current = current;
	ae_setup_maps_load(&maps);
	screen.facts_left = maps.count;
	ae_text_init(&screen.search, query ? query : "", AE_GTP_QUERY - 1, 0);
	snprintf(screen.query, sizeof(screen.query), "%s", screen.search.text);
	/* (the folder of the edit's gametype open, its row focused) */
	screen.open_folder = -1;
	screen.focus_row = no_row();
	if (current >= 0)
	{
		base = ae_gtp_base(&gtp, current);
		screen.open_folder = gtp.gametypes[base].folder;
		screen.focus_row.folder = (short)screen.open_folder;
		screen.focus_row.gametype = base;
	}
	ae_list_view_init(&screen.list, 0, 10);
	refresh();
	if (!ae_ui_push(&gametype_class, AE_OWNER_ANY, &screen))
		return 0;
	if (screen.open_folder >= 0)
		platform_log("ae change gametype: folder %s open", ae_gtp_folder_name(screen.open_folder));
	platform_log("ae change gametype: open (filter %s): %d gametypes%s%s%s", ae_gtp_filter_name(screen.filter),
		ae_gtp_count(&gtp, screen.filter, screen.query), searching() ? ", search '" : "", searching() ? screen.query : "",
		searching() ? "'" : "");
	return 1;
}

int ae_screen_change_gametype_open(int filter, const char *query)
{
	if (!open_screen(filter, query, NULL))
		return 0;
	ae_ui_replace_menus();
	return 1;
}

int ae_screen_change_gametype_pick(void (*picked)(long profile))
{
	return open_screen(AE_GTF_ALL, NULL, picked);
}
