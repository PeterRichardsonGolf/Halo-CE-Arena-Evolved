/*
AE_SCREEN_CHANGE_GAMETYPE.C

AE's CHANGE GAMETYPE screen (ae_screen_change_gametype.h; the v2 decisions of 2026-10-22). CHANGE MAP's frame (ae_screen_change_map.c):
the title and the count top right; the mode filters' chips between LB and RB, the search field beside them over the
panel; the list (RECENT open: the gametypes played last, each once; FAVOURITES and the player's own folders,
then the folders, one open in place; its header pinned on the list's top once it scrolls off; AE menus Phase 2); the panel (the full name and its mode badge, folder . score to win .
time, the description, RULES, PLAYS WELL ON's maps, the maps with the mode and the name older machines list).
Positions are fractions of the frame, sizes u.

A row is a battle rifle PAIR when there is one: its label the name without the BR word, its value the start weapon of
the one shown (PISTOL START, BR START, AR START ...), grey unless focused; left / right on an AE row with a pair toggle
it. A RECENT row shows the gametype played (its own start, no arrows; RECENT is the picker's own, owner 2026-10-28:
no map); search rows carry their folder's name.

Input (anyone's): up / down move; A opens or closes a folder, or picks a gametype (the caller's, or the GAME OPTIONS
edit's) and closes (on a RECENT row too: the gametype only); right opens a folder, left closes the open one (on a
pair: left / right its start); LB / RB (Q / E) the filter; LT / RT (PgUp / PgDn) the cursor to the next folder, opening nothing; X (F) the
folder menu (ae_picker_common.h) on a gametype, RENAME / DELETE on a folder of the player's; Y (R) the search: typing on
a keyboard (Enter or down keeps it, Esc clears it, then leaves it), AE's keyboard on a pad, the filter set to ALL
first (owner 2026-10-28: a search covers every gametype; it stays on ALL after); B clears a search, else
back; View (Select) deletes one of MY GAMETYPES. The pointer: hover, a click on a row as A, on its arrows the start, on
a chip its filter, on the search field typing.

Its log: "ae change gametype: open (filter <name>)", "filter <name>: N of M", "folder <name> open|closed",
"focus '<stored>'" / "focus folder <name>", "start '<stored>'", "search '<text>': N", "pick '<stored>'", "pick recent
'<stored>'", "(filter ALL): search opens", the common parts' and the glue's lines, "closed".
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_gametype_picker.h"
#include "ae_glue_gametype_picker.h"
#include "ae_glue_picker_stores.h"
#include "ae_glue_setup_maps.h"
#include "ae_glue_setup_options.h"
#include "ae_hooks.h"
#include "ae_motion.h"
#include "ae_picker_common.h"
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
#define TAG_GAP_U 24.0f
/* (0.7.0) the START switch's pips (the owner's look B, 10-30): 10 u dots 16 u apart, 16 u before
the left arrow, an empty one's ring 2 u */
#define PIP_U 10.0f
#define PIP_STEP_U 16.0f
#define PIP_GAP_U 16.0f
#define PIP_LINE_U 2.0f
#define NOTCH_U 6.0f
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
	/* the search: its field (typed on a keyboard, AE's keyboard on a pad); the query the rows were made for */
	struct ae_picker_search search;
	char query[AE_GTP_QUERY];
	/* (CREATE GAME's) a pick told back instead of taken here */
	void (*picked)(long profile);
	/* the list as last drawn (layout units) */
	float list_x, list_top, list_width, list_bottom;
	/* the gametype DELETE asks about, -1 none */
	short deleting;
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

/* RECENT: folds on its own flag (gtp.recent_folded), not by open_folder */
static int always_open(int folder)
{
	return folder == AE_GTP_FOLDER_RECENT;
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
	/* (an empty folder's line is never focused: its folder's header is) */
	if (index > 0 && screen.rows[index].gametype == AE_GTP_ROW_EMPTY)
		index--;
	ae_list_set_focus(&screen.list.list, index < 0 ? 0 : index);
	screen.focus_row = focused_row();
	if (!same_row(screen.focus_row, screen.logged_row) && screen.count)
	{
		short gametype = ae_gtp_row_gametype(&gtp, screen.focus_row);

		screen.logged_row = screen.focus_row;
		if (gametype < 0)
			platform_log("ae change gametype: focus folder %s", ae_gtp_folder_label(&gtp, screen.focus_row.folder));
		else
			platform_log("ae change gametype: focus '%s' (%s)", gtp.gametypes[gametype].stored,
				gtp.gametypes[gametype].start);
	}
}

