/* CAREER's RECORDS: players beside bots (see the header). Measures are design units of the 1920 x 1080 record frame. */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "math/real_math.h"
#include "../src/ae_draw.h"
#include "../src/ae_layout.h"
#include "ae_bot_record_rules.h"
#include "ae_glue_bot_records.h"
#include "ae_glue_service_record.h"
#include "ae_glue_social.h"
#include "ae_glue_social_game.h"
#include "ae_pgcr_draw.h"
#include "ae_record_columns_rules.h"
#include "ae_record_step_rules.h"
#include "ae_screen_bot_record.h"
#include "ae_screen_main_menu.h"
#include "ae_screen_record.h"
#include "ae_screen_record_window.h"
#include "ae_screen_records.h"
#include "ae_sound.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	RECORDS_HIT = 0x7634,
	RECORDS_ROW_HIT = 0x7635,
	LEFT = 96, RIGHT = 1824,
	LIST_W = 560, GAP = 24,
	LABEL_CY = 176, ROWS_TOP = 200, ROW_H = 44, ROW_PITCH = 50, SHOWN = 10,
	CARD_Y = 168, CARD_H = 572,
	/* the lists */
	PLAYERS = 0, BOTS = 1, LISTS = 2
};

static struct
{
	int on_stack;
	short player;
	char key[AE_SR_KEY];
	char viewer[48];
	int have_key;
	/* each list: its order (indices into its store), its count, the focused one and the first shown */
	int players[AE_SR_PLAYERS], player_count;
	int bots[AE_BOT_RECORD_BOTS], bot_count;
	int index[LISTS], first[LISTS];
	int list, sort;
	int mouse, hover_list, hover_row;
	struct ae_prompt prompts[5];
	short prompt_count;
} records;

static struct ae_screen_class const records_class;

/* ---------- the lists */

static void lists_build(void)
{
	struct ae_service_record const *store = ae_record_store();
	struct ae_bot_records const *bots = ae_bot_records_active();
	char bot_key[AE_BOT_RECORD_KEY];
	int list;

	records.have_key = ae_social_screen_key(records.player, records.key, sizeof(records.key)) && records.key[0];
	if (!records.have_key)
		records.key[0] = 0;
	if (!ae_social_profile_name(records.player, records.viewer, sizeof(records.viewer)))
		records.viewer[0] = 0;
	records.player_count = store ? ae_rstep_order(store, records.key, records.players) : 0;
	snprintf(bot_key, sizeof(bot_key), "%s", records.key);
	records.bot_count = records.have_key && bots ? ae_bot_records_list(bots, bot_key, records.sort, records.bots,
		AE_BOT_RECORD_BOTS) : 0;
	for (list = 0; list < LISTS; list++)
	{
		int count = list == PLAYERS ? records.player_count : records.bot_count;

		if (records.index[list] >= count)
			records.index[list] = count - 1;
		if (records.index[list] < 0)
			records.index[list] = 0;
		if (records.index[list] < records.first[list])
			records.first[list] = records.index[list];
		if (records.index[list] >= records.first[list] + SHOWN)
			records.first[list] = records.index[list] - SHOWN + 1;
	}
	if (records.list == PLAYERS && !records.player_count && records.bot_count)
		records.list = BOTS;
}

static int list_count(int list)
{
	return list == PLAYERS ? records.player_count : records.bot_count;
}

static struct ae_bot_record const *bot_at(int index)
{
	struct ae_bot_records const *bots = ae_bot_records_active();

	return index >= 0 && index < records.bot_count ? &bots->bot[records.bots[index]] : NULL;
}

static struct ae_sr_player const *player_at(int index)
{
	struct ae_service_record const *store = ae_record_store();

	return index >= 0 && index < records.player_count ? &store->player[records.players[index]] : NULL;
}

