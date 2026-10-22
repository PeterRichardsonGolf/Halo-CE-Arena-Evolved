/*
AE_SCREEN_CHANGE_MAP.C

The game setup spike's CHANGE MAP screen (ae_screen_change_map.h). One page (the mockup's board 4,
https://claude.ai/artifact/DNum7ugkfszzX99GuZEpsD): the title and "N OF M MAPS SUIT <filter>" top right; the filter's
chips between LB and RB; the folders' list (a chevron, the name, its maps under the filter; one open in place, its maps
under it); the panel (the map's picture or a dashed place for one, its name and badge, players . size . credits,
PLAYS WELL WITH, how players get it). Positions are fractions of the frame (ae_frame_compute), sizes u.

Input (anyone's): up / down move; A opens or closes a folder, or picks a map (the next game's: logged) and closes;
right opens a folder, left closes the open one (back on its row); LB / RB (Q / E) the filter; LT / RT (PgUp / PgDn) the next
folder, opened; Y FAVOURITE (this session's); B back. The pointer: hover, a click on a row as A, on a chip its filter.

Its log: "ae change map: open (filter <name>)", "filter <name>: N of M", "folder <name> open|closed", "focus '<map>'",
the glue's lines, "closed".
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_glue_setup_maps.h"
#include "ae_hooks.h"
#include "ae_screen_change_map.h"
#include "ae_setup_maps.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	MAP_HIT = 0x7E00,
	HIT_LIST = MAP_HIT + 1, HIT_CHIPS, HIT_PROMPTS, HIT_PLAYS,
	ROWS_MAXIMUM = 640,
	PROMPTS = 7,
	FACTS_PER_FRAME = 4
};

/* the frame's positions (fractions of 1920 x 1080 at 100 %) */
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
#define PICTURE_RATIO 0.52f
#define PANEL_GAP_U 18.0f
#define NAME_SCALE 1.5f
#define FOOTER_ABOVE_U 62.0f
#define FOOTER_ROOM_U 40.0f

static struct ae_setup_maps maps;

static struct
{
	struct ae_density density;
	struct ae_frame frame;
	int filter, open_group;
	short rows[ROWS_MAXIMUM], count;
	short focus_entry, logged_entry;
	struct ae_list_view list;
	struct ae_prompt prompts[PROMPTS];
	short prompt_count;
	struct ae_chip chips[AE_MAP_FILTERS];
	char chip_labels[AE_MAP_FILTERS][32];
	short facts_left;
	/* (CREATE GAME's) a pick told back instead of taken here */
	void (*picked)(short list_index, char const *name);
} screen;

static struct ae_screen_class const map_class;

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
}

static short focused_entry(void)
{
	short focus = screen.list.list.focus;

	return focus >= 0 && focus < screen.count ? screen.rows[focus] : (short)-1 - AE_SETUP_MAP_GROUPS;
}

static short index_of_entry(short entry)
{
	short index;

	for (index = 0; index < screen.count; index++)
	{
		if (screen.rows[index] == entry)
			return index;
	}
	return -1;
}

/* the rows again (the filter, the open folder, facts read, a favourite): the focus stays on its row, else on its
folder's, else the first */
static void refresh(void)
{
	short entry = screen.focus_entry, index;

	screen.count = ae_setup_map_rows(&maps, screen.filter, screen.open_group, screen.rows, ROWS_MAXIMUM);
	ae_list_set_count(&screen.list.list, screen.count);
	index = index_of_entry(entry);
	if (index < 0 && !ae_setup_map_row_is_group(entry) && screen.open_group >= 0)
		index = ae_setup_map_group_row(screen.rows, screen.count, screen.open_group);
	ae_list_set_focus(&screen.list.list, index < 0 ? 0 : index);
	screen.focus_entry = focused_entry();
	if (screen.focus_entry != screen.logged_entry && screen.count)
	{
		screen.logged_entry = screen.focus_entry;
		if (ae_setup_map_row_is_group(screen.focus_entry))
			platform_log("ae change map: focus folder %s",
				maps.group_names[ae_setup_map_row_group(screen.focus_entry)]);
		else
			platform_log("ae change map: focus '%s'", maps.maps[screen.focus_entry].file);
	}
}

