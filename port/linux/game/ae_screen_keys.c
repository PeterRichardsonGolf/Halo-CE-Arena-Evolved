/*
AE_SCREEN_KEYS.C

The 0.3.9 menus' KEY BINDINGS and BACK PADDLES (ae_screen_keys.h). Both are laid out as SETTINGS' group screens
(ae_screen_settings.c: the title and the crumb, the sections' rail, one list under its sections' headers, the help, the
prompts), with the same positions and sizes, so that they read as SETTINGS' own pages. Neither draw resets the view at
its start: the screen's slide and fade (ae_hooks' before_draw) stay theirs.
*/

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "interface/player_ui.h"
#include "saved games/player_profile.h"
#include "../src/ae_draw.h"
#include "../src/controller_layout.h"
#include "ae_custom_layout_rules.h"
#include "ae_glue_player_settings.h"
#include "ae_glue_profiles.h"
#include "ae_input_rules.h"
#include "ae_dev_mode.h"
#include "ae_hooks.h"
#include "ae_key_bindings.h"
#include "ae_player_settings.h"
#include "ae_screen_keys.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);
int config_write(const char *name, const char *value);
int config_text(const char *name, char *text, size_t size);
int config_default(const char *name, char *text, size_t size);
int config_boolean(const char *name);
/* (sdl_platform.c / xinput_sdl.c, as menu_functions.c declares them: the classic menus' Controls Setup's) */
void platform_binding_capture_begin(void);
int platform_binding_capture_poll(int *input);
void halo_input_name(int input, char *name, size_t size);
void halo_input_shown_name(const char *binding, char *shown, size_t size);

enum
{
	KEYS_HIT = 0x7E40,
	/* (the caps' own id: under the list's, a cap's index (item x 2 + slot) read as an item made a hover focus a row
	far below, and the list scrolled to it under the pointer, again and again) */
	HIT_LIST = KEYS_HIT + 1, HIT_RAIL, HIT_PROMPTS, HIT_CAP,
	PROMPTS = 6,
	TEXT_SIZE = 128,
	PLAYERS = 4
};

/* SETTINGS' group screens' frame (fractions of 1920 x 1080 at 100 %) and sizes (u): ae_screen_settings.c's */
#define TITLE_Y (70.0f / 1080.0f)
#define RULE_Y (156.0f / 1080.0f)
#define CONTENT_Y (186.0f / 1080.0f)
#define RAIL_WIDTH 0.17f
#define TITLE_U 60.0f
#define CRUMB_U 20.0f
#define CRUMB_TRACKING 0.09f
#define CRUMB_GAP_U 14.0f
#define CRUMB_DOT_U 5.0f
#define EMBLEM_U 30.0f
#define EMBLEM_NUMBER_U 20.0f
#define EMBLEM_GAP_U 12.0f
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
/* the key caps: AE_ROW_KEY's (ae_widgets.c: 30 u high, its text 15 u, 3 u corners), in two columns of 176 u, 16 u
apart, the second at the row's padding; a header's column names as its section's (Overpass 750, tracked, muted) */
#define CAP_HEIGHT_U 30.0f
#define CAP_TEXT_U 15.0f
#define CAP_CORNER_U 3.0f
#define CAP_PAD_U 9.0f
#define CAP_WIDTH_U 176.0f
#define CAP_GAP_U 16.0f
#define CAP_OUTLINE_U 2.0f
#define CAP_DASH_U 6.0f
#define HEADER_INSET_U 10.0f

static unsigned int const player_colors[PLAYERS] = { 0xC0392BFFu, 0x2E86DEFFu, 0x7FA035FFu, 0xE6B422FFu };

/* ---------- what both screens draw alike */

struct page
{
	struct ae_density density;
	struct ae_frame frame;
	float logged_width, logged_scale;
};

static float pu(struct page const *page, float spec_units)
{
	return spec_units * page->frame.unit;
}

static float cap_top(int font, float size, float center_y)
{
	return center_y - ae_draw_cap_height(font, size) * 0.5f;
}

static void upper(const char *text, char *out, int size)
{
	int at;

	for (at = 0; at < size - 1 && text[at]; at++)
		out[at] = text[at] >= 'a' && text[at] <= 'z' ? (char)(text[at] - 'a' + 'A') : text[at];
	out[at] = 0;
}

static void page_density(struct page *page, const char *log_name)
{
	struct ae_layout layout;
	float scale = ae_settings_ui_scale();

	ae_draw_current_layout(&layout);
	ae_density_full(layout.height * layout.scale, scale, &page->density);
	ae_frame_compute(layout.width, scale, &page->frame);
	if (layout.width != page->logged_width || layout.scale != page->logged_scale)
	{
		page->logged_width = layout.width;
		page->logged_scale = layout.scale;
		platform_log("%s: density FULL s=%.2f frame=%.0fx%.0f", log_name, page->density.s,
			page->frame.rect.width * layout.scale, page->frame.rect.height * layout.scale);
	}
}

/* the title, the crumb right-aligned on the title's capitals' middle (parts between dots, the last in last_color, the
player's emblem first when player >= 0), the rule under them */
static void page_header(struct page const *page, const char *title, const char *const *parts, short count,
	unsigned int last_color, short player)
{
	float size = pu(page, TITLE_U), top_y = ae_frame_y(&page->frame, TITLE_Y), middle, crumb = pu(page, CRUMB_U);
	float left = page->frame.rect.x + page->frame.margin, right = page->frame.rect.x + page->frame.rect.width -
		page->frame.margin, x = right;
	short index;

	middle = top_y + ae_draw_cap_height(AE_FONT_TITLE, size) * 0.5f;
	ae_draw_text(AE_FONT_TITLE, size, left, top_y, AE_ALIGN_LEFT, AE_COLOR_TITLE, title);
	for (index = (short)(count - 1); index >= 0; index--)
	{
		x -= ae_draw_text_tracked(AE_FONT_BODY, crumb, CRUMB_TRACKING, x, cap_top(AE_FONT_BODY, crumb, middle),
			AE_ALIGN_RIGHT, index == count - 1 ? last_color : AE_COLOR_TEXT, parts[index]);
		if (index > 0)
		{
			x -= pu(page, CRUMB_GAP_U);
			ae_widget_separator(&page->density, x, middle, CRUMB_DOT_U, AE_COLOR_MUTED);
			x -= pu(page, CRUMB_GAP_U);
		}
	}
	if (player >= 0)
	{
		float emblem = pu(page, EMBLEM_U), number = pu(page, EMBLEM_NUMBER_U);
		char text[4];

		x -= pu(page, EMBLEM_GAP_U) + emblem;
		ae_draw_rect(x, middle - emblem * 0.5f, emblem, emblem, emblem * 0.07f, player_colors[player % PLAYERS]);
		snprintf(text, sizeof(text), "%d", player + 1);
		ae_draw_text(AE_FONT_ROW, number, x + emblem * 0.5f, cap_top(AE_FONT_ROW, number, middle), AE_ALIGN_CENTER,
			ae_emblem_number_color(player_colors[player % PLAYERS]), text);
	}
	ae_draw_rect(left, ae_frame_y(&page->frame, RULE_Y), right - left, pu(page, RULE_U), 0.0f, AE_COLOR_RULE);
}

/* a rail item: lit (ROW fill, the accent stripe), its name, its count right, a dot where a change is */
static void page_rail_item(struct page const *page, float x, float y, float width, const char *name, short rows,
	int lit, int changed, short index)
{
	float text = ae_size_text(&page->density), minor = ae_size_minor(&page->density), pitch = pu(page, RAIL_PITCH_U);
	float middle = y + (pitch - pu(page, RAIL_GAP_U)) * 0.5f, count_x = x + width - pu(page, RAIL_PAD_U);
	char count[8];

	if (lit)
	{
		ae_draw_rect(x, y, width, pitch - pu(page, RAIL_GAP_U), 0.0f, AE_COLOR_ROW);
		ae_draw_rect(x, y, pu(page, RAIL_STRIPE_U), pitch - pu(page, RAIL_GAP_U), 0.0f, AE_COLOR_ACCENT);
	}
	ae_draw_text(AE_FONT_ROW, text, x + pu(page, RAIL_PAD_U), cap_top(AE_FONT_ROW, text, middle), AE_ALIGN_LEFT,
		lit ? AE_COLOR_TITLE : AE_COLOR_TEXT, name);
	snprintf(count, sizeof(count), "%d", rows);
	count_x -= ae_draw_text(AE_FONT_BODY, minor, count_x, cap_top(AE_FONT_BODY, minor, middle), AE_ALIGN_RIGHT,
		AE_COLOR_MUTED, count);
	if (changed)
		ae_widget_separator(&page->density, count_x - pu(page, 14.0f), middle, DOT_U, AE_COLOR_ACCENT);
	ae_hit_add(x, y, width, pitch - pu(page, RAIL_GAP_U), HIT_RAIL, AE_PART_ROW, index);
}

/* the content's parts: the rail's left and width, the list's left, width, top and height, the help's top */
struct page_parts { float left, top, rail_width, list_x, list_width, list_height, help_y; };

static void page_parts(struct page const *page, struct page_parts *parts)
{
	float bottom = page->frame.rect.height - pu(page, FOOTER_ABOVE_U) - pu(page, FOOTER_ROOM_U);

	parts->left = page->frame.rect.x + page->frame.margin;
	parts->top = ae_frame_y(&page->frame, CONTENT_Y);
	parts->help_y = bottom - pu(page, page->density.metrics->body_line) * (float)HELP_LINES;
	parts->rail_width = page->frame.rect.width * RAIL_WIDTH;
	parts->list_x = parts->left + parts->rail_width + pu(page, LIST_GAP_U);
	parts->list_width = page->frame.rect.x + page->frame.rect.width - page->frame.margin - parts->list_x;
	parts->list_height = parts->help_y - pu(page, HELP_GAP_U) - parts->top;
}

/* the help: the rule over it, then up to two lines (the first in first_color) */
static void page_help(struct page const *page, struct page_parts const *parts, const char *first,
	unsigned int first_color, const char *second)
{
	float body = pu(page, page->density.metrics->body), line = pu(page, page->density.metrics->body_line);
	char lines[HELP_LINES][160];
	char expanded[320];
	int count, index, at = 0;

	ae_draw_rect(parts->list_x, parts->help_y - pu(page, HELP_GAP_U) * 0.5f, parts->list_width, pu(page, RULE_U), 0.0f,
		AE_COLOR_RULE);
	if (first && first[0])
	{
		count = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(first, expanded, sizeof(expanded)), parts->list_width, lines, second && second[0] ? 1 : HELP_LINES);
		for (index = 0; index < count; index++, at++)
			ae_draw_text(AE_FONT_BODY, body, parts->list_x, parts->help_y + line * (float)at, AE_ALIGN_LEFT, first_color,
				lines[index]);
	}
	if (second && second[0] && at < HELP_LINES)
	{
		count = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(second, expanded, sizeof(expanded)), parts->list_width, lines, HELP_LINES - at);
		for (index = 0; index < count; index++, at++)
			ae_draw_text(AE_FONT_BODY, body, parts->list_x, parts->help_y + line * (float)at, AE_ALIGN_LEFT,
				AE_COLOR_TEXT, lines[index]);
	}
}

