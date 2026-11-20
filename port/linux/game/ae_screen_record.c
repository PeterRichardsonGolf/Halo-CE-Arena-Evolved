/*
AE_SCREEN_RECORD.C

The service record's page (ae_screen_record.h). Its measures are in the post-game report's 1920 x 1080 design units.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "math/real_math.h"
#include "../src/ae_draw.h"
#include "../src/ae_layout.h"
#include "ae_glue_service_record.h"
#include "ae_hooks.h"
#include "ae_medal_pack.h"
#include "ae_pgcr_draw.h"
#include "ae_pgcr_screen_rules.h"
#include "ae_record_medals_rules.h"
#include "ae_record_focus.h"
#include "ae_record_pages.h"
#include "ae_record_step_rules.h"
#include "ae_record_columns_rules.h"
#include "ae_screen_record_details.h"
#include "ae_screen_record_window.h"
#include "ae_record_view.h"
#include "ae_screen_record.h"
#include "ae_screen_record_medals.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_test_hooks.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

#define DASH "\xE2\x80\x94"
#define BACKDROP 0x04070AFFu

enum
{
	RECORD_HIT = 0x7620,
	/* (0.8.1) the MEDALS pane, the page's one focusable part: its door to the MEDALS page */
	RECORD_PANE_HIT = 0x7621,
	/* (rc2) the page chips of the head */
	RECORD_CHIP_HIT = 0x7622,
	LEFT = 96, TOP = 232,
	LAST_W = 520, HIGHLIGHTS_X = 1344, HIGHLIGHTS_W = 480,
	PANE_PAD = 28, PANE_H = 440,
	RIVALS_Y = TOP + PANE_H + 24, RIVALS_H = 232
};

static struct
{
	int on_stack;
	char name[AE_SR_NAME];
	char viewer[AE_SR_NAME];
	/* the record shown: the store's, or DEV MODE's made-up one (record_medals), never written */
	struct ae_service_record const *record;
	/* the medal kinds of the player, as last drawn: 1 or more makes the MEDALS pane a door (on the MODES & MAPS page) */
	/* (rc2) the page shown: AE_RP_OVERVIEW, AE_RP_COMBAT or AE_RP_MODES */
	int page;
	/* the pointer's look (after a mouse move, until pad or key input): no ring, the hover wash on the pane, the chip under it */
	int mouse, hover, chip_hover;
	/* (rc2) the focused box (ae_record_focus.h's, AE_RF_NONE for none) and the boxes with a details page, as last drawn */
	int focus;
	unsigned long has;
	/* (rc2) the rival rows of one's own YOUR RIVALS strip as last drawn, and the one ringed while the strip has the focus */
	int rival_count, rival_row;
	char rival_name[4][AE_SR_NAME];
	struct ae_prompt prompts[5];
	short prompt_count;
} page;

static struct ae_screen_class const page_class;

/* (DEV MODE's record_medals: a record of one made-up player, in place of the store's for the page alone) */
static struct ae_service_record dev_record;

static struct ae_rw_source const window_source;

static int open_record(short owner, char const *name, char const *viewer, struct ae_service_record const *record, int first_page)
{
	char key[AE_SR_KEY];
	struct ae_sr_player const *player;
	int window_page;

	if (page.on_stack || !name || !name[0])
		return 0;
	memset(&page, 0, sizeof(page));
	snprintf(page.name, sizeof(page.name), "%s", name);
	snprintf(page.viewer, sizeof(page.viewer), "%s", viewer ? viewer : "");
	page.record = record;
	page.hover = -1;
	page.focus = AE_RF_NONE;
	page.chip_hover = -1;
	page.page = first_page >= 0 && first_page < AE_RP_PAGES ? first_page : AE_RP_OVERVIEW;
	window_page = first_page >= AE_RP_PAGES && first_page < AE_RP_PAGES + AE_RC_PAGES ? first_page - AE_RP_PAGES : -1;
	/* (a new page: the MEDALS page opens afresh from it) */
	ae_screen_record_medals_forget();
	if (!ae_ui_push(&page_class, owner, &page))
		return 0;
	page.on_stack = 1;
	ae_sr_key(name, key, sizeof(key));
	player = ae_sr_find(record, key);
	platform_log("ae record: page %s (%ld games)", name, player ? player->total.games : 0L);
	/* (debug.capture_on_event's "record_page": the tests' frame of the page) */
	ae_test_capture("record_page", 500);
	/* (DEV MODE's pages 4-7: the detail window over the record) */
	if (window_page >= 0)
		ae_screen_record_window_open(AE_OWNER_PARENT, &window_source, window_page, -1);
	return 1;
}

