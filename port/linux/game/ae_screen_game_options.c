/*
AE_SCREEN_GAME_OPTIONS.C

The game setup spike's GAME OPTIONS screen (ae_screen_game_options.h). One page (the mockup's board 2,
https://claude.ai/artifact/DNum7ugkfszzX99GuZEpsD): the title and the gametype with its changes top right; the groups'
rail on the left (name, rows, a dot when changed; the focus's group lit, a click jumps to it); one list of every rule
the gametype has, under its groups' headers; the focused value's help under the list; the prompts. Positions are
fractions of the frame (ae_frame_compute), sizes u, as the gallery's list page.

Input (anyone's): up / down and the triggers move (past the headers); left / right step the value in place; A opens
the values' picker; LB / RB jump a group; Y DEFAULTS (asks first); X SAVE AS NEW; B DONE (the copy is kept for the
game). The pointer: hover focuses, a click on a row opens its picker, on an arrow steps it, on the rail jumps.

Its log: "ae game options: open '<name>' engine E rows R", "focus <label>", "group <name>", the glue's "set ..." and
"defaults", "saved as new ...", "closed (N changed)".
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
	ENTRIES_MAXIMUM = 128,
	PROMPTS = 6
};

/* the frame's positions (fractions of 1920 x 1080 at 100 %) */
#define TITLE_Y (70.0f / 1080.0f)
#define RULE_Y (156.0f / 1080.0f)
#define CONTENT_Y (186.0f / 1080.0f)
#define RAIL_WIDTH 0.17f
/* sizes, u */
#define TITLE_U 60.0f
#define CRUMB_U 20.0f
#define CRUMB_TRACKING 0.09f
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
	char name[16];
	int dialog;                          /* 0 none, 1 DEFAULTS, 2 saved / failed */
	float logged_width, logged_scale;
} screen;

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