static struct ae_prompt prompt_of(int button, const char *label, const char *key, int action, int pair)
{
	struct ae_prompt prompt;

	prompt.button = button;
	prompt.label = label;
	prompt.key = key;
	prompt.action = action;
	prompt.pair = pair;
	return prompt;
}

static void page_footer(struct page const *page, struct ae_prompt const *prompts, short count)
{
	float left = page->frame.rect.x + page->frame.margin, right = page->frame.rect.x + page->frame.rect.width -
		page->frame.margin;

	ae_widget_prompts(&page->density, left, page->frame.rect.height - pu(page, FOOTER_ABOVE_U), prompts, count, NULL,
		right, -1, HIT_PROMPTS);
}

static void page_dialog(struct page const *page, const char *title, const char *body, const char *const *choices,
	short count, short safe, short cancel, void (*picked)(short choice, void *context))
{
	struct ae_dialog_spec spec;
	short index;

	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = title;
	spec.body = body;
	for (index = 0; index < count && index < AE_DIALOG_CHOICES; index++)
		spec.choices[index] = choices[index];
	spec.choice_count = count;
	spec.safe_choice = safe;
	spec.cancel_choice = cancel;
	spec.has_cancel = 1;
	spec.density = page->density;
	spec.bounds = page->frame.rect;
	spec.picked = picked;
	ae_dialog_open(&spec, AE_OWNER_PARENT);
}

/* ---------- KEY BINDINGS */

static struct
{
	struct page page;
	short entries[AE_KEYS_ENTRIES], count;
	struct ae_list_view list;
	short slot;                          /* the cap left / right pick on the focused row: 0 PRIMARY, 1 SECONDARY */
	int capturing;                       /* waiting for a key, mouse button or wheel turn */
	short capture_action, capture_slot;
	struct ae_keys now;                  /* every action's keys now (config.toml's) */
	char opened[AE_KEYS_ACTIONS][TEXT_SIZE];   /* each action's setting as the screen opened (joined) */
	char note[160];                      /* the line after a key moved here ("" none); goes with the focus */
	short note_row;
	struct ae_prompt prompts[PROMPTS];
	short prompt_count;
	int dialog;                          /* 0 none, 1 KEEP / UNDO, 2 RESTORE DEFAULTS, 3 already the game's */
	short restore_section;               /* (2) the focused section as asked; its choice is the second, ALL the last */
	short logged_focus;
	int with;                            /* the actions shown (AE_KEYS_WITH_*: the chat, DEV MODE) */
} keys;

static short keys_focus_action(void)
{
	short focus = keys.list.list.focus;

	return focus >= 0 && focus < keys.count && !ae_keys_entry_is_header(keys.entries[focus]) ? keys.entries[focus] :
		-1;
}

static void keys_joined(short action, char *text, size_t size)
{
	ae_keys_join((char const (*)[AE_KEYS_NAME])keys.now.names[action], text, size);
}

static int keys_changed(short action)
{
	char text[TEXT_SIZE];

	keys_joined(action, text, sizeof(text));
	return strcmp(text, keys.opened[action]) != 0;
}

/* the actions changed since the screen opened (their labels, the list's order): the count */
static short keys_changes(const char **labels, short maximum)
{
	short place, found = 0;

	for (place = 0; place < keys.count; place++)
	{
		short entry = keys.entries[place];

		if (ae_keys_entry_is_header(entry) || !keys_changed(entry))
			continue;
		if (labels && found < maximum)
			labels[found] = ae_keys_action(entry)->label;
		found++;
	}
	return found;
}

static void keys_read(void)
{
	short action;

	memset(&keys.now, 0, sizeof(keys.now));
	for (action = 0; action < ae_keys_action_count(); action++)
	{
		char text[TEXT_SIZE];

		if (!config_text(ae_keys_action(action)->key, text, sizeof(text)))
			text[0] = 0;
		ae_keys_parse(text, keys.now.names[action]);
	}
}

/* every action whose keys differ from config.toml's written there (in use at once: the bindings read it again);
0 when one could not be */
static int keys_write(void)
{
	short action;
	int ok = 1;

	for (action = 0; action < ae_keys_action_count(); action++)
	{
		char text[TEXT_SIZE], now[TEXT_SIZE], parsed[AE_KEYS_SLOTS][AE_KEYS_NAME];

		keys_joined(action, text, sizeof(text));
		if (!config_text(ae_keys_action(action)->key, now, sizeof(now)))
			now[0] = 0;
		ae_keys_parse(now, parsed);
		ae_keys_join((char const (*)[AE_KEYS_NAME])parsed, now, sizeof(now));
		if (!strcmp(text, now))
			continue;
		if (!config_write(ae_keys_action(action)->key, text))
		{
			platform_log("ae keys: %s not written", ae_keys_action(action)->key);
			ok = 0;
		}
		/* (as config.toml now has it: the bindings read the same) */
		else if (config_text(ae_keys_action(action)->key, now, sizeof(now)))
			platform_log("ae keys: %s = \"%s\"", ae_keys_action(action)->key, now);
	}
	return ok;
}

/* a binding as the caps show it: as the keyboard's layout labels the key, in capitals */
static void keys_shown(const char *binding, char *text, int size)
{
	char shown[64];

	halo_input_shown_name(binding, shown, sizeof(shown));
	upper(shown, text, size);
}

static void keys_refresh(void)
{
	short action;

	if (keys.list.list.focus >= 0 && keys.list.list.focus < keys.count &&
		ae_keys_entry_is_header(keys.entries[keys.list.list.focus]))
		ae_list_set_focus(&keys.list.list, ae_keys_row_step(keys.entries, keys.count, keys.list.list.focus, 1));
	action = keys_focus_action();
	if (action != keys.logged_focus)
	{
		keys.logged_focus = action;
		if (action >= 0)
			platform_log("ae keys: focus %s", ae_keys_action(action)->label);
	}
	if (keys.note[0] && action != keys.note_row)
		keys.note[0] = 0;
}

static void keys_skip_headers(short direction)
{
	short focus = keys.list.list.focus;

	if (focus < 0 || focus >= keys.count || !ae_keys_entry_is_header(keys.entries[focus]))
		return;
	ae_list_set_focus(&keys.list.list, ae_keys_row_step(keys.entries, keys.count, focus, direction));
	if (ae_keys_entry_is_header(keys.entries[keys.list.list.focus]))
		ae_list_set_focus(&keys.list.list, ae_keys_row_step(keys.entries, keys.count, keys.list.list.focus,
			(short)-direction));
}

/* the focus on a place, its section's header at the list's top (as far as the list goes) */
static void keys_show_place(short place)
{
	struct ae_list *list = &keys.list.list;
	short last_first = list->count > list->rows ? (short)(list->count - list->rows) : 0;

	ae_list_set_focus(list, place);
	list->first = (short)(place - 1 < last_first ? place - 1 : last_first);
	if (list->first < 0)
		list->first = 0;
}

static void keys_section_go(short section)
{
	short place = ae_keys_section_place(keys.entries, keys.count, section);

	if (place < 0)
		return;
	keys_show_place((short)(place + 1));
	platform_log("ae keys: section %s", ae_keys_section_name(section));
	ae_sound_request(AE_SOUND_CURSOR, 0);
	keys_refresh();
}

static void keys_section_jump(short direction)
{
	short at = ae_keys_section_at(keys.entries, keys.count, keys.list.list.focus), next;

	for (next = (short)(at + direction); next >= 0 && next < AE_KEYS_SECTIONS; next = (short)(next + direction))
	{
		if (ae_keys_section_place(keys.entries, keys.count, next) >= 0)
		{
			keys_section_go(next);
			return;
		}
	}
	ae_sound_request(AE_SOUND_FAILURE, 0);
}

/* ---------- KEY BINDINGS: changing a key */