/* the focus moved by the list (input, pointer): remembered by its row */
static void focus_taken(void)
{
	screen.focus_entry = focused_entry();
	refresh();
}

static void folder_set(int group)
{
	int was = screen.open_group;

	screen.open_group = group;
	if (was >= 0 && was != group)
		platform_log("ae change map: folder %s closed", maps.group_names[was]);
	if (group >= 0)
		platform_log("ae change map: folder %s open", maps.group_names[group]);
	refresh();
}

static void filter_set(int filter)
{
	screen.filter = (filter % AE_MAP_FILTERS + AE_MAP_FILTERS) % AE_MAP_FILTERS;
	refresh();
	platform_log("ae change map: filter %s: %d of %d", ae_setup_map_filter_name(screen.filter),
		ae_setup_map_suiting(&maps, screen.filter), maps.count);
}

/* the focus's folder row scrolled to the list's top (as far as the list goes) */
static void show_row_at_top(short row)
{
	struct ae_list *list = &screen.list.list;
	short last_first = list->count > list->rows ? (short)(list->count - list->rows) : 0;

	ae_list_set_focus(list, row);
	list->first = row < last_first ? row : last_first;
	screen.focus_entry = focused_entry();
}

/* A on a row (or a click) */
static void accept(short entry)
{
	if (ae_setup_map_row_is_group(entry))
	{
		int group = ae_setup_map_row_group(entry);

		if (group >= maps.group_count)
			return;
		folder_set(screen.open_group == group ? -1 : group);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	}
	if (entry < 0 || entry >= maps.count)
		return;
	if (screen.picked)
	{
		platform_log("ae change map: pick '%s' (for the caller)", maps.maps[entry].file);
		screen.picked(maps.maps[entry].list_index, maps.maps[entry].name);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		ae_ui_pop();
		return;
	}
	if (!ae_setup_map_pick(&maps, entry))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_ui_pop();
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

struct item_context { struct ae_density const *density; };

static void list_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct item_context const *list = context;
	struct ae_row row;
	short entry = screen.rows[item];
	float pad = u(list->density->metrics->pad), minor = ae_size_minor(list->density), center = y + height * 0.5f;
	int on_bar = (flags & AE_ROW_ON_BAR) != 0;
	unsigned int ink = on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED;
	char right[32];

	memset(&row, 0, sizeof(row));
	row.flags = flags;
	if (ae_setup_map_row_is_group(entry))
	{
		int group = ae_setup_map_row_group(entry);
		char label[48];

		/* (room at the left for the chevron) */
		snprintf(label, sizeof(label), "     %s", maps.group_names[group]);
		row.label = label;
		ae_widget_row(list->density, x, y, width, &row, HIT_LIST, item);
		chevron(x + pad + u(CHEVRON_U), center, group == screen.open_group, on_bar ? AE_COLOR_SELECTION_TEXT :
			AE_COLOR_ACCENT);
		snprintf(right, sizeof(right), "%d", ae_setup_map_group_count(&maps, group, screen.filter));
		ae_draw_text(AE_FONT_BODY, minor, x + width - pad, cap_top(AE_FONT_BODY, minor, center), AE_ALIGN_RIGHT, ink,
			right);
		return;
	}
	{
		struct ae_setup_map const *map = &maps.maps[entry];
		float indent = u(INDENT_U), text_right = x + width - pad;

		row.label = map->name;
		ae_widget_row(list->density, x + indent, y, width - indent, &row, HIT_LIST, item);
		if (map->players_min && map->players_max)
			snprintf(right, sizeof(right), "%d-%d", map->players_min, map->players_max);
		else
			snprintf(right, sizeof(right), "%s", map->size == 's' ? "SMALL" : map->size == 'm' ? "MEDIUM" :
				map->size == 'l' ? "LARGE" : "");
		text_right -= ae_draw_text(AE_FONT_BODY, minor, text_right, cap_top(AE_FONT_BODY, minor, center),
			AE_ALIGN_RIGHT, ink, right);
		if (map->favourite)
			ae_widget_separator(list->density, text_right - u(14.0f), center, 8.0f, on_bar ? AE_COLOR_SELECTION_TEXT :
				AE_COLOR_ACCENT);
	}
}