/* a bot's record as the view's input; weapons copied to the view's own type */
static void bot_in(struct ae_bot_record const *bot, struct ae_rv_in *in, struct ae_rs_weapon *weapons)
{
	static struct ae_rs_stats const none;
	static unsigned long const no_medals[AE_MEDAL_COUNT];
	int index;

	memset(in, 0, sizeof(*in));
	in->x = &none;
	in->medals = no_medals;
	in->spree_from_record = 1;
	if (!bot)
		return;
	in->games = bot->games; in->wins = bot->wins; in->ties = bot->ties; in->kills = bot->kills; in->deaths = bot->deaths;
	in->assists = bot->assists; in->best_spree = bot->best_spree; in->x = &bot->x; in->medals = bot->medals;
	for (index = 0; index < AE_RS_KINDS && index < AE_BOT_RECORD_KINDS; index++)
	{
		in->kind[index].games = bot->kind[index].games;
		in->kind[index].wins = bot->kind[index].wins;
		in->kind[index].kills = bot->kind[index].kills;
		in->kind[index].deaths = bot->kind[index].deaths;
		in->kind[index].assists = bot->kind[index].assists;
	}
	in->maps = bot->map;
	in->map_count = bot->map_count;
	for (index = 0; index < bot->weapon_count && index < AE_RS_WEAPONS; index++)
	{
		snprintf(weapons[index].name, sizeof(weapons[index].name), "%s", bot->weapons[index].name);
		weapons[index].kills = bot->weapons[index].kills;
	}
	in->weapons = weapons;
	in->weapon_count = bot->weapon_count < AE_RS_WEAPONS ? bot->weapon_count : AE_RS_WEAPONS;
}

/* ---------- drawing */

static void draw_list(struct frame_metrics const *f, int list, float x)
{
	struct ae_service_record const *store = ae_record_store();
	int count = list_count(list), at, row;
	char text[64], name[64];

	put_tracked(f, AE_FONT_BODY, 15, 0.14f, x, LABEL_CY - 30.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED,
		list == PLAYERS ? "PLAYERS ON THIS PC" : "BOTS YOU HAVE MET");
	put_tracked(f, AE_FONT_BODY, 15, 0.14f, x + LIST_W - 40.0f, LABEL_CY - 30.0f, AE_ALIGN_RIGHT, AE_COLOR_MUTED,
		list == PLAYERS ? "GAMES SEEN" : "WITH YOU");
	for (row = 0, at = records.first[list]; row < SHOWN && at < count; row++, at++)
	{
		float top = ROWS_TOP - 30.0f + (float)row * ROW_PITCH, cy = top + ROW_H * 0.5f, w;
		int focused = !records.mouse && records.list == list && records.index[list] == at;
		int hovered = records.mouse && records.hover_list == list && records.hover_row == at;
		unsigned int ink = focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TITLE;
		long games;

		rect(f, x, top, LIST_W, ROW_H, 3.0f, focused ? AE_COLOR_SELECTION : AE_COLOR_ROW);
		if (hovered)
			rect(f, x, top, LIST_W, ROW_H, 3.0f, AE_COLOR_HOVER);
		if (list == PLAYERS)
		{
			struct ae_sr_player const *player = player_at(at);

			fit(f, AE_FONT_ROW, 20, player->name, LIST_W - 220.0f, name, sizeof(name));
			w = put(f, AE_FONT_ROW, 20, x + 16.0f, cy, AE_ALIGN_LEFT, ink, name);
			if (records.have_key && !strcmp(player->key, records.key))
				draw_you_tag(f, x + 16.0f + w + 12.0f, cy, focused);
			else if (records.have_key && ae_sr_is_rival(store, records.key, player->key))
				draw_rival_tag(f, x + 16.0f + w + 12.0f, cy, focused);
			games = player->total.games;
		}
		else
		{
			struct ae_bot_record const *bot = bot_at(at);
			struct ae_bot_versus const *versus = records.have_key ? ae_bot_record_versus(ae_bot_records_active(), records.key,
				bot->key) : NULL;

			fit(f, AE_FONT_ROW, 20, bot->name, LIST_W - 220.0f, name, sizeof(name));
			put(f, AE_FONT_ROW, 20, x + 16.0f, cy, AE_ALIGN_LEFT, ink, name);
			games = versus ? versus->against + versus->with : 0;
		}
		ae_rs_number_text(games, text, sizeof(text));
		put(f, AE_FONT_ROW, 20, x + LIST_W - 40.0f, cy, AE_ALIGN_RIGHT, ink, text);
		/* (the chevron: the row opens) */
		put(f, AE_FONT_BODY, 20, x + LIST_W - 16.0f, cy, AE_ALIGN_RIGHT, focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED, ">");
		ae_hit_add(tx(f, x), ty(f, top), LIST_W * f->m, ROW_H * f->m, RECORDS_ROW_HIT, AE_PART_CARD,
			(short)(list * 1000 + at));
	}
	if (count > records.first[list] + SHOWN || records.first[list] > 0)
	{
		int more = count - SHOWN;

		snprintf(text, sizeof(text), "%d MORE", more > 0 ? more : 0);
		put_tracked(f, AE_FONT_BODY, 14, 0.14f, x + LIST_W * 0.5f, ROWS_TOP - 30.0f + (float)SHOWN * ROW_PITCH + 14.0f,
			AE_ALIGN_CENTER, AE_COLOR_MUTED, text);
	}
	if (!count)
		put(f, AE_FONT_BODY, 16, x, ROWS_TOP - 8.0f, AE_ALIGN_LEFT, AE_COLOR_MUTED, list == PLAYERS ?
			"No games kept on this PC yet." : records.have_key ? "No bots met yet: play an AE TOURNAMENT game." :
			"Sign in to a profile to keep a record of the bots you meet.");
}