static void keys_capture_begin(void)
{
	short action = keys_focus_action();

	if (action < 0)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	keys.capturing = 1;
	keys.capture_action = action;
	keys.capture_slot = keys.slot;
	keys.note[0] = 0;
	platform_binding_capture_begin();
	platform_log("ae keys: press a key for %s %d", ae_keys_action(action)->label, keys.slot + 1);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* a key put on an action's cap (empty: none there); another action that had it loses it, and the note says so */
static void keys_set(short action, short slot, const char *name)
{
	short from = -1;
	char shown[64];

	if (!name[0] && !keys.now.names[action][slot][0])
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_keys_bind(&keys.now, action, slot, name, &from);
	if (!keys_write())
	{
		/* (not written: the keys as config.toml has them) */
		keys_read();
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	keys.note[0] = 0;
	if (!name[0])
		platform_log("ae keys: cleared %s %d", ae_keys_action(action)->label, slot + 1);
	else if (from >= 0)
	{
		keys_shown(name, shown, sizeof(shown));
		ae_keys_moved_text(shown, ae_keys_action(from)->label, !ae_keys_has_any(&keys.now, from), keys.note,
			sizeof(keys.note));
		keys.note_row = action;
		platform_log("ae keys: set %s %d %s (moved from %s)", ae_keys_action(action)->label, slot + 1, name,
			ae_keys_action(from)->label);
	}
	else
		platform_log("ae keys: set %s %d %s", ae_keys_action(action)->label, slot + 1, name);
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void keys_capture_end(int result, int input)
{
	char name[64];

	keys.capturing = 0;
	if (result == 3)
	{
		platform_log("ae keys: cancelled");
		ae_sound_request(AE_SOUND_BACK, 0);
		return;
	}
	name[0] = 0;
	if (result == 1)
	{
		halo_input_name(input, name, sizeof(name));
		/* (Backspace leaves no key there, as Delete does: neither is a key an action wants) */
		if (!strcmp(name, "Backspace"))
			name[0] = 0;
		else if (!name[0])
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
	}
	keys_set(keys.capture_action, keys.capture_slot, name);
}

static void keys_update(struct ae_screen *instance)
{
	int input = -1, result;

	(void)instance;
	if (!keys.capturing)
		return;
	result = platform_binding_capture_poll(&input);
	if (result)
		keys_capture_end(result, input);
}

/* ---------- KEY BINDINGS: the dialogs */

static short keys_defaults_changes(short section)
{
	short place, found = 0;

	for (place = 0; place < keys.count; place++)
	{
		short action = keys.entries[place];
		char wanted[TEXT_SIZE], parsed[AE_KEYS_SLOTS][AE_KEYS_NAME], now[TEXT_SIZE];

		if (ae_keys_entry_is_header(action) || (section >= 0 && ae_keys_action(action)->section != section))
			continue;
		if (!config_default(ae_keys_action(action)->key, wanted, sizeof(wanted)))
			continue;
		ae_keys_parse(wanted, parsed);
		ae_keys_join((char const (*)[AE_KEYS_NAME])parsed, wanted, sizeof(wanted));
		keys_joined(action, now, sizeof(now));
		if (strcmp(now, wanted))
			found++;
	}
	return found;
}

/* a section's actions (every one's: -1) back to the game's keys; a key one of them takes back leaves any other
action that has it now */
static short keys_defaults(short section)
{
	short place, put = 0;

	put = keys_defaults_changes(section);
	for (place = 0; place < keys.count; place++)
	{
		short action = keys.entries[place], slot;
		char wanted[TEXT_SIZE], parsed[AE_KEYS_SLOTS][AE_KEYS_NAME];

		if (ae_keys_entry_is_header(action) || (section >= 0 && ae_keys_action(action)->section != section))
			continue;
		if (!config_default(ae_keys_action(action)->key, wanted, sizeof(wanted)))
			continue;
		ae_keys_parse(wanted, parsed);
		for (slot = 0; slot < AE_KEYS_SLOTS; slot++)
			ae_keys_bind(&keys.now, action, slot, "", NULL);
		for (slot = 0; slot < AE_KEYS_SLOTS; slot++)
			ae_keys_bind(&keys.now, action, slot, parsed[slot], NULL);
	}
	if (!keys_write())
	{
		keys_read();
		ae_sound_request(AE_SOUND_FAILURE, 0);
	}
	keys.note[0] = 0;
	return put;
}

static void keys_dialog_picked(short choice, void *context)
{
	int dialog = keys.dialog;

	(void)context;
	keys.dialog = 0;
	if (dialog == 1)
	{
		/* KEEP CHANGES (0), UNDO CHANGES (1), KEEP EDITING (2) */
		if (choice == 1)
		{
			short action, put = keys_changes(NULL, 0);

			for (action = 0; action < ae_keys_action_count(); action++)
				ae_keys_parse(keys.opened[action], keys.now.names[action]);
			keys_write();
			platform_log("ae keys: undo (%d actions)", put);
		}
		else if (choice == 0)
			platform_log("ae keys: keep");
		if (choice == 0 || choice == 1)
			ae_ui_pop();
	}
	else if (dialog == 2 && choice > 0)
	{
		/* CANCEL (0), RESTORE <SECTION> (1, when it has changes), RESTORE ALL (the last) */
		short section = choice == 1 && keys.restore_section >= 0 ? keys.restore_section : -1;
		short put = keys_defaults(section);

		platform_log("ae keys: defaults %s (%d actions)", section >= 0 ? ae_keys_section_name(section) : "ALL", put);
	}
}

static int keys_keep_ask(void)
{
	static const char *const choices[] = { "KEEP CHANGES", "UNDO CHANGES", "KEEP EDITING" };
	const char *labels[AE_KEYS_ENTRIES];
	short count = keys_changes(labels, AE_KEYS_ENTRIES);
	char body[400];

	if (!count)
		return 0;
	ae_keys_keep_body(labels, count, body, sizeof(body));
	platform_log("ae keys: keep asked (%d changed)", count);
	keys.dialog = 1;
	page_dialog(&keys.page, "Keep your new keys?", body, choices, 3, 0, 2, keys_dialog_picked);
	return 1;
}

static void keys_defaults_ask(void)
{
	static char section_choice[48];
	const char *choices[3];
	short section = ae_keys_section_at(keys.entries, keys.count, keys.list.list.focus);
	short section_changes = section >= 0 ? keys_defaults_changes(section) : 0, all = keys_defaults_changes(-1), count = 0;
	char body[400];

	ae_keys_restore_body(ae_keys_section_name(section), section_changes, all, body, sizeof(body));
	platform_log("ae keys: defaults asked (%s %d, all %d)", ae_keys_section_name(section), section_changes, all);
	if (!all)
	{
		static const char *const ok[] = { "OK" };

		keys.dialog = 3;
		page_dialog(&keys.page, "Restore the game's keys?", body, ok, 1, 0, 0, keys_dialog_picked);
		return;
	}
	choices[count++] = "CANCEL";
	keys.restore_section = -1;
	if (section_changes)
	{
		snprintf(section_choice, sizeof(section_choice), "RESTORE %s", ae_keys_section_name(section));
		choices[count++] = section_choice;
		keys.restore_section = section;
	}
	choices[count++] = "RESTORE ALL";
	keys.dialog = 2;
	page_dialog(&keys.page, "Restore the game's keys?", body, choices, count, 0, 0, keys_dialog_picked);
}

/* ---------- KEY BINDINGS: drawing */

/* a cap: its fill and text by the row's state (light on a row, navy on the selection bar, the bar's other cap an
outline, ice blue while it waits for a key), a dashed outline when it has no key */
static void keys_cap(float x, float center_y, float width, const char *binding, int on_bar, int focused,
	int waiting, short hit_index)
{
	struct page const *page = &keys.page;
	float height = pu(page, CAP_HEIGHT_U), size = pu(page, CAP_TEXT_U), corner = pu(page, CAP_CORNER_U);
	float top = center_y - height * 0.5f;
	unsigned int text_color = AE_COLOR_KEY_CAP_TEXT;
	char shown[64], fitted[64];
	const char *words = NULL;

	if (waiting)
	{
		ae_draw_rect(x, top, width, height, corner, AE_COLOR_ACCENT);
		words = "PRESS A KEY";
	}
	else if (!binding[0])
	{
		if (on_bar && focused)
			ae_draw_rect(x, top, width, height, corner, AE_COLOR_KEY_CAP_TEXT);
		else
			ae_widget_dashed(x, top, width, height, pu(page, 1.0f), pu(page, CAP_DASH_U),
				on_bar ? AE_COLOR_KEY_CAP_TEXT : AE_COLOR_MUTED);
	}
	else
	{
		if (on_bar && focused)
		{
			ae_draw_rect(x, top, width, height, corner, AE_COLOR_KEY_CAP_TEXT);
			text_color = AE_COLOR_TITLE;
		}
		else if (on_bar)
			ae_draw_outline(x, top, width, height, corner, pu(page, CAP_OUTLINE_U), AE_COLOR_KEY_CAP_TEXT);
		else
			ae_draw_rect(x, top, width, height, corner, AE_COLOR_KEY_CAP);
		keys_shown(binding, shown, sizeof(shown));
		words = shown;
	}
	if (words)
	{
		ae_fit_text(AE_FONT_ROW, size, words, width - 2.0f * pu(page, CAP_PAD_U), fitted, sizeof(fitted));
		ae_draw_text(AE_FONT_ROW, size, x + width * 0.5f, cap_top(AE_FONT_ROW, size, center_y), AE_ALIGN_CENTER,
			text_color, fitted);
	}
	ae_hit_add(x, top, width, height, HIT_CAP, AE_PART_KEY, hit_index);
}

static void keys_columns(float x, float width, float *first, float *second)
{
	struct page const *page = &keys.page;

	*second = x + width - pu(page, page->density.metrics->pad) - pu(page, CAP_WIDTH_U);
	*first = *second - pu(page, CAP_GAP_U) - pu(page, CAP_WIDTH_U);
}

static void keys_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct ae_density const *density = context;
	short entry = keys.entries[item], slot;
	float columns[AE_KEYS_SLOTS], cap_width = pu(&keys.page, CAP_WIDTH_U);
	struct ae_row row;

	keys_columns(x, width, &columns[0], &columns[1]);
	if (ae_keys_entry_is_header(entry))
	{
		float head = density->metrics->group_height * density->unit, size = pu(&keys.page, density->metrics->group);
		float text_y = y + height - pu(&keys.page, HEADER_INSET_U) - ae_draw_cap_height(AE_FONT_BODY, size);
		static const char *const names[AE_KEYS_SLOTS] = { "PRIMARY", "SECONDARY" };

		ae_widget_group(density, x, y + height - head, width, ae_keys_section_name((short)(entry & ~AE_KEYS_HEADER)));
		for (slot = 0; slot < AE_KEYS_SLOTS; slot++)
			ae_draw_text_tracked(AE_FONT_BODY, size, CRUMB_TRACKING, columns[slot] + cap_width * 0.5f, text_y,
				AE_ALIGN_CENTER, AE_COLOR_MUTED, names[slot]);
		return;
	}
	memset(&row, 0, sizeof(row));
	row.label = ae_keys_action(entry)->label;
	row.flags = flags;
	if (keys_changed(entry))
		row.flags |= AE_ROW_CHANGED;
	ae_widget_row(density, x, y, width, &row, HIT_LIST, item);
	/* (0.7.5, a vote's controller button: the D-pad has no picture in the glyph set, its words in muted text) */
	if (ae_keys_action_note(entry))
	{
		float size = pu(&keys.page, density->metrics->group);

		ae_draw_text(AE_FONT_BODY, size, columns[0] - pu(&keys.page, CAP_GAP_U) * 2.0f,
			cap_top(AE_FONT_BODY, size, y + height * 0.5f), AE_ALIGN_RIGHT, AE_COLOR_MUTED,
			ae_keys_action_note(entry));
	}
	for (slot = 0; slot < AE_KEYS_SLOTS; slot++)
	{
		int waiting = keys.capturing && keys.capture_action == entry && keys.capture_slot == slot;

		keys_cap(columns[slot], y + height * 0.5f, cap_width, keys.now.names[entry][slot],
			(flags & AE_ROW_ON_BAR) != 0, (flags & AE_ROW_FOCUSED) && slot == keys.slot, waiting,
			(short)(item * AE_KEYS_SLOTS + slot));
	}
}

static void keys_draw(struct ae_screen *instance)
{
	struct page_parts parts;
	short section, current, count = 0, action = keys_focus_action();
	char changed[32], help[200];
	const char *crumb[2];
	float y;

	(void)instance;
	page_density(&keys.page, "ae keys");
	if (!ae_ui_drawing_ghost())
		keys_refresh();
	count = keys_changes(NULL, 0);
	if (count)
		snprintf(changed, sizeof(changed), "%d CHANGED", count);
	else
		snprintf(changed, sizeof(changed), "NO CHANGES");
	crumb[0] = "THIS PC";
	crumb[1] = changed;
	page_header(&keys.page, "KEY BINDINGS", crumb, 2, count ? AE_COLOR_ACCENT : AE_COLOR_MUTED, -1);
	page_parts(&keys.page, &parts);
	/* the rail */
	current = ae_keys_section_at(keys.entries, keys.count, keys.list.list.focus);
	y = parts.top;
	for (section = 0; section < AE_KEYS_SECTIONS; section++)
	{
		short place, rows = ae_keys_section_rows(keys.entries, keys.count, section);
		int changes = 0;

		if (!rows)
			continue;
		for (place = 0; place < keys.count; place++)
		{
			short entry = keys.entries[place];

			if (!ae_keys_entry_is_header(entry) && ae_keys_action(entry)->section == section)
				changes |= keys_changed(entry);
		}
		page_rail_item(&keys.page, parts.left, y, parts.rail_width, ae_keys_section_name(section), rows,
			section == current, changes, section);
		y += pu(&keys.page, RAIL_PITCH_U);
	}
	ae_widget_list(&keys.page.density, &keys.list, parts.list_x, parts.top, parts.list_width, parts.list_height,
		keys_item, &keys.page.density, HIT_LIST);
	/* the help: waiting, how; after a move, what moved; else the action's own and the rule */
	if (keys.capturing)
	{
		snprintf(help, sizeof(help), "Press the key, mouse button or wheel you want for %s.",
			ae_keys_action(keys.capture_action)->label);
		page_help(&keys.page, &parts, help, AE_COLOR_TITLE, "Esc cancels. Backspace or Delete leaves no key there.");
	}
	else if (action >= 0 && keys.note[0])
		page_help(&keys.page, &parts, keys.note, AE_COLOR_ACCENT, ae_keys_action(action)->help);
	else if (action >= 0)
		page_help(&keys.page, &parts, ae_keys_action(action)->help, AE_COLOR_TEXT,
			"Two keys each: either one does it. A key another action has moves here.");
	/* the prompts */
	keys.prompt_count = 0;
	if (keys.capturing)
	{
		keys.prompts[keys.prompt_count++] = prompt_of(AE_BUTTON_B, "Cancel", NULL, AE_ACTION_BACK, 0);
		keys.prompts[keys.prompt_count++] = prompt_of(AE_BUTTON_X, "No key", "Backspace", AE_ACTION_X, 0);
	}
	else
	{
		keys.prompts[keys.prompt_count++] = prompt_of(AE_BUTTON_A, "Change key", NULL, AE_ACTION_ACCEPT, 0);
		keys.prompts[keys.prompt_count++] = prompt_of(AE_BUTTON_B, ae_string(AE_STR_BACK), NULL, AE_ACTION_BACK, 0);
		keys.prompts[keys.prompt_count++] = prompt_of(AE_BUTTON_LEFT_SHOULDER, "Section", NULL, AE_ACTION_TAB_PREVIOUS,
			1);
		keys.prompts[keys.prompt_count++] = prompt_of(AE_BUTTON_X, "No key", NULL, AE_ACTION_X, 0);
		keys.prompts[keys.prompt_count++] = prompt_of(AE_BUTTON_Y, "Restore defaults", NULL, AE_ACTION_Y, 0);
	}
	page_footer(&keys.page, keys.prompts, keys.prompt_count);
}

/* ---------- KEY BINDINGS: input */

static void keys_clear(void)
{
	short action = keys_focus_action();

	if (action < 0)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	keys_set(action, keys.slot, "");
}

static void keys_slot_step(short direction)
{
	short slot = ae_value_step(AE_KEYS_SLOTS, keys.slot, direction);

	/* (the two slots round on a fresh press, a held repeat stops at the end) */
	if (slot == keys.slot)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	keys.slot = slot;
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static int keys_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	/* (waiting for a key: the keyboard and mouse go to the capture; a pad's B cancels, its X leaves no key there) */
	if (keys.capturing)
	{
		if (event->action == AE_ACTION_BACK)
			keys_capture_end(3, -1);
		else if (event->action == AE_ACTION_X)
			keys_capture_end(2, -1);
		return 1;
	}
	keys_refresh();
	switch (event->action)
	{
	case AE_ACTION_BACK:
		/* (with changes: KEEP / UNDO first) */
		return keys_keep_ask();
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		keys_section_jump(event->action == AE_ACTION_TAB_NEXT ? 1 : -1);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		keys_slot_step(event->action == AE_ACTION_RIGHT ? 1 : -1);
		return 1;
	case AE_ACTION_ACCEPT:
		keys_capture_begin();
		return 1;
	case AE_ACTION_X:
		keys_clear();
		return 1;
	case AE_ACTION_Y:
		keys_defaults_ask();
		return 1;
	default:
		break;
	}
	{
		short direction = event->action == AE_ACTION_UP || event->action == AE_ACTION_PAGE_UP || event->action == AE_ACTION_END ? -1 : 1;

		if (ae_list_view_event(&keys.list, event))
		{
			keys_skip_headers(direction);
			keys_refresh();
		}
	}
	return 1;
}

static void keys_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	short item, before;

	/* (waiting: the mouse's buttons and wheel are the capture's) */
	if (keys.capturing)
		return;
	if (ae_prompts_pointer(keys.prompts, keys.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			if (!keys_keep_ask())
			{
				ae_sound_request(AE_SOUND_BACK, 0);
				ae_ui_pop();
			}
		}
		else
			keys_handle(instance, &event);
		return;
	}
	if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_RAIL)
	{
		keys_section_go(hit.index);
		return;
	}
	/* the list: the wheel scrolls, a hover only highlights the row under the pointer (never scrolls: a header
	hovered keeps the focus where it was), as SETTINGS' group screens */
	before = keys.list.list.focus;
	item = ae_list_view_pointer(&keys.list, pointer, HIT_LIST);
	/* a cap: hovered, its row is focused and it is the one left / right pick; clicked, it waits for a key (a right
	click is BACK in AE's menus: ae_input.c) */
	if ((pointer->moved || pointer->left_clicks) && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_CAP &&
		hit.part == AE_PART_KEY)
	{
		short place = (short)(hit.index / AE_KEYS_SLOTS), slot = (short)(hit.index % AE_KEYS_SLOTS);

		if (place >= 0 && place < keys.count && !ae_keys_entry_is_header(keys.entries[place]))
		{
			keys.list.list.focus = place;
			keys.list.hover = place;
			keys.slot = slot;
			keys_refresh();
			if (pointer->left_clicks)
				keys_capture_begin();
			return;
		}
	}
	if (keys.list.list.focus >= 0 && keys.list.list.focus < keys.count &&
		ae_keys_entry_is_header(keys.entries[keys.list.list.focus]))
	{
		if (item < 0 && before >= 0 && before < keys.count && !ae_keys_entry_is_header(keys.entries[before]))
			keys.list.list.focus = before;
		else
			keys_skip_headers(1);
	}
	keys_refresh();
}

