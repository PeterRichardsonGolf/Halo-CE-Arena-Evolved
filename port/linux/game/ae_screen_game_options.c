/*
AE_SCREEN_GAME_OPTIONS.C

The game setup spike's GAME OPTIONS screen (ae_screen_game_options.h). One page (the mockup's board 2): the title and the gametype with its changes top right; the groups'
rail on the left (name, rows, a dot when changed; the focus's group lit, a click jumps to it); one list of every rule
the gametype has, under its groups' headers; the focused value's help under the list; the prompts. Positions are
fractions of the frame (ae_frame_compute), sizes u, as the gallery's list page.

Input (anyone's): up / down move (past the headers); left / right step the value in place; A opens the values' picker;
LB / RB the filter chips under the title (ALL, CHANGED, CLASSIC, COMMUNITY, AE, NHE: up from the first row focuses
them, left / right then change the filter, down goes back); LT / RT jump a group; Y DEFAULTS (asks first); X SAVE AS NEW
(named first); B DONE (the copy is kept for the game). The pointer: hover focuses, a click on a row opens its picker, on
an arrow steps it, on the rail jumps, on a chip filters.

Its log: "ae game options: open '<name>' engine E rows R", "focus <label>", "group <name>", "filter <name> (N rows)", the glue's "set ..." and
"defaults", "naming '<free name>' (save as new|save as copy)", "naming: refused (empty|taken) '<name>'", "naming:
cancelled", "saved as new ...", "closed (N changed)".
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_glue_setup_options.h"
#include "ae_hooks.h"
#include "ae_screen_game_options.h"
#include "ae_setup_options.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	OPTIONS_HIT = 0x7D00,
	HIT_LIST = OPTIONS_HIT + 1, HIT_RAIL, HIT_PROMPTS,
	HIT_CHIPS = OPTIONS_HIT + 14,
	ENTRIES_MAXIMUM = 160,
	PROMPTS = 6
};

/* the frame's positions (fractions of 1920 x 1080 at 100 %) */
#define TITLE_Y (70.0f / 1080.0f)
#define RULE_Y (156.0f / 1080.0f)
#define CONTENT_Y (186.0f / 1080.0f)
/* GAME OPTIONS' own page has the filter chips between the title and the rule: CHANGE GAMETYPE's line, the rule and the
content one chip row lower */
#define CHIPS_Y (150.0f / 1080.0f)
#define PAGE_RULE_Y (199.0f / 1080.0f)
#define PAGE_CONTENT_Y (229.0f / 1080.0f)
#define GLYPH_GAP_U 16.0f
#define RAIL_WIDTH 0.17f
/* sizes, u */
#define TITLE_U 60.0f
#define CRUMB_U 20.0f
#define CRUMB_TRACKING 0.09f
#define CRUMB_ABOVE_U 16.0f
#define CRUMB_CHEVRON_GAP_U 12.0f
#define RULE_U 1.0f
#define RAIL_PITCH_U 44.0f
#define RAIL_GAP_U 4.0f
#define RAIL_STRIPE_U 4.0f
#define RAIL_PAD_U 16.0f
#define DOT_U 8.0f
#define LIST_GAP_U 48.0f
#define HELP_LINES 2
#define HELP_GAP_U 28.0f
#define FOOTER_ABOVE_U 62.0f
#define FOOTER_ROOM_U 40.0f

static struct
{
	struct ae_density density;
	struct ae_frame frame;
	struct ae_setup_access access;
	short entries[ENTRIES_MAXIMUM], count;
	struct ae_list_view list;
	struct ae_rect focus_row;           /* the focused row as last drawn, layout units (the picker's place) */
	short focus_option, logged_focus;
	/* the value tick of the row stepped last */
	struct ae_motion tick;
	short tick_option, tick_side;
	char tick_old[64];
	struct ae_prompt prompts[PROMPTS];
	short prompt_count;
	/* the filter chips (ae_setup_filter): the one on, and whether the focus is on them (the list shows no focus then) */
	int filter, on_chips;
	struct ae_chip chips[AE_SETUP_FILTERS];
	char chip_labels[AE_SETUP_FILTERS][32];
	char name[16];
	/* NAME's typing: its text, typed (a keyboard) or AE's keyboard (a pad) */
	struct ae_text name_text;
	struct ae_field_edit name_edit;
	int typing, keyboard_open;
	unsigned long caret_since;
	int dialog;                          /* 0 none, 1 DEFAULTS, 2 saved / failed, 3 leaving with changes,
	                                        4 WEAPON PACKS' DEFAULTS */
	float logged_width, logged_scale;
	char packs_summary[32];              /* WEAPON PACKS' value as last drawn */
} screen;

/* the open picker's values when a row offers only some of its own (PRIMARY / SECONDARY): their texts and their
indexes in the row (static: the picker and its ghost read them after the pick) */
static struct
{
	const char *texts[24];
	short indexes[24], count;
	int mapped;
} picker_map;

static struct ae_screen_class const options_class;

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

static float frame_y(float fraction)
{
	return ae_frame_y(&screen.frame, fraction);
}

static float cap_top(int font, float size, float center_y)
{
	return center_y - ae_draw_cap_height(font, size) * 0.5f;
}

static struct ae_prompt prompt_of(int button, const char *label, int action)
{
	struct ae_prompt prompt;

	prompt.button = button;
	prompt.label = label;
	prompt.key = NULL;
	prompt.action = action;
	prompt.pair = 0;
	return prompt;
}

static void full_density(void)
{
	struct ae_layout layout;
	float scale = ae_settings_ui_scale();

	ae_draw_current_layout(&layout);
	ae_density_full(layout.height * layout.scale, scale, &screen.density);
	ae_frame_compute(layout.width, scale, &screen.frame);
	if (layout.width != screen.logged_width || layout.scale != screen.logged_scale)
	{
		screen.logged_width = layout.width;
		screen.logged_scale = layout.scale;
		platform_log("ae game options: density FULL s=%.2f frame=%.0fx%.0f row=%.1fpx", screen.density.s,
			screen.frame.rect.width * layout.scale, screen.frame.rect.height * layout.scale,
			ae_size_row(&screen.density) / screen.density.pixel);
	}
}

/* the option under the focus, -1 */
static short focused_option(void)
{
	short focus = screen.list.list.focus;

	return !screen.on_chips && focus >= 0 && focus < screen.count && !ae_setup_entry_is_header(screen.entries[focus]) ?
		screen.entries[focus] : -1;
}

/* the option the list's cursor is on, even while the focus is on the chips */
static short cursor_option(void)
{
	short focus = screen.list.list.focus;

	return focus >= 0 && focus < screen.count && !ae_setup_entry_is_header(screen.entries[focus]) ?
		screen.entries[focus] : -1;
}

static short index_of_option(short option)
{
	short index;

	for (index = 0; index < screen.count; index++)
	{
		if (screen.entries[index] == option)
			return index;
	}
	return -1;
}

/* the entries again, under the filter (a value can show or hide rows: TEAM PLAY, LOADOUT, a CUSTOM vehicle set, a change
under CHANGED); the focus stays on its option, else on the row now at its place. force: rebuild even when the count is
the same (the filter changed) */
static void rebuild(int force)
{
	short option = cursor_option(), count;

	ae_setup_edit_access(&screen.access);
	/* (a filter with nothing in it, CHANGED after the last change was undone: back to ALL) */
	if (screen.filter != AE_SF_ALL && !ae_setup_filter_count(&screen.access, screen.filter))
	{
		screen.filter = AE_SF_ALL;
		force = 1;
	}
	count = ae_setup_entries_filtered(&screen.access, screen.filter, screen.entries, ENTRIES_MAXIMUM);
	if (count != screen.count || force)
	{
		short index;

		screen.count = count;
		ae_list_set_count(&screen.list.list, count);
		index = option >= 0 ? index_of_option(option) : -1;
		if (index >= 0)
			ae_list_set_focus(&screen.list.list, index);
		else if (screen.list.list.focus >= screen.count || screen.list.list.focus < 0)
			ae_list_set_focus(&screen.list.list, screen.count > 1 ? 1 : 0);
	}
}

static void refresh(void)
{
	rebuild(0);
	/* (never a header) */
	if (screen.list.list.focus >= 0 && screen.list.list.focus < screen.count &&
		ae_setup_entry_is_header(screen.entries[screen.list.list.focus]))
		ae_list_set_focus(&screen.list.list, ae_setup_row_step(screen.entries, screen.count, screen.list.list.focus, 1));
	/* (NAME's value: the copy's name, kept for the model) */
	ae_setup_edit_name(screen.name, sizeof(screen.name));
	ae_setup_name_set(screen.name);
	screen.focus_option = focused_option();
	if (screen.focus_option != screen.logged_focus)
	{
		screen.logged_focus = screen.focus_option;
		if (screen.focus_option >= 0)
			platform_log("ae game options: focus %s", ae_setup_label(&screen.access, screen.focus_option));
	}
}

