/*
AE_SCREEN_CHANGE_MAP.C

The game setup spike's CHANGE MAP screen (ae_screen_change_map.h). One page (the mockup's board 4; AE menus Phase 2):
the title and "N MAPS" top right; the filter's chips between LB and RB, the search field on their line's right; the
list: RECENT (the maps played last, each once, newest first: the picker's own, 2026-10-28; the pause menu's
CHANGE MAP too), FAVOURITES and the player's own folders, then the folders by source (a chevron, the name, its maps
under the filter; one open in place, its maps under it); the panel (the map's picture or a dashed place for one, its
name and badge, players . size . credits, PLAYS WELL WITH, how players get it). Positions are fractions of the frame
(ae_frame_compute), sizes u.

Input (anyone's): up / down move; A opens or closes a folder, or picks a map (the next game's: logged; a RECENT row's
too, the map only) and closes; right opens a folder, left closes the open one (back on its row); LB / RB
(Q / E) the filter; LT / RT (PgUp / PgDn) the cursor to the next folder, opening nothing; X (F) the folder menu on a map (FAVOURITES, the
player's folders, NEW FOLDER...), RENAME / DELETE on a folder of the player's; Y (R) the search: typing on a keyboard
(Enter or down to the list, Esc clears it, then leaves it), AE's keyboard on a pad, the filter set to ALL first (owner
2026-10-28: a search covers every map; it stays on ALL after); B clears a search, else back. The
pointer: hover, a click on a row as A, on a chip its filter, on the search field typing.

Its log: "ae change map: open (filter <name>)", "filter <name>: N of M", "folder <name> open|closed", "focus '<map>'",
"recent N: '<map>', ...", "(filter ALL): search opens", "search '<text>': N", "pick recent '<map>'", the common parts'
(ae_picker_common.h), the glue's lines, "closed".
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_glue_picker_stores.h"
#include "ae_glue_setup_maps.h"
#include "ae_glue_setup_options.h"
#include "ae_hooks.h"
#include "ae_picker_common.h"
#include "ae_screen_change_map.h"
#include "ae_setup_maps.h"
#include "ae_sound.h"
#include "ae_stage_map.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	MAP_HIT = 0x7E00,
	HIT_LIST = MAP_HIT + 1, HIT_CHIPS, HIT_PROMPTS, HIT_PLAYS, HIT_SEARCH,
	ROWS_MAXIMUM = AE_SETUP_MAPS_MAXIMUM + AE_SETUP_MAP_GROUPS + AE_SETUP_ROW_RECENT_MAXIMUM + 1,
	PROMPTS = 7,
	FACTS_PER_FRAME = 4,
	NO_ENTRY = -1 - AE_SETUP_MAP_GROUPS
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
#define TRACK_STRIP_U 24.0f
#define TAG_GAP_U 24.0f
#define NOTCH_U 6.0f

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
	/* (CREATE GAME's, the pause menu's) a pick told back instead of taken here */
	void (*picked)(short list_index, char const *name);
	/* RECENT's maps (indices into maps): those still here, read as the screen opens; those under the filter */
	short recent_here_count, recent_here[AE_RECENT_SHOWN], recent_count, recent_maps[AE_RECENT_SHOWN];
	/* the search: its field, the query the rows were made for */
	struct ae_picker_search search;
	char query[AE_PICKER_QUERY];
	/* the list as last drawn (layout units) */
	float list_x, list_top, list_width, list_bottom;
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