/* (never on an empty folder's line) */
static void focus_taken(int direction)
{
	struct ae_list *list = &screen.list.list;

	if (list->focus >= 0 && list->focus < screen.count && screen.rows[list->focus].gametype == AE_GTP_ROW_EMPTY)
	{
		short next = (short)(list->focus + (direction >= 0 ? 1 : -1));

		ae_list_set_focus(list, next >= 0 && next < screen.count ? next : (short)(list->focus - 1));
	}
	screen.focus_row = focused_row();
	refresh();
}

static void folder_set(int folder)
{
	int was = screen.open_folder;

	screen.open_folder = folder;
	if (was >= 0 && was != folder)
		platform_log("ae change gametype: folder %s closed", ae_gtp_folder_label(&gtp, was));
	if (folder >= 0)
		platform_log("ae change gametype: folder %s open", ae_gtp_folder_label(&gtp, folder));
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
	if (!strncmp(screen.query, screen.search.text.text, sizeof(screen.query) - 1))
		return;
	snprintf(screen.query, sizeof(screen.query), "%s", screen.search.text.text);
	/* (a new search starts at its first result; a cleared one goes back to the row it was on) */
	if (searching())
		screen.focus_row = no_row();
	refresh();
	platform_log("ae change gametype: search '%s': %d", screen.query, ae_gtp_count(&gtp, screen.filter, screen.query));
}

static void search_clear(void)
{
	ae_picker_search_clear(&screen.search);
	query_follow();
}

