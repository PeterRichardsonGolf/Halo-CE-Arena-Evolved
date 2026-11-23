/*
AE_SCREEN_PROFILES.C

AE's PROFILES (ae_screen_profiles.h; the mockup's board 8), three
pages of AE's row screens (ae_setup_flow.h):
- the list: the profiles on this PC (PLAYER 1 on the one player 1 plays with) and NEW PROFILE; on the right the
  focused profile: its colour, its controls. A edits it, Y: player 1 plays with it, X deletes it (asked first; never
  one a player plays with), B back.
- EDIT (a profile's): COLOUR, BUTTON LAYOUT, LOOK SENSITIVITY, INVERT LOOK, VIBRATION, changed with left / right (or
  A), each saved to the profile at once (ae_glue_profiles.h). B back to the list.
- NEW PROFILE: its name typed (Enter on a keyboard, AE's keyboard with a pad's A); made, then player 1 plays with it.
- RENAME (0.7.8, EDIT's first row): the NEW PROFILE page again, on the profile's own name; the profile's save folder and
  its name move in one step (ae_profile_rename: colour, controls and campaign stay; an empty, taken or unusable name, or a
  profile another split-screen player plays with, is refused in the page's error words).

Its log: the glue's ("ae profiles: ..."), "ae profiles screen: open", "ae profiles screen: <page or item>".
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "interface/player_ui.h"
#include "../src/ae_draw.h"
#include "ae_glue_profile_move.h"
#include "ae_glue_profiles.h"
#include "ae_motion.h"
#include "ae_screen_profiles.h"
#include "ae_screen_record.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "menu_chief.h"

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
boolean game_in_progress(void);
boolean main_menu_is_active(void);

enum
{
	PROFILES_HIT = 0x7900,
	HIT_ROWS = PROFILES_HIT + 1, HIT_PROMPTS, HIT_SIDE, HIT_FIELD, HIT_SWATCH, HIT_FIGURE
};
enum
{
	ROW_HEADER = AE_FLOW_ROW_HEADER,
	ROW_PROFILE = AE_FLOW_ROW_OWN, ROW_NEW,
	ROW_RENAME, ROW_COLOR, ROW_LAYOUT, ROW_LOOK, ROW_INVERT, ROW_VIBRATION,
	ROW_MODERATOR_KEY,
	/* (the progress file: a profile's EXPORT and IMPORT rows, and the import's list of files) */
	ROW_EXPORT, ROW_IMPORT, ROW_FILE
};
enum { PAGE_LIST, PAGE_EDIT, PAGE_NEW, PAGE_FILES };
/* (the list's rows: a header, the profiles, NEW PROFILE) */
enum { LIST_PROFILES = AE_FLOW_ROWS - 4, NAME_LIMIT = 11, KEY_NOTE_MS = 6000 };

/* the game's button presets (player_profile.h), as upstream's gamepad setup names them */
static const char *const layout_names[] =
{
	"DEFAULT", "SWAP TRIGGERS", "SWAP A, L TRIGGER", "SWAP B, L TRIGGER", "SWAP B, R STICK", "BUMPER JUMPER",
	"FISHSTICKS", "HERO", "RECON", "THE DUKE", "ANNIVERSARY", "RECLAIMER", "ZOOM & SHOOT", "BUMP & JUMP",
	"GREEN FINGERS", "CUSTOM",
};
/* the profile's colours, in its order (player_profile.c; shield_color.c's names) */
static const char *const color_names[] =
{
	"WHITE", "BLACK", "RED", "BLUE", "GRAY", "YELLOW", "GREEN", "PINK", "PURPLE", "CYAN", "COBALT", "ORANGE", "TEAL",
	"SAGE", "BROWN", "TAN", "MAROON", "SALMON",
};

struct page
{
	struct ae_flow_screen flow;
	int kind;
};

static struct
{
	struct page list, edit, made, files;
	struct ae_profile_summary profiles[AE_PROFILES_MAXIMUM];
	short count;
	int player1;                        /* the profile player 1 plays with (-1: none) */
	short profile_of_row[AE_FLOW_ROWS];
	/* (0.7.7) MODERATOR KEY: copied, and for how long its help line says so */
	unsigned long key_copied_at;
	/* the profile EDIT shows */
	int editing;
	char editing_name[48];
	struct ae_profile_controls controls;
	short color;
	/* EDIT's colours: the one that has the focus (-1: the rows have it); he shows it, and it is the profile's only when
	kept. hover: the pointer gave it the focus, and takes it away by leaving the colours */
	short swatch;
	int swatch_hover;
	/* (DEV MODE) drawn as on a PC with no Spartan model */
	int plain;
	/* the pointer turns him: held down on him, and where it was */
	int turning;
	float turn_x;
	/* the focused profile's, for the list's right side */
	int side_index;
	struct ae_profile_controls side_controls;
	short side_color;
	/* IMPORT PROGRESS's files (the exports folder's, the newest first) */
	struct ae_pm_listing listing[AE_PM_LIST_MAX];
	int listing_count;
	/* (DEV MODE staging) the first file's question, asked once the files page is drawn: a page not drawn yet has no
	text sizes, and the question would be refused */
	int stage_import_ask;
	/* NEW PROFILE's name */
	struct ae_text name;
	struct ae_field_edit field_edit;
	int typing, keyboard_open;
	/* (0.7.8) the name page renames the profile EDIT shows (screen.editing) instead of making one */
	int renaming;
	unsigned long caret_since;
	struct ae_rect well;
	/* (split screen) NEW PROFILE opened alone for this player (0: PROFILES' own, player 1), and who hears of it */
	short for_player;
	ae_screen_profiles_made for_done;
	void *for_context;
} screen;

static struct ae_screen_class const profiles_class;
static void rows_files(void);

static const char *layout_name(short layout)
{
	return layout >= 0 && layout < (short)(sizeof(layout_names) / sizeof(layout_names[0])) ? layout_names[layout] : "";
}

static const char *color_name(short color)
{
	return color >= 0 && color < (short)(sizeof(color_names) / sizeof(color_names[0])) ? color_names[color] : "RANDOM";
}

static struct ae_profile_summary const *profile_by_index(int index)
{
	short at;

	for (at = 0; at < screen.count; at++)
	{
		if (screen.profiles[at].index == index)
			return &screen.profiles[at];
	}
	return NULL;
}

/* the profiles and player 1's, read again */
static void profiles_read(void)
{
	long active = player_ui_get_active_player_profile_index(0);

	screen.count = 0;
	if (!ae_profiles_list(screen.profiles, AE_PROFILES_MAXIMUM, &screen.count).ok)
		screen.count = 0;
	screen.player1 = active == NONE ? -1 : (int)(unsigned int)active;
	screen.side_index = -1;
}

/* ---------- the list */