/* a filter chip on (LB / RB, a click, left / right on the chips): the list is rebuilt, the focused row kept when the
filter lists it, else the focus on the first row */
static void filter_set(int filter)
{
	if (filter < 0 || filter >= AE_SETUP_FILTERS || filter == screen.filter)
		return;
	screen.filter = filter;
	rebuild(1);
	ae_list_set_focus(&screen.list.list, screen.list.list.focus);
	screen.tick_option = -1;
	refresh();
	platform_log("ae game options: filter %s (%d rows)", ae_setup_filter_name(filter),
		ae_setup_filter_count(&screen.access, filter));
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

/* the next filter (direction -1 / 1) that has rows, round past the ends; ALL always has */
static int filter_next(int direction)
{
	int step, filter = screen.filter;

	for (step = 0; step < AE_SETUP_FILTERS; step++)
	{
		filter = (filter + direction + AE_SETUP_FILTERS) % AE_SETUP_FILTERS;
		if (filter == AE_SF_ALL || ae_setup_filter_count(&screen.access, filter))
			return filter;
	}
	return screen.filter;
}

/* past a header the same way, else back (as the gallery's skip_groups) */
static void skip_headers(short direction)
{
	short focus = screen.list.list.focus;

	if (focus < 0 || focus >= screen.count || !ae_setup_entry_is_header(screen.entries[focus]))
		return;
	ae_list_set_focus(&screen.list.list, ae_setup_row_step(screen.entries, screen.count, focus, direction));
	if (ae_setup_entry_is_header(screen.entries[screen.list.list.focus]))
		ae_list_set_focus(&screen.list.list, ae_setup_row_step(screen.entries, screen.count, screen.list.list.focus,
			(short)-direction));
}

/* ---------- NAME's typing */

enum { NAME_LIMIT = 11 };

/* the typed text becomes the copy's name (kept); nothing typed keeps the old one */
static void name_commit(void)
{
	ae_setup_edit_set_name(screen.name_text.text);
	screen.tick_option = -1;
	refresh();
}

static void name_typing_done(int keep, void *context)
{
	(void)context;
	screen.typing = 0;
	if (keep)
		name_commit();
}

static void name_typing_begin(void)
{
	screen.name_edit.done = name_typing_done;
	screen.name_edit.context = NULL;
	screen.name_edit.holder = &screen;
	ae_field_edit_begin(&screen.name_edit, &screen.name_text);
	ae_text_end(&screen.name_text, 0);
	screen.typing = 1;
	screen.caret_since = ae_motion_now();
}

static void name_typing_end(int keep)
{
	if (!screen.typing)
		return;
	screen.typing = 0;
	ae_field_edit_end(&screen.name_edit, keep);
	if (keep)
		name_commit();
	ae_sound_request(keep ? AE_SOUND_FORWARD : AE_SOUND_BACK, 0);
}

static void name_keyboard_done(int keep, void *context)
{
	(void)context;
	screen.keyboard_open = 0;
	/* (a physical key closed it into the field's own typing: that goes on) */
	if (screen.name_edit.active && screen.name_edit.text == &screen.name_text)
	{
		screen.typing = 1;
		screen.caret_since = ae_motion_now();
		return;
	}
	if (keep)
		name_commit();
}

static void name_begin(void)
{
	ae_text_init(&screen.name_text, screen.name, NAME_LIMIT, 1);
	if (ae_ui_last_device() == AE_DEVICE_KEYBOARD_MOUSE)
		name_typing_begin();
	else
	{
		struct ae_keyboard_spec spec;

		memset(&spec, 0, sizeof(spec));
		spec.text = &screen.name_text;
		spec.field = screen.focus_row;
		spec.bounds = screen.frame.rect;
		spec.density = screen.density;
		spec.done = name_keyboard_done;
		screen.name_edit.done = name_typing_done;
		screen.name_edit.context = NULL;
		screen.name_edit.holder = &screen;
		spec.edit = &screen.name_edit;
		if (ae_keyboard_open(&spec, AE_OWNER_PARENT))
			screen.keyboard_open = 1;
	}
	platform_log("ae game options: name %s", screen.typing ? "typing" : "keyboard");
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* ---------- changing values */

static void value_step(short option, short direction)
{
	struct ae_setup_row const *row = ae_setup_row(option);
	int current = screen.access.get(screen.access.context, option);
	short next;

	if (!row || current < 0)
		return;
	if (ae_setup_edit_reason(option))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	/* (PRIMARY / SECONDARY: the stock weapons and the packs that can start; WEAPON PACKS has one value: it opens) */
	next = ae_setup_value_step(&screen.access, option, (short)current, direction);
	if (next == current)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	snprintf(screen.tick_old, sizeof(screen.tick_old), "%s", ae_setup_value_text(option, current));
	screen.tick_option = option;
	screen.tick_side = direction;
	ae_motion_start(&screen.tick, 0.0f, 1.0f, AE_MOTION_VALUE_TICK_MS);
	ae_setup_edit_set(option, next);
	ae_sound_request(AE_SOUND_CURSOR, 0);
	refresh();
}

static void picked(short index, void *context)
{
	short option = (short)(intptr_t)context;

	if (picker_map.mapped)
	{
		if (index < 0 || index >= picker_map.count)
			return;
		index = picker_map.indexes[index];
	}
	ae_setup_edit_set(option, index);
	screen.tick_option = -1;
	refresh();
}

static int packs_open(void);

/* the picker's values: the row's own, or (a row that offers only some) the offered ones through picker_map */
static void picker_values(short option, struct ae_picker_spec *spec)
{
	struct ae_setup_row const *row = ae_setup_row(option);
	short current = (short)screen.access.get(screen.access.context, option), index;

	picker_map.mapped = 0;
	spec->values = row->values;
	spec->count = row->value_count;
	spec->current = current;
	if (option != AE_SO_PRIMARY && option != AE_SO_SECONDARY)
		return;
	picker_map.count = ae_setup_offered_values(&screen.access, option, picker_map.indexes,
		(short)(sizeof(picker_map.indexes) / sizeof(picker_map.indexes[0])));
	spec->current = 0;
	for (index = 0; index < picker_map.count; index++)
	{
		picker_map.texts[index] = row->values[picker_map.indexes[index]];
		if (picker_map.indexes[index] == current)
			spec->current = index;
	}
	picker_map.mapped = 1;
	spec->values = picker_map.texts;
	spec->count = picker_map.count;
	/* (the log: what the list offers, for the tests) */
	{
		char offered[400];
		size_t length = 0;

		offered[0] = 0;
		for (index = 0; index < picker_map.count && length < sizeof(offered); index++)
			length += (size_t)snprintf(offered + length, sizeof(offered) - length, "%s%s", index ? "," : "",
				picker_map.texts[index]);
		platform_log("ae game options: offered %s %s", row->label, offered);
	}
}

static void picker_open(short option)
{
	struct ae_setup_row const *row = ae_setup_row(option);
	struct ae_picker_spec spec;

	if (!row)
		return;
	if (option == AE_SO_NAME)
	{
		name_begin();
		return;
	}
	if (option == AE_SO_WEAPON_PACKS)
	{
		if (packs_open())
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	}
	if (ae_setup_edit_reason(option))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	memset(&spec, 0, sizeof(spec));
	/* (the row's own static texts, or picker_map's: they outlive the picker and its ghost) */
	picker_values(option, &spec);
	spec.row = screen.focus_row;
	spec.bounds = screen.frame.rect;
	spec.density = screen.density;
	spec.picked = picked;
	spec.title = ae_setup_label(&screen.access, option);
	spec.context = (void *)(intptr_t)option;
	if (ae_picker_open(&spec, AE_OWNER_PARENT))
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* the focus on a group's first row, the group's header scrolled to the list's top (as far as the list goes) */
static void show_group_row(short row)
{
	struct ae_list *list = &screen.list.list;
	short last_first = list->count > list->rows ? (short)(list->count - list->rows) : 0;

	ae_list_set_focus(list, row);
	list->first = (short)(row - 1 < last_first ? row - 1 : last_first);
	if (list->first < 0)
		list->first = 0;
}

static void group_jump(short direction)
{
	short before = screen.list.list.focus, next;
	short group;

	next = ae_setup_group_step(screen.entries, screen.count, before, direction);
	if (next == before)
		return;
	show_group_row(next);
	group = ae_setup_group_at(screen.entries, screen.count, next);
	platform_log("ae game options: group %s", ae_setup_group_name(&screen.access, group));
	ae_sound_request(AE_SOUND_CURSOR, 0);
	refresh();
}

/* the leave dialog's choices (MCC's: a seeded or built-in gametype is never saved over, only as a copy) */
enum { LEAVE_SAVE, LEAVE_SAVE_COPY, LEAVE_THIS_GAME, LEAVE_DISCARD, LEAVE_KEEP_EDITING };
static short leave_actions[AE_DIALOG_CHOICES];

static void dialog_open(int kind, const char *title, const char *body, const char *first, const char *second);
static void naming_open(int from_leave);

static void leave_picked(short action)
{
	if (action == LEAVE_KEEP_EDITING)
		return;
	/* (SAVE AS COPY: named first; its START saves and closes the screen, its B asks again) */
	if (action == LEAVE_SAVE_COPY)
	{
		naming_open(1);
		return;
	}
	if (action == LEAVE_SAVE && !ae_setup_edit_save())
	{
		screen.dialog = 2;
		dialog_open(AE_DIALOG_ERROR, "Not saved", "The game type could not be written.", ae_string(AE_STR_BACK), NULL);
		return;
	}
	/* DISCARD: the rules go back to where the edit started, so the game keeps them as they were */
	if (action == LEAVE_DISCARD)
	{
		ae_setup_edit_defaults();
		platform_log("ae game options: leave: discarded");
	}
	/* (saved, THIS GAME ONLY or DISCARD: the caller applies the rules as it does on B) */
	ae_sound_request(AE_SOUND_BACK, 0);
	ae_ui_pop();
}

static void dialog_picked(short choice, void *context)
{
	int kind = screen.dialog;

	(void)context;
	screen.dialog = 0;
	if (kind == 1 && choice == 1)
	{
		ae_setup_edit_defaults();
		screen.tick_option = -1;
		refresh();
	}
	else if (kind == 4 && choice == 1)
	{
		ae_setup_edit_defaults_packs();
		screen.tick_option = -1;
		refresh();
	}
	else if (kind == 3 && choice >= 0 && choice < AE_DIALOG_CHOICES)
		leave_picked(leave_actions[choice]);
}

static void dialog_open(int kind, const char *title, const char *body, const char *first, const char *second)
{
	struct ae_dialog_spec spec;

	memset(&spec, 0, sizeof(spec));
	spec.kind = kind;
	spec.title = title;
	spec.body = body;
	spec.choices[0] = first;
	spec.choices[1] = second;
	spec.choice_count = second ? 2 : 1;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.density = screen.density;
	spec.bounds = screen.frame.rect;
	spec.picked = dialog_picked;
	ae_dialog_open(&spec, AE_OWNER_PARENT);
}

static void defaults_ask(void)
{
	char body[160];
	short changes = ae_setup_changes(&screen.access);

	if (!changes)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	snprintf(body, sizeof(body), "Every rule goes back to %s's own: %d change%s undone.", screen.name, changes,
		changes == 1 ? "" : "s");
	screen.dialog = 1;
	dialog_open(AE_DIALOG_CONFIRM, "Back to defaults?", body, ae_string(AE_STR_CANCEL_CAPS), "DEFAULTS");
}

static void save_as_new(void)
{
	naming_open(0);
}

/* B: with no changes the screen closes (returns 1); with changes it asks first, as MCC does: SAVE (the player's own
gametypes only), SAVE AS COPY, THIS GAME ONLY (the rules for this game, the file as it was), DISCARD (the rules as
they were), KEEP EDITING (B) */
static int leave_or_ask(void)
{
	struct ae_dialog_spec spec;
	char body[200];
	short count = 0;
	int own = ae_setup_edit_can_save();

	if (!ae_setup_changes(&screen.access))
		return 1;
	memset(&spec, 0, sizeof(spec));
	if (own)
	{
		leave_actions[count] = LEAVE_SAVE;
		spec.choices[count++] = "SAVE";
	}
	leave_actions[count] = LEAVE_SAVE_COPY;
	spec.choices[count++] = "SAVE AS COPY";
	leave_actions[count] = LEAVE_THIS_GAME;
	spec.choices[count++] = "THIS GAME ONLY";
	leave_actions[count] = LEAVE_DISCARD;
	spec.choices[count++] = "DISCARD";
	leave_actions[count] = LEAVE_KEEP_EDITING;
	spec.choices[count++] = "KEEP EDITING";
	if (own)
		snprintf(body, sizeof(body), "Save the changes to %s, or as a new game type? DISCARD drops them.", screen.name);
	else
		snprintf(body, sizeof(body), "%s can't be changed. Save these rules as a new game type, use them for this "
			"game only, or discard them.", screen.name);
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "Save changes?";
	spec.body = body;
	spec.choice_count = count;
	spec.safe_choice = (short)(count - 1);
	spec.cancel_choice = (short)(count - 1);
	spec.has_cancel = 1;
	spec.density = screen.density;
	spec.bounds = screen.frame.rect;
	spec.picked = dialog_picked;
	screen.dialog = 3;
	ae_dialog_open(&spec, AE_OWNER_PARENT);
	platform_log("ae game options: leave: asked (save over: %s)", own ? "yes" : "no");
	return 0;
}

/* ---------- naming a new game type (X SAVE AS NEW, the leave dialog's SAVE AS COPY)

A popover as the dialogs are (the scrim, a 720 u box, the accent stripe, the title "Name the game type"), with the
NAME field: the free name ae_setup_edit_save_as_new would take, selected, so the first key types over it (upper case,
31 characters: the stored name keeps the first 11, a longer one is the new file's display name). A keyboard types at
once; a pad gets AE's keyboard beside the field (A opens it again). START or Enter saves under the name, B or Esc goes
back without saving (to the screen, or to the leave dialog). An empty name or one a game type has already: the
field's line says why, nothing is saved. */

enum { NAMING_LIMIT = 31, HIT_NAMING = OPTIONS_HIT + 12, HIT_NAMING_PROMPTS };
#define NAMING_WIDTH_U 720.0f
#define NAMING_STRIPE_U 4.0f
#define NAMING_TITLE_U 32.0f
#define NAMING_HELP_GAP_U 12.0f
#define NAMING_KEYBOARD_ROOM_U 700.0f
#define NAMING_LINE 1.4f

static struct
{
	int from_leave;                      /* SAVE AS COPY: saving closes the screen, B asks again */
	int beside_keyboard;                 /* opened from a pad: the box left of the middle, AE's keyboard to its right */
	struct ae_text text;
	struct ae_field_edit edit;
	int typing, keyboard_open;
	unsigned long caret_since;
	char refused[AE_TEXT_MAXIMUM];       /* the text refused last; its reason shows until the text changes */
	char reason[96];
	struct ae_rect well;                 /* the field's well as last drawn (layout units: AE's keyboard beside it) */
	struct ae_motion motion;
	struct ae_prompt prompts[3];
	short prompt_count;
} naming;

static struct ae_screen_class const naming_class;

static int naming_refusal_showing(void)
{
	return naming.reason[0] && !strcmp(naming.refused, naming.text.text);
}

static void naming_typing_done(int keep, void *context)
{
	(void)keep;
	(void)context;
	naming.typing = 0;
}

static void naming_typing_begin(void)
{
	naming.edit.done = naming_typing_done;
	naming.edit.context = NULL;
	naming.edit.holder = &naming;
	ae_field_edit_begin(&naming.edit, &naming.text);
	ae_text_select_all(&naming.text);
	naming.typing = 1;
	naming.caret_since = ae_motion_now();
}

static void naming_save(void);
static void naming_cancel(void);

static void naming_keyboard_done(int keep, void *context)
{
	(void)context;
	naming.keyboard_open = 0;
	/* (a physical key closed it into the field's own typing: that goes on) */
	if (naming.edit.active && naming.edit.text == &naming.text)
	{
		naming.typing = 1;
		naming.caret_since = ae_motion_now();
		return;
	}
	/* (its DONE or START saves; its B goes back, as the box's own B) */
	if (keep)
		naming_save();
	else
		naming_cancel();
}

static void naming_keyboard_open(void)
{
	struct ae_keyboard_spec spec;

	memset(&spec, 0, sizeof(spec));
	spec.text = &naming.text;
	spec.field = naming.well;
	spec.bounds = screen.frame.rect;
	spec.density = screen.density;
	spec.done = naming_keyboard_done;
	naming.edit.done = naming_typing_done;
	naming.edit.context = NULL;
	naming.edit.holder = &naming;
	spec.edit = &naming.edit;
	ae_text_select_all(&naming.text);
	if (ae_keyboard_open(&spec, AE_OWNER_PARENT))
		naming.keyboard_open = 1;
	naming.caret_since = ae_motion_now();
}

/* the box (layout units): 720 u wide, its height from its parts; centred, or left of the middle beside AE's keyboard */
static void naming_place(struct ae_rect *box, float *title_center, float *field_y, float *help_center,
	float *prompts_center)
{
	float pad = u(screen.density.metrics->pad), gap = u(screen.density.metrics->gap);
	float title = u(NAMING_TITLE_U), minor = ae_size_minor(&screen.density), glyph = ae_size_glyph(&screen.density);
	float y = u(NAMING_STRIPE_U) + pad;
	struct ae_field field;
	struct ae_rect well;

	memset(&field, 0, sizeof(field));
	ae_field_well(&screen.density, 0.0f, 0.0f, u(NAMING_WIDTH_U), &field, &well);
	*title_center = y + title * NAMING_LINE * 0.5f;
	y += title * NAMING_LINE + gap;
	*field_y = y;
	y += well.height + u(NAMING_HELP_GAP_U);
	*help_center = y + minor * NAMING_LINE * 0.5f;
	y += minor * NAMING_LINE + pad * 0.75f;
	*prompts_center = y + glyph * 0.8f;
	y += glyph * 1.6f + pad * 0.75f;
	box->width = u(NAMING_WIDTH_U);
	box->height = y;
	box->x = screen.frame.rect.x + (screen.frame.rect.width - box->width) * 0.5f;
	if (naming.beside_keyboard)
	{
		box->x = screen.frame.rect.x + (screen.frame.rect.width - box->width - u(NAMING_KEYBOARD_ROOM_U)) * 0.5f;
		if (box->x < left_x())
			box->x = left_x();
	}
	box->y = screen.frame.rect.y + (screen.frame.rect.height - box->height) * 0.5f;
}

static void naming_draw(struct ae_screen *instance)
{
	struct ae_density const *d = &screen.density;
	struct ae_field field;
	struct ae_rect box;
	float scrim, scale, alpha, title_center, field_y, help_center, prompts_center, x, inner;
	float pad = u(d->metrics->pad), title = u(NAMING_TITLE_U), minor = ae_size_minor(d);
	int refused = naming_refusal_showing();
	short count = 0;

	(void)instance;
	full_density();
	ae_motion_dialog(ae_motion_progress(&naming.motion), 1, &scrim, &scale, &alpha);
	if (ae_ui_drawing_ghost())
	{
		scrim *= ae_ui_drawing_alpha();
		alpha *= ae_ui_drawing_alpha();
	}
	/* the scrim over the screen, then the box */
	ae_draw_set_alpha(scrim);
	ae_draw_rect(screen.frame.rect.x, screen.frame.rect.y, screen.frame.rect.width, screen.frame.rect.height, 0.0f,
		AE_COLOR_SCRIM);
	ae_hit_add(screen.frame.rect.x, screen.frame.rect.y, screen.frame.rect.width, screen.frame.rect.height, HIT_NAMING,
		AE_PART_OUTSIDE, -1);
	ae_draw_set_alpha(alpha);
	naming_place(&box, &title_center, &field_y, &help_center, &prompts_center);
	ae_draw_rect(box.x, box.y, box.width, box.height, u(d->metrics->corner), AE_COLOR_POPOVER);
	ae_hit_add(box.x, box.y, box.width, box.height, HIT_NAMING, AE_PART_CARD, -1);
	ae_draw_rect(box.x, box.y, box.width, u(NAMING_STRIPE_U), 0.0f, AE_COLOR_ACCENT);
	x = box.x + pad;
	inner = box.width - 2.0f * pad;
	ae_draw_text(AE_FONT_ROW, title, x, cap_top(AE_FONT_ROW, title, box.y + title_center), AE_ALIGN_LEFT, AE_COLOR_TITLE,
		"Name the game type");
	/* the field: the name selected while it is typed into (the keyboard's or AE's), its count; under it the help, or a
	refusal's reason in the warning colour until the text changes */
	memset(&field, 0, sizeof(field));
	field.text = &naming.text;
	field.placeholder = "A name for these rules";
	field.flags = AE_FIELD_FOCUSED | (naming.typing || naming.keyboard_open ? AE_FIELD_EDITING : 0);
	field.caret_since = naming.caret_since;
	ae_field_well(d, x, box.y + field_y, inner, &field, &naming.well);
	ae_widget_field(d, x, box.y + field_y, inner, &field, HIT_NAMING);
	ae_draw_text(AE_FONT_BODY, minor, x, cap_top(AE_FONT_BODY, minor, box.y + help_center), AE_ALIGN_LEFT,
		refused ? AE_COLOR_WARNING : AE_COLOR_MUTED, refused ? naming.reason :
		"Up to 31 letters, numbers and spaces.");
	/* its prompts inside it: START saves, B goes back; a pad's A brings AE's keyboard back */
	naming.prompts[count++] = prompt_of(AE_BUTTON_START, ae_string(AE_STR_SAVE), AE_ACTION_START);
	if (!naming.typing && !naming.keyboard_open)
		naming.prompts[count++] = prompt_of(AE_BUTTON_A, "Keyboard", AE_ACTION_ACCEPT);
	naming.prompts[count++] = prompt_of(AE_BUTTON_B, ae_string(AE_STR_CANCEL), AE_ACTION_BACK);
	naming.prompt_count = count;
	ae_widget_prompts(d, x, box.y + prompts_center, naming.prompts, count, NULL, x + inner, -1, HIT_NAMING_PROMPTS);
	ae_draw_set_alpha(1.0f);
}

static void naming_close(void)
{
	if (naming.typing)
	{
		naming.typing = 0;
		ae_field_edit_end(&naming.edit, 0);
	}
	ae_ui_remove(&naming);
}

static void naming_cancel(void)
{
	int from_leave = naming.from_leave;

	platform_log("ae game options: naming: cancelled");
	naming_close();
	ae_sound_request(AE_SOUND_BACK, 0);
	/* (from the leave dialog: it asks again) */
	if (from_leave)
		leave_or_ask();
}

static void naming_refuse(const char *why, const char *reason)
{
	snprintf(naming.refused, sizeof(naming.refused), "%s", naming.text.text);
	snprintf(naming.reason, sizeof(naming.reason), "%s", reason);
	platform_log("ae game options: naming: refused (%s) '%s'", why, naming.text.text);
	ae_sound_request(AE_SOUND_FAILURE, 0);
}

/* START: saved under the typed name (spaces round it trimmed); the stored name its first 11 characters, the display
name the whole when longer */
static void naming_save(void)
{
	char typed[AE_TEXT_MAXIMUM], before[16], saved[16], body[200];
	int start = 0, end = naming.text.length, from_leave = naming.from_leave;

	while (start < end && naming.text.text[start] == ' ')
		start++;
	while (end > start && naming.text.text[end - 1] == ' ')
		end--;
	snprintf(typed, sizeof(typed), "%.*s", end - start, naming.text.text + start);
	if (!typed[0])
	{
		naming_refuse("empty", "Type a name first.");
		return;
	}
	if (ae_setup_name_taken(typed))
	{
		char reason[96];

		snprintf(reason, sizeof(reason), "A game type is called %s already.", typed);
		naming_refuse("taken", reason);
		return;
	}
	naming_close();
	ae_setup_edit_name(before, sizeof(before));
	ae_setup_edit_set_name(typed);
	if (!ae_setup_edit_save_as_new(saved, sizeof(saved)))
	{
		ae_setup_edit_set_name(before);
		screen.dialog = 2;
		dialog_open(AE_DIALOG_ERROR, "Not saved", "The new game type could not be written.", ae_string(AE_STR_BACK),
			NULL);
		return;
	}
	/* (a name longer than the stored one, or one whose stored name took a number: the whole name in the lists) */
	if (strcmp(saved, typed))
		ae_setup_edit_set_display_name(typed);
	ae_setup_edit_name(screen.name, sizeof(screen.name));
	screen.tick_option = -1;
	refresh();
	/* (SAVE AS COPY: the screen closes, as the leave dialog's other saves do) */
	if (from_leave)
	{
		ae_sound_request(AE_SOUND_BACK, 0);
		ae_ui_pop();
		return;
	}
	snprintf(body, sizeof(body), "These rules are saved as a new game type, %s.", typed);
	screen.dialog = 2;
	dialog_open(AE_DIALOG_CONFIRM, "Saved", body, "OK", NULL);
}

static void naming_open(int from_leave)
{
	char name[16];

	memset(&naming, 0, sizeof(naming));
	naming.from_leave = from_leave;
	naming.beside_keyboard = ae_ui_last_device() != AE_DEVICE_KEYBOARD_MOUSE;
	if (!ae_setup_edit_free_name(name, sizeof(name)))
	{
		screen.dialog = 2;
		dialog_open(AE_DIALOG_ERROR, "Not saved", "Every name for a copy of this game type is taken.",
			ae_string(AE_STR_BACK), NULL);
		return;
	}
	ae_text_init(&naming.text, name, NAMING_LIMIT, 1);
	ae_text_select_all(&naming.text);
	ae_motion_start(&naming.motion, 0.0f, 1.0f, AE_MOTION_DIALOG_OPEN_MS);
	if (!ae_ui_push(&naming_class, AE_OWNER_PARENT, &naming))
		return;
	platform_log("ae game options: naming '%s' (%s)", name, from_leave ? "save as copy" : "save as new");
	ae_sound_request(AE_SOUND_FORWARD, 0);
	/* (a keyboard types at once; a pad gets AE's keyboard, placed beside the field as it is drawn first) */
	if (naming.beside_keyboard)
	{
		full_density();
		{
			struct ae_rect box;
			struct ae_field field;
			float title_center, field_y, help_center, prompts_center, pad = u(screen.density.metrics->pad);

			naming_place(&box, &title_center, &field_y, &help_center, &prompts_center);
			memset(&field, 0, sizeof(field));
			field.text = &naming.text;
			ae_field_well(&screen.density, box.x + pad, box.y + field_y, box.width - 2.0f * pad, &field, &naming.well);
		}
		naming_keyboard_open();
	}
	else
		naming_typing_begin();
}

static int naming_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	ae_motion_finish(&naming.motion);
	switch (event->action)
	{
	case AE_ACTION_START:
		naming_save();
		return 1;
	case AE_ACTION_ACCEPT:
		/* (Enter while typing saves; a pad's A brings AE's keyboard back) */
		if (naming.typing || event->device == AE_DEVICE_KEYBOARD_MOUSE)
			naming_save();
		else
			naming_keyboard_open();
		return 1;
	case AE_ACTION_BACK:
		naming_cancel();
		return 1;
	default:
		return 1;
	}
}

static void naming_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;

	if (ae_prompts_pointer(naming.prompts, naming.prompt_count, pointer, HIT_NAMING_PROMPTS, &event))
	{
		naming_handle(instance, &event);
		return;
	}
	/* (a click on the field types into it; elsewhere nothing) */
	if (pointer->left_clicks && !naming.typing && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_NAMING &&
		hit.part == AE_PART_FIELD)
		naming_typing_begin();
}

static void naming_update(struct ae_screen *instance)
{
	(void)instance;
	if (naming.typing && naming.edit.active && !ae_field_edit_keys(&naming.edit))
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

static void naming_leave(struct ae_screen *instance)
{
	(void)instance;
	if (naming.typing)
	{
		naming.typing = 0;
		ae_field_edit_end(&naming.edit, 0);
	}
}

static struct ae_screen_class const naming_class =
{
	.name = "name the game type", .leave = naming_leave, .handle = naming_handle, .draw = naming_draw,
	.pointer = naming_pointer, .popover = 1, .update = naming_update,
};

/* ---------- drawing */

struct item_context { struct ae_density const *density; };

/* a row of the WEAPON PACKS screen differs from where the edit started (PRIMARY / SECONDARY are in the list too) */
static int packs_changed(void)
{
	int option;

	for (option = AE_SO_PACK_FIRST; option < AE_SO_PACK_FIRST + AE_SETUP_PACKS; option++)
	{
		if (ae_setup_applies(&screen.access, option) && screen.access.changed(screen.access.context, option))
			return 1;
	}
	return ae_setup_applies(&screen.access, AE_SO_PACKS_ON_MAP) &&
		screen.access.changed(screen.access.context, AE_SO_PACKS_ON_MAP);
}

static void list_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct item_context const *list = context;
	struct ae_setup_row const *row;
	struct ae_row widget;
	short entry = screen.entries[item];
	int value, base;

	/* (a group header: its label at the slot's foot, not focusable) */
	if (ae_setup_entry_is_header(entry))
	{
		float group = list->density->metrics->group_height * list->density->unit;

		ae_widget_group(list->density, x, y + height - group, width,
			ae_setup_group_name(&screen.access, ae_setup_entry_group(entry)));
		return;
	}
	row = ae_setup_row(entry);
	value = screen.access.get(screen.access.context, entry);
	base = ae_setup_edit_base(entry);
	memset(&widget, 0, sizeof(widget));
	widget.label = ae_setup_label(&screen.access, entry);
	widget.value = ae_setup_value_text(entry, value);
	widget.flags = flags;
	if (row && row->source != AE_SRC_CLASSIC)
	{
		widget.flags |= AE_ROW_AE;
		widget.source_badge = ae_setup_source_badge(row);
	}
	if (screen.access.changed(screen.access.context, entry))
	{
		widget.flags |= AE_ROW_CHANGED;
		widget.base_value = ae_setup_value_text(entry, base);
	}
	if (ae_setup_edit_reason(entry))
	{
		widget.flags |= AE_ROW_DISABLED;
		widget.reason = ae_setup_edit_reason(entry);
	}
	/* (WEAPON PACKS: what is on, a chevron; changed when a row of its screen is) */
	if (entry == AE_SO_WEAPON_PACKS)
	{
		ae_setup_packs_summary(&screen.access, screen.packs_summary, sizeof(screen.packs_summary));
		widget.value = screen.packs_summary;
		widget.flags |= AE_ROW_SUBSCREEN;
		widget.base_value = NULL;
		if (packs_changed())
			widget.flags |= AE_ROW_CHANGED;
	}
	if (entry == screen.tick_option)
	{
		widget.tick = &screen.tick;
		widget.tick_side = screen.tick_side;
		widget.old_value = screen.tick_old;
	}
	if (flags & AE_ROW_FOCUSED)
	{
		screen.focus_row.x = x;
		screen.focus_row.y = y;
		screen.focus_row.width = width;
		screen.focus_row.height = height;
	}
	if (entry == AE_SO_NAME && (screen.typing || screen.keyboard_open))
	{
		/* (NAME being typed: the row's value is the text, in the ring of an edit field) */
		char shown[AE_TEXT_MAXIMUM + 8];

		snprintf(shown, sizeof(shown), "%s%s", screen.name_text.text,
			((ae_motion_now() - screen.caret_since) / 530u) % 2u == 0u ? "|" : "");
		widget.value = shown;
		widget.flags &= ~(unsigned int)AE_ROW_CHANGED;
		ae_widget_row(list->density, x, y, width, &widget, HIT_LIST, item);
		ae_draw_outline(x, y, width, height, u(screen.density.metrics->corner), u(1.0f), AE_COLOR_RING);
		return;
	}
	ae_widget_row(list->density, x, y, width, &widget, HIT_LIST, item);
}

/* (a sub-screen) the crumb above the title, small tracked capitals: GAME OPTIONS > the sub-screen's name (a line
under the title has no room at 130 %: the mockup's finding) */
static void draw_crumb(const char *tail, float title_top)
{
	float size = u(CRUMB_U), cap = ae_draw_cap_height(AE_FONT_BODY, size);
	float top = title_top - u(CRUMB_ABOVE_U) - cap, middle = top + cap * 0.5f, x = left_x(), half = cap * 0.32f;

	x += ae_draw_text_tracked(AE_FONT_BODY, size, CRUMB_TRACKING, x, top, AE_ALIGN_LEFT, AE_COLOR_MUTED,
		"GAME OPTIONS") + u(CRUMB_CHEVRON_GAP_U);
	/* (a drawn chevron, the row's own, not a glyph) */
	ae_draw_line(x, middle - half, x + half, middle, u(2.0f), AE_COLOR_MUTED);
	ae_draw_line(x + half, middle, x, middle + half, u(2.0f), AE_COLOR_MUTED);
	x += half + u(CRUMB_CHEVRON_GAP_U);
	ae_draw_text_tracked(AE_FONT_BODY, size, CRUMB_TRACKING, x, top, AE_ALIGN_LEFT, AE_COLOR_TEXT, tail);
}

static void draw_header(const char *crumb_tail, float rule_y)
{
	char changed[32];
	float size = u(TITLE_U), top = frame_y(TITLE_Y), crumb = u(CRUMB_U), x = right_x(), middle, width;
	short changes = ae_setup_changes(&screen.access);
	unsigned int color = changes ? AE_COLOR_ACCENT : AE_COLOR_MUTED;

	middle = top + ae_draw_cap_height(AE_FONT_TITLE, size) * 0.5f;
	ae_draw_text(AE_FONT_TITLE, size, left_x(), top, AE_ALIGN_LEFT, AE_COLOR_TITLE, "GAME OPTIONS");
	if (crumb_tail)
		draw_crumb(crumb_tail, top);
	if (changes)
		snprintf(changed, sizeof(changed), "%d CHANGED", changes);
	else
		snprintf(changed, sizeof(changed), "NO CHANGES");
	width = ae_draw_text_tracked(AE_FONT_BODY, crumb, CRUMB_TRACKING, x, cap_top(AE_FONT_BODY, crumb, middle),
		AE_ALIGN_RIGHT, color, changed);
	x -= width + u(14.0f);
	ae_widget_separator(&screen.density, x, middle, 5.0f, AE_COLOR_MUTED);
	x -= u(14.0f);
	ae_draw_text_tracked(AE_FONT_BODY, crumb, CRUMB_TRACKING, x, cap_top(AE_FONT_BODY, crumb, middle), AE_ALIGN_RIGHT,
		AE_COLOR_TEXT, screen.name);
	ae_draw_rect(left_x(), frame_y(rule_y), right_x() - left_x(), u(RULE_U), 0.0f, AE_COLOR_RULE);
}

/* the filter chips between LB and RB under the title (CHANGE GAMETYPE's line), one on, the focused one ringed */
static void draw_filters(void)
{
	struct ae_rect rects[AE_SETUP_FILTERS];
	float glyph = ae_size_glyph(&screen.density), y = frame_y(CHIPS_Y), x = left_x(), chips_x, room, end;
	int filter;

	for (filter = 0; filter < AE_SETUP_FILTERS; filter++)
	{
		short count = ae_setup_filter_count(&screen.access, filter);

		snprintf(screen.chip_labels[filter], sizeof(screen.chip_labels[filter]), "%s", ae_setup_filter_name(filter));
		screen.chips[filter].label = screen.chip_labels[filter];
		screen.chips[filter].count = count;
		screen.chips[filter].flags = filter == screen.filter ? AE_CHIP_ON : count || filter == AE_SF_ALL ? 0 :
			AE_CHIP_DISABLED;
		if (screen.on_chips && filter == screen.filter)
			screen.chips[filter].flags |= AE_CHIP_FOCUSED;
	}
	chips_x = x + ae_draw_button_width(AE_FONT_LAST_DEVICE, AE_BUTTON_LEFT_SHOULDER, glyph) + u(GLYPH_GAP_U);
	room = right_x() - chips_x - ae_draw_button_width(AE_FONT_LAST_DEVICE, AE_BUTTON_RIGHT_SHOULDER, glyph) - u(GLYPH_GAP_U);
	ae_chips_layout(&screen.density, room, screen.chips, AE_SETUP_FILTERS, rects);
	ae_widget_chip_key(AE_FONT_LAST_DEVICE, AE_BUTTON_LEFT_SHOULDER, glyph, x, y + rects[0].height * 0.5f);
	ae_widget_chips(&screen.density, chips_x, y, room, screen.chips, AE_SETUP_FILTERS, HIT_CHIPS);
	end = chips_x + rects[AE_SETUP_FILTERS - 1].x + rects[AE_SETUP_FILTERS - 1].width + u(GLYPH_GAP_U);
	ae_widget_chip_key(AE_FONT_LAST_DEVICE, AE_BUTTON_RIGHT_SHOULDER, glyph, end,
		y + rects[AE_SETUP_FILTERS - 1].y + rects[AE_SETUP_FILTERS - 1].height * 0.5f);
}

/* the rail: the shown groups, the focus's lit */
static void draw_rail(float x, float y, float width)
{
	struct ae_setup_group_info groups[AE_SETUP_GROUPS];
	float text = ae_size_text(&screen.density), minor = ae_size_minor(&screen.density), pitch = u(RAIL_PITCH_U);
	short current = screen.on_chips ? -1 : ae_setup_group_at(screen.entries, screen.count, screen.list.list.focus);
	int group;

	ae_setup_groups(&screen.access, screen.entries, screen.count, groups);
	for (group = 0; group < AE_SETUP_GROUPS; group++)
	{
		float middle = y + (pitch - u(RAIL_GAP_U)) * 0.5f, count_x = x + width - u(RAIL_PAD_U);
		int lit = group == current;
		char rows[8];

		if (groups[group].header < 0)
			continue;
		if (lit)
		{
			ae_draw_rect(x, y, width, pitch - u(RAIL_GAP_U), 0.0f, AE_COLOR_ROW);
			ae_draw_rect(x, y, u(RAIL_STRIPE_U), pitch - u(RAIL_GAP_U), 0.0f, AE_COLOR_ACCENT);
		}
		ae_draw_text(AE_FONT_ROW, text, x + u(RAIL_PAD_U), cap_top(AE_FONT_ROW, text, middle), AE_ALIGN_LEFT,
			lit ? AE_COLOR_TITLE : AE_COLOR_TEXT, ae_setup_group_name(&screen.access, group));
		snprintf(rows, sizeof(rows), "%d", groups[group].rows);
		count_x -= ae_draw_text(AE_FONT_BODY, minor, count_x, cap_top(AE_FONT_BODY, minor, middle), AE_ALIGN_RIGHT,
			AE_COLOR_MUTED, rows);
		if (groups[group].changed)
			ae_widget_separator(&screen.density, count_x - u(14.0f), middle, DOT_U, AE_COLOR_ACCENT);
		ae_hit_add(x, y, width, pitch - u(RAIL_GAP_U), HIT_RAIL, AE_PART_ROW, (short)group);
		y += pitch;
	}
}

static void draw_help_text(float x, float y, float width, const char *text)
{
	char lines[HELP_LINES][160];
	char expanded[320];
	float body = u(screen.density.metrics->body), line = u(screen.density.metrics->body_line);
	int count, index;

	count = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(text, expanded, sizeof(expanded)), width, lines, HELP_LINES);
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, x, y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
}

