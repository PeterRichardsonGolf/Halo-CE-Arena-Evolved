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
#include "ae_screen_record.h"
#include "ae_screen_record_medals.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_test_hooks.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

#define DASH "\xE2\x80\x94"
#define BACKDROP 0x04070AFFu
#define PANE_FILL 0x10161CF2u

enum
{
	RECORD_HIT = 0x7620,
	/* (0.8.1) the MEDALS pane, the page's one focusable part: its door to the MEDALS page */
	RECORD_PANE_HIT = 0x7621,
	RING_OUT = 6,
	LEFT = 96, TOP = 232,
	TOTALS_W = 520, KINDS_X = 648, KINDS_W = 664, MEDALS_X = 1344, MEDALS_W = 480,
	PANE_PAD = 28, PANE_H = 440,
	RIVALS_Y = TOP + PANE_H + 24, RIVALS_H = 232,
	MEDAL_LINES = 10
};

static struct
{
	int on_stack;
	char name[AE_SR_NAME];
	char viewer[AE_SR_NAME];
	/* the record shown: the store's, or DEV MODE's made-up one (record_medals), never written */
	struct ae_service_record const *record;
	/* the medal kinds of the player, as last drawn: 1 or more makes the MEDALS pane a door */
	int kinds;
	/* the pointer's look (after a mouse move, until pad or key input): no ring, the hover wash on the pane */
	int mouse, hover;
	struct ae_prompt prompts[2];
	short prompt_count;
} page;

static struct ae_screen_class const page_class;

/* (DEV MODE's record_medals: a record of one made-up player, in place of the store's for the page alone) */
static struct ae_service_record dev_record;

static int open_record(short owner, char const *name, char const *viewer, struct ae_service_record const *record)
{
	char key[AE_SR_KEY];
	struct ae_sr_player const *player;

	if (page.on_stack || !name || !name[0])
		return 0;
	memset(&page, 0, sizeof(page));
	snprintf(page.name, sizeof(page.name), "%s", name);
	snprintf(page.viewer, sizeof(page.viewer), "%s", viewer ? viewer : "");
	page.record = record;
	page.hover = 0;
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
	return 1;
}

int ae_screen_record_open(short owner, char const *name, char const *viewer)
{
	return open_record(owner, name, viewer, ae_record_store());
}

int ae_screen_record_dev(int kinds, unsigned long seed, char *text, int size)
{
	static char const name[] = "Dev Record";
	struct ae_sr_player *player;
	unsigned long total;

	/* (an open page, and its MEDALS page, leave first: the made-up record takes their place) */
	ae_screen_record_medals_remove();
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
	player->total.games = (long)(total / 30 + 1);
	player->total.wins = player->total.games / 2;
	player->total.kills = (long)(total / 2 + 1);
	player->total.deaths = player->total.kills / 2 + 1;
	player->total.assists = player->total.kills / 5;
	player->mode[0] = player->total;
	if (!open_record(AE_OWNER_PARENT, name, name, &dev_record))
	{
		snprintf(text, (size_t)size, "the SERVICE RECORD page could not open (the menus' stack is full)");
		return 0;
	}
	snprintf(text, (size_t)size, "SERVICE RECORD of a made-up player: %d medal kinds, %lu medals (seed %lu); the record file is "
		"not touched", ae_rm_earned(player->medals, AE_RM_ALL), total, seed);
	return 1;
}

static void page_leave(struct ae_screen *screen)
{
	(void)screen;
	page.on_stack = 0;
	ae_screen_record_medals_forget();
	platform_log("ae record: page closed");
}

static void open_medals(void)
{
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_screen_record_medals_open(AE_OWNER_PARENT, page.record, page.name, page.viewer);
}

static int page_handle(struct ae_screen *screen, struct ae_event const *event)
{
	(void)screen;
	/* (pad or key input: the ring again, no hover) */
	page.mouse = 0;
	page.hover = 0;
	if (event->action == AE_ACTION_ACCEPT && page.kinds > 0)
	{
		open_medals();
		return 1;
	}
	return event->action == AE_ACTION_BACK ? 0 : 1;
}