static short focused_entry(void)
{
	short focus = screen.list.list.focus;

	return focus >= 0 && focus < screen.count ? screen.rows[focus] : (short)NO_ENTRY;
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

/* a map row (a map, or a RECENT game's): its map, -1 none */
static short map_of_entry(short entry)
{
	if (entry >= 0 && entry < maps.count)
		return entry;
	if (ae_setup_map_row_is_recent(entry) && ae_setup_map_row_recent(entry) < screen.recent_count)
		return screen.recent_maps[ae_setup_map_row_recent(entry)];
	return -1;
}

/* a group of the player's own (renamed or deleted with X) */
static int own_group(int group)
{
	return group >= 0 && group < maps.group_count && (maps.group_flags[group] & AE_SETUP_GROUP_OWN);
}

/* a map of the list by its id (the folders' and RECENT's), -1 none */
static short map_of_id(const char *id)
{
	short index;

	for (index = 0; index < maps.count; index++)
	{
		if (!strcmp(maps.maps[index].id, id))
			return index;
	}
	return -1;
}

/* a RECENT map still there: listed, its map file here */
static int recent_installed(const char *id, void *context)
{
	short map = map_of_id(id);

	(void)context;
	return map >= 0 && ae_stage_map_file_present(maps.maps[map].file);
}

/* RECENT's maps still here (ae_recent_games_by: the maps of the games played, newest first, each once, the first 8
listed with their map file; once, as the screen opens: the file checks) */
static void recent_read(void)
{
	struct ae_recent_games const *recent;
	short shown[AE_RECENT_SHOWN], count, index;
	char words[AE_RECENT_SHOWN * (AE_RECENT_ID + 4)];

	screen.recent_here_count = 0;
	recent = ae_recent_store();
	count = ae_recent_games_by(recent, AE_RECENT_BY_MAP, recent_installed, NULL, shown, AE_RECENT_SHOWN);
	words[0] = 0;
	for (index = 0; index < count; index++)
	{
		size_t length = strlen(words);

		screen.recent_here[screen.recent_here_count++] = map_of_id(recent->games[shown[index]].map);
		snprintf(words + length, sizeof(words) - length, "%s'%s'", index ? ", " : "", recent->games[shown[index]].map);
	}
	platform_log("ae change map: recent %d%s%s", count, count ? ": " : "", words);
}

/* RECENT's maps that suit the filter */
static void recent_make(void)
{
	short index;

	screen.recent_count = 0;
	for (index = 0; index < screen.recent_here_count; index++)
	{
		if (ae_setup_map_suits(&maps.maps[screen.recent_here[index]], screen.filter))
			screen.recent_maps[screen.recent_count++] = screen.recent_here[index];
	}
}

/* the rows again (the filter, the open folder, facts read, the folders, the search): the focus stays on its row, else
on its folder's, else the first */
static void refresh(void)
{
	short entry = screen.focus_entry, index;

	recent_make();
	if (searching())
		screen.count = ae_setup_map_search(&maps, screen.filter, screen.query, screen.rows, ROWS_MAXIMUM);
	else
		screen.count = ae_setup_map_rows(&maps, screen.filter, screen.open_group, screen.recent_count, screen.rows,
			ROWS_MAXIMUM);
	ae_list_set_count(&screen.list.list, screen.count);
	index = index_of_entry(entry);
	if (index < 0 && !ae_setup_map_row_is_group(entry) && screen.open_group >= 0 && !searching())
		index = ae_setup_map_group_row(screen.rows, screen.count, screen.open_group);
	/* (an empty folder's line is never focused: its folder's row is) */
	if (index >= 0 && screen.rows[index] == AE_SETUP_ROW_EMPTY)
		index--;
	ae_list_set_focus(&screen.list.list, index < 0 ? 0 : index);
	screen.focus_entry = focused_entry();
	if (screen.focus_entry != screen.logged_entry && screen.count)
	{
		short map = map_of_entry(screen.focus_entry);

		screen.logged_entry = screen.focus_entry;
		if (ae_setup_map_row_is_group(screen.focus_entry))
			platform_log("ae change map: focus folder %s",
				maps.group_names[ae_setup_map_row_group(screen.focus_entry)]);
		else if (screen.focus_entry == AE_SETUP_ROW_RECENT)
			platform_log("ae change map: focus folder RECENT");
		else if (map >= 0)
			platform_log("ae change map: focus '%s'%s", maps.maps[map].file,
				ae_setup_map_row_is_recent(screen.focus_entry) ? " (recent)" : "");
	}
}

/* the focus moved by the list (input, pointer): remembered by its row; never on an empty folder's line */
static void focus_taken(int direction)
{
	struct ae_list *list = &screen.list.list;

	if (list->focus >= 0 && list->focus < screen.count && screen.rows[list->focus] == AE_SETUP_ROW_EMPTY)
	{
		short next = (short)(list->focus + (direction >= 0 ? 1 : -1));

		ae_list_set_focus(list, next >= 0 && next < screen.count ? next : (short)(list->focus - 1));
	}
	screen.focus_entry = focused_entry();
	refresh();
}

static void folder_set(int group)
{
	int was = screen.open_group;

	screen.open_group = group;
	/* (a folder that stays shown open, WORKSHOP while one of its collections opens, isn't closed) */
	if (was >= 0 && was != group && !ae_setup_map_group_open(&maps, group, was))
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
		ae_setup_map_matching(&maps, screen.filter, screen.query), maps.count);
}

/* the search's text as the rows' query, when it changed */
static void query_follow(void)
{
	if (!strncmp(screen.query, screen.search.text.text, sizeof(screen.query) - 1))
		return;
	snprintf(screen.query, sizeof(screen.query), "%s", screen.search.text.text);
	/* (a new search starts at its first result; a cleared one goes back to the row it was on) */
	if (searching())
		screen.focus_entry = NO_ENTRY;
	refresh();
	platform_log("ae change map: search '%s': %d", screen.query, ae_setup_map_matching(&maps, screen.filter,
		screen.query));
}