static void rows_list(void)
{
	struct ae_flow_screen *flow = &screen.list.flow;
	short count = 0, at;

	screen.profile_of_row[count] = -1;
	flow->rows[count++] = (struct ae_flow_row){ ROW_HEADER, -1, "ON THIS PC", NULL, NULL, NULL };
	for (at = 0; at < screen.count && at < LIST_PROFILES; at++)
	{
		screen.profile_of_row[count] = at;
		flow->rows[count++] = (struct ae_flow_row){ ROW_PROFILE, -1, screen.profiles[at].name,
			screen.profiles[at].index == screen.player1 ? "PRIMARY" : NULL,
			screen.profiles[at].index == screen.player1 ? "{A} or {Y} edits it; {X} deletes it." :
			"{A} makes it your primary profile; {Y} edits it; {X} deletes it.", NULL };
	}
	screen.profile_of_row[count] = -1;
	flow->rows[count++] = (struct ae_flow_row){ ROW_NEW, -1, "NEW PROFILE", NULL,
		"A profile keeps a player's name, colour, controls and campaign.", NULL };
	/* (0.7.7) this PC's moderator key: A copies it; a server owner lists it in admins.txt to make this player a moderator */
	{
		char key[80];

		if (ae_moderator_key_text(key, (int)sizeof(key)))
		{
			static char value[24];
			boolean copied = screen.key_copied_at && system_milliseconds() - screen.key_copied_at < KEY_NOTE_MS;

			snprintf(value, sizeof(value), "%.8s...", key);
			screen.profile_of_row[count] = -1;
			flow->rows[count++] = (struct ae_flow_row){ ROW_HEADER, -1, "FOR SERVERS", NULL, NULL, NULL };
			screen.profile_of_row[count] = -1;
			flow->rows[count++] = (struct ae_flow_row){ ROW_MODERATOR_KEY, -1, "MODERATOR KEY", value,
				copied ? "Copied: your moderator key is on the clipboard." :
				"Give this to a server owner to make you a moderator. {A} copies it.", NULL };
		}
	}
	flow->count = count;
	if (flow->focus >= count)
		flow->focus = (short)(count - 1);
	if (flow->rows[flow->focus].kind == ROW_HEADER)
		flow->focus++;
}

static struct ae_profile_summary const *focused_profile(void)
{
	struct ae_flow_screen const *flow = &screen.list.flow;
	short row = flow->focus;

	if (row < 0 || row >= flow->count || screen.profile_of_row[row] < 0)
		return NULL;
	return &screen.profiles[screen.profile_of_row[row]];
}

/* a colour square, then the name beside it; returns the height used */
/* ---------- the Spartan's side (PROFILES and EDIT): one centred column of the panel. The game's own model stands on a
floor line in the profile's colour, a name under the line, and under that EDIT's colours, nine a row, which end on the
line of the help's rule. The game draws him at once (the screens' shapes are gathered for the frame's end), so nothing
of a screen lies over his rectangle. With no model to draw (the maps have none, or it is not read yet) a square of the
colour stands in his place */
enum { SIDE_COLUMNS = 9 };

struct side
{
	float center, top, floor, block_x, block_y, block_width, cell, gap;
};

static void side_layout(struct ae_flow_screen *flow, struct side *side)
{
	float x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X, width = ae_flow_right_x(flow) - x;
	/* (the rows end 28 u over the help's text and its rule is half way: ae_setup_flow.c) */
	float bottom = flow->rows_limit + ae_flow_u(flow, 14.0f);
	short rows = (short)((ae_profile_color_count() + SIDE_COLUMNS - 1) / SIDE_COLUMNS);

	if (flow->rows_limit <= 0.0f || bottom > ae_flow_content_bottom(flow))
		bottom = ae_flow_content_bottom(flow);
	if (rows < 1)
		rows = 1;
	side->gap = ae_flow_u(flow, 14.0f);
	side->cell = ae_flow_u(flow, 42.0f);
	if ((float)SIDE_COLUMNS * side->cell + (float)(SIDE_COLUMNS - 1) * side->gap > width)
		side->cell = (width - (float)(SIDE_COLUMNS - 1) * side->gap) / (float)SIDE_COLUMNS;
	side->block_width = (float)SIDE_COLUMNS * side->cell + (float)(SIDE_COLUMNS - 1) * side->gap;
	side->center = x + width * 0.5f;
	side->block_x = side->center - side->block_width * 0.5f;
	side->block_y = bottom - ((float)rows * side->cell + (float)(rows - 1) * side->gap);
	side->floor = side->block_y - ae_flow_u(flow, 62.0f);
	side->top = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y) - ae_flow_u(flow, 12.0f);
}

/* him in that colour (else the colour's square), the floor line and the name under it */
static void draw_side_figure(struct ae_flow_screen *flow, struct side const *side, short color, const char *name)
{
	float bottom = side->floor - ae_flow_u(flow, 2.0f), height = bottom - side->top, width = height * 0.62f;
	/* (the panel's whole width less the model's own side margin, 36 of 480 lines a side: turned side on, what he holds
	reaches past his standing width and would be cut at the view's edges) */
	float room = 2.0f * (ae_flow_right_x(flow) - side->center) - 2.0f * 36.0f * 1080.0f / 480.0f * ae_flow_u(flow, 1.0f);
	float size = ae_size_text(&flow->density) * 1.25f;

	if (room > width)
		width = room;
	/* (the game's viewports are in its own 480-line picture; the screens' units are 1080 lines of the same) */
	float lines = 480.0f / 1080.0f;
	boolean drawn = FALSE;

	if (color < 0)
		color = 0;
	if (height > ae_flow_u(flow, 200.0f) && !screen.plain)
	{
		drawn = menu_chief_draw((short)((side->center - width * 0.5f) * lines), (short)(side->top * lines),
			(short)((side->center + width * 0.5f) * lines), (short)(bottom * lines), color, TRUE);
		ae_hit_add(side->center - width * 0.5f, side->top, width, height, HIT_FIGURE, 0, 0);
	}
	if (!drawn)
	{
		float square = ae_flow_u(flow, 260.0f), room = height - ae_flow_u(flow, 48.0f);

		if (square > room)
			square = room;
		if (square > 0.0f)
		{
			ae_draw_rect(side->center - square * 0.5f, side->floor - ae_flow_u(flow, 24.0f) - square, square, square, 0.0f,
				ae_profile_color_rgba(color));
			ae_draw_outline(side->center - square * 0.5f, side->floor - ae_flow_u(flow, 24.0f) - square, square, square,
				0.0f, ae_flow_u(flow, AE_FLOW_RULE_U), AE_COLOR_RULE);
		}
	}
	/* (the name on a plate of the rows' own material, the floor its top edge: the menu's picture moves behind this side) */
	ae_draw_rect(side->block_x, side->floor, side->block_width, ae_flow_u(flow, 48.0f), 0.0f, AE_COLOR_ROW);
	ae_draw_rect(side->block_x, side->floor, side->block_width, ae_flow_u(flow, 2.0f), 0.0f, AE_COLOR_MUTED);
	ae_draw_text(AE_FONT_ROW, size, side->center, side->floor + ae_flow_u(flow, 25.0f) -
		ae_draw_cap_height(AE_FONT_ROW, size) * 0.5f, AE_ALIGN_CENTER, AE_COLOR_TITLE, name);
}

