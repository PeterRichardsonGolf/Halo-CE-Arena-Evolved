/*
AE_SCREEN_MAPS_HUB.C

MAPS & MODS (ae_screen_maps_hub.h): the flow screens' look (ae_setup_flow.h's title, rule, rows column of 46 %, a card from
1095 / 1920, the help line over the footer), a tab strip under the rule (ae_widget_tabs) and one page per tab. A page is a
column of controls (SEARCH, SHOW, SORT), the count line and a list on the left, a card on the right. The rules are
ae_maps_hub_rules.c's, INSTALLED's and REQUIRED FILES' lists ae_maps_hub_data.c's, MODS' ae_mods_list.h's and BROWSE CE MAPS'
ae_ce_archive.h's; a staged page (DEV MODE) draws made-up copies of all of them and touches none.
*/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/ae_platform.h"
#include "../src/ae_ce_archive.h"
#include "ae_glue_ce_archive.h"
#include "ae_glue_player_settings.h"
#include "ae_hooks.h"
#include "ae_input.h"
#include "ae_map_library_rules.h"
#include "ae_maps_hub_data.h"
#include "ae_mods_list.h"
#include "ae_screen_campaign.h"
#include "ae_screen_create_game.h"
#include "ae_screen_maps_hub.h"
#include "ae_screen_steam.h"
#include "ae_collections_data.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);
void platform_open_url(const char *url);
const char *platform_data_root(void);
int platform_folder_list(const char *path, void (*visit)(const char *name, void *context), void *context);
int platform_restart(void);
long config_integer(char const *name);
void ui_widget_port_multiplayer_maps_refresh(void);
short ae_campaign_level_of_map(const char *map);
int config_write(const char *name, const char *value);

enum
{
	HUB_HIT = 0x7C00,
	HIT_TABS = HUB_HIT + 1, HIT_CTL, HIT_LIST, HIT_PROMPTS, HIT_FIELD, HIT_POP, HIT_ACTION, HIT_CARD
};

/* the controls above a list: SEARCH, SHOW, SORT */
enum { CTL_SEARCH, CTL_SHOW, CTL_SORT, CTLS };
/* the rows of a page's own (REQUIRED FILES' doors, the consent card's buttons, MODS' disc) are focus ids past the list */
enum { OVERLAY_NONE, OVERLAY_OPTIONS, OVERLAY_FOLDER, OVERLAY_NAME };
enum { SORT_NAME, SORT_SIZE, SORT_SOURCE, SORTS };
enum { BROWSE_PAGE = 20, DISCS_MOST = 8, QUEUE_NOTE = 160 };

/* the most rows a popover lists (a map's options, or the maps folder + its folders + NEW FOLDER) */
enum { OVERLAY_ROWS = 12 };

struct overlay_row { char label[64], value[32]; int muted; };

static struct
{
	struct ae_flow_screen flow;
	struct ae_tabs tabs;
	struct ae_tab tab_defs[AE_HUB_TABS];
	int tab, state, staged;
	/* the focus: a control (CTL_*), or -1 the list */
	int ctl;
	/* the other things a page has the focus on: REQUIRED FILES' rows are its list; the consent card's two buttons (0 NOT NOW, 1 TURN ON) */
	int button;
	int show[AE_HUB_TABS], sort[AE_HUB_TABS];
	struct ae_text search;
	struct ae_field_edit edit;
	struct ae_rect well;
	unsigned long caret_since;
	int typing, keyboard;
	struct ae_list_view list;
	short list_count;
	/* INSTALLED */
	struct ae_hub_maps maps;
	int view[AE_MAP_LIBRARY_MAXIMUM];
	int view_count, view_key;
	/* the map of a row, and of the card (the library's, filled when asked) */
	struct ae_hub_map row_map, card_map;
	/* REQUIRED FILES */
	struct ae_hub_files files;
	/* BROWSE CE MAPS */
	int browse_page, browse_started;
	struct ae_ce_archive_status archive;
	struct ae_ce_archive_download download;
	int staged_progress;
	/* MODS */
	struct ae_mods_list mods;
	int mods_read, moving, moving_from;
	char discs[DISCS_MOST][48];
	int disc_count, disc_now;
	/* the popover over the page (the options of a map, the folder picker) */
	int overlay;
	struct overlay_row overlay_rows[OVERLAY_ROWS];
	int overlay_count, overlay_focus;
	char overlay_title[80];
	/* the help line, and a notice that replaces it for a while */
	char notice[200];
	int notice_warning;
	unsigned long notice_until;
	unsigned long opened;
	/* BROWSE's view kept from the query and the archive's serial it was made for; the install asked about; the folder name being typed */
	struct ae_ce_archive_query browse_asked;
	unsigned long long browse_serial;
	int browse_view_valid;
	char again_file[96], again_name[96];
	struct ae_text name_text;
	struct ae_field_edit name_edit;
	struct ae_rect name_well;
	int naming, naming_keyboard, device;
	unsigned long list_generation;
	int first_frame, pending_delete;
	/* the name of a Workshop map the client is removing (the notice waits for it to finish) */
	char removing_name[96];
	int logged_state;
} H;

static struct ae_screen_class const hub_class;

static float U(float spec_units)
{
	return ae_flow_u(&H.flow, spec_units);
}

/* a y position of the frame's 1080 units */
static float Y(float spec_units)
{
	return ae_frame_y(&H.flow.frame, spec_units / 1080.0f);
}

static unsigned long now_ms(void)
{
	return ae_motion_now();
}

static void notice_set(int warning, unsigned long ms, char const *format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	vsnprintf(H.notice, sizeof(H.notice), format, arguments);
	va_end(arguments);
	H.notice_warning = warning;
	H.notice_until = now_ms() + ms;
}

/* ---------- what a page has */

static int browse_real(void)
{
	return !H.staged;
}

/* a staged page's made-up pictures: a wide one for a Workshop map (16:9: fills the card's frame), a 560 x 456 one for a stock
map (loses a strip top and bottom); a gradient with a bright border and a cross, so a cut or a bar shows */
static int staged_picture(int wide, int *width, int *height)
{
	static int images[2];
	static unsigned char pixels[640 * 456 * 4];
	int w = wide ? 640 : 560, h = wide ? 360 : 456, x, y;

	*width = w;
	*height = h;
	if (images[wide] && ae_draw_image_live(images[wide]))
		return images[wide];
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++)
		{
			unsigned char *pixel = pixels + 4 * (y * w + x);
			int edge = x < 6 || y < 6 || x >= w - 6 || y >= h - 6 || (x > w / 2 - 3 && x < w / 2 + 3) || (y > h / 2 - 3 && y < h / 2 + 3);
			int mix = (x * 255 / w + y * 255 / h) / 2;

			pixel[0] = (unsigned char)(edge ? 232 : 40 + mix / 4);
			pixel[1] = (unsigned char)(edge ? 238 : 80 + mix / 2);
			pixel[2] = (unsigned char)(edge ? 242 : wide ? 150 + mix / 4 : 90 + mix / 3);
			pixel[3] = 255;
		}
	images[wide] = ae_draw_image_load(pixels, w, h);
	return images[wide];
}

/* the map of a place in the view: a staged page's own, else the library's filled into a scratch (card: with its picture) */
static struct ae_hub_map *map_at_for(int item, int card)
{
	struct ae_hub_map *scratch = card ? &H.card_map : &H.row_map;

	if (item < 0 || item >= H.view_count)
		return NULL;
	if (H.staged)
	{
		struct ae_hub_map *staged = &H.maps.maps[H.view[item]];

		if (card && (!staged->picture || !ae_draw_image_live(staged->picture)) && (staged->source == AE_HUB_SOURCE_STOCK || staged->workshop_item))
			staged->picture = staged_picture(staged->workshop_item != 0, &staged->picture_width, &staged->picture_height);
		return staged;
	}
	return ae_hub_data_fill(H.view[item], scratch, card) ? scratch : NULL;
}

static struct ae_hub_map *map_at(int item)
{
	return map_at_for(item, 0);
}

static struct ae_hub_map *focused_map(void)
{
	return H.tab == AE_HUB_TAB_INSTALLED && H.ctl < 0 ? map_at_for(H.list.list.focus, 1) : NULL;
}

/* the map a popover or a question was opened for: the list under it can be read again (a scan ends, a download lands)
while it is up, and the focus then names another map */
static char target_file[64], target_path[512];

static void target_keep(struct ae_hub_map const *map)
{
	snprintf(target_file, sizeof(target_file), "%s", map ? map->file : "");
	snprintf(target_path, sizeof(target_path), "%s", map ? map->path : "");
}

static int target_is(struct ae_hub_map const *map)
{
	return map && target_file[0] && !strcmp(target_file, map->file) && !strcmp(target_path, map->path);
}

/* the focus is no longer the map the popover or the question was opened for: nothing is done to either */
static void target_changed(void)
{
	H.overlay = OVERLAY_NONE;
	notice_set(1, 6000, "The list of maps changed. Nothing was done: choose the map again.");
	platform_log("ae maps hub: the list changed under a popover; nothing done");
	ae_sound_request(AE_SOUND_FAILURE, 0);
}

static int same_text(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if ((*a >= 'A' && *a <= 'Z' ? *a + 32 : *a) != (*b >= 'A' && *b <= 'Z' ? *b + 32 : *b))
			return 0;
	return *a == *b;
}

static int contains(char const *text, char const *needle)
{
	size_t length = strlen(needle), at;

	if (!length)
		return 1;
	for (at = 0; text[at]; at++)
	{
		size_t each;

		for (each = 0; each < length && text[at + each]; each++)
			if ((text[at + each] >= 'A' && text[at + each] <= 'Z' ? text[at + each] + 32 : text[at + each]) !=
				(needle[each] >= 'A' && needle[each] <= 'Z' ? needle[each] + 32 : needle[each]))
				break;
		if (each == length)
			return 1;
	}
	return 0;
}

static int compare_maps(struct ae_hub_map const *a, struct ae_hub_map const *b, int sort)
{
	int by_name = strcmp(a->name, b->name);

	switch (sort)
	{
	case SORT_SIZE:
		if (a->size != b->size)
			return a->size > b->size ? -1 : 1;
		return by_name;
	case SORT_SOURCE:
		if (a->source != b->source)
			return a->source - b->source;
		return by_name;
	}
	return by_name;
}

static void installed_view(void)
{
	int each, count = 0, place, sort = H.sort[AE_HUB_TAB_INSTALLED], show = H.show[AE_HUB_TAB_INSTALLED];
	int key;

	if (!H.staged)
	{
		/* (the library's query, asked again only when the words, SHOW, SORT or the list changed) */
		key = (int)ae_hub_data_generation() * 131 + show * 17 + sort * 5 + (int)strlen(H.search.text) * 1009;
		for (each = 0; H.search.text[each]; each++)
			key = key * 31 + H.search.text[each];
		if (key != H.view_key || ae_hub_data_scanning())
		{
			H.view_key = key;
			H.view_count = ae_hub_data_query(show, H.search.text, sort, H.view, AE_MAP_LIBRARY_MAXIMUM);
		}
		return;
	}
	for (each = 0; each < H.maps.count; each++)
	{
		struct ae_hub_map const *map = &H.maps.maps[each];

		if (!ae_hub_show_has(show, map->source))
			continue;
		if (H.search.text[0] && !contains(map->name, H.search.text) && !contains(map->file, H.search.text))
			continue;
		/* (insertion by the sort: a stable order for the few a page holds) */
		for (place = count; place > 0 && compare_maps(map, &H.maps.maps[H.view[place - 1]], sort) < 0; place--)
			H.view[place] = H.view[place - 1];
		H.view[place] = each;
		count++;
	}
	H.view_count = count;
}

static void list_set_count(int count)
{
	if (count != H.list_count)
	{
		H.list_count = (short)count;
		ae_list_set_count(&H.list.list, (short)count);
		if (H.list.list.focus < 0 && count > 0)
			ae_list_set_focus(&H.list.list, 0);
	}
}

/* ---------- MODS' list: a staged page keeps its own, the real one is the engine's */

static void mods_stage(void)
{
	static struct { char const *name, *title, *version, *authors, *description, *maps; int on; long files[AE_MODS_GROUPS]; } const made_up[] =
	{
		{ "hd_audio", "HD Audio", "2.0", "A made-up author", "Sharper sounds for every map.", "", 1, { 1204, 0, 18, 0, 0 } },
		{ "sharper_hud", "Sharper HUD", "1.2", "A made-up author", "A clearer set of HUD pictures.", "", 1, { 0, 312, 0, 0, 0 } },
		{ "new_skies", "New Skies", "1.0", "A made-up author", "Skies for the maps that show one.", "", 1, { 0, 96, 0, 0, 0 } },
		{ "retro_guns", "Retro Guns", "0.9", "A made-up author", "The old weapon models.", "", 1, { 12, 0, 0, 38, 0 } },
		{ "bright_text", "Bright Text", "1.1", "A made-up author", "Larger, clearer menu text.", "", 0, { 0, 0, 44, 0, 0 } },
		{ "old_maps_only", "Old Maps Pack", "1.0", "A made-up author", "Pictures for a map pack this PC does not have.", "coldwater", 0, { 0, 20, 0, 0, 0 } },
	};
	int each, count = (int)(sizeof(made_up) / sizeof(made_up[0]));

	memset(&H.mods, 0, sizeof(H.mods));
	for (each = 0; each < count; each++)
	{
		struct ae_mods_item *item = &H.mods.items[each];
		int group;

		snprintf(item->name, sizeof(item->name), "%s", made_up[each].name);
		snprintf(item->title, sizeof(item->title), "%s", made_up[each].title);
		snprintf(item->version, sizeof(item->version), "%s", made_up[each].version);
		snprintf(item->authors, sizeof(item->authors), "%s", made_up[each].authors);
		snprintf(item->description, sizeof(item->description), "%s", made_up[each].description);
		snprintf(item->maps, sizeof(item->maps), "%s", made_up[each].maps);
		item->on = made_up[each].on;
		item->priority = each + 1;
		item->position = count - 1 - each;
		item->state = item->on ? AE_MODS_STATE_ON : AE_MODS_STATE_OFF;
		item->files_known = 1;
		for (group = 0; group < AE_MODS_GROUPS; group++)
			item->files[group] = made_up[each].files[group];
		if (!strcmp(made_up[each].name, "sharper_hud"))
		{
			/* (asks to load after HD Audio, sits above it) */
			snprintf(item->note, sizeof(item->note), "It asks to load after HD Audio, but sits above it: Y, then down, moves it under HD Audio.");
		}
		if (!strcmp(made_up[each].name, "old_maps_only"))
		{
			item->state = AE_MODS_STATE_REFUSED;
			snprintf(item->reason, sizeof(item->reason), "NO MAP OF IT HERE");
		}
	}
	H.mods.count = count;
	H.mods.active = 1;
}

static void mods_read(void)
{
	if (H.staged)
	{
		if (!H.mods.count)
			mods_stage();
	}
	else
		ae_mods_list_read(ae_mods_list_engine(), &H.mods);
	H.mods_read = 1;
}

static int mods_on_count(void)
{
	int each, on = 0;

	for (each = 0; each < H.mods.count; each++)
		on += H.mods.items[each].on;
	return on;
}