/* Y (R), a click on the field: the search, on ALL first (owner 2026-10-28: a search covers every map, whatever the
filter was; it stays on ALL when the search is cleared or left, LB / RB still step it) */
static void search_open(int device)
{
	if (screen.filter != AE_MF_ALL)
	{
		screen.filter = AE_MF_ALL;
		refresh();
	}
	platform_log("ae change map: (filter ALL): search opens");
	ae_picker_search_begin(&screen.search, device, &screen.density, &screen.frame.rect, &screen);
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

/* a map for the next game: the caller's, else the game's (1 when taken) */
static int map_take(short map)
{
	/* (a Workshop map Steam is updating: not in the game's list until its file is whole) */
	if (maps.maps[map].unavailable || maps.maps[map].list_index < 0)
	{
		platform_log("ae change map: '%s' can't be picked: Steam is updating it", maps.maps[map].file);
		return 0;
	}
	/* (a stock map is listed with or without its file: one without would stop the game on the damaged-disc screen) */
	if (!ae_stage_map_file_present(maps.maps[map].file))
	{
		platform_log("ae change map: '%s' has no map file here", maps.maps[map].file);
		return 0;
	}
	if (screen.picked)
	{
		platform_log("ae change map: pick '%s' (for the caller)", maps.maps[map].file);
		screen.picked(maps.maps[map].list_index, maps.maps[map].name);
		return 1;
	}
	return ae_setup_map_pick(&maps, map);
}

/* RECENT folded or open (kept for the next time) */
static void recent_fold_set(int folded)
{
	maps.recent_folded = (short)(folded != 0);
	ae_recent_store_set_folded(AE_RECENT_BY_MAP, folded);
	platform_log("ae change map: folder RECENT %s", folded ? "folded" : "open");
	refresh();
}

/* A on a row (or a click) */
static void accept(short entry)
{
	if (ae_setup_map_row_is_group(entry))
	{
		int group = ae_setup_map_row_group(entry);

		if (group >= maps.group_count)
			return;
		/* (an open folder closes to its parent: WORKSHOP's collection back to WORKSHOP, WORKSHOP to none) */
		folder_set(ae_setup_map_group_open(&maps, screen.open_group, group) ? ae_setup_map_group_parent(&maps, group) :
			group);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	}
	/* (RECENT folds and unfolds on its own, the choice kept; an empty folder's line is no row) */
	if (entry == AE_SETUP_ROW_RECENT)
	{
		recent_fold_set(!maps.recent_folded);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	}
	if (entry == AE_SETUP_ROW_EMPTY)
		return;
	/* (a RECENT row: its map only) */
	if (ae_setup_map_row_is_recent(entry))
	{
		if (ae_setup_map_row_recent(entry) >= screen.recent_count)
			return;
		entry = screen.recent_maps[ae_setup_map_row_recent(entry)];
		platform_log("ae change map: pick recent '%s'", maps.maps[entry].id);
	}
	if (entry < 0 || entry >= maps.count)
		return;
	if (!map_take(entry))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_ui_pop();
}

/* ---------- the folder menu (X) */

static short menu_map = -1;

static int menu_in(short folder, void *context)
{
	(void)context;
	return menu_map >= 0 && ae_pf_has(ae_picker_store(AE_PF_MAPS), folder, maps.maps[menu_map].id);
}

static int menu_toggle(short folder, void *context)
{
	int now;

	(void)context;
	now = ae_setup_map_folder_toggle(&maps, menu_map, folder);
	if (now >= 0)
		refresh();
	return now;
}

static void menu_changed(short folder, void *context)
{
	(void)context;
	ae_setup_maps_folders_apply(&maps);
	/* (a folder of the player's open or focused: renamed, it moved (its row then); deleted, it is gone (FAVOURITES'
	row then)) */
	if (ae_setup_map_row_is_group(screen.focus_entry) && ae_setup_map_row_group(screen.focus_entry) < AE_PF_FOLDERS)
	{
		if (screen.open_group == ae_setup_map_row_group(screen.focus_entry))
			screen.open_group = folder;
		screen.focus_entry = (short)(-1 - (folder >= 0 ? folder : AE_PF_FAVOURITES));
	}
	if (screen.open_group >= 0 && screen.open_group < AE_PF_FOLDERS && !maps.group_names[screen.open_group][0])
		screen.open_group = -1;
	refresh();
}

static float pitch(void)
{
	return ae_size_row(&screen.density) + u(screen.density.metrics->gap);
}

static void menu_open(short entry)
{
	struct ae_folder_menu_spec spec;
	int group = ae_setup_map_row_is_group(entry) ? ae_setup_map_row_group(entry) : -1;
	short map = map_of_entry(entry);
	float rows_top = ae_list_view_rows_top(&screen.density, &screen.list, screen.list_top);

	if (map < 0 && !own_group(group))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_PF_MAPS;
	spec.log = "ae change map";
	/* (the focused row's box, the list as last drawn; a map's row is indented) */
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
	if (map < 0)
	{
		spec.actions = 1;
		spec.folder = maps.group_folder[group];
		spec.item = maps.group_names[group];
	}
	else
	{
		menu_map = map;
		spec.item = maps.maps[map].name;
		/* (inside one of the folders, not a search's or RECENT's row: REMOVE FROM it) */
		spec.folder = !searching() && entry >= 0 && screen.open_group >= 0 && screen.open_group < AE_PF_FOLDERS ?
			maps.group_folder[screen.open_group] : -1;
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

/* a folder's row: the chevron, the name, its count */
static void group_row(float x, float y, float width, const char *name, int open, short count, unsigned int flags,
	short item)
{
	struct ae_density const *d = &screen.density;
	struct ae_row row;
	float pad = u(d->metrics->pad), minor = ae_size_minor(d), center = y + ae_size_row(d) * 0.5f;
	int on_bar = (flags & AE_ROW_ON_BAR) != 0;
	char label[64], right[16];

	memset(&row, 0, sizeof(row));
	row.flags = flags;
	/* (room at the left for the chevron) */
	snprintf(label, sizeof(label), "     %s", name);
	row.label = label;
	ae_widget_row(d, x, y, width, &row, HIT_LIST, item);
	chevron(x + pad + u(CHEVRON_U), center, open, on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT);
	snprintf(right, sizeof(right), "%d", count);
	ae_draw_text(AE_FONT_BODY, minor, x + width - pad, cap_top(AE_FONT_BODY, minor, center), AE_ALIGN_RIGHT,
		on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED, right);
}

/* a map's players or size, for the right of its row */
static void size_words(struct ae_setup_map const *map, char *right, int size)
{
	if (map->players_min && map->players_max)
		snprintf(right, (size_t)size, "%d-%d", map->players_min, map->players_max);
	else
		snprintf(right, (size_t)size, "%s", map->size == 's' ? "SMALL" : map->size == 'm' ? "MEDIUM" :
			map->size == 'l' ? "LARGE" : "");
}

static void list_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct ae_density const *d = &screen.density;
	struct ae_row row;
	short entry = screen.rows[item];
	float pad = u(d->metrics->pad), minor = ae_size_minor(d), text = ae_size_text(d), center = y + height * 0.5f;
	float indent = u(INDENT_U);
	int on_bar = (flags & AE_ROW_ON_BAR) != 0;
	unsigned int ink = on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED;
	char right[64], label[160];

	(void)context;
	if (ae_setup_map_row_is_group(entry))
	{
		int group = ae_setup_map_row_group(entry);
		/* (a folder in a folder, WORKSHOP's collections: indented as its maps are) */
		float in = ae_setup_map_group_parent(&maps, group) >= 0 ? indent : 0.0f;

		group_row(x + in, y, width - in, maps.group_names[group], ae_setup_map_group_open(&maps, screen.open_group,
			group), ae_setup_map_group_count(&maps, group, screen.filter), flags, item);
		return;
	}
	if (entry == AE_SETUP_ROW_RECENT)
	{
		group_row(x, y, width, "RECENT", !maps.recent_folded, screen.recent_count, flags, item);
		return;
	}
	if (entry == AE_SETUP_ROW_EMPTY)
	{
		ae_picker_empty_line(d, x + indent + pad, center, AE_PF_MAPS);
		return;
	}
	/* (a RECENT row: its map's, as any map's row) */
	entry = map_of_entry(entry);
	if (entry < 0)
		return;
	memset(&row, 0, sizeof(row));
	row.flags = flags;
	{
		struct ae_setup_map const *map = &maps.maps[entry];
		float text_right = x + width - pad, right_width;
		const char *group = searching() && map->source >= 0 ? maps.group_names[map->source] : "";
		int cut, match;

		size_words(map, right, sizeof(right));
		/* (a Workshop map Steam is still writing: listed, dimmed, its reason on the right) */
		if (map->unavailable == AE_SETUP_MAP_UPDATING)
		{
			snprintf(right, sizeof(right), "STEAM IS UPDATING IT");
			/* (dimmed off the bar; on it, the bar's dark text, so the name still reads) */
			if (!on_bar)
				row.flags |= AE_ROW_DISABLED;
		}
		right_width = ae_draw_text_width(AE_FONT_BODY, minor, right);
		if (group[0])
			right_width += u(TAG_GAP_U) + ae_draw_text_width(AE_FONT_BODY, minor, group);
		cut = ae_fit_text(AE_FONT_ROW, text, map->name, width - indent - 2.0f * pad - right_width - u(TAG_GAP_U), label,
			sizeof(label));
		row.label = label;
		ae_widget_row(d, x + indent, y, width - indent, &row, HIT_LIST, item);
		text_right -= ae_draw_text(AE_FONT_BODY, minor, text_right, cap_top(AE_FONT_BODY, minor, center),
			AE_ALIGN_RIGHT, ink, right);
		/* (a search's row: its group, so two maps of one name are told apart) */
		if (group[0])
			ae_draw_text(AE_FONT_BODY, minor, text_right - u(TAG_GAP_U), cap_top(AE_FONT_BODY, minor, center),
				AE_ALIGN_RIGHT, ink, group);
		/* the search's letters in the accent (not on the bar: its text is dark; not in a cut label) */
		match = searching() && !on_bar && !cut ? ae_setup_map_match(label, screen.query) : -1;
		if (match >= 0)
		{
			char before[160], part[AE_PICKER_QUERY];

			snprintf(before, sizeof(before), "%.*s", match, label);
			snprintf(part, sizeof(part), "%.*s", (int)strlen(screen.query), label + match);
			ae_draw_text(AE_FONT_ROW, text, x + indent + pad + ae_draw_text_width(AE_FONT_ROW, text, before),
				cap_top(AE_FONT_ROW, text, center), AE_ALIGN_LEFT, AE_COLOR_ACCENT, part);
		}
	}
}

/* the query upper case (the crumb's) */
static void query_upper(char *out, int size)
{
	int index;

	for (index = 0; index < size - 1 && screen.query[index]; index++)
		out[index] = screen.query[index] >= 'a' && screen.query[index] <= 'z' ? (char)(screen.query[index] - 32) :
			screen.query[index];
	out[index] = 0;
}

static void draw_header(void)
{
	char crumb[96], query[AE_PICKER_QUERY];
	float size = u(TITLE_U), top = frame_y(TITLE_Y), crumb_size = u(CRUMB_U), middle;
	short shown = ae_setup_map_matching(&maps, screen.filter, screen.query);

	middle = top + ae_draw_cap_height(AE_FONT_TITLE, size) * 0.5f;
	ae_draw_text(AE_FONT_TITLE, size, left_x(), top, AE_ALIGN_LEFT, AE_COLOR_TITLE, "CHANGE MAP");
	query_upper(query, sizeof(query));
	if (searching())
		snprintf(crumb, sizeof(crumb), "%d MAP%s MATCH%s %s", shown, shown == 1 ? "" : "S", shown == 1 ? "ES" : "",
			query);
	else if (screen.filter == AE_MF_ALL)
		snprintf(crumb, sizeof(crumb), "%d MAPS", maps.count);
	else
		snprintf(crumb, sizeof(crumb), "%d OF %d MAPS SUIT %s", shown, maps.count,
			ae_setup_map_filter_name(screen.filter));
	ae_draw_text_tracked(AE_FONT_BODY, crumb_size, CRUMB_TRACKING, right_x(), cap_top(AE_FONT_BODY, crumb_size, middle),
		AE_ALIGN_RIGHT, AE_COLOR_TEXT, crumb);
}

/* the filters between LB and RB, the search on their line's right; returns the rule's y under them */
static float draw_filters(void)
{
	struct ae_rect rects[AE_MAP_FILTERS];
	float glyph = ae_size_glyph(&screen.density), y = frame_y(CHIPS_Y), x = left_x(), chips_x, height, room;
	float rb = ae_draw_button_width(AE_FONT_LAST_DEVICE, AE_BUTTON_RIGHT_SHOULDER, glyph), rb_x, first_end;
	int filter, last = AE_MAP_FILTERS - 1, first_line_last = 0;

	for (filter = 0; filter < AE_MAP_FILTERS; filter++)
	{
		short count = ae_setup_map_matching(&maps, filter, screen.query);

		snprintf(screen.chip_labels[filter], sizeof(screen.chip_labels[filter]), "%s", ae_setup_map_filter_name(filter));
		screen.chips[filter].label = screen.chip_labels[filter];
		screen.chips[filter].count = count;
		screen.chips[filter].flags = filter == screen.filter ? AE_CHIP_ON : count || !searching() ? 0 :
			AE_CHIP_DISABLED;
	}
	chips_x = x + ae_draw_button_width(AE_FONT_LAST_DEVICE, AE_BUTTON_LEFT_SHOULDER, glyph) + u(GLYPH_GAP_U);
	/* (the chips wrap before the search field goes under its 320 u) */
	room = ae_picker_search_chip_room(&screen.density, chips_x, right_x(), rb);
	height = ae_chips_layout(&screen.density, room, screen.chips, AE_MAP_FILTERS, rects);
	ae_widget_chip_key(AE_FONT_LAST_DEVICE, AE_BUTTON_LEFT_SHOULDER, glyph, x, y + rects[0].height * 0.5f);
	ae_widget_chips(&screen.density, chips_x, y, room, screen.chips, AE_MAP_FILTERS, HIT_CHIPS);
	rb_x = chips_x + rects[last].x + rects[last].width + u(GLYPH_GAP_U);
	ae_widget_chip_key(AE_FONT_LAST_DEVICE, AE_BUTTON_RIGHT_SHOULDER, glyph, rb_x,
		y + rects[last].y + rects[last].height * 0.5f);
	/* (the first line's end: RB's when it is on it) */
	for (filter = 0; filter < AE_MAP_FILTERS; filter++)
	{
		if (rects[filter].y == rects[0].y)
			first_line_last = filter;
	}
	first_end = first_line_last == last ? rb_x + rb : chips_x + rects[first_line_last].x + rects[first_line_last].width;
	ae_picker_search_draw(&screen.search, &screen.density, ae_picker_search_left(&screen.density, first_end, panel_x()),
		right_x(), y + rects[0].height * 0.5f, HIT_SEARCH);
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
			ae_widget_separator(&screen.density, x + u(12.0f), center, 5.0f, AE_COLOR_MUTED);
			x += u(24.0f);
		}
		x += ae_draw_text_tracked(AE_FONT_BODY, minor, 0.06f, x, y, AE_ALIGN_LEFT, AE_COLOR_TEXT, parts[index]);
	}
}