int ae_screen_record_open(short owner, char const *name, char const *viewer)
{
	return open_record(owner, name, viewer, ae_record_store(), AE_RP_OVERVIEW);
}

int ae_screen_record_dev(int kinds, unsigned long seed, int first_page, long counted, int mode_kinds, char *text, int size)
{
	static char const name[] = "Dev Record";
	struct ae_sr_player *player;
	unsigned long total;
	long games, kind[AE_RS_KINDS][5];
	int index;

	/* (an open page, and its MEDALS page, leave first: the made-up record takes their place) */
	ae_screen_record_medals_remove();
	ae_screen_record_details_remove();
	ae_screen_record_window_remove();
	if (page.on_stack)
		ae_ui_remove(&page);
	ae_sr_init(&dev_record);
	player = &dev_record.player[0];
	dev_record.player_count = 1;
	ae_sr_key(name, player->key, sizeof(player->key));
	snprintf(player->name, sizeof(player->name), "%s", name);
	ae_rm_dev_counts(player->medals, kinds, seed);
	total = ae_rm_total(player->medals);
	/* (games and kills to match: a game is about 30 medals) */
	games = (long)(total / 30 + 1);
	if (games < 7)
		games = 7;
	player->total.games = games;
	player->total.wins = player->total.games / 2;
	player->total.kills = (long)(total / 2 + 1) + games * 6;
	player->total.deaths = player->total.kills / 3 + 1;
	player->total.assists = player->total.kills / 5;
	ae_rs_demo_kinds_over(kind, player->total.games, player->total.wins, player->total.kills, player->total.deaths,
		player->total.assists, seed, mode_kinds);
	for (index = 0; index < AE_SR_MODES; index++)
	{
		player->mode[index].games = kind[index][0];
		player->mode[index].wins = kind[index][1];
		player->mode[index].kills = kind[index][2];
		player->mode[index].deaths = kind[index][3];
		player->mode[index].assists = kind[index][4];
	}
	/* (the new numbers: every game counted, or `counted` of them, the version 3 numbers with them; 8 to 12 maps, 5 weapons) */
	ae_rs_demo(&player->x, player->weapons, &player->weapon_count, player->map, &player->map_count, AE_SR_MAPS,
		8 + (int)(seed % 5), player->total.games, player->total.wins, player->total.kills, player->total.deaths,
		player->total.assists, counted < 0 ? player->total.games : counted, counted < 0 ? player->total.games : counted, seed);
	player->x.ties = player->total.games / 10;
	player->last.has = player->x.counted > 0;
	player->last.mode = AE_RS_KIND_FLAG;
	snprintf(player->last.map, sizeof(player->last.map), "%s", player->map_count > 1 ? player->map[1].name : "Prisoner");
	player->last.result = 1;
	player->last.kills = 11;
	player->last.deaths = 7;
	player->last.assists = 3;
	player->last.spree = 5;
	player->last.medals = 31;
	snprintf(player->last.weapon, sizeof(player->last.weapon), "%s", player->weapon_count ? player->weapons[0].name : "");
	if (!open_record(AE_OWNER_PARENT, name, name, &dev_record, first_page))
	{
		snprintf(text, (size_t)size, "the SERVICE RECORD page could not open (the menus' stack is full)");
		return 0;
	}
	snprintf(text, (size_t)size, "SERVICE RECORD of a made-up player: %d medal kinds, %lu medals, %ld games (seed %lu), page %d of 7, "
		"the new numbers from %ld games; the record file is not touched", ae_rm_earned(player->medals, AE_RM_ALL), total,
		player->total.games, seed, first_page + 1, player->x.counted);
	return 1;
}

static void page_leave(struct ae_screen *screen)
{
	(void)screen;
	page.on_stack = 0;
	ae_screen_record_medals_forget();
	platform_log("ae record: page closed");
}

/* the MEDALS page on a category's chip (the record window's ringed column; AE_RM_ALL from the overview's box) */
static void open_medals(int category)
{
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_screen_record_medals_open_at(AE_OWNER_PARENT, page.record, page.name, page.viewer, category);
}