static void mods_turn(int index)
{
	struct ae_mods_item *item = &H.mods.items[index];
	char answer[200];

	if (item->state == AE_MODS_STATE_REFUSED)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		notice_set(1, 4000, "%s can't be turned on here: %s.", item->title[0] ? item->title : item->name, item->reason);
		return;
	}
	if (H.staged)
	{
		item->on = !item->on;
		item->state = item->on ? AE_MODS_STATE_ON : AE_MODS_STATE_OFF;
		platform_log("ae maps hub: staged: mod %s turned %s", item->name, item->on ? "on" : "off");
	}
	else
	{
		ae_mods_list_turn(ae_mods_list_engine(), item->name, !item->on, answer, sizeof(answer));
		platform_log("ae maps hub: mods: %s", answer);
		mods_read();
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

/* the mod at list index moved one place (-1 up: plays over more) */
static void mods_shift(int index, int direction)
{
	long priority = H.mods.items[index].priority + direction;
	int swap, each;

	if (priority < 1 || priority > H.mods.count)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (H.staged)
	{
		struct ae_mods_item moved;

		swap = index + direction;
		moved = H.mods.items[index];
		H.mods.items[index] = H.mods.items[swap];
		H.mods.items[swap] = moved;
		for (each = 0; each < H.mods.count; each++)
			H.mods.items[each].priority = each + 1;
		ae_list_set_focus(&H.list.list, (short)swap);
		platform_log("ae maps hub: staged: mod moved to place %d", swap + 1);
	}
	else
	{
		char answer[200];

		ae_mods_list_order(ae_mods_list_engine(), H.mods.items[index].name, priority, answer, sizeof(answer));
		platform_log("ae maps hub: mod moved to priority %ld", priority);
		mods_read();
		for (each = 0; each < H.mods.count; each++)
			if (H.mods.items[each].priority == priority)
				ae_list_set_focus(&H.list.list, (short)each);
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

/* ---------- the modded disc (SETTINGS > SYSTEM > FILES > MODDED DISC's setting) */

static void disc_seen(const char *name, void *context)
{
	int each;

	(void)context;
	if (H.disc_count >= DISCS_MOST - 1 || name[0] == '.')
		return;
	for (each = 0; each < H.disc_count; each++)
		if (!strcmp(H.discs[each], name))
			return;
	snprintf(H.discs[H.disc_count++], sizeof(H.discs[0]), "%s", name);
}

static void discs_read(void)
{
	char folder[1100], current[48];
	int each;

	H.disc_count = 0;
	H.disc_now = 0;
	if (H.staged)
	{
		snprintf(H.discs[H.disc_count++], sizeof(H.discs[0]), "NHE");
		snprintf(H.discs[H.disc_count++], sizeof(H.discs[0]), "CE PLUS");
		return;
	}
	snprintf(folder, sizeof(folder), "%s/discs", platform_data_root());
	platform_folder_list(folder, disc_seen, NULL);
	snprintf(folder, sizeof(folder), "%s/mods", platform_data_root());
	platform_folder_list(folder, disc_seen, NULL);
	current[0] = 0;
	ae_player_setting_get(0, "game.disc", current, sizeof(current));
	for (each = 0; each < H.disc_count; each++)
		if (!strcmp(H.discs[each], current))
			H.disc_now = each + 1;
}

static char const *disc_text(char *text, int size)
{
	int each;

	if (H.disc_now <= 0)
		return "NONE";
	snprintf(text, (size_t)size, "%s", H.discs[H.disc_now - 1]);
	for (each = 0; text[each]; each++)
		if (text[each] >= 'a' && text[each] <= 'z')
			text[each] = (char)(text[each] - 32);
	return text;
}

static void disc_step(int direction)
{
	int next = H.disc_now + direction;

	if (H.disc_count == 0)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		notice_set(0, 4000, "No modded disc is on this PC.");
		return;
	}
	if (next < 0)
		next = H.disc_count;
	if (next > H.disc_count)
		next = 0;
	H.disc_now = next;
	if (!H.staged)
	{
		char const *value = next ? H.discs[next - 1] : "";

		if (!ae_player_setting_put(0, "game.disc", value))
			config_write("game.disc", value);
	}
	H.state = AE_HUB_STATE_RESTART;
	ae_sound_request(AE_SOUND_CURSOR, 0);
	platform_log("ae maps hub: modded disc %s", next ? H.discs[next - 1] : "none");
}

/* ---------- BROWSE CE MAPS' list: the archive's, or a staged page's made-up one */

static struct { char const *name; unsigned int megabytes; } const staged_archive[] =
{
	{ "Arcadia Ridge", 41 }, { "Ashfall", 22 }, { "Bastion Point", 36 }, { "Coldwater", 38 }, { "Crossfire Canyon", 27 },
	{ "Deadlock Keep", 31 }, { "Dune Sea", 44 }, { "Emberfall", 19 }, { "Fallow Bridge", 27 }, { "Glasshouse", 22 },
	{ "Harbor Light", 45 }, { "Ironwood", 33 }, { "Juniper Pass", 48 }, { "Kestrel Court", 19 }, { "Lowland", 24 },
	{ "Mirror Lake", 30 }, { "Northgate", 35 }, { "Obsidian", 26 }, { "Pale Harbor", 28 }, { "Quarry Run", 21 },
};
#define STAGED_ARCHIVE_COUNT 20

static int browse_total(void)
{
	return browse_real() ? H.archive.view_count : STAGED_ARCHIVE_COUNT;
}

/* the row at an absolute place of the view: its name as shown, its size, whether this PC has it */
static int browse_item(int place, char *name, int name_size, unsigned long long *size, int *installed, int *update)
{
	*update = 0;
	if (!browse_real())
	{
		if (place < 0 || place >= STAGED_ARCHIVE_COUNT)
			return 0;
		snprintf(name, (size_t)name_size, "%s", staged_archive[place].name);
		*size = (unsigned long long)staged_archive[place].megabytes * 1024ull * 1024ull;
		*installed = place == 0 || place == 2;
		*update = place == 2;
		return 1;
	}
	{
		struct ae_ce_archive_row row;
		size_t length;

		if (!ae_ce_archive_row(place, &row))
			return 0;
		snprintf(name, (size_t)name_size, "%s", row.file);
		length = strlen(name);
		if (length > 4 && !strcmp(name + length - 4, ".map"))
			name[length - 4] = 0;
		*size = row.size;
		*installed = row.installed;
		return 1;
	}
}

static int browse_downloading(void)
{
	return browse_real() ? H.download.state == AE_CE_ARCHIVE_DOWNLOAD_RUNNING : H.state == AE_HUB_STATE_DOWNLOAD;
}

/* the value of a row of the page: its words, and a kind (0 plain, 1 muted, 2 accent, 3 warning) */
static void browse_value(int place, char *text, int size, int *kind)
{
	char name[96];
	unsigned long long bytes;
	int installed, update;
	char file[AE_CE_ARCHIVE_FILE_SIZE];

	text[0] = 0;
	*kind = 0;
	if (!browse_item(place, name, sizeof(name), &bytes, &installed, &update))
		return;
	if (!browse_real())
	{
		if (H.state == AE_HUB_STATE_DOWNLOAD && place == 3)
		{
			snprintf(text, (size_t)size, "%d %%", H.staged_progress);
			*kind = 2;
		}
		else if (H.state == AE_HUB_STATE_DOWNLOAD && (place == 4 || place == 5))
		{
			snprintf(text, (size_t)size, "WAITING");
			*kind = 1;
		}
		else if (H.state == AE_HUB_STATE_DISK_FULL && place == 3)
		{
			snprintf(text, (size_t)size, "NO ROOM");
			*kind = 3;
		}
		else if (H.state == AE_HUB_STATE_FAILED && place == 3)
		{
			snprintf(text, (size_t)size, "FAILED");
			*kind = 3;
		}
		else if (update)
		{
			snprintf(text, (size_t)size, "UPDATE");
			*kind = 2;
		}
		else if (installed)
		{
			snprintf(text, (size_t)size, "INSTALLED");
			*kind = 1;
		}
		return;
	}
	snprintf(file, sizeof(file), "%s.map", name);
	if (H.download.file[0] && same_text(H.download.file, file))
	{
		if (H.download.state == AE_CE_ARCHIVE_DOWNLOAD_RUNNING)
		{
			snprintf(text, (size_t)size, "%d %%", H.download.total ? (int)(H.download.done * 100ull / H.download.total) : 0);
			*kind = 2;
			return;
		}
		if (H.download.state == AE_CE_ARCHIVE_DOWNLOAD_FAILED)
		{
			snprintf(text, (size_t)size, "FAILED");
			*kind = 3;
			return;
		}
	}
	if (installed)
	{
		snprintf(text, (size_t)size, "INSTALLED");
		*kind = 1;
	}
}

static void browse_query(void)
{
	struct ae_ce_archive_query query;

	memset(&query, 0, sizeof(query));
	snprintf(query.text, sizeof(query.text), "%s", H.search.text);
	query.sort = H.sort[AE_HUB_TAB_BROWSE] == 1 ? AE_CE_ARCHIVE_SORT_SIZE : AE_CE_ARCHIVE_SORT_NAME;
	query.show = H.show[AE_HUB_TAB_BROWSE] == 1 ? AE_CE_ARCHIVE_SHOW_INSTALLED : H.show[AE_HUB_TAB_BROWSE] == 2 ?
		AE_CE_ARCHIVE_SHOW_MISSING : AE_CE_ARCHIVE_SHOW_ALL;
	/* (the whole query and the whole serial are compared: the view is made again when either differs) */
	if (!H.browse_view_valid || memcmp(&query, &H.browse_asked, sizeof(query)) || H.archive.serial != H.browse_serial)
	{
		H.browse_asked = query;
		H.browse_view_valid = 1;
		ae_ce_archive_query(&query);
		ae_ce_archive_get_status(&H.archive);
		H.browse_serial = H.archive.serial;
		H.browse_page = 0;
	}
}

/* every list that shows what is installed asked again: after a map is deleted, moved or installed */
static void lists_changed(void)
{
	ui_widget_port_multiplayer_maps_refresh();
	if (ae_ce_archive_available())
		ae_ce_archive_rescan();
	H.view_key = -1;
	H.browse_view_valid = 0;
}

/* a download that was running and is done: its map is on this PC, so the lists are asked again and the result forgotten */
static void download_watch(int notify)
{
	static int last_state;

	if (last_state == AE_CE_ARCHIVE_DOWNLOAD_RUNNING && H.download.state == AE_CE_ARCHIVE_DOWNLOAD_DONE)
	{
		char name[96];
		size_t length;

		snprintf(name, sizeof(name), "%s", H.download.file);
		length = strlen(name);
		if (length > 4 && !strcmp(name + length - 4, ".map"))
			name[length - 4] = 0;
		lists_changed();
		if (notify)
			notice_set(0, 6000, "%s installed: it is on INSTALLED.", name);
		platform_log("ae maps hub: installed %s", H.download.file);
		ae_ce_archive_download_reset();
		H.download.state = AE_CE_ARCHIVE_DOWNLOAD_NONE;
	}
	last_state = H.download.state;
}

/* the page's state from the archive's: asked every frame, the list and the download read once */
static void browse_sync(void)
{
	if (!browse_real())
		return;
	if (!ae_ce_archive_available())
	{
		H.state = AE_HUB_STATE_OFFLINE;
		return;
	}
	if (!ae_ce_archive_enabled() || !ae_ce_archive_consented())
	{
		H.state = AE_HUB_STATE_CONSENT;
		return;
	}
	if (!H.browse_started)
	{
		char why[160];

		H.browse_started = 1;
		if (!ae_ce_archive_start(why, sizeof(why)))
			platform_log("ae maps hub: the archive did not start: %s", why);
	}
	ae_glue_ce_archive_poll();
	ae_ce_archive_get_status(&H.archive);
	ae_ce_archive_download_status(&H.download);
	download_watch(1);
	if (H.archive.state == AE_CE_ARCHIVE_FAILED)
		H.state = AE_HUB_STATE_OFFLINE;
	else if (H.archive.state != AE_CE_ARCHIVE_READY)
		H.state = AE_HUB_STATE_LOADING;
	else
	{
		browse_query();
		H.state = H.download.state == AE_CE_ARCHIVE_DOWNLOAD_RUNNING ? AE_HUB_STATE_DOWNLOAD :
			H.download.state == AE_CE_ARCHIVE_DOWNLOAD_FAILED ?
			(strstr(H.download.error, "room") || strstr(H.download.error, "free") || strstr(H.download.error, "space") ?
			AE_HUB_STATE_DISK_FULL : AE_HUB_STATE_FAILED) : AE_HUB_STATE_RESULTS;
	}
}

/* ---------- drawing parts */

static void text_shadow(int font, float size, float x, float y, int align, unsigned int color, char const *text)
{
	ae_draw_text(font, size, x + U(2.0f), y + U(2.0f), align, 0x99000000u, text);
	ae_draw_text(font, size, x, y, align, color, text);
}

/* a card: the panel, its outline and the 4 u stripe (accent, or WARNING for a page that reports a fault) */
static void card_frame(float x, float y, float width, float height, int warning)
{
	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_PANEL);
	ae_draw_outline(x, y, width, height, 0.0f, U(1.0f), AE_COLOR_RULE);
	ae_draw_rect(x, y, width, U(4.0f), 0.0f, warning ? AE_COLOR_WARNING : AE_COLOR_ACCENT);
}

/* a label / value line of a card: the label tracked and muted on the left, the value on the right, a rule under it */
static float card_fact(float x, float y, float width, char const *label, char const *value, unsigned int color)
{
	struct ae_density const *d = &H.flow.density;
	float inset = U(AE_FLOW_INSET_U), label_size = ae_size_minor(d) * 0.84f, value_size = ae_size_text(d) * 0.83f;
	char fitted[160];
	float label_width = ae_draw_text_tracked_width(AE_FONT_BODY, label_size, 0.09f, label);

	ae_draw_text_tracked(AE_FONT_BODY, label_size, 0.09f, x + inset, y + U(4.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	ae_fit_text(AE_FONT_ROW, value_size, value, width - 2.0f * inset - label_width - U(20.0f), fitted, sizeof(fitted));
	ae_draw_text(AE_FONT_ROW, value_size, x + width - inset, y, AE_ALIGN_RIGHT, color, fitted);
	ae_draw_rect(x + inset, y + U(31.0f), width - 2.0f * inset, U(1.0f), 0.0f, AE_COLOR_RULE);
	return U(40.0f);
}

/* wrapped body text in a card: up to `lines` lines; returns the height used */
static float card_body(float x, float y, float width, char const *text, int lines, unsigned int color)
{
	struct ae_density const *d = &H.flow.density;
	float inset = U(AE_FLOW_INSET_U), body = ae_size_text(d) * 0.88f, line = U(31.0f);
	char wrapped[8][160];
	int count, each;

	count = ae_wrap_text(AE_FONT_BODY, body, text, width - 2.0f * inset, wrapped, lines > 8 ? 8 : lines);
	for (each = 0; each < count; each++)
		ae_draw_text(AE_FONT_BODY, body, x + inset, y + line * (float)each, AE_ALIGN_LEFT, color, wrapped[each]);
	return line * (float)count;
}

/* a card's small section label */
static float card_label(float x, float y, char const *label)
{
	struct ae_density const *d = &H.flow.density;

	ae_draw_text_tracked(AE_FONT_BODY, ae_size_minor(d) * 0.84f, 0.09f, x + U(AE_FLOW_INSET_U), y, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	return U(34.0f);
}

/* the card's picture box: the plain box, with its words, as wide as the card's inside and at most 260 u tall, a wide picture's proportions (the whole picture in it, its proportions kept) */
static float card_picture(float x, float y, float width, char const *words, int image, int image_width, int image_height)
{
	struct ae_density const *d = &H.flow.density;
	float inset = U(AE_FLOW_INSET_U), height = U(260.0f), size = ae_size_minor(d) * 0.9f, box = width - 2.0f * inset;

	/* (every map's picture in the same 16:9 frame; one too tall for the card is narrowed to it) */
	if (ae_cover_frame_height(box) > height)
		box = height * 16.0f / 9.0f;
	height = ae_cover_frame_height(box);
	/* (centred in the card's inside) */
	inset += (width - 2.0f * inset - box) * 0.5f;
	ae_draw_rect(x + inset, y, box, height, 0.0f, AE_COLOR_ROW);
	if (image && image_width > 0 && image_height > 0)
	{
		ae_draw_image_fill(image, x + inset, y, box, height, 0xFFFFFFFFu);
	}
	else if (words)
		ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x + width * 0.5f, y + height * 0.5f - ae_draw_cap_height(AE_FONT_BODY, size) * 0.5f,
			AE_ALIGN_CENTER, AE_COLOR_MUTED, words);
	return height;
}

static float card_title(float x, float y, float width, char const *text)
{
	char fitted[100];
	float inset = U(AE_FLOW_INSET_U);

	ae_fit_text(AE_FONT_TITLE, U(40.0f), text, width - 2.0f * inset, fitted, sizeof(fitted));
	ae_draw_text(AE_FONT_TITLE, U(40.0f), x + inset, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, fitted);
	return U(58.0f);
}

static void upper_copy(char *to, int size, char const *from)
{
	int each;

	for (each = 0; each < size - 1 && from[each]; each++)
		to[each] = from[each] >= 'a' && from[each] <= 'z' ? (char)(from[each] - 32) : from[each];
	to[each] = 0;
}

/* a row of a list or a control: label and value, the value in a colour when it says something (kind: 1 muted, 2 accent,
3 WARNING); the row's hits are the widget's */
static float page_row(float x, float y, float width, short hit_id, short index, char const *label, char const *value, int kind,
	unsigned int flags)
{
	struct ae_density const *d = &H.flow.density;
	struct ae_row row;
	char fitted[120];
	float height, value_width = 0.0f, size = ae_size_text(d);
	int on_bar = (flags & AE_ROW_FOCUSED) != 0;

	memset(&row, 0, sizeof(row));
	if (kind && value && value[0])
	{
		/* (the row draws the label; the value is ours, in its colour, right-aligned inside the row's padding) */
		value_width = ae_draw_text_width(AE_FONT_ROW, size, value);
		ae_fit_text(AE_FONT_ROW, size, label, width - value_width - U(60.0f), fitted, sizeof(fitted));
		row.label = fitted;
		row.value = NULL;
		row.flags = flags | AE_ROW_STATUS;
		height = ae_widget_row(d, x, y, width, &row, hit_id, index);
		ae_draw_text(AE_FONT_ROW, size, x + width - U(20.0f), ae_flow_cap_top(AE_FONT_ROW, size, y + height * 0.5f), AE_ALIGN_RIGHT,
			on_bar ? (kind == 3 ? AE_COLOR_WARNING : AE_COLOR_SELECTION_TEXT) :
			kind == 3 ? AE_COLOR_WARNING : kind == 2 ? AE_COLOR_ACCENT : AE_COLOR_MUTED, value);
		return height;
	}
	row.label = label;
	row.value = value && value[0] ? value : NULL;
	row.flags = flags;
	if (row.value && !(flags & (AE_ROW_LINK | AE_ROW_SUBSCREEN)) && !(flags & AE_ROW_FOCUSED) && 0)
		row.flags |= AE_ROW_STATUS;
	return ae_widget_row(d, x, y, width, &row, hit_id, index);
}

static void count_line(float x, float y, float width, char const *left, char const *right)
{
	struct ae_density const *d = &H.flow.density;
	float size = ae_size_minor(d) * 0.9f;

	ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x + U(11.0f), y + U(1.5f), AE_ALIGN_LEFT, 0x99000000u, left);
	ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x + U(10.0f), y, AE_ALIGN_LEFT, AE_COLOR_MUTED, left);
	if (right && right[0])
	{
		ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x + width - U(9.0f), y + U(1.5f), AE_ALIGN_RIGHT, 0x99000000u, right);
		ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x + width - U(10.0f), y, AE_ALIGN_RIGHT, AE_COLOR_MUTED, right);
	}
}