/* a muted block of words: drawn whole when it fits above bottom, else not at all (never cut mid-sentence); returns
the y under it */
static float muted_block(const char *words, float x, float y, float width, float bottom)
{
	float body = u(screen.density.metrics->body), line = u(screen.density.metrics->body_line);
	char lines[3][160];
	int count = ae_wrap_text(AE_FONT_BODY, body, words, width, lines, 3), index;

	if (y + line * (float)count > bottom)
		return y;
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, x, y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_MUTED, lines[index]);
	return y + line * (float)count;
}

/* a folder's panel (RECENT's: group -1): its name and what it holds */
static void draw_group_panel(int group, float x, float y, float width)
{
	float text = ae_size_text(&screen.density), body = u(screen.density.metrics->body);
	float line = u(screen.density.metrics->body_line);
	char words[200], expanded[240], lines[3][160];
	short count = group >= 0 ? ae_setup_map_group_count(&maps, group, screen.filter) : screen.recent_count;
	int index, written;

	ae_draw_text(AE_FONT_ROW, text * NAME_SCALE, x, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, group >= 0 ?
		maps.group_names[group] : "RECENT");
	y += ae_draw_cap_height(AE_FONT_ROW, text * NAME_SCALE) + u(PANEL_GAP_U);
	if (group < 0)
		snprintf(words, sizeof(words), "The maps you played last, newest first. {A} on one sets the map.");
	else if (own_group(group))
		snprintf(words, sizeof(words), "%d map%s. Your folder: {X} renames it or deletes it.", count,
			count == 1 ? "" : "s");
	else if (group == AE_SETUP_GROUP_FAVOURITES)
		snprintf(words, sizeof(words), "%d map%s. {X} on a map puts it in your folders or takes it out.", count,
			count == 1 ? "" : "s");
	else
		snprintf(words, sizeof(words), "%d map%s%s. {A} opens the folder here; {LT} / {RT} go from folder to folder.",
			count, count == 1 ? "" : "s", screen.filter == AE_MF_ALL ? "" : " suit the filter");
	written = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(words, expanded, sizeof(expanded)), width, lines, 3);
	for (index = 0; index < written; index++)
		ae_draw_text(AE_FONT_BODY, body, x, y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
}