static void draw_list_side(void)
{
	struct ae_flow_screen *flow = &screen.list.flow;
	float x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X;
	float y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y), minor = ae_size_minor(&flow->density);
	struct ae_profile_summary const *profile = focused_profile();
	struct side side;
	char line[96];

	if (!profile)
	{
		ae_draw_text(AE_FONT_ROW, ae_size_text(&flow->density) * 1.5f, x, y + ae_flow_u(flow, 30.0f), AE_ALIGN_LEFT,
			AE_COLOR_TITLE, "NEW PROFILE");
		ae_draw_text(AE_FONT_BODY, minor, x, y + ae_flow_u(flow, 74.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			screen.count ? "Name it; player 1 then plays with it." :
			"No profile on this PC yet: name one, and player 1 plays with it.");
		return;
	}
	if (screen.side_index != profile->index)
	{
		/* (read once per focus, not every frame: a profile's file is read from disk) */
		screen.side_index = profile->index;
		memset(&screen.side_controls, 0, sizeof(screen.side_controls));
		screen.side_color = -1;
		ae_profile_read(profile->index, &screen.side_controls, &screen.side_color);
	}
	side_layout(flow, &side);
	draw_side_figure(flow, &side, screen.side_color, profile->name);
	/* (where EDIT has the colours: the profile's settings, as two lines) */
	snprintf(line, sizeof(line), "%s  \xC2\xB7  %s  \xC2\xB7  LOOK %d", color_name(screen.side_color),
		layout_name(screen.side_controls.layout), screen.side_controls.look_sensitivity);
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.06f, side.center, side.block_y + ae_flow_u(flow, 12.0f), AE_ALIGN_CENTER,
		AE_COLOR_TEXT, line);
	snprintf(line, sizeof(line), "INVERT %s  \xC2\xB7  VIBRATION %s", screen.side_controls.invert ? "ON" : "OFF",
		screen.side_controls.vibration ? "ON" : "OFF");
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.06f, side.center, side.block_y + ae_flow_u(flow, 44.0f), AE_ALIGN_CENTER,
		AE_COLOR_TEXT, line);
}

/* ---------- EDIT */

static void rows_edit(void)
{
	struct ae_flow_screen *flow = &screen.edit.flow;
	short count = 0;

	snprintf(flow->value_texts[2], sizeof(flow->value_texts[2]), "%d", screen.controls.look_sensitivity);
	flow->rows[count++] = (struct ae_flow_row){ ROW_HEADER, -1, "PLAYER", NULL, NULL, NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_RENAME, -1, "RENAME", screen.editing_name,
		"Type a new name. Its colour, controls and campaign progress stay.", NULL, .subscreen = 1 };
	/* (the colours have the focus: the row is not lit, shows the one focused, and its help says what A and B do) */
	if (screen.swatch >= 0)
		snprintf(flow->value_texts[3], sizeof(flow->value_texts[3]), "{A} keeps %s. {B} goes back to %s.",
			color_name(screen.swatch), color_name(screen.color));
	flow->rows[count++] = (struct ae_flow_row){ ROW_COLOR, -1, "COLOUR",
		color_name(screen.swatch >= 0 ? screen.swatch : screen.color), screen.swatch >= 0 ? flow->value_texts[3] :
		"Your armour's colour, from the next game you join. {A} opens the colours; the right stick turns the Spartan.",
		NULL, .quiet = screen.swatch >= 0 };
	flow->rows[count++] = (struct ae_flow_row){ ROW_HEADER, -1, "CONTROLS", NULL, NULL, NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_LAYOUT, -1, "BUTTON LAYOUT", layout_name(screen.controls.layout),
		"The controller's buttons: Halo's own, Anniversary's and the Master Chief Collection's layouts.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_LOOK, -1, "LOOK SENSITIVITY", flow->value_texts[2],
		"How fast the right stick turns you: 1 to 10.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_INVERT, -1, "INVERT LOOK", screen.controls.invert ? "ON" : "OFF",
		"Up on the right stick looks down.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_VIBRATION, -1, "VIBRATION",
		screen.controls.vibration ? "ON" : "OFF", "The controller's rumble.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_HEADER, -1, "PROGRESS", NULL, NULL, NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_EXPORT, -1, "EXPORT PROGRESS", NULL,
		"Save this profile's ranks, service record and bot records to a file, to carry them to another PC.", NULL,
		.subscreen = 1 };
	flow->rows[count++] = (struct ae_flow_row){ ROW_IMPORT, -1, "IMPORT PROGRESS", NULL,
		"Load ranks and records from a file exported on another PC. This profile's records are backed up first.", NULL,
		.subscreen = 1 };
	flow->count = count;
	if (flow->focus >= count)
		flow->focus = (short)(count - 1);
	if (flow->rows[flow->focus].kind == ROW_HEADER)
		flow->focus++;
}

/* the profile EDIT shows, read again from its file */
static int edit_read(void)
{
	memset(&screen.controls, 0, sizeof(screen.controls));
	screen.color = -1;
	return ae_profile_read(screen.editing, &screen.controls, &screen.color).ok;
}