static void keys_leave(struct ae_screen *instance)
{
	(void)instance;
	keys.capturing = 0;
	platform_log("ae keys: closed (%d changed)", keys_changes(NULL, 0));
}

static struct ae_screen_class const keys_class =
{
	.name = "key bindings", .leave = keys_leave, .handle = keys_handle, .draw = keys_draw, .pointer = keys_pointer,
	.update = keys_update,
};

int ae_screen_keys_open(const char *focus_key)
{
	short action, place, focus = -1;

	memset(&keys, 0, sizeof(keys));
	keys.page.logged_width = -1.0f;
	keys.logged_focus = -1;
	keys.note_row = -1;
	keys.restore_section = -1;
	keys.with = (config_boolean("chat.enabled") ? AE_KEYS_WITH_CHUPA_CHAT : 0) |
		(ae_dev_mode_on() ? AE_KEYS_WITH_DEV_MODE : 0);
	keys_read();
	for (action = 0; action < ae_keys_action_count(); action++)
		keys_joined(action, keys.opened[action], sizeof(keys.opened[action]));
	keys.count = ae_keys_entries(keys.with, keys.entries, AE_KEYS_ENTRIES);
	ae_list_view_init(&keys.list, keys.count, 12);
	action = ae_keys_action_of(focus_key);
	for (place = 0; place < keys.count && action >= 0; place++)
	{
		if (keys.entries[place] == action)
			focus = place;
	}
	if (focus < 0)
		focus = 1;
	keys_show_place(focus);
	keys.list.previous_focus = focus;
	if (!ae_ui_push(&keys_class, AE_OWNER_PARENT, &keys))
		return 0;
	platform_log("ae keys: open (focus %s)", ae_keys_action(keys.entries[focus])->label);
	return 1;
}

/* ---------- BACK PADDLES */

/* the Xbox button a paddle presses (ae_platform.c's back_button_target words), as shown */
static const char *const paddle_values[] = { "none", "a", "b", "x", "y", "white", "black", "left_stick", "right_stick",
	"back", "start", "up", "down", "left", "right" };
static const char *const paddle_texts[] = { "NONE", "A", "B", "X", "Y", "LB", "RB", "LEFT STICK", "RIGHT STICK",
	"BACK", "START", "D-PAD UP", "D-PAD DOWN", "D-PAD LEFT", "D-PAD RIGHT" };
enum { PADDLE_VALUES = sizeof(paddle_values) / sizeof(paddle_values[0]), PADDLES = 2 };
static const char *const paddle_keys[PADDLES] = { "input.back_left", "input.back_right" };
static const char *const paddle_labels[PADDLES] = { "LEFT PADDLE", "RIGHT PADDLE" };

static struct
{
	struct page page;
	short player;
	char player_name[48];
	struct ae_list_view list;
	char opened[PADDLES][32];
	struct ae_motion tick;
	short tick_row, tick_side;
	char tick_old[32];
	struct ae_rect focus_rect;
	struct ae_prompt prompts[PROMPTS];
	short prompt_count;
	int dialog;                          /* 0 none, 1 KEEP / UNDO, 2 RESTORE DEFAULTS, 3 already at defaults */
} paddles;

static short paddle_index(short row)
{
	char text[32];
	short index;

	if (!ae_player_setting_get(paddles.player, paddle_keys[row], text, sizeof(text)))
		text[0] = 0;
	for (index = 0; index < (short)PADDLE_VALUES; index++)
	{
		const char *a = text, *b = paddle_values[index];

		for (; *a && *b && (*a >= 'A' && *a <= 'Z' ? *a + 32 : *a) == *b; a++, b++)
			;
		if (!*a && !*b)
			return index;
	}
	return 0;
}

static short paddle_changes(void)
{
	short row, count = 0;

	for (row = 0; row < PADDLES; row++)
		count += strcmp(paddle_values[paddle_index(row)], paddles.opened[row]) != 0;
	return count;
}