static void draw_panel(float x, float y, float width, float bottom)
{
	short entry = screen.focus_entry, index = map_of_entry(entry);
	float text = ae_size_text(&screen.density), minor = ae_size_minor(&screen.density);
	float picture_height = width * PICTURE_RATIO;

	if (!screen.count || entry == AE_SETUP_ROW_EMPTY)
		return;
	if (ae_setup_map_row_is_group(entry) || entry == AE_SETUP_ROW_RECENT)
	{
		draw_group_panel(entry == AE_SETUP_ROW_RECENT ? -1 : ae_setup_map_row_group(entry), x, y, width);
		return;
	}
	if (index < 0)
		return;
	{
		struct ae_setup_map const *map = &maps.maps[index];
		struct ae_chip chips[AE_MAP_FILTERS];
		int filters[AE_MAP_FILTERS], image, w = 0, h = 0, plays, chip;
		char const *badge = ae_setup_map_badge(&maps, index);

		/* the picture, fitted whole in its place; else the place dashed, with the map's name */
		image = ae_setup_map_picture(&maps, index, &w, &h);
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
			/* (30 u tall, 10 u sides: CHANGE GAMETYPE's clearance check found 24 u / 8 u short of 8 px) */
			float badge_width = ae_draw_text_tracked_width(AE_FONT_BODY, minor * 0.85f, 0.1f, badge) + u(20.0f);
			float badge_height = u(30.0f), center = y + ae_draw_cap_height(AE_FONT_ROW, text * NAME_SCALE) * 0.5f;

			if (x + name_width + u(14.0f) + badge_width <= x + width)
			{
				ae_draw_outline(x + name_width + u(14.0f), center - badge_height * 0.5f, badge_width, badge_height, u(2.0f),
					u(1.0f), AE_COLOR_ACCENT);
				ae_draw_text_tracked(AE_FONT_BODY, minor * 0.85f, 0.1f, x + name_width + u(24.0f),
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
			for (chip = 0; chip < plays; chip++)
			{
				chips[chip].label = ae_setup_map_filter_name(filters[chip]);
				chips[chip].count = -1;
				chips[chip].flags = filters[chip] == screen.filter ? AE_CHIP_ON : 0;
			}
			y += ae_widget_chips(&screen.density, x, y, width, chips, (short)plays, HIT_PLAYS) + u(PANEL_GAP_U);
		}
		muted_block(ae_setup_map_sharing(&maps, index), x, y, width, bottom);
	}
}

static void draw_footer(void)
{
	char status[48];
	short count = 0, entry = screen.focus_entry, map = map_of_entry(entry);
	int group = ae_setup_map_row_is_group(entry) ? ae_setup_map_row_group(entry) : -1;
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
		if (group >= 0)
			screen.prompts[count++] = prompt_of(AE_BUTTON_A, ae_setup_map_group_open(&maps, screen.open_group, group) ?
				"Close" : "Open", AE_ACTION_ACCEPT, 0);
		else if (entry == AE_SETUP_ROW_RECENT)
			screen.prompts[count++] = prompt_of(AE_BUTTON_A, maps.recent_folded ? "Open" : "Close", AE_ACTION_ACCEPT, 0);
		else if (map >= 0 && !maps.maps[map].unavailable)
			screen.prompts[count++] = prompt_of(AE_BUTTON_A, "Select", AE_ACTION_ACCEPT, 0);
		screen.prompts[count++] = prompt_of(AE_BUTTON_B, searching() ? "Clear search" : ae_string(AE_STR_BACK),
			AE_ACTION_BACK, 0);
		if (map >= 0)
			screen.prompts[count++] = prompt_of(AE_BUTTON_X, "Folders", AE_ACTION_X, 0);
		else if (own_group(group))
			screen.prompts[count++] = prompt_of(AE_BUTTON_X, "Rename or Delete", AE_ACTION_X, 0);
		screen.prompts[count++] = prompt_of(AE_BUTTON_Y, "Search", AE_ACTION_Y, 0);
		/* (the pairs: "LB RB Filter", "LT RT Folder", the owner's pick, at every UI scale) */
		screen.prompts[count++] = prompt_of(AE_BUTTON_LEFT_SHOULDER, "Filter", AE_ACTION_TAB_PREVIOUS, 1);
		if (!searching())
			screen.prompts[count++] = prompt_of(AE_BUTTON_LEFT_TRIGGER, "Folder", AE_ACTION_PAGE_UP, 1);
	}
	screen.prompt_count = count;
	status[0] = 0;
	if (screen.facts_left)
		snprintf(status, sizeof(status), "Reading maps %d / %d", maps.count - screen.facts_left, maps.count);
	ae_widget_prompts(&screen.density, left_x(), screen.frame.rect.height - u(FOOTER_ABOVE_U), screen.prompts, count,
		status[0] ? status : NULL, right_x(), -1, HIT_PROMPTS);
}

