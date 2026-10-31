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
A profile's name is its save file's name, so it is not changed here.

Its log: the glue's ("ae profiles: ..."), "ae profiles screen: open", "ae profiles screen: <page or item>".
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "interface/player_ui.h"
#include "../src/ae_draw.h"
#include "ae_glue_profiles.h"
#include "ae_motion.h"
#include "ae_screen_profiles.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	PROFILES_HIT = 0x7900,
	HIT_ROWS = PROFILES_HIT + 1, HIT_PROMPTS, HIT_SIDE, HIT_FIELD
};
enum
{
	ROW_HEADER = AE_FLOW_ROW_HEADER,
	ROW_PROFILE = AE_FLOW_ROW_OWN, ROW_NEW,
	ROW_COLOR, ROW_LAYOUT, ROW_LOOK, ROW_INVERT, ROW_VIBRATION
};
enum { PAGE_LIST, PAGE_EDIT, PAGE_NEW };
/* (the list's rows: a header, the profiles, NEW PROFILE) */
enum { LIST_PROFILES = AE_FLOW_ROWS - 2, NAME_LIMIT = 11 };

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
	struct page list, edit, made;
	struct ae_profile_summary profiles[AE_PROFILES_MAXIMUM];
	short count;
	int player1;                        /* the profile player 1 plays with (-1: none) */
	short profile_of_row[AE_FLOW_ROWS];
	/* the profile EDIT shows */
	int editing;
	char editing_name[48];
	struct ae_profile_controls controls;
	short color;
	/* the focused profile's, for the list's right side */
	int side_index;
	struct ae_profile_controls side_controls;
	short side_color;
	/* NEW PROFILE's name */
	struct ae_text name;
	struct ae_field_edit field_edit;
	int typing, keyboard_open;
	unsigned long caret_since;
	struct ae_rect well;
	/* (split screen) NEW PROFILE opened alone for this player (0: PROFILES' own, player 1), and who hears of it */
	short for_player;
	ae_screen_profiles_made for_done;
	void *for_context;
} screen;

static struct ae_screen_class const profiles_class;

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
			screen.profiles[at].index == screen.player1 ? "PLAYER 1" : NULL,
			"{A} edits it; {Y}: player 1 plays with it; {X} deletes it.", NULL };
	}
	screen.profile_of_row[count] = -1;
	flow->rows[count++] = (struct ae_flow_row){ ROW_NEW, -1, "NEW PROFILE", NULL,
		"A profile keeps a player's name, colour, controls and campaign.", NULL };
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
static float draw_name(struct ae_flow_screen *flow, float x, float y, const char *name, unsigned int color,
	const char *sub)
{
	float text = ae_size_text(&flow->density), minor = ae_size_minor(&flow->density);
	float square = ae_flow_u(flow, 56.0f), gap = ae_flow_u(flow, 18.0f);

	ae_draw_rect(x, y, square, square, 0.0f, color);
	ae_draw_outline(x, y, square, square, 0.0f, ae_flow_u(flow, AE_FLOW_RULE_U), AE_COLOR_RULE);
	ae_draw_text(AE_FONT_ROW, text * 1.5f, x + square + gap, y + square * 0.5f -
		(sub ? ae_flow_u(flow, 6.0f) : -ae_draw_cap_height(AE_FONT_ROW, text * 1.5f) * 0.5f), AE_ALIGN_LEFT,
		AE_COLOR_TITLE, name);
	if (sub)
		ae_draw_text(AE_FONT_BODY, minor, x + square + gap, y + square - ae_flow_u(flow, 2.0f), AE_ALIGN_LEFT,
			AE_COLOR_MUTED, sub);
	return square;
}

/* the controls' rows, as values (read-only on the list; EDIT's own are its rows) */
static float draw_settings(struct ae_flow_screen *flow, float x, float y, float width, short color,
	struct ae_profile_controls const *controls)
{
	struct ae_density const *d = &flow->density;
	float gap = ae_flow_u(flow, d->metrics->gap);
	char look[8];
	struct ae_row row;
	short index;
	const char *labels[5] = { "COLOUR", "BUTTON LAYOUT", "LOOK SENSITIVITY", "INVERT LOOK", "VIBRATION" };
	const char *values[5];

	snprintf(look, sizeof(look), "%d", controls->look_sensitivity);
	values[0] = color_name(color);
	values[1] = layout_name(controls->layout);
	values[2] = look;
	values[3] = controls->invert ? "ON" : "OFF";
	values[4] = controls->vibration ? "ON" : "OFF";
	for (index = 0; index < 5; index++)
	{
		memset(&row, 0, sizeof(row));
		row.label = labels[index];
		row.value = values[index];
		y += ae_widget_row(d, x, y, width, &row, HIT_SIDE, index) + gap;
	}
	return y;
}