static void draw_header(void)
{
	char crumb[96];
	float size = u(TITLE_U), top = frame_y(TITLE_Y), crumb_size = u(CRUMB_U), middle;

	middle = top + ae_draw_cap_height(AE_FONT_TITLE, size) * 0.5f;
	ae_draw_text(AE_FONT_TITLE, size, left_x(), top, AE_ALIGN_LEFT, AE_COLOR_TITLE, "CHANGE MAP");
	if (screen.filter == AE_MF_ALL)
		snprintf(crumb, sizeof(crumb), "%d MAPS", maps.count);
	else
		snprintf(crumb, sizeof(crumb), "%d OF %d MAPS SUIT %s", ae_setup_map_suiting(&maps, screen.filter), maps.count,
			ae_setup_map_filter_name(screen.filter));
	ae_draw_text_tracked(AE_FONT_BODY, crumb_size, CRUMB_TRACKING, right_x(), cap_top(AE_FONT_BODY, crumb_size, middle),
		AE_ALIGN_RIGHT, AE_COLOR_TEXT, crumb);
}

/* the filters between LT and RT; returns the rule's y under them */
static float draw_filters(void)
{
	struct ae_rect rects[AE_MAP_FILTERS];
	float glyph = ae_size_glyph(&screen.density), y = frame_y(CHIPS_Y), x = left_x(), chips_x, height, end;
	int filter;

	for (filter = 0; filter < AE_MAP_FILTERS; filter++)
	{
		snprintf(screen.chip_labels[filter], sizeof(screen.chip_labels[filter]), "%s", ae_setup_map_filter_name(filter));
		screen.chips[filter].label = screen.chip_labels[filter];
		screen.chips[filter].count = ae_setup_map_suiting(&maps, filter);
		screen.chips[filter].flags = filter == screen.filter ? AE_CHIP_ON : 0;
	}
	chips_x = x + ae_draw_button_width(AE_FONT_LAST_DEVICE, AE_BUTTON_LEFT_SHOULDER, glyph) + u(GLYPH_GAP_U);
	height = ae_chips_layout(&screen.density, right_x() - chips_x - u(80.0f), screen.chips, AE_MAP_FILTERS, rects);
	ae_draw_button(AE_FONT_LAST_DEVICE, AE_BUTTON_LEFT_SHOULDER, glyph, x, y + (rects[0].height - glyph) * 0.5f,
		AE_COLOR_KEY_CAP);
	ae_widget_chips(&screen.density, chips_x, y, right_x() - chips_x - u(80.0f), screen.chips, AE_MAP_FILTERS, HIT_CHIPS);
	end = chips_x + rects[AE_MAP_FILTERS - 1].x + rects[AE_MAP_FILTERS - 1].width + u(GLYPH_GAP_U);
	ae_draw_button(AE_FONT_LAST_DEVICE, AE_BUTTON_RIGHT_SHOULDER, glyph, end,
		y + rects[AE_MAP_FILTERS - 1].y + (rects[AE_MAP_FILTERS - 1].height - glyph) * 0.5f, AE_COLOR_KEY_CAP);
	return y + height + u(14.0f);
}