/* nothing has the search's letters: one box at the list's top */
static void draw_empty(float x, float y, float width)
{
	struct ae_density const *d = &screen.density;
	float row = ae_size_row(d), gap = u(d->metrics->gap), pad = u(d->metrics->pad), text = ae_size_text(d);
	float body = u(d->metrics->body), height = row * 2.0f + gap;

	ae_draw_rect(x, y, width, height, u(d->metrics->corner), AE_COLOR_ROW);
	ae_draw_text(AE_FONT_ROW, text, x + pad, cap_top(AE_FONT_ROW, text, y + row * 0.5f + gap * 0.5f), AE_ALIGN_LEFT,
		AE_COLOR_TEXT, "Nothing is called that.");
	ae_draw_text(AE_FONT_BODY, body, x + pad, cap_top(AE_FONT_BODY, body, y + row * 1.5f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
		"Try part of a name, like \"gulch\" or \"creek\".");
}

static void map_draw(struct ae_screen *instance)
{
	float top, bottom, list_width, rule;

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
	screen.list_x = left_x();
	screen.list_top = top;
	screen.list_width = list_width;
	screen.list_bottom = bottom;
	if (!screen.count && searching())
		draw_empty(left_x(), top, list_width - u(TRACK_STRIP_U));
	else if (ae_picker_search_editing(&screen.search))
	{
		/* (typing: no row has the bar; the row Enter or down goes to, the first, has the accent notch) */
		short focus = screen.list.list.focus;

		screen.list.list.focus = -1;
		ae_widget_list(&screen.density, &screen.list, left_x(), top, list_width, bottom - top, list_item, NULL, HIT_LIST);
		screen.list.list.focus = focus;
		if (focus >= screen.list.list.first && focus < screen.list.list.first + screen.list.list.rows)
		{
			short entry = focus < screen.count ? screen.rows[focus] : NO_ENTRY;
			float indent = entry >= 0 || ae_setup_map_row_is_recent(entry) ? u(INDENT_U) : 0.0f;

			ae_draw_rect(left_x() + indent, ae_list_view_rows_top(&screen.density, &screen.list, top) +
				(float)(focus - screen.list.list.first) * pitch(), u(NOTCH_U), ae_size_row(&screen.density), 0.0f,
				AE_COLOR_ACCENT);
		}
	}
	else
		ae_widget_list(&screen.density, &screen.list, left_x(), top, list_width, bottom - top, list_item, NULL, HIT_LIST);
	draw_panel(panel_x(), top, right_x() - panel_x(), bottom);
	draw_footer();
}

/* ---------- input */

static void map_update(struct ae_screen *instance)
{
	(void)instance;
	if (!ae_picker_search_keys(&screen.search))
		ae_sound_request(AE_SOUND_FAILURE, 0);
	/* (typed, or AE's keyboard: the list follows the text as it changes) */
	query_follow();
	if (!screen.facts_left)
		return;
	screen.facts_left = ae_setup_maps_read_facts(&maps, FACTS_PER_FRAME);
	refresh();
}

static int map_handle(struct ae_screen *instance, struct ae_event const *event)
{
	short entry = screen.focus_entry;

	(void)instance;
	/* (typing: Enter, START or down leaves it onto the results; Esc clears the text, then leaves; the filter still
	steps; the rest is the typing's) */
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
			ae_picker_search_clear(&screen.search);
			query_follow();
			ae_sound_request(AE_SOUND_BACK, 0);
			return 1;
		}
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
		short row;

		if (searching())
			return 1;
		row = ae_setup_map_group_step(screen.rows, screen.count, screen.list.list.focus,
			event->action == AE_ACTION_PAGE_DOWN ? 1 : -1);
		if (row < 0 || row >= screen.count || !ae_setup_map_row_is_group(screen.rows[row]))
			return 1;
		/* (the cursor only, the owner 10-30: nothing opens or closes; A opens) */
		screen.focus_entry = screen.rows[row];
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
		else if (entry == AE_SETUP_ROW_RECENT && maps.recent_folded)
			accept(entry);
		return 1;
	case AE_ACTION_LEFT:
		if (screen.open_group >= 0 && !searching())
		{
			screen.focus_entry = (short)(-1 - screen.open_group);
			folder_set(-1);
			ae_sound_request(AE_SOUND_BACK, 0);
		}
		return 1;
	case AE_ACTION_X:
		menu_open(entry);
		return 1;
	case AE_ACTION_Y:
		search_open(event->device);
		return 1;
	default:
		break;
	}
	if (ae_list_view_event(&screen.list, event))
		focus_taken(event->action == AE_ACTION_UP || event->action == AE_ACTION_PAGE_UP ? -1 : 1);
	return 1;
}