static void draw_help(float x, float y, float width)
{
	short option = screen.focus_option;

	if (screen.on_chips)
		draw_help_text(x, y, width, ae_setup_filter_help(screen.filter));
	else if (option >= 0)
		draw_help_text(x, y, width, ae_setup_help(option, screen.access.get(screen.access.context, option)));
}

static void draw_footer(void)
{
	short count = 0;

	screen.prompts[count++] = prompt_of(AE_BUTTON_A, screen.on_chips ? "To the list" :
		screen.focus_option == AE_SO_WEAPON_PACKS ? "Open" : ae_string(AE_STR_ALL_VALUES), AE_ACTION_ACCEPT);
	screen.prompts[count++] = prompt_of(AE_BUTTON_B, ae_string(AE_STR_DONE), AE_ACTION_BACK);
	screen.prompts[count] = prompt_of(AE_BUTTON_LEFT_SHOULDER, "Filter", AE_ACTION_TAB_PREVIOUS);
	screen.prompts[count++].pair = 1;
	screen.prompts[count] = prompt_of(AE_BUTTON_LEFT_TRIGGER, "Group", AE_ACTION_PAGE_UP);
	screen.prompts[count++].pair = 1;
	screen.prompts[count++] = prompt_of(AE_BUTTON_Y, "Defaults", AE_ACTION_Y);
	screen.prompts[count++] = prompt_of(AE_BUTTON_X, "Save as new", AE_ACTION_X);
	screen.prompt_count = count;
	ae_widget_prompts(&screen.density, left_x(), screen.frame.rect.height - u(FOOTER_ABOVE_U), screen.prompts, count,
		NULL, right_x(), -1, HIT_PROMPTS);
}