static void paddle_set(short row, short index)
{
	if (!ae_player_setting_put(paddles.player, paddle_keys[row], paddle_values[index]))
	{
		platform_log("ae paddles: %s not set", paddle_labels[row]);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	platform_log("ae paddles: set %s %s", paddle_labels[row], paddle_texts[index]);
}

static void paddle_step(short row, short direction)
{
	short current = paddle_index(row), next = ae_value_step((short)PADDLE_VALUES, current, direction);

	if (next == current)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	snprintf(paddles.tick_old, sizeof(paddles.tick_old), "%s", paddle_texts[current]);
	paddles.tick_row = row;
	paddles.tick_side = direction;
	ae_motion_start(&paddles.tick, 0.0f, 1.0f, AE_MOTION_VALUE_TICK_MS);
	paddle_set(row, next);
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void paddle_picked(short index, void *context)
{
	short row = (short)(intptr_t)context;

	if (row >= 0 && row < PADDLES && index >= 0 && index < (short)PADDLE_VALUES)
		paddle_set(row, index);
	paddles.tick_row = -1;
}

static void paddle_picker(short row)
{
	struct ae_picker_spec spec;

	memset(&spec, 0, sizeof(spec));
	spec.values = paddle_texts;
	spec.count = (short)PADDLE_VALUES;
	spec.current = paddle_index(row);
	spec.row = paddles.focus_rect;
	spec.bounds = paddles.page.frame.rect;
	spec.density = paddles.page.density;
	spec.picked = paddle_picked;
	spec.title = paddle_labels[row];
	spec.context = (void *)(intptr_t)row;
	if (ae_picker_open(&spec, AE_OWNER_PARENT))
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void paddle_dialog_picked(short choice, void *context)
{
	int dialog = paddles.dialog;
	short row;

	(void)context;
	paddles.dialog = 0;
	if (dialog == 1)
	{
		if (choice == 1)
		{
			for (row = 0; row < PADDLES; row++)
				ae_player_setting_put(paddles.player, paddle_keys[row], paddles.opened[row]);
			platform_log("ae paddles: undo");
		}
		else if (choice == 0)
		{
			ae_player_settings_flush();
			platform_log("ae paddles: keep");
		}
		if (choice == 0 || choice == 1)
			ae_ui_pop();
	}
	else if (dialog == 2 && choice == 1)
	{
		for (row = 0; row < PADDLES; row++)
		{
			char wanted[32];

			if (ae_player_setting_default(paddle_keys[row], wanted, sizeof(wanted)))
				ae_player_setting_put(paddles.player, paddle_keys[row], wanted);
		}
		paddles.tick_row = -1;
		platform_log("ae paddles: defaults");
	}
}

static int paddle_keep_ask(void)
{
	static const char *const choices[] = { "KEEP CHANGES", "UNDO CHANGES", "KEEP EDITING" };
	char body[200];

	if (!paddle_changes())
		return 0;
	snprintf(body, sizeof(body), "BACK PADDLES changed for %s. They are in use already.", paddles.player_name);
	paddles.dialog = 1;
	page_dialog(&paddles.page, "Keep your changes?", body, choices, 3, 0, 2, paddle_dialog_picked);
	return 1;
}

static void paddle_defaults_ask(void)
{
	static const char *const choices[] = { "CANCEL", "RESTORE DEFAULTS" };
	static const char *const ok[] = { "OK" };
	short row, changes = 0;
	char body[200];

	for (row = 0; row < PADDLES; row++)
		changes += paddle_index(row) != 0;
	if (!changes)
	{
		paddles.dialog = 3;
		page_dialog(&paddles.page, "Restore BACK PADDLES defaults?", "BACK PADDLES is already at its defaults.", ok, 1,
			0, 0, paddle_dialog_picked);
		return;
	}
	snprintf(body, sizeof(body), "Both paddles for %s go back to NONE. You can still undo it as you leave.",
		paddles.player_name);
	paddles.dialog = 2;
	page_dialog(&paddles.page, "Restore BACK PADDLES defaults?", body, choices, 2, 0, 0, paddle_dialog_picked);
}

static void paddle_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct ae_density const *density = context;
	struct ae_row widget;
	short index;

	if (item == 0)
	{
		ae_widget_group(density, x, y + height - density->metrics->group_height * density->unit, width, "PADDLES");
		return;
	}
	index = paddle_index((short)(item - 1));
	memset(&widget, 0, sizeof(widget));
	widget.label = paddle_labels[item - 1];
	widget.value = paddle_texts[index];
	widget.flags = flags;
	if (strcmp(paddle_values[index], paddles.opened[item - 1]))
		widget.flags |= AE_ROW_CHANGED;
	if (paddles.tick_row == item - 1)
	{
		widget.tick = &paddles.tick;
		widget.tick_side = paddles.tick_side;
		widget.old_value = paddles.tick_old;
	}
	if (flags & AE_ROW_FOCUSED)
	{
		paddles.focus_rect.x = x;
		paddles.focus_rect.y = y;
		paddles.focus_rect.width = width;
		paddles.focus_rect.height = height;
	}
	ae_widget_row(density, x, y, width, &widget, HIT_LIST, item);
}

static void paddles_draw(struct ae_screen *instance)
{
	struct page_parts parts;
	short count, focus;
	char changed[32], name[48], player[16];
	const char *crumb[3];

	(void)instance;
	page_density(&paddles.page, "ae paddles");
	if (paddles.list.list.focus < 1)
		ae_list_set_focus(&paddles.list.list, 1);
	count = paddle_changes();
	if (count)
		snprintf(changed, sizeof(changed), "%d CHANGED", count);
	else
		snprintf(changed, sizeof(changed), "NO CHANGES");
	upper(paddles.player_name, name, sizeof(name));
	snprintf(player, sizeof(player), "PLAYER %d", paddles.player + 1);
	crumb[0] = name;
	crumb[1] = player;
	crumb[2] = changed;
	page_header(&paddles.page, "BACK PADDLES", crumb, 3, count ? AE_COLOR_ACCENT : AE_COLOR_MUTED, paddles.player);
	page_parts(&paddles.page, &parts);
	page_rail_item(&paddles.page, parts.left, parts.top, parts.rail_width, "PADDLES", PADDLES, 1, count != 0, 0);
	ae_widget_list(&paddles.page.density, &paddles.list, parts.list_x, parts.top, parts.list_width, parts.list_height,
		paddle_item, &paddles.page.density, HIT_LIST);
	focus = paddles.list.list.focus;
	page_help(&paddles.page, &parts, focus == 1 ?
		"What the left back paddle presses: GL on a Switch 2 Pro, the upper left on an Elite or DualSense Edge." :
		"What the right back paddle presses: GR on a Switch 2 Pro, the upper right on an Elite or DualSense Edge.",
		AE_COLOR_TEXT, "It presses that button as your BUTTON LAYOUT has it.");
	paddles.prompt_count = 0;
	paddles.prompts[paddles.prompt_count++] = prompt_of(AE_BUTTON_A, ae_string(AE_STR_ALL_VALUES), NULL,
		AE_ACTION_ACCEPT, 0);
	paddles.prompts[paddles.prompt_count++] = prompt_of(AE_BUTTON_B, ae_string(AE_STR_BACK), NULL, AE_ACTION_BACK, 0);
	paddles.prompts[paddles.prompt_count++] = prompt_of(AE_BUTTON_Y, "Restore defaults", NULL, AE_ACTION_Y, 0);
	page_footer(&paddles.page, paddles.prompts, paddles.prompt_count);
}

static int paddles_handle(struct ae_screen *instance, struct ae_event const *event)
{
	short row = (short)(paddles.list.list.focus - 1);

	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return paddle_keep_ask();
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (row >= 0 && row < PADDLES)
			paddle_step(row, event->action == AE_ACTION_RIGHT ? 1 : -1);
		return 1;
	case AE_ACTION_ACCEPT:
		if (row >= 0 && row < PADDLES)
			paddle_picker(row);
		return 1;
	case AE_ACTION_Y:
		paddle_defaults_ask();
		return 1;
	default:
		break;
	}
	if (ae_list_view_event(&paddles.list, event) && paddles.list.list.focus < 1)
		ae_list_set_focus(&paddles.list.list, 1);
	return 1;
}

static void paddles_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	short item;

	if (ae_prompts_pointer(paddles.prompts, paddles.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			if (!paddle_keep_ask())
			{
				ae_sound_request(AE_SOUND_BACK, 0);
				ae_ui_pop();
			}
		}
		else
			paddles_handle(instance, &event);
		return;
	}
	if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_LIST && hit.index >= 1 &&
		hit.index <= PADDLES && (hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT))
	{
		ae_list_set_focus(&paddles.list.list, hit.index);
		paddle_step((short)(hit.index - 1), hit.part == AE_PART_ARROW_RIGHT ? 1 : -1);
		return;
	}
	item = ae_list_view_pointer(&paddles.list, pointer, HIT_LIST);
	if (paddles.list.list.focus < 1)
		ae_list_set_focus(&paddles.list.list, 1);
	if (item >= 1 && item <= PADDLES)
		paddle_picker((short)(item - 1));
}

static void paddles_leave(struct ae_screen *instance)
{
	(void)instance;
	ae_player_settings_flush();
	platform_log("ae paddles: closed (%d changed)", paddle_changes());
}

static struct ae_screen_class const paddles_class =
{
	.name = "back paddles", .leave = paddles_leave, .handle = paddles_handle, .draw = paddles_draw,
	.pointer = paddles_pointer,
};

int ae_screen_paddles_open(short local_player)
{
	struct player_profile profile;
	short row;
	int at;

	memset(&paddles, 0, sizeof(paddles));
	paddles.page.logged_width = -1.0f;
	paddles.tick_row = -1;
	paddles.player = local_player >= 0 && local_player < PLAYERS ? local_player : 0;
	memset(&profile, 0, sizeof(profile));
	player_ui_get_active_player_profile(paddles.player, &profile);
	for (at = 0; at < (int)sizeof(paddles.player_name) - 1 && at < MAXIMUM_PLAYER_PROFILE_NAME_LENGTH &&
		profile.player_name[at]; at++)
		paddles.player_name[at] = profile.player_name[at] >= 0x20 && profile.player_name[at] < 0x7F ?
			(char)profile.player_name[at] : '?';
	paddles.player_name[at] = 0;
	if (!paddles.player_name[0])
		snprintf(paddles.player_name, sizeof(paddles.player_name), "Player %d", paddles.player + 1);
	for (row = 0; row < PADDLES; row++)
		snprintf(paddles.opened[row], sizeof(paddles.opened[row]), "%s", paddle_values[paddle_index(row)]);
	ae_list_view_init(&paddles.list, 1 + PADDLES, 12);
	ae_list_set_focus(&paddles.list.list, 1);
	paddles.list.previous_focus = 1;
	if (!ae_ui_push(&paddles_class, AE_OWNER_PARENT, &paddles))
		return 0;
	platform_log("ae paddles: open player %d (%s, %s)", paddles.player + 1, paddles.opened[0], paddles.opened[1]);
	return 1;
}

/* ---------- CUSTOM LAYOUT */