static void go_to_page(int next)
{
	if (next == page.page || next < 0 || next >= AE_RP_PAGES)
		return;
	page.page = next;
	ae_sound_request(AE_SOUND_CURSOR, 0);
	platform_log("ae record: page %s: %s", page.name, ae_rp_page_name(next));
}

/* LT / RT (Page Up / Page Down): the next or previous player of the store's record (ae_record_step_rules.h), the page and the
focused box kept. A made-up record (DEV MODE's) steps nowhere */
static int step_target(int delta)
{
	char key[AE_SR_KEY], viewer_key[AE_SR_KEY];

	if (page.record != ae_record_store())
		return -1;
	ae_sr_key(page.name, key, sizeof(key));
	ae_sr_key(page.viewer, viewer_key, sizeof(viewer_key));
	return ae_rstep_target(page.record, viewer_key, key, delta);
}

static int step_player(int delta)
{
	int target = step_target(delta);

	if (target < 0)
		return 0;
	snprintf(page.name, sizeof(page.name), "%s", page.record->player[target].name);
	ae_screen_record_medals_forget();
	ae_sound_request(AE_SOUND_CURSOR, 0);
	platform_log("ae record: page %s (%ld games)", page.name, page.record->player[target].total.games);
	ae_test_capture("record_page", 500);
	return 1;
}

/* the sub line of the page: what the record covers */
static void sub_text(char *out, size_t size)
{
	char key[AE_SR_KEY], viewer_key[AE_SR_KEY];
	struct ae_sr_player const *player;

	ae_sr_key(page.name, key, sizeof(key));
	ae_sr_key(page.viewer, viewer_key, sizeof(viewer_key));
	player = ae_sr_find(page.record, key);
	if (player)
		snprintf(out, size, "SEEN IN %ld GAME%s ON THIS PC", player->total.games, player->total.games == 1 ? "" : "S");
	else if (viewer_key[0] && !strcmp(viewer_key, key))
		snprintf(out, size, "NO GAMES RECORDED ON THIS PC YET");
	else
		snprintf(out, size, "THIS PC HAS NOT SEEN THEM IN A GAME YET");
}

/* the detail window's source: the record shown here, LT / RT through this page's list */
static int window_fill(struct ae_rw_view *view)
{
	char key[AE_SR_KEY], viewer_key[AE_SR_KEY];
	struct ae_sr_player const *player;
	int order[AE_SR_PLAYERS], count, index;

	ae_sr_key(page.name, key, sizeof(key));
	ae_sr_key(page.viewer, viewer_key, sizeof(viewer_key));
	player = ae_sr_find(page.record, key);
	snprintf(view->name, sizeof(view->name), "%s", page.name);
	view->own = viewer_key[0] && !strcmp(viewer_key, key);
	view->rival = !view->own && ae_sr_is_rival(page.record, viewer_key, key);
	sub_text(view->sub, sizeof(view->sub));
	ae_rv_from_player(player, &view->in);
	if (player)
		view->last = player->last;
	view->position[0] = 0;
	if (page.record == ae_record_store())
	{
		count = ae_rstep_order(page.record, viewer_key, order);
		for (index = 0; index < count && count > 1; index++)
			if (!strcmp(page.record->player[order[index]].key, key))
				snprintf(view->position, sizeof(view->position), "%d OF %d", index + 1, count);
	}
	return player != NULL;
}

static int window_can_step(void)
{
	return step_target(1) >= 0;
}

static struct ae_rw_source const window_source =
{ window_fill, step_player, window_can_step, open_medals };