static void options_draw(struct ae_screen *instance)
{
	struct item_context context;
	float top, bottom, rail_width, list_x, list_width, help_y, line;

	(void)instance;
	full_density();
	refresh();
	draw_header(NULL, PAGE_RULE_Y);
	draw_filters();
	top = frame_y(PAGE_CONTENT_Y);
	bottom = screen.frame.rect.height - u(FOOTER_ABOVE_U) - u(FOOTER_ROOM_U);
	line = u(screen.density.metrics->body_line);
	help_y = bottom - line * (float)HELP_LINES;
	rail_width = screen.frame.rect.width * RAIL_WIDTH;
	list_x = left_x() + rail_width + u(LIST_GAP_U);
	list_width = right_x() - list_x;
	draw_rail(left_x(), top, rail_width);
	context.density = &screen.density;
	/* (0.4.2, owner) under the naming step the rows and their help are not drawn: no row cut by its box shows beside it */
	if (!ae_ui_holds(&naming))
	{
		short focus = screen.list.list.focus;

		/* (the focus on the chips: the list shows none) */
		if (screen.on_chips)
			screen.list.list.focus = -1;
		ae_widget_list(&screen.density, &screen.list, list_x, top, list_width, help_y - u(HELP_GAP_U) - top, list_item,
			&context, HIT_LIST);
		screen.list.list.focus = focus;
		ae_draw_rect(list_x, help_y - u(HELP_GAP_U) * 0.5f, list_width, u(RULE_U), 0.0f, AE_COLOR_RULE);
		draw_help(list_x, help_y, list_width);
	}
	draw_footer();
}