/* the search field: label over a well; typing edits the text the lists are narrowed by */
static float search_field(float x, float y, float width, char const *placeholder)
{
	struct ae_field field;

	memset(&field, 0, sizeof(field));
	field.label = "SEARCH";
	field.placeholder = placeholder;
	field.text = &H.search;
	field.flags = (H.ctl == CTL_SEARCH ? AE_FIELD_FOCUSED : 0) | (H.typing || H.keyboard ? AE_FIELD_EDITING : 0);
	field.caret_since = H.caret_since;
	ae_field_well(&H.flow.density, x, y, width, &field, &H.well);
	return ae_widget_field(&H.flow.density, x, y, width, &field, HIT_FIELD);
}

/* the SHOW and SORT rows */
static float show_sort_rows(float x, float y, float width, char const *show_value, char const *sort_value)
{
	float start = y;

	y += page_row(x, y, width, HIT_CTL, CTL_SHOW, "SHOW", show_value, 0, H.ctl == CTL_SHOW ? AE_ROW_FOCUSED : 0) + U(6.0f);
	y += page_row(x, y, width, HIT_CTL, CTL_SORT, "SORT", sort_value, 0, AE_ROW_LINK | (H.ctl == CTL_SORT ? AE_ROW_FOCUSED : 0)) + U(6.0f);
	return y - start;
}

struct list_context { int kind; };

static void list_item(void *context, short item, float x, float y, float width, float height, unsigned int flags);

/* the list of a page under its count line: count items, the page's own rows */
static void page_list(float x, float y, float width, int kind, int count)
{
	struct list_context context;
	float bottom = ae_flow_content_bottom(&H.flow) - U(96.0f);

	context.kind = kind;
	list_set_count(count);
	if (H.ctl >= 0)
		H.list.hover = -1;
	ae_widget_list(&H.flow.density, &H.list, x, y, width, bottom - y, list_item, &context, HIT_LIST);
}

/* ---------- INSTALLED */

static void installed_card(float x, float y, float width, float height)
{
	struct ae_hub_map const *map = map_at_for(H.list.list.focus, 1);
	char size[32], line[200], text[200], folder[120];
	float inset = U(AE_FLOW_INSET_U), cy;
	int warning;

	if (!map)
	{
		card_frame(x, y, width, height, 0);
		return;
	}
	warning = map->in_use || map->unreadable;
	card_frame(x, y, width, height, 0);
	cy = y + U(24.0f);
	cy += card_picture(x, cy, width, map->picture ? NULL : "NO PICTURE IN THIS MAP", map->picture, map->picture_width, map->picture_height) + U(22.0f);
	cy += card_title(x, cy, width, map->name);
	cy += U(12.0f);
	ae_hub_size_text(map->size, size, sizeof(size));
	ae_hub_folder_label(map->path, folder, sizeof(folder));
	snprintf(line, sizeof(line), "%s%s%s", ae_hub_map_source_name(map->source, map->list_name), folder[0] ? " \xE2\x80\xA2 " : "",
		folder);
	cy += card_fact(x, cy, width, "SOURCE", line, AE_COLOR_TITLE);
	cy += card_fact(x, cy, width, "SIZE", size, AE_COLOR_TITLE);
	if (map->plays[0])
		cy += card_fact(x, cy, width, "PLAYS", map->plays, AE_COLOR_TITLE);
	if (map->added[0])
		cy += card_fact(x, cy, width, "ADDED", map->added, AE_COLOR_TITLE);
	cy += U(12.0f);
	if (map->unreadable)
		snprintf(text, sizeof(text), "This file isn't a map AE can load (wrong version or damaged). DELETE removes it.");
	else
		snprintf(text, sizeof(text), "%s", map->description);
	if (text[0])
		cy += card_body(x, cy, width, text, 2, map->unreadable ? AE_COLOR_WARNING : AE_COLOR_TEXT);
	if (map->in_use)
		card_body(x, y + height - U(24.0f) - U(62.0f), width, "The lobby is on this map: it can't be moved or deleted until you leave the lobby.", 2,
			AE_COLOR_WARNING);
	(void)inset;
	(void)warning;
}

static void draw_installed(float x, float y, float width, float card_x, float card_width, float height)
{
	struct ae_density const *d = &H.flow.density;
	char left[96], right[40], number[32], show_text[48];

	installed_view();
	y += search_field(x, y, width, "Search your maps") + U(18.0f);
	y += show_sort_rows(x, y, width, ae_hub_show_name(H.show[AE_HUB_TAB_INSTALLED]),
		H.sort[AE_HUB_TAB_INSTALLED] == SORT_NAME ? "NAME" : H.sort[AE_HUB_TAB_INSTALLED] == SORT_SIZE ? "SIZE" : "SOURCE") + U(10.0f);
	if (!H.staged)
		ae_hub_data_update();
	upper_copy(show_text, sizeof(show_text), H.show[AE_HUB_TAB_INSTALLED] == AE_HUB_SHOW_ALL ? "ALL SOURCES" :
		ae_hub_show_name(H.show[AE_HUB_TAB_INSTALLED]));
	if (H.state == AE_HUB_STATE_LOADING || H.maps.loading || (!H.staged && ae_hub_data_scanning() && !ae_hub_data_ready()))
	{
		count_line(x, y, width, "READING THE MAP FOLDERS...", "");
		y += U(32.0f);
		{
			int row;

			for (row = 0; row < 5; row++)
				ae_draw_rect(x, y + (float)row * U(56.0f), width, U(50.0f), 0.0f, AE_COLOR_ROW);
		}
		card_frame(card_x, Y(252.0f), card_width, height, 0);
		return;
	}
	ae_hub_count_text(H.view_count, number, sizeof(number));
	if (H.show[AE_HUB_TAB_INSTALLED] == AE_HUB_SHOW_ALL && !H.search.text[0])
		ae_hub_count_text(H.staged ? H.maps.total_count : ae_hub_data_total_count(), number, sizeof(number));
	snprintf(left, sizeof(left), "%s \xE2\x80\xA2 %s MAPS", show_text, number);
	ae_hub_size_text(H.staged ? H.maps.total_bytes : ae_hub_data_total_bytes(), right, sizeof(right));
	count_line(x, y, width, left, (H.staged ? H.maps.total_count : ae_hub_data_total_count()) ? right : "");
	y += U(32.0f);
	if (H.view_count == 0)
	{
		char const *sentence = H.show[AE_HUB_TAB_INSTALLED] == AE_HUB_SHOW_WORKSHOP ?
			"No Workshop maps on this PC yet. The WORKSHOP page finds and installs them." :
			H.show[AE_HUB_TAB_INSTALLED] == AE_HUB_SHOW_HOSTS ? "Maps hosts sent you show here." :
			H.show[AE_HUB_TAB_INSTALLED] == AE_HUB_SHOW_MCC ?
			"MCC isn't on this PC. STEAM ACCOUNT downloads MCC's shared files and Workshop maps with a Steam account that owns MCC." :
			H.search.text[0] ? "No map matches that search." :
			H.show[AE_HUB_TAB_INSTALLED] == AE_HUB_SHOW_COMMUNITY ?
			"No community Xbox maps in your maps folders." :
			"No Custom Edition maps yet. BROWSE CE MAPS installs them, or copy .map files into the maps_ce folder (Y opens it).";
		char wrapped[4][160];
		int count = ae_wrap_text(AE_FONT_BODY, ae_size_text(d) * 0.9f, sentence, width - U(20.0f), wrapped, 4), each;

		for (each = 0; each < count; each++)
			ae_draw_text(AE_FONT_BODY, ae_size_text(d) * 0.9f, x + U(10.0f), y + U(8.0f) + U(31.0f) * (float)each, AE_ALIGN_LEFT,
				AE_COLOR_TEXT, wrapped[each]);
		y += U(8.0f) + U(31.0f) * (float)count + U(20.0f);
		if (H.show[AE_HUB_TAB_INSTALLED] == AE_HUB_SHOW_WORKSHOP && !H.search.text[0])
			page_row(x, y, width, HIT_ACTION, 0, "GET WORKSHOP MAPS", NULL, 0, AE_ROW_SUBSCREEN | AE_ROW_FOCUSED);
		card_frame(card_x, Y(252.0f), card_width, height, 0);
		return;
	}
	page_list(x, y, width, AE_HUB_TAB_INSTALLED, H.view_count);
	installed_card(card_x, Y(252.0f), card_width, height);
}

/* ---------- REQUIRED FILES */

static void required_card(float x, float y, float width, float height)
{
	struct ae_hub_file const *file = H.list.list.focus >= 0 && H.list.list.focus < H.files.count ? &H.files.files[H.list.list.focus] : NULL;
	char size[32];
	float cy;
	int missing;

	if (!file)
	{
		card_frame(x, y, width, height, 0);
		return;
	}
	missing = file->state == AE_HUB_FILE_MISSING;
	card_frame(x, y, width, height, missing);
	cy = y + U(26.0f);
	cy += card_title(x, cy, width, file->name);
	if (file->state != AE_HUB_FILE_COUNT_ONLY)
	{
		ae_draw_text_tracked(AE_FONT_BODY, ae_size_minor(&H.flow.density) * 0.95f, 0.09f, x + U(AE_FLOW_INSET_U), cy, AE_ALIGN_LEFT,
			missing ? AE_COLOR_WARNING : file->state == AE_HUB_FILE_ABSENT ? AE_COLOR_MUTED : AE_COLOR_ACCENT,
			missing ? "MISSING" : file->state == AE_HUB_FILE_ABSENT ? "NOT FOUND" : "FOUND");
		cy += U(46.0f);
	}
	if (!missing && file->from[0])
	{
		cy += card_fact(x, cy, width, "FROM", file->from, AE_COLOR_TITLE);
		if (file->size)
		{
			ae_hub_size_text(file->size, size, sizeof(size));
			cy += card_fact(x, cy, width, "SIZE", size, AE_COLOR_TITLE);
		}
		cy += U(8.0f);
	}
	cy += card_label(x, cy, "WHAT IT UNLOCKS");
	cy += card_body(x, cy, width, file->unlocks, 4, AE_COLOR_TEXT) + U(14.0f);
	if (missing)
	{
		/* the two doors of a missing file, each with its button */
		float inset = U(AE_FLOW_INSET_U), size_row = ae_size_text(&H.flow.density) * 0.9f, glyph = ae_size_glyph(&H.flow.density);
		float gw;

		cy += card_label(x, cy, "GET IT");
		gw = ae_draw_button(AE_FONT_LAST_DEVICE, AE_BUTTON_A, glyph, x + inset, cy, AE_COLOR_TITLE);
		ae_draw_text(AE_FONT_ROW, size_row, x + inset + gw + U(12.0f), ae_flow_cap_top(AE_FONT_ROW, size_row, cy + glyph * 0.5f),
			AE_ALIGN_LEFT, AE_COLOR_TITLE, "STEAM ACCOUNT \xE2\x80\xBA");
		ae_draw_text(AE_FONT_BODY, ae_size_minor(&H.flow.density) * 0.9f, x + width - inset,
			ae_flow_cap_top(AE_FONT_BODY, ae_size_minor(&H.flow.density) * 0.9f, cy + glyph * 0.5f), AE_ALIGN_RIGHT, AE_COLOR_MUTED, "MCC'S COPY");
		cy += glyph + U(14.0f);
		gw = ae_draw_button(AE_FONT_LAST_DEVICE, AE_BUTTON_Y, glyph, x + inset, cy, AE_COLOR_TITLE);
		ae_draw_text(AE_FONT_ROW, size_row, x + inset + gw + U(12.0f), ae_flow_cap_top(AE_FONT_ROW, size_row, cy + glyph * 0.5f),
			AE_ALIGN_LEFT, AE_COLOR_TITLE, "OPEN THE MAPS FOLDER");
		cy += glyph + U(18.0f);
		card_body(x, cy, width, "Or copy it from a Custom Edition install's maps folder into this game's maps folder.", 2, AE_COLOR_MUTED);
	}
	else if (!H.files.missing)
		card_body(x, y + height - U(24.0f) - U(62.0f), width, "Every file Custom Edition and MCC maps need is on this PC.", 2, AE_COLOR_ACCENT);
}

static void draw_required(float x, float y, float width, float card_x, float card_width, float height)
{
	struct ae_density const *d = &H.flow.density;
	int each;
	float row_y = y;

	(void)d;
	list_set_count(H.files.count);
	for (each = 0; each < H.files.count; each++)
	{
		struct ae_hub_file const *file = &H.files.files[each];
		char value[40];
		int kind = 0;

		if (file->group[0])
		{
			if (each)
				row_y += U(10.0f);
			count_line(x, row_y, width, file->group, file->group_value);
			row_y += U(34.0f);
		}
		if (file->value[0])
			snprintf(value, sizeof(value), "%s", file->value);
		else if (file->state == AE_HUB_FILE_FOUND)
			snprintf(value, sizeof(value), "FOUND");
		else if (file->state == AE_HUB_FILE_MISSING)
			snprintf(value, sizeof(value), "MISSING");
		else
			snprintf(value, sizeof(value), "NOT FOUND");
		kind = file->state == AE_HUB_FILE_MISSING ? 3 : file->state == AE_HUB_FILE_FOUND ? 2 : 1;
		if (file->state == AE_HUB_FILE_COUNT_ONLY)
			kind = 2;
		row_y += page_row(x, row_y, width, HIT_LIST, (short)each, file->name, value, kind,
			H.list.list.focus == each ? AE_ROW_FOCUSED : 0) + U(6.0f);
	}
	required_card(card_x, Y(252.0f), card_width, height);
}

/* ---------- BROWSE CE MAPS */

/* the two hosts the archive asks, named on the consent card and the error card only (the archive's own getter) */
static char list_host_text[96], files_host_text[96];

static char const *archive_list_host(void)
{
	ae_ce_archive_hosts(list_host_text, sizeof(list_host_text), files_host_text, sizeof(files_host_text));
	return list_host_text;
}

static char const *archive_files_host(void)
{
	ae_ce_archive_hosts(list_host_text, sizeof(list_host_text), files_host_text, sizeof(files_host_text));
	return files_host_text;
}

static void consent_card(float x, float y, float width, float height)
{
	char hosts[320];
	float cy;

	card_frame(x, y, width, height, 0);
	cy = y + U(24.0f);
	ae_draw_text(AE_FONT_ROW, U(30.0f), x + U(AE_FLOW_INSET_U), cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, "Custom Edition maps online");
	cy += U(52.0f);
	cy += card_body(x, cy, width, "Thousands of Custom Edition maps from a community map archive: search them and install one with A. "
		"They go to the maps folder and show on INSTALLED.", 3, AE_COLOR_TEXT) + U(14.0f);
	cy += card_label(x, cy, "WHAT THIS PAGE CONTACTS");
	snprintf(hosts, sizeof(hosts), "The archive's map list: %s. Its files: %s. They see this PC's internet address, as any website does. "
		"Nothing about you or your games is sent.", archive_list_host()[0] ? archive_list_host() : "(this build has none)",
		archive_files_host()[0] ? archive_files_host() : "(this build has none)");
	cy += card_body(x, cy, width, hosts, 4, AE_COLOR_TEXT) + U(14.0f);
	cy += card_label(x, cy, "EVERY FILE IS CHECKED");
	card_body(x, cy, width, "A map is kept only when its size and fingerprint match the archive's signed list.", 2, AE_COLOR_TEXT);
}

static void consent_side(float x, float y, float width, float height)
{
	float cy;

	card_frame(x, y, width, height, 0);
	cy = y + U(24.0f);
	cy += card_label(x, cy, "OFF UNTIL YOU SAY YES");
	cy += card_body(x, cy, width, "Until then this page asks nothing of anyone, and the other pages work as before.", 3, AE_COLOR_TEXT) + U(10.0f);
	cy += card_label(x, cy, "CHANGE IT LATER");
	cy += card_body(x, cy, width, "SETTINGS \xE2\x80\xBA ONLINE \xE2\x80\xBA MAPS \xE2\x80\xBA MAP ARCHIVE", 2, AE_COLOR_TITLE) + U(10.0f);
	cy += card_label(x, cy, "YOUR OWN MAPS");
	card_body(x, cy, width, "Downloads are yours to move or delete on INSTALLED, as any map.", 3, AE_COLOR_TEXT);
}