static void draw_card(struct frame_metrics const *f, float x)
{
	static struct ae_rc_column card;
	struct ae_rv_in in;
	struct ae_rs_weapon weapons[AE_RS_WEAPONS];
	struct ae_sr_last const *last = NULL;
	char const *name;
	int own = 0, rival = 0, bot = records.list == BOTS;
	float w;

	if (bot)
	{
		struct ae_bot_record const *record = bot_at(records.index[BOTS]);

		if (!record)
			return;
		bot_in(record, &in, weapons);
		name = record->name;
	}
	else
	{
		struct ae_sr_player const *player = player_at(records.index[PLAYERS]);

		if (!player)
			return;
		ae_rv_from_player(player, &in);
		last = &player->last;
		name = player->name;
		own = records.have_key && !strcmp(player->key, records.key);
		rival = !own && records.have_key && ae_sr_is_rival(ae_record_store(), records.key, player->key);
	}
	ae_rc_card(&in, last, &card);
	{
		char fitted[64];

		fit(f, AE_FONT_ROW, 26, name, LIST_W - 160.0f, fitted, sizeof(fitted));
		snprintf(card.name, sizeof(card.name), "%s", fitted);
	}
	ae_rw_draw_column(f, x, CARD_Y, LIST_W, CARD_H, &card);
	w = width_of(f, AE_FONT_ROW, 26, card.name);
	if (bot)
		draw_outlined_tag(f, x + 28.0f + w + 14.0f, CARD_Y + 44.0f, "BOT", AE_COLOR_MUTED);
	else if (own)
		draw_you_tag(f, x + 28.0f + w + 14.0f, CARD_Y + 44.0f, 0);
	else if (rival)
		draw_rival_tag(f, x + 28.0f + w + 14.0f, CARD_Y + 44.0f, 0);
}