/* A on a box: its page of the detail window (the MEDALS box opens the MEDALS page) */
static void open_details(int box)
{
	char key[AE_SR_KEY];
	struct ae_sr_player const *player;
	int column, window_page;

	if (box == AE_RF_NONE || !(page.has & (1ul << box)))
		return;
	if (box == AE_RF_MEDALS)
	{
		open_medals(AE_RM_ALL);
		return;
	}
	/* (a rival's row: that player's record in this page's place; LT / RT and B as from any record) */
	if (box == AE_RF_RIVALS)
	{
		if (page.rival_row < 0 || page.rival_row >= page.rival_count)
			return;
		snprintf(page.name, sizeof(page.name), "%s", page.rival_name[page.rival_row]);
		page.focus = AE_RF_NONE;
		ae_screen_record_medals_forget();
		ae_sound_request(AE_SOUND_FORWARD, 0);
		platform_log("ae record: page %s (a rival)", page.name);
		ae_test_capture("record_page", 500);
		return;
	}
	window_page = ae_rc_page_of_box(box, &column);
	if (window_page < 0)
		return;
	ae_sr_key(page.name, key, sizeof(key));
	player = ae_sr_find(page.record, key);
	ae_sound_request(AE_SOUND_FORWARD, 0);
	if (window_page == AE_RC_MODES)
		ae_screen_record_window_open_kind(AE_OWNER_PARENT, &window_source, box == AE_RF_LAST && player ? player->last.mode : -1);
	else
		ae_screen_record_window_open(AE_OWNER_PARENT, &window_source, window_page, column);
}

static int page_handle(struct ae_screen *screen, struct ae_event const *event)
{
	(void)screen;
	/* (pad or key input: the ring again, no hover) */
	page.mouse = 0;
	page.hover = -1;
	page.chip_hover = -1;
	switch (event->action)
	{
	case AE_ACTION_TAB_NEXT:
	case AE_ACTION_Y:
		go_to_page(ae_rp_next(page.page));
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
		go_to_page((page.page + AE_RP_PAGES - 1) % AE_RP_PAGES);
		return 1;
	case AE_ACTION_PAGE_DOWN:
		step_player(1);
		return 1;
	case AE_ACTION_PAGE_UP:
		step_player(-1);
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
	{
		int next;

		/* (up / down inside the rivals' strip first) */
		if (page.focus == AE_RF_RIVALS && event->action == AE_ACTION_DOWN && page.rival_row + 1 < page.rival_count)
		{
			page.rival_row++;
			ae_sound_request(AE_SOUND_CURSOR, 0);
			return 1;
		}
		if (page.focus == AE_RF_RIVALS && event->action == AE_ACTION_UP && page.rival_row > 0)
		{
			page.rival_row--;
			ae_sound_request(AE_SOUND_CURSOR, 0);
			return 1;
		}
		next = ae_rf_move(page.page, page.has, page.focus, event->action == AE_ACTION_UP ? AE_RF_UP :
			event->action == AE_ACTION_DOWN ? AE_RF_DOWN : event->action == AE_ACTION_LEFT ? AE_RF_LEFT : AE_RF_RIGHT);

		if (next != page.focus)
		{
			page.focus = next;
			page.rival_row = 0;
			ae_sound_request(AE_SOUND_CURSOR, 0);
			platform_log("ae record: focus %s", ae_rf_name(next));
		}
		return 1;
	}
	case AE_ACTION_ACCEPT:
		open_details(page.focus);
		return 1;
	default:
		break;
	}
	return event->action == AE_ACTION_BACK ? 0 : 1;
}

static void page_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	int found;

	(void)screen;
	if (!pointer->touch && (pointer->moved || pointer->left_clicks || pointer->wheel_steps))
		page.mouse = 1;
	found = ae_hit_at(pointer->x, pointer->y, 0, &hit);
	if (pointer->moved)
	{
		page.hover = found && hit.id == RECORD_PANE_HIT && hit.part == AE_PART_CARD ? hit.index : -1;
		page.chip_hover = found && hit.id == RECORD_CHIP_HIT && hit.part == AE_PART_CHIP ? hit.index : -1;
	}
	if (pointer->left_clicks && found && hit.id == RECORD_CHIP_HIT && hit.part == AE_PART_CHIP)
	{
		go_to_page(hit.index);
		return;
	}
	if (pointer->left_clicks && found && hit.id == RECORD_PANE_HIT && hit.part == AE_PART_CARD)
	{
		page.focus = hit.index;
		open_details(hit.index);
		return;
	}
	if (ae_prompts_pointer(page.prompts, page.prompt_count, pointer, RECORD_HIT, &event))
		ae_ui_dispatch(&event);
}

/* the number of rivals the viewer has set */
static int rivals_of(struct ae_service_record const *record, char const *viewer_key)
{
	int index, count = 0;

	for (index = 0; viewer_key[0] && index < record->rival_count; index++)
		count += !strcmp(record->rival[index].owner, viewer_key);
	return count;
}