/* Y (R), a click on the field: the search, on ALL first (owner 2026-10-28: a search covers every gametype, whatever
the filter was; it stays on ALL when the search is cleared or left, LB / RB still step it) */
static void search_open(int device)
{
	if (screen.filter != AE_GTF_ALL)
	{
		screen.filter = AE_GTF_ALL;
		refresh();
	}
	platform_log("ae change gametype: (filter ALL): search opens");
	ae_picker_search_begin(&screen.search, device, &screen.density, &screen.frame.rect, &screen);
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

/* RECENT folded or open (kept for the next time) */
static void recent_fold_set(int folded)
{
	gtp.recent_folded = (short)(folded != 0);
	ae_recent_store_set_folded(AE_RECENT_BY_GAMETYPE, folded);
	platform_log("ae change gametype: folder RECENT %s", folded ? "folded" : "open");
	refresh();
}

/* A on a row (or a click) */
static void accept(struct ae_gtp_row row)
{
	short gametype = ae_gtp_row_gametype(&gtp, row);

	if (row.folder < 0)
		return;
	if (row.gametype == -1)
	{
		if (always_open(row.folder))
		{
			recent_fold_set(!gtp.recent_folded);
			ae_sound_request(AE_SOUND_FORWARD, 0);
			return;
		}
		folder_set(screen.open_folder == row.folder ? -1 : row.folder);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	}
	if (gametype < 0)
		return;
	/* (a RECENT row: the gametype only) */
	if (row.folder == AE_GTP_FOLDER_RECENT)
		platform_log("ae change gametype: pick recent '%s'", gtp.gametypes[gametype].stored);
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

/* left / right on a pair: the START switch's previous (step -1) or next start, PISTOL > BR > EVO */
static int toggle(struct ae_gtp_row row, int step)
{
	short gametype;

	if (!ae_gtp_has_arrows(&gtp, ae_gtp_row_gametype(&gtp, row)) || !ae_gtp_step(&gtp, row, step))
		return 0;
	gametype = ae_gtp_row_gametype(&gtp, screen.focus_row);
	if (gametype >= 0)
		platform_log("ae change gametype: start '%s' %s", gtp.gametypes[gametype].stored, gtp.gametypes[gametype].start);
	ae_sound_request(AE_SOUND_CURSOR, 0);
	return 1;
}

/* ---------- the folder menu (X) */

static short menu_gametype = -1;

static int menu_in(short folder, void *context)
{
	struct ae_gtp_gametype const *base;

	(void)context;
	if (menu_gametype < 0)
		return 0;
	base = &gtp.gametypes[ae_gtp_base(&gtp, menu_gametype)];
	return folder == AE_PF_FAVOURITES ? base->favourite : folder - 1 < AE_GTP_OWN && (base->own >> (folder - 1) & 1u);
}

static int menu_toggle(short folder, void *context)
{
	int now;

	(void)context;
	now = ae_gtp_folder_toggle(&gtp, menu_gametype, folder);
	if (now >= 0)
		refresh();
	return now;
}

/* a store folder's list folder (FAVOURITES, or one of the player's) */
static int folder_of_store(short folder)
{
	return folder == AE_PF_FAVOURITES ? AE_GTP_FOLDER_FAVOURITES : AE_GTP_FOLDER_OWN + folder - 1;
}

static void menu_changed(short folder, void *context)
{
	(void)context;
	ae_gtp_folders_apply(&gtp);
	/* (a folder of the player's focused: renamed, it moved (its header then); deleted, it is gone (FAVOURITES') */
	if (screen.focus_row.gametype == -1 && ae_gtp_folder_is_own(screen.focus_row.folder))
	{
		int now = folder >= 0 ? folder_of_store(folder) : AE_GTP_FOLDER_FAVOURITES;

		if (screen.open_folder == screen.focus_row.folder)
			screen.open_folder = folder >= 0 ? now : -1;
		screen.focus_row.folder = (short)now;
	}
	if (ae_gtp_folder_is_own(screen.open_folder) && screen.open_folder - AE_GTP_FOLDER_OWN >= gtp.own_count)
		screen.open_folder = -1;
	refresh();
}

static float pitch(void)
{
	return ae_size_row(&screen.density) + u(screen.density.metrics->gap);
}

static void menu_open(struct ae_gtp_row row)
{
	struct ae_folder_menu_spec spec;
	short gametype = ae_gtp_row_gametype(&gtp, row);
	float rows_top = ae_list_view_rows_top(&screen.density, &screen.list, screen.list_top);
	int own = row.gametype == -1 && ae_gtp_folder_is_own(row.folder);

	if (gametype < 0 && !own)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_PF_GAMETYPES;
	spec.log = "ae change gametype";
	spec.row.x = screen.list_x;
	spec.row.y = rows_top + (float)(screen.list.list.focus - screen.list.list.first) * pitch();
	spec.row.width = screen.list_width - u(TRACK_STRIP_U);
	spec.row.height = ae_size_row(&screen.density);
	spec.bounds = screen.frame.rect;
	spec.list_x = screen.list_x;
	spec.list_right = screen.list_x + screen.list_width;
	spec.rows_top = rows_top;
	spec.rows_bottom = screen.list_bottom;
	spec.row_height = ae_size_row(&screen.density);
	spec.pitch = pitch();
	spec.density = screen.density;
	spec.in = menu_in;
	spec.toggle = menu_toggle;
	spec.changed = menu_changed;
	if (own)
	{
		spec.actions = 1;
		spec.folder = (short)(row.folder - AE_GTP_FOLDER_OWN + 1);
		spec.item = ae_gtp_folder_label(&gtp, row.folder);
	}
	else
	{
		menu_gametype = gametype;
		spec.item = gtp.gametypes[gametype].name;
		/* (inside FAVOURITES or one of the player's, not a search's or RECENT's row: REMOVE FROM it) */
		spec.folder = -1;
		if (!searching() && row.folder == AE_GTP_FOLDER_FAVOURITES)
			spec.folder = AE_PF_FAVOURITES;
		else if (!searching() && ae_gtp_folder_is_own(row.folder))
			spec.folder = (short)(row.folder - AE_GTP_FOLDER_OWN + 1);
	}
	ae_folder_menu_open(&spec);
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
	snprintf(label, sizeof(label), "     %s", ae_gtp_folder_label(&gtp, folder));
	row.label = label;
	ae_widget_row(&screen.density, x, y, width, &row, item < 0 ? HIT_PINNED : HIT_LIST, item);
	chevron(x + pad + u(CHEVRON_U), center, always_open(folder) ? !gtp.recent_folded : folder == screen.open_folder,
		on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT);
	snprintf(right, sizeof(right), "%d", ae_gtp_folder_count(&gtp, folder, screen.filter, screen.query));
	ae_draw_text(AE_FONT_BODY, minor, x + width - pad, cap_top(AE_FONT_BODY, minor, center), AE_ALIGN_RIGHT,
		on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED, right);
}

/* a gametype's row: its label (the search's letters in the accent), and on the right its start (grey unless focused;
with arrows on a focused pair, none on RECENT's: the one played), after its folder's name in a search */
static void gametype_row(float x, float y, float width, struct ae_gtp_row where, unsigned int flags, short item)
{
	struct ae_density const *d = &screen.density;
	struct ae_row row;
	short gametype = ae_gtp_row_gametype(&gtp, where);
	struct ae_gtp_gametype const *g = &gtp.gametypes[gametype];
	float pad = u(d->metrics->pad), minor = ae_size_minor(d), text = ae_size_text(d), indent = u(INDENT_U);
	float center = y + ae_size_row(d) * 0.5f, right, room;
	int on_bar = (flags & AE_ROW_ON_BAR) != 0, focused = (flags & AE_ROW_FOCUSED) != 0;
	int recent = where.folder == AE_GTP_FOLDER_RECENT;
	int arrows = focused && !recent && ae_gtp_has_arrows(&gtp, gametype);
	unsigned int ink = on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED;
	char label[160];
	const char *tag = searching() ? ae_gtp_folder_label(&gtp, g->folder) : NULL;
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
		/* (0.7.0, the owner's look B) the START switch's pips before the arrows: one a start of the group, the shown
		one filled; left out where the label would come within the gap of them */
		{
			int place, count = ae_gtp_start_place(&gtp, gametype, &place), pip;
			float size = u(PIP_U), step = u(PIP_STEP_U);
			float first = x + layout.arrow_left_x - u(PIP_GAP_U) - size - (float)(count - 1) * step;

			if (count > 1 && first > x + layout.label_x + layout.label_width + u(PIP_GAP_U))
			{
				for (pip = 0; pip < count; pip++)
				{
					float left = first + (float)pip * step, top = center - size * 0.5f;

					if (pip == place)
						ae_draw_rect(left, top, size, size, size * 0.5f, AE_COLOR_SELECTION_TEXT);
					else
						ae_draw_outline(left, top, size, size, size * 0.5f, u(PIP_LINE_U), AE_COLOR_MUTED);
				}
			}
		}
	}
	else
	{
		/* (from the right: the start, then the folder's name (a search's)) */
		const char *last = g->start, *before = tag;
		float last_width = ae_draw_text_width(AE_FONT_BODY, minor, last);
		float before_width = before ? ae_draw_text_width(AE_FONT_BODY, minor, before) : 0.0f;

		right -= last_width;
		if (before)
			right -= u(TAG_GAP_U) + before_width;
		room = right - u(TAG_GAP_U) - (x + pad);
		cut = ae_fit_text(AE_FONT_ROW, text, g->row, room, label, sizeof(label));
		row.label = label;
		ae_widget_row(d, x, y, width, &row, HIT_LIST, item);
		right = x + width - pad;
		ae_draw_text(AE_FONT_BODY, minor, right, cap_top(AE_FONT_BODY, minor, center), AE_ALIGN_RIGHT, ink, last);
		right -= last_width + u(TAG_GAP_U);
		if (before)
			ae_draw_text(AE_FONT_BODY, minor, right, cap_top(AE_FONT_BODY, minor, center), AE_ALIGN_RIGHT, ink, before);
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
	if (where.gametype == AE_GTP_ROW_EMPTY)
		ae_picker_empty_line(&screen.density, x + u(INDENT_U) + u(screen.density.metrics->pad),
			y + ae_size_row(&screen.density) * 0.5f, AE_PF_GAMETYPES);
	else if (where.gametype < 0)
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

/* the filters between LB and RB, the search beside them; returns the rule's y under them */
static float draw_filters(void)
{
	struct ae_rect rects[AE_GTP_FILTERS];
	float glyph = ae_size_glyph(&screen.density), y = frame_y(CHIPS_Y), x = left_x(), chips_x, height, end, room;
	float rb = ae_draw_button_width(device_font(), AE_BUTTON_RIGHT_SHOULDER, glyph), first_end;
	int filter, first_line_last = 0;

	for (filter = 0; filter < AE_GTP_FILTERS; filter++)
	{
		short count = ae_gtp_count(&gtp, filter, screen.query);

		snprintf(screen.chip_labels[filter], sizeof(screen.chip_labels[filter]), "%s", ae_gtp_filter_name(filter));
		screen.chips[filter].label = screen.chip_labels[filter];
		screen.chips[filter].count = count;
		screen.chips[filter].flags = filter == screen.filter ? AE_CHIP_ON : count ? 0 : AE_CHIP_DISABLED;
	}
	chips_x = x + ae_draw_button_width(device_font(), AE_BUTTON_LEFT_SHOULDER, glyph) + u(GLYPH_GAP_U);
	/* (the chips wrap before the search field goes under its 320 u) */
	room = ae_picker_search_chip_room(&screen.density, chips_x, right_x(), rb);
	height = ae_chips_layout(&screen.density, room, screen.chips, AE_GTP_FILTERS, rects);
	ae_widget_chip_key(device_font(), AE_BUTTON_LEFT_SHOULDER, glyph, x, y + rects[0].height * 0.5f);
	ae_widget_chips(&screen.density, chips_x, y, room, screen.chips, AE_GTP_FILTERS, HIT_CHIPS);
	end = chips_x + rects[AE_GTP_FILTERS - 1].x + rects[AE_GTP_FILTERS - 1].width + u(GLYPH_GAP_U);
	ae_widget_chip_key(device_font(), AE_BUTTON_RIGHT_SHOULDER, glyph, end,
		y + rects[AE_GTP_FILTERS - 1].y + rects[AE_GTP_FILTERS - 1].height * 0.5f);
	/* (the first line's end: RB's when it is on it) */
	for (filter = 0; filter < AE_GTP_FILTERS; filter++)
	{
		if (rects[filter].y == rects[0].y)
			first_line_last = filter;
	}
	first_end = first_line_last == AE_GTP_FILTERS - 1 ? end + rb : chips_x + rects[first_line_last].x +
		rects[first_line_last].width;
	ae_picker_search_draw(&screen.search, &screen.density, ae_picker_search_left(&screen.density, first_end, panel_x()),
		right_x(), y + rects[0].height * 0.5f, HIT_SEARCH);
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

/* a muted block (the maps with the mode, the stored name): drawn whole when it fits above bottom, else not at all
(never cut mid-sentence); returns the y under it */
static float muted_line(const char *words, float x, float y, float width, float bottom)
{
	float body = u(screen.density.metrics->body), line = u(screen.density.metrics->body_line);
	char lines[2][160];
	int count = ae_wrap_text(AE_FONT_BODY, body, words, width, lines, 2), index;

	if (y + line * (float)count > bottom)
		return y;
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, x, y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_MUTED, lines[index]);
	return y + line * (float)count;
}

static const char *mode_words(int engine)
{
	switch (engine)
	{
	/* (Bomb's goals are the CTF flags) */
	case AE_SETUP_ENGINE_CTF: case AE_SETUP_ENGINE_BOMB: return "CTF flags";
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
	char words[200], expanded[240], lines[3][160];
	short count = ae_gtp_folder_count(&gtp, folder, screen.filter, screen.query);
	int index, written;

	ae_draw_text(AE_FONT_ROW, text, x, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, ae_gtp_folder_label(&gtp, folder));
	y += ae_draw_cap_height(AE_FONT_ROW, text) + u(PANEL_GAP_U);
	if (folder == AE_GTP_FOLDER_RECENT)
		snprintf(words, sizeof(words), "The gametypes you played last, newest first. {A} on one sets the gametype.");
	else if (folder == AE_GTP_FOLDER_FAVOURITES)
		snprintf(words, sizeof(words), "%d gametype%s. {X} on a gametype puts it in your folders or takes it out.",
			count, count == 1 ? "" : "s");
	else if (ae_gtp_folder_is_own(folder))
		snprintf(words, sizeof(words), "%d gametype%s. Your folder: {X} renames it or deletes it.", count,
			count == 1 ? "" : "s");
	else
		snprintf(words, sizeof(words), "%d gametype%s%s. {A} opens the folder here; {LT} / {RT} go from folder to folder.",
			count, count == 1 ? "" : "s", screen.filter == AE_GTF_ALL ? "" : " suit the filter");
	written = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(words, expanded, sizeof(expanded)), width, lines, 3);
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
	parts[0] = ae_gtp_folder_label(&gtp, g->folder);
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
		unsigned int prefer[AE_SETUP_MAP_GROUP_WORDS];
		struct ae_chip chips[PLAYS_MAXIMUM];
		char names[PLAYS_MAXIMUM][AE_SETUP_MAP_TEXT];
		struct ae_rect rects[PLAYS_MAXIMUM];
		const char *mode = mode_words(g->engine);

		ae_gtp_preferred_map_groups(&maps, prefer);
		shown = ae_gtp_plays_well(&maps, g->engine, g->team, prefer, plays, PLAYS_MAXIMUM,
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
	ae_draw_rect(x, y, width, height, u(d->metrics->corner), AE_COLOR_ROW);
	ae_draw_text(AE_FONT_ROW, text, x + pad, cap_top(AE_FONT_ROW, text, y + row * 0.5f + gap * 0.5f), AE_ALIGN_LEFT,
		AE_COLOR_TEXT, "Nothing is called that.");
	ae_draw_text(AE_FONT_BODY, body, x + pad, cap_top(AE_FONT_BODY, body, y + row * 1.5f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
		"Try part of a name, like \"slayer\" or \"flag\".");
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

/* DELETE: one of MY GAMETYPES, not the one the game is set to (CREATE GAME's edit is made from it) */
static int deletable(short gametype)
{
	return gametype >= 0 && ae_gtp_can_delete(&gtp, gametype) &&
		(screen.current < 0 || ae_gtp_base(&gtp, screen.current) != ae_gtp_base(&gtp, gametype));
}

static void delete_picked(short choice, void *context)
{
	short gametype = screen.deleting;

	(void)context;
	screen.deleting = -1;
	if (choice != 1 || !deletable(gametype))
		return;
	if (!ae_gtp_delete(&gtp, gametype))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_gtp_load(&gtp, &screen.current, 1);
	gtp.recent_folded = (short)(ae_recent_store_folded(AE_RECENT_BY_GAMETYPE) != 0);
	screen.focus_row.folder = AE_GTP_FOLDER_MINE;
	screen.focus_row.gametype = -1;
	refresh();
	ae_sound_request(AE_SOUND_BACK, 0);
}

static void delete_ask(short gametype)
{
	static const char *const choices[] = { "CANCEL", "DELETE" };
	struct ae_dialog_spec spec;
	char title[96], body[200];

	if (!deletable(gametype))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	screen.deleting = gametype;
	snprintf(title, sizeof(title), "Delete %s?", gtp.gametypes[gametype].name);
	snprintf(body, sizeof(body), "It is gone from this PC, its gametype file too. This can't be undone.");
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = title;
	spec.body = body;
	spec.choices[0] = choices[0];
	spec.choices[1] = choices[1];
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.density = screen.density;
	spec.bounds = screen.frame.rect;
	spec.picked = delete_picked;
	if (ae_dialog_open(&spec, AE_OWNER_ANY))
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void draw_footer(void)
{
	struct ae_gtp_row row = screen.focus_row;
	short gametype = ae_gtp_row_gametype(&gtp, row), count = 0;
	int editing = ae_picker_search_editing(&screen.search);

	if (ae_folder_menu_is_open())
		count = ae_folder_menu_prompts(screen.prompts, PROMPTS);
	else if (editing || (searching() && !screen.count))
	{
		/* (typing: Enter to the list, Esc clears; nothing found: Esc clears) */
		if (editing && screen.count)
			screen.prompts[count++] = prompt_of(AE_BUTTON_A, "To the list", AE_ACTION_ACCEPT, 0);
		screen.prompts[count++] = prompt_of(AE_BUTTON_B, "Clear search", AE_ACTION_BACK, 0);
		screen.prompts[count++] = prompt_of(AE_BUTTON_LEFT_SHOULDER, "Filter", AE_ACTION_TAB_PREVIOUS, 1);
	}
	else
	{
		if (!screen.count)
			;
		else if (row.gametype < 0)
		{
			if (always_open(row.folder))
				screen.prompts[count++] = prompt_of(AE_BUTTON_A, gtp.recent_folded ? "Open" : "Close", AE_ACTION_ACCEPT, 0);
			else
				screen.prompts[count++] = prompt_of(AE_BUTTON_A, row.folder == screen.open_folder ? "Close" : "Open",
					AE_ACTION_ACCEPT, 0);
		}
		else
			screen.prompts[count++] = prompt_of(AE_BUTTON_A, "Select", AE_ACTION_ACCEPT, 0);
		screen.prompts[count++] = prompt_of(AE_BUTTON_B, searching() ? "Clear search" : ae_string(AE_STR_BACK),
			AE_ACTION_BACK, 0);
		if (gametype >= 0)
			screen.prompts[count++] = prompt_of(AE_BUTTON_X, "Folders", AE_ACTION_X, 0);
		else if (row.gametype == -1 && ae_gtp_folder_is_own(row.folder))
			screen.prompts[count++] = prompt_of(AE_BUTTON_X, "Rename or Delete", AE_ACTION_X, 0);
		screen.prompts[count++] = prompt_of(AE_BUTTON_Y, "Search", AE_ACTION_Y, 0);
		screen.prompts[count++] = prompt_of(AE_BUTTON_LEFT_SHOULDER, "Filter", AE_ACTION_TAB_PREVIOUS, 1);
		if (!searching())
			screen.prompts[count++] = prompt_of(AE_BUTTON_LEFT_TRIGGER, "Folder", AE_ACTION_PAGE_UP, 1);
		if (row.folder != AE_GTP_FOLDER_RECENT && ae_gtp_has_arrows(&gtp, gametype))
			screen.prompts[count++] = prompt_of(AE_BUTTON_DPAD_LEFT, "Start weapon", AE_ACTION_LEFT, 1);
		if (deletable(gametype))
			screen.prompts[count++] = prompt_of(AE_BUTTON_BACK, "Delete", AE_ACTION_SELECT, 0);
	}
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
	screen.list_x = left_x();
	screen.list_top = top;
	screen.list_width = list_width;
	screen.list_bottom = bottom;
	if (!screen.count && searching())
		draw_empty(left_x(), top, list_width - u(TRACK_STRIP_U));
	else if (ae_picker_search_editing(&screen.search))
	{
		/* (typing: no row has the bar; the row Enter or down goes to, the first, has the accent notch) */
		short focus = list->focus;

		list->focus = -1;
		ae_widget_list(&screen.density, &screen.list, left_x(), top, list_width, bottom - top, list_item, NULL, HIT_LIST);
		list->focus = focus;
		if (focus >= list->first && focus < list->first + list->rows)
			ae_draw_rect(left_x() + u(INDENT_U), ae_list_view_rows_top(&screen.density, &screen.list, top) +
				(float)(focus - list->first) * pitch(), u(NOTCH_U), row_height, 0.0f, AE_COLOR_ACCENT);
	}
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
	if (!ae_picker_search_keys(&screen.search))
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
	/* (typing: Enter, START or down keeps the search and goes to the list; Esc clears the text, then leaves; the
	filter still steps; the rest is the typing's) */
	if (screen.search.typing)
	{
		if (event->action == AE_ACTION_START || event->action == AE_ACTION_ACCEPT || event->action == AE_ACTION_DOWN)
			ae_picker_search_end(&screen.search, 1);
		else if (event->action == AE_ACTION_BACK)
		{
			if (screen.search.text.length)
			{
				ae_picker_search_clear(&screen.search);
				ae_sound_request(AE_SOUND_BACK, 0);
			}
			else
				ae_picker_search_end(&screen.search, 0);
		}
		else if (event->action == AE_ACTION_TAB_PREVIOUS || event->action == AE_ACTION_TAB_NEXT)
		{
			filter_set(screen.filter + (event->action == AE_ACTION_TAB_NEXT ? 1 : -1));
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		}
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		/* (a search first: B clears it) */
		if (searching() || screen.search.text.length)
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
		/* (the cursor only, the owner 10-30: nothing opens or closes; A opens) */
		screen.focus_row = screen.rows[at];
		show_row_at_top(index_of_row(screen.focus_row));
		ae_sound_request(AE_SOUND_CURSOR, 0);
		return 1;
	}
	case AE_ACTION_ACCEPT:
		accept(row);
		return 1;
	case AE_ACTION_RIGHT:
		if (row.gametype >= 0)
			toggle(row, 1);
		else if (row.gametype == -1 && (always_open(row.folder) ? gtp.recent_folded : row.folder != screen.open_folder))
			accept(row);
		return 1;
	case AE_ACTION_LEFT:
		if (row.gametype >= 0 && toggle(row, -1))
			return 1;
		if (!searching() && screen.open_folder >= 0)
		{
			screen.focus_row.folder = (short)screen.open_folder;
			screen.focus_row.gametype = -1;
			folder_set(-1);
			ae_sound_request(AE_SOUND_BACK, 0);
		}
		return 1;
	case AE_ACTION_X:
		menu_open(row);
		return 1;
	case AE_ACTION_Y:
		search_open(event->device);
		return 1;
	case AE_ACTION_SELECT:
		if (row.gametype >= 0)
			delete_ask(ae_gtp_row_gametype(&gtp, row));
		return 1;
	default:
		break;
	}
	if (ae_list_view_event(&screen.list, event))
		focus_taken(event->action == AE_ACTION_UP || event->action == AE_ACTION_PAGE_UP ? -1 : 1);
	return 1;
}

static void gametype_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	short item;

	if (ae_prompts_pointer(screen.prompts, screen.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK && !screen.search.typing && !searching())
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
		if (hit.id == HIT_SEARCH && !screen.search.typing)
		{
			search_open(AE_DEVICE_KEYBOARD_MOUSE);
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
			toggle(screen.rows[hit.index], hit.part == AE_PART_ARROW_LEFT ? -1 : 1);
			return;
		}
	}
	item = ae_list_view_pointer(&screen.list, pointer, HIT_LIST);
	focus_taken(1);
	if (item >= 0 && item < screen.count && screen.rows[item].gametype != AE_GTP_ROW_EMPTY)
		accept(screen.rows[item]);
}

static void gametype_leave(struct ae_screen *instance)
{
	(void)instance;
	ae_picker_search_leave(&screen.search);
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
	screen.deleting = -1;
	screen.logged_row = no_row();
	screen.filter = filter >= 0 && filter < AE_GTP_FILTERS ? filter : AE_GTF_ALL;
	/* (the maps: PLAYS WELL ON's) */
	ae_setup_maps_load(&maps);
	screen.facts_left = maps.count;
	ae_gtp_load(&gtp, &current, 1);
	gtp.recent_folded = (short)(ae_recent_store_folded(AE_RECENT_BY_GAMETYPE) != 0);
	screen.current = current;
	ae_picker_search_init(&screen.search, "ae change gametype", query);
	snprintf(screen.query, sizeof(screen.query), "%s", screen.search.text.text);
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
		platform_log("ae change gametype: folder %s open", ae_gtp_folder_label(&gtp, screen.open_folder));
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