/* the two buttons of the consent card, side by side: NOT NOW (first focus), TURN ON BROWSING */
static void consent_buttons(float x, float y, float width)
{
	struct ae_density const *d = &H.flow.density;
	float gap = U(12.0f), half = (width - gap) * 0.5f, size = ae_size_text(d);
	int each;

	for (each = 0; each < 2; each++)
	{
		struct ae_row row;
		float height, bx = x + (half + gap) * (float)each, center;
		char const *label = each ? "TURN ON BROWSING" : "NOT NOW";
		int focused = H.button == each;

		memset(&row, 0, sizeof(row));
		row.label = "";
		row.flags = (focused ? AE_ROW_FOCUSED : 0) | AE_ROW_PRIMARY | AE_ROW_STATUS;
		height = ae_widget_row(d, bx, y, half, &row, HIT_ACTION, (short)each);
		center = y + height * 0.5f;
		ae_draw_text(AE_FONT_ROW, size, bx + half * 0.5f, ae_flow_cap_top(AE_FONT_ROW, size, center), AE_ALIGN_CENTER,
			focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TITLE, label);
	}
}

static void error_card(float x, float y, float width, float height, char const *title, char const *what, char const *todo)
{
	float cy;

	card_frame(x, y, width, height, 1);
	cy = y + U(24.0f);
	cy += card_title(x, cy, width, title);
	cy += card_body(x, cy, width, what, 3, AE_COLOR_TEXT) + U(14.0f);
	cy += card_label(x, cy, "WHAT TO DO");
	card_body(x, cy, width, todo, 3, AE_COLOR_TEXT);
}