static void page_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	int over;

	(void)screen;
	if (!pointer->touch && (pointer->moved || pointer->left_clicks || pointer->wheel_steps))
		page.mouse = 1;
	over = page.kinds > 0 && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == RECORD_PANE_HIT;
	if (pointer->moved)
		page.hover = over;
	if (pointer->left_clicks && over)
	{
		open_medals();
		return;
	}
	if (ae_prompts_pointer(page.prompts, page.prompt_count, pointer, RECORD_HIT, &event))
		ae_ui_dispatch(&event);
}

static float section_label(struct frame_metrics const *f, float x, float y, char const *label, char const *tail)
{
	float w = put_tracked(f, AE_FONT_BODY, 16, 0.14f, x, y + 12.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);

	if (tail && tail[0])
		put_tracked(f, AE_FONT_BODY, 16, 0.14f, x + w + 14.0f, y + 12.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, tail);
	return y + 34.0f;
}

static void pane(struct frame_metrics const *f, float x, float y, float w, float h)
{
	rect(f, x, y, w, h, 3.0f, PANE_FILL);
	rect(f, x, y, w, 4.0f, 0.0f, AE_COLOR_ACCENT);
}

static void cell(struct frame_metrics const *f, float left, float right, float cy, char const *label, char const *value)
{
	put_tracked(f, AE_FONT_BODY, 16, 0.1f, left, cy, AE_ALIGN_LEFT, AE_COLOR_TEXT, label);
	put(f, AE_FONT_ROW, 24, right, cy, AE_ALIGN_RIGHT, strcmp(value, DASH) ? AE_COLOR_TITLE : AE_COLOR_MUTED, value);
}

static void percent_text(long wins, long games, char *out, size_t size)
{
	if (games <= 0)
		snprintf(out, size, DASH);
	else
		snprintf(out, size, "%ld%%", (wins * 100 + games / 2) / games);
}

static void draw_totals(struct frame_metrics const *f, struct ae_sr_player const *player)
{
	static char const *const labels[8] = { "GAMES", "WINS", "WIN RATE", "KILLS", "DEATHS", "ASSISTS", "K/D", "MEDALS" };
	float left = LEFT + PANE_PAD, right = LEFT + TOTALS_W - PANE_PAD, y;
	char values[8][24];
	long medals = 0;
	int index;

	pane(f, LEFT, TOP, TOTALS_W, PANE_H);
	y = section_label(f, left, TOP + PANE_PAD, "TOTALS", NULL);
	if (player)
	{
		for (index = 0; index < AE_MEDAL_COUNT; index++)
			medals += (long)player->medals[index];
		snprintf(values[0], sizeof(values[0]), "%ld", player->total.games);
		snprintf(values[1], sizeof(values[1]), "%ld", player->total.wins);
		percent_text(player->total.wins, player->total.games, values[2], sizeof(values[2]));
		snprintf(values[3], sizeof(values[3]), "%ld", player->total.kills);
		snprintf(values[4], sizeof(values[4]), "%ld", player->total.deaths);
		snprintf(values[5], sizeof(values[5]), "%ld", player->total.assists);
		ae_pgr_kd_text(player->total.kills, player->total.deaths, values[6], sizeof(values[6]));
		snprintf(values[7], sizeof(values[7]), "%ld", medals);
	}
	else
	{
		for (index = 0; index < 8; index++)
			snprintf(values[index], sizeof(values[index]), DASH);
	}
	for (index = 0; index < 8; index++)
		cell(f, left, right, y + 17.0f + index * 46.0f, labels[index], values[index]);
}