static void records_draw(struct ae_screen *screen)
{
	struct ae_frame frame;
	struct ae_density density;
	struct frame_metrics f;
	char text[96];
	float w;

	(void)screen;
	ae_record_frame_begin(&f, &frame, &density);
	lists_build();
	put(&f, AE_FONT_TITLE, 64, LEFT, 92.0f, AE_ALIGN_LEFT, AE_COLOR_TITLE, "RECORDS");
	/* the profile: its name and its player */
	snprintf(text, sizeof(text), "PLAYER %d", (int)records.player + 1);
	w = put_tracked(&f, AE_FONT_BODY, 16, 0.14f, RIGHT, 92.0f, AE_ALIGN_RIGHT, AE_COLOR_MUTED, text);
	if (records.viewer[0])
	{
		rect(&f, RIGHT - w - 16.0f, 90.0f, 4.0f, 4.0f, 2.0f, AE_COLOR_MUTED);
		put(&f, AE_FONT_ROW, 22, RIGHT - w - 32.0f, 92.0f, AE_ALIGN_RIGHT, AE_COLOR_TITLE, records.viewer);
	}
	draw_list(&f, PLAYERS, LEFT);
	draw_list(&f, BOTS, LEFT + LIST_W + GAP);
	draw_card(&f, LEFT + 2.0f * (LIST_W + GAP));
	put(&f, AE_FONT_BODY, 17, LEFT, 800.0f, AE_ALIGN_LEFT, AE_COLOR_TEXT,
		"One record for everyone this PC has played with or against. A opens it; inside, LT / RT steps through this list.");
	records.prompt_count = 0;
	memset(records.prompts, 0, sizeof(records.prompts));
	if (list_count(records.list) > 0)
	{
		records.prompts[records.prompt_count].button = AE_BUTTON_A;
		records.prompts[records.prompt_count].label = "Record";
		records.prompts[records.prompt_count++].action = AE_ACTION_ACCEPT;
	}
	records.prompts[records.prompt_count].button = AE_BUTTON_B;
	records.prompts[records.prompt_count].label = "Back";
	records.prompts[records.prompt_count++].action = AE_ACTION_BACK;
	if (records.bot_count > 1)
	{
		records.prompts[records.prompt_count].button = AE_BUTTON_Y;
		records.prompts[records.prompt_count].label = "Sort";
		records.prompts[records.prompt_count++].action = AE_ACTION_Y;
	}
	records.prompts[records.prompt_count].button = AE_BUTTON_LEFT_SHOULDER;
	records.prompts[records.prompt_count].label = "Profile";
	records.prompts[records.prompt_count].action = AE_ACTION_TAB_PREVIOUS;
	records.prompts[records.prompt_count++].pair = 1;
	ae_widget_footer_band(&density, frame.rect.y + 1080.0f * f.m);
	ae_widget_prompts(&density, frame.rect.x + LEFT * f.m, frame.rect.y + 1020.0f * f.m, records.prompts, records.prompt_count,
		ae_bot_records_demo_on() ? "MADE-UP RECORDS (DEV)" : "THIS PC'S VIEW", frame.rect.x + RIGHT * f.m, -1, RECORDS_HIT);
}

/* ---------- input */

static void announce(void)
{
	platform_log("ae records: list (player %d, %d players, %d bots)", (int)records.player + 1, records.player_count,
		records.bot_count);
}

static void open_focused(void)
{
	if (records.list == PLAYERS)
	{
		struct ae_sr_player const *player = player_at(records.index[PLAYERS]);

		if (!player)
			return;
		platform_log("ae records: open %s", player->name);
		if (ae_screen_record_open(AE_OWNER_PARENT, player->name, records.viewer[0] ? records.viewer : ""))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	}
	{
		struct ae_bot_page_source *source = ae_screen_bot_record_source();
		struct ae_bot_records const *bots = ae_bot_records_active();
		int index;

		if (records.bot_count <= 0 || !source)
			return;
		source->from_career = 1;
		for (index = 0; index < records.bot_count && index < AE_BOT_RECORD_BOTS; index++)
			snprintf(source->name[index], sizeof(source->name[index]), "%s", bots->bot[records.bots[index]].name);
		source->count = index;
		if (records.have_key)
		{
			snprintf(source->profile_key[0], sizeof(source->profile_key[0]), "%s", records.key);
			if (!ae_social_profile_name(records.player, source->profile_name[0], sizeof(source->profile_name[0])))
				snprintf(source->profile_name[0], sizeof(source->profile_name[0]), "PLAYER %d", (int)records.player + 1);
			source->profile_chip[0] = AE_COLOR_ACCENT;
			source->profile_player[0] = (int)records.player + 1;
			source->profiles = 1;
		}
		platform_log("ae records: open %s", bot_at(records.index[BOTS]) ? bot_at(records.index[BOTS])->name : "?");
		if (ae_screen_bot_record_open(AE_OWNER_PARENT, records.index[BOTS], 0))
			ae_sound_request(AE_SOUND_FORWARD, 0);
	}
}