static void draw_browse(float x, float y, float width, float card_x, float card_width, float height)
{
	struct ae_density const *d = &H.flow.density;
	char name[96], size[32], line[200], text[200], total[32], right[48];
	unsigned long long bytes;
	int installed, update, place, pages, total_rows;
	float cy, card_y = Y(252.0f);
	char file_name[110];
	char const *show_names[3] = { "ALL MAPS", "INSTALLED", "NOT INSTALLED" };
	if (H.state == AE_HUB_STATE_CONSENT)
	{
		/* the card's height leaves the two buttons under it */
		float card_h = height - U(74.0f);

		consent_card(x, card_y, width, card_h);
		consent_buttons(x, card_y + card_h + U(24.0f), width);
		consent_side(card_x, card_y, card_width, height);
		return;
	}
	y += search_field(x, y, width, "Search Custom Edition maps") + U(18.0f);
	y += show_sort_rows(x, y, width, show_names[H.show[AE_HUB_TAB_BROWSE] % 3], H.sort[AE_HUB_TAB_BROWSE] == 1 ? "SIZE" : "NAME") + U(10.0f);
	total_rows = browse_total();
	if (H.state == AE_HUB_STATE_OFFLINE || H.state == AE_HUB_STATE_BAD_LIST || (browse_real() && !ae_ce_archive_available()))
	{
		count_line(x, y, width, "NO MAP LIST", "");
		y += U(32.0f);
		ae_draw_text(AE_FONT_BODY, ae_size_text(d) * 0.9f, x + U(10.0f), y + U(8.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			H.state == AE_HUB_STATE_BAD_LIST ? "No map list is in use." : "No map list yet. A asks the archive again.");
		if (H.state == AE_HUB_STATE_BAD_LIST)
			error_card(card_x, card_y, card_width, height, "The map list was refused",
				"The archive's list didn't pass its check, so none of it is used.", "Try again later. Your installed maps and the other pages work as usual.");
		else
		{
			snprintf(line, sizeof(line), "%s did not answer in 15 seconds. This PC may be offline, or the archive may be down.",
				archive_list_host()[0] ? archive_list_host() : "The archive");
			if (browse_real() && H.archive.error[0] && H.archive.state == AE_CE_ARCHIVE_FAILED)
				snprintf(line, sizeof(line), "%s: %s.", archive_list_host()[0] ? archive_list_host() : "The archive", H.archive.error);
			error_card(card_x, card_y, card_width, height, "Can't reach the map archive", line,
				"Check this PC's internet, then A tries again. Your installed maps and the other pages work as usual.");
		}
		return;
	}
	if (H.state == AE_HUB_STATE_LOADING)
	{
		count_line(x, y, width, "LOADING THE MAP LIST...", "");
		y += U(32.0f);
		for (place = 0; place < 5; place++)
			ae_draw_rect(x, y + (float)place * U(56.0f), width, U(50.0f), 0.0f, AE_COLOR_ROW);
		card_frame(card_x, card_y, card_width, height, 0);
		return;
	}
	pages = browse_real() ? (total_rows + BROWSE_PAGE - 1) / BROWSE_PAGE : 286;
	if (pages < 1)
		pages = 1;
	ae_hub_count_text(browse_real() ? (unsigned long long)total_rows : 5717ull, total, sizeof(total));
	snprintf(text, sizeof(text), "PAGE %d OF %d", H.browse_page + 1, pages);
	snprintf(right, sizeof(right), "%s MAPS", total);
	count_line(x, y, width, text, right);
	y += U(32.0f);
	{
		int on_page = browse_real() ? total_rows - H.browse_page * BROWSE_PAGE : STAGED_ARCHIVE_COUNT;

		if (on_page > BROWSE_PAGE)
			on_page = BROWSE_PAGE;
		if (on_page < 0)
			on_page = 0;
		if (on_page == 0)
		{
			ae_draw_text(AE_FONT_BODY, ae_size_text(d) * 0.9f, x + U(10.0f), y + U(8.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
				H.search.text[0] ? "No map matches that search." : "No maps to show.");
			card_frame(card_x, card_y, card_width, height, 0);
			return;
		}
		page_list(x, y, width, AE_HUB_TAB_BROWSE, on_page);
	}
	place = H.browse_page * BROWSE_PAGE + H.list.list.focus;
	if (!browse_item(place, name, sizeof(name), &bytes, &installed, &update))
	{
		card_frame(card_x, card_y, card_width, height, 0);
		return;
	}
	snprintf(file_name, sizeof(file_name), "%s.map", name);
	card_frame(card_x, card_y, card_width, height, H.state == AE_HUB_STATE_DISK_FULL || H.state == AE_HUB_STATE_FAILED);
	cy = card_y + U(26.0f);
	{
		char title[100];

		upper_copy(title, sizeof(title), name);
		cy += card_title(card_x, cy, card_width, title);
	}
	ae_hub_size_text(bytes, size, sizeof(size));
	cy += card_fact(card_x, cy, card_width, "SIZE", size, AE_COLOR_TITLE);
	cy += card_fact(card_x, cy, card_width, "STATE", browse_downloading() && H.list.list.focus == 3 && !browse_real() ? "Installing" :
		update ? "A newer file is listed" : installed ? "On this PC" : "Not installed", AE_COLOR_TITLE);
	cy += card_fact(card_x, cy, card_width, "INSTALLS INTO", "maps folder", AE_COLOR_TITLE);
	cy += U(16.0f);
	if (H.state == AE_HUB_STATE_DISK_FULL)
	{
		snprintf(line, sizeof(line), "It needs %s; the maps folder's drive has 12 MB free.", size);
		cy += card_title(card_x, cy, card_width, "Not enough room");
		cy += card_body(card_x, cy, card_width, line, 2, AE_COLOR_TEXT) + U(8.0f);
		card_body(card_x, cy, card_width, "Delete maps you don't play on INSTALLED, or free space on that drive, then A tries again.", 3, AE_COLOR_TEXT);
	}
	else if (H.state == AE_HUB_STATE_FAILED)
	{
		cy += card_body(card_x, cy, card_width, browse_real() && H.download.error[0] ? H.download.error :
			"The file that came didn't match the archive's list, so it was thrown away. A tries again.", 3, AE_COLOR_WARNING);
	}
	else if (browse_downloading() && (browse_real() ? same_text(H.download.file, file_name) : place == 3))
	{
		unsigned long long done = browse_real() ? H.download.done : 16ull * 1024ull * 1024ull;
		unsigned long long all = browse_real() ? H.download.total : 38ull * 1024ull * 1024ull;
		float inset = U(AE_FLOW_INSET_U), bar_w = card_width - 2.0f * inset, fraction;
		char done_text[32], all_text[32];

		ae_hub_size_text(done, done_text, sizeof(done_text));
		ae_hub_size_text(all, all_text, sizeof(all_text));
		ae_draw_text_tracked(AE_FONT_BODY, ae_size_minor(d) * 0.84f, 0.09f, card_x + inset, cy, AE_ALIGN_LEFT, AE_COLOR_ACCENT, "INSTALLING");
		snprintf(line, sizeof(line), browse_real() ? "%s OF %s" : "%s OF %s \xE2\x80\xA2 2 WAITING", done_text, all_text);
		ae_draw_text_tracked(AE_FONT_BODY, ae_size_minor(d) * 0.84f, 0.09f, card_x + card_width - inset, cy, AE_ALIGN_RIGHT, AE_COLOR_MUTED, line);
		cy += U(34.0f);
		fraction = all ? (float)((double)done / (double)all) : 0.0f;
		ae_draw_rect(card_x + inset, cy, bar_w, U(6.0f), 0.0f, AE_COLOR_RULE);
		ae_draw_rect(card_x + inset, cy, bar_w * (fraction > 1.0f ? 1.0f : fraction), U(6.0f), 0.0f, AE_COLOR_ACCENT);
		cy += U(26.0f);
		card_body(card_x, cy, card_width, "It keeps going while you use other pages. A cancels it.", 2, AE_COLOR_MUTED);
	}
	else
	{
		cy += card_body(card_x, cy, card_width, installed && !update ? "This map is on this PC. A asks before it installs it again." :
			update ? "A installs the newer file over the one on this PC." : "A installs it into the maps folder.", 3, AE_COLOR_TEXT);
		if (browse_downloading())
			card_body(card_x, cy + U(8.0f), card_width, "Another map is installing. A cancels that one.", 2, AE_COLOR_MUTED);
	}
}

/* ---------- WORKSHOP (signed out; signed in is the Steam page's own list, ae_screen_steam.c) */

static void draw_workshop(float x, float y, float width, float card_x, float card_width, float height)
{
	char count[48];
	float cy, card_y = Y(252.0f), top_height = U(154.0f);

	(void)y;
	/* the card on the left: why; the two doors under it; on the right, what the page gives */
	card_frame(x, card_y, width, top_height, 0);
	cy = card_y + U(22.0f);
	ae_draw_text(AE_FONT_ROW, U(30.0f), x + U(AE_FLOW_INSET_U), cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, "Sign in to browse the Workshop");
	cy += U(48.0f);
	card_body(x, cy, width, "WORKSHOP MAPS asks Steam for Halo 1 Workshop maps and collections. It needs a Steam account that owns MCC. Workshop maps you "
		"already subscribed to are on INSTALLED.", 3, AE_COLOR_TEXT);
	cy = card_y + top_height + U(24.0f);
	cy += page_row(x, cy, width, HIT_ACTION, 0, "STEAM ACCOUNT", "SIGN IN", 0, AE_ROW_SUBSCREEN | (H.button == 0 ? AE_ROW_FOCUSED : 0)) + U(6.0f);
	snprintf(count, sizeof(count), "%d", H.staged ? 4 : ae_hub_data_workshop_count());
	cy += page_row(x, cy, width, HIT_ACTION, 1, "WORKSHOP MAPS ON INSTALLED", count, 0, AE_ROW_SUBSCREEN | (H.button == 1 ? AE_ROW_FOCUSED : 0)) +
		U(18.0f);
	/* (the two collections the offline playlists play: how many of their maps this PC has; A signs in) */
	{
		static struct ae_coll_model collections;
		int each, shown = 0;

		if (H.staged)
			ae_coll_data_stage(&collections, AE_COLL_SCENE_NONE, 0);
		else
			ae_coll_data_read(&collections, 0, 0);
		for (each = 0; each < collections.count && shown < 2; each++)
		{
			struct ae_coll const *coll = &collections.colls[each];
			struct ae_coll_summary summary;
			char value[48];

			if (!coll->playlist)
				continue;
			if (!shown)
			{
				ae_draw_text_tracked(AE_FONT_BODY, ae_size_minor(&H.flow.density) * 0.9f, 0.09f, x + U(10.0f), cy, AE_ALIGN_LEFT,
					AE_COLOR_MUTED, "FOR THE OFFLINE PLAYLISTS");
				cy += U(36.0f);
			}
			ae_coll_summarize(coll, &summary);
			snprintf(value, sizeof(value), "%d OF %d \xE2\x80\xA2 SIGN IN", summary.present, summary.count);
			cy += page_row(x, cy, width, HIT_ACTION, (short)(2 + shown), coll->name, value, 0,
				AE_ROW_SUBSCREEN | (H.button == 2 + shown ? AE_ROW_FOCUSED : 0)) + U(6.0f);
			shown++;
		}
	}
	card_frame(card_x, card_y, card_width, height, 0);
	cy = card_y + U(24.0f);
	cy += card_label(card_x, cy, "WHAT YOU GET");
	cy += card_body(card_x, cy, card_width, "Search, sort and install Halo 1 Workshop maps, with their pictures. Installs go to mcc_maps and show on INSTALLED.",
		3, AE_COLOR_TEXT) + U(10.0f);
	cy += card_label(card_x, cy, "YOUR PASSWORD");
	card_body(card_x, cy, card_width, "Goes to Steam only. Arena Evolved never saves it.", 2, AE_COLOR_TEXT);
}

/* ---------- MODS */

static void mod_state_text(struct ae_mods_item const *item, char *text, int size, int *kind)
{
	*kind = 0;
	if (item->state == AE_MODS_STATE_REFUSED)
	{
		snprintf(text, (size_t)size, "%s", item->reason[0] ? item->reason : "CAN'T BE PLAYED");
		*kind = 1;
	}
	else if (item->note[0] && strstr(item->note, "Y, then"))
	{
		snprintf(text, (size_t)size, "ORDER");
		*kind = 3;
	}
	else
	{
		snprintf(text, (size_t)size, "%s", item->on ? "ON" : "OFF");
		*kind = item->on ? 2 : 1;
	}
}

static void mods_card(float x, float y, float width, float height)
{
	struct ae_mods_item const *item = H.list.list.focus >= 0 && H.list.list.focus < H.mods.count ? &H.mods.items[H.list.list.focus] : NULL;
	char line[240], text[160];
	float cy;
	int warning = item && item->note[0] && strstr(item->note, "Y, then");

	if (!item)
	{
		card_frame(x, y, width, height, 0);
		cy = y + U(26.0f);
		card_title(x, cy, width, "OPEN THE MODS FOLDER");
		card_body(x, cy + U(60.0f), width, "A mod is a folder here with a mod.toml and its files. Mods play on this PC only: they never stop "
			"anyone joining a game.", 4, AE_COLOR_TEXT);
		return;
	}
	card_frame(x, y, width, height, warning);
	cy = y + U(26.0f);
	{
		char title[100];

		upper_copy(title, sizeof(title), item->title[0] ? item->title : item->name);
		cy += card_title(x, cy, width, title);
	}
	snprintf(line, sizeof(line), "%s \xE2\x80\xA2 PRIORITY %ld OF %ld%s%s", item->on ? "ON" : "OFF", item->priority, H.mods.count,
		item->version[0] ? " \xE2\x80\xA2 VERSION " : "", item->version);
	ae_draw_text_tracked(AE_FONT_BODY, ae_size_minor(&H.flow.density) * 0.95f, 0.09f, x + U(AE_FLOW_INSET_U), cy, AE_ALIGN_LEFT,
		item->on ? AE_COLOR_ACCENT : AE_COLOR_MUTED, line);
	cy += U(48.0f);
	text[0] = 0;
	{
		static char const *const names[AE_MODS_GROUPS] = { "Sounds", "Pictures", "Text", "Models", "Other" };
		int group, first = 1;

		for (group = 0; group < AE_MODS_GROUPS; group++)
			if (item->files[group] > 0 && group != AE_MODS_GROUP_OTHER)
			{
				snprintf(text + strlen(text), sizeof(text) - strlen(text), "%s%s: %ld", first ? "" : ", ", names[group], item->files[group]);
				first = 0;
			}
		if (first)
			snprintf(text, sizeof(text), item->files_known ? "Nothing yet" : "Not played yet");
	}
	cy += card_fact(x, cy, width, "CHANGES", text, AE_COLOR_TITLE);
	cy += card_fact(x, cy, width, "MAPS", item->maps[0] ? item->maps : "Every map", AE_COLOR_TITLE);
	if (item->authors[0])
		cy += card_fact(x, cy, width, "BY", item->authors, AE_COLOR_TITLE);
	snprintf(line, sizeof(line), "mods \xE2\x80\xBA %s", item->name);
	cy += card_fact(x, cy, width, "FOLDER", line, AE_COLOR_TITLE);
	cy += U(12.0f);
	if (item->note[0])
		cy += card_body(x, cy, width, item->note, 3, AE_COLOR_WARNING) + U(8.0f);
	if (item->state == AE_MODS_STATE_REFUSED && item->reason[0])
		cy += card_body(x, cy, width, item->reason, 2, AE_COLOR_MUTED) + U(8.0f);
	if (item->description[0])
		card_body(x, cy, width, item->description, 3, AE_COLOR_TEXT);
}

static void restart_card(float x, float y, float width, float height)
{
	char disc[48], line[160];
	float cy, inset = U(AE_FLOW_INSET_U), glyph = ae_size_glyph(&H.flow.density), size_row = ae_size_text(&H.flow.density) * 0.9f, gw;

	card_frame(x, y, width, height, 1);
	cy = y + U(26.0f);
	cy += card_title(x, cy, width, disc_text(disc, sizeof(disc)));
	ae_draw_text_tracked(AE_FONT_BODY, ae_size_minor(&H.flow.density) * 0.95f, 0.09f, x + inset, cy, AE_ALIGN_LEFT, AE_COLOR_WARNING, "AFTER A RESTART");
	cy += U(48.0f);
	cy += card_body(x, cy, width, "A modded disc's maps replace the stock maps it has. The game restarts to play it.", 3, AE_COLOR_TEXT) + U(14.0f);
	gw = ae_draw_button(AE_FONT_LAST_DEVICE, AE_BUTTON_START, glyph, x + inset, cy, AE_COLOR_TITLE);
	ae_draw_text(AE_FONT_ROW, size_row, x + inset + gw + U(12.0f), ae_flow_cap_top(AE_FONT_ROW, size_row, cy + glyph * 0.5f), AE_ALIGN_LEFT,
		AE_COLOR_TITLE, "RESTART NOW");
	cy += glyph + U(16.0f);
	gw = ae_draw_button(AE_FONT_LAST_DEVICE, AE_BUTTON_B, glyph, x + inset, cy, AE_COLOR_TITLE);
	ae_draw_text(AE_FONT_ROW, size_row, x + inset + gw + U(12.0f), ae_flow_cap_top(AE_FONT_ROW, size_row, cy + glyph * 0.5f), AE_ALIGN_LEFT,
		AE_COLOR_TITLE, "AT THE NEXT START");
	cy += glyph + U(24.0f);
	snprintf(line, sizeof(line), "Mods don't need a restart: they play as soon as you leave this page.");
	card_body(x, cy, width, line, 3, AE_COLOR_MUTED);
}

static void draw_mods(float x, float y, float width, float card_x, float card_width, float height)
{
	char disc[48], count[64], right[48];

	if (!H.mods_read)
		mods_read();
	/* the modded disc: left / right */
	y += page_row(x, y, width, HIT_CTL, CTL_SHOW, "MODDED DISC", disc_text(disc, sizeof(disc)), 0, H.ctl == CTL_SHOW ? AE_ROW_FOCUSED : 0) + U(24.0f);
	snprintf(count, sizeof(count), "MODS \xE2\x80\xA2 %d OF %ld ON", mods_on_count(), H.mods.count);
	snprintf(right, sizeof(right), "THE TOP ONE PLAYS FIRST");
	count_line(x, y, width, count, right);
	y += U(32.0f);
	page_list(x, y, width, AE_HUB_TAB_MODS, H.mods.count + 1);
	if (H.state == AE_HUB_STATE_RESTART)
		restart_card(card_x, Y(252.0f), card_width, height);
	else
		mods_card(card_x, Y(252.0f), card_width, H.list.list.focus >= H.mods.count ? height : height);
}

/* ---------- the list's rows */

static void list_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct list_context const *kind = context;
	char value[48], label[120];
	int tone = 0;

	(void)height;
	if (H.ctl >= 0 || H.overlay)
		flags &= ~(unsigned int)(AE_ROW_FOCUSED | AE_ROW_ON_BAR);
	switch (kind->kind)
	{
	case AE_HUB_TAB_INSTALLED:
	{
		struct ae_hub_map const *map = map_at(item);

		if (!map)
			return;
		if (map->in_use)
		{
			snprintf(value, sizeof(value), "IN USE");
			tone = 3;
		}
		else if (map->unreadable)
		{
			snprintf(value, sizeof(value), "CAN'T READ");
			tone = 1;
		}
		else if (H.show[AE_HUB_TAB_INSTALLED] == AE_HUB_SHOW_ALL)
		{
			snprintf(value, sizeof(value), "%s", ae_hub_map_source_name(map->source, map->list_name));
			tone = 1;
		}
		else
		{
			ae_hub_size_text(map->size, value, sizeof(value));
			tone = 1;
		}
		page_row(x, y, width, HIT_LIST, item, map->name, value, tone, flags);
		break;
	}
	case AE_HUB_TAB_BROWSE:
	{
		unsigned long long bytes;
		int installed, update;

		if (!browse_item(H.browse_page * BROWSE_PAGE + item, label, sizeof(label), &bytes, &installed, &update))
			return;
		browse_value(H.browse_page * BROWSE_PAGE + item, value, sizeof(value), &tone);
		page_row(x, y, width, HIT_LIST, item, label, value, tone, flags);
		break;
	}
	case AE_HUB_TAB_MODS:
	{
		struct ae_density const *d = &H.flow.density;
		float box = U(22.0f), space = ae_draw_text_width(AE_FONT_ROW, ae_size_text(d), " ");
		int pad = space > 0.0f ? (int)((box + U(16.0f)) / space) + 1 : 4, each;

		if (item >= H.mods.count)
		{
			page_row(x, y, width, HIT_LIST, item, "OPEN THE MODS FOLDER", NULL, 0, flags | AE_ROW_SUBSCREEN);
			break;
		}
		{
			struct ae_mods_item const *mod = &H.mods.items[item];

			mod_state_text(mod, value, sizeof(value), &tone);
			for (each = 0; each < pad && each < 20; each++)
				label[each] = ' ';
			snprintf(label + each, sizeof(label) - (size_t)each, "%s", mod->title[0] ? mod->title : mod->name);
			if (H.moving && H.moving_from == item)
			{
				snprintf(value, sizeof(value), "\xE2\x96\xB2 MOVING \xE2\x96\xBC");
				tone = 2;
			}
			page_row(x, y, width, HIT_LIST, item, label, value, tone ? tone : 1, flags);
			{
				float by = y + (U(50.0f) - box) * 0.5f;
				unsigned int line = (flags & AE_ROW_FOCUSED) ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED;

				if (mod->on && mod->state != AE_MODS_STATE_REFUSED)
					ae_draw_rect(x + U(20.0f), by, box, box, U(3.0f), AE_COLOR_ACCENT);
				else
					ae_draw_outline(x + U(20.0f), by, box, box, U(3.0f), U(2.0f), line);
			}
		}
		break;
	}
	}
}

/* ---------- the popover over a page: a map's options, the folder picker */

static void overlay_open(int kind, char const *title)
{
	H.overlay = kind;
	H.overlay_count = 0;
	H.overlay_focus = 0;
	snprintf(H.overlay_title, sizeof(H.overlay_title), "%s", title);
}

static void overlay_add(char const *label, char const *value, int muted)
{
	struct overlay_row *row;

	if (H.overlay_count >= OVERLAY_ROWS)
		return;
	row = &H.overlay_rows[H.overlay_count++];
	snprintf(row->label, sizeof(row->label), "%s", label);
	snprintf(row->value, sizeof(row->value), "%s", value ? value : "");
	row->muted = muted;
}

/* the stem of a map's file name ("coldwater.map": "coldwater") */
static void map_stem(struct ae_hub_map const *map, char *stem, int size)
{
	size_t length;

	snprintf(stem, (size_t)size, "%s", map->file);
	length = strlen(stem);
	if (length > 4 && !strcmp(stem + length - 4, ".map"))
		stem[length - 4] = 0;
}

/* why a game can't be made on a map (a level can't be played): NULL when it can */
static char const *host_reason(struct ae_hub_map const *map)
{
	char stem[64];

	if (H.staged)
		return NULL;
	map_stem(map, stem, sizeof(stem));
	if (map->kind_campaign)
		return ae_campaign_level_of_map(stem) >= 0 ? NULL : "NOT A STOCK LEVEL";
	if (!(map->serves & AE_MAP_SERVES_MULTIPLAYER))
		return "MENU MAP";
	return ae_screen_create_game_map_listed(map->list_name) ? NULL : "NOT IN THE MAP LIST";
}

static void map_flags(struct ae_hub_map const *map, struct ae_hub_map_flags *flags)
{
	flags->source = map->source;
	flags->in_use = map->in_use;
	flags->kind_campaign = map->kind_campaign;
	flags->unreadable = map->unreadable;
	flags->downloaded = map->downloaded;
	flags->client_open = map->client_open;
	flags->host_reason = host_reason(map);
}

static void overlay_for_map(struct ae_hub_map const *map)
{
	struct ae_hub_map_flags flags;
	struct ae_hub_option options[AE_HUB_OPTIONS];
	int each, option_count;
	char value[48];

	map_flags(map, &flags);
	option_count = ae_hub_map_options(&flags, options);
	target_keep(map);
	overlay_open(OVERLAY_OPTIONS, map->name);
	for (each = 0; each < option_count; each++)
	{
		value[0] = 0;
		if (options[each].reason)
			snprintf(value, sizeof(value), "%s", options[each].reason);
		else if (options[each].option == AE_HUB_OPTION_MOVE)
			snprintf(value, sizeof(value), "%s", map->folder[0] ? map->folder : "MAPS FOLDER");
		overlay_add(options[each].label, value, options[each].reason != NULL);
	}
	/* (the first offered row has the focus) */
	for (each = 0; each < H.overlay_count && H.overlay_rows[each].muted; each++)
		;
	H.overlay_focus = each < H.overlay_count ? each : 0;
}

static void overlay_for_folders(struct ae_hub_map const *map)
{
	char title[96];
	int each, counts[16], found = 0;
	char names[16][48];
	static char const *const staged_names[3] = { "brawl", "comp", "social" };
	static int const staged_counts[3] = { 4, 2, 3 };

	snprintf(title, sizeof(title), "Move %s", map->name);
	target_keep(map);
	overlay_open(OVERLAY_FOLDER, title);
	overlay_add("MAPS FOLDER", map->folder[0] ? "" : "HERE NOW", !map->folder[0]);
	if (H.staged)
	{
		for (each = 0; each < 3; each++)
		{
			snprintf(names[each], sizeof(names[each]), "%s", staged_names[each]);
			counts[each] = staged_counts[each];
		}
		found = 3;
	}
	else
		found = ae_hub_data_folders(map->library_index, names, counts, OVERLAY_ROWS - 2);
	for (each = 0; each < found; each++)
	{
		char value[32];

		if (!strcmp(map->folder, names[each]))
			snprintf(value, sizeof(value), "HERE NOW");
		else
			snprintf(value, sizeof(value), "%d MAPS", counts[each]);
		overlay_add(names[each], value, !strcmp(map->folder, names[each]));
	}
	overlay_add("NEW FOLDER...", "", 0);
	H.overlay_focus = map->folder[0] ? 0 : (found ? 1 : 0);
}

static void overlay_draw_name(void)
{
	struct ae_density const *d = &H.flow.density;
	float width = U(620.0f), height = U(190.0f);
	float x = H.flow.frame.rect.x + (H.flow.frame.rect.width - width) * 0.5f, y = H.flow.frame.rect.y + (H.flow.frame.rect.height - height) * 0.5f;
	struct ae_field field;

	ae_draw_rect(H.flow.frame.rect.x, H.flow.frame.rect.y, H.flow.frame.rect.width, H.flow.frame.rect.height, 0.0f, AE_COLOR_SCRIM);
	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_POPOVER);
	ae_draw_outline(x, y, width, height, 0.0f, U(1.0f), AE_COLOR_RULE);
	ae_draw_rect(x, y, width, U(4.0f), 0.0f, AE_COLOR_ACCENT);
	ae_hit_add(x, y, width, height, HIT_POP, AE_PART_CARD, -1);
	ae_draw_text(AE_FONT_ROW, U(30.0f), x + U(24.0f), y + U(26.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, H.overlay_title);
	memset(&field, 0, sizeof(field));
	field.label = "FOLDER NAME";
	field.placeholder = "Letters, numbers, spaces";
	field.text = &H.name_text;
	field.flags = AE_FIELD_FOCUSED | (H.naming || H.naming_keyboard ? AE_FIELD_EDITING : 0);
	field.caret_since = H.caret_since;
	ae_field_well(d, x + U(24.0f), y + U(76.0f), width - U(48.0f), &field, &H.name_well);
	ae_widget_field(d, x + U(24.0f), y + U(76.0f), width - U(48.0f), &field, HIT_POP);
	ae_draw_text(AE_FONT_BODY, ae_size_minor(d) * 0.9f, x + width - U(24.0f), y + height - U(34.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED,
		"A moves the map there. B cancels.");
}

static void overlay_draw(void)
{
	struct ae_density const *d = &H.flow.density;

	if (H.overlay == OVERLAY_NAME)
	{
		overlay_draw_name();
		return;
	}
	float width = U(620.0f), row_h = U(56.0f), title_h = U(70.0f), height = title_h + row_h * (float)H.overlay_count + U(24.0f);
	float x = H.flow.frame.rect.x + (H.flow.frame.rect.width - width) * 0.5f, y = H.flow.frame.rect.y + (H.flow.frame.rect.height - height) * 0.5f;
	int each;

	ae_draw_rect(H.flow.frame.rect.x, H.flow.frame.rect.y, H.flow.frame.rect.width, H.flow.frame.rect.height, 0.0f, AE_COLOR_SCRIM);
	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_POPOVER);
	ae_draw_outline(x, y, width, height, 0.0f, U(1.0f), AE_COLOR_RULE);
	ae_draw_rect(x, y, width, U(4.0f), 0.0f, AE_COLOR_ACCENT);
	ae_hit_add(x, y, width, height, HIT_POP, AE_PART_CARD, -1);
	ae_draw_text(AE_FONT_ROW, U(30.0f), x + U(24.0f), y + U(26.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, H.overlay_title);
	for (each = 0; each < H.overlay_count; each++)
	{
		struct ae_row row;
		struct overlay_row const *option = &H.overlay_rows[each];

		memset(&row, 0, sizeof(row));
		row.label = option->label;
		row.value = option->muted ? NULL : option->value[0] ? option->value : NULL;
		row.reason = option->muted ? option->value : NULL;
		row.flags = (option->muted ? AE_ROW_DISABLED : 0) | (H.overlay_focus == each ? AE_ROW_FOCUSED : 0) |
			(option->value[0] && !option->muted ? AE_ROW_STATUS : 0) |
			(!strcmp(option->label, "DELETE") ? AE_ROW_DESTRUCTIVE_FRAMED : 0);
		ae_widget_row(d, x + U(16.0f), y + title_h + row_h * (float)each, width - U(32.0f), &row, HIT_POP, (short)each);
	}
}

/* ---------- the delete confirm */

static void delete_picked(short choice, void *context)
{
	struct ae_hub_map *map = focused_map();
	char name[64];

	(void)context;
	if (choice != 1 || !map)
		return;
	if (!target_is(map))
	{
		target_changed();
		return;
	}
	snprintf(name, sizeof(name), "%s", map->name);
	/* a staged page removes the made-up map; the real page's delete is the map library's */
	if (H.staged)
	{
		int at = (int)(map - H.maps.maps);

		memmove(&H.maps.maps[at], &H.maps.maps[at + 1], sizeof(H.maps.maps[0]) * (size_t)(H.maps.count - at - 1));
		H.maps.count--;
		H.maps.total_count--;
		notice_set(0, 4000, "%s deleted.", name);
		platform_log("ae maps hub: staged: %s deleted", name);
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
	else
	{
		char reason[240];

		if (map->source == AE_HUB_SOURCE_WORKSHOP_INSTALLED && H.removing_name[0])
		{
			notice_set(1, 4000, "%s is still being removed: wait for it to finish.", H.removing_name);
			ae_sound_request(AE_SOUND_FAILURE, 0);
		}
		else if (map->source == AE_HUB_SOURCE_WORKSHOP_INSTALLED)
		{
			if (ae_hub_data_delete_downloaded(map->library_index, reason, sizeof(reason)))
			{
				snprintf(H.removing_name, sizeof(H.removing_name), "%s", name);
				platform_log("ae maps hub: removing %s", name);
				ae_sound_request(AE_SOUND_FORWARD, 0);
			}
			else
			{
				notice_set(1, 6000, "Can't delete %s: %s", name, reason);
				ae_sound_request(AE_SOUND_FAILURE, 0);
			}
		}
		else if (ae_hub_data_delete(map->library_index, reason, sizeof(reason)))
		{
			notice_set(0, 4000, "%s deleted.", name);
			platform_log("ae maps hub: %s deleted", name);
			lists_changed();
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else
		{
			notice_set(1, 6000, "Can't delete %s: %s", name, reason);
			ae_sound_request(AE_SOUND_FAILURE, 0);
		}
	}
}

static void delete_ask(void)
{
	struct ae_dialog_spec spec;
	struct ae_hub_map const *map = focused_map();
	static char title[96], body[240];
	struct ae_hub_map_flags flags;
	char const *why;
	char size[32];
	int maps;

	if (!map)
		return;
	/* one Workshop removal at a time: its notice names the map it was started for */
	if (H.removing_name[0] && map->source == AE_HUB_SOURCE_WORKSHOP_INSTALLED)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		notice_set(1, 4000, "%s is still being removed: wait for it to finish.", H.removing_name);
		return;
	}
	map_flags(map, &flags);
	why = ae_hub_map_delete_reason(&flags);
	if (why)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		notice_set(1, 4000, "%s", map->in_use ? "The lobby is on this map: leave the lobby first." : why);
		return;
	}
	ae_hub_size_text(map->size, size, sizeof(size));
	snprintf(title, sizeof(title), "Delete %s?", map->name);
	maps = map->source == AE_HUB_SOURCE_WORKSHOP_INSTALLED ? ae_hub_data_item_maps(map->library_index) : 1;
	if (maps > 1)
		snprintf(body, sizeof(body), "The whole Workshop item goes: its %d maps leave this PC, %s among them. MAPS & MODS > WORKSHOP gets it again.", maps, map->file);
	else if (map->source == AE_HUB_SOURCE_WORKSHOP_INSTALLED)
		snprintf(body, sizeof(body), "%s (%s) leaves this PC. MAPS & MODS > WORKSHOP gets it again.", map->file, size);
	else
		snprintf(body, sizeof(body), "%s (%s) leaves this PC. A game on it needs it again; a host that shares maps sends it back.", map->file, size);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = title;
	spec.body = body;
	spec.choices[0] = "CANCEL";
	spec.choices[1] = "DELETE";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.destructive_choices = 2u;
	spec.density = H.flow.density;
	spec.bounds = H.flow.frame.rect;
	spec.picked = delete_picked;
	target_keep(map);
	if (!ae_dialog_open(&spec, AE_OWNER_ANY))
		platform_log("ae maps hub: the delete dialog was refused");
}

/* ---------- help line, footer */

static char const *help_text(int *warning)
{
	static char text[320];
	struct ae_hub_map const *map = focused_map();

	*warning = 0;
	if (H.notice[0] && now_ms() < H.notice_until)
	{
		*warning = H.notice_warning;
		return H.notice;
	}
	switch (H.tab)
	{
	case AE_HUB_TAB_INSTALLED:
		if (H.state == AE_HUB_STATE_LOADING)
			return "Reading this PC's map folders: maps, maps_ce, the map packs, MCC and the Workshop.";
		if (map && map->unreadable)
			return "This file isn't a map AE can load. DELETE removes it.";
		if (map && map->in_use)
		{
			*warning = 1;
			return "The lobby is on this map: it can't be moved or deleted until you leave the lobby.";
		}
		if (H.ctl == CTL_SEARCH)
			return "Type part of a map's name. The list narrows as you type.";
		if (H.ctl == CTL_SHOW)
			return "Left and right pick which maps are listed, by where they came from.";
		if (H.ctl == CTL_SORT)
			return "A changes the order of the list.";
		if (map)
		{
			snprintf(text, sizeof(text), "A: play %s, move it to a folder or delete it. Y opens its folder; X searches your maps.", map->name);
			return text;
		}
		return "Maps this PC has, from every source the game reads.";
	case AE_HUB_TAB_REQUIRED:
		if (H.files.missing)
		{
			*warning = 1;
			return "A file is missing: STEAM ACCOUNT downloads MCC's copy, or copy it into the maps folder. AE looks again when you come back here.";
		}
		return "Everything Custom Edition and MCC maps need is here. X looks again.";
	case AE_HUB_TAB_BROWSE:
		switch (H.state)
		{
		case AE_HUB_STATE_CONSENT:
			return "NOT NOW keeps it off (B does too). TURN ON BROWSING asks the archive for its list now.";
		case AE_HUB_STATE_OFFLINE:
			*warning = 1;
			return "The map archive didn't answer. Check this PC's internet, then A tries again.";
		case AE_HUB_STATE_BAD_LIST:
			*warning = 1;
			return "The archive's list didn't pass its check, so none of it is used. Try again later.";
		case AE_HUB_STATE_DISK_FULL:
			*warning = 1;
			return "Not enough room for that map: free space on the maps folder's drive, then A tries again.";
		case AE_HUB_STATE_FAILED:
			*warning = 1;
			return "That download didn't finish. A tries again.";
		case AE_HUB_STATE_DOWNLOAD:
			return "Installing a map. Leave this page any time: MAPS & MODS on the main menu shows the downloads.";
		case AE_HUB_STATE_LOADING:
			return "Asking the archive for its map list.";
		default:
			return "A installs the map into the maps folder. It shows on INSTALLED when it is done.";
		}
	case AE_HUB_TAB_WORKSHOP:
		if (H.button >= 2)
			return "The offline playlists play the maps of two Workshop collections. Sign in on STEAM ACCOUNT to get them; B on that screen comes back here.";
		return H.button == 1 ? "Shows the Workshop maps already on this PC." :
			"Opens STEAM ACCOUNT: your Steam account name, your password, then Steam Guard. B there comes back here.";
	case AE_HUB_TAB_MODS:
		if (H.state == AE_HUB_STATE_RESTART)
		{
			*warning = 1;
			snprintf(text, sizeof(text), "MODDED DISC changed: the game restarts to play it. The MENU button restarts now; B keeps it for the next start.");
			return text;
		}
		if (H.moving)
			return "Up and down move the mod in the order. A or Y puts it down; B puts it back.";
		return "A turns a mod on or off. Y moves it in the order: the top one plays first. Mods play as soon as you leave this page.";
	}
	return "";
}

static void prompts_build(void)
{
	struct ae_flow_screen *flow = &H.flow;
	struct ae_hub_hint hints[AE_HUB_HINTS_MOST];
	int count, each;

	flow->prompt_count = 0;
	if (H.overlay == OVERLAY_NAME)
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Move", AE_ACTION_ACCEPT);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Cancel", AE_ACTION_BACK);
		return;
	}
	if (H.typing)
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Search", AE_ACTION_ACCEPT);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Stop typing", AE_ACTION_BACK);
		return;
	}
	count = ae_hub_hints(H.tab, H.state, 0, hints);
	for (each = 0; each < count; each++)
	{
		struct ae_prompt prompt;

		switch (hints[each].hint)
		{
		case AE_HUB_HINT_SEARCH:
			prompt = ae_flow_prompt(AE_BUTTON_X, hints[each].label, AE_ACTION_X);
			break;
		case AE_HUB_HINT_AGAIN:
			prompt = ae_flow_prompt(AE_BUTTON_X, hints[each].label, AE_ACTION_X);
			break;
		case AE_HUB_HINT_FOLDER:
		case AE_HUB_HINT_MOVE:
		case AE_HUB_HINT_PICTURE:
			prompt = ae_flow_prompt(AE_BUTTON_Y, hints[each].label, AE_ACTION_Y);
			break;
		case AE_HUB_HINT_PAGE:
			prompt = ae_flow_prompt(AE_BUTTON_LEFT_TRIGGER, hints[each].label, AE_ACTION_PAGE_UP);
			prompt.pair = 1;
			break;
		case AE_HUB_HINT_BACK:
		case AE_HUB_HINT_LATER:
			prompt = ae_flow_prompt(AE_BUTTON_B, hints[each].label, AE_ACTION_BACK);
			break;
		case AE_HUB_HINT_RESTART:
			prompt = ae_flow_prompt(AE_BUTTON_START, hints[each].label, AE_ACTION_START);
			break;
		default:
			prompt = ae_flow_prompt(AE_BUTTON_A, hints[each].label, AE_ACTION_ACCEPT);
			break;
		}
		flow->prompts[flow->prompt_count++] = prompt;
	}
	/* (the page strip's own keys are drawn at its ends; the footer says them too) */
	if (flow->prompt_count < AE_FLOW_PROMPTS - 1 && H.state != AE_HUB_STATE_MOVING && H.state != AE_HUB_STATE_RESTART)
	{
		struct ae_prompt prompt = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Pages", AE_ACTION_TAB_PREVIOUS);

		prompt.pair = 1;
		/* (before Back: the last prompt stays Back) */
		flow->prompts[flow->prompt_count] = flow->prompts[flow->prompt_count - 1];
		flow->prompts[flow->prompt_count - 1] = prompt;
		flow->prompt_count++;
	}
}