static void draw_kinds(struct frame_metrics const *f, struct ae_sr_player const *player)
{
	float left = KINDS_X + PANE_PAD, right = KINDS_X + KINDS_W - PANE_PAD, y;
	/* (right edges of the number columns) */
	float const columns[6] = { right - 340.0f, right - 268.0f, right - 196.0f, right - 130.0f, right - 68.0f, right };
	static char const *const heads[6] = { "GAMES", "WINS", "K", "D", "A", "K/D" };
	int kind, shown = 0, index;

	pane(f, KINDS_X, TOP, KINDS_W, PANE_H);
	y = section_label(f, left, TOP + PANE_PAD, "BY KIND OF GAME", NULL);
	for (index = 0; index < 6; index++)
		put_tracked(f, AE_FONT_BODY, 13, 0.14f, columns[index], y + 8.0f, AE_ALIGN_RIGHT, AE_COLOR_MUTED, heads[index]);
	y += 28.0f;
	for (kind = 0; player && kind < AE_SR_MODES; kind++)
	{
		struct ae_sr_line const *line = &player->mode[kind];
		char text[24];
		float cy = y + 17.0f + shown * 46.0f;

		if (!line->games)
			continue;
		shown++;
		put_tracked(f, AE_FONT_ROW, 20, 0.04f, left, cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, ae_sr_mode_name(kind));
		snprintf(text, sizeof(text), "%ld", line->games);
		put(f, AE_FONT_ROW, 20, columns[0], cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, text);
		snprintf(text, sizeof(text), "%ld", line->wins);
		put(f, AE_FONT_ROW, 20, columns[1], cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, text);
		snprintf(text, sizeof(text), "%ld", line->kills);
		put(f, AE_FONT_ROW, 20, columns[2], cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, text);
		snprintf(text, sizeof(text), "%ld", line->deaths);
		put(f, AE_FONT_ROW, 20, columns[3], cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, text);
		snprintf(text, sizeof(text), "%ld", line->assists);
		put(f, AE_FONT_ROW, 20, columns[4], cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, text);
		ae_pgr_kd_text(line->kills, line->deaths, text, sizeof(text));
		put(f, AE_FONT_ROW, 20, columns[5], cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, text);
	}
	if (!shown)
		put(f, AE_FONT_ROW, 24, left, y + 17.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, DASH);
}

/* the pane's last line: "+26 more", a dot, then ALL MEDALS (the page it opens); ALL MEDALS alone when nothing is hidden */
static void draw_medals_link(struct frame_metrics const *f, float right, float cy, int hidden)
{
	char text[24];
	float w = put_tracked(f, AE_FONT_BODY, 16, 0.10f, right, cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, "ALL MEDALS"), dot;

	if (hidden <= 0)
		return;
	dot = right - w - 12.0f;
	rect(f, dot - 2.0f, cy - 2.0f, 4.0f, 4.0f, 2.0f, AE_COLOR_MUTED);
	snprintf(text, sizeof(text), "+%d more", hidden);
	put(f, AE_FONT_BODY, 16, dot - 12.0f, cy, AE_ALIGN_RIGHT, AE_COLOR_MUTED, text);
}