static void move(int list, int delta)
{
	int count = list_count(list), before = records.index[list];

	records.index[list] += delta;
	if (records.index[list] >= count)
		records.index[list] = count - 1;
	if (records.index[list] < 0)
		records.index[list] = 0;
	if (records.index[list] < records.first[list])
		records.first[list] = records.index[list];
	if (records.index[list] >= records.first[list] + SHOWN)
		records.first[list] = records.index[list] - SHOWN + 1;
	if (records.index[list] != before)
		ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void profile_step(int direction)
{
	short each, candidate = records.player;

	for (each = 0; each < AE_SOCIAL_LOCAL; each++)
	{
		candidate = (short)((candidate + direction + AE_SOCIAL_LOCAL) % AE_SOCIAL_LOCAL);
		if (candidate == 0 || ae_screen_main_menu_signed_in(candidate))
		{
			records.player = candidate;
			records.index[PLAYERS] = records.index[BOTS] = 0;
			records.first[PLAYERS] = records.first[BOTS] = 0;
			ae_sound_request(AE_SOUND_CURSOR, 0);
			lists_build();
			announce();
			return;
		}
	}
	ae_sound_request(AE_SOUND_FAILURE, 0);
}

static int records_handle(struct ae_screen *screen, struct ae_event const *event)
{
	(void)screen;
	records.mouse = 0;
	switch (event->action)
	{
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		move(records.list, event->action == AE_ACTION_UP ? -1 : 1);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
	{
		int next = event->action == AE_ACTION_LEFT ? PLAYERS : BOTS;

		if (next != records.list && list_count(next) > 0)
		{
			records.list = next;
			ae_sound_request(AE_SOUND_CURSOR, 0);
		}
		return 1;
	}
	case AE_ACTION_ACCEPT:
		open_focused();
		return 1;
	case AE_ACTION_Y:
		records.sort = (records.sort + 1) % AE_BOT_RECORD_SORTS;
		ae_sound_request(AE_SOUND_CURSOR, 0);
		lists_build();
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		profile_step(event->action == AE_ACTION_TAB_NEXT ? 1 : -1);
		return 1;
	case AE_ACTION_BACK:
		return 0;
	default:
		return 1;
	}
}

static void records_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	int found;

	(void)screen;
	if (!pointer->touch && (pointer->moved || pointer->left_clicks || pointer->wheel_steps))
		records.mouse = 1;
	if (ae_prompts_pointer(records.prompts, records.prompt_count, pointer, RECORDS_HIT, &event))
	{
		ae_ui_dispatch(&event);
		return;
	}
	found = ae_hit_at(pointer->x, pointer->y, 0, &hit);
	records.hover_list = found && hit.id == RECORDS_ROW_HIT ? hit.index / 1000 : -1;
	records.hover_row = found && hit.id == RECORDS_ROW_HIT ? hit.index % 1000 : -1;
	if (pointer->left_clicks && records.hover_list >= 0)
	{
		records.list = records.hover_list;
		records.index[records.list] = records.hover_row;
		open_focused();
	}
}

static void records_leave(struct ae_screen *screen)
{
	(void)screen;
	records.on_stack = 0;
	platform_log("ae records: closed");
}

static struct ae_screen_class const records_class =
{
	.name = "records", .leave = records_leave, .handle = records_handle, .draw = records_draw, .pointer = records_pointer,
};

int ae_screen_records_open(short local_player)
{
	if (records.on_stack)
		return 0;
	memset(&records, 0, sizeof(records));
	records.player = local_player < 0 || local_player >= AE_SOCIAL_LOCAL ? 0 : local_player;
	records.sort = AE_BOT_RECORD_SORT_GAMES;
	records.hover_list = records.hover_row = -1;
	lists_build();
	if (!ae_ui_push(&records_class, AE_OWNER_ANY, &records))
		return 0;
	records.on_stack = 1;
	announce();
	return 1;
}

void ae_screen_records_close(void)
{
	if (records.on_stack)
		ae_ui_remove(&records);
}