/* OVERVIEW's LAST GAME: the player's last game with the map and kind after its label */
static void draw_last_game(struct frame_metrics const *f, struct ae_sr_player const *player)
{
	struct ae_rv_cell cells[AE_RV_LAST];
	char map[AE_RS_MAP_NAME], kind[16];
	float left = LEFT + PANE_PAD, right = LEFT + LAST_W - PANE_PAD, y = TOP + PANE_PAD, w;
	int index;

	ae_rv_last_game(player ? &player->last : NULL, cells, map, kind, sizeof(map));
	ae_rp_pane(f, LEFT, TOP, LAST_W, PANE_H);
	w = put_tracked(f, AE_FONT_BODY, 16, 0.14f, left, y + 12.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, "LAST GAME");
	if (map[0])
	{
		/* (a drawn 4 u dot between each: never U+00B7) */
		rect(f, left + w + 10.0f, y + 10.0f, 4.0f, 4.0f, 2.0f, AE_COLOR_MUTED);
		w += 24.0f + put_tracked(f, AE_FONT_BODY, 16, 0.14f, left + w + 24.0f, y + 12.0f, AE_ALIGN_LEFT, AE_COLOR_TEXT, map);
		rect(f, left + w + 10.0f, y + 10.0f, 4.0f, 4.0f, 2.0f, AE_COLOR_MUTED);
		put_tracked(f, AE_FONT_BODY, 16, 0.14f, left + w + 24.0f, y + 12.0f, AE_ALIGN_LEFT, AE_COLOR_TEXT, kind);
	}
	y += 34.0f;
	for (index = 0; index < AE_RV_LAST; index++)
		ae_rp_cell(f, left, right, y + 17.0f + (float)index * 46.0f, &cells[index]);
}

static void draw_highlights(struct frame_metrics const *f, struct ae_rv_in const *in, int rivals)
{
	struct ae_rv_cell cells[AE_RV_HIGHLIGHTS];
	float left = HIGHLIGHTS_X + PANE_PAD, right = HIGHLIGHTS_X + HIGHLIGHTS_W - PANE_PAD, y;
	int index;

	ae_rv_highlights(in, rivals, cells);
	ae_rp_pane(f, HIGHLIGHTS_X, TOP, HIGHLIGHTS_W, PANE_H);
	y = ae_rp_label(f, left, TOP + PANE_PAD, "HIGHLIGHTS", NULL);
	for (index = 0; index < AE_RV_HIGHLIGHTS; index++)
		ae_rp_cell(f, left, right, y + 17.0f + (float)index * 46.0f, &cells[index]);
}