/* the rows' marks and the wait's pill, in the value column (KEY BINDINGS' second cap's place); the moved note; the
edge's pulse, menu blue to focus blue and back; the rest of the wait, a bar on the row's foot */
#define CUSTOM_MARK_U 30.0f
#define CUSTOM_GLYPH_U 44.0f
#define CUSTOM_PILL_EDGE_U 3.0f
#define CUSTOM_BAR_U 3.0f
#define CUSTOM_NEAR_U 16.0f
#define CUSTOM_NOTE_U 18.0f
#define CUSTOM_NOTE_GAP_U 8.0f
#define CUSTOM_FIXED_ABOVE_U 24.0f
#define CUSTOM_FIXED_PITCH_U 44.0f
#define CUSTOM_FIXED_GAP_U 12.0f
#define CUSTOM_COLOR_PILL 0x000E23FFu
#define CUSTOM_COLOR_PULSE_LOW 0x2894FEFFu
#define CUSTOM_COLOR_PULSE_HIGH 0xB8D6FFFFu

enum
{
	CUSTOM_ROWS = AE_CUSTOM_LAYOUT_ROWS,
	CUSTOM_ITEM_RESET = CUSTOM_ROWS + 1,
	CUSTOM_ITEMS = CUSTOM_ROWS + 2,
	CUSTOM_PULSE_MS = 900,
	CUSTOM_NOTICE_MS = 3000,
	CUSTOM_PADS = 4,
	CUSTOM_DIALOG_KEEP = 1, CUSTOM_DIALOG_RESET, CUSTOM_DIALOG_NOTHING
};

typedef char custom_text_fits_a_player_setting[AE_PLAYER_SETTINGS_VALUE >= AE_CUSTOM_LAYOUT_TEXT ? 1 : -1];

extern unsigned char pc_menu_split_players(void);
int platform_gamepad_describe(int index, char *name, size_t size);
int platform_gamepad_buttons(int port, unsigned long *buttons);

static const char *const custom_key = "controls.controller_layout";

static struct
{
	struct page page;
	short player;
	char player_name[48];
	struct ae_list_view list;
	char opened[AE_CUSTOM_LAYOUT_TEXT];
	short opened_preset;
	unsigned char opened_layout[CONTROLLER_ACTION_COUNT];
	/* what the last bind did; what the wait told, for a moment */
	char said[200];
	char notice[120];
	unsigned long notice_ms;
	/* the row the last bind moved, and the button it was on */
	short moved_row;
	int moved_button;
	int kind;
	int waiting, wait_row;
	struct ae_custom_layout_wait wait;
	/* a bind or cancel made: the pad's press is no menu press until it is let go */
	int settling;
	struct ae_prompt prompts[PROMPTS];
	short prompt_count;
	int dialog;
	short reset_row;
	char reset_choice[48];
} custom;

static void custom_layout(short player, unsigned char layout[CONTROLLER_ACTION_COUNT])
{
	char text[AE_CUSTOM_LAYOUT_TEXT];

	if (!ae_player_setting_get(player, custom_key, text, sizeof(text)))
		text[0] = 0;
	controller_layout_preset(CONTROLLER_PRESET_CUSTOM, text, layout);
}

void ae_screen_custom_layout_summary(short local_player, char *text, int size)
{
	unsigned char layout[CONTROLLER_ACTION_COUNT];
	int count;

	custom_layout(local_player, layout);
	count = ae_custom_layout_changes(layout);
	if (count)
		snprintf(text, (size_t)size, "%d CHANGED", count);
	else
		snprintf(text, (size_t)size, "NO CHANGES");
}

/* whether a controller drives the page's player: a lone player's menus take any controller, else each keeps to its
own (ae_input_rules.h) */
static int custom_pad_is_ours(short controller)
{
	short bindings[AE_MAXIMUM_PLAYERS], player;

	if (!pc_menu_split_players())
		return 1;
	for (player = 0; player < AE_MAXIMUM_PLAYERS; player++)
		bindings[player] = player_ui_get_single_player_local_player_controller(player);
	return ae_input_player_of_controller(controller, bindings) == custom.player;
}

/* the buttons down on the player's pads (a bit for each CONTROLLER_BUTTON_*), whether there is one */
static unsigned long custom_pad_buttons(int *has_pad)
{
	unsigned long buttons = 0, down;
	short controller;

	*has_pad = 0;
	for (controller = 0; controller < CUSTOM_PADS; controller++)
	{
		if (custom_pad_is_ours(controller) && platform_gamepad_buttons(controller, &down))
		{
			*has_pad = 1;
			buttons |= down;
		}
	}
	return buttons;
}

/* the kind of the player's pad (controller_layout.h), for a lone player the one last used; -1 none */
static int custom_pad_kind(void)
{
	unsigned long down;
	short controller, connected = 0;

	if (!pc_menu_split_players())
		return platform_gamepad_describe(-1, NULL, 0);
	for (controller = 0; controller < CUSTOM_PADS; controller++)
	{
		if (!platform_gamepad_buttons(controller, &down))
			continue;
		if (custom_pad_is_ours(controller))
			return platform_gamepad_describe(connected, NULL, 0);
		connected++;
	}
	return -1;
}

/* a pad's kind as the menus' device for its marks: a pad's always, never the keyboard's */
static int custom_device(int kind)
{
	switch (kind)
	{
	case CONTROLLER_KIND_DUALSENSE:
	case CONTROLLER_KIND_DUALSHOCK:
		return AE_DEVICE_PLAYSTATION;
	case CONTROLLER_KIND_SWITCH_PRO:
	case CONTROLLER_KIND_WII_U_PRO:
		return AE_DEVICE_NINTENDO;
	default:
		return AE_DEVICE_XBOX;
	}
}

/* a CONTROLLER_BUTTON_* as a mark's button */
static int custom_mark_button(int button)
{
	switch (button)
	{
	case CONTROLLER_BUTTON_A: return AE_BUTTON_A;
	case CONTROLLER_BUTTON_B: return AE_BUTTON_B;
	case CONTROLLER_BUTTON_X: return AE_BUTTON_X;
	case CONTROLLER_BUTTON_Y: return AE_BUTTON_Y;
	case CONTROLLER_BUTTON_WHITE: return AE_BUTTON_LEFT_SHOULDER;
	case CONTROLLER_BUTTON_BLACK: return AE_BUTTON_RIGHT_SHOULDER;
	case CONTROLLER_BUTTON_LEFT_TRIGGER: return AE_BUTTON_LEFT_TRIGGER;
	case CONTROLLER_BUTTON_RIGHT_TRIGGER: return AE_BUTTON_RIGHT_TRIGGER;
	case CONTROLLER_BUTTON_LEFT_STICK: return AE_MARK_LEFT_STICK;
	default: return AE_MARK_RIGHT_STICK;
	}
}

/* a mark's height: a face button's glyph has air in its square, a drawn cap none */
static float custom_mark_height(struct page const *page, int button)
{
	return pu(page, button == CONTROLLER_BUTTON_A || button == CONTROLLER_BUTTON_B || button == CONTROLLER_BUTTON_X ||
		button == CONTROLLER_BUTTON_Y ? CUSTOM_GLYPH_U : CUSTOM_MARK_U);
}

static void custom_action_name(int row, char *text, int size)
{
	upper(controller_layout_action_title(ae_custom_layout_actions[row]), text, size);
}

static void custom_button_name(int button, char *text, int size)
{
	upper(controller_layout_button_name(custom.kind, button), text, size);
}

/* the profile plays CUSTOM (once, at the first change), and every local player on CUSTOM takes the new layout */
static void custom_apply(void)
{
	struct ae_profile_controls controls;

	if (ae_profile_get_controls(custom.player, &controls).ok && controls.layout != _button_preset_custom)
	{
		controls.layout = _button_preset_custom;
		if (ae_profile_set_controls(custom.player, &controls).ok)
			platform_log("ae custom layout: player %d BUTTON LAYOUT now CUSTOM", custom.player + 1);
	}
	player_ui_custom_layout_changed();
}

static int custom_write(unsigned char const layout[CONTROLLER_ACTION_COUNT])
{
	char text[AE_CUSTOM_LAYOUT_TEXT];

	controller_layout_format(layout, text, sizeof(text));
	if (!ae_player_setting_put(custom.player, custom_key, text))
	{
		platform_log("ae custom layout: not set (%s)", text);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return 0;
	}
	platform_log("ae custom layout: player %d \"%s\"", custom.player + 1, text);
	custom_apply();
	return 1;
}

static short custom_row_of_action(int action)
{
	short row;

	for (row = 0; row < CUSTOM_ROWS; row++)
	{
		if (ae_custom_layout_actions[row] == action)
			return row;
	}
	return -1;
}