/* the entries again (a value can show or hide rows: TEAM PLAY, LOADOUT, a CUSTOM vehicle set); the focus stays on
its option, else on the row now at its place */
static void refresh(void)
{
	short option = focused_option(), count;

	ae_setup_edit_access(&screen.access);
	count = ae_setup_entries(&screen.access, screen.entries, ENTRIES_MAXIMUM);
	if (count != screen.count)
	{
		short index;

		screen.count = count;
		ae_list_set_count(&screen.list.list, count);
		index = option >= 0 ? index_of_option(option) : -1;
		if (index >= 0)
			ae_list_set_focus(&screen.list.list, index);
	}
	/* (never a header) */
	if (screen.list.list.focus >= 0 && screen.list.list.focus < screen.count &&
		ae_setup_entry_is_header(screen.entries[screen.list.list.focus]))
		ae_list_set_focus(&screen.list.list, ae_setup_row_step(screen.entries, screen.count, screen.list.list.focus, 1));
	screen.focus_option = focused_option();
	if (screen.focus_option != screen.logged_focus)
	{
		screen.logged_focus = screen.focus_option;
		if (screen.focus_option >= 0)
			platform_log("ae game options: focus %s", ae_setup_label(&screen.access, screen.focus_option));
	}
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

/* ---------- changing values */

static void value_step(short option, short direction)
{
	struct ae_setup_row const *row = ae_setup_row(option);
	int current = screen.access.get(screen.access.context, option);
	short next;

	if (!row || current < 0)
		return;
	next = ae_value_step(row->value_count, (short)current, direction);
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

	ae_setup_edit_set(option, index);
	screen.tick_option = -1;
	refresh();
}

static void picker_open(short option)
{
	struct ae_setup_row const *row = ae_setup_row(option);
	struct ae_picker_spec spec;

	if (!row)
		return;
	memset(&spec, 0, sizeof(spec));
	/* (the row's own static texts: they outlive the picker and its ghost) */
	spec.values = row->values;
	spec.count = row->value_count;
	spec.current = (short)screen.access.get(screen.access.context, option);
	spec.row = screen.focus_row;
	spec.bounds = screen.frame.rect;
	spec.density = screen.density;
	spec.picked = picked;
	spec.context = (void *)(intptr_t)option;
	if (ae_picker_open(&spec, AE_OWNER_ANY))
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

static void dialog_picked(short choice, void *context)
{
	(void)context;
	if (screen.dialog == 1 && choice == 1)
	{
		ae_setup_edit_defaults();
		screen.tick_option = -1;
		refresh();
	}
	screen.dialog = 0;
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
	ae_dialog_open(&spec, AE_OWNER_ANY);
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
	char saved[16], body[160];

	if (ae_setup_edit_save_as_new(saved, sizeof(saved)))
	{
		ae_setup_edit_name(screen.name, sizeof(screen.name));
		snprintf(body, sizeof(body), "These rules are saved as a new game type, %s. Rename it in EDIT GAME TYPES.", saved);
		screen.dialog = 2;
		dialog_open(AE_DIALOG_CONFIRM, "Saved", body, "OK", NULL);
		refresh();
		return;
	}
	screen.dialog = 2;
	dialog_open(AE_DIALOG_ERROR, "Not saved", "The new game type could not be written.", ae_string(AE_STR_BACK), NULL);
}

/* ---------- drawing */

struct item_context { struct ae_density const *density; };

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
	if (row && row->ae)
		widget.flags |= AE_ROW_AE;
	if (screen.access.changed(screen.access.context, entry))
	{
		widget.flags |= AE_ROW_CHANGED;
		widget.base_value = ae_setup_value_text(entry, base);
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
	ae_widget_row(list->density, x, y, width, &widget, HIT_LIST, item);
}

static void draw_header(void)
{
	char changed[32];
	float size = u(TITLE_U), top = frame_y(TITLE_Y), crumb = u(CRUMB_U), x = right_x(), middle, width;
	short changes = ae_setup_changes(&screen.access);
	unsigned int color = changes ? AE_COLOR_ACCENT : AE_COLOR_MUTED;

	middle = top + ae_draw_cap_height(AE_FONT_TITLE, size) * 0.5f;
	ae_draw_text(AE_FONT_TITLE, size, left_x(), top, AE_ALIGN_LEFT, AE_COLOR_TITLE, "GAME OPTIONS");
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
	ae_draw_rect(left_x(), frame_y(RULE_Y), right_x() - left_x(), u(RULE_U), 0.0f, AE_COLOR_RULE);
}

/* the rail: the shown groups, the focus's lit */
static void draw_rail(float x, float y, float width)
{
	struct ae_setup_group_info groups[AE_SETUP_GROUPS];
	float text = ae_size_text(&screen.density), minor = ae_size_minor(&screen.density), pitch = u(RAIL_PITCH_U);
	short current = ae_setup_group_at(screen.entries, screen.count, screen.list.list.focus);
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
			ae_widget_separator(&screen.density, count_x - u(10.0f), middle, DOT_U, AE_COLOR_ACCENT);
		ae_hit_add(x, y, width, pitch - u(RAIL_GAP_U), HIT_RAIL, AE_PART_ROW, (short)group);
		y += pitch;
	}
}

static void draw_help(float x, float y, float width)
{
	char lines[HELP_LINES][160];
	short option = screen.focus_option;
	float body = u(screen.density.metrics->body), line = u(screen.density.metrics->body_line);
	int count, index;

	if (option < 0)
		return;
	count = ae_wrap_text(AE_FONT_BODY, body, ae_setup_help(option, screen.access.get(screen.access.context, option)),
		width, lines, HELP_LINES);
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, x, y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
}

static void draw_footer(void)
{
	short count = 0;

	screen.prompts[count++] = prompt_of(AE_BUTTON_A, ae_string(AE_STR_ALL_VALUES), AE_ACTION_ACCEPT);
	screen.prompts[count++] = prompt_of(AE_BUTTON_B, ae_string(AE_STR_DONE), AE_ACTION_BACK);
	screen.prompts[count++] = prompt_of(AE_BUTTON_LEFT_SHOULDER, "Group", AE_ACTION_TAB_PREVIOUS);
	screen.prompts[count++] = prompt_of(AE_BUTTON_RIGHT_SHOULDER, "Group", AE_ACTION_TAB_NEXT);
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
	ae_draw_view_full();
	full_density();
	refresh();
	draw_header();
	top = frame_y(CONTENT_Y);
	bottom = screen.frame.rect.height - u(FOOTER_ABOVE_U) - u(FOOTER_ROOM_U);
	line = u(screen.density.metrics->body_line);
	help_y = bottom - line * (float)HELP_LINES;
	rail_width = screen.frame.rect.width * RAIL_WIDTH;
	list_x = left_x() + rail_width + u(LIST_GAP_U);
	list_width = right_x() - list_x;
	draw_rail(left_x(), top, rail_width);
	context.density = &screen.density;
	ae_widget_list(&screen.density, &screen.list, list_x, top, list_width, help_y - u(HELP_GAP_U) - top, list_item,
		&context, HIT_LIST);
	ae_draw_rect(list_x, help_y - u(HELP_GAP_U) * 0.5f, list_width, u(RULE_U), 0.0f, AE_COLOR_RULE);
	draw_help(list_x, help_y, list_width);
	draw_footer();
}

/* ---------- input */

static int options_handle(struct ae_screen *instance, struct ae_event const *event)
{
	short option;

	(void)instance;
	refresh();
	option = screen.focus_option;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		group_jump(event->action == AE_ACTION_TAB_NEXT ? 1 : -1);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (option >= 0)
			value_step(option, event->action == AE_ACTION_RIGHT ? 1 : -1);
		return 1;
	case AE_ACTION_ACCEPT:
		if (option >= 0)
			picker_open(option);
		return 1;
	case AE_ACTION_Y:
		defaults_ask();
		return 1;
	case AE_ACTION_X:
		save_as_new();
		return 1;
	default:
		break;
	}
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

	if (ae_prompts_pointer(screen.prompts, screen.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
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

static void options_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae game options: closed (%d changed)", ae_setup_changes(&screen.access));
}

static struct ae_screen_class const options_class =
{
	.name = "game options", .leave = options_leave, .handle = options_handle, .draw = options_draw,
	.pointer = options_pointer,
};

int ae_screen_game_options_open(char const *gametype)
{
	char const *name = gametype ? gametype : getenv("HALO_AE_TEST_GAMETYPE");

	if (!ae_setup_edit_begin_named(name))
		return 0;
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
	if (!ae_ui_push(&options_class, AE_OWNER_ANY, &screen))
		return 0;
	platform_log("ae game options: open '%s' engine %d rows %d", screen.name, screen.access.engine, screen.count);
	(void)ae_settings_ui_scale();
	return 1;
}