/* players, size and credits on one line, drawn dots between them */
static void facts_line(struct ae_setup_map const *map, float x, float y)
{
	char const *parts[3];
	char players[24];
	float minor = ae_size_minor(&screen.density) * 1.1f, center = y + ae_draw_cap_height(AE_FONT_BODY, minor) * 0.5f;
	int count = 0, index;

	if (map->players_min && map->players_max)
	{
		snprintf(players, sizeof(players), "%d-%d PLAYERS", map->players_min, map->players_max);
		parts[count++] = players;
	}
	if (map->size)
		parts[count++] = map->size == 's' ? "SMALL" : map->size == 'm' ? "MEDIUM" : "LARGE";
	if (map->credits[0])
		parts[count++] = map->credits;
	if (!count)
		parts[count++] = map->facts == AE_SETUP_FACTS_NONE ? "READING THE MAP..." : "NO FACTS YET";
	for (index = 0; index < count && x < screen.frame.rect.x + screen.frame.rect.width; index++)
	{
		if (index)
		{
			ae_widget_separator(&screen.density, x + u(10.0f), center, 5.0f, AE_COLOR_MUTED);
			x += u(20.0f);
		}
		x += ae_draw_text_tracked(AE_FONT_BODY, minor, 0.06f, x, y, AE_ALIGN_LEFT, AE_COLOR_TEXT, parts[index]);
	}
}