/* ---------- input */

/* the focus from the chips back to the list's cursor */
static void chips_leave(void)
{
	screen.on_chips = 0;
	ae_sound_request(AE_SOUND_CURSOR, 0);
	refresh();
}

static int options_handle(struct ae_screen *instance, struct ae_event const *event)
{
	short option;

	(void)instance;
	refresh();
	option = screen.focus_option;
	/* (NAME typing: Enter or START keeps it, Esc or B drops it; the rest is the typing's) */
	if (screen.typing)
	{
		if (event->action == AE_ACTION_START || event->action == AE_ACTION_ACCEPT)
			name_typing_end(1);
		else if (event->action == AE_ACTION_BACK)
			name_typing_end(0);
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		/* (0: the screen closes as before; the leave dialog when there are changes) */
		return leave_or_ask() ? 0 : 1;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		/* (LB / RB: the filter chips, whatever the focus is on) */
		filter_set(filter_next(event->action == AE_ACTION_TAB_NEXT ? 1 : -1));
		return 1;
	case AE_ACTION_PAGE_UP:
	case AE_ACTION_PAGE_DOWN:
		/* (LT / RT: the next group's first row; from the chips, the first group's) */
		if (screen.on_chips)
			chips_leave();
		else
			group_jump(event->action == AE_ACTION_PAGE_DOWN ? 1 : -1);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (screen.on_chips)
			filter_set(filter_next(event->action == AE_ACTION_RIGHT ? 1 : -1));
		else if (option >= 0)
			value_step(option, event->action == AE_ACTION_RIGHT ? 1 : -1);
		return 1;
	case AE_ACTION_ACCEPT:
		if (screen.on_chips)
			chips_leave();
		else if (option >= 0)
			picker_open(option);
		return 1;
	case AE_ACTION_UP:
		/* (up from the first row: onto the chips) */
		if (!screen.on_chips && ae_setup_row_step(screen.entries, screen.count, screen.list.list.focus, -1) ==
			screen.list.list.focus)
		{
			screen.on_chips = 1;
			ae_sound_request(AE_SOUND_CURSOR, 0);
			return 1;
		}
		break;
	case AE_ACTION_DOWN:
		if (screen.on_chips)
		{
			chips_leave();
			return 1;
		}
		break;
	case AE_ACTION_Y:
		defaults_ask();
		return 1;
	case AE_ACTION_X:
		save_as_new();
		return 1;
	default:
		break;
	}
	if (screen.on_chips)
		return 1;
	{
		short direction = event->action == AE_ACTION_UP || event->action == AE_ACTION_PAGE_UP ? -1 : 1;

		if (ae_list_view_event(&screen.list, event))
		{
			skip_headers(direction);
			refresh();
		}
	}
	return 1;
}

