/*
AE_SCREEN_BOT_RECORD.C

BOT RECORD, a tournament bot's SERVICE RECORD page (ae_screen_bot_record.h). Its measures are the SERVICE RECORD page's
1920 x 1080 design units (ae_screen_record.c): the same head, the same three panes at TOP and the strip under them.
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "math/real_math.h"
#include "../src/ae_draw.h"
#include "../src/ae_layout.h"
#include "ae_bot_record_rules.h"
#include "ae_bots_seat_rules.h"
#include "ae_bots_style_rules.h"
#include "ae_dev_events.h"
#include "ae_glue_bot_records.h"
#include "ae_glue_social.h"
#include "ae_hooks.h"
#include "ae_pgcr_draw.h"
#include "ae_pgcr_screen_rules.h"
#include "ae_record_focus.h"
#include "ae_record_pages.h"
#include "ae_record_medals_rules.h"
#include "ae_record_columns_rules.h"
#include "ae_screen_record_details.h"
#include "ae_screen_record_window.h"
#include "ae_record_view.h"
#include "ae_screen_record_medals.h"
#include "ae_screen_bot_record.h"
#include "ae_screen_record.h"
#include "ae_social_playlists.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_test_hooks.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

#define DASH "\xE2\x80\x94"
#define PANE_FILL 0x10161CF2u

enum
{
	BOT_PAGE_HIT = 0x7628,
	/* (rc2) the page chips of the head, and the MEDALS pane's door */
	BOT_CHIP_HIT = 0x7629,
	BOT_PANE_HIT = 0x762A,
	LEFT = 96, TOP = 232,
	GAME_W = 520, TOTALS_X = 648, TOTALS_W = 664, IDENTITY_X = 1344, IDENTITY_W = 480,
	PANE_PAD = 28, PANE_H = 440,
	VERSUS_Y = TOP + PANE_H + 24, VERSUS_W = 1728, VERSUS_H = 232,
	CELL_PITCH = 46,
	/* VS YOU: the head words' place, the lines (1-2 profiles with their sentence, 3-4 without) */
	VERSUS_HEAD_X = 372, VERSUS_LINE = 64, VERSUS_LINE_SHORT = 40
};

static struct
{
	int on_stack;
	int index;
	/* (rc2) the page shown (AE_RP_*), the pointer's look, and the medal kinds of the bot as last drawn */
	int page, mouse, hover, chip_hover;
	/* the focused box and the boxes with a details page, as last drawn */
	int focus;
	unsigned long has;
	struct ae_prompt prompts[6];
	short prompt_count;
} page;

/* the bot's medals as a service record of one player, for the MEDALS page it opens (read through the same list code) */
static struct ae_service_record medals_view;

static struct ae_bot_page_source source;

static struct ae_screen_class const page_class;

/* ---------- the source and the stack */

struct ae_bot_page_source *ae_screen_bot_record_source(void)
{
	if (page.on_stack)
		return NULL;
	memset(&source, 0, sizeof(source));
	return &source;
}

static struct ae_bot_record const *current_record(void)
{
	return ae_bot_record_find(ae_bot_records_active(), source.name[page.index]);
}

static void announce(void)
{
	struct ae_bot_record const *record = current_record();
	char name[AE_BOT_RECORD_KEY * 2];

	platform_log("ae botrecord: page %s (%ld games)%s", source.name[page.index], record ? record->games : 0L,
		source.from_career ? " from CAREER" : "");
	ae_dev_event_text(name, sizeof(name), source.name[page.index]);
	ae_dev_event("bot_record_page", "\"bot\":\"%s\",\"games\":%ld,\"from\":\"%s\",\"profiles\":%d,\"demo\":%d", name,
		record ? record->games : 0L, source.from_career ? "career" : "postgame", source.profiles, ae_bot_records_demo_on());
	/* (debug.capture_on_event's "bot_record_page": the tests' frame of the page) */
	ae_test_capture("bot_record_page", 500);
}

static struct ae_rw_source const window_source;

int ae_screen_bot_record_open(short owner, int index, int first_page)
{
	int window_page;

	if (page.on_stack || source.count <= 0)
		return 0;
	memset(&page, 0, sizeof(page));
	page.index = index >= 0 && index < source.count ? index : 0;
	page.page = first_page >= 0 && first_page < AE_RP_PAGES ? first_page : AE_RP_OVERVIEW;
	window_page = first_page >= AE_RP_PAGES && first_page < AE_RP_PAGES + AE_RC_PAGES ? first_page - AE_RP_PAGES : -1;
	page.chip_hover = -1;
	page.hover = -1;
	page.focus = AE_RF_NONE;
	if (!ae_ui_push(&page_class, owner, &page))
		return 0;
	page.on_stack = 1;
	announce();
	/* (DEV MODE's pages 4-7: the detail window over the record) */
	if (window_page >= 0)
	{
		ae_screen_record_window_remove();
		ae_screen_record_window_open(AE_OWNER_PARENT, &window_source, window_page, -1);
	}
	return 1;
}