static void draw_medals(struct frame_metrics const *f, struct ae_sr_player const *player)
{
	static unsigned long const none[AE_MEDAL_COUNT];
	/* (the MEDALS page's own order, RAREST FIRST: its first cells are this pane's rows) */
	int medals[AE_MEDAL_COUNT], count, shown, index;
	float left = MEDALS_X + PANE_PAD, right = MEDALS_X + MEDALS_W - PANE_PAD, y;
	char text[24];

	ae_rm_list(player ? player->medals : none, AE_RM_SORT_RAREST, AE_RM_ALL, medals);
	count = ae_rm_earned(player ? player->medals : none, AE_RM_ALL);
	page.kinds = count;
	pane(f, MEDALS_X, TOP, MEDALS_W, PANE_H);
	if (count > 0)
	{
		/* the door: one hit over the whole pane; the mouse's hover wash, or (pad and keys) the 2 u white ring 6 u out */
		ae_hit_add(tx(f, MEDALS_X), ty(f, TOP), MEDALS_W * f->m, PANE_H * f->m, RECORD_PANE_HIT, AE_PART_CARD, 0);
		if (page.mouse && page.hover)
			rect(f, MEDALS_X, TOP, MEDALS_W, PANE_H, 3.0f, AE_COLOR_HOVER);
		else if (!page.mouse)
			ae_draw_outline(tx(f, MEDALS_X - RING_OUT), ty(f, TOP - RING_OUT), (MEDALS_W + 2.0f * RING_OUT) * f->m,
				(PANE_H + 2.0f * RING_OUT) * f->m, 3.0f * f->m, 2.0f * f->m, AE_COLOR_RING);
	}
	snprintf(text, sizeof(text), "%d KINDS", count);
	y = section_label(f, left, TOP + PANE_PAD, "MEDALS", count ? text : NULL);
	shown = count < MEDAL_LINES ? count : MEDAL_LINES;
	for (index = 0; index < shown; index++)
	{
		char name[AE_PGR_NAME];
		float cy = y + 17.0f + index * 34.0f;
		unsigned int color = ae_pgr_family_color(ae_pgr_medal_family(medals[index]));

		ae_pgr_medal_name(medals[index], name, sizeof(name));
		snprintf(text, sizeof(text), "%lu", player->medals[medals[index]]);
		if (ae_medal_pack_menu_image(medals[index]))
		{
			ae_draw_image(ae_medal_pack_menu_image(medals[index]), tx(f, left), ty(f, cy - 14.0f), 28.0f * f->m, 28.0f * f->m,
				0xFFFFFFFFu);
			put_tracked(f, AE_FONT_BODY, 17, 0.06f, left + 40.0f, cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, name);
		}
		else
			put_tracked(f, AE_FONT_BODY, 17, 0.06f, left, cy, AE_ALIGN_LEFT, color, name);
		put(f, AE_FONT_ROW, 20, right, cy, AE_ALIGN_RIGHT, AE_COLOR_TITLE, text);
	}
	if (count > 0)
		draw_medals_link(f, right, TOP + PANE_H - 28.0f, count - shown);
	if (!count)
		put(f, AE_FONT_ROW, 24, left, y + 17.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, DASH);
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
	pane(f, LEFT, RIVALS_Y, 1728.0f, RIVALS_H);
	if (!viewer_key[0])
	{
		section_label(f, left, RIVALS_Y + PANE_PAD, "RIVALS", NULL);
		return;
	}
	y = section_label(f, left, RIVALS_Y + PANE_PAD, own ? "YOUR RIVALS" : "YOU AGAINST THEM", NULL);
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

static void page_draw(struct ae_screen *screen)
{
	struct ae_frame frame;
	struct ae_density density;
	struct frame_metrics f;
	struct ae_service_record const *record = page.record;
	struct ae_sr_player const *player;
	char key[AE_SR_KEY], viewer_key[AE_SR_KEY], text[96];

	(void)screen;
	ae_record_frame_begin(&f, &frame, &density);
	ae_sr_key(page.name, key, sizeof(key));
	ae_sr_key(page.viewer, viewer_key, sizeof(viewer_key));
	player = ae_sr_find(record, key);
	if (player)
		snprintf(text, sizeof(text), "SEEN IN %ld GAME%s ON THIS PC", player->total.games, player->total.games == 1 ? "" : "S");
	else if (viewer_key[0] && !strcmp(viewer_key, key))
		snprintf(text, sizeof(text), "NO GAMES RECORDED ON THIS PC YET");
	else
		snprintf(text, sizeof(text), "THIS PC HAS NOT SEEN THEM IN A GAME YET");
	ae_record_head(&f, record, page.name, page.viewer, NULL, text);
	draw_totals(&f, player);
	draw_kinds(&f, player);
	draw_medals(&f, player);
	draw_rivals(&f, record);
	/* the foot: A opens the MEDALS page while the pane is a door */
	page.prompt_count = 0;
	memset(page.prompts, 0, sizeof(page.prompts));
	if (page.kinds > 0)
	{
		page.prompts[page.prompt_count].button = AE_BUTTON_A;
		page.prompts[page.prompt_count].label = "All medals";
		page.prompts[page.prompt_count++].action = AE_ACTION_ACCEPT;
	}
	page.prompts[page.prompt_count].button = AE_BUTTON_B;
	page.prompts[page.prompt_count].label = "Back";
	page.prompts[page.prompt_count++].action = AE_ACTION_BACK;
	ae_widget_prompts(&density, frame.rect.x + LEFT * f.m, frame.rect.y + 1020.0f * f.m, page.prompts, page.prompt_count,
		"THIS PC'S VIEW", frame.rect.x + (LEFT + 1728.0f) * f.m, -1, RECORD_HIT);
}

static struct ae_screen_class const page_class =
{
	.name = "service_record", .leave = page_leave, .handle = page_handle, .draw = page_draw, .pointer = page_pointer,
};