static void draw_panel(float x, float y, float width, float bottom)
{
	short entry = screen.focus_entry;
	float text = ae_size_text(&screen.density), minor = ae_size_minor(&screen.density), body = u(screen.density.metrics->body);
	float line = u(screen.density.metrics->body_line), picture_height = width * PICTURE_RATIO;
	char lines[3][160];
	int count, index;

	if (!screen.count)
		return;
	if (ae_setup_map_row_is_group(entry))
	{
		int group = ae_setup_map_row_group(entry);
		char words[96];

		ae_draw_text(AE_FONT_ROW, text * NAME_SCALE, x, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, maps.group_names[group]);
		y += ae_draw_cap_height(AE_FONT_ROW, text * NAME_SCALE) + u(PANEL_GAP_U);
		snprintf(words, sizeof(words), "%d maps%s. A opens the folder here; LB / RB go from folder to folder.",
			ae_setup_map_group_count(&maps, group, screen.filter), screen.filter == AE_MF_ALL ? "" : " suit the filter");
		count = ae_wrap_text(AE_FONT_BODY, body, words, width, lines, 3);
		for (index = 0; index < count; index++)
			ae_draw_text(AE_FONT_BODY, body, x, y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
		return;
	}
	{
		struct ae_setup_map const *map = &maps.maps[entry];
		struct ae_chip chips[AE_MAP_FILTERS];
		int filters[AE_MAP_FILTERS], image, w = 0, h = 0, plays;
		char const *badge = ae_setup_map_badge(&maps, entry);

		/* the picture, fitted whole in its place; else the place dashed, with the map's name */
		image = ae_setup_map_picture(&maps, entry, &w, &h);
		ae_draw_rect(x, y, width, picture_height, 0.0f, AE_COLOR_WELL);
		if (image && w > 0 && h > 0)
		{
			float scale = width / (float)w < picture_height / (float)h ? width / (float)w : picture_height / (float)h;
			float dw = (float)w * scale, dh = (float)h * scale;

			ae_draw_image(image, x + (width - dw) * 0.5f, y + (picture_height - dh) * 0.5f, dw, dh, 0xFFFFFFFFu);
		}
		else
		{
			ae_widget_dashed(x, y, width, picture_height, u(1.0f), u(6.0f), AE_COLOR_RULE);
			ae_draw_text_tracked(AE_FONT_BODY, minor, 0.12f, x + width * 0.5f,
				cap_top(AE_FONT_BODY, minor, y + picture_height * 0.5f), AE_ALIGN_CENTER, AE_COLOR_MUTED, "NO PICTURE");
		}
		y += picture_height + u(PANEL_GAP_U);
		ae_draw_text(AE_FONT_ROW, text * NAME_SCALE, x, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, map->name);
		if (badge[0])
		{
			float name_width = ae_draw_text_width(AE_FONT_ROW, text * NAME_SCALE, map->name);
			float badge_width = ae_draw_text_tracked_width(AE_FONT_BODY, minor * 0.85f, 0.1f, badge) + u(16.0f);
			float badge_height = u(24.0f), center = y + ae_draw_cap_height(AE_FONT_ROW, text * NAME_SCALE) * 0.5f;

			if (x + name_width + u(14.0f) + badge_width <= x + width)
			{
				ae_draw_outline(x + name_width + u(14.0f), center - badge_height * 0.5f, badge_width, badge_height, u(2.0f),
					u(1.0f), AE_COLOR_ACCENT);
				ae_draw_text_tracked(AE_FONT_BODY, minor * 0.85f, 0.1f, x + name_width + u(22.0f),
					cap_top(AE_FONT_BODY, minor * 0.85f, center), AE_ALIGN_LEFT, AE_COLOR_ACCENT, badge);
			}
		}
		y += ae_draw_cap_height(AE_FONT_ROW, text * NAME_SCALE) + u(PANEL_GAP_U);
		facts_line(map, x, y);
		y += u(40.0f);
		plays = ae_setup_map_plays_well(map, filters, AE_MAP_FILTERS);
		if (plays)
		{
			y += ae_widget_group(&screen.density, x, y, width, "PLAYS WELL WITH");
			for (index = 0; index < plays; index++)
			{
				chips[index].label = ae_setup_map_filter_name(filters[index]);
				chips[index].count = -1;
				chips[index].flags = filters[index] == screen.filter ? AE_CHIP_ON : 0;
			}
			y += ae_widget_chips(&screen.density, x, y, width, chips, (short)plays, HIT_PLAYS) + u(PANEL_GAP_U);
		}
		count = ae_wrap_text(AE_FONT_BODY, body, ae_setup_map_sharing(&maps, entry), width, lines, 3);
		for (index = 0; index < count && y + line * (float)(index + 1) <= bottom; index++)
			ae_draw_text(AE_FONT_BODY, body, x, y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_MUTED, lines[index]);
	}
}

static void draw_footer(void)
{
	char status[48];
	short count = 0;
	short entry = screen.focus_entry;

	screen.prompts[count++] = prompt_of(AE_BUTTON_A, ae_setup_map_row_is_group(entry) ? "Open" : "Play this map",
		AE_ACTION_ACCEPT);
	screen.prompts[count++] = prompt_of(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	screen.prompts[count++] = prompt_of(AE_BUTTON_LEFT_SHOULDER, "Filter", AE_ACTION_TAB_PREVIOUS);
	screen.prompts[count++] = prompt_of(AE_BUTTON_RIGHT_SHOULDER, "Filter", AE_ACTION_TAB_NEXT);
	screen.prompts[count++] = prompt_of(AE_BUTTON_LEFT_TRIGGER, "Folder", AE_ACTION_PAGE_UP);
	screen.prompts[count++] = prompt_of(AE_BUTTON_RIGHT_TRIGGER, "Folder", AE_ACTION_PAGE_DOWN);
	if (!ae_setup_map_row_is_group(entry))
		screen.prompts[count++] = prompt_of(AE_BUTTON_Y, "Favourite", AE_ACTION_Y);
	screen.prompt_count = count;
	status[0] = 0;
	if (screen.facts_left)
		snprintf(status, sizeof(status), "Reading maps %d / %d", maps.count - screen.facts_left, maps.count);
	ae_widget_prompts(&screen.density, left_x(), screen.frame.rect.height - u(FOOTER_ABOVE_U), screen.prompts, count,
		status[0] ? status : NULL, right_x(), -1, HIT_PROMPTS);
}

static void map_draw(struct ae_screen *instance)
{
	struct item_context context;
	float top, bottom, list_width, panel_x, rule;

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
	panel_x = screen.frame.rect.x + screen.frame.rect.width * PANEL_X;
	context.density = &screen.density;
	ae_widget_list(&screen.density, &screen.list, left_x(), top, list_width, bottom - top, list_item, &context, HIT_LIST);
	draw_panel(panel_x, top, right_x() - panel_x, bottom);
	draw_footer();
}

/* ---------- input */

static void map_update(struct ae_screen *instance)
{
	(void)instance;
	if (!screen.facts_left)
		return;
	screen.facts_left = ae_setup_maps_read_facts(&maps, FACTS_PER_FRAME);
	refresh();
}

static int map_handle(struct ae_screen *instance, struct ae_event const *event)
{
	short entry = screen.focus_entry;

	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		/* (LB / RB: the filter here, the owner's pick; LT / RT the folders) */
		filter_set(screen.filter + (event->action == AE_ACTION_TAB_NEXT ? 1 : -1));
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		return 1;
	case AE_ACTION_PAGE_UP:
	case AE_ACTION_PAGE_DOWN:
	{
		short row = ae_setup_map_group_step(screen.rows, screen.count, screen.list.list.focus,
			event->action == AE_ACTION_PAGE_DOWN ? 1 : -1);

		if (row < 0 || row >= screen.count || !ae_setup_map_row_is_group(screen.rows[row]))
			return 1;
		screen.focus_entry = screen.rows[row];
		folder_set(ae_setup_map_row_group(screen.rows[row]));
		show_row_at_top(index_of_entry(screen.focus_entry));
		ae_sound_request(AE_SOUND_CURSOR, 0);
		return 1;
	}
	case AE_ACTION_ACCEPT:
		accept(entry);
		return 1;
	case AE_ACTION_RIGHT:
		if (ae_setup_map_row_is_group(entry) && ae_setup_map_row_group(entry) != screen.open_group)
			accept(entry);
		return 1;
	case AE_ACTION_LEFT:
		if (screen.open_group >= 0)
		{
			screen.focus_entry = (short)(-1 - screen.open_group);
			folder_set(-1);
			ae_sound_request(AE_SOUND_BACK, 0);
		}
		return 1;
	case AE_ACTION_Y:
		if (!ae_setup_map_row_is_group(entry))
		{
			ae_setup_map_favourite(&maps, entry);
			refresh();
		}
		return 1;
	default:
		break;
	}
	if (ae_list_view_event(&screen.list, event))
		focus_taken();
	return 1;
}

static void map_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
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
			map_handle(instance, &event);
		return;
	}
	if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_CHIPS && hit.index >= 0 &&
		hit.index < AE_MAP_FILTERS)
	{
		filter_set(hit.index);
		ae_sound_request(AE_SOUND_CURSOR, 0);
		return;
	}
	item = ae_list_view_pointer(&screen.list, pointer, HIT_LIST);
	focus_taken();
	if (item >= 0 && item < screen.count)
		accept(screen.rows[item]);
}