/* the strip under: on one's own record, one's rivals and the kills each way; on a rival's, the line against them */
static void draw_rivals(struct frame_metrics const *f, struct ae_service_record const *record)
{
	char viewer_key[AE_SR_KEY], own_key[AE_SR_KEY];
	float left = LEFT + PANE_PAD, right = LEFT + 1728.0f - PANE_PAD, y;
	int own, index, shown = 0;
	float const columns[3] = { right - 330.0f, right - 170.0f, right };
	static char const *const heads[3] = { "GAMES TOGETHER", "YOUR KILLS", "THEIR KILLS" };

	ae_sr_key(page.viewer, viewer_key, sizeof(viewer_key));
	ae_sr_key(page.name, own_key, sizeof(own_key));
	own = viewer_key[0] && !strcmp(viewer_key, own_key);
	page.rival_count = 0;
	ae_rp_pane(f, LEFT, RIVALS_Y, 1728.0f, RIVALS_H);
	if (!viewer_key[0])
	{
		ae_rp_label(f, left, RIVALS_Y + PANE_PAD, "RIVALS", NULL);
		return;
	}
	y = ae_rp_label(f, left, RIVALS_Y + PANE_PAD, own ? "YOUR RIVALS" : "YOU AGAINST THEM", NULL);
	for (index = 0; index < 3; index++)
		put_tracked(f, AE_FONT_BODY, 13, 0.14f, columns[index], y + 8.0f, AE_ALIGN_RIGHT, AE_COLOR_MUTED, heads[index]);
	y += 28.0f;
	for (index = 0; index < record->rival_count && shown < 4; index++)
	{
		struct ae_sr_rival const *rival = &record->rival[index];
		struct ae_sr_versus const *versus;
		char text[24], name[AE_SR_NAME];
		float cy = y + 17.0f + shown * 40.0f;

		if (strcmp(rival->owner, viewer_key) || (!own && strcmp(rival->key, own_key)))
			continue;
		versus = ae_sr_versus_of(record, viewer_key, rival->key);
		if (own)
		{
			snprintf(page.rival_name[shown], sizeof(page.rival_name[0]), "%s", rival->name);
			/* (the ringed row: a 4 u accent bar before its name) */
			if (page.focus == AE_RF_RIVALS && !page.mouse && page.rival_row == shown)
				rect(f, left - 16.0f, cy - 12.0f, 4.0f, 24.0f, 2.0f, AE_COLOR_ACCENT);
		}
		shown++;
		fit(f, AE_FONT_ROW, 20, rival->name, 420.0f, name, sizeof(name));
		put_tracked(f, AE_FONT_ROW, 20, 0.04f, left, cy, AE_ALIGN_LEFT, AE_PGCR_RIVAL_INK, name);
		snprintf(text, sizeof(text), "%ld", versus ? versus->games : 0L);
		put(f, AE_FONT_ROW, 20, columns[0], cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, text);
		snprintf(text, sizeof(text), "%ld", versus ? versus->kills : 0L);
		put(f, AE_FONT_ROW, 20, columns[1], cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, text);
		snprintf(text, sizeof(text), "%ld", versus ? versus->deaths : 0L);
		put(f, AE_FONT_ROW, 20, columns[2], cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, text);
	}
	page.rival_count = own ? shown : 0;
	if (!shown)
		put(f, AE_FONT_BODY, 18, left, y + 17.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, own ?
			"No rivals set. Open a player's card on the post-game report and choose SET RIVAL." :
			"Not one of your rivals. SET RIVAL on their card counts your kills against them.");
}

void ae_record_frame_begin(struct frame_metrics *f, struct ae_frame *frame, struct ae_density *density)
{
	struct ae_layout layout;

	ae_draw_view_full();
	ae_draw_current_layout(&layout);
	ae_frame_compute(layout.width, ae_settings_ui_scale(), frame);
	ae_draw_rect(0.0f, 0.0f, layout.width, layout.height, 0.0f, BACKDROP);
	f->m = frame->rect.width / 1920.0f;
	/* (a window taller than 16:9: the frame, scaled by width, sits in the middle of the height, not at its top) */
	if (layout.height > 1080.0f * f->m)
		frame->rect.y = (layout.height - 1080.0f * f->m) * 0.5f;
	f->x = frame->rect.x;
	f->y = frame->rect.y;
	ae_density_full(layout.height * layout.scale, ae_settings_ui_scale(), density);
}