/* ---------- drawing */

static void hub_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &H.flow;
	struct ae_density const *d;
	float x, width, card_x, card_width, top, bottom, height, body, line;
	char expanded[400], lines_text[2][160];
	char const *help;
	int warning, count, each;

	(void)instance;
	ae_flow_full_density(flow);
	d = &flow->density;
	if (H.tab == AE_HUB_TAB_BROWSE)
		browse_sync();
	x = ae_flow_left_x(flow);
	width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH;
	card_x = ae_frame_x(&flow->frame, AE_FLOW_PANEL_X);
	card_width = ae_flow_right_x(flow) - card_x;
	top = Y(252.0f);
	bottom = ae_flow_content_bottom(flow) - U(96.0f);
	height = bottom - top;
	ae_flow_title(flow, "MAPS & MODS", "MAIN MENU");
	/* the tab strip: the change dot on REQUIRED FILES while a file is missing, on BROWSE CE MAPS while a map installs */
	H.tab_defs[AE_HUB_TAB_REQUIRED].changed = H.files.missing > 0;
	H.tab_defs[AE_HUB_TAB_BROWSE].changed = browse_downloading();
	H.tabs.active = (short)H.tab;
	ae_widget_tabs(d, &H.tabs, x, Y(182.0f), U(1030.0f), HIT_TABS);
	/* (a search that changed starts the list over) */
	{
		static char previous[64];

		if (strcmp(previous, H.search.text))
		{
			snprintf(previous, sizeof(previous), "%s", H.search.text);
			if (H.list.list.focus > 0)
				ae_list_set_focus(&H.list.list, 0);
			H.browse_page = 0;
		}
	}
	switch (H.tab)
	{
	case AE_HUB_TAB_INSTALLED:
		draw_installed(x, top, width, card_x, card_width, height);
		break;
	case AE_HUB_TAB_REQUIRED:
		draw_required(x, top, width, card_x, card_width, height);
		break;
	case AE_HUB_TAB_BROWSE:
		draw_browse(x, top, width, card_x, card_width, height);
		break;
	case AE_HUB_TAB_WORKSHOP:
		draw_workshop(x, top, width, card_x, card_width, height);
		break;
	case AE_HUB_TAB_MODS:
		draw_mods(x, top, width, card_x, card_width, height);
		break;
	}
	/* the help line over the footer */
	help = help_text(&warning);
	body = ae_flow_u(flow, d->metrics->body);
	line = ae_flow_u(flow, d->metrics->body_line);
	ae_draw_rect(x, ae_flow_content_bottom(flow) - line * 2.0f - U(14.0f), ae_flow_right_x(flow) - x, U(AE_FLOW_RULE_U), 0.0f, AE_COLOR_RULE);
	count = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(help, expanded, sizeof(expanded)), ae_flow_right_x(flow) - x, lines_text, 2);
	for (each = 0; each < count; each++)
		text_shadow(AE_FONT_BODY, body, x, ae_flow_content_bottom(flow) - line * 2.0f + line * (float)each, AE_ALIGN_LEFT,
			warning ? AE_COLOR_WARNING : AE_COLOR_TEXT, lines_text[each]);
	prompts_build();
	ae_flow_draw_prompts(flow);
	if (H.overlay)
		overlay_draw();
	if (H.state != H.logged_state)
	{
		H.logged_state = H.state;
		platform_log("ae maps hub: page %s state %d", ae_hub_tab_name(H.tab), H.state);
	}
}

/* ---------- the pages' entry */

static void list_reset(void)
{
	ae_list_view_init(&H.list, 0, 6);
	H.list_count = 0;
}

static void page_enter(int tab, int state)
{
	H.tab = tab;
	H.state = state;
	H.ctl = -1;
	H.button = 0;
	H.overlay = OVERLAY_NONE;
	H.moving = 0;
	H.tabs.active = (short)tab;
	list_reset();
	switch (tab)
	{
	case AE_HUB_TAB_INSTALLED:
		H.view_key = -1;
		if (H.staged)
			ae_hub_data_stage_maps(&H.maps, state);
		else
			ae_hub_data_update();
		if (state == AE_HUB_STATE_IN_USE || state == AE_HUB_STATE_UNREADABLE || state == AE_HUB_STATE_OPTIONS ||
			state == AE_HUB_STATE_FOLDER || state == AE_HUB_STATE_DELETE || state == AE_HUB_STATE_NEW_FOLDER ||
			state == AE_HUB_STATE_STEAM_PAGE || state == AE_HUB_STATE_OPEN_FOLDER)
		{
			/* (the staged map is Coldwater, the fourth by name) */
			installed_view();
			H.pending_delete = state == AE_HUB_STATE_DELETE;
			ae_list_set_count(&H.list.list, (short)H.view_count);
			H.list_count = (short)H.view_count;
			/* (the Workshop map's options: Delta Ruins, the fifth) */
			ae_list_set_focus(&H.list.list, state == AE_HUB_STATE_STEAM_PAGE ? 4 : 3);
		}
		if (state == AE_HUB_STATE_CARD_WORKSHOP || state == AE_HUB_STATE_CARD_STOCK)
		{
			/* (by name: Blood Gulch, the third, and Delta Ruins, the fifth) */
			installed_view();
			ae_list_set_count(&H.list.list, (short)H.view_count);
			H.list_count = (short)H.view_count;
			ae_list_set_focus(&H.list.list, state == AE_HUB_STATE_CARD_WORKSHOP ? 4 : 2);
		}
		if (state == AE_HUB_STATE_SHOW_COMMUNITY || state == AE_HUB_STATE_SHOW_CE)
		{
			H.show[AE_HUB_TAB_INSTALLED] = state == AE_HUB_STATE_SHOW_CE ? AE_HUB_SHOW_CUSTOM_EDITION : AE_HUB_SHOW_COMMUNITY;
			installed_view();
			ae_list_set_count(&H.list.list, (short)H.view_count);
			H.list_count = (short)H.view_count;
			ae_list_set_focus(&H.list.list, 0);
		}
		else if (state == AE_HUB_STATE_EMPTY)
		{
			H.show[AE_HUB_TAB_INSTALLED] = AE_HUB_SHOW_WORKSHOP;
			H.maps.total_count = 0;
		}
		else if (H.staged && H.show[AE_HUB_TAB_INSTALLED] == AE_HUB_SHOW_WORKSHOP)
			H.show[AE_HUB_TAB_INSTALLED] = AE_HUB_SHOW_ALL;
		break;
	case AE_HUB_TAB_REQUIRED:
	{
		int each;

		if (H.staged)
			ae_hub_data_stage_files(&H.files, state);
		else
		{
			discs_read();
			ae_hub_data_read_files(&H.files, H.disc_count);
		}
		list_set_count(H.files.count);
		for (each = 0; each < H.files.count; each++)
			if (H.files.files[each].state == AE_HUB_FILE_MISSING)
			{
				ae_list_set_focus(&H.list.list, (short)each);
				break;
			}
		break;
	}
	case AE_HUB_TAB_BROWSE:
		H.browse_view_valid = 0;
		H.browse_page = 0;
		H.staged_progress = 42;
		if (H.staged)
		{
			list_set_count(STAGED_ARCHIVE_COUNT);
			/* (a download under way is row 3's; the focus is on another row: A cancels it all the same) */
			if (state == AE_HUB_STATE_DISK_FULL || state == AE_HUB_STATE_FAILED)
				ae_list_set_focus(&H.list.list, 3);
		}
		break;
	case AE_HUB_TAB_WORKSHOP:
		/* (signed in: the Workshop list is the Steam page's own, drawn under this strip; a staged page opens its made-up one) */
		if (H.staged && state >= AE_HUB_STATE_COLL_SHOW)
			ae_screen_steam_open_collections_staged(state - AE_HUB_STATE_COLL_SHOW + 1);
		else if (H.staged ? state != AE_HUB_STATE_SIGNED_OUT : ae_screen_steam_workshop_ready())
			ae_screen_steam_open_workshop_hub(H.staged);
		else
			/* (nothing of the list works signed out: the footer offers only the two doors) */
			H.state = AE_HUB_STATE_SIGNED_OUT;
		break;
	case AE_HUB_TAB_MODS:
		H.mods_read = 0;
		mods_read();
		discs_read();
		if (state == AE_HUB_STATE_MOVING && H.mods.count > 1)
		{
			list_set_count(H.mods.count + 1);
			ae_list_set_focus(&H.list.list, 1);
			H.moving = 1;
			H.moving_from = 1;
		}
		if (state == AE_HUB_STATE_RESTART)
		{
			H.disc_now = 1;
			H.ctl = CTL_SHOW;
		}
		break;
	}
	if (!H.staged)
	{
		char value[8];

		snprintf(value, sizeof(value), "%d", tab);
		if (config_integer("menus.maps_tab") != tab)
			config_write("menus.maps_tab", value);
	}
}