/* the press taken: the action on its button takes the action's old one */
static void custom_bind(short row, int button)
{
	unsigned char layout[CONTROLLER_ACTION_COUNT];
	char action[48], moved_name[48], name[32], other[32];
	int moved;

	custom_layout(custom.player, layout);
	moved = controller_layout_assign(layout, ae_custom_layout_actions[row], button);
	if (!custom_write(layout))
		return;
	custom_action_name(row, action, sizeof(action));
	custom_button_name(button, name, sizeof(name));
	custom.moved_row = -1;
	if (moved >= 0 && moved != ae_custom_layout_actions[row])
	{
		custom.moved_row = custom_row_of_action(moved);
		custom.moved_button = button;
		upper(controller_layout_action_title(moved), moved_name, sizeof(moved_name));
		custom_button_name(layout[moved], other, sizeof(other));
		snprintf(custom.said, sizeof(custom.said), "%s is on %s now. %s moved to %s.", action, name, moved_name, other);
		platform_log("ae custom layout: set %s %s (moved %s to %s)", action, name, moved_name, other);
	}
	else
	{
		snprintf(custom.said, sizeof(custom.said), "%s is on %s now.", action, name);
		platform_log("ae custom layout: set %s %s", action, name);
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void custom_tell(const char *text)
{
	snprintf(custom.notice, sizeof(custom.notice), "%s", text);
	custom.notice_ms = ae_motion_now();
}

static void custom_wait_end(const char *log_word, const char *told, int sound)
{
	custom.waiting = 0;
	custom.settling = 1;
	if (told)
		custom_tell(told);
	platform_log("ae custom layout: %s", log_word);
	ae_sound_request(sound, 0);
}

static void custom_wait_begin(short row)
{
	int has_pad = 0;

	custom_pad_buttons(&has_pad);
	if (!has_pad)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	custom.waiting = 1;
	custom.wait_row = row;
	custom.said[0] = 0;
	custom.notice[0] = 0;
	custom.moved_row = -1;
	ae_custom_layout_wait_begin(&custom.wait, ae_motion_now());
	platform_log("ae custom layout: press a button for %s", controller_layout_action_key(ae_custom_layout_actions[row]));
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void custom_update(struct ae_screen *instance)
{
	unsigned char layout[CONTROLLER_ACTION_COUNT];
	unsigned long buttons;
	int has_pad = 0, button = -1;
	char action[48], name[32];

	(void)instance;
	buttons = custom_pad_buttons(&has_pad);
	if (custom.settling && !buttons)
		custom.settling = 0;
	if (!custom.waiting)
		return;
	custom_layout(custom.player, layout);
	switch (ae_custom_layout_wait_step(&custom.wait, has_pad, buttons, layout[ae_custom_layout_actions[custom.wait_row]],
		ae_motion_now(), &button))
	{
	case AE_CUSTOM_LAYOUT_BOUND:
		custom.waiting = 0;
		custom.settling = 1;
		custom_bind((short)custom.wait_row, button);
		break;
	case AE_CUSTOM_LAYOUT_SAME:
		custom_action_name(custom.wait_row, action, sizeof(action));
		custom_button_name(button, name, sizeof(name));
		snprintf(custom.said, sizeof(custom.said), "%s stays on %s.", action, name);
		custom_wait_end("same button", NULL, AE_SOUND_BACK);
		break;
	case AE_CUSTOM_LAYOUT_CANCELLED:
		custom_wait_end("cancelled", NULL, AE_SOUND_BACK);
		break;
	case AE_CUSTOM_LAYOUT_TIMED_OUT:
		custom_wait_end("timed out", "Nothing changed.", AE_SOUND_BACK);
		break;
	case AE_CUSTOM_LAYOUT_UNPLUGGED:
		custom_wait_end("unplugged", "The controller was unplugged. Nothing changed.", AE_SOUND_FAILURE);
		break;
	case AE_CUSTOM_LAYOUT_DPAD:
		custom_tell("The d-pad can't take an action. Press another button.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		break;
	default:
		break;
	}
}

static void custom_reset(short row)
{
	unsigned char layout[CONTROLLER_ACTION_COUNT], fresh[CONTROLLER_ACTION_COUNT];

	controller_layout_preset(CONTROLLER_PRESET_DEFAULT, NULL, fresh);
	if (row < 0)
		memcpy(layout, fresh, sizeof(layout));
	else
	{
		custom_layout(custom.player, layout);
		controller_layout_assign(layout, ae_custom_layout_actions[row], fresh[ae_custom_layout_actions[row]]);
	}
	if (!custom_write(layout))
		return;
	custom.moved_row = -1;
	custom.notice[0] = 0;
	if (row < 0)
		snprintf(custom.said, sizeof(custom.said), "Every action is back on its DEFAULT button.");
	else
	{
		char action[48];

		custom_action_name(row, action, sizeof(action));
		snprintf(custom.said, sizeof(custom.said), "%s is back on its DEFAULT button.", action);
	}
	platform_log("ae custom layout: reset %d", (int)row);
}

static void custom_dialog_picked(short choice, void *context)
{
	int dialog = custom.dialog;

	(void)context;
	custom.dialog = 0;
	if (dialog == CUSTOM_DIALOG_KEEP)
	{
		if (choice == 1)
		{
			struct ae_profile_controls controls;

			ae_player_setting_put(custom.player, custom_key, custom.opened[0] ? custom.opened : NULL);
			if (ae_profile_get_controls(custom.player, &controls).ok && controls.layout != custom.opened_preset)
			{
				controls.layout = custom.opened_preset;
				ae_profile_set_controls(custom.player, &controls);
			}
			player_ui_custom_layout_changed();
			platform_log("ae custom layout: undo");
		}
		else if (choice == 0)
			platform_log("ae custom layout: keep");
		if (choice == 0 || choice == 1)
			ae_ui_pop();
	}
	else if (dialog == CUSTOM_DIALOG_RESET)
	{
		if (choice == 1)
			custom_reset(-1);
		else if (choice == 2)
			custom_reset(custom.reset_row);
	}
}

static int custom_keep_ask(void)
{
	static const char *const choices[] = { "KEEP CHANGES", "UNDO CHANGES", "KEEP EDITING" };
	unsigned char layout[CONTROLLER_ACTION_COUNT];
	char body[200];

	custom_layout(custom.player, layout);
	if (!memcmp(layout, custom.opened_layout, sizeof(layout)))
		return 0;
	snprintf(body, sizeof(body), "CUSTOM LAYOUT changed for %s. It is in use already.", custom.player_name);
	custom.dialog = CUSTOM_DIALOG_KEEP;
	page_dialog(&custom.page, "Keep your changes?", body, choices, 3, 0, 2, custom_dialog_picked);
	return 1;
}

/* row: the action Y was pressed on, which may go back alone (-1: the RESET row) */
static void custom_reset_ask(short row)
{
	static const char *const nothing[] = { "OK" };
	const char *choices[3];
	unsigned char layout[CONTROLLER_ACTION_COUNT], fresh[CONTROLLER_ACTION_COUNT];
	char body[200], action[48];
	short count = 2;

	custom_layout(custom.player, layout);
	if (!ae_custom_layout_changes(layout))
	{
		custom.dialog = CUSTOM_DIALOG_NOTHING;
		page_dialog(&custom.page, "Reset CUSTOM LAYOUT?", "CUSTOM LAYOUT is already the DEFAULT layout.", nothing, 1, 0, 0,
			custom_dialog_picked);
		return;
	}
	controller_layout_preset(CONTROLLER_PRESET_DEFAULT, NULL, fresh);
	choices[0] = "CANCEL";
	choices[1] = "RESET ALL";
	custom.reset_row = row;
	if (row >= 0 && layout[ae_custom_layout_actions[row]] != fresh[ae_custom_layout_actions[row]])
	{
		custom_action_name(row, action, sizeof(action));
		snprintf(custom.reset_choice, sizeof(custom.reset_choice), "RESET %s", action);
		choices[2] = custom.reset_choice;
		count = 3;
	}
	snprintf(body, sizeof(body), "Every action for %s goes back to its DEFAULT button. You can still undo it as you "
		"leave.", custom.player_name);
	custom.dialog = CUSTOM_DIALOG_RESET;
	page_dialog(&custom.page, "Reset CUSTOM LAYOUT?", body, choices, count, 0, 0, custom_dialog_picked);
}

/* the wait in its row: the seconds left, the pill with its pulsing edge, the bar running out on the row's foot */
static void custom_wait_draw(struct page const *page, float x, float y, float width, float height, float column)
{
	unsigned long now = ae_motion_now(), gone = now - custom.wait.started_ms;
	float pill = pu(page, CAP_WIDTH_U), tall = pu(page, CAP_HEIGHT_U), corner = pu(page, CAP_CORNER_U);
	float middle = y + height * 0.5f, size = pu(page, CAP_TEXT_U), text = ae_size_text(&page->density);
	float phase = 0.5f - 0.5f * cosf(6.2831853f * (float)(now % CUSTOM_PULSE_MS) / (float)CUSTOM_PULSE_MS);
	float left = 1.0f - (float)gone / (float)AE_CUSTOM_LAYOUT_WAIT_MS;
	unsigned int edge = 0xFFu;
	int shift;
	char seconds[8], fitted[32];

	for (shift = 24; shift >= 8; shift -= 8)
	{
		float low = (float)((CUSTOM_COLOR_PULSE_LOW >> shift) & 0xFFu);
		float high = (float)((CUSTOM_COLOR_PULSE_HIGH >> shift) & 0xFFu);

		edge |= (unsigned int)(low + (high - low) * phase) << shift;
	}
	ae_draw_rect(column - pill * 0.5f, middle - tall * 0.5f, pill, tall, corner, CUSTOM_COLOR_PILL);
	ae_draw_outline(column - pill * 0.5f, middle - tall * 0.5f, pill, tall, corner, pu(page, CUSTOM_PILL_EDGE_U), edge);
	ae_fit_text(AE_FONT_ROW, size, "PRESS A BUTTON...", pill - 2.0f * pu(page, CAP_PAD_U), fitted, sizeof(fitted));
	ae_draw_text(AE_FONT_ROW, size, column, cap_top(AE_FONT_ROW, size, middle), AE_ALIGN_CENTER, AE_COLOR_TITLE, fitted);
	snprintf(seconds, sizeof(seconds), "%d S", ae_custom_layout_wait_seconds(&custom.wait, now));
	ae_draw_text(AE_FONT_ROW, text, column - pill * 0.5f - pu(page, CUSTOM_NEAR_U), cap_top(AE_FONT_ROW, text, middle),
		AE_ALIGN_RIGHT, AE_COLOR_SELECTION_TEXT, seconds);
	if (left > 0.0f)
		ae_draw_rect(x, y + height - pu(page, CUSTOM_BAR_U), width * left, pu(page, CUSTOM_BAR_U), 0.0f,
			CUSTOM_COLOR_PULSE_LOW);
}

static void custom_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct ae_density const *density = context;
	struct page const *page = &custom.page;
	float column = x + width - pu(page, density->metrics->pad) - pu(page, CAP_WIDTH_U) * 0.5f;
	float mark, middle = y + height * 0.5f;
	int kind = custom.kind, device = custom_device(kind), on_bar = (flags & AE_ROW_ON_BAR) != 0;
	unsigned char layout[CONTROLLER_ACTION_COUNT], fresh[CONTROLLER_ACTION_COUNT];
	struct ae_row widget;
	char label[48], summary[32];
	short row;
	int button;
	float width_of;

	if (item == 0)
	{
		float head = density->metrics->group_height * density->unit, size = pu(page, density->metrics->group);
		float text_y = y + height - pu(page, HEADER_INSET_U) - ae_draw_cap_height(AE_FONT_BODY, size);

		upper(kind >= 0 ? controller_layout_kind_title(kind) : "NO CONTROLLER", label, sizeof(label));
		ae_widget_group(density, x, y + height - head, width, label);
		ae_draw_text_tracked(AE_FONT_BODY, size, CRUMB_TRACKING, column, text_y, AE_ALIGN_CENTER, AE_COLOR_MUTED, "BUTTON");
		return;
	}
	memset(&widget, 0, sizeof(widget));
	widget.flags = flags;
	if (item == CUSTOM_ITEM_RESET)
	{
		ae_screen_custom_layout_summary(custom.player, summary, sizeof(summary));
		widget.label = "RESET";
		widget.value = summary;
		widget.flags |= AE_ROW_SUBSCREEN;
		ae_widget_row(density, x, y, width, &widget, HIT_LIST, item);
		return;
	}
	row = (short)(item - 1);
	custom_layout(custom.player, layout);
	controller_layout_preset(CONTROLLER_PRESET_DEFAULT, NULL, fresh);
	button = layout[ae_custom_layout_actions[row]];
	custom_action_name(row, label, sizeof(label));
	widget.label = label;
	if (button != fresh[ae_custom_layout_actions[row]])
		widget.flags |= AE_ROW_CHANGED;
	ae_widget_row(density, x, y, width, &widget, HIT_LIST, item);
	if (custom.waiting && custom.wait_row == row)
	{
		custom_wait_draw(page, x, y, width, height, column);
		return;
	}
	mark = custom_mark_height(page, button);
	width_of = ae_widget_pad_mark(density, device, custom_mark_button(button), 0.0f, middle, mark, on_bar, 0);
	ae_widget_pad_mark(density, device, custom_mark_button(button), column - width_of * 0.5f, middle, mark, on_bar, 1);
	if (custom.moved_row == row)
	{
		/* (MOVED FROM and the button it was on, before the column) */
		float size = pu(page, CUSTOM_NOTE_U), note = column - pu(page, CAP_WIDTH_U) * 0.5f;
		float from;

		mark = custom_mark_height(page, custom.moved_button);
		from = ae_widget_pad_mark(density, device, custom_mark_button(custom.moved_button), 0.0f, middle, mark, on_bar,
			0);

		ae_widget_pad_mark(density, device, custom_mark_button(custom.moved_button), note - from, middle, mark, on_bar, 1);
		ae_draw_text_tracked(AE_FONT_BODY, size, CRUMB_TRACKING, note - from - pu(page, CUSTOM_NOTE_GAP_U),
			cap_top(AE_FONT_BODY, size, middle), AE_ALIGN_RIGHT, on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT,
			"MOVED FROM");
	}
}

/* under the rail: what can't move */
static void custom_fixed_draw(struct page const *page, struct page_parts const *parts, int device)
{
	static struct { int button; const char *name; } const fixed[] = { { AE_BUTTON_START, "PAUSE" },
		{ AE_BUTTON_BACK, "SCOREBOARD" } };
	float x = parts->left + pu(page, RAIL_PAD_U), y = parts->top + pu(page, RAIL_PITCH_U) + pu(page, CUSTOM_FIXED_ABOVE_U);
	float size = pu(page, page->density.metrics->group), text = ae_size_text(&page->density);
	float mark = pu(page, CUSTOM_GLYPH_U);
	short index;

	ae_draw_text_tracked(AE_FONT_BODY, size, CRUMB_TRACKING, x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, "FIXED");
	y += pu(page, CUSTOM_FIXED_PITCH_U);
	for (index = 0; index < (short)(sizeof(fixed) / sizeof(fixed[0])); index++)
	{
		float width = ae_widget_pad_mark(&page->density, device, fixed[index].button, x, y, mark, 0, 1);

		ae_draw_text(AE_FONT_ROW, text, x + width + pu(page, CUSTOM_FIXED_GAP_U), cap_top(AE_FONT_ROW, text, y), AE_ALIGN_LEFT,
			AE_COLOR_MUTED, fixed[index].name);
		y += pu(page, CUSTOM_FIXED_PITCH_U);
	}
}

static void custom_draw(struct ae_screen *instance)
{
	struct page_parts parts;
	unsigned char layout[CONTROLLER_ACTION_COUNT];
	int count, kind, has_pad = 0;
	short focus;
	char changed[32], name[48], player[16], help[200], action[48], who[20], pad[48];
	const char *crumb[3], *second = "START and BACK stay where they are.";
	unsigned int first_color = AE_COLOR_TEXT;

	(void)instance;
	page_density(&custom.page, "ae custom layout");
	if (custom.list.list.focus < 1)
		ae_list_set_focus(&custom.list.list, 1);
	custom_layout(custom.player, layout);
	count = ae_custom_layout_changes(layout);
	kind = custom.kind = custom_pad_kind();
	custom_pad_buttons(&has_pad);
	if (custom.notice[0] && ae_motion_now() - custom.notice_ms >= CUSTOM_NOTICE_MS)
		custom.notice[0] = 0;
	ae_screen_custom_layout_summary(custom.player, changed, sizeof(changed));
	upper(custom.player_name, name, sizeof(name));
	snprintf(player, sizeof(player), "PLAYER %d", custom.player + 1);
	crumb[0] = name;
	crumb[1] = player;
	crumb[2] = changed;
	page_header(&custom.page, "CUSTOM LAYOUT", crumb, 3, count ? AE_COLOR_ACCENT : AE_COLOR_MUTED, custom.player);
	page_parts(&custom.page, &parts);
	page_rail_item(&custom.page, parts.left, parts.top, parts.rail_width, "BUTTONS", CUSTOM_ROWS, 1, count != 0, 0);
	custom_fixed_draw(&custom.page, &parts, custom_device(kind));
	ae_widget_list(&custom.page.density, &custom.list, parts.list_x, parts.top, parts.list_width, parts.list_height,
		custom_item, &custom.page.density, HIT_LIST);
	/* the help: waiting, how; else what just happened; else the rule */
	focus = custom.list.list.focus;
	who[0] = 0;
	if (pc_menu_split_players())
		snprintf(who, sizeof(who), "%s: ", player);
	if (custom.waiting)
	{
		custom_action_name(custom.wait_row, action, sizeof(action));
		upper(controller_layout_kind_title(kind), pad, sizeof(pad));
		snprintf(help, sizeof(help), "%sPress the button you want for %s on your %s.", who, action, pad);
		first_color = AE_COLOR_TITLE;
		second = "START or BACK cancels. After 8 seconds with no press, nothing changes.";
		if (custom.notice[0])
		{
			snprintf(help, sizeof(help), "%s", custom.notice);
			first_color = AE_COLOR_ACCENT;
		}
	}
	else if (!has_pad)
		snprintf(help, sizeof(help), "Plug in a controller for %s to change this.", player);
	else if (custom.notice[0] || custom.said[0])
	{
		snprintf(help, sizeof(help), "%s", custom.notice[0] ? custom.notice : custom.said);
		first_color = AE_COLOR_ACCENT;
	}
	else if (focus >= 1 && focus <= CUSTOM_ROWS)
	{
		custom_action_name(focus - 1, action, sizeof(action));
		snprintf(help, sizeof(help), "%sPress {A}, then the button you want for %s. The action on it takes %s's old one.",
			who, action, action);
	}
	else
		snprintf(help, sizeof(help), "Every action goes back to its DEFAULT button.");
	page_help(&custom.page, &parts, help, first_color, second);
	custom.prompt_count = 0;
	if (custom.waiting)
		custom.prompts[custom.prompt_count++] = prompt_of(AE_BUTTON_START, "Cancel", "Esc", AE_ACTION_BACK, 0);
	else
	{
		custom.prompts[custom.prompt_count++] = prompt_of(AE_BUTTON_A, focus == CUSTOM_ITEM_RESET ? "Reset" : "Change button",
			NULL, AE_ACTION_ACCEPT, 0);
		custom.prompts[custom.prompt_count++] = prompt_of(AE_BUTTON_B, ae_string(AE_STR_BACK), NULL, AE_ACTION_BACK, 0);
		custom.prompts[custom.prompt_count++] = prompt_of(AE_BUTTON_Y, "Restore defaults", NULL, AE_ACTION_Y, 0);
	}
	page_footer(&custom.page, custom.prompts, custom.prompt_count);
}

static void custom_accept(short item)
{
	if (item >= 1 && item <= CUSTOM_ROWS)
		custom_wait_begin((short)(item - 1));
	else if (item == CUSTOM_ITEM_RESET)
		custom_reset_ask(-1);
}

static int custom_handle(struct ae_screen *instance, struct ae_event const *event)
{
	short item = custom.list.list.focus;

	(void)instance;
	/* (waiting: a pad's buttons are the wait's, B too; with none of its down, BACK is Esc, which cancels) */
	if (custom.waiting)
	{
		int has_pad = 0;

		if (event->action == AE_ACTION_BACK && !custom_pad_buttons(&has_pad))
			custom_wait_end("cancelled", NULL, AE_SOUND_BACK);
		return 1;
	}
	if (custom.settling)
		return 1;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return custom_keep_ask();
	case AE_ACTION_ACCEPT:
		custom_accept(item);
		return 1;
	case AE_ACTION_Y:
		custom_reset_ask(item >= 1 && item <= CUSTOM_ROWS ? (short)(item - 1) : -1);
		return 1;
	default:
		break;
	}
	if (ae_list_view_event(&custom.list, event))
	{
		if (custom.list.list.focus < 1)
			ae_list_set_focus(&custom.list.list, 1);
		if (custom.list.list.focus != item)
		{
			custom.said[0] = 0;
			custom.notice[0] = 0;
		}
	}
	return 1;
}

static void custom_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	short item;

	/* (waiting: the mouse can't bind; a click anywhere cancels) */
	if (custom.waiting)
	{
		if (pointer->left_clicks)
			custom_wait_end("cancelled", NULL, AE_SOUND_BACK);
		return;
	}
	if (ae_prompts_pointer(custom.prompts, custom.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			if (!custom_keep_ask())
			{
				ae_sound_request(AE_SOUND_BACK, 0);
				ae_ui_pop();
			}
		}
		else
			custom_handle(instance, &event);
		return;
	}
	item = ae_list_view_pointer(&custom.list, pointer, HIT_LIST);
	if (custom.list.list.focus < 1)
		ae_list_set_focus(&custom.list.list, 1);
	custom_accept(item);
}

static void custom_leave(struct ae_screen *instance)
{
	(void)instance;
	ae_player_settings_flush();
	platform_log("ae custom layout: closed");
}

static struct ae_screen_class const custom_class =
{
	.name = "custom layout", .leave = custom_leave, .handle = custom_handle, .draw = custom_draw,
	.pointer = custom_pointer, .update = custom_update,
};

int ae_screen_custom_layout_open(short local_player)
{
	struct player_profile profile;
	struct ae_profile_controls controls;
	int at;

	memset(&custom, 0, sizeof(custom));
	custom.page.logged_width = -1.0f;
	custom.moved_row = -1;
	custom.player = local_player >= 0 && local_player < PLAYERS ? local_player : 0;
	memset(&profile, 0, sizeof(profile));
	player_ui_get_active_player_profile(custom.player, &profile);
	for (at = 0; at < (int)sizeof(custom.player_name) - 1 && at < MAXIMUM_PLAYER_PROFILE_NAME_LENGTH &&
		profile.player_name[at]; at++)
	{
		custom.player_name[at] = profile.player_name[at] >= 0x20 && profile.player_name[at] < 0x7F ?
			(char)profile.player_name[at] : '?';
	}
	custom.player_name[at] = 0;
	if (!custom.player_name[0])
		snprintf(custom.player_name, sizeof(custom.player_name), "Player %d", custom.player + 1);
	if (!ae_player_setting_get(custom.player, custom_key, custom.opened, sizeof(custom.opened)))
		custom.opened[0] = 0;
	custom_layout(custom.player, custom.opened_layout);
	custom.opened_preset = ae_profile_get_controls(custom.player, &controls).ok ? controls.layout : 0;
	ae_list_view_init(&custom.list, CUSTOM_ITEMS, 12);
	ae_list_set_focus(&custom.list.list, 1);
	custom.list.previous_focus = 1;
	/* (the press that opened this page is no press on it) */
	custom.settling = 1;
	if (!ae_ui_push(&custom_class, AE_OWNER_ANY, &custom))
		return 0;
	platform_log("ae custom layout: open player %d (preset %d, \"%s\")", custom.player + 1, custom.opened_preset,
		custom.opened);
	return 1;
}