static void options_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	short item;

	if (screen.typing)
		return;
	if (ae_prompts_pointer(screen.prompts, screen.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			if (leave_or_ask())
			{
				ae_sound_request(AE_SOUND_BACK, 0);
				ae_ui_pop();
			}
		}
		else
			options_handle(instance, &event);
		return;
	}
	if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit))
	{
		if (hit.id == HIT_RAIL)
		{
			short first = ae_setup_group_first_row(screen.entries, screen.count, hit.index);

			if (first >= 0)
			{
				show_group_row(first);
				platform_log("ae game options: group %s", ae_setup_group_name(&screen.access, hit.index));
				ae_sound_request(AE_SOUND_CURSOR, 0);
				refresh();
			}
			return;
		}
		if (hit.id == HIT_LIST || hit.id == HIT_RAIL)
			screen.on_chips = 0;
		if (hit.id == HIT_CHIPS && hit.part == AE_PART_CHIP)
		{
			if (hit.index >= 0 && hit.index < AE_SETUP_FILTERS &&
				(hit.index == AE_SF_ALL || ae_setup_filter_count(&screen.access, hit.index)))
				filter_set(hit.index);
			return;
		}
		/* (an arrow of a row: a step) */
		if (hit.id == HIT_LIST && (hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT) &&
			hit.index >= 0 && hit.index < screen.count && !ae_setup_entry_is_header(screen.entries[hit.index]))
		{
			ae_list_set_focus(&screen.list.list, hit.index);
			refresh();
			value_step(screen.entries[hit.index], hit.part == AE_PART_ARROW_RIGHT ? 1 : -1);
			return;
		}
	}
	item = ae_list_view_pointer(&screen.list, pointer, HIT_LIST);
	skip_headers(1);
	refresh();
	if (item >= 0 && item < screen.count && !ae_setup_entry_is_header(screen.entries[item]))
		picker_open(screen.entries[item]);
}