static void map_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae change map: closed");
}

static struct ae_screen_class const map_class =
{
	.name = "change map", .leave = map_leave, .handle = map_handle, .draw = map_draw, .pointer = map_pointer,
	.update = map_update,
};

int ae_screen_change_map_open(int filter)
{
	if (!ae_screen_change_map_pick(filter, -1, NULL))
		return 0;
	ae_ui_replace_menus();
	return 1;
}

int ae_screen_change_map_pick(int filter, short current, void (*picked)(short list_index, char const *name))
{
	short last;
	int source;

	memset(&screen, 0, sizeof(screen));
	screen.picked = picked;
	screen.logged_entry = (short)-1 - AE_SETUP_MAP_GROUPS;
	screen.filter = filter >= 0 && filter < AE_MAP_FILTERS ? filter : AE_MF_ALL;
	ae_setup_maps_load(&maps);
	screen.facts_left = maps.count;
	/* (the folder of the map given, else the map used last, open, the map focused) */
	last = current >= 0 ? current : ae_setup_maps_last_used();
	source = ae_setup_map_source(last);
	screen.open_group = source;
	if (last >= 0 && last < maps.count && ae_setup_map_suits(&maps.maps[last], screen.filter))
		screen.focus_entry = last;
	else
		screen.focus_entry = source >= 0 ? (short)(-1 - source) : (short)-1 - AE_SETUP_MAP_GROUPS;
	ae_list_view_init(&screen.list, 0, 10);
	refresh();
	if (!ae_ui_push(&map_class, AE_OWNER_ANY, &screen))
		return 0;
	platform_log("ae change map: open (filter %s)", ae_setup_map_filter_name(screen.filter));
	(void)ae_settings_ui_scale();
	return 1;
}