int ae_screen_bot_record_is_open(void)
{
	return page.on_stack;
}

void ae_screen_bot_record_remove(void)
{
	ae_screen_record_window_remove();
	if (page.on_stack)
		ae_ui_remove(&page);
}

static void page_leave(struct ae_screen *screen)
{
	(void)screen;
	page.on_stack = 0;
	ae_screen_record_medals_forget();
	ae_screen_record_details_remove();
	ae_screen_record_window_remove();
	platform_log("ae botrecord: page closed");
}

static void go_to_page(int next)
{
	if (next == page.page || next < 0 || next >= AE_RP_PAGES)
		return;
	page.page = next;
	ae_sound_request(AE_SOUND_CURSOR, 0);
	platform_log("ae botrecord: page %s: %s", source.name[page.index], ae_rp_page_name(next));
}

/* the MEDALS page of this bot: its medals in a record of one player (the bot's name as the player) */
static void open_medals(struct ae_bot_record const *record, int category)
{
	struct ae_sr_player *player = &medals_view.player[0];
	int medal;

	if (!record)
		return;
	ae_sr_init(&medals_view);
	medals_view.player_count = 1;
	ae_sr_key(record->name, player->key, sizeof(player->key));
	snprintf(player->name, sizeof(player->name), "%s", record->name);
	player->total.games = record->games;
	for (medal = 0; medal < AE_MEDAL_COUNT; medal++)
		player->medals[medal] = record->medals[medal];
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_screen_record_medals_forget();
	ae_screen_record_medals_open_at(AE_OWNER_PARENT, &medals_view, record->name, "", category);
}

static void open_details(int box);

static void page_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	int found;

	(void)screen;
	if (!pointer->touch && (pointer->moved || pointer->left_clicks))
		page.mouse = 1;
	found = ae_hit_at(pointer->x, pointer->y, 0, &hit);
	if (pointer->moved)
	{
		page.hover = found && hit.id == BOT_PANE_HIT && hit.part == AE_PART_CARD ? hit.index : -1;
		page.chip_hover = found && hit.id == BOT_CHIP_HIT && hit.part == AE_PART_CHIP ? hit.index : -1;
	}
	if (pointer->left_clicks && found && hit.id == BOT_CHIP_HIT && hit.part == AE_PART_CHIP)
	{
		go_to_page(hit.index);
		return;
	}
	if (pointer->left_clicks && found && hit.id == BOT_PANE_HIT && hit.part == AE_PART_CARD)
	{
		page.focus = hit.index;
		open_details(hit.index);
		return;
	}
	if (ae_prompts_pointer(page.prompts, page.prompt_count, pointer, BOT_PAGE_HIT, &event))
		ae_ui_dispatch(&event);
}

/* ---------- words */

static void upper_copy(char const *text, char *out, size_t size)
{
	size_t at = 0;

	for (; text && *text && at + 1 < size; text++)
		out[at++] = *text == '-' || *text == '_' ? ' ' : (char)toupper((unsigned char)*text);
	if (size)
		out[at] = 0;
}

/* a playlist's name from its id ("team-slayer": the playlists' TEAM SLAYER), else the id in capitals */
static void playlist_name(char const *id, char *out, size_t size)
{
	int index, count = ae_social_playlist_count();

	for (index = 0; index < count; index++)
	{
		struct ae_social_playlist const *playlist = ae_social_playlist_at(index);

		if (playlist && !strcmp(playlist->id, id))
		{
			upper_copy(playlist->label, out, size);
			return;
		}
	}
	upper_copy(id, out, size);
}

static void place_text(int place, char *out, size_t size)
{
	int n = place + 1, tens = n % 100;
	char const *tail = tens >= 11 && tens <= 13 ? "TH" : n % 10 == 1 ? "ST" : n % 10 == 2 ? "ND" : n % 10 == 3 ? "RD" : "TH";

	snprintf(out, size, "%d%s", n, tail);
}

static void number_text(long value, char *out, size_t size)
{
	if (value < 0)
		snprintf(out, size, DASH);
	else
		snprintf(out, size, "%ld", value);
}