static void tab_set(int tab)
{
	if (tab == H.tab)
		return;
	if (H.typing)
		ae_field_edit_end(&H.edit, 0);
	H.typing = 0;
	page_enter(tab, AE_HUB_STATE_LIST);
	/* (a staged BROWSE page opens on its results; a real one is as the archive says) */
	if (tab == AE_HUB_TAB_BROWSE && H.staged)
		H.state = AE_HUB_STATE_RESULTS;
	if (tab == AE_HUB_TAB_REQUIRED && H.staged)
		H.state = H.files.missing ? AE_HUB_STATE_MISSING : AE_HUB_STATE_FOUND;
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

/* ---------- typing in the search field */

static void typing_done(int keep, void *context)
{
	(void)keep;
	(void)context;
	H.typing = 0;
}

static void keyboard_done(int keep, void *context)
{
	(void)keep;
	(void)context;
	H.keyboard = 0;
}

static void typing_begin(int device)
{
	H.ctl = CTL_SEARCH;
	H.edit.done = typing_done;
	H.edit.context = NULL;
	H.edit.holder = &H;
	if (device == AE_DEVICE_KEYBOARD_MOUSE)
	{
		ae_field_edit_begin(&H.edit, &H.search);
		ae_text_select_all(&H.search);
		H.typing = 1;
		H.caret_since = ae_motion_now();
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
	else
	{
		struct ae_keyboard_spec spec;

		memset(&spec, 0, sizeof(spec));
		spec.text = &H.search;
		spec.field = H.well;
		spec.bounds = H.flow.frame.rect;
		spec.density = H.flow.density;
		spec.done = keyboard_done;
		spec.edit = &H.edit;
		if (ae_keyboard_open(&spec, AE_OWNER_ANY))
			H.keyboard = 1;
	}
}

static void typing_end(int keep)
{
	if (!H.typing)
		return;
	H.typing = 0;
	ae_field_edit_end(&H.edit, keep);
	ae_sound_request(keep ? AE_SOUND_FORWARD : AE_SOUND_BACK, 0);
}

/* ---------- what a press does */

/* a folder opened in the system's file manager (an absolute path; a staged page only says it would) */
static void open_path(char const *path)
{
	char url[900];

	if (!path[0] || !ae_hub_folder_url(path, url, sizeof(url)))
	{
		notice_set(1, 4000, "That folder can't be opened.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (H.staged)
	{
		platform_log("ae maps hub: staged: would open folder %s", path);
		notice_set(0, 3000, "Opens %s in the file manager.", path);
		return;
	}
	platform_log("ae maps hub: open folder %s", path);
	platform_open_url(url);
}

/* a folder of the data root */
static void open_root_folder(char const *under_root)
{
	char path[800];

	snprintf(path, sizeof(path), "%s/%s", platform_data_root(), under_root);
	open_path(path);
}

/* the folder the focused map's file is in, else the maps_ce folder */
static void open_map_folder(struct ae_hub_map const *map)
{
	if (map && map->path[0])
		open_path(map->path);
	else
		open_root_folder("maps_ce");
}

/* a Workshop map's page on Steam, in the PC's browser */
static void open_steam_page(struct ae_hub_map const *map)
{
	char url[160];

	if (!map || !map->workshop_item)
	{
		notice_set(1, 4000, "That map has no Workshop page.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_hub_workshop_url(map->workshop_item, url, sizeof(url));
	if (H.staged)
	{
		platform_log("ae maps hub: staged: would open the Steam page %s", url);
		notice_set(0, 3000, "Opens its Workshop page on Steam.");
		return;
	}
	platform_log("ae maps hub: open the Steam page %s", url);
	platform_open_url(url);
}

static void show_step(int direction)
{
	int count = H.tab == AE_HUB_TAB_INSTALLED ? AE_HUB_SHOWS : H.tab == AE_HUB_TAB_BROWSE ? 3 : 1;

	H.show[H.tab] = (H.show[H.tab] + direction + count) % count;
	H.browse_view_valid = 0;
	if (H.list.list.focus > 0)
		ae_list_set_focus(&H.list.list, 0);
	H.browse_page = 0;
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void sort_step(void)
{
	int count = H.tab == AE_HUB_TAB_INSTALLED ? SORTS : 2;

	H.sort[H.tab] = (H.sort[H.tab] + 1) % count;
	H.browse_view_valid = 0;
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

/* the install of a map started (overwrite: a file of that name is replaced): the archive's refusals shown as they come */
static void browse_start(char const *file, char const *name, int overwrite)
{
	char why[160];

	ae_ce_archive_download_reset();
	if (!ae_ce_archive_download(file, overwrite, why, sizeof(why)))
	{
		notice_set(1, 6000, "Can't install %s: %s.", name, why);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	platform_log("ae maps hub: installing %s", file);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void again_picked(short choice, void *context)
{
	(void)context;
	if (choice != 1)
		return;
	if (H.staged)
	{
		notice_set(0, 4000, "A staged page: nothing is downloaded.");
		platform_log("ae maps hub: staged: install again taken for %s", H.again_name);
		return;
	}
	browse_start(H.again_file, H.again_name, 1);
}

/* the question over a map this PC has already: installing it again fetches the file once more */
static void again_ask(char const *file, char const *name)
{
	struct ae_dialog_spec spec;
	static char title[120], body[240];

	snprintf(H.again_file, sizeof(H.again_file), "%s", file);
	snprintf(H.again_name, sizeof(H.again_name), "%s", name);
	snprintf(title, sizeof(title), "Install %.60s again?", name);
	snprintf(body, sizeof(body), "%.60s is on this PC. Installing it again downloads the file once more and replaces the one here.", name);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = title;
	spec.body = body;
	spec.choices[0] = "CANCEL";
	spec.choices[1] = "INSTALL AGAIN";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.density = H.flow.density;
	spec.bounds = H.flow.frame.rect;
	spec.picked = again_picked;
	if (ae_dialog_open(&spec, AE_OWNER_ANY))
	{
		platform_log("ae maps hub: asked to install %s again", name);
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
}

static void browse_accept_row(void)
{
	char name[96], file[100];
	unsigned long long bytes;
	int installed, update, place = H.browse_page * BROWSE_PAGE + H.list.list.focus;
	int running = browse_real() ? H.download.state == AE_CE_ARCHIVE_DOWNLOAD_RUNNING : H.state == AE_HUB_STATE_DOWNLOAD;

	/* (A while a map installs cancels that install, whatever row has the focus) */
	if (running)
	{
		if (browse_real())
		{
			snprintf(file, sizeof(file), "%s", H.download.file);
			ae_ce_archive_cancel();
			ae_ce_archive_download_reset();
		}
		else
		{
			snprintf(file, sizeof(file), "%s.map", staged_archive[3].name);
			H.state = AE_HUB_STATE_RESULTS;
		}
		notice_set(0, 4000, "Install of %s cancelled. Nothing was kept.", file);
		platform_log("ae maps hub: install cancelled %s", file);
		ae_sound_request(AE_SOUND_BACK, 0);
		return;
	}
	if (!browse_item(place, name, sizeof(name), &bytes, &installed, &update))
		return;
	snprintf(file, sizeof(file), "%s.map", name);
	if (installed && !update)
	{
		again_ask(file, name);
		return;
	}
	if (!browse_real())
	{
		notice_set(0, 4000, "A staged page: nothing is downloaded.");
		ae_sound_request(AE_SOUND_CURSOR, 0);
		return;
	}
	browse_start(file, name, update);
}

static void consent_answer(int yes)
{
	if (!yes)
		return;
	if (H.staged)
	{
		H.state = AE_HUB_STATE_RESULTS;
		list_set_count(STAGED_ARCHIVE_COUNT);
		return;
	}
	config_write("game.ce_archive", "true");
	ae_ce_archive_set_consent(1);
	H.browse_started = 0;
	platform_log("ae maps hub: the map archive turned on");
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void accept(void)
{
	struct ae_hub_map *map;

	switch (H.tab)
	{
	case AE_HUB_TAB_INSTALLED:
		if (H.ctl == CTL_SEARCH)
		{
			typing_begin(AE_DEVICE_KEYBOARD_MOUSE);
			return;
		}
		if (H.ctl == CTL_SHOW)
		{
			show_step(1);
			return;
		}
		if (H.ctl == CTL_SORT)
		{
			sort_step();
			return;
		}
		if (H.view_count == 0)
		{
			if (H.show[AE_HUB_TAB_INSTALLED] == AE_HUB_SHOW_WORKSHOP)
				tab_set(AE_HUB_TAB_WORKSHOP);
			return;
		}
		map = map_at(H.list.list.focus);
		if (map)
		{
			overlay_for_map(map);
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		return;
	case AE_HUB_TAB_REQUIRED:
	{
		struct ae_hub_file const *file = H.list.list.focus >= 0 && H.list.list.focus < H.files.count ? &H.files.files[H.list.list.focus] : NULL;

		if (!file)
			return;
		if (file->door == 1 && (file->state == AE_HUB_FILE_MISSING || file->state == AE_HUB_FILE_FOUND))
		{
			if (H.staged)
				notice_set(0, 4000, "Opens STEAM ACCOUNT; B comes back here.");
			else if (!ae_screen_steam_open("MAPS & MODS"))
				ae_sound_request(AE_SOUND_FAILURE, 0);
			else
				ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else if (file->door == 2)
			tab_set(AE_HUB_TAB_MODS);
		else
			ae_sound_request(AE_SOUND_CURSOR, 0);
		return;
	}
	case AE_HUB_TAB_BROWSE:
		if (H.state == AE_HUB_STATE_CONSENT)
		{
			if (H.button == 0)
				ae_ui_pop();
			else
				consent_answer(1);
			return;
		}
		if (H.ctl == CTL_SEARCH)
		{
			typing_begin(AE_DEVICE_KEYBOARD_MOUSE);
			return;
		}
		if (H.ctl == CTL_SHOW)
		{
			show_step(1);
			return;
		}
		if (H.ctl == CTL_SORT)
		{
			sort_step();
			return;
		}
		if (H.state == AE_HUB_STATE_OFFLINE || H.state == AE_HUB_STATE_BAD_LIST)
		{
			if (browse_real())
			{
				H.browse_started = 0;
				ae_ce_archive_start(NULL, 0);
			}
			return;
		}
		browse_accept_row();
		return;
	case AE_HUB_TAB_WORKSHOP:
		if (H.button == 0 || H.button >= 2)
		{
			if (H.staged)
				notice_set(0, 4000, "Opens STEAM ACCOUNT; B comes back here.");
			else
				ae_screen_steam_open("MAPS & MODS");
		}
		else
		{
			H.show[AE_HUB_TAB_INSTALLED] = AE_HUB_SHOW_WORKSHOP;
			tab_set(AE_HUB_TAB_INSTALLED);
		}
		return;
	case AE_HUB_TAB_MODS:
		if (H.moving)
		{
			H.moving = 0;
			H.state = AE_HUB_STATE_LIST;
			ae_sound_request(AE_SOUND_FORWARD, 0);
			return;
		}
		if (H.ctl == CTL_SHOW)
		{
			disc_step(1);
			return;
		}
		if (H.list.list.focus >= H.mods.count)
			open_root_folder("mods");
		else if (H.list.list.focus >= 0)
			mods_turn(H.list.list.focus);
		return;
	}
}

/* the map moved into a folder ("" the maps folder's top) */
static void move_map_to(struct ae_hub_map *map, char const *folder)
{
	char name[64];

	snprintf(name, sizeof(name), "%s", map->name);
	if (H.staged)
	{
		snprintf(map->folder, sizeof(map->folder), "%s", folder);
		notice_set(0, 4000, "%s moved to %s.", name, folder[0] ? folder : "the maps folder");
		platform_log("ae maps hub: staged: %s moved to %s", name, folder[0] ? folder : "the maps folder");
	}
	else
	{
		char reason[240];

		if (ae_hub_data_move(map->library_index, folder, reason, sizeof(reason)))
		{
			notice_set(0, 4000, "%s moved to %s.", name, folder[0] ? folder : "the maps folder");
			platform_log("ae maps hub: %s moved to %s", name, folder[0] ? folder : "the maps folder");
			lists_changed();
		}
		else
		{
			notice_set(1, 6000, "Can't move %s: %s", name, reason);
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* ---------- a new folder's name: typed here (a keyboard types into the field, a pad's A opens AE's keyboard) */

static void name_cancel(void)
{
	if (H.naming)
		ae_field_edit_end(&H.name_edit, 0);
	H.naming = 0;
	H.overlay = OVERLAY_NONE;
	ae_sound_request(AE_SOUND_BACK, 0);
}

static void name_begin(void);

/* the name typed taken: checked as a folder's name, then the map moves there */
static void name_commit(void)
{
	struct ae_hub_map *map = focused_map();
	char text[64];

	snprintf(text, sizeof(text), "%s", H.name_text.text);
	if (H.naming)
		ae_field_edit_end(&H.name_edit, 1);
	H.naming = 0;
	if (!map)
	{
		H.overlay = OVERLAY_NONE;
		return;
	}
	if (!target_is(map))
	{
		target_changed();
		return;
	}
	if (!ae_hub_data_folder_name_ok(text))
	{
		notice_set(1, 7000, "A folder's name can't have \\ / : * ? \" < > | in it, start with . or _, or end with a dot or a space.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		name_begin();
		return;
	}
	H.overlay = OVERLAY_NONE;
	move_map_to(map, text);
}

static void name_typing_done(int keep, void *context)
{
	(void)keep;
	(void)context;
	H.naming = 0;
}

static void name_keyboard_done(int keep, void *context)
{
	(void)context;
	H.naming_keyboard = 0;
	/* (a physical key closed the keyboard into the field's own typing: that goes on) */
	if (H.name_edit.active && H.name_edit.text == &H.name_text)
	{
		H.naming = 1;
		return;
	}
	if (keep)
		name_commit();
	else
		name_cancel();
}

static void name_begin(void)
{
	struct ae_hub_map const *map = focused_map();
	char title[96];

	snprintf(title, sizeof(title), "New folder for %s", map ? map->name : "the map");
	if (H.overlay != OVERLAY_NAME)
	{
		overlay_open(OVERLAY_NAME, title);
		ae_text_init(&H.name_text, H.staged && H.state == AE_HUB_STATE_NEW_FOLDER ? "new_folder" : "", 40, 0);
	}
	H.name_edit.done = name_typing_done;
	H.name_edit.context = NULL;
	H.name_edit.holder = &H;
	platform_log("ae maps hub: a new folder's name asked");
	if (H.device == AE_DEVICE_KEYBOARD_MOUSE || H.staged)
	{
		ae_field_edit_begin(&H.name_edit, &H.name_text);
		ae_text_select_all(&H.name_text);
		H.naming = 1;
		H.caret_since = ae_motion_now();
	}
	else
	{
		struct ae_keyboard_spec spec;

		memset(&spec, 0, sizeof(spec));
		spec.text = &H.name_text;
		spec.field = H.name_well;
		spec.bounds = H.flow.frame.rect;
		spec.density = H.flow.density;
		spec.done = name_keyboard_done;
		spec.edit = &H.name_edit;
		if (ae_keyboard_open(&spec, AE_OWNER_ANY))
			H.naming_keyboard = 1;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* the options list's row done */
static void overlay_pick(int row)
{
	struct ae_hub_map *map = focused_map();

	if (row < 0 || row >= H.overlay_count || H.overlay_rows[row].muted || !map)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (!target_is(map))
	{
		target_changed();
		return;
	}
	if (H.overlay == OVERLAY_OPTIONS)
	{
		struct ae_hub_option options[AE_HUB_OPTIONS];
		struct ae_hub_map_flags flags;
		char stem[64];

		map_flags(map, &flags);
		ae_hub_map_options(&flags, options);
		H.overlay = OVERLAY_NONE;
		map_stem(map, stem, sizeof(stem));
		switch (options[row].option)
		{
		case AE_HUB_OPTION_PLAY:
			if (H.staged)
			{
				platform_log("ae maps hub: staged: would open %s on %s", map->kind_campaign ? "CAMPAIGN" : "CREATE GAME", map->name);
				notice_set(0, 4000, "A staged page: %s is not opened.", map->kind_campaign ? "CAMPAIGN" : "CREATE GAME");
			}
			else if (map->kind_campaign ? ae_screen_campaign_open_on_level(stem) : ae_screen_create_game_open_on_map(map->list_name))
			{
				platform_log("ae maps hub: opened %s on %s", map->kind_campaign ? "CAMPAIGN" : "CREATE GAME", map->list_name);
				ae_sound_request(AE_SOUND_FORWARD, 0);
			}
			else
			{
				notice_set(1, 5000, "%s can't be opened on %s.", map->kind_campaign ? "CAMPAIGN" : "CREATE GAME", map->name);
				ae_sound_request(AE_SOUND_FAILURE, 0);
			}
			break;
		case AE_HUB_OPTION_MOVE:
			overlay_for_folders(map);
			break;
		case AE_HUB_OPTION_OPEN_FOLDER:
			open_map_folder(map);
			break;
		case AE_HUB_OPTION_DELETE:
			delete_ask();
			break;
		case AE_HUB_OPTION_STEAM_PAGE:
			open_steam_page(map);
			break;
		}
		return;
	}
	if (H.overlay == OVERLAY_FOLDER)
	{
		if (row == H.overlay_count - 1)
		{
			name_begin();
			return;
		}
		H.overlay = OVERLAY_NONE;
		move_map_to(map, row == 0 ? "" : H.overlay_rows[row].label);
	}
}

/* ---------- events */

static void nav(struct ae_event const *event)
{
	int direction = event->action == AE_ACTION_UP ? -1 : 1;
	int first_ctl = H.tab == AE_HUB_TAB_MODS ? CTL_SHOW : CTL_SEARCH, last_ctl = H.tab == AE_HUB_TAB_MODS ? CTL_SHOW : CTL_SORT;
	int has_controls = H.tab == AE_HUB_TAB_INSTALLED || H.tab == AE_HUB_TAB_BROWSE || H.tab == AE_HUB_TAB_MODS;

	if ((H.tab == AE_HUB_TAB_BROWSE && H.state == AE_HUB_STATE_CONSENT) || H.tab == AE_HUB_TAB_WORKSHOP)
	{
		if (H.tab == AE_HUB_TAB_BROWSE)
			H.button = 1 - H.button;
		else
		{
			/* (WORKSHOP signed out: STEAM ACCOUNT, INSTALLED, then the playlists' two collections) */
			H.button += direction;
			H.button = H.button < 0 ? 0 : H.button > 3 ? 3 : H.button;
		}
		ae_sound_request(AE_SOUND_CURSOR, 0);
		return;
	}
	if (H.moving)
	{
		mods_shift(H.list.list.focus, direction);
		return;
	}
	if (!has_controls)
	{
		/* (REQUIRED FILES: the rows are drawn by the page, the focus is the list's) */
		short next = (short)(H.list.list.focus + direction);

		if (next >= 0 && next < H.files.count)
		{
			ae_list_set_focus(&H.list.list, next);
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		}
		return;
	}
	if (H.ctl >= 0)
	{
		if (direction > 0)
		{
			if (H.ctl < last_ctl)
				H.ctl++;
			else if (H.list_count > 0)
				H.ctl = -1;
		}
		else if (H.ctl > first_ctl)
			H.ctl--;
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		return;
	}
	if (direction < 0 && H.list.list.focus <= 0)
	{
		H.ctl = last_ctl;
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		return;
	}
	ae_list_view_event(&H.list, event);
}

static void hub_leave(struct ae_screen *instance)
{
	(void)instance;
	ae_screen_steam_return_forget();
	if (H.typing)
		ae_field_edit_end(&H.edit, 0);
	H.typing = 0;
}

static int hub_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	H.device = event->device;
	if (H.overlay == OVERLAY_NAME)
	{
		if (event->action == AE_ACTION_ACCEPT || event->action == AE_ACTION_START)
		{
			if (!H.naming && !H.naming_keyboard)
				name_begin();
			else if (H.naming)
				name_commit();
		}
		else if (event->action == AE_ACTION_BACK)
			name_cancel();
		return 1;
	}
	if (H.overlay)
	{
		switch (event->action)
		{
		case AE_ACTION_BACK:
			H.overlay = OVERLAY_NONE;
			ae_sound_request(AE_SOUND_BACK, 0);
			return 1;
		case AE_ACTION_UP:
		case AE_ACTION_DOWN:
			H.overlay_focus = (H.overlay_focus + (event->action == AE_ACTION_UP ? H.overlay_count - 1 : 1)) % H.overlay_count;
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
			return 1;
		case AE_ACTION_ACCEPT:
			overlay_pick(H.overlay_focus);
			return 1;
		default:
			return 1;
		}
	}
	if (H.typing)
	{
		if (event->action == AE_ACTION_START || event->action == AE_ACTION_ACCEPT)
			typing_end(1);
		else if (event->action == AE_ACTION_BACK)
			typing_end(0);
		else if (event->action == AE_ACTION_UP || event->action == AE_ACTION_DOWN)
		{
			typing_end(1);
			nav(event);
		}
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		if (H.moving)
		{
			H.moving = 0;
			H.state = AE_HUB_STATE_LIST;
			mods_read();
			ae_sound_request(AE_SOUND_BACK, 0);
			return 1;
		}
		if (H.state == AE_HUB_STATE_RESTART)
		{
			H.state = AE_HUB_STATE_LIST;
			ae_sound_request(AE_SOUND_BACK, 0);
			return 1;
		}
		return 0;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		if (!H.moving)
			tab_set(ae_hub_tab_step(H.tab, event->action == AE_ACTION_TAB_PREVIOUS ? -1 : 1));
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		nav(event);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (H.tab == AE_HUB_TAB_BROWSE && H.state == AE_HUB_STATE_CONSENT)
			H.button = 1 - H.button;
		else if (H.ctl == CTL_SHOW && H.tab == AE_HUB_TAB_MODS)
			disc_step(event->action == AE_ACTION_RIGHT ? 1 : -1);
		else if (H.ctl == CTL_SHOW)
			show_step(event->action == AE_ACTION_RIGHT ? 1 : -1);
		else if (H.ctl == CTL_SORT)
			sort_step();
		return 1;
	case AE_ACTION_PAGE_UP:
	case AE_ACTION_PAGE_DOWN:
		if (H.tab == AE_HUB_TAB_BROWSE && H.ctl < 0)
		{
			int pages = browse_real() ? (browse_total() + BROWSE_PAGE - 1) / BROWSE_PAGE : 286;

			H.browse_page += event->action == AE_ACTION_PAGE_UP ? -1 : 1;
			if (H.browse_page < 0)
				H.browse_page = 0;
			if (H.browse_page >= pages)
				H.browse_page = pages - 1;
			ae_list_set_focus(&H.list.list, 0);
			ae_sound_request(AE_SOUND_CURSOR, 0);
		}
		else if (H.ctl < 0 && H.tab != AE_HUB_TAB_REQUIRED)
			ae_list_view_event(&H.list, event);
		return 1;
	case AE_ACTION_ACCEPT:
		accept();
		return 1;
	case AE_ACTION_X:
	case AE_ACTION_FIND:
		if (H.tab == AE_HUB_TAB_INSTALLED || (H.tab == AE_HUB_TAB_BROWSE && H.state != AE_HUB_STATE_CONSENT))
			typing_begin(event->device);
		else if (H.tab == AE_HUB_TAB_REQUIRED)
		{
			page_enter(H.tab, H.state);
			if (H.staged)
				H.state = H.files.missing ? AE_HUB_STATE_MISSING : AE_HUB_STATE_FOUND;
			notice_set(0, 3000, "Looked again.");
		}
		else if (H.tab == AE_HUB_TAB_MODS)
		{
			if (!H.staged)
				ae_mods_list_engine()->apply();
			mods_read();
			notice_set(0, 3000, "Looked for mods again.");
		}
		return 1;
	case AE_ACTION_Y:
		if (H.tab == AE_HUB_TAB_INSTALLED)
			open_map_folder(H.ctl < 0 ? map_at_for(H.list.list.focus, 1) : NULL);
		else if (H.tab == AE_HUB_TAB_REQUIRED)
			open_root_folder("maps");
		else if (H.tab == AE_HUB_TAB_MODS && H.ctl < 0 && H.list.list.focus >= 0 && H.list.list.focus < H.mods.count)
		{
			H.moving = 1;
			H.moving_from = H.list.list.focus;
			H.state = AE_HUB_STATE_MOVING;
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else if (H.tab == AE_HUB_TAB_BROWSE)
			ae_sound_request(AE_SOUND_FAILURE, 0);
		return 1;
	case AE_ACTION_START:
		if (H.state == AE_HUB_STATE_RESTART)
		{
			if (H.staged)
				notice_set(0, 4000, "A staged page: the game does not restart.");
			else
				platform_restart();
		}
		return 1;
	case AE_ACTION_DELETE:
		if (H.tab == AE_HUB_TAB_INSTALLED && H.ctl < 0)
			delete_ask();
		return 1;
	default:
		return 1;
	}
}

static void hub_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	short item;

	if (ae_prompts_pointer(H.flow.prompts, H.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK && !H.overlay && !H.moving && H.state != AE_HUB_STATE_RESTART)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			hub_handle(instance, &event);
		return;
	}
	if (pointer->right_clicks)
	{
		event.player = pointer->player;
		event.action = AE_ACTION_BACK;
		event.device = AE_DEVICE_KEYBOARD_MOUSE;
		event.repeat = 0;
		if (!hub_handle(instance, &event))
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		return;
	}
	if (H.overlay == OVERLAY_NAME)
	{
		if (pointer->left_clicks && !H.naming_keyboard && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id != HIT_POP)
			name_cancel();
		return;
	}
	if (H.overlay)
	{
		if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit))
		{
			if (hit.id == HIT_POP && hit.part != AE_PART_CARD)
			{
				H.overlay_focus = hit.index;
				overlay_pick(hit.index);
			}
			else if (hit.id != HIT_POP)
				H.overlay = OVERLAY_NONE;
		}
		return;
	}
	if (ae_tabs_pointer(&H.tabs, pointer, HIT_TABS))
	{
		if (H.tabs.active != H.tab && !H.moving)
			tab_set(H.tabs.active);
		H.tabs.active = (short)H.tab;
		return;
	}
	/* (a mod being moved: a click on a row moves it there, place by place, as up and down do) */
	if (H.moving && pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_LIST && hit.index >= 0 &&
		hit.index < H.mods.count)
	{
		int guard = H.mods.count;

		while (guard-- > 0 && H.list.list.focus != hit.index)
			mods_shift(H.list.list.focus, hit.index < H.list.list.focus ? -1 : 1);
		return;
	}
	if (!pointer->left_clicks)
	{
		if (H.tab == AE_HUB_TAB_REQUIRED)
			return;
	}
	if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit))
	{
		if (hit.id == HIT_FIELD)
		{
			if (!H.typing)
				typing_begin(AE_DEVICE_KEYBOARD_MOUSE);
			return;
		}
		if (H.typing)
			typing_end(1);
		if (hit.id == HIT_CTL)
		{
			H.ctl = hit.index;
			if (hit.part == AE_PART_ARROW_LEFT)
				show_step(-1);
			else if (hit.index == CTL_SHOW && H.tab == AE_HUB_TAB_MODS)
				disc_step(hit.part == AE_PART_ARROW_LEFT ? -1 : 1);
			else if (hit.index == CTL_SHOW)
				show_step(1);
			else if (hit.index == CTL_SORT)
				sort_step();
			return;
		}
		if (hit.id == HIT_ACTION)
		{
			H.button = hit.index;
			accept();
			return;
		}
		if (hit.id == HIT_LIST && H.tab == AE_HUB_TAB_REQUIRED)
		{
			if (hit.index == H.list.list.focus)
				accept();
			else
				ae_list_set_focus(&H.list.list, hit.index);
			return;
		}
	}
	if (H.tab != AE_HUB_TAB_REQUIRED && H.tab != AE_HUB_TAB_WORKSHOP && !(H.tab == AE_HUB_TAB_BROWSE && H.state == AE_HUB_STATE_CONSENT))
	{
		item = ae_list_view_pointer(&H.list, pointer, HIT_LIST);
		if (item >= 0 && item < H.list_count)
		{
			if (H.ctl < 0 && item == H.list.list.focus)
				accept();
			else
			{
				H.ctl = -1;
				ae_list_set_focus(&H.list.list, item);
			}
		}
	}
}

static void hub_update(struct ae_screen *instance)
{
	(void)instance;
	if (H.removing_name[0])
	{
		int removed;

		if (ae_hub_data_delete_poll(&removed))
		{
			if (removed)
				notice_set(0, 4000, "%s deleted.", H.removing_name);
			else
				notice_set(1, 6000, "Can't delete %s: the removal did not finish.", H.removing_name);
			platform_log("ae maps hub: %s %s", H.removing_name, removed ? "deleted" : "not removed");
			H.removing_name[0] = 0;
			lists_changed();
		}
	}
	if (H.typing && H.edit.active && !ae_field_edit_keys(&H.edit))
		ae_sound_request(AE_SOUND_FAILURE, 0);
	if (H.naming && H.name_edit.active && !ae_field_edit_keys(&H.name_edit))
		ae_sound_request(AE_SOUND_FAILURE, 0);
	/* (a staged page's first frame: the delete confirm, the options or the folder picker the case opens on) */
	if (H.first_frame && H.flow.density.metrics)
	{
		H.first_frame = 0;
		if (H.pending_delete)
		{
			H.pending_delete = 0;
			delete_ask();
		}
		else if (H.tab == AE_HUB_TAB_INSTALLED && (H.state == AE_HUB_STATE_OPTIONS || H.state == AE_HUB_STATE_FOLDER ||
			H.state == AE_HUB_STATE_STEAM_PAGE) && focused_map())
		{
			overlay_for_map(focused_map());
			if (H.state == AE_HUB_STATE_FOLDER)
				overlay_for_folders(focused_map());
		}
		else if (H.tab == AE_HUB_TAB_INSTALLED && H.state == AE_HUB_STATE_NEW_FOLDER && focused_map())
		{
			overlay_for_folders(focused_map());
			name_begin();
		}
		else if (H.tab == AE_HUB_TAB_INSTALLED && H.state == AE_HUB_STATE_CREATE)
		{
			/* (a real map of the game's own list, so the screen that opens is the real one) */
			if (!ae_screen_create_game_open_on_map("bloodgulch"))
				platform_log("ae maps hub: staged: CREATE GAME did not open");
		}
		else if (H.tab == AE_HUB_TAB_INSTALLED && H.state == AE_HUB_STATE_PLAY)
		{
			if (!ae_screen_campaign_open_on_level("a10"))
				platform_log("ae maps hub: staged: CAMPAIGN did not open");
		}
		else if (H.tab == AE_HUB_TAB_INSTALLED && H.state == AE_HUB_STATE_OPEN_FOLDER)
			open_map_folder(focused_map());
		else if (H.tab == AE_HUB_TAB_BROWSE && H.state == AE_HUB_STATE_AGAIN)
			browse_accept_row();
	}
	/* (a staged download moves a little each second) */
	if (H.staged && H.state == AE_HUB_STATE_DOWNLOAD)
		H.staged_progress = 42;
}

static struct ae_screen_class const hub_class =
{
	.name = "maps_hub", .leave = hub_leave, .handle = hub_handle, .draw = hub_draw, .pointer = hub_pointer, .update = hub_update,
};

/* ---------- opening */

static void hub_init(int staged)
{
	int each;

	memset(&H, 0, sizeof(H));
	H.staged = staged;
	H.flow.hit_rows = HIT_LIST;
	H.flow.hit_prompts = HIT_PROMPTS;
	for (each = 0; each < AE_HUB_TABS; each++)
		H.tab_defs[each].label = ae_hub_tab_name(each);
	H.tabs.tabs = H.tab_defs;
	H.tabs.count = AE_HUB_TABS;
	H.tabs.hover = -1;
	ae_text_init(&H.search, "", 48, 0);
	H.ctl = -1;
	H.first_frame = 1;
	H.logged_state = -1;
	H.opened = now_ms();
}

int ae_screen_maps_hub_open(int tab)
{
	if (tab < 0 || tab >= AE_HUB_TABS)
	{
		tab = (int)config_integer("menus.maps_tab");
		if (tab < 0 || tab >= AE_HUB_TABS)
			tab = AE_HUB_TAB_INSTALLED;
	}
	hub_init(0);
	if (!ae_ui_push(&hub_class, AE_OWNER_ANY, &H))
		return 0;
	page_enter(tab, AE_HUB_STATE_LIST);
	platform_log("ae maps hub: open on %s", ae_hub_tab_name(tab));
	return 1;
}

int ae_screen_maps_hub_open_staged(int tab, int state)
{
	/* (a question left open by the page staged before it is closed: the next scene opens its own) */
	while (ae_ui_depth() && ae_ui_top() && ae_ui_top()->screen_class && !strcmp(ae_ui_top()->screen_class->name, "dialog"))
		ae_ui_pop();
	if (ae_ui_depth() && ae_ui_top() && ae_ui_top()->screen_class == &hub_class)
		ae_ui_pop();
	hub_init(1);
	if (!ae_ui_push(&hub_class, AE_OWNER_ANY, &H))
		return 0;
	page_enter(tab, state);
	platform_log("ae maps hub: staged: %s, state %d", ae_hub_tab_name(tab), state);
	return 1;
}

const char *ae_screen_maps_hub_badge(void)
{
	static char text[24];
	struct ae_ce_archive_download status;

	ae_ce_archive_download_status(&status);
	/* (a map finished while another screen is up is still marked and listed: the main menu draws this each frame) */
	if (status.state != AE_CE_ARCHIVE_DOWNLOAD_NONE)
		ae_glue_ce_archive_poll();
	if (status.state != AE_CE_ARCHIVE_DOWNLOAD_RUNNING)
		return NULL;
	ae_hub_badge_text(1, text, sizeof(text));
	return text;
}

/* ---------- the WORKSHOP page's door (ae_screen_steam.c draws the page over this screen) */

void ae_screen_maps_hub_strip(struct ae_flow_screen *flow, int tab)
{
	float x = ae_flow_left_x(flow);

	H.tabs.active = (short)tab;
	ae_widget_tabs(&flow->density, &H.tabs, x, ae_frame_y(&flow->frame, 182.0f / 1080.0f),
		ae_flow_u(flow, 1030.0f), HIT_TABS);
}

int ae_screen_maps_hub_strip_pointer(struct ae_pointer const *pointer)
{
	if (!ae_tabs_pointer(&H.tabs, pointer, HIT_TABS))
		return 0;
	if (H.tabs.active != H.tab)
	{
		int tab = H.tabs.active;

		ae_ui_pop();
		tab_set(tab);
	}
	return 1;
}

void ae_screen_maps_hub_step(int direction)
{
	tab_set(ae_hub_tab_step(H.tab, direction));
}