static void options_update(struct ae_screen *instance)
{
	(void)instance;
	if (screen.typing && screen.name_edit.active && !ae_field_edit_keys(&screen.name_edit))
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

static void options_leave(struct ae_screen *instance)
{
	(void)instance;
	if (screen.typing)
	{
		screen.typing = 0;
		ae_field_edit_end(&screen.name_edit, 0);
	}
	platform_log("ae game options: closed (%d changed)", ae_setup_changes(&screen.access));
}

static struct ae_screen_class const options_class =
{
	.name = "game options", .leave = options_leave, .handle = options_handle, .draw = options_draw,
	.pointer = options_pointer, .update = options_update,
};

/* ---------- the WEAPON PACKS screen (EQUIPMENT > WEAPON PACKS; owner 2026-10-22, the mockups' option B)

The same frame: the crumb GAME OPTIONS > WEAPON PACKS above the title, the rail (EQUIPMENT lit), one list: PACKS (the
five packs and PACK WEAPONS ON MAP, each a switch; any pack on can be a start weapon (owner 2026-10-23: the
capsules that said which went with the two-pack limit); a pack not installed here and off, NOT INSTALLED) and START WEAPONS
(PRIMARY and SECONDARY, the same rows as EQUIPMENT's). A or left / right toggles a switch, A opens a start weapon's
values; Y DEFAULTS this screen's rows only (the gametype's own, after asking); B back to EQUIPMENT on WEAPON PACKS.
Log: "ae game options: weapon packs open (N rows, on: ...)", "packs focus <label>", the glue's "set ...",
"weapon packs closed (on: ...)". */

enum { PACKS_ENTRIES = 16, HIT_PACKS_LIST = OPTIONS_HIT + 8, HIT_PACKS_PROMPTS };

/* the switch (u; the mockup's 52 x 28 pill) */
#define SWITCH_WIDTH_U 52.0f
#define SWITCH_HEIGHT_U 28.0f
#define SWITCH_INSET_U 4.0f
#define SWITCH_TEXT_GAP_U 14.0f
#define SWITCH_MS 120
/* menu blue (40,148,254): the switch's on fill; its off track the rule's colour at .25 (the slider's) */
#define SWITCH_ON 0x2894FEFFu
#define SWITCH_TRACK_OFF 0xC8D6E040u
#define SWITCH_TRACK_OFF_ON_BAR 0x0F171E40u
#define SWITCH_KNOB 0xFFFFFFFFu
#define SWITCH_KNOB_OFF 0xC8D6E0FFu
#define SWITCH_KNOB_OFF_ON_BAR 0x84939FFFu

static struct
{
	short entries[PACKS_ENTRIES], count;
	struct ae_list_view list;
	struct ae_rect focus_row;
	short focus_option, logged_focus;
	struct ae_prompt prompts[PROMPTS];
	short prompt_count;
	/* the knob of the switch flipped last, sliding */
	struct ae_motion knob;
	short knob_option;
	char help[320], summary[32];
} packs;

static struct ae_screen_class const packs_class;

static unsigned int with_alpha(unsigned int rgba, float factor)
{
	unsigned int alpha = (unsigned int)((float)(rgba & 0xFFu) * (factor < 0.0f ? 0.0f : factor > 1.0f ? 1.0f : factor));

	return (rgba & 0xFFFFFF00u) | alpha;
}

static int packs_is_switch(short option)
{
	return (option >= AE_SO_PACK_FIRST && option < AE_SO_PACK_FIRST + AE_SETUP_PACKS) || option == AE_SO_PACKS_ON_MAP;
}

static int packs_loadout_custom(void)
{
	return screen.access.get(screen.access.context, AE_SO_LOADOUT) == AE_SETUP_LOADOUT_CUSTOM;
}

static short packs_focused(void)
{
	short focus = packs.list.list.focus;

	return focus >= 0 && focus < packs.count && !ae_setup_entry_is_header(packs.entries[focus]) ?
		packs.entries[focus] : -1;
}

static void packs_refresh(void)
{
	short option = packs_focused(), count = ae_setup_packs_entries(&screen.access, packs.entries, PACKS_ENTRIES);

	if (count != packs.count)
	{
		short index;

		packs.count = count;
		ae_list_set_count(&packs.list.list, count);
		for (index = 0; index < count; index++)
		{
			if (packs.entries[index] == option)
				ae_list_set_focus(&packs.list.list, index);
		}
	}
	if (packs.list.list.focus >= 0 && packs.list.list.focus < packs.count &&
		ae_setup_entry_is_header(packs.entries[packs.list.list.focus]))
		ae_list_set_focus(&packs.list.list, ae_setup_row_step(packs.entries, packs.count, packs.list.list.focus, 1));
	packs.focus_option = packs_focused();
	if (packs.focus_option != packs.logged_focus)
	{
		packs.logged_focus = packs.focus_option;
		if (packs.focus_option >= 0)
			platform_log("ae game options: packs focus %s", ae_setup_label(&screen.access, packs.focus_option));
	}
}

static void packs_skip_headers(short direction)
{
	short focus = packs.list.list.focus;

	if (focus < 0 || focus >= packs.count || !ae_setup_entry_is_header(packs.entries[focus]))
		return;
	ae_list_set_focus(&packs.list.list, ae_setup_row_step(packs.entries, packs.count, focus, direction));
	if (ae_setup_entry_is_header(packs.entries[packs.list.list.focus]))
		ae_list_set_focus(&packs.list.list, ae_setup_row_step(packs.entries, packs.count, packs.list.list.focus,
			(short)-direction));
}

/* a switch flipped (A, left or right, a click): a pack not installed here (and off) can't be */
static void packs_toggle(short option)
{
	int current = screen.access.get(screen.access.context, option);

	if (current < 0 || ae_setup_edit_reason(option))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_motion_start(&packs.knob, current ? 1.0f : 0.0f, current ? 0.0f : 1.0f, SWITCH_MS);
	packs.knob_option = option;
	ae_setup_edit_set(option, current ? 0 : 1);
	ae_sound_request(AE_SOUND_CURSOR, 0);
	screen.tick_option = -1;
	refresh();
	packs_refresh();
}

/* A or a click on a row: a switch flips, a start weapon's values open (only with a CUSTOM loadout) */
static void packs_activate(short option)
{
	if (packs_is_switch(option))
	{
		packs_toggle(option);
		return;
	}
	if (!packs_loadout_custom())
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	screen.focus_row = packs.focus_row;
	picker_open(option);
}

static void packs_step(short option, short direction)
{
	if (packs_is_switch(option))
		packs_toggle(option);
	else if (packs_loadout_custom())
		value_step(option, direction);
	else
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

/* this screen's changed rows (the DEFAULTS dialog's count) */
static short packs_changes(void)
{
	short changes = 0, index;

	for (index = 0; index < packs.count; index++)
	{
		short option = packs.entries[index];

		if (!ae_setup_entry_is_header(option) && screen.access.changed(screen.access.context, option))
			changes++;
	}
	return changes;
}

static void packs_defaults_ask(void)
{
	char body[200];
	short changes = packs_changes();

	if (!changes)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	snprintf(body, sizeof(body), "The packs and start weapons go back to %s's own: %d change%s undone.", screen.name,
		changes, changes == 1 ? "" : "s");
	screen.dialog = 4;
	dialog_open(AE_DIALOG_CONFIRM, "Back to defaults?", body, ae_string(AE_STR_CANCEL_CAPS), "DEFAULTS");
}

/* a switch row's right block: the switch (its knob sliding for SWITCH_MS after a flip), ON / OFF left of it, and an
ON pack's capsule left of that; a pack not installed here: NOT INSTALLED, grey (never amber: the OG theme) */
static void packs_switch_block(short option, float x, float y, float width, float height, int on_bar, int disabled)
{
	struct ae_density const *density = &screen.density;
	float pad = u(density->metrics->pad), center = y + height * 0.5f, right = x + width - pad;
	float text = ae_size_text(density), sw = u(SWITCH_WIDTH_U), sh = u(SWITCH_HEIGHT_U), inset = u(SWITCH_INSET_U);
	float knob = sh - 2.0f * inset, sx = right - sw, sy = center - sh * 0.5f, t, text_x;
	int on = screen.access.get(screen.access.context, option) == 1;
	int slot = option - AE_SO_PACK_FIRST + 1;

	if (disabled)
	{
		ae_draw_text(AE_FONT_ROW, text, right, cap_top(AE_FONT_ROW, text, center), AE_ALIGN_RIGHT,
			on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED, "NOT INSTALLED");
		return;
	}
	t = on ? 1.0f : 0.0f;
	if (packs.knob_option == option && ae_motion_running(&packs.knob))
		t = ae_motion_value(&packs.knob);
	ae_draw_rect(sx, sy, sw, sh, sh * 0.5f, on_bar ? SWITCH_TRACK_OFF_ON_BAR : SWITCH_TRACK_OFF);
	if (t > 0.0f)
		ae_draw_rect(sx, sy, sw, sh, sh * 0.5f, with_alpha(SWITCH_ON, t));
	ae_draw_rect(sx + inset + t * (sw - 2.0f * inset - knob), sy + inset, knob, knob, knob * 0.5f,
		t >= 0.5f ? SWITCH_KNOB : on_bar ? SWITCH_KNOB_OFF_ON_BAR : SWITCH_KNOB_OFF);
	text_x = sx - u(SWITCH_TEXT_GAP_U);
	ae_draw_text(AE_FONT_ROW, text, text_x, cap_top(AE_FONT_ROW, text, center), AE_ALIGN_RIGHT,
		on_bar ? AE_COLOR_SELECTION_TEXT : on ? AE_COLOR_ACCENT : AE_COLOR_MUTED, on ? "ON" : "OFF");
}

static void packs_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct item_context const *list = context;
	struct ae_row widget;
	short entry = packs.entries[item];
	int value, on_bar;

	if (ae_setup_entry_is_header(entry))
	{
		float group = list->density->metrics->group_height * list->density->unit;

		ae_widget_group(list->density, x, y + height - group, width,
			ae_setup_section_name(&screen.access, ae_setup_entry_group(entry)));
		return;
	}
	value = screen.access.get(screen.access.context, entry);
	memset(&widget, 0, sizeof(widget));
	widget.label = ae_setup_label(&screen.access, entry);
	widget.flags = flags;
	on_bar = (flags & AE_ROW_ON_BAR) || ((flags & AE_ROW_FOCUSED) && !(flags & AE_ROW_BAR_BY_LIST));
	if (screen.access.changed(screen.access.context, entry))
		widget.flags |= AE_ROW_CHANGED;
	if (flags & AE_ROW_FOCUSED)
	{
		packs.focus_row.x = x;
		packs.focus_row.y = y;
		packs.focus_row.width = width;
		packs.focus_row.height = height;
	}
	if (packs_is_switch(entry))
	{
		int disabled = ae_setup_edit_reason(entry) != NULL;

		/* (the value is the switch, drawn after the row: no text, no arrows; a pack not installed is dimmed, but on
		the selection bar its label stays readable, its reason on the help line) */
		if (disabled && !on_bar)
			widget.flags |= AE_ROW_DISABLED;
		ae_widget_row(list->density, x, y, width, &widget, HIT_PACKS_LIST, item);
		packs_switch_block(entry, x, y, width, height, on_bar, disabled);
		return;
	}
	/* (PRIMARY / SECONDARY: as EQUIPMENT's; read only, grey, while the loadout follows the weapon set) */
	if (!packs_loadout_custom())
	{
		float text = ae_size_minor(list->density);

		widget.flags = (widget.flags | AE_ROW_DISABLED) & ~(unsigned int)AE_ROW_CHANGED;
		ae_widget_row(list->density, x, y, width, &widget, HIT_PACKS_LIST, item);
		ae_draw_text(AE_FONT_ROW, text, x + width - u(list->density->metrics->pad), cap_top(AE_FONT_ROW, text,
			y + height * 0.5f), AE_ALIGN_RIGHT, on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED, "LOADOUT: CATEGORY");
		return;
	}
	widget.value = ae_setup_value_text(entry, value);
	if (widget.flags & AE_ROW_CHANGED)
		widget.base_value = ae_setup_value_text(entry, ae_setup_edit_base(entry));
	if (entry == screen.tick_option)
	{
		widget.tick = &screen.tick;
		widget.tick_side = screen.tick_side;
		widget.old_value = screen.tick_old;
	}
	ae_widget_row(list->density, x, y, width, &widget, HIT_PACKS_LIST, item);
}

static const char *packs_help(void)
{
	short option = packs.focus_option;
	const char *text;

	if (option < 0)
		return "";
	text = ae_setup_pack_help(&screen.access, option, ae_setup_edit_reason(option) == NULL, packs.help,
		sizeof(packs.help));
	if (text)
		return text;
	if ((option == AE_SO_PRIMARY || option == AE_SO_SECONDARY) && !packs_loadout_custom())
		return "The weapon set picks the start weapons. Set LOADOUT to CUSTOM in EQUIPMENT to choose them here.";
	return ae_setup_help(option, screen.access.get(screen.access.context, option));
}

static void packs_footer(void)
{
	short count = 0;
	int start_row = packs.focus_option == AE_SO_PRIMARY || packs.focus_option == AE_SO_SECONDARY;

	packs.prompts[count++] = prompt_of(AE_BUTTON_A, start_row ? ae_string(AE_STR_ALL_VALUES) : "Toggle",
		AE_ACTION_ACCEPT);
	packs.prompts[count++] = prompt_of(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	packs.prompts[count++] = prompt_of(AE_BUTTON_Y, "Defaults", AE_ACTION_Y);
	packs.prompt_count = count;
	ae_widget_prompts(&screen.density, left_x(), screen.frame.rect.height - u(FOOTER_ABOVE_U), packs.prompts, count,
		NULL, right_x(), -1, HIT_PACKS_PROMPTS);
}

static void packs_draw(struct ae_screen *instance)
{
	struct item_context context;
	float top, bottom, rail_width, list_x, list_width, help_y, line;

	(void)instance;
	full_density();
	refresh();
	packs_refresh();
	draw_header("WEAPON PACKS", RULE_Y);
	top = frame_y(CONTENT_Y);
	bottom = screen.frame.rect.height - u(FOOTER_ABOVE_U) - u(FOOTER_ROOM_U);
	line = u(screen.density.metrics->body_line);
	help_y = bottom - line * (float)HELP_LINES;
	rail_width = screen.frame.rect.width * RAIL_WIDTH;
	list_x = left_x() + rail_width + u(LIST_GAP_U);
	list_width = right_x() - list_x;
	/* (the rail as GAME OPTIONS': EQUIPMENT lit, its focus is on WEAPON PACKS) */
	draw_rail(left_x(), top, rail_width);
	context.density = &screen.density;
	ae_widget_list(&screen.density, &packs.list, list_x, top, list_width, help_y - u(HELP_GAP_U) - top, packs_item,
		&context, HIT_PACKS_LIST);
	ae_draw_rect(list_x, help_y - u(HELP_GAP_U) * 0.5f, list_width, u(RULE_U), 0.0f, AE_COLOR_RULE);
	draw_help_text(list_x, help_y, list_width, packs_help());
	packs_footer();
}

static int packs_handle(struct ae_screen *instance, struct ae_event const *event)
{
	short option;

	(void)instance;
	refresh();
	packs_refresh();
	option = packs.focus_option;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		/* (0: the screen pops, back to EQUIPMENT on WEAPON PACKS) */
		return 0;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (option >= 0)
			packs_step(option, event->action == AE_ACTION_RIGHT ? 1 : -1);
		return 1;
	case AE_ACTION_ACCEPT:
		if (option >= 0)
			packs_activate(option);
		return 1;
	case AE_ACTION_Y:
		packs_defaults_ask();
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
	case AE_ACTION_X:
		return 1;
	default:
		break;
	}
	{
		short direction = event->action == AE_ACTION_UP || event->action == AE_ACTION_PAGE_UP ? -1 : 1;

		if (ae_list_view_event(&packs.list, event))
		{
			packs_skip_headers(direction);
			packs_refresh();
		}
	}
	return 1;
}

static void packs_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	short item;

	if (ae_prompts_pointer(packs.prompts, packs.prompt_count, pointer, HIT_PACKS_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			packs_handle(instance, &event);
		return;
	}
	if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit))
	{
		/* (the rail: back to GAME OPTIONS on that group) */
		if (hit.id == HIT_RAIL)
		{
			short first = ae_setup_group_first_row(screen.entries, screen.count, hit.index);

			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
			if (first >= 0)
			{
				show_group_row(first);
				platform_log("ae game options: group %s", ae_setup_group_name(&screen.access, hit.index));
				refresh();
			}
			return;
		}
		if (hit.id == HIT_PACKS_LIST && (hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT) &&
			hit.index >= 0 && hit.index < packs.count && !ae_setup_entry_is_header(packs.entries[hit.index]))
		{
			ae_list_set_focus(&packs.list.list, hit.index);
			packs_refresh();
			packs_step(packs.entries[hit.index], hit.part == AE_PART_ARROW_RIGHT ? 1 : -1);
			return;
		}
	}
	item = ae_list_view_pointer(&packs.list, pointer, HIT_PACKS_LIST);
	packs_skip_headers(1);
	packs_refresh();
	if (item >= 0 && item < packs.count && !ae_setup_entry_is_header(packs.entries[item]))
		packs_activate(packs.entries[item]);
}