/* ---------- drawing */

static float section_label(struct frame_metrics const *f, float x, float y, char const *label)
{
	put_tracked(f, AE_FONT_BODY, 16, 0.14f, x, y + 12.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	return y + 34.0f;
}

static void pane(struct frame_metrics const *f, float x, float y, float w, float h)
{
	rect(f, x, y, w, h, 3.0f, PANE_FILL);
	rect(f, x, y, w, 4.0f, 0.0f, AE_COLOR_ACCENT);
}

/* a label left, its value right (row 24 white; a dash muted), the value fitted to the room the label leaves */
static void cell(struct frame_metrics const *f, float left, float right, float cy, char const *label, char const *value)
{
	char fitted[64];
	float w = put_tracked(f, AE_FONT_BODY, 16, 0.1f, left, cy, AE_ALIGN_LEFT, AE_COLOR_TEXT, label);

	fit(f, AE_FONT_ROW, 24, value, right - left - w - 20.0f, fitted, sizeof(fitted));
	put(f, AE_FONT_ROW, 24, right, cy, AE_ALIGN_RIGHT, strcmp(value, DASH) ? AE_COLOR_TITLE : AE_COLOR_MUTED, fitted);
}

static void draw_head(struct frame_metrics const *f, struct ae_bot_record const *record)
{
	char text[96], playlist[48];
	float w, x;

	w = put_tracked(f, AE_FONT_BODY, 18, 0.14f, LEFT, 96.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, "SERVICE RECORD");
	/* (a drawn 4 u dot between, never U+00B7) */
	rect(f, LEFT + w + 12.0f, 94.0f, 4.0f, 4.0f, 2.0f, AE_COLOR_MUTED);
	put_tracked(f, AE_FONT_BODY, 18, 0.14f, LEFT + w + 28.0f, 96.0f, AE_ALIGN_LEFT, AE_COLOR_TEXT,
		page.page == AE_RP_OVERVIEW ? "BOT" : ae_rp_crumb(page.page));
	fit(f, AE_FONT_ROW, 44, record ? record->name : source.name[page.index], 900.0f, text, sizeof(text));
	w = put(f, AE_FONT_ROW, 44, LEFT, 150.0f, AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
	draw_outlined_tag(f, LEFT + w + 16.0f, 150.0f, "BOT", AE_COLOR_MUTED);
	if (!record)
	{
		put_tracked(f, AE_FONT_BODY, 18, 0.08f, LEFT, 196.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED,
			"NOT MET IN A TOURNAMENT GAME ON THIS PC YET");
		return;
	}
	snprintf(text, sizeof(text), "MET IN %ld TOURNAMENT GAME%s ON THIS PC", record->games, record->games == 1 ? "" : "S");
	x = LEFT + put_tracked(f, AE_FONT_BODY, 18, 0.08f, LEFT, 196.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
	if (record->first_playlist[0])
	{
		rect(f, x + 12.0f, 194.0f, 4.0f, 4.0f, 2.0f, AE_COLOR_MUTED);
		playlist_name(record->first_playlist, playlist, sizeof(playlist));
		snprintf(text, sizeof(text), "FIRST IN %s", playlist);
		put_tracked(f, AE_FONT_BODY, 18, 0.08f, x + 28.0f, 196.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
	}
}

/* a pane's label with its tail after it: the map, a dot, the playlist (both in capitals, the playlist fitted) */
static void label_with_tail(struct frame_metrics const *f, float left, float right, float y, char const *label,
	char const *map, char const *playlist)
{
	char fitted[100];
	float w = put_tracked(f, AE_FONT_BODY, 16, 0.14f, left, y + 12.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);

	if (!map[0] && !playlist[0])
		return;
	if (map[0])
	{
		rect(f, left + w + 10.0f, y + 10.0f, 4.0f, 4.0f, 2.0f, AE_COLOR_MUTED);
		w += 24.0f + put_tracked(f, AE_FONT_BODY, 16, 0.14f, left + w + 24.0f, y + 12.0f, AE_ALIGN_LEFT, AE_COLOR_TEXT, map);
	}
	if (playlist[0])
	{
		rect(f, left + w + 10.0f, y + 10.0f, 4.0f, 4.0f, 2.0f, AE_COLOR_MUTED);
		fit(f, AE_FONT_BODY, 16, playlist, right - left - w - 26.0f, fitted, sizeof(fitted));
		put_tracked(f, AE_FONT_BODY, 16, 0.14f, left + w + 24.0f, y + 12.0f, AE_ALIGN_LEFT, AE_COLOR_TEXT, fitted);
	}
}

/* THIS GAME (the board's numbers), or from CAREER the LAST GAME TOGETHER of the page's first profile */
static void draw_game(struct frame_metrics const *f, struct ae_bot_record const *record)
{
	float left = LEFT + PANE_PAD, right = LEFT + GAME_W - PANE_PAD, y = TOP + PANE_PAD;
	char const *labels[8];
	char values[8][40], kills_label[40];
	int count = 0, index;

	pane(f, LEFT, TOP, GAME_W, PANE_H);
	for (index = 0; index < 8; index++)
		snprintf(values[index], sizeof(values[index]), DASH);
	if (!source.from_career)
	{
		struct ae_bot_page_game const *game = page.index < AE_BOT_PAGE_GAMES ? &source.game[page.index] : NULL;
		int has = game && game->has;

		label_with_tail(f, left, right, y, "THIS GAME", source.map, source.playlist);
		y += 34.0f;
		labels[0] = "RESULT";
		labels[1] = "KILLS";
		labels[2] = "DEATHS";
		labels[3] = "ASSISTS";
		labels[4] = "K/D";
		labels[5] = "BEST SPREE";
		labels[6] = "MOST KILLS WITH";
		/* (split screen: two profiles each their own, three or four the sum: VS YOU has each) */
		snprintf(kills_label, sizeof(kills_label), source.profiles == 2 ? "ITS KILLS ON YOU (P1 / P2)" : "ITS KILLS ON YOU");
		labels[7] = kills_label;
		count = 8;
		if (has)
		{
			long sum = 0;

			snprintf(values[0], sizeof(values[0]), "%s", game->result[0] ? game->result : DASH);
			number_text(game->kills, values[1], sizeof(values[1]));
			number_text(game->deaths, values[2], sizeof(values[2]));
			number_text(game->assists, values[3], sizeof(values[3]));
			ae_pgr_kd_text(game->kills, game->deaths, values[4], sizeof(values[4]));
			number_text(game->spree, values[5], sizeof(values[5]));
			if (game->weapon[0])
				snprintf(values[6], sizeof(values[6]), "%s", game->weapon);
			for (index = 0; index < source.profiles && index < AE_BOT_PAGE_PROFILES; index++)
				sum += game->its_kills_on[index];
			if (source.profiles == 2)
				snprintf(values[7], sizeof(values[7]), "%ld / %ld", game->its_kills_on[0], game->its_kills_on[1]);
			else if (source.profiles > 0)
				snprintf(values[7], sizeof(values[7]), "%ld", sum);
		}
	}
	else
	{
		struct ae_bot_versus const *versus = record && source.profiles > 0 ?
			ae_bot_record_versus(ae_bot_records_active(), source.profile_key[0], record->key) : NULL;
		struct ae_bot_lastgame const *last = versus && versus->lastgame.has ? &versus->lastgame : NULL;
		char map[48], playlist[48];

		map[0] = playlist[0] = 0;
		if (last)
		{
			upper_copy(last->map, map, sizeof(map));
			playlist_name(last->playlist, playlist, sizeof(playlist));
		}
		label_with_tail(f, left, right, y, "LAST GAME TOGETHER", map, playlist);
		y += 34.0f;
		labels[0] = "RESULT";
		labels[1] = "KILLS";
		labels[2] = "DEATHS";
		labels[3] = "ASSISTS";
		labels[4] = "K/D";
		labels[5] = "BEST SPREE";
		labels[6] = "ITS KILLS ON YOU";
		count = 7;
		if (last)
		{
			if (last->place >= 0)
				place_text(last->place, values[0], sizeof(values[0]));
			else
				snprintf(values[0], sizeof(values[0]), "%s", last->result == AE_BOT_RECORD_WIN ? "WIN" :
					last->result == AE_BOT_RECORD_TIE ? "TIE" : "LOSS");
			number_text(last->kills, values[1], sizeof(values[1]));
			number_text(last->deaths, values[2], sizeof(values[2]));
			number_text(last->assists, values[3], sizeof(values[3]));
			ae_pgr_kd_text(last->kills, last->deaths, values[4], sizeof(values[4]));
			number_text(last->spree, values[5], sizeof(values[5]));
			number_text(last->its_kills, values[6], sizeof(values[6]));
		}
	}
	for (index = 0; index < count; index++)
		cell(f, left, right, y + 17.0f + index * (float)CELL_PITCH, labels[index], values[index]);
}

/* a bot's record as the pages' words read it (ae_record_view.h); weapons are copied to the view's own type */
static void bot_view(struct ae_bot_record const *record, struct ae_rv_in *in, struct ae_rs_weapon *weapons)
{
	static struct ae_rs_stats const none;
	static unsigned long const no_medals[AE_MEDAL_COUNT];
	int index;

	memset(in, 0, sizeof(*in));
	in->x = &none;
	in->medals = no_medals;
	in->spree_from_record = 1;
	if (!record)
		return;
	in->games = record->games;
	in->wins = record->wins;
	in->ties = record->ties;
	in->kills = record->kills;
	in->deaths = record->deaths;
	in->assists = record->assists;
	in->best_spree = record->best_spree;
	in->x = &record->x;
	in->medals = record->medals;
	for (index = 0; index < AE_RS_KINDS && index < AE_BOT_RECORD_KINDS; index++)
	{
		in->kind[index].games = record->kind[index].games;
		in->kind[index].wins = record->kind[index].wins;
		in->kind[index].kills = record->kind[index].kills;
		in->kind[index].deaths = record->kind[index].deaths;
		in->kind[index].assists = record->kind[index].assists;
	}
	in->maps = record->map;
	in->map_count = record->map_count;
	for (index = 0; index < record->weapon_count && index < AE_RS_WEAPONS; index++)
	{
		snprintf(weapons[index].name, sizeof(weapons[index].name), "%s", record->weapons[index].name);
		weapons[index].kills = record->weapons[index].kills;
	}
	in->weapons = weapons;
	in->weapon_count = record->weapon_count < AE_RS_WEAPONS ? record->weapon_count : AE_RS_WEAPONS;
}

static void draw_identity(struct frame_metrics const *f, struct ae_bot_record const *record)
{
	static char const *const labels[5] = { "SKILL", "TIER", "PLAY STYLE", "FAVOURITE WEAPON", "MOST KILLS WITH" };
	float left = IDENTITY_X + PANE_PAD, right = IDENTITY_X + IDENTITY_W - PANE_PAD, y;
	char values[5][48];
	int index;

	pane(f, IDENTITY_X, TOP, IDENTITY_W, PANE_H);
	y = section_label(f, left, TOP + PANE_PAD, "IDENTITY");
	for (index = 0; index < 5; index++)
		snprintf(values[index], sizeof(values[index]), DASH);
	if (record)
	{
		if (record->shooting > 0 && record->movement > 0)
			snprintf(values[0], sizeof(values[0]), "%s / %s", ae_seat_step_name(record->shooting),
				ae_seat_step_name(record->movement));
		/* (the tiers it has played at: one, or the range) */
		if (record->tier_low > 0 && record->tier_high > record->tier_low)
			snprintf(values[1], sizeof(values[1]), "T%d-T%d", record->tier_low, record->tier_high);
		else if (record->tier_low > 0)
			snprintf(values[1], sizeof(values[1]), "T%d", record->tier_low);
		if (record->style >= 0 && record->style < AE_STYLE_PRESETS)
			upper_copy(ae_bots_style_preset_name(record->style), values[2], sizeof(values[2]));
		if (record->style >= 0)
		{
			if (record->favourite > AE_STYLE_WEAPON_NONE)
				upper_copy(ae_bots_style_weapon_name(record->favourite), values[3], sizeof(values[3]));
			else
				snprintf(values[3], sizeof(values[3]), "ANY");
		}
		if (record->weapon_count > 0 && record->weapons[0].name[0])
			snprintf(values[4], sizeof(values[4]), "%s", record->weapons[0].name);
	}
	for (index = 0; index < 5; index++)
		cell(f, left, right, y + 17.0f + index * (float)CELL_PITCH, labels[index], values[index]);
	/* (rc1: the BOT RANK slot a later version was to fill is gone: no placeholder in 1.0) */
}

/* VS YOU: a line each profile; the columns' right edges are the design's */
static void draw_versus(struct frame_metrics const *f, struct ae_bot_record const *record)
{
	static float const columns[6] = { 1046.0f, 1196.0f, 1346.0f, 1496.0f, 1646.0f, 1796.0f };
	static char const *const heads[6] = { "GAMES AGAINST", "YOU WON", "IT WON", "YOUR KILLS", "ITS KILLS", "WINS TOGETHER" };
	float left = LEFT + PANE_PAD, y, line = source.profiles <= 2 ? (float)VERSUS_LINE : (float)VERSUS_LINE_SHORT;
	int index, column;

	pane(f, LEFT, VERSUS_Y, VERSUS_W, VERSUS_H);
	y = section_label(f, left, VERSUS_Y + PANE_PAD, "VS YOU");
	/* (the column heads on the label's line: four short lines fit under it) */
	for (column = 0; column < 6; column++)
		put_tracked(f, AE_FONT_BODY, 13, 0.14f, columns[column], VERSUS_Y + PANE_PAD + 12.0f, AE_ALIGN_RIGHT, AE_COLOR_MUTED,
			heads[column]);
	if (source.profiles <= 0)
	{
		put(f, AE_FONT_BODY, 18, left, y + 17.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED,
			"No profile in this game: a guest keeps no record against a bot.");
		return;
	}
	for (index = 0; index < source.profiles && index < AE_BOT_PAGE_PROFILES; index++)
	{
		struct ae_bot_versus const *versus = record ?
			ae_bot_record_versus(ae_bot_records_active(), source.profile_key[index], record->key) : NULL;
		int met = versus && versus->against + versus->with > 0;
		float top = y + index * line, cy = top + (line == VERSUS_LINE ? 18.0f : 20.0f), x, w;
		char name[AE_BOT_RECORD_KEY * 2], head[64], sentence[160], fitted[160], values[6][24];

		/* (the profile's colour chip with its player's number) */
		rect(f, left, cy - 15.0f, 30.0f, 30.0f, 2.0f, source.profile_chip[index] ? source.profile_chip[index] : AE_COLOR_ACCENT);
		if (source.profile_player[index] > 0)
		{
			char number[4];

			snprintf(number, sizeof(number), "%d", source.profile_player[index]);
			put(f, AE_FONT_ROW, 18, left + 15.0f, cy, AE_ALIGN_CENTER, AE_COLOR_TITLE, number);
		}
		fit(f, AE_FONT_ROW, 22, source.profile_name[index], 300.0f, name, sizeof(name));
		x = left + 44.0f;
		w = put(f, AE_FONT_ROW, 22, x, cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, name);
		x += w + 12.0f;
		if (met && ae_bot_record_is_rival(versus))
		{
			draw_rival_tag(f, x, cy, 0);
			x += width_of(f, AE_FONT_ROW, 12, "RIVAL") + 17.0f + 16.0f;
		}
		if (x < VERSUS_HEAD_X)
			x = VERSUS_HEAD_X;
		head[0] = sentence[0] = 0;
		if (met)
			ae_bot_record_rival_line(versus, head, sizeof(head), sentence, sizeof(sentence), NULL, 0);
		if (met && head[0])
			put_tracked(f, AE_FONT_ROW, 22, 0.02f, x, cy, AE_ALIGN_LEFT, AE_COLOR_TITLE, head);
		else
			put(f, AE_FONT_BODY, 18, x, cy, AE_ALIGN_LEFT, AE_COLOR_MUTED, "Not met yet");
		if (met && line == VERSUS_LINE && sentence[0])
		{
			fit(f, AE_FONT_BODY, 16, sentence, 736.0f, fitted, sizeof(fitted));
			put(f, AE_FONT_BODY, 16, left + 44.0f, top + 46.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, fitted);
		}
		for (column = 0; column < 6; column++)
			snprintf(values[column], sizeof(values[column]), DASH);
		if (met)
		{
			snprintf(values[0], sizeof(values[0]), "%ld", versus->against);
			snprintf(values[1], sizeof(values[1]), "%ld", versus->you_won);
			snprintf(values[2], sizeof(values[2]), "%ld", versus->it_won);
			snprintf(values[3], sizeof(values[3]), "%ld", versus->your_kills);
			snprintf(values[4], sizeof(values[4]), "%ld", versus->its_kills);
			if (versus->with > 0)
				snprintf(values[5], sizeof(values[5]), "%ld OF %ld", versus->with_won, versus->with);
		}
		for (column = 0; column < 6; column++)
			put(f, AE_FONT_ROW, 22, columns[column], cy, AE_ALIGN_RIGHT, strcmp(values[column], DASH) ? AE_COLOR_TITLE :
				AE_COLOR_MUTED, values[column]);
	}
}

/* VS YOU's whole record: a row for each profile of the page with the numbers against and with the bot */
static int versus_table(struct ae_bot_record const *record, struct ae_rv_table *table)
{
	int index;
	static char const *const heads[6] = { "AGAINST", "YOU WON", "IT WON", "YOUR KILLS", "ITS KILLS", "WITH" };

	memset(table, 0, sizeof(*table));
	if (!record || source.profiles <= 0)
		return 0;
	snprintf(table->title, sizeof(table->title), "VS YOU");
	table->columns = 6;
	for (index = 0; index < 6; index++)
		snprintf(table->head[index], sizeof(table->head[0]), "%s", heads[index]);
	for (index = 0; index < source.profiles && index < AE_BOT_PAGE_PROFILES; index++)
	{
		struct ae_bot_versus const *versus = ae_bot_record_versus(ae_bot_records_active(), source.profile_key[index], record->key);
		char v[6][20];
		int at;

		for (at = 0; at < 6; at++)
			snprintf(v[at], sizeof(v[0]), "%s", DASH);
		if (versus && versus->against + versus->with > 0)
		{
			snprintf(v[0], sizeof(v[0]), "%ld", versus->against);
			snprintf(v[1], sizeof(v[0]), "%ld", versus->you_won);
			snprintf(v[2], sizeof(v[0]), "%ld", versus->it_won);
			snprintf(v[3], sizeof(v[0]), "%ld", versus->your_kills);
			snprintf(v[4], sizeof(v[0]), "%ld", versus->its_kills);
			if (versus->with > 0)
				snprintf(v[5], sizeof(v[0]), "%ld OF %ld", versus->with_won, versus->with);
		}
		ae_rv_table_row(table, source.profile_name[index], v[0], v[1], v[2], v[3], v[4], v[5], NULL);
	}
	return 1;
}

static void sub_text(struct ae_bot_record const *record, char *out, size_t size)
{
	if (!record)
		snprintf(out, size, "NOT MET IN A TOURNAMENT GAME ON THIS PC YET");
	else
		snprintf(out, size, "MET IN %ld TOURNAMENT GAME%s ON THIS PC", record->games, record->games == 1 ? "" : "S");
}

static unsigned long boxes_with_details(struct ae_rv_in const *view, struct ae_bot_record const *record)
{
	unsigned long has = 0;
	int box;

	for (box = 0; box < AE_RF_BOXES; box++)
		if (box != AE_RF_LAST && box != AE_RF_VERSUS && box != AE_RF_HIGHLIGHTS && box != AE_RF_RIVALS && ae_rv_has_details(box, view, NULL))
			has |= 1ul << box;
	if (record && source.profiles > 0)
		has |= 1ul << AE_RF_VERSUS;
	return has;
}

static int step_bot(int delta);

/* the detail window's source: the bot shown here, LT / RT through this page's list */
static int window_fill(struct ae_rw_view *view)
{
	struct ae_bot_record const *record = current_record();

	view->bot = 1;
	snprintf(view->name, sizeof(view->name), "%s", record ? record->name : source.name[page.index]);
	sub_text(record, view->sub, sizeof(view->sub));
	bot_view(record, &view->in, view->weapons);
	if (source.count > 1)
		snprintf(view->position, sizeof(view->position), "%d OF %d", page.index + 1, source.count);
	return record != NULL;
}

static int window_can_step(void)
{
	return source.count > 1;
}

static void window_medals(int category)
{
	open_medals(current_record(), category);
}

static struct ae_rw_source const window_source =
{ window_fill, step_bot, window_can_step, window_medals };

/* A on a box: its page of the detail window (the MEDALS box opens the MEDALS page, VS YOU its table) */
static void open_details(int box)
{
	static struct ae_rv_table table;
	struct ae_bot_record const *record = current_record();
	char sub[96];
	int column, window_page;

	if (box == AE_RF_NONE || !(page.has & (1ul << box)) || !record)
		return;
	if (box == AE_RF_MEDALS)
	{
		open_medals(record, AE_RM_ALL);
		return;
	}
	if (box == AE_RF_VERSUS)
	{
		if (!versus_table(record, &table))
			return;
		sub_text(record, sub, sizeof(sub));
		ae_sound_request(AE_SOUND_FORWARD, 0);
		ae_screen_record_details_open(AE_OWNER_PARENT, &table, record->name, 1, sub);
		return;
	}
	window_page = ae_rc_page_of_box(box, &column);
	if (window_page < 0)
		return;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	if (window_page == AE_RC_MODES)
		ae_screen_record_window_open_kind(AE_OWNER_PARENT, &window_source, -1);
	else
		ae_screen_record_window_open(AE_OWNER_PARENT, &window_source, window_page, column);
}

/* LT / RT (Page Up / Page Down): the next bot of the list, the page and the focused box kept */
static int step_bot(int delta)
{
	if (source.count <= 1)
		return 0;
	page.index = (page.index + (delta > 0 ? 1 : source.count - 1)) % source.count;
	ae_sound_request(AE_SOUND_CURSOR, 0);
	announce();
	return 1;
}

static int page_handle(struct ae_screen *screen, struct ae_event const *event)
{
	(void)screen;
	page.mouse = 0;
	page.hover = -1;
	page.chip_hover = -1;
	switch (event->action)
	{
	case AE_ACTION_Y:
	case AE_ACTION_TAB_NEXT:
		go_to_page(ae_rp_next(page.page));
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
		go_to_page((page.page + AE_RP_PAGES - 1) % AE_RP_PAGES);
		return 1;
	case AE_ACTION_PAGE_DOWN:
		step_bot(1);
		return 1;
	case AE_ACTION_PAGE_UP:
		step_bot(-1);
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
	{
		int next = ae_rf_move(page.page, page.has, page.focus, event->action == AE_ACTION_UP ? AE_RF_UP :
			event->action == AE_ACTION_DOWN ? AE_RF_DOWN : event->action == AE_ACTION_LEFT ? AE_RF_LEFT : AE_RF_RIGHT);

		if (next != page.focus)
		{
			page.focus = next;
			ae_sound_request(AE_SOUND_CURSOR, 0);
			platform_log("ae botrecord: focus %s", ae_rf_name(next));
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

static void page_draw(struct ae_screen *screen)
{
	struct ae_frame frame;
	struct ae_density density;
	struct frame_metrics f;
	struct ae_bot_record const *record = current_record();
	struct ae_rs_weapon weapons[AE_RS_WEAPONS];
	struct ae_rv_in view;

	(void)screen;
	ae_record_frame_begin(&f, &frame, &density);
	bot_view(record, &view, weapons);
	page.has = boxes_with_details(&view, record);
	page.focus = ae_rf_keep(page.page, page.has, page.focus);
	draw_head(&f, record);
	ae_rp_chips(&f, page.page, page.mouse, page.chip_hover, BOT_CHIP_HIT);
	if (page.page == AE_RP_OVERVIEW)
	{
		draw_game(&f, record);
		ae_rp_totals(&f, &view);
		draw_identity(&f, record);
		draw_versus(&f, record);
	}
	else if (page.page == AE_RP_COMBAT)
		ae_rp_combat(&f, &view);
	else
		ae_rp_modes(&f, &view);
	ae_rp_boxes(&f, page.page, page.has, page.focus, page.mouse, page.hover, BOT_PANE_HIT);
	/* the foot: A the focused box's details, B back, LB / RB the page, LT / RT the next bot (two or more) */
	page.prompt_count = 0;
	memset(page.prompts, 0, sizeof(page.prompts));
	if (page.focus != AE_RF_NONE)
	{
		page.prompts[page.prompt_count].button = AE_BUTTON_A;
		page.prompts[page.prompt_count].label = page.focus == AE_RF_MEDALS ? "All medals" : "Details";
		page.prompts[page.prompt_count++].action = AE_ACTION_ACCEPT;
	}
	page.prompts[page.prompt_count].button = AE_BUTTON_B;
	page.prompts[page.prompt_count].label = "Back";
	page.prompts[page.prompt_count++].action = AE_ACTION_BACK;
	page.prompts[page.prompt_count].button = AE_BUTTON_LEFT_SHOULDER;
	page.prompts[page.prompt_count].label = "Page";
	page.prompts[page.prompt_count].action = AE_ACTION_TAB_PREVIOUS;
	page.prompts[page.prompt_count++].pair = 1;
	if (source.count > 1)
	{
		page.prompts[page.prompt_count].button = AE_BUTTON_LEFT_TRIGGER;
		page.prompts[page.prompt_count].label = "Next bot";
		page.prompts[page.prompt_count].action = AE_ACTION_PAGE_UP;
		page.prompts[page.prompt_count++].pair = 1;
	}
	ae_widget_footer_band(&density, frame.rect.y + 1080.0f * f.m);
	ae_widget_prompts(&density, frame.rect.x + LEFT * f.m, frame.rect.y + 1020.0f * f.m, page.prompts, page.prompt_count,
		ae_bot_records_demo_on() ? "MADE-UP RECORDS (DEV)" : "THIS PC'S VIEW", frame.rect.x + (LEFT + 1728.0f) * f.m, -1,
		BOT_PAGE_HIT);
}

static struct ae_screen_class const page_class =
{
	.name = "bot_record", .leave = page_leave, .handle = page_handle, .draw = page_draw, .pointer = page_pointer,
};