static void map_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
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
			map_handle(instance, &event);
		return;
	}
	if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit))
	{
		if (hit.id == HIT_CHIPS && hit.index >= 0 && hit.index < AE_MAP_FILTERS)
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
	}
	item = ae_list_view_pointer(&screen.list, pointer, HIT_LIST);
	focus_taken(1);
	if (item >= 0 && item < screen.count && screen.rows[item] != AE_SETUP_ROW_EMPTY)
		accept(screen.rows[item]);
}

static void map_leave(struct ae_screen *instance)
{
	(void)instance;
	ae_picker_search_leave(&screen.search);
	platform_log("ae change map: closed");
}

static struct ae_screen_class const map_class =
{
	.name = "change map", .leave = map_leave, .handle = map_handle, .draw = map_draw, .pointer = map_pointer,
	.update = map_update,
};

static int open_screen(int filter, const char *query, short current, void (*picked)(short list_index,
	char const *name))
{
	short last;
	int source;

	memset(&screen, 0, sizeof(screen));
	screen.picked = picked;
	screen.logged_entry = NO_ENTRY;
	screen.filter = filter >= 0 && filter < AE_MAP_FILTERS ? filter : AE_MF_ALL;
	ae_picker_search_init(&screen.search, "ae change map", query);
	snprintf(screen.query, sizeof(screen.query), "%s", screen.search.text.text);
	ae_setup_maps_load(&maps);
	maps.recent_folded = (short)(ae_recent_store_folded(AE_RECENT_BY_MAP) != 0);
	screen.facts_left = maps.count;
	recent_read();
	/* (the folder of the map given, else the map used last, open, the map focused) */
	last = current >= 0 ? current : ae_setup_maps_last_used();
	source = ae_setup_map_source(last);
	screen.open_group = source;
	if (last >= 0 && last < maps.count && ae_setup_map_suits(&maps.maps[last], screen.filter))
		screen.focus_entry = last;
	else
		screen.focus_entry = source >= 0 ? (short)(-1 - source) : (short)NO_ENTRY;
	ae_list_view_init(&screen.list, 0, 10);
	refresh();
	if (!ae_ui_push(&map_class, AE_OWNER_PARENT, &screen))
		return 0;
	platform_log("ae change map: open (filter %s)%s%s%s", ae_setup_map_filter_name(screen.filter),
		searching() ? ": search '" : "", searching() ? screen.query : "", searching() ? "'" : "");
	(void)ae_settings_ui_scale();
	return 1;
}

int ae_screen_change_map_open(int filter, const char *query)
{
	if (!open_screen(filter, query, -1, NULL))
		return 0;
	ae_ui_replace_menus();
	return 1;
}

int ae_screen_change_map_pick(int filter, short current, void (*picked)(short list_index, char const *name))
{
	return open_screen(filter, NULL, current, picked);
}