static void edit_step(short row, short direction)
{
	struct ae_flow_screen *flow = &screen.edit.flow;
	struct ae_profile_controls controls = screen.controls;
	struct ae_result result;
	short color = screen.color;

	if (row < 0 || row >= flow->count)
		return;
	switch (flow->rows[row].kind)
	{
	case ROW_COLOR:
		color = ae_value_step(ae_profile_color_count(), color < 0 ? 0 : color, direction);
		if (color == screen.color)
			break;
		result = ae_profile_save_color(screen.editing, color);
		goto saved;
	case ROW_LAYOUT:
		controls.layout = ae_value_step((short)(sizeof(layout_names) / sizeof(layout_names[0])), controls.layout,
			direction);
		break;
	case ROW_LOOK:
		/* (stored as Halo shows it, 1 to 10) */
		controls.look_sensitivity = 1 + ae_value_step(10, controls.look_sensitivity - 1, direction);
		break;
	case ROW_INVERT:
		controls.invert = !controls.invert;
		break;
	case ROW_VIBRATION:
		controls.vibration = !controls.vibration;
		break;
	default:
		return;
	}
	if (!memcmp(&controls, &screen.controls, sizeof(controls)) && color == screen.color)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	result = ae_profile_save_controls(screen.editing, &controls);
saved:
	if (!result.ok)
	{
		ae_flow_error(flow, "Not saved", result.reason);
		edit_read();
		return;
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
	edit_read();
	/* (the list shows the change: its colour square and the sub-line's layout) */
	profiles_read();
}

/* the rows' focus on COLOUR (the colours take theirs from that row) */
static void focus_colour_row(void)
{
	struct ae_flow_screen *flow = &screen.edit.flow;
	short row;

	for (row = 0; row < flow->count; row++)
		if (flow->rows[row].kind == ROW_COLOR)
			flow->focus = row;
}

/* the colours close: kept, the focused one becomes the profile's; else the profile's stays */
static void swatch_close(int keep)
{
	short color = screen.swatch;

	screen.swatch = -1;
	screen.swatch_hover = 0;
	if (!keep || color < 0 || color == screen.color)
	{
		ae_sound_request(keep ? AE_SOUND_CURSOR : AE_SOUND_BACK, 0);
		return;
	}
	{
		struct ae_result result = ae_profile_save_color(screen.editing, color);

		if (!result.ok)
		{
			ae_flow_error(&screen.edit.flow, "Not saved", result.reason);
			edit_read();
			return;
		}
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
	edit_read();
	profiles_read();
}

/* the colours have the focus: the stick moves over them (left from the first column goes back to the row), A keeps
the focused one, B goes back; nonzero always (the rows get nothing) */
static int swatch_handle(struct ae_event const *event)
{
	short count = ae_profile_color_count(), next = screen.swatch;

	switch (event->action)
	{
	case AE_ACTION_LEFT:
		if (next % SIDE_COLUMNS == 0)
		{
			swatch_close(0);
			return 1;
		}
		next--;
		break;
	case AE_ACTION_RIGHT:
		if (next % SIDE_COLUMNS != SIDE_COLUMNS - 1 && next + 1 < count)
			next++;
		break;
	case AE_ACTION_UP:
		if (next >= SIDE_COLUMNS)
			next -= SIDE_COLUMNS;
		break;
	case AE_ACTION_DOWN:
		if (next + SIDE_COLUMNS < count)
			next += SIDE_COLUMNS;
		break;
	case AE_ACTION_ACCEPT:
		swatch_close(1);
		return 1;
	case AE_ACTION_BACK:
		swatch_close(0);
		return 1;
	default:
		return 1;
	}
	if (next != screen.swatch)
	{
		screen.swatch = next;
		screen.swatch_hover = 0;
		ae_sound_request(AE_SOUND_CURSOR, 0);
	}
	return 1;
}

static void draw_edit_side(void)
{
	struct ae_flow_screen *flow = &screen.edit.flow;
	short count = ae_profile_color_count(), color, row;
	short shown = screen.swatch >= 0 ? screen.swatch : screen.color;
	struct side side;

	side_layout(flow, &side);
	draw_side_figure(flow, &side, shown, color_name(shown));
	for (color = 0; color < count; color++)
	{
		float cx = side.block_x + (float)(color % SIDE_COLUMNS) * (side.cell + side.gap);
		float cy = side.block_y + (float)(color / SIDE_COLUMNS) * (side.cell + side.gap);
		float out = ae_flow_u(flow, 5.0f);

		ae_draw_rect(cx, cy, side.cell, side.cell, 0.0f, ae_profile_color_rgba(color));
		ae_draw_outline(cx, cy, side.cell, side.cell, 0.0f, ae_flow_u(flow, color == screen.color ? 3.0f : AE_FLOW_RULE_U),
			color == screen.color ? AE_COLOR_TITLE : AE_COLOR_RULE);
		if (color == screen.swatch)
			ae_draw_outline(cx - out, cy - out, side.cell + out * 2.0f, side.cell + out * 2.0f, 0.0f, ae_flow_u(flow, 3.0f),
				AE_COLOR_ACCENT);
		ae_hit_add(cx, cy, side.cell, side.cell, HIT_SWATCH, 0, color);
	}
	/* (the COLOUR row the focus came from keeps a lit edge) */
	for (row = 0; screen.swatch >= 0 && row < flow->count; row++)
		if (flow->rows[row].kind == ROW_COLOR && flow->rects[row].width > 0.0f)
			ae_draw_rect(flow->rects[row].x, flow->rects[row].y, ae_flow_u(flow, 6.0f), flow->rects[row].height, 0.0f,
				AE_COLOR_ACCENT);
}

/* EDIT's pointer on the side: over a colour he shows it and a click keeps it (off the colours again, the profile's is
back); held down on him and moved, it turns him. Nonzero when the pointer was the side's */
static int edit_pointer(struct ae_pointer const *pointer)
{
	struct ae_hit hit;
	int over = ae_hit_at(pointer->x, pointer->y, 0, &hit);

	if (screen.turning && pointer->left_held)
	{
		/* (a drag across the panel's width is about one turn) */
		menu_chief_turn((pointer->x - screen.turn_x) * 6.2832f / ae_flow_u(&screen.edit.flow, 700.0f));
		screen.turn_x = pointer->x;
		return 1;
	}
	screen.turning = 0;
	if (over && hit.id == HIT_FIGURE)
	{
		if (pointer->left_held)
		{
			screen.turning = 1;
			screen.turn_x = pointer->x;
		}
		return 1;
	}
	if (over && hit.id == HIT_SWATCH)
	{
		if (screen.swatch != hit.index)
		{
			screen.swatch = hit.index;
			screen.swatch_hover = 1;
			focus_colour_row();
		}
		if (pointer->left_clicks)
			swatch_close(1);
		return 1;
	}
	if (screen.swatch >= 0 && screen.swatch_hover && pointer->moved)
	{
		screen.swatch = -1;
		screen.swatch_hover = 0;
	}
	return 0;
}

/* ---------- NEW PROFILE */

static void name_field(struct ae_field *field)
{
	memset(field, 0, sizeof(*field));
	field->label = "NAME";
	field->placeholder = "Up to 11 characters";
	field->text = &screen.name;
	field->flags = AE_FIELD_FOCUSED | (screen.typing ? AE_FIELD_EDITING : 0);
	field->caret_since = screen.caret_since;
}

/* the name typed: the profile made, player 1 plays with it, back to the list on it */
static void name_rename(void)
{
	struct ae_result result = ae_profile_rename(screen.editing, screen.name.text);
	struct ae_profile_summary const *profile;

	if (!result.ok)
	{
		ae_flow_error(&screen.made.flow, "Not renamed", result.reason);
		return;
	}
	profiles_read();
	profile = profile_by_index(screen.editing);
	if (profile)
		snprintf(screen.editing_name, sizeof(screen.editing_name), "%s", profile->name);
	edit_read();
	platform_log("ae profiles screen: renamed to '%s'", screen.editing_name);
	screen.renaming = 0;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_ui_remove(&screen.made);
}

static void name_create(void)
{
	struct ae_result result;
	int made = -1;

	if (screen.renaming)
	{
		name_rename();
		return;
	}
	result = ae_profile_new(screen.name.text, &made);
	if (!result.ok || made == -1)
	{
		ae_flow_error(&screen.made.flow, "No profile made", result.reason);
		return;
	}
	result = ae_profile_switch(screen.for_player, made);
	if (!result.ok)
		ae_flow_error(screen.for_player ? &screen.made.flow : &screen.list.flow, "Made, not used", result.reason);
	platform_log("ae profiles screen: made '%s' (player %d)", screen.name.text, screen.for_player + 1);
	if (screen.for_player)
	{
		ae_screen_profiles_made done = screen.for_done;

		/* (the page alone: it closes, and the card that opened it is done) */
		screen.for_done = NULL;
		ae_ui_remove(&screen.made);
		if (done && result.ok)
			done(screen.for_player, 1, screen.for_context);
		return;
	}
	profiles_read();
	rows_list();
	{
		short row;

		for (row = 0; row < screen.list.flow.count; row++)
		{
			if (screen.profile_of_row[row] >= 0 && screen.profiles[screen.profile_of_row[row]].index == made)
				screen.list.flow.focus = row;
		}
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_ui_remove(&screen.made);
}

static void typing_done(int keep, void *context)
{
	(void)keep;
	(void)context;
	screen.typing = 0;
}

static void typing_begin(void)
{
	screen.field_edit.done = typing_done;
	screen.field_edit.context = NULL;
	screen.field_edit.holder = &screen.made;
	ae_field_edit_begin(&screen.field_edit, &screen.name);
	ae_text_select_all(&screen.name);
	screen.typing = 1;
	screen.caret_since = ae_motion_now();
	platform_log("ae profiles screen: typing");
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void typing_end(int keep)
{
	if (!screen.typing)
		return;
	screen.typing = 0;
	ae_field_edit_end(&screen.field_edit, keep);
	platform_log("ae profiles screen: name '%s' %s", screen.name.text, keep ? "kept" : "dropped");
	if (keep && screen.name.length)
		name_create();
	else
		ae_sound_request(keep ? AE_SOUND_FAILURE : AE_SOUND_BACK, 0);
}

static void keyboard_done(int keep, void *context)
{
	(void)context;
	screen.keyboard_open = 0;
	/* (a physical key closed it into the field's own typing: that goes on) */
	if (screen.field_edit.active && screen.field_edit.text == &screen.name)
	{
		screen.typing = 1;
		screen.caret_since = ae_motion_now();
		return;
	}
	if (keep && screen.name.length)
		name_create();
}

static void keyboard_open(void)
{
	struct ae_keyboard_spec spec;

	memset(&spec, 0, sizeof(spec));
	spec.text = &screen.name;
	spec.field = screen.well;
	spec.bounds = screen.made.flow.frame.rect;
	spec.density = screen.made.flow.density;
	spec.done = keyboard_done;
	screen.field_edit.done = typing_done;
	screen.field_edit.context = NULL;
	screen.field_edit.holder = &screen.made;
	spec.edit = &screen.field_edit;
	/* (NEW PROFILE alone for a split-screen player: their keyboard) */
	if (ae_keyboard_open(&spec, screen.for_player ? screen.for_player : AE_OWNER_ANY))
		screen.keyboard_open = 1;
}

static void draw_new(void)
{
	struct ae_flow_screen *flow = &screen.made.flow;
	struct ae_density const *d = &flow->density;
	struct ae_field field;
	float x = ae_flow_left_x(flow), width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH;
	float y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);

	name_field(&field);
	ae_field_well(d, x, y, width, &field, &screen.well);
	y += ae_widget_field(d, x, y, width, &field, HIT_FIELD) + ae_flow_u(flow, 18.0f);
	ae_draw_text(AE_FONT_BODY, ae_size_minor(d), x, y + ae_flow_u(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
		screen.renaming ? "Letters, numbers and spaces. Its colour, controls and campaign progress stay." :
		screen.for_player == 1 ? "Letters, numbers and spaces. Player 2 plays with it once it is made." :
		screen.for_player == 2 ? "Letters, numbers and spaces. Player 3 plays with it once it is made." :
		screen.for_player == 3 ? "Letters, numbers and spaces. Player 4 plays with it once it is made." :
		"Letters, numbers and spaces. Player 1 plays with it once it is made.");
}

/* ---------- the screen */

static struct page *page_of(struct ae_screen const *instance)
{
	return (struct page *)instance->data;
}

static void import_ask(int index);

static void profiles_draw(struct ae_screen *instance)
{
	struct page *page = page_of(instance);
	struct ae_flow_screen *flow = &page->flow;
	struct ae_profile_summary const *player1 = profile_by_index(screen.player1);
	char crumb[64];

	ae_flow_full_density(flow);
	flow->prompt_count = 0;
	switch (page->kind)
	{
	case PAGE_LIST:
		rows_list();
		snprintf(crumb, sizeof(crumb), "PRIMARY: %s", player1 ? player1->name : "NO PROFILE");
		ae_flow_title(flow, "PROFILES", crumb);
		ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
		draw_list_side();
		if (focused_profile())
		{
			/* (owner 11-11: A / a click picks the profile, Y / R edits it; A on the primary edits it too) */
			if (focused_profile()->index != screen.player1)
				flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Make primary", AE_ACTION_ACCEPT);
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Edit", AE_ACTION_Y);
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Delete", AE_ACTION_X);
			/* (0.7.6) the profile's service record: RB */
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_RIGHT_SHOULDER, "Service record",
				AE_ACTION_TAB_NEXT);
		}
		else
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A,
				flow->rows[flow->focus].kind == ROW_MODERATOR_KEY ? "Copy key" : ae_string(AE_STR_SELECT),
				AE_ACTION_ACCEPT);
		break;
	case PAGE_EDIT:
		rows_edit();
		ae_flow_title(flow, screen.editing_name, "PROFILES");
		ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
		draw_edit_side();
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, screen.swatch >= 0 ? "Keep colour" :
			flow->rows[flow->focus].kind == ROW_RENAME ? "Rename" : flow->rows[flow->focus].kind == ROW_COLOR ? "Colours" :
			flow->rows[flow->focus].kind == ROW_EXPORT ? "Export" : flow->rows[flow->focus].kind == ROW_IMPORT ? "Import" :
			"Change", AE_ACTION_ACCEPT);
		break;
	case PAGE_FILES:
		rows_files();
		ae_flow_title(flow, "IMPORT PROGRESS", screen.editing_name);
		ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Choose", AE_ACTION_ACCEPT);
		if (screen.stage_import_ask && flow->density.metrics)
		{
			screen.stage_import_ask = 0;
			import_ask(0);
		}
		break;
	default:
		ae_flow_title(flow, screen.renaming ? "RENAME" : "NEW PROFILE", screen.renaming ? screen.editing_name : "PROFILES");
		draw_new();
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A,
			screen.renaming ? (screen.typing ? "Rename it" : "Type a name") : screen.typing ? "Make it" : "Name it",
			AE_ACTION_ACCEPT);
		break;
	}
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

static void profiles_update(struct ae_screen *instance)
{
	/* (typing into NEW PROFILE's name: this frame's keys) */
	if (page_of(instance)->kind == PAGE_NEW && screen.typing && screen.field_edit.active && !ae_field_edit_keys(&screen.field_edit))
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

static int page_push(struct page *page)
{
	memset(&page->flow, 0, sizeof(page->flow));
	/* (EDIT opens with the focus in its rows) */
	screen.swatch = -1;
	screen.swatch_hover = screen.turning = 0;
	page->flow.hit_rows = HIT_ROWS;
	page->flow.hit_prompts = HIT_PROMPTS;
	return ae_ui_push(&profiles_class, AE_OWNER_ANY, page);
}

static void delete_picked(short choice, void *context)
{
	int index = (int)(intptr_t)context;
	struct ae_result result;

	if (choice != 1)
		return;
	result = ae_profile_delete(index);
	if (!result.ok)
	{
		ae_flow_error(&screen.list.flow, "Not deleted", result.reason);
		return;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	profiles_read();
}

static void delete_ask(struct ae_profile_summary const *profile)
{
	static char title[80];
	struct ae_dialog_spec spec;

	if (profile->index == screen.player1)
	{
		ae_flow_error(&screen.list.flow, "Not deleted", ae_string(AE_STR_ERR_PROFILE_IN_USE));
		return;
	}
	snprintf(title, sizeof(title), "Delete %s?", profile->name);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = title;
	spec.body = "Its colour, controls and campaign progress go with it.";
	spec.choices[0] = ae_string(AE_STR_CANCEL_CAPS);
	spec.choices[1] = "DELETE";
	spec.choice_count = 2;
	spec.has_cancel = 1;
	spec.density = screen.list.flow.density;
	spec.bounds = screen.list.flow.frame.rect;
	spec.picked = delete_picked;
	spec.context = (void *)(intptr_t)profile->index;
	ae_dialog_open(&spec, AE_OWNER_ANY);
}

/* ---------- the progress file: EXPORT PROGRESS, IMPORT PROGRESS ---------- */

static struct
{
	char file[AE_PM_FILE_NAME];     /* the file chosen (the exports folder's) */
	char to[48];                    /* the profile it goes into */
	char backup[AE_PM_FILE_NAME];   /* what the import kept of the profile's progress */
	char title[96];
	char body[720];
	char reason[200];
	char help[AE_PM_LIST_MAX][120];
} move;

static void dialog_basics(struct ae_dialog_spec *spec, struct ae_flow_screen const *flow)
{
	memset(spec, 0, sizeof(*spec));
	spec->kind = AE_DIALOG_CONFIRM;
	spec->density = flow->density;
	spec->bounds = flow->frame.rect;
	spec->has_cancel = 1;
}

/* a message with DONE, and a second choice (a folder opened, an import undone) when second is given */
static void notice(struct ae_flow_screen const *flow, char const *title, char const *body, char const *second,
	void (*picked)(short choice, void *context))
{
	struct ae_dialog_spec spec;

	dialog_basics(&spec, flow);
	spec.title = title;
	spec.body = body;
	spec.choices[0] = "DONE";
	spec.choice_count = 1;
	if (second)
	{
		spec.choices[1] = second;
		spec.choice_count = 2;
	}
	spec.picked = picked;
	ae_dialog_open(&spec, AE_OWNER_ANY);
}

/* a game running: the progress files change the records a game writes (0: none is) */
static int game_running(struct ae_flow_screen *flow, char const *title)
{
	if (game_in_progress() && !main_menu_is_active())
	{
		ae_flow_error(flow, title, "Not while a game is running. Leave the game first.");
		return 1;
	}
	return 0;
}

static void exported_picked(short choice, void *context)
{
	(void)context;
	if (choice == 1 && !ae_profile_move_open_folder(NULL, 0))
		ae_flow_error(&screen.edit.flow, "Can't open the folder", "Open AE's save folder yourself: the exports folder is inside it.");
}

static void export_picked(short choice, void *context)
{
	char file[AE_PM_FILE_NAME];
	int counts[3];

	(void)context;
	if (choice != 1 || game_running(&screen.edit.flow, "Not exported"))
		return;
	if (!ae_profile_move_export(screen.editing_name, file, sizeof(file), counts, move.reason, sizeof(move.reason)))
	{
		ae_flow_error(&screen.edit.flow, "Not exported", move.reason);
		return;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	snprintf(move.title, sizeof(move.title), "Saved %s's progress", screen.editing_name);
	snprintf(move.body, sizeof(move.body), "Saved as %s in the exports folder. To move it: copy the file into the other PC's exports "
		"folder (OPEN FOLDER shows this PC's), then choose PROFILES > EDIT > IMPORT PROGRESS there.", file);
	notice(&screen.edit.flow, move.title, move.body, "OPEN FOLDER", exported_picked);
}

static void export_ask(void)
{
	struct ae_dialog_spec spec;

	if (game_running(&screen.edit.flow, "Not now"))
		return;

	dialog_basics(&spec, &screen.edit.flow);
	snprintf(move.title, sizeof(move.title), "Export %s's progress?", screen.editing_name);
	spec.title = move.title;
	spec.body = "Saves this profile's ranks, service record and bot records as one file in the exports folder. Its colour, "
		"controls and online identity stay on this PC, so the file is safe to copy.";
	spec.choices[0] = ae_string(AE_STR_CANCEL_CAPS);
	spec.choices[1] = "EXPORT";
	spec.choice_count = 2;
	spec.safe_choice = 1;
	spec.cancel_choice = 0;
	spec.picked = export_picked;
	ae_dialog_open(&spec, AE_OWNER_ANY);
}

static void undone_picked(short choice, void *context)
{
	(void)choice;
	(void)context;
}

static void imported_picked(short choice, void *context)
{
	(void)context;
	if (choice != 1 || game_running(&screen.files.flow, "Not undone"))
		return;
	if (!ae_profile_move_undo(move.backup, move.to, move.reason, sizeof(move.reason)))
	{
		ae_flow_error(&screen.files.flow, "Not undone", move.reason);
		return;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	snprintf(move.body, sizeof(move.body), "This profile's ranks and records are back as they were before the import.");
	notice(&screen.files.flow, "Import undone", move.body, NULL, undone_picked);
}

static void import_picked(short choice, short chip, void *context)
{
	struct ae_pm_summary summary;
	char words[320];

	(void)context;
	if (choice != 1 || game_running(&screen.files.flow, "Not imported"))
		return;
	if (!ae_profile_move_import(move.file, move.to, chip == 1 ? AE_PM_REPLACE : AE_PM_MERGE, &summary, move.backup,
		sizeof(move.backup), move.reason, sizeof(move.reason)))
	{
		ae_flow_error(&screen.files.flow, "Not imported", move.reason);
		return;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_profile_move_summary_text(&summary, 3, words, sizeof(words));
	snprintf(move.title, sizeof(move.title), "Imported into %s", move.to);
	snprintf(move.body, sizeof(move.body), "%s Changed your mind? UNDO IMPORT puts the old records back (a copy is also kept in "
		"exports/backup).", words);
	notice(&screen.files.flow, move.title, move.body, "UNDO IMPORT", imported_picked);
}

static void import_ask(int index)
{
	struct ae_dialog_spec spec;
	struct ae_pm_summary merge, replace;
	struct ae_pm_listing const *listing;
	char merge_words[200], replace_words[160];

	if (index < 0 || index >= screen.listing_count)
		return;
	listing = &screen.listing[index];
	if (game_running(&screen.files.flow, "Not now"))
		return;
	if (listing->error)
	{
		ae_flow_error(&screen.files.flow, "Can't use that file", ae_pm_error_text(listing->error));
		return;
	}
	snprintf(move.file, sizeof(move.file), "%s", listing->name);
	snprintf(move.to, sizeof(move.to), "%s", screen.editing_name);
	if (!ae_profile_move_preview(move.file, move.to, AE_PM_MERGE, &merge, move.reason, sizeof(move.reason)) ||
		!ae_profile_move_preview(move.file, move.to, AE_PM_REPLACE, &replace, move.reason, sizeof(move.reason)))
	{
		ae_flow_error(&screen.files.flow, "Can't use that file", move.reason);
		return;
	}
	ae_profile_move_summary_text(&merge, 2, merge_words, sizeof(merge_words));
	ae_profile_move_summary_text(&replace, 0, replace_words, sizeof(replace_words));
	dialog_basics(&spec, &screen.files.flow);
	snprintf(move.title, sizeof(move.title), "Import %s into %s?", listing->profile, move.to);
	snprintf(move.body, sizeof(move.body), "MERGE: for each playlist, the record with more games played wins, this PC's or the "
		"file's. REPLACE: the file's records take the place of this profile's. Either way the old records are backed up "
		"and UNDO IMPORT can put them back. MERGE: %s REPLACE: %s", merge_words, replace_words);
	spec.title = move.title;
	spec.body = move.body;
	spec.choices[0] = ae_string(AE_STR_CANCEL_CAPS);
	spec.choices[1] = "IMPORT";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.chips[0] = "MERGE";
	spec.chips[1] = "REPLACE";
	spec.chip_count = 2;
	spec.chip_first = 0;
	spec.chip_choice = 1;
	spec.chip_labels[0] = "IMPORT AND MERGE";
	spec.chip_labels[1] = "IMPORT AND REPLACE";
	spec.picked_chip = import_picked;
	ae_dialog_open(&spec, AE_OWNER_ANY);
}

static void rows_files(void)
{
	struct ae_flow_screen *flow = &screen.files.flow;
	short count = 0;
	int at;

	for (at = 0; at < screen.listing_count && count < AE_FLOW_ROWS; at++)
	{
		struct ae_pm_listing const *listing = &screen.listing[at];

		snprintf(flow->value_texts[count], sizeof(flow->value_texts[count]), "%s", listing->error ? "NOT USABLE" : listing->profile);
		snprintf(move.help[at], sizeof(move.help[at]), "%s", listing->error ? ae_pm_error_text(listing->error) :
			"{A} shows what this file would change. Nothing is imported until you confirm.");
		flow->rows[count] = (struct ae_flow_row){ ROW_FILE, -1, listing->name, flow->value_texts[count], move.help[at], NULL };
		count++;
	}
	flow->count = count;
	if (flow->focus >= count)
		flow->focus = (short)(count > 0 ? count - 1 : 0);
}

static int files_begin(void)
{
	if (game_running(&screen.edit.flow, "Not now"))
		return 0;
	screen.listing_count = ae_profile_move_list(screen.listing, AE_PM_LIST_MAX);
	if (screen.listing_count <= 0)
	{
		notice(&screen.edit.flow, "No progress files yet", "On the other PC, choose PROFILES > EDIT > EXPORT PROGRESS. Copy the "
			"file it saves into this PC's exports folder (OPEN FOLDER shows it), then choose IMPORT PROGRESS again.",
			"OPEN FOLDER", exported_picked);
		return 0;
	}
	if (!page_push(&screen.files))
		return 0;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("ae profiles screen: import progress into '%s' (%d files)", screen.editing_name, screen.listing_count);
	return 1;
}

/* RENAME: the name page on the profile's own name */
static void rename_begin(void)
{
	ae_text_init(&screen.name, screen.editing_name, NAME_LIMIT, 0);
	screen.typing = screen.keyboard_open = 0;
	screen.renaming = 1;
	if (page_push(&screen.made))
	{
		platform_log("ae profiles screen: rename '%s'", screen.editing_name);
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
	else
		screen.renaming = 0;
}

static void list_accept(short row)
{
	struct ae_flow_screen *flow = &screen.list.flow;
	struct ae_profile_summary const *profile;

	if (row < 0 || row >= flow->count)
		return;
	if (flow->rows[row].kind == ROW_NEW)
	{
		ae_text_init(&screen.name, "", NAME_LIMIT, 0);
		screen.typing = screen.keyboard_open = 0;
		screen.renaming = 0;
		if (page_push(&screen.made))
		{
			platform_log("ae profiles screen: new profile");
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		return;
	}
	if (flow->rows[row].kind == ROW_MODERATOR_KEY)
	{
		char key[80];

		if (ae_moderator_key_text(key, (int)sizeof(key)))
		{
			ae_glue_clipboard_set(key);
			screen.key_copied_at = system_milliseconds();
			platform_log("ae profiles screen: the moderator key is on the clipboard");
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else
			ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (screen.profile_of_row[row] < 0)
		return;
	profile = &screen.profiles[screen.profile_of_row[row]];
	screen.editing = profile->index;
	snprintf(screen.editing_name, sizeof(screen.editing_name), "%s", profile->name);
	if (!edit_read())
	{
		ae_flow_error(flow, "Can't edit it", ae_string(AE_STR_ERR_PROFILE));
		return;
	}
	if (page_push(&screen.edit))
	{
		platform_log("ae profiles screen: edit '%s'", profile->name);
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
}

static void list_use(void)
{
	struct ae_profile_summary const *profile = focused_profile();
	struct ae_result result;

	if (!profile || profile->index == screen.player1)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	result = ae_profile_switch(0, profile->index);
	if (!result.ok)
	{
		ae_flow_error(&screen.list.flow, "Not changed", result.reason);
		return;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	profiles_read();
}

static int profiles_handle(struct ae_screen *instance, struct ae_event const *event)
{
	struct page *page = page_of(instance);
	struct ae_flow_screen *flow = &page->flow;

	if (page->kind == PAGE_NEW)
	{
		/* (typing: Enter or START keeps (and makes it), Esc or B restores; the rest is the typing's) */
		if (screen.typing)
		{
			if (event->action == AE_ACTION_START || event->action == AE_ACTION_ACCEPT)
				typing_end(1);
			else if (event->action == AE_ACTION_BACK)
				typing_end(0);
			return 1;
		}
		if (event->action == AE_ACTION_BACK)
			return 0;
		if (event->action == AE_ACTION_ACCEPT)
		{
			if (event->device == AE_DEVICE_KEYBOARD_MOUSE)
				typing_begin();
			else
				keyboard_open();
		}
		else if (event->action == AE_ACTION_START && screen.name.length)
			name_create();
		return 1;
	}
	if (page->kind == PAGE_EDIT && screen.swatch >= 0)
		return swatch_handle(event);
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (page->kind == PAGE_EDIT)
			edit_step(flow->focus, event->action == AE_ACTION_RIGHT ? 1 : -1);
		return 1;
	case AE_ACTION_ACCEPT:
		if (page->kind == PAGE_FILES)
			import_ask(flow->focus);
		else if (page->kind == PAGE_EDIT && flow->rows[flow->focus].kind == ROW_EXPORT)
			export_ask();
		else if (page->kind == PAGE_EDIT && flow->rows[flow->focus].kind == ROW_IMPORT)
			files_begin();
		else if (page->kind == PAGE_EDIT && flow->rows[flow->focus].kind == ROW_RENAME)
			rename_begin();
		else if (page->kind == PAGE_EDIT && flow->rows[flow->focus].kind == ROW_COLOR)
		{
			/* (the colours take the focus, on the profile's) */
			screen.swatch = (short)(screen.color >= 0 ? screen.color : 0);
			screen.swatch_hover = 0;
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else if (page->kind == PAGE_EDIT)
			edit_step(flow->focus, 1);
		else if (focused_profile() && focused_profile()->index != screen.player1)
			list_use();
		else
			list_accept(flow->focus);
		return 1;
	case AE_ACTION_Y:
		if (page->kind == PAGE_LIST && focused_profile())
			list_accept(flow->focus);
		return 1;
	case AE_ACTION_X:
		if (page->kind == PAGE_LIST && focused_profile())
			delete_ask(focused_profile());
		return 1;
	case AE_ACTION_TAB_NEXT:
		if (page->kind == PAGE_LIST && focused_profile())
		{
			ae_sound_request(AE_SOUND_FORWARD, 0);
			ae_screen_record_open(AE_OWNER_PARENT, focused_profile()->name, focused_profile()->name);
		}
		return 1;
	default:
		return 1;
	}
}

static void profiles_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct page *page = page_of(instance);
	struct ae_event event;
	struct ae_hit hit;
	short row;

	if (ae_prompts_pointer(page->flow.prompts, page->flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK && !screen.typing)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			profiles_handle(instance, &event);
		return;
	}
	if (page->kind == PAGE_NEW)
	{
		/* (a click on the field starts typing into it) */
		if (pointer->left_clicks && !screen.typing && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_FIELD)
			typing_begin();
		return;
	}
	if (page->kind == PAGE_EDIT && edit_pointer(pointer))
		return;
	row = ae_flow_rows_pointer(&page->flow, pointer);
	if (page->kind == PAGE_EDIT && (row == -2 || row == -3))
		edit_step(page->flow.focus, row == -3 ? 1 : -1);
	else if (row >= 0)
		profiles_handle(instance, &(struct ae_event){ .player = pointer->player, .action = AE_ACTION_ACCEPT,
			.device = AE_DEVICE_KEYBOARD_MOUSE });
}

static void profiles_leave(struct ae_screen *instance)
{
	if (page_of(instance)->kind == PAGE_NEW && screen.typing)
	{
		screen.typing = 0;
		ae_field_edit_end(&screen.field_edit, 0);
	}
	/* (the page alone, left without a profile made: the card that opened it goes on) */
	if (page_of(instance)->kind == PAGE_NEW && screen.for_player && screen.for_done)
	{
		ae_screen_profiles_made done = screen.for_done;

		screen.for_done = NULL;
		done(screen.for_player, 0, screen.for_context);
	}
}

static struct ae_screen_class const profiles_class =
{
	.name = "profiles", .leave = profiles_leave, .handle = profiles_handle, .draw = profiles_draw,
	.pointer = profiles_pointer, .update = profiles_update,
};

int ae_screen_profiles_new_for(short local_player, ae_screen_profiles_made done, void *context)
{
	/* (one PROFILES at a time: its state is the screen's own) */
	if (local_player < 1 || local_player >= AE_MAXIMUM_PLAYERS || ae_ui_holds(&screen.list) ||
		ae_ui_holds(&screen.edit) || ae_ui_holds(&screen.made))
	{
		return 0;
	}
	memset(&screen, 0, sizeof(screen));
	screen.list.kind = PAGE_LIST;
	screen.edit.kind = PAGE_EDIT;
	screen.made.kind = PAGE_NEW;
	screen.files.kind = PAGE_FILES;
	screen.side_index = -1;
	screen.for_player = local_player;
	screen.for_done = done;
	screen.for_context = context;
	profiles_read();
	ae_text_init(&screen.name, "", NAME_LIMIT, 0);
	memset(&screen.made.flow, 0, sizeof(screen.made.flow));
	screen.made.flow.hit_rows = HIT_ROWS;
	screen.made.flow.hit_prompts = HIT_PROMPTS;
	if (!ae_ui_push(&profiles_class, local_player, &screen.made))
	{
		screen.for_done = NULL;
		return 0;
	}
	platform_log("ae profiles screen: new profile for player %d", local_player + 1);
	return 1;
}

int ae_screen_profiles_open(void)
{
	memset(&screen, 0, sizeof(screen));
	screen.list.kind = PAGE_LIST;
	screen.edit.kind = PAGE_EDIT;
	screen.made.kind = PAGE_NEW;
	screen.files.kind = PAGE_FILES;
	screen.side_index = -1;
	profiles_read();
	if (!page_push(&screen.list))
		return 0;
	platform_log("ae profiles screen: open (%d profiles)", screen.count);
	return 1;
}

int ae_screen_profiles_dev_open(int edit)
{
	short profile_at = 0;

	if (!ae_screen_profiles_open())
		return 0;
	screen.stage_import_ask = 0;
	screen.plain = edit == 3;
	if (!edit)
		return 1;
	/* (a PC with no profile: one named DEV is made, so the page can be staged) */
	if (screen.count <= 0)
	{
		int made = -1;

		if (!ae_profile_new("DEV", &made).ok)
			return 0;
		profiles_read();
	}
	if (screen.count <= 0)
		return 0;
	/* (the progress file's staging works on its own profile, DEVMOVE, made when it is not there: a PC's own profiles keep
	their records) */
	if (edit >= 4)
	{
		short at;

		for (at = 0; at < screen.count && strcmp(screen.profiles[at].name, "DEVMOVE"); at++)
			;
		if (at == screen.count)
		{
			int made = -1;

			if (!ae_profile_new("DEVMOVE", &made).ok)
				return 0;
			profiles_read();
			for (at = 0; at < screen.count && strcmp(screen.profiles[at].name, "DEVMOVE"); at++)
				;
			if (at == screen.count)
				return 0;
		}
		profile_at = at;
	}
	screen.editing = screen.profiles[profile_at].index;
	snprintf(screen.editing_name, sizeof(screen.editing_name), "%s", screen.profiles[profile_at].name);
	if (!edit_read() || !page_push(&screen.edit))
		return 0;
	if (edit == 2 && ae_profile_color_count() > 0)
	{
		screen.swatch = (short)(((screen.color >= 0 ? screen.color : 0) + 1) % ae_profile_color_count());
		rows_edit();
		focus_colour_row();
	}
	platform_log("ae profiles screen: edit '%s'%s", screen.profiles[profile_at].name, edit == 2 ? ", the colours focused" :
		edit == 3 ? ", no model" : "");
	/* (4: the export's question over it; 5: the import's files; 6: and the first file's question) */
	if (edit == 4)
		export_ask();
	else if (edit >= 5 && files_begin() && edit == 6)
		screen.stage_import_ask = 1;
	return 1;
}