static void packs_leave(struct ae_screen *instance)
{
	(void)instance;
	ae_setup_packs_summary(&screen.access, packs.summary, sizeof(packs.summary));
	platform_log("ae game options: weapon packs closed (on: %s)", packs.summary);
}

static struct ae_screen_class const packs_class =
{
	.name = "weapon packs", .leave = packs_leave, .handle = packs_handle, .draw = packs_draw,
	.pointer = packs_pointer,
};

static int packs_open(void)
{
	memset(&packs, 0, sizeof(packs));
	packs.logged_focus = -1;
	packs.knob_option = -1;
	refresh();
	packs.count = ae_setup_packs_entries(&screen.access, packs.entries, PACKS_ENTRIES);
	if (packs.count < 2)
		return 0;
	ae_list_view_init(&packs.list, packs.count, 10);
	/* (the first pack, under PACKS' header) */
	ae_list_set_focus(&packs.list.list, 1);
	packs.list.previous_focus = 1;
	if (!ae_ui_push(&packs_class, AE_OWNER_PARENT, &packs))
		return 0;
	ae_setup_packs_summary(&screen.access, packs.summary, sizeof(packs.summary));
	platform_log("ae game options: weapon packs open (%d rows, on: %s)", packs.count, packs.summary);
	return 1;
}

/* the tests' hooks (the harness types no text): HALO_AE_TEST_SET "LABEL=index;LABEL=index" sets rows by value index,
HALO_AE_TEST_NAME renames the copy, HALO_AE_TEST_SAVE saves it as new and opens the new file again (the rules read
back from the file), and with it or HALO_AE_TEST_DUMP every row's value index is logged as "ae game options: value
LABEL=index" */
static void test_hooks(void)
{
	char const *sets = getenv("HALO_AE_TEST_SET"), *rename = getenv("HALO_AE_TEST_NAME");
	int save = getenv("HALO_AE_TEST_SAVE") != NULL, dump = save || getenv("HALO_AE_TEST_DUMP") != NULL;
	int option;

	while (sets && *sets)
	{
		char label[64], *equals;
		char const *end = strchr(sets, ';');
		size_t length = end ? (size_t)(end - sets) : strlen(sets);

		snprintf(label, sizeof(label), "%.*s", (int)length, sets);
		equals = strrchr(label, '=');
		if (equals)
		{
			*equals = 0;
			for (option = 0; option < AE_SETUP_OPTIONS; option++)
			{
				if (!strcmp(ae_setup_label(NULL, option), label))
				{
					ae_setup_edit_set(option, atoi(equals + 1));
					break;
				}
			}
		}
		sets = end ? end + 1 : NULL;
	}
	if (rename)
		ae_setup_edit_set_name(rename);
	if (save)
	{
		char saved[16];

		if (ae_setup_edit_save_as_new(saved, sizeof(saved)) && ae_setup_edit_begin_named(saved))
			platform_log("ae game options: test: reopened '%s'", saved);
		else
			platform_log("ae game options: test: save failed");
	}
	if (dump)
	{
		struct ae_setup_access access;

		ae_setup_edit_access(&access);
		for (option = 0; option < AE_SETUP_OPTIONS; option++)
		{
			if (access.get(access.context, option) >= 0)
				platform_log("ae game options: value %s=%d", ae_setup_label(NULL, option),
					access.get(access.context, option));
		}
	}
}

int ae_screen_game_options_open(char const *gametype)
{
	char const *name = gametype ? gametype : getenv("HALO_AE_TEST_GAMETYPE");

	if (!ae_setup_edit_begin_named(name))
		return 0;
	test_hooks();
	ae_setup_edit_check();
	if (!ae_screen_game_options_show())
		return 0;
	ae_ui_replace_menus();
	return 1;
}

int ae_screen_game_options_show(void)
{
	if (!ae_setup_edit_open())
		return 0;
	memset(&screen, 0, sizeof(screen));
	screen.tick_option = -1;
	screen.logged_focus = -1;
	screen.logged_width = -1.0f;
	ae_setup_edit_name(screen.name, sizeof(screen.name));
	ae_setup_edit_access(&screen.access);
	screen.count = ae_setup_entries(&screen.access, screen.entries, ENTRIES_MAXIMUM);
	ae_list_view_init(&screen.list, screen.count, 10);
	/* (the first row, under MATCH's header) */
	ae_list_set_focus(&screen.list.list, 1);
	screen.list.previous_focus = 1;
	if (!ae_ui_push(&options_class, AE_OWNER_PARENT, &screen))
		return 0;
	platform_log("ae game options: open '%s' engine %d rows %d", screen.name, screen.access.engine, screen.count);
	(void)ae_settings_ui_scale();
	return 1;
}