void ae_record_head(struct frame_metrics const *f, struct ae_service_record const *record, char const *name, char const *viewer,
	char const *crumb, char const *sub)
{
	char key[AE_SR_KEY], viewer_key[AE_SR_KEY], text[96];
	float name_w, w;

	ae_sr_key(name, key, sizeof(key));
	ae_sr_key(viewer, viewer_key, sizeof(viewer_key));
	/* the head: the page's name (and the part of it shown), the player, and what the page is */
	w = put_tracked(f, AE_FONT_BODY, 18, 0.14f, LEFT, 96.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, "SERVICE RECORD");
	if (crumb && crumb[0])
	{
		/* (a drawn 4 u dot between, never U+00B7) */
		rect(f, LEFT + w + 12.0f, 94.0f, 4.0f, 4.0f, 2.0f, AE_COLOR_MUTED);
		put_tracked(f, AE_FONT_BODY, 18, 0.14f, LEFT + w + 28.0f, 96.0f, AE_ALIGN_LEFT, AE_COLOR_TEXT, crumb);
	}
	fit(f, AE_FONT_ROW, 44, name, 900.0f, text, sizeof(text));
	name_w = put(f, AE_FONT_ROW, 44, LEFT, 150.0f, AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
	if (viewer_key[0] && !strcmp(viewer_key, key))
		draw_you_tag(f, LEFT + name_w + 16.0f, 150.0f, 0);
	else if (ae_sr_is_rival(record, viewer_key, key))
		draw_rival_tag(f, LEFT + name_w + 16.0f, 150.0f, 0);
	put_tracked(f, AE_FONT_BODY, 18, 0.08f, LEFT, 196.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, sub);
}

/* the boxes of this record that have a details page now */
static unsigned long boxes_with_details(struct ae_rv_in const *view, struct ae_sr_last const *last)
{
	unsigned long has = 0;
	int box;

	for (box = 0; box < AE_RF_BOXES; box++)
		if (box != AE_RF_VERSUS && box != AE_RF_IDENTITY && box != AE_RF_RIVALS &&
			ae_rv_has_details(box, view, last))
			has |= 1ul << box;
	return has;
}

static void page_draw(struct ae_screen *screen)
{
	struct ae_frame frame;
	struct ae_density density;
	struct frame_metrics f;
	struct ae_service_record const *record = page.record;
	struct ae_sr_player const *player;
	struct ae_rv_in view;
	char key[AE_SR_KEY], viewer_key[AE_SR_KEY], text[96];

	(void)screen;
	ae_record_frame_begin(&f, &frame, &density);
	ae_sr_key(page.name, key, sizeof(key));
	ae_sr_key(page.viewer, viewer_key, sizeof(viewer_key));
	player = ae_sr_find(record, key);
	ae_rv_from_player(player, &view);
	sub_text(text, sizeof(text));
	/* (the focus: on a box of this page that has details, the first when it was on another page's) */
	page.has = boxes_with_details(&view, player ? &player->last : NULL);
	if (page.rival_count > 0)
		page.has |= 1ul << AE_RF_RIVALS;
	page.focus = ae_rf_keep(page.page, page.has, page.focus);
	ae_record_head(&f, record, page.name, page.viewer, ae_rp_crumb(page.page), text);
	ae_rp_chips(&f, page.page, page.mouse, page.chip_hover, RECORD_CHIP_HIT);
	if (page.page == AE_RP_OVERVIEW)
	{
		draw_last_game(&f, player);
		ae_rp_totals(&f, &view);
		draw_highlights(&f, &view, rivals_of(record, viewer_key));
		draw_rivals(&f, record);
	}
	else if (page.page == AE_RP_COMBAT)
		ae_rp_combat(&f, &view);
	else
		ae_rp_modes(&f, &view);
	ae_rp_boxes(&f, page.page, page.has, page.focus, page.mouse, page.hover, RECORD_PANE_HIT);
	/* the foot: A the focused box's details, LB / RB the page, LT / RT the next player (two or more kept), B back */
	page.prompt_count = 0;
	memset(page.prompts, 0, sizeof(page.prompts));
	if (page.focus != AE_RF_NONE)
	{
		page.prompts[page.prompt_count].button = AE_BUTTON_A;
		page.prompts[page.prompt_count].label = page.focus == AE_RF_MEDALS ? "All medals" : page.focus == AE_RF_RIVALS ? "Record" :
			"Details";
		page.prompts[page.prompt_count++].action = AE_ACTION_ACCEPT;
	}
	page.prompts[page.prompt_count].button = AE_BUTTON_LEFT_SHOULDER;
	page.prompts[page.prompt_count].label = "Page";
	page.prompts[page.prompt_count].action = AE_ACTION_TAB_PREVIOUS;
	page.prompts[page.prompt_count++].pair = 1;
	if (step_target(1) >= 0)
	{
		page.prompts[page.prompt_count].button = AE_BUTTON_LEFT_TRIGGER;
		page.prompts[page.prompt_count].label = "Next player";
		page.prompts[page.prompt_count].action = AE_ACTION_PAGE_UP;
		page.prompts[page.prompt_count++].pair = 1;
	}
	page.prompts[page.prompt_count].button = AE_BUTTON_B;
	page.prompts[page.prompt_count].label = "Back";
	page.prompts[page.prompt_count++].action = AE_ACTION_BACK;
	ae_widget_footer_band(&density, frame.rect.y + 1080.0f * f.m);
	ae_widget_prompts(&density, frame.rect.x + LEFT * f.m, frame.rect.y + 1020.0f * f.m, page.prompts, page.prompt_count,
		"THIS PC'S VIEW", frame.rect.x + (LEFT + 1728.0f) * f.m, -1, RECORD_HIT);
}

static struct ae_screen_class const page_class =
{
	.name = "service_record", .leave = page_leave, .handle = page_handle, .draw = page_draw, .pointer = page_pointer,
};