static void draw_list_side(void)
{
	struct ae_flow_screen *flow = &screen.list.flow;
	float x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X, width = ae_flow_right_x(flow) - x;
	float y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y), minor = ae_size_minor(&flow->density);
	struct ae_profile_summary const *profile = focused_profile();

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
	y += draw_name(flow, x, y, profile->name, profile->color,
		profile->index == screen.player1 ? "Player 1 plays with it" : "Y: player 1 plays with it") +
		ae_flow_u(flow, 34.0f);
	draw_settings(flow, x, y, width, screen.side_color, &screen.side_controls);
}

/* ---------- EDIT */

static void rows_edit(void)
{
	struct ae_flow_screen *flow = &screen.edit.flow;
	short count = 0;

	snprintf(flow->value_texts[2], sizeof(flow->value_texts[2]), "%d", screen.controls.look_sensitivity);
	flow->rows[count++] = (struct ae_flow_row){ ROW_HEADER, -1, "PLAYER", NULL, NULL, NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_COLOR, -1, "COLOUR", color_name(screen.color),
		"Your armour's colour, from the next game you join.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_HEADER, -1, "CONTROLS", NULL, NULL, NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_LAYOUT, -1, "BUTTON LAYOUT", layout_name(screen.controls.layout),
		"The controller's buttons: Halo's own, Anniversary's and the Master Chief Collection's layouts.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_LOOK, -1, "LOOK SENSITIVITY", flow->value_texts[2],
		"How fast the right stick turns you: 1 to 10.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_INVERT, -1, "INVERT LOOK", screen.controls.invert ? "ON" : "OFF",
		"Up on the right stick looks down.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_VIBRATION, -1, "VIBRATION",
		screen.controls.vibration ? "ON" : "OFF", "The controller's rumble.", NULL };
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

static void draw_edit_side(void)
{
	struct ae_flow_screen *flow = &screen.edit.flow;
	float x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X, width = ae_flow_right_x(flow) - x;
	float y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y), cell, gap = ae_flow_u(flow, 8.0f);
	short count = ae_profile_color_count(), color, per_row = 9;

	y += draw_name(flow, x, y, screen.editing_name, ae_profile_color_rgba(screen.color),
		screen.editing == screen.player1 ? "Player 1 plays with it" : NULL) + ae_flow_u(flow, 34.0f);
	ae_draw_text_tracked(AE_FONT_BODY, ae_size_minor(&flow->density), 0.09f, x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED,
		"COLOURS");
	y += ae_flow_u(flow, 24.0f);
	cell = (width - gap * (float)(per_row - 1)) / (float)per_row;
	if (cell > ae_flow_u(flow, 48.0f))
		cell = ae_flow_u(flow, 48.0f);
	for (color = 0; color < count; color++)
	{
		float cx = x + (float)(color % per_row) * (cell + gap), cy = y + (float)(color / per_row) * (cell + gap);

		ae_draw_rect(cx, cy, cell, cell, 0.0f, ae_profile_color_rgba(color));
		ae_draw_outline(cx, cy, cell, cell, 0.0f, ae_flow_u(flow, color == screen.color ? 3.0f : AE_FLOW_RULE_U),
			color == screen.color ? AE_COLOR_TITLE : AE_COLOR_RULE);
	}
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
static void name_create(void)
{
	struct ae_result result;
	int made = -1;

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
		snprintf(crumb, sizeof(crumb), "PLAYER 1: %s", player1 ? player1->name : "NO PROFILE");
		ae_flow_title(flow, "PROFILES", crumb);
		ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
		draw_list_side();
		if (focused_profile())
		{
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Edit", AE_ACTION_ACCEPT);
			if (focused_profile()->index != screen.player1)
				flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Player 1 plays with it",
					AE_ACTION_Y);
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Delete", AE_ACTION_X);
		}
		else
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, ae_string(AE_STR_SELECT),
				AE_ACTION_ACCEPT);
		break;
	case PAGE_EDIT:
		rows_edit();
		ae_flow_title(flow, screen.editing_name, "PROFILES");
		ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
		draw_edit_side();
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Change", AE_ACTION_ACCEPT);
		break;
	default:
		ae_flow_title(flow, "NEW PROFILE", "PROFILES");
		draw_new();
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, screen.typing ? "Make it" : "Name it",
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
		if (page_push(&screen.made))
		{
			platform_log("ae profiles screen: new profile");
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
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
		if (page->kind == PAGE_EDIT)
			edit_step(flow->focus, 1);
		else
			list_accept(flow->focus);
		return 1;
	case AE_ACTION_Y:
		if (page->kind == PAGE_LIST)
			list_use();
		return 1;
	case AE_ACTION_X:
		if (page->kind == PAGE_LIST && focused_profile())
			delete_ask(focused_profile());
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
	screen.side_index = -1;
	profiles_read();
	if (!page_push(&screen.list))
		return 0;
	platform_log("ae profiles screen: open (%d profiles)", screen.count);
	return 1;
}
