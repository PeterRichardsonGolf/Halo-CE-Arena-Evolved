/*
AE_SCREEN_SERVER_LOBBY.C

A dedicated server's lobby page (ae_screen_server_lobby.h). The measures are the approved mockup's
(mockups-077-server/server-lobby/mk.py, option A) at 1920 x 1080: positions as fractions of the frame (ae_setup_flow.h:
a centred 16:9 box, the whole width when narrower), sizes u.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "../src/ae_draw.h"
#include "../src/ae_layout.h"
#include "ae_glue_lobby.h"
#include "ae_glue_service_record.h"
#include "ae_glue_setup_maps.h"
#include "ae_hooks.h"
#include "ae_input.h"
#include "ae_moderation.h"
#include "ae_screen_attention.h"
#include "ae_screen_browser.h"
#include "ae_screen_create_game.h"
#include "ae_screen_main_menu.h"
#include "ae_screen_server_lobby.h"
#include "ae_server_ingame.h"
#include "ae_server_lobby.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_test_hooks.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "network_map_vote.h"

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
boolean main_menu_is_active(void);
void ui_widgets_close_all(void);
boolean player_ui_local_player_wants_to_play_multiplayer(short local_player_index);

enum
{
	HIT_ROWS = 0x7E60, HIT_PROMPTS, HIT_PEOPLE, HIT_CARD,
	/* the left column's items: the vote's choices, CHANGE TEAM, RUN THE SERVER */
	ITEM_CHOICE = 0, ITEM_TEAM, ITEM_SERVER,
	ITEMS = NETWORK_MAP_VOTE_MAXIMUM_CHOICES + 2,
	PEOPLE = AE_LOBBY_MAXIMUM_PLAYERS,
	/* the upstream lobby a join left up is taken over once this machine's player is in, or after this long */
	TAKE_OVER_MS = 6000,
	/* a note in the help line's place (a refusal, the server's answer) this long */
	NOTE_MS = 4000,
};

/* the mockup's measures (u at 1920 x 1080) */
#define TEAM_RED 0xC0392BFFu
#define TEAM_BLUE 0x2E86DEFFu
#define TEAM_RED_HEAD 0x6E2421FFu
#define TEAM_BLUE_HEAD 0x2962A0FFu
#define TEAM_RED_INK 0xEC8074FFu       /* the red's light ink (AE_COLOR_DESTRUCTIVE_INK): a team word on a dark row */
#define TEAM_BLUE_INK 0x86BEF2FFu
#define ICE 0xB8D6FFFFu
#define OK_INK 0x78D28CFFu             /* the browser's good-ping green: the vote's winner */
#define DIM_INK 0x96A2ACFFu
#define CARD_FILL 0x10161CECu
#define PANEL_FILL 0x10161CEEu
#define PANEL_HEAD 0x0A0E12FFu
#define PLATE_FILL 0x161E26FFu
#define PLATE_INK 0x60707EFFu
#define TOKEN_FILL 0x0C1218C8u
#define TOKEN_EDGE 0xB8D6FF96u
#define TRACK 0xC8D6E02Eu
#define PIP_EMPTY 0xC8D6E03Cu
#define PIP_EMPTY_ON_BAR 0x0F171E46u
#define DIM_WASH 0x0A0E126Eu

#define COLUMN_SPLIT (1116.0f / 1920.0f)
#define PANEL_BOTTOM_Y (880.0f / 1080.0f)
#define HELP_RULE_Y (902.0f / 1080.0f)
#define HELP_BASE_Y (931.0f / 1080.0f)
#define CARD_WIDTH_U 882.0f
#define CARD_HEIGHT_U 242.0f
#define PICTURE_W_U 258.0f
#define PICTURE_H_U 210.0f
#define ROW_U 50.0f
#define PITCH_U 56.0f

struct person { short card; short team; };

static struct
{
	struct ae_flow_screen flow;
	int on_stack;
	char how[48];
	unsigned long opened_at, joined_at;
	/* what the page reads each frame */
	struct ae_lobby_roster roster;
	int have_roster;
	struct network_map_vote_status vote;
	int vote_heard;
	int state, logged_state;
	short seconds;               /* the countdown, -1 for none */
	/* the left column */
	short items[ITEMS], item_count, focus;
	/* the right column: its people (display order) and the focus there */
	struct person people[PEOPLE];
	short people_count, person_focus;
	int column;                  /* 0 left, 1 THIS SERVER */
	/* split screen: the controller CHANGE TEAM speaks for */
	short team_controller;
	/* a note over the help line */
	char note[160];
	unsigned long note_until;
	/* as drawn (layout units): the left items' rows, the people's rows */
	struct ae_rect item_rects[ITEMS], person_rects[PEOPLE];
	/* the player card open: who */
	short card_machine, card_controller;
	char card_name[AE_LOBBY_NAME_SIZE], card_title[64], card_body[200], card_move[32];
	short card_rows[5], card_count;
	/* the server page's: the entries UP NEXT offers */
	char const *next_values[AE_INGAME_WIRE_ENTRIES];
	char next_texts[AE_INGAME_WIRE_ENTRIES][64];
	short next_indexes[AE_INGAME_WIRE_ENTRIES], next_count;
	short server_rows[4], server_count;
	struct ae_text say;
	/* the captions read for the card and rows (a map's own caption takes a lookup) */
	char caption_map[AE_SL_MAP], caption[64];
	/* THIS SERVER's room under its teams, window pixels as last drawn (the chat's lines go there) */
	float chat_left, chat_top, chat_right, chat_bottom;
	unsigned long chat_drawn_at;
	int download_seen;
	int chat_logged;
	/* the server's refusal heard while the page was up (a kick or a ban: the blacklisted code) */
	short removed_code;
	short winner_round, vote_round;
	unsigned long winner_at;
	int vote_focus_pending;
	unsigned long removed_at;
	char server_name[48];
} sl;

static struct ae_screen_class const page_class;

/* ---------- small helpers */

static float u(float spec)
{
	return ae_flow_u(&sl.flow, spec);
}

static float fx(float fraction)
{
	return ae_frame_x(&sl.flow.frame, fraction);
}

static float fy(float fraction)
{
	return ae_frame_y(&sl.flow.frame, fraction);
}

/* text by its baseline (the mockup's anchor) */
static float text_at(int font, float size, float x, float baseline, int align, unsigned int rgba, char const *text)
{
	return ae_draw_text(font, size, x, baseline - ae_draw_cap_height(font, size), align, rgba, text);
}

static float tracked_at(int font, float size, float tracking, float x, float baseline, int align, unsigned int rgba,
	char const *text)
{
	return ae_draw_text_tracked(font, size, tracking, x, baseline - ae_draw_cap_height(font, size), align, rgba, text);
}

static void note_set(char const *text)
{
	snprintf(sl.note, sizeof(sl.note), "%s", text);
	sl.note_until = system_milliseconds() + NOTE_MS;
}

static void upper(char const *text, char *out, int size)
{
	int index;

	for (index = 0; text && text[index] && index < size - 1; index++)
		out[index] = text[index] >= 'a' && text[index] <= 'z' ? (char)(text[index] - 32) : text[index];
	if (size > 0)
		out[index] = 0;
}

/* a map's caption ("PRISONER PRO"), the last one kept */
static char const *map_caption(char const *map)
{
	if (!map || !map[0])
		return "";
	if (strcmp(sl.caption_map, map))
	{
		snprintf(sl.caption_map, sizeof(sl.caption_map), "%s", map);
		ae_server_ingame_map_caption(map, sl.caption, (int)sizeof(sl.caption));
	}
	return sl.caption;
}

/* the card's map in words for the help line ("Hektik": the caption with only its first letters capitals) */
static void map_words(char const *map, char *text, int size)
{
	int index, start = 1;

	snprintf(text, (size_t)size, "%s", map_caption(map));
	for (index = 0; text[index]; index++)
	{
		char c = text[index];

		if (!start && c >= 'A' && c <= 'Z')
			text[index] = (char)(c + 32);
		start = c == ' ' || c == '-';
	}
}

static int team_game(void)
{
	return sl.have_roster && sl.roster.team_game;
}

static int local_people(void)
{
	short index;
	int count = 0;

	for (index = 0; sl.have_roster && index < sl.roster.count; index++)
		count += sl.roster.players[index].local && !sl.roster.players[index].bot;
	return count;
}

static struct ae_lobby_player const *local_player_of(short controller)
{
	short index;

	for (index = 0; sl.have_roster && index < sl.roster.count; index++)
	{
		struct ae_lobby_player const *player = &sl.roster.players[index];

		if (player->local && !player->bot && player->controller == controller)
			return player;
	}
	return NULL;
}

/* the info's entry for the card: the focused vote choice while the vote runs, the winner after it, else the lobby's */
static int card_entry(char const **map, char const **gametype)
{
	struct ae_sl_wire const *info = ae_server_lobby_info();
	short choice = -1;

	if (sl.vote_heard)
	{
		if (sl.vote.winner != NONE)
			choice = sl.vote.winner;
		else if (sl.column == 0 && sl.focus >= 0 && sl.focus < sl.item_count && sl.items[sl.focus] < ITEM_TEAM)
			choice = sl.items[sl.focus];
		else if (sl.vote.chosen != NONE)
			choice = sl.vote.chosen;
		else
			choice = 0;
	}
	if (choice >= 0 && choice < sl.vote.count && sl.state != AE_SL_STATE_COUNTDOWN && sl.state != AE_SL_STATE_DOWNLOAD)
	{
		*map = sl.vote.map_names[choice];
		*gametype = sl.vote.gametypes[choice];
		return info ? ae_sl_entry_of(info, *map, *gametype) : -1;
	}
	if (info)
	{
		int now = ae_sl_entry_with(info, AE_SL_ENTRY_NOW, 0);

		if (now >= 0)
		{
			*map = info->entries[now].map;
			*gametype = info->entries[now].gametype;
			return now;
		}
	}
	*map = sl.have_roster ? sl.roster.map : "";
	*gametype = NULL;
	return -1;
}

/* ---------- reading the game */

static void read_state(void)
{
	short seconds = ae_lobby_seconds_to_start();
	unsigned long done, total;
	int downloading = ae_server_lobby_download(NULL, 0, &done, &total);

	sl.have_roster = ae_lobby_roster(&sl.roster).ok;
	sl.vote_heard = network_map_vote_status_get(&sl.vote) && sl.vote.count > 0;
	sl.seconds = seconds >= 0 ? seconds : -1;
	sl.state = ae_sl_state(downloading, sl.seconds, sl.vote_heard, sl.vote_heard ? sl.vote.winner : -1);
	/* (the vote's winner shown a moment (WON THE VOTE) before the count that follows it at once) */
	if (sl.vote_heard && sl.vote.winner != NONE)
	{
		if (sl.winner_round != sl.vote.round + 1)
		{
			sl.winner_round = (short)(sl.vote.round + 1);
			sl.winner_at = system_milliseconds();
		}
		if (sl.state == AE_SL_STATE_COUNTDOWN && system_milliseconds() - sl.winner_at < 3000)
			sl.state = AE_SL_STATE_RESULT;
	}
	/* (a vote begun: the focus on its choices, this PC's own if it voted already) */
	if (sl.vote_heard && sl.vote.winner == NONE && sl.vote_round != sl.vote.round + 1)
	{
		sl.vote_round = (short)(sl.vote.round + 1);
		sl.vote_focus_pending = 1;
	}
	if (sl.state != sl.logged_state)
	{
		static char const *const names[] = { "WAITING", "VOTE", "RESULT", "COUNTDOWN", "DOWNLOAD" };
		static char const *const kinds[] = { "server_lobby_waiting", "server_lobby_vote", "server_lobby_result",
			"server_lobby_countdown", "server_lobby_download" };

		sl.logged_state = sl.state;
		platform_log("ae server lobby: state %s", names[sl.state]);
		ae_test_capture(kinds[sl.state], sl.state == AE_SL_STATE_VOTE || sl.state == AE_SL_STATE_WAITING ? 2500 : 900);
	}
}

/* the left column's items, the focus kept on the same kind */
static void items_build(void)
{
	short kept = sl.focus >= 0 && sl.focus < sl.item_count ? sl.items[sl.focus] : ITEM_TEAM, index;

	sl.item_count = 0;
	if (sl.vote_heard && sl.vote.winner == NONE)
	{
		for (index = 0; index < sl.vote.count && index < NETWORK_MAP_VOTE_MAXIMUM_CHOICES; index++)
			sl.items[sl.item_count++] = (short)(ITEM_CHOICE + index);
	}
	sl.items[sl.item_count++] = (short)(NETWORK_MAP_VOTE_MAXIMUM_CHOICES + 0);
	if (ae_server_ingame_role() > AE_INGAME_ROLE_NONE)
		sl.items[sl.item_count++] = (short)(NETWORK_MAP_VOTE_MAXIMUM_CHOICES + 1);
	if (sl.vote_focus_pending && sl.item_count > 1)
	{
		sl.vote_focus_pending = 0;
		sl.column = 0;
		kept = (short)(sl.vote.chosen != NONE ? sl.vote.chosen : 0);
	}
	/* (the kept item's place again; a choice gone: CHANGE TEAM) */
	sl.focus = -1;
	for (index = 0; index < sl.item_count; index++)
		if (sl.items[index] == kept)
			sl.focus = index;
	if (sl.focus < 0)
	{
		for (index = 0; index < sl.item_count; index++)
			if (sl.items[index] == NETWORK_MAP_VOTE_MAXIMUM_CHOICES)
				sl.focus = index;
	}
}

static int item_kind(short index)
{
	if (index < 0 || index >= sl.item_count)
		return -1;
	if (sl.items[index] < NETWORK_MAP_VOTE_MAXIMUM_CHOICES)
		return ITEM_CHOICE;
	return sl.items[index] == NETWORK_MAP_VOTE_MAXIMUM_CHOICES ? ITEM_TEAM : ITEM_SERVER;
}

/* the people (no bots) by team, red then blue; a free for all's in the roster's order */
static void people_build(void)
{
	short team, index, kept_machine = -1, kept_controller = -1;

	if (sl.person_focus >= 0 && sl.person_focus < sl.people_count)
	{
		struct ae_lobby_player const *player = &sl.roster.players[sl.people[sl.person_focus].card];

		kept_machine = player->machine;
		kept_controller = player->controller;
	}
	sl.people_count = 0;
	for (team = 0; team < (team_game() ? 2 : 1); team++)
	{
		for (index = 0; sl.have_roster && index < sl.roster.count && sl.people_count < PEOPLE; index++)
		{
			struct ae_lobby_player const *player = &sl.roster.players[index];

			if (player->bot || (team_game() && player->team != team))
				continue;
			sl.people[sl.people_count].card = index;
			sl.people[sl.people_count].team = team_game() ? team : -1;
			sl.people_count++;
		}
	}
	sl.person_focus = sl.people_count ? 0 : -1;
	for (index = 0; index < sl.people_count; index++)
	{
		struct ae_lobby_player const *player = &sl.roster.players[sl.people[index].card];

		if (player->machine == kept_machine && player->controller == kept_controller)
			sl.person_focus = index;
	}
}

static void team_bots(int *red, int *blue, int *all)
{
	struct ae_sl_wire const *info = ae_server_lobby_info();
	short index;

	*red = *blue = *all = 0;
	if (info)
	{
		*red = info->bots_red;
		*blue = info->bots_blue;
		*all = info->bots;
		return;
	}
	for (index = 0; sl.have_roster && index < sl.roster.count; index++)
	{
		struct ae_lobby_player const *player = &sl.roster.players[index];

		if (!player->bot)
			continue;
		(*all)++;
		*red += player->team == 0;
		*blue += player->team == 1;
	}
}

/* ---------- drawing */

/* a map's picture covering box (clipped), or its name plate: the map's name in the title face on a dark plate */
static void draw_picture(char const *map, float x, float y, float width, float height, int plate_text)
{
	int w = 0, h = 0, image = map && map[0] ? ae_setup_map_picture_named(map, &w, &h) : 0;

	if (image && w > 0 && h > 0)
	{
		float scale = width / (float)w > height / (float)h ? width / (float)w : height / (float)h;
		float dw = (float)w * scale, dh = (float)h * scale;

		ae_draw_clip_push(x, y, width, height);
		ae_draw_image(image, x + (width - dw) * 0.5f, y + (height - dh) * 0.5f, dw, dh, 0xFFFFFFFFu);
		ae_draw_clip_pop();
		return;
	}
	ae_draw_rect(x, y, width, height, 0.0f, PLATE_FILL);
	if (plate_text)
	{
		char const *caption = map_caption(map);
		float size = width / 9.0f;
		char fitted[80];

		if (size > u(40.0f))
			size = u(40.0f);
		if (size < u(18.0f))
			size = u(18.0f);
		if (!ae_fit_text(AE_FONT_TITLE, size, caption, width - u(16.0f), fitted, (int)sizeof(fitted)))
			text_at(AE_FONT_TITLE, size, x + width * 0.5f, y + height * 0.5f + ae_draw_cap_height(AE_FONT_TITLE, size) * 0.5f,
				AE_ALIGN_CENTER, PLATE_INK, fitted);
	}
}

/* the title: the server's name, its digits and '#' in Overpass at the title's capital height (the title face draws a 1
as an I), on the title's baseline */
static void draw_title(char const *name)
{
	char title[64], run[64];
	struct ae_sl_run runs[16];
	float size = u(60.0f), top = fy(AE_FLOW_TITLE_Y), cap = ae_draw_cap_height(AE_FONT_TITLE, size);
	float numeral_size = size * cap / ae_draw_cap_height(AE_FONT_ROW, size), x = ae_flow_left_x(&sl.flow);
	int count, index;

	upper(name, title, (int)sizeof(title));
	count = ae_sl_title_runs(title, runs, 16);
	for (index = 0; index < count; index++)
	{
		snprintf(run, sizeof(run), "%.*s", runs[index].length, title + runs[index].start);
		if (runs[index].numerals)
			x += ae_draw_text(AE_FONT_ROW, numeral_size, x, top, AE_ALIGN_LEFT, AE_COLOR_TITLE, run);
		else
			x += ae_draw_text(AE_FONT_TITLE, size, x, top, AE_ALIGN_LEFT, AE_COLOR_TITLE, run);
	}
}

/* a square-cornered tag (the browser's words: SERVER, AE), its right at right, centred on center_y; its width */
static float draw_tag(float right, float center_y, char const *label, unsigned int ink, float size)
{
	float width = ae_draw_text_tracked_width(AE_FONT_ROW, size, 0.1f, label) + u(14.0f), height = u(22.0f);

	ae_draw_outline(right - width, center_y - height * 0.5f, width, height, 0.0f, u(2.0f), ink);
	ae_draw_text_tracked(AE_FONT_ROW, size, 0.1f, right - width + u(7.0f), center_y - ae_draw_cap_height(AE_FONT_ROW, size) *
		0.5f, AE_ALIGN_LEFT, ink, label);
	return width;
}

static void draw_header(void)
{
	struct ae_sl_wire const *info = ae_server_lobby_info();
	struct network_game *game = network_game_get_game();
	char name[48];
	float right = ae_flow_right_x(&sl.flow), center = fy(92.0f / 1080.0f), x;
	int index;

	name[0] = 0;
	if (game)
	{
		for (index = 0; index < (int)sizeof(name) - 1 && index < (int)NUMBEROF(game->name) && game->name[index]; index++)
			name[index] = game->name[index] >= 0x20 && game->name[index] < 0x7F ? (char)game->name[index] : '?';
		name[index] = 0;
	}
	draw_title(name[0] ? name : "SERVER LOBBY");
	if (name[0])
		snprintf(sl.server_name, sizeof(sl.server_name), "%s", name);
	x = right;
	if (info && info->region[0])
	{
		tracked_at(AE_FONT_ROW, u(19.5f), 0.12f, x, fy(99.0f / 1080.0f), AE_ALIGN_RIGHT, AE_COLOR_TEXT, info->region);
		x -= ae_draw_text_tracked_width(AE_FONT_ROW, u(19.5f), 0.12f, info->region) + u(18.0f);
	}
	if (info)
		x -= draw_tag(x, center, ae_sl_join_word(info->join), ICE, u(15.0f)) + u(10.0f);
	draw_tag(x, center, "SERVER", AE_COLOR_TEXT, u(15.0f));
	ae_draw_rect(ae_flow_left_x(&sl.flow), fy(AE_FLOW_RULE_Y), right - ae_flow_left_x(&sl.flow), u(AE_FLOW_RULE_U), 0.0f,
		AE_COLOR_RULE);
}

static void draw_bar(float x, float y, float width, float fraction, unsigned int ink)
{
	if (fraction < 0.0f)
		fraction = 0.0f;
	if (fraction > 1.0f)
		fraction = 1.0f;
	ae_draw_rect(x, y, width, u(4.0f), 0.0f, TRACK);
	if (fraction > 0.0f)
		ae_draw_rect(x, y, width * fraction, u(4.0f), 0.0f, ink);
}

/* the game card: picture, map, gametype, its rules, then the state in big on its floor; its bottom */
static float draw_card(float x0, float x1, float y0)
{
	struct ae_sl_wire const *info = ae_server_lobby_info();
	char const *map = "", *gametype = NULL;
	int entry = card_entry(&map, &gametype);
	float scale = (x1 - x0) / u(CARD_WIDTH_U) < 1.0f ? (x1 - x0) / u(CARD_WIDTH_U) : 1.0f;
	float pw = u(PICTURE_W_U) * scale, ph = u(PICTURE_H_U) * scale, y1 = y0 + u(CARD_HEIGHT_U), tx, sb, right;
	char name[AE_SL_NAME + 8], line[160], lines[2][160];
	int count, index;

	/* (a soft shadow under it: the panel lifted off the backdrop) */
	for (index = 3; index >= 1; index--)
		ae_draw_rect(x0 - u(2.0f) * index, y0 + u(8.0f) - u(2.0f) * index, (x1 - x0) + u(4.0f) * index,
			(y1 - y0) + u(4.0f) * index, u(3.0f) * index, 0x00000014u);
	ae_draw_rect(x0, y0, x1 - x0, y1 - y0, 0.0f, CARD_FILL);
	draw_picture(map, x0 + u(16.0f), y0 + u(16.0f), pw, ph, 1);
	tx = x0 + u(16.0f) + pw + u(24.0f);
	right = x1 - u(20.0f);
	{
		char fitted[96];

		ae_fit_text(AE_FONT_ROW, u(34.0f), map_caption(map), right - tx, fitted, (int)sizeof(fitted));
		text_at(AE_FONT_ROW, u(34.0f), tx, y0 + u(52.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, fitted);
	}
	if (entry >= 0 && info->entries[entry].name[0])
		snprintf(name, sizeof(name), "%s", info->entries[entry].name);
	else if (gametype)
		ae_sl_gametype_word(gametype, name, (int)sizeof(name));
	else
		upper(sl.have_roster ? sl.roster.gametype : "", name, (int)sizeof(name));
	text_at(AE_FONT_ROW, u(22.0f), tx, y0 + u(86.0f), AE_ALIGN_LEFT, AE_COLOR_ACCENT, name);
	if (entry >= 0 && info->entries[entry].facts[0])
	{
		count = ae_wrap_text(AE_FONT_BODY, u(16.5f), info->entries[entry].facts, right - tx, lines, 2);
		for (index = 0; index < count; index++)
			text_at(AE_FONT_BODY, u(16.5f), tx, y0 + u(116.0f) + u(24.0f) * (float)index, AE_ALIGN_LEFT, AE_COLOR_TEXT,
				lines[index]);
	}
	/* (the state block on the card's floor) */
	sb = y1 - u(22.0f);
	switch (sl.state)
	{
	case AE_SL_STATE_VOTE:
	{
		int seconds = sl.vote.seconds_left > 0 ? sl.vote.seconds_left : 0;
		int total = info && info->vote_seconds ? info->vote_seconds : 20;
		float width = text_at(AE_FONT_ROW, u(26.0f), tx, sb - u(30.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, "VOTING");

		snprintf(line, sizeof(line), "%d s LEFT", seconds);
		text_at(AE_FONT_ROW, u(26.0f), tx + width + u(16.0f), sb - u(30.0f), AE_ALIGN_LEFT, AE_COLOR_ACCENT, line);
		draw_bar(tx, sb - u(10.0f), x1 - u(24.0f) - tx, (float)seconds / (float)(total > 0 ? total : 1), AE_COLOR_ACCENT);
		break;
	}
	case AE_SL_STATE_RESULT:
	{
		short winner = sl.vote.winner, index2;
		int total = 0;

		for (index2 = 0; index2 < sl.vote.count; index2++)
			total += sl.vote.votes[index2];
		text_at(AE_FONT_ROW, u(26.0f), tx, sb - u(30.0f), AE_ALIGN_LEFT, OK_INK, "WON THE VOTE");
		ae_sl_won_line(winner >= 0 && winner < sl.vote.count ? sl.vote.votes[winner] : 0, total, line, (int)sizeof(line));
		text_at(AE_FONT_BODY, u(17.5f), tx, sb, AE_ALIGN_LEFT, AE_COLOR_TEXT, line);
		break;
	}
	case AE_SL_STATE_COUNTDOWN:
	{
		float width = text_at(AE_FONT_ROW, u(26.0f), tx, sb, AE_ALIGN_LEFT, AE_COLOR_TITLE, "STARTS IN");

		snprintf(line, sizeof(line), "%d", (int)sl.seconds);
		text_at(AE_FONT_ROW, u(64.0f), tx + width + u(18.0f), sb + u(6.0f), AE_ALIGN_LEFT, AE_COLOR_ACCENT, line);
		break;
	}
	case AE_SL_STATE_DOWNLOAD:
	{
		unsigned long done = 0, total = 0;
		long percent;

		ae_server_lobby_download(NULL, 0, &done, &total);
		percent = total ? (long)(done / (total / 100 + 1)) : 0;
		if (percent > 100)
			percent = 100;
		/* (a frame of it under way, for the tests) */
		if (percent >= 25 && !sl.download_seen)
		{
			sl.download_seen = 1;
			ae_test_capture("server_lobby_transfer", 0);
		}
		if (total)
			snprintf(line, sizeof(line), "GETTING THE MAP  %ld%%", percent);
		else
			snprintf(line, sizeof(line), "GETTING THE MAP");
		text_at(AE_FONT_ROW, u(26.0f), tx, sb - u(30.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, line);
		draw_bar(tx, sb - u(14.0f), x1 - u(24.0f) - tx, (float)percent / 100.0f, AE_COLOR_ACCENT);
		if (total)
			snprintf(line, sizeof(line), "%lu of %lu MB, from the server.", (done + 0x80000) >> 20, (total + 0x80000) >> 20);
		else
			snprintf(line, sizeof(line), "Asking the server for it...");
		text_at(AE_FONT_BODY, u(16.5f), tx, sb + u(8.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT, line);
		break;
	}
	default:
	{
		int people = sl.have_roster ? sl.roster.people : 0;

		text_at(AE_FONT_ROW, u(26.0f), tx, sb - u(30.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, "WAITING FOR PLAYERS");
		ae_sl_waiting_line(info ? info->min_players : 1, people, info ? info->bots : 0, line, (int)sizeof(line));
		text_at(AE_FONT_BODY, u(17.5f), tx, sb, AE_ALIGN_LEFT, AE_COLOR_TEXT, line);
		break;
	}
	}
	ae_hit_add(x0, y0, x1 - x0, y1 - y0, HIT_CARD, AE_PART_CARD, -1);
	return y1;
}

/* a section's label (muted, tracked), and its right word */
static void draw_section(float x0, float x1, float baseline, char const *label, char const *right)
{
	tracked_at(AE_FONT_BODY, u(17.5f), 0.12f, x0 + u(10.0f), baseline, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	if (right)
		tracked_at(AE_FONT_ROW, u(17.5f), 0.08f, x1, baseline, AE_ALIGN_RIGHT, AE_COLOR_MUTED, right);
}

/* a small square-cornered tag on a row: filled (YOUR VOTE) */
static void draw_row_tag(float x, float center_y, char const *label, unsigned int fill, unsigned int ink)
{
	float size = u(12.5f), width = ae_draw_text_width(AE_FONT_ROW, size, label) + u(14.0f), height = u(20.0f);

	ae_draw_rect(x, center_y - height * 0.5f, width, height, 0.0f, fill);
	ae_draw_text(AE_FONT_ROW, size, x + u(7.0f), center_y - ae_draw_cap_height(AE_FONT_ROW, size) * 0.5f, AE_ALIGN_LEFT,
		ink, label);
}

/* a game's row of the list (a vote's choice, or a game to come): thumbnail, map, gametype, tag, votes */
static void draw_game_row(float x0, float x1, float y, char const *map, char const *gametype, int focused, int dim,
	int winner, int votes, int pips, char const *tag, int item)
{
	struct ae_sl_wire const *info = ae_server_lobby_info();
	int entry = info ? ae_sl_entry_of(info, map, gametype) : -1;
	float height = u(ROW_U), base = y + u(33.0f), lx = x0 + u(76.0f), width;
	unsigned int ink = focused ? AE_COLOR_SELECTION_TEXT : dim ? DIM_INK : AE_COLOR_TITLE;
	unsigned int gt_ink = focused ? 0x283C50FFu : dim ? DIM_INK : AE_COLOR_ACCENT;
	char name[AE_SL_NAME + 8], fitted[96];

	if (focused)
	{
		ae_draw_rect(x0, y, x1 - x0, height, 0.0f, AE_COLOR_SELECTION);
		ae_draw_rect(x0, y, u(5.0f), height, 0.0f, AE_COLOR_ACCENT);
	}
	else
		ae_draw_rect(x0, y, x1 - x0, height, 0.0f, AE_COLOR_ROW);
	if (winner)
		ae_draw_outline(x0 - u(3.0f), y - u(3.0f), x1 - x0 + u(6.0f), height + u(6.0f), 0.0f, u(3.0f), OK_INK);
	draw_picture(map, x0 + u(11.0f), y + u(5.0f), u(50.0f), u(40.0f), 0);
	ae_fit_text(AE_FONT_ROW, u(22.0f), map_caption(map), (x1 - x0) * 0.45f, fitted, (int)sizeof(fitted));
	width = text_at(AE_FONT_ROW, u(22.0f), lx, base, AE_ALIGN_LEFT, ink, fitted);
	if (entry >= 0 && info->entries[entry].name[0])
		snprintf(name, sizeof(name), "%s", info->entries[entry].name);
	else
		ae_sl_gametype_word(gametype, name, (int)sizeof(name));
	width += u(16.0f);
	width += text_at(AE_FONT_ROW, u(17.0f), lx + width, base, AE_ALIGN_LEFT, gt_ink, name);
	if (tag)
		draw_row_tag(lx + width + u(16.0f), y + u(25.0f), tag, focused ? AE_COLOR_SELECTION_TEXT : ICE,
			focused ? 0xFFFFFFFFu : AE_COLOR_SELECTION_TEXT);
	if (votes >= 0)
	{
		char word[24];
		float right = x1 - u(20.0f), pip = u(10.0f), gap = u(4.0f), px;
		int index;

		ae_sl_votes_word(votes, word, (int)sizeof(word));
		right -= text_at(AE_FONT_ROW, u(17.0f), right, y + u(32.0f), AE_ALIGN_RIGHT, focused ? AE_COLOR_SELECTION_TEXT :
			ink, word) + u(16.0f);
		px = right - (float)pips * (pip + gap) + gap;
		for (index = 0; index < pips; index++)
			ae_draw_rect(px + (float)index * (pip + gap), y + u(25.0f) - pip * 0.5f, pip, pip, 0.0f, index < votes ?
				(focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT) : focused ? PIP_EMPTY_ON_BAR : PIP_EMPTY);
	}
	if (dim)
		ae_draw_rect(x0, y, x1 - x0, height, 0.0f, DIM_WASH);
	if (item >= 0)
	{
		sl.item_rects[item].x = x0;
		sl.item_rects[item].y = y;
		sl.item_rects[item].width = x1 - x0;
		sl.item_rects[item].height = height;
		ae_hit_add(x0, y, x1 - x0, height, HIT_ROWS, AE_PART_ROW, (short)item);
	}
}

static void draw_left(float x0, float x1, float y)
{
	struct ae_sl_wire const *info = ae_server_lobby_info();
	short index;
	int split = local_people() > 1;

	memset(sl.item_rects, 0, sizeof(sl.item_rects));
	if (sl.vote_heard)
	{
		int most = 0, pips;

		for (index = 0; index < sl.vote.count; index++)
			if (sl.vote.votes[index] > most)
				most = sl.vote.votes[index];
		pips = ae_sl_pips(sl.have_roster ? sl.roster.people : 1, most);
		draw_section(x0, x1, y + u(2.0f), "VOTE FOR THE NEXT GAME", sl.vote.winner == NONE ? "MOST VOTES WINS" : "VOTE OVER");
		y += u(18.0f);
		for (index = 0; index < sl.vote.count && index < NETWORK_MAP_VOTE_MAXIMUM_CHOICES; index++)
		{
			short item = -1, at;
			int over = sl.vote.winner != NONE;

			for (at = 0; at < sl.item_count; at++)
				if (sl.items[at] == index)
					item = at;
			draw_game_row(x0, x1, y, sl.vote.map_names[index], sl.vote.gametypes[index],
				!over && sl.column == 0 && item >= 0 && item == sl.focus, over && index != sl.vote.winner,
				over && index == sl.vote.winner, sl.vote.votes[index], pips, index == sl.vote.chosen ? (split ? "THIS PC'S VOTE" :
				"YOUR VOTE") : NULL, item);
			y += u(PITCH_U);
		}
		y += u(30.0f);
	}
	else if (info && ae_sl_entry_with(info, AE_SL_ENTRY_NEXT, 0) >= 0)
	{
		draw_section(x0, x1, y + u(2.0f), "AFTER THAT", "FROM THE PLAYLIST");
		y += u(18.0f);
		for (index = 0; index < AE_SL_NEXT; index++)
		{
			int entry = ae_sl_entry_with(info, AE_SL_ENTRY_NEXT, index);

			if (entry < 0)
				break;
			draw_game_row(x0, x1, y, info->entries[entry].map, info->entries[entry].gametype, 0, 0, 0, -1, 0, NULL, -1);
			y += u(PITCH_U);
		}
		y += u(30.0f);
	}
	draw_section(x0, x1, y + u(2.0f), split ? "THIS PC" : "YOU", NULL);
	y += u(18.0f);
	for (index = 0; index < sl.item_count; index++)
	{
		struct ae_row widget;
		struct ae_row_layout layout;
		char value[48];
		int kind = item_kind(index), focused = sl.column == 0 && index == sl.focus;
		struct ae_lobby_player const *player;
		int rule = info ? info->team_rule : AE_SL_TEAMS_OPEN;

		if (kind == ITEM_CHOICE)
			continue;
		memset(&widget, 0, sizeof(widget));
		value[0] = 0;
		if (kind == ITEM_TEAM)
		{
			widget.label = "CHANGE TEAM";
			player = local_player_of(split ? sl.team_controller : (local_player_of(0) ? 0 : sl.team_controller));
			if (!player && sl.have_roster)
			{
				short at;

				for (at = 0; at < sl.roster.count && !player; at++)
					if (sl.roster.players[at].local && !sl.roster.players[at].bot)
						player = &sl.roster.players[at];
			}
			if (!team_game())
			{
				widget.value = sl.vote_heard && sl.vote.winner == NONE ? "AFTER THE VOTE" : "NO TEAMS";
				widget.flags |= AE_ROW_STATUS;
			}
			else if (rule == AE_SL_TEAMS_LOCKED)
			{
				widget.value = "THE SERVER BALANCES TEAMS";
				widget.flags |= AE_ROW_STATUS;
			}
			else if (!player)
			{
				widget.value = sl.state == AE_SL_STATE_DOWNLOAD ? "ONCE THE MAP IS HERE" : "JOINING...";
				widget.flags |= AE_ROW_STATUS;
			}
			else
			{
				if (split)
					snprintf(value, sizeof(value), "CONTROLLER %d: %s", player->controller + 1, player->team ? "BLUE" : "RED");
				else
					snprintf(value, sizeof(value), "%s", player->team ? "BLUE" : "RED");
				/* (the team's own word: on the bar the row's arrows; off it, in the team's light ink) */
				widget.value = focused ? value : NULL;
			}
		}
		else
		{
			widget.label = "RUN THE SERVER";
			widget.value = ae_ingame_role_word(ae_server_ingame_role());
			widget.flags |= AE_ROW_SUBSCREEN;
		}
		if (focused)
			widget.flags |= AE_ROW_FOCUSED;
		sl.item_rects[index].x = x0;
		sl.item_rects[index].y = y;
		sl.item_rects[index].width = x1 - x0;
		sl.item_rects[index].height = ae_widget_row(&sl.flow.density, x0, y, x1 - x0, &widget, HIT_ROWS, index);
		if (kind == ITEM_TEAM && !widget.value && value[0] && player)
		{
			widget.value = value;
			ae_row_layout(&sl.flow.density, x1 - x0, &widget, &layout);
			text_at(AE_FONT_ROW, ae_size_text(&sl.flow.density), x1 - u(20.0f), y + u(33.0f), AE_ALIGN_RIGHT,
				player->team ? TEAM_BLUE_INK : TEAM_RED_INK, value);
		}
		y += u(PITCH_U);
	}
	/* (the room left under the rows, over the help line: the menus' chat lines go there, nothing of the page under them
	(the game draws them under AE's screens)) */
	{
		struct ae_layout layout;
		float top_x, top_y, bottom_x, bottom_y;

		ae_draw_current_layout(&layout);
		ae_layout_to_pixels(&layout, x0 + u(10.0f), y + u(4.0f), &top_x, &top_y);
		ae_layout_to_pixels(&layout, x1, fy(HELP_RULE_Y) - u(12.0f), &bottom_x, &bottom_y);
		sl.chat_left = top_x;
		sl.chat_top = top_y;
		sl.chat_right = bottom_x;
		sl.chat_bottom = bottom_y;
		sl.chat_drawn_at = system_milliseconds() | 1;
	}
}

/* a team's bar: its stripe, its name, its count */
static float draw_team_bar(float x0, float x1, float y, char const *name, int people, int bots, unsigned int head,
	unsigned int stripe)
{
	char count[32];
	float height = u(34.0f);

	ae_draw_rect(x0, y, x1 - x0, height, 0.0f, head);
	ae_draw_rect(x0, y, u(5.0f), height, 0.0f, stripe);
	text_at(AE_FONT_ROW, u(18.0f), x0 + u(16.0f), y + height * 0.5f + u(7.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, name);
	ae_sl_team_count(people, bots, count, (int)sizeof(count));
	tracked_at(AE_FONT_ROW, u(16.0f), 0.06f, x1 - u(14.0f), y + height * 0.5f + u(7.0f), AE_ALIGN_RIGHT, 0xEBF0F5FFu, count);
	return height + u(8.0f);
}

/* the chat commands the server takes, as tokens: "Chat: [!nominate <map>] adds a game, [!rtv] starts a vote now" */
static void draw_chat_hint(float x, float baseline, float right, unsigned int flags)
{
	char const *parts[8];
	int tokens[8], count = 0, index;
	float size = u(16.5f), pad = u(8.0f);

	if (!(flags & (AE_SL_FLAG_NOMINATE | AE_SL_FLAG_RTV | AE_SL_FLAG_EXTEND)))
		return;
	parts[count] = "Chat:";
	tokens[count++] = 0;
	if (flags & AE_SL_FLAG_NOMINATE)
	{
		parts[count] = "!nominate <map>";
		tokens[count++] = 1;
		parts[count] = (flags & (AE_SL_FLAG_RTV | AE_SL_FLAG_EXTEND)) ? "adds a game," : "adds a game";
		tokens[count++] = 0;
	}
	if (flags & AE_SL_FLAG_RTV)
	{
		parts[count] = "!rtv";
		tokens[count++] = 1;
		parts[count] = (flags & AE_SL_FLAG_EXTEND) ? "starts a vote now," : "starts a vote now";
		tokens[count++] = 0;
	}
	if (flags & AE_SL_FLAG_EXTEND)
	{
		parts[count] = "!extend";
		tokens[count++] = 1;
		parts[count] = "gives the game more time";
		tokens[count++] = 0;
	}
	/* (what fits: whole pairs of a command and the words that say what it does) */
	{
		float at = x;
		int shown = 0;

		for (index = 0; index < count; index++)
		{
			float width = ae_draw_text_width(AE_FONT_BODY, size, parts[index]) + (tokens[index] ? 2.0f * pad : 0.0f);

			if (at + width > right || (tokens[index] && index + 1 < count && at + width + u(8.0f) +
				ae_draw_text_width(AE_FONT_BODY, size, parts[index + 1]) > right))
			{
				break;
			}
			at += width + u(8.0f);
			shown = index + 1;
		}
		/* ("Chat:" alone says nothing) */
		count = shown > 1 ? shown : 0;
	}
	for (index = 0; index < count; index++)
	{
		char words[64];
		float width;

		snprintf(words, sizeof(words), "%s", parts[index]);
		/* (the last words drawn end the line: no comma after them) */
		if (index == count - 1 && words[0] && words[strlen(words) - 1] == ',')
			words[strlen(words) - 1] = 0;
		width = ae_draw_text_width(AE_FONT_BODY, size, words) + (tokens[index] ? 2.0f * pad : 0.0f);
		if (tokens[index])
		{
			ae_draw_rect(x, baseline - u(18.0f), width, u(25.0f), 0.0f, TOKEN_FILL);
			ae_draw_outline(x, baseline - u(18.0f), width, u(25.0f), 0.0f, u(1.0f), TOKEN_EDGE);
			text_at(AE_FONT_BODY, size, x + pad, baseline, AE_ALIGN_LEFT, ICE, words);
		}
		else
			text_at(AE_FONT_BODY, size, x, baseline, AE_ALIGN_LEFT, AE_COLOR_MUTED, words);
		x += width + u(8.0f);
	}
}

static void draw_panel(float x0, float x1, float y0, float y1)
{
	struct ae_sl_wire const *info = ae_server_lobby_info();
	struct ae_density const *d = &sl.flow.density;
	float p0 = x0 + u(16.0f), p1 = x1 - u(16.0f), y, indent = u(AE_PARTY_INDENT_U), bottom = y1 - u(12.0f);
	char head[64], lines[4][160], line[64];
	int count, index, red_bots, blue_bots, all_bots, people = sl.have_roster ? sl.roster.people : 0, compact;
	short at = 0, team;

	ae_draw_rect(x0, y0, x1 - x0, y1 - y0, 0.0f, PANEL_FILL);
	ae_draw_rect(x0, y0, x1 - x0, u(39.0f), 0.0f, PANEL_HEAD);
	text_at(AE_FONT_ROW, u(18.0f), x0 + u(16.0f), y0 + u(26.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, "THIS SERVER");
	team_bots(&red_bots, &blue_bots, &all_bots);
	ae_sl_people_line(people, info && info->max_players ? info->max_players : sl.roster.maximum_players, all_bots, head,
		(int)sizeof(head));
	text_at(AE_FONT_BODY, u(16.0f), x1 - u(16.0f), y0 + u(26.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED, head);
	y = y0 + u(48.0f);
	if (info && info->motd[0])
	{
		count = ae_wrap_text(AE_FONT_BODY, u(17.0f), info->motd, p1 - p0 - u(32.0f), lines, 4);
		ae_draw_rect(p0, y, p1 - p0, u(18.0f) + u(24.0f) * (float)count, 0.0f, AE_COLOR_WELL);
		for (index = 0; index < count; index++)
			text_at(AE_FONT_BODY, u(17.0f), p0 + u(16.0f), y + u(24.0f) + u(24.0f) * (float)index, AE_ALIGN_LEFT,
				AE_COLOR_TEXT, lines[index]);
		y += u(18.0f) + u(24.0f) * (float)count;
	}
	if (info && (info->flags & (AE_SL_FLAG_NOMINATE | AE_SL_FLAG_RTV | AE_SL_FLAG_EXTEND)))
	{
		y += u(30.0f);
		draw_chat_hint(p0 + u(16.0f), y, p1, info->flags);
		y += u(18.0f);
	}
	else
		y += u(12.0f);
	/* (rows of 50 u while they fit, else LOBBY's 36 u rows) */
	{
		float tall = (float)people * u(56.0f) + (team_game() ? 2.0f : 1.0f) * (u(42.0f) + u(46.0f));

		compact = y + tall > bottom;
	}
	memset(sl.person_rects, 0, sizeof(sl.person_rects));
	for (team = 0; team < (team_game() ? 2 : 1); team++)
	{
		int team_people = 0, bots = team_game() ? (team ? blue_bots : red_bots) : all_bots;
		short index2;

		for (index2 = 0; index2 < sl.people_count; index2++)
			team_people += sl.people[index2].team == (team_game() ? team : -1);
		if (team_game())
			y += draw_team_bar(p0, p1, y, team ? "BLUE TEAM" : "RED TEAM", team_people, bots, team ? TEAM_BLUE_HEAD :
				TEAM_RED_HEAD, team ? TEAM_BLUE : TEAM_RED);
		else
			y += draw_team_bar(p0, p1, y, "PLAYERS", team_people, bots, PANEL_HEAD, AE_COLOR_ACCENT);
		for (; at < sl.people_count && sl.people[at].team == (team_game() ? team : -1); at++)
		{
			struct ae_lobby_player const *player = &sl.roster.players[sl.people[at].card];
			struct ae_party_row row;
			char sub[48];
			float height;

			if (y + u(compact || !team_game() ? 42.0f : 56.0f) > bottom)
				break;
			memset(&row, 0, sizeof(row));
			/* (a free for all's: a dark row tinted with their colour, never a white one that reads as the focus) */
			row.kind = !team_game() ? AE_PARTY_MEMBER : compact ? (player->local ? AE_PARTY_SELF : AE_PARTY_MEMBER) :
				AE_PARTY_PERSON;
			row.name = player->name;
			row.slot = (short)(at + 1);
			row.color = team_game() ? (player->team ? TEAM_BLUE : TEAM_RED) : player->color;
			if (player->local)
			{
				if (local_people() > 1)
					snprintf(sub, sizeof(sub), "This PC \xE2\x80\xA2 controller %d", player->controller + 1);
				else
					snprintf(sub, sizeof(sub), "This PC");
				row.sub_line = sub;
			}
			if (sl.column == 1 && at == sl.person_focus)
				row.flags |= AE_CARD_FOCUSED;
			height = ae_widget_party_row(d, p0 - indent, y, p1 - p0 + indent, &row, HIT_PEOPLE, at);
			sl.person_rects[at].x = p0;
			sl.person_rects[at].y = y;
			sl.person_rects[at].width = p1 - p0;
			sl.person_rects[at].height = height;
			y += height;
		}
		/* (the people that do not fit: one line) */
		{
			short left = 0, index3;

			for (index3 = at; index3 < sl.people_count && sl.people[index3].team == (team_game() ? team : -1); index3++)
				left++;
			if (left)
			{
				snprintf(line, sizeof(line), "+ %d MORE", left);
				tracked_at(AE_FONT_ROW, u(14.5f), 0.08f, p0 + u(16.0f), y + u(20.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT, line);
				y += u(28.0f);
				at = (short)(at + left);
			}
		}
		ae_sl_bots_line(bots, line, (int)sizeof(line));
		if (line[0] && y + u(28.0f) <= bottom)
		{
			tracked_at(AE_FONT_ROW, u(14.5f), 0.08f, p0 + u(16.0f), y + u(20.0f), AE_ALIGN_LEFT, DIM_INK, line);
			y += u(36.0f);
		}
		y += u(10.0f);
	}
}

int ae_screen_server_lobby_chat_box(short line_height, short *left, short *right, short *top, short *bottom)
{
	if (!ae_screen_server_lobby_open() || !sl.chat_drawn_at || system_milliseconds() - sl.chat_drawn_at > 500 ||
		line_height <= 0 || !ae_ui_top() || ae_ui_top()->data != &sl || sl.chat_bottom - sl.chat_top < line_height * 2)
	{
		return 0;
	}
	*left = (short)(sl.chat_left + line_height * 0.5f);
	*right = (short)(sl.chat_right - line_height * 0.5f);
	/* (chat.c stops a line once it is past four lines over its top, measured before its 0.8 scale: about two lines
	over the room's top keeps its lines inside the room) */
	*top = (short)(sl.chat_top - line_height * 2);
	*bottom = (short)sl.chat_bottom;
	if (!sl.chat_logged)
	{
		sl.chat_logged = 1;
		platform_log("ae server lobby: the chat's lines in the page's room (%d to %d, %d to %d)", *left, *right, *top,
			*bottom);
		ae_test_capture("server_lobby_chat", 400);
	}
	return 1;
}

int ae_screen_server_lobby_chat_area(short line_height, short *left, short *right, short *edge, short *rows)
{
	short fit;

	if (!ae_screen_server_lobby_open() || !sl.chat_drawn_at || system_milliseconds() - sl.chat_drawn_at > 500 ||
		line_height <= 0 || !ae_ui_top() || ae_ui_top()->data != &sl)
	{
		return 0;
	}
	fit = (short)((sl.chat_bottom - sl.chat_top) / (float)line_height);
	if (fit < 2)
		return 0;
	*left = (short)(sl.chat_left + line_height * 0.5f);
	*right = (short)(sl.chat_right - line_height * 0.5f);
	*edge = (short)sl.chat_bottom;
	if (*rows > fit)
		*rows = fit;
	return 1;
}

static void draw_help(void)
{
	struct ae_sl_help help;
	char text[200], expanded[320], words[64], lines[2][160];
	char const *map = "", *gametype = NULL;
	float x = ae_flow_left_x(&sl.flow), width = ae_flow_right_x(&sl.flow) - x, size = u(21.0f);
	int count, index, kind = sl.column == 0 ? item_kind(sl.focus) : -1;

	ae_draw_rect(x, fy(HELP_RULE_Y), width, u(AE_FLOW_RULE_U), 0.0f, AE_COLOR_RULE);
	if (sl.note_until && system_milliseconds() < sl.note_until)
		snprintf(text, sizeof(text), "%s", sl.note);
	else
	{
		card_entry(&map, &gametype);
		map_words(map, words, (int)sizeof(words));
		memset(&help, 0, sizeof(help));
		help.state = sl.state;
		help.focus = sl.column == 1 ? AE_SL_FOCUS_PLAYER : kind == ITEM_CHOICE ? AE_SL_FOCUS_LIST :
			kind == ITEM_SERVER ? AE_SL_FOCUS_SERVER : AE_SL_FOCUS_TEAM;
		help.team_rule = ae_server_lobby_info() ? ae_server_lobby_info()->team_rule : AE_SL_TEAMS_OPEN;
		help.team_game = team_game();
		help.split = local_people() > 1;
		help.pack_map = strchr(map, '@') && !strstr(map, "@mcc") && !strstr(map, "@ws");
		help.map = words;
		if (sl.column == 1 && sl.person_focus >= 0 && sl.person_focus < sl.people_count)
		{
			help.player = sl.roster.players[sl.people[sl.person_focus].card].name;
			help.player_bot = sl.roster.players[sl.people[sl.person_focus].card].bot;
		}
		ae_sl_help_text(&help, text, (int)sizeof(text));
	}
	count = ae_wrap_text(AE_FONT_BODY, size, ae_help_expand(text, expanded, sizeof(expanded)), width, lines, 2);
	for (index = 0; index < count; index++)
		text_at(AE_FONT_BODY, size, x, fy(HELP_BASE_Y) + u(31.0f) * (float)index, AE_ALIGN_LEFT, AE_COLOR_TITLE,
			lines[index]);
}

static void draw_prompts(void)
{
	int kind = sl.column == 0 ? item_kind(sl.focus) : -1;
	struct ae_sl_wire const *info = ae_server_lobby_info();
	int teams = team_game() && (!info || info->team_rule != AE_SL_TEAMS_LOCKED);

	sl.flow.prompt_count = 0;
	if (sl.column == 1)
		sl.flow.prompts[sl.flow.prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Player card", AE_ACTION_ACCEPT);
	else if (kind == ITEM_CHOICE)
		sl.flow.prompts[sl.flow.prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Vote", AE_ACTION_ACCEPT);
	else if (kind == ITEM_SERVER || (kind == ITEM_TEAM && teams))
		sl.flow.prompts[sl.flow.prompt_count++] = ae_flow_prompt(AE_BUTTON_A, ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
	sl.flow.prompts[sl.flow.prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Leave", AE_ACTION_BACK);
	if (teams)
		sl.flow.prompts[sl.flow.prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Change team", AE_ACTION_Y);
	if (sl.column == 0)
		sl.flow.prompts[sl.flow.prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Player card", AE_ACTION_X);
	sl.flow.prompts[sl.flow.prompt_count] = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Column", AE_ACTION_TAB_PREVIOUS);
	sl.flow.prompts[sl.flow.prompt_count++].pair = 1;
	ae_flow_draw_prompts(&sl.flow);
}

static void page_draw(struct ae_screen *screen)
{
	float left, split, right, top, card_bottom;

	(void)screen;
	ae_flow_full_density(&sl.flow);
	draw_header();
	left = ae_flow_left_x(&sl.flow);
	split = left + sl.flow.frame.rect.width * AE_FLOW_ROWS_WIDTH;
	right = ae_flow_right_x(&sl.flow);
	top = fy(AE_FLOW_CONTENT_Y);
	card_bottom = draw_card(left, split, top);
	draw_left(left, split, card_bottom + u(34.0f));
	draw_panel(fx(COLUMN_SPLIT), right - u(20.0f), top, fy(PANEL_BOTTOM_Y));
	draw_help();
	draw_prompts();
}

/* ---------- acting */

/* ---- removed by the server (a kick or a ban: the server sends no reason or length on the wire) */

static void removed_picked(short button, void *context)
{
	(void)context;
	if (button == 0 && ae_screen_browser_open(AE_BROWSER_INTERNET))
		platform_log("ae server lobby: removed: SERVER BROWSER");
}

static void removed_show(void)
{
	struct ae_attention_spec spec;
	char upper_name[48], body[160];

	upper(sl.server_name[0] ? sl.server_name : "the server", upper_name, (int)sizeof(upper_name));
	snprintf(body, sizeof(body), "An admin removed you from %s. You can join it again once its admins let you back in.",
		upper_name);
	memset(&spec, 0, sizeof(spec));
	spec.title = "REMOVED FROM THE SERVER";
	spec.body = body;
	spec.buttons[0] = "SERVER BROWSER";
	spec.buttons[1] = "MAIN MENU";
	spec.button_count = 2;
	spec.focus = 0;
	spec.danger = -1;
	spec.back = 1;
	spec.note = "Kicks and bans come from the server's admins, not from Arena Evolved.";
	spec.back_label = "Main menu";
	spec.density = sl.flow.density;
	spec.picked = removed_picked;
	if (ae_attention_open(&spec))
	{
		platform_log("ae server lobby: removed from the server (code %d): told", (int)sl.removed_code);
		ae_test_capture("server_lobby_removed", 900);
	}
	sl.removed_code = 0;
}

int ae_screen_server_lobby_rejected(unsigned short code)
{
	if (!ae_screen_server_lobby_open() || code != 6)
		return 0;
	sl.removed_code = (short)code;
	sl.removed_at = system_milliseconds();
	return 1;
}

static void close_page(char const *why)
{
	if (!sl.on_stack)
		return;
	sl.on_stack = 0;
	if (ae_ui_holds(&sl))
		ae_ui_remove(&sl);
	platform_log("ae server lobby: closed (%s)", why);
	ae_test_capture("server_lobby_closed", 1500);
	if (sl.removed_code == 6 && system_milliseconds() - sl.removed_at < 5000)
		removed_show();
}

static void leave_picked(short choice, void *context)
{
	(void)context;
	if (choice != 1)
		return;
	platform_log("ae server lobby: leave");
	ae_lobby_leave();
	close_page("left");
}

static void leave_ask(void)
{
	struct ae_dialog_spec spec;

	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "Leave the server?";
	spec.body = "You leave its lobby. Its games go on without you.";
	spec.choices[0] = "STAY";
	spec.choices[1] = "LEAVE";
	spec.choice_count = 2;
	spec.has_cancel = 1;
	spec.density = sl.flow.density;
	spec.bounds = sl.flow.frame.rect;
	spec.picked = leave_picked;
	ae_sound_request(AE_SOUND_BACK, 0);
	ae_dialog_open(&spec, AE_OWNER_ANY);
	ae_test_capture("server_lobby_dialog", 600);
}

static void vote_for(short choice)
{
	if (!sl.vote_heard || sl.vote.winner != NONE || choice < 0 || choice >= sl.vote.count ||
		!network_map_vote_choose(choice))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("ae server lobby: voted for %s:%s", sl.vote.map_names[choice], sl.vote.gametypes[choice]);
	ae_test_capture("server_lobby_voted", 700);
}

static void team_change(short controller)
{
	struct ae_sl_wire const *info = ae_server_lobby_info();

	if (!team_game())
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (info && info->team_rule == AE_SL_TEAMS_LOCKED)
	{
		note_set("This server puts players on teams itself.");
		platform_log("ae server lobby: team change refused: the server balances teams");
		ae_test_capture("server_lobby_team", 300);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (!local_player_of(controller))
		controller = 0;
	if (!ae_lobby_change_team(controller))
	{
		platform_log("ae server lobby: team change refused: not sent (controller %d)", controller + 1);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	platform_log("ae server lobby: team change asked (controller %d)", controller + 1);
	ae_test_capture("server_lobby_team", 1500);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* ---- the server page (RUN THE SERVER): UP NEXT, SAY TO EVERYONE, RESTART THE SERVER */

enum { SERVER_NEXT, SERVER_SAY, SERVER_RESTART };

static void next_picked(short index, void *context)
{
	int sent;

	(void)context;
	if (index < 0 || index >= sl.next_count)
		return;
	sent = ae_server_ingame_act(AE_INGAME_NEXT, NONE, sl.next_indexes[index], NULL);
	note_set(sent ? "Asking the server..." : "The server can't be asked now.");
	platform_log("ae server lobby: asks the server to play %s next: %s", sl.next_texts[index], sent ? "sent" : "not sent");
}

static void next_open(void)
{
	struct ae_ingame_wire const *wire = ae_server_ingame_info();
	struct ae_picker_spec spec;
	short index;

	sl.next_count = 0;
	for (index = 0; wire && index < wire->count && sl.next_count < AE_INGAME_WIRE_ENTRIES; index++)
	{
		char map[48], gametype[32];

		if (wire->entries[index].flags & AE_INGAME_ENTRY_REJECTED)
			continue;
		ae_ingame_map_word(wire->entries[index].map, map, (int)sizeof(map));
		ae_ingame_gametype_word(wire->entries[index].gametype, gametype, (int)sizeof(gametype));
		snprintf(sl.next_texts[sl.next_count], sizeof(sl.next_texts[0]), "%s  %s", map, gametype);
		sl.next_values[sl.next_count] = sl.next_texts[sl.next_count];
		sl.next_indexes[sl.next_count] = wire->entries[index].index;
		sl.next_count++;
	}
	if (!sl.next_count)
	{
		note_set("The server lists no games to pick now.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	memset(&spec, 0, sizeof(spec));
	spec.values = sl.next_values;
	spec.count = sl.next_count;
	spec.current = 0;
	spec.row = sl.item_rects[sl.focus];
	spec.bounds = sl.flow.frame.rect;
	spec.density = sl.flow.density;
	spec.picked = next_picked;
	spec.title = "UP NEXT";
	if (ae_picker_open(&spec, AE_OWNER_ANY))
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* (a text with no limit takes no character; the line is cut at the 95 the server takes) */
enum { SAY_LIMIT = 95 };

static void say_done(int keep, void *context)
{
	char line[96];
	int sent;

	(void)context;
	if (!keep || !ae_ingame_clean_text(sl.say.text, line, (int)sizeof(line)))
		return;
	sent = ae_server_ingame_act(AE_INGAME_SAY, NONE, 0, line);
	note_set(sent ? "Asking the server..." : "The server can't be asked now.");
	platform_log("ae server lobby: says to everyone: %s", sent ? "sent" : "not sent");
	ae_text_init(&sl.say, "", SAY_LIMIT, 0);
}

static void say_open(void)
{
	struct ae_keyboard_spec spec;

	memset(&spec, 0, sizeof(spec));
	ae_text_init(&sl.say, "", SAY_LIMIT, 0);
	spec.text = &sl.say;
	spec.field = sl.item_rects[sl.focus];
	spec.bounds = sl.flow.frame.rect;
	spec.density = sl.flow.density;
	spec.done = say_done;
	if (!ae_keyboard_open(&spec, AE_OWNER_ANY))
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

static void restart_picked(short choice, void *context)
{
	int sent;

	(void)context;
	if (choice != 1)
		return;
	sent = ae_server_ingame_act(AE_INGAME_RESTART, NONE, AE_INGAME_RESTART_SECONDS, NULL);
	note_set(sent ? "Asking the server: it restarts in a minute, everyone warned." : "The server can't be asked now.");
	platform_log("ae server lobby: asks the server to restart: %s", sent ? "sent" : "not sent");
}

static void server_picked(short choice, void *context)
{
	struct ae_dialog_spec spec;

	(void)context;
	if (choice < 0 || choice >= sl.server_count)
		return;
	switch (sl.server_rows[choice])
	{
	case SERVER_NEXT:
		next_open();
		return;
	case SERVER_SAY:
		say_open();
		return;
	default:
		memset(&spec, 0, sizeof(spec));
		spec.kind = AE_DIALOG_CONFIRM;
		spec.title = "Restart the server?";
		spec.body = "Everyone is warned and the server restarts in a minute. Players join again after it.";
		spec.choices[0] = ae_string(AE_STR_CANCEL_CAPS);
		spec.choices[1] = "RESTART";
		spec.choice_count = 2;
		spec.has_cancel = 1;
		spec.destructive_choices = 2u;
		spec.density = sl.flow.density;
		spec.bounds = sl.flow.frame.rect;
		spec.picked = restart_picked;
		ae_dialog_open(&spec, AE_OWNER_ANY);
		return;
	}
}

static void server_open(void)
{
	struct ae_ingame_wire const *wire = ae_server_ingame_info();
	struct ae_dialog_spec spec;
	char body[160];
	short count = 0;

	if (!wire)
	{
		note_set("The server has not said your role yet.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	memset(&spec, 0, sizeof(spec));
	if (ae_ingame_allows(wire->allowed, AE_INGAME_NEXT))
	{
		spec.choices[count] = "UP NEXT...";
		sl.server_rows[count++] = SERVER_NEXT;
	}
	if (ae_ingame_allows(wire->allowed, AE_INGAME_SAY))
	{
		spec.choices[count] = "SAY TO EVERYONE...";
		sl.server_rows[count++] = SERVER_SAY;
	}
	if (ae_ingame_allows(wire->allowed, AE_INGAME_RESTART))
	{
		spec.destructive_choices |= 1u << count;
		spec.choices[count] = "RESTART THE SERVER";
		sl.server_rows[count++] = SERVER_RESTART;
	}
	sl.server_count = count;
	spec.choices[count++] = ae_string(AE_STR_CANCEL_CAPS);
	snprintf(body, sizeof(body), "You are %s %s here. The web page and the pause menu's SERVER page have the rest.",
		wire->role == AE_INGAME_ROLE_ADMIN ? "an" : "the", ae_ingame_role_word(wire->role));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "Run the server";
	spec.body = body;
	spec.choice_count = count;
	spec.safe_choice = 0;
	spec.cancel_choice = (short)(count - 1);
	spec.has_cancel = 1;
	spec.density = sl.flow.density;
	spec.bounds = sl.flow.frame.rect;
	spec.picked = server_picked;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_dialog_open(&spec, AE_OWNER_ANY);
	platform_log("ae server lobby: server page (%s)", ae_ingame_role_word(wire->role));
	ae_test_capture("server_lobby_dialog", 600);
}

/* ---- the player card */

enum { CARD_MOVE, CARD_MUTE, CARD_KICK, CARD_BAN, CARD_RIVAL, CARD_TEAM };

static struct ae_lobby_player const *card_player(void)
{
	short index;

	for (index = 0; sl.have_roster && index < sl.roster.count; index++)
		if (sl.roster.players[index].machine == sl.card_machine && sl.roster.players[index].controller == sl.card_controller)
			return &sl.roster.players[index];
	return NULL;
}

static char const *owner_name(void)
{
	struct ae_lobby_player const *mine = local_player_of(0);

	return mine ? mine->name : "";
}

static void ban_picked(short choice, short chip, void *context)
{
	struct ae_lobby_player const *player = card_player();
	int sent;

	(void)context;
	if (choice != 1 || !player)
		return;
	sent = ae_server_ingame_act(AE_INGAME_BAN, player->machine, ae_ingame_ban_minutes(chip), player->name);
	note_set(sent ? "Asking the server..." : "The server can't be asked now.");
	platform_log("ae server lobby: asks the server to ban '%s' for %s: %s", player->name, ae_ingame_ban_word(chip),
		sent ? "sent" : "not sent");
}

static void card_picked(short choice, void *context)
{
	struct ae_lobby_player const *player = card_player();
	int sent, index;

	(void)context;
	if (choice < 0 || choice >= sl.card_count || !player)
		return;
	switch (sl.card_rows[choice])
	{
	case CARD_TEAM:
		team_change(player->controller);
		return;
	case CARD_MOVE:
		sent = ae_server_ingame_act(AE_INGAME_MOVE, player->machine, player->team ? 0 : 1, player->name);
		note_set(sent ? "Asking the server..." : "The server can't be asked now.");
		platform_log("ae server lobby: asks the server to move '%s': %s", player->name, sent ? "sent" : "not sent");
		return;
	case CARD_MUTE:
		if (ae_server_ingame_role() > AE_INGAME_ROLE_NONE)
		{
			sent = ae_server_ingame_act(AE_INGAME_MUTE, player->machine, 0, player->name);
			note_set(sent ? "Asking the server..." : "The server can't be asked now.");
		}
		else
		{
			int muted = ae_moderation_muted(player->name);

			sent = ae_moderation_mute(player->name, !muted);
			snprintf(sl.note, sizeof(sl.note), sent ? (muted ? "%s can chat and talk again." : "%s is muted for you.") :
				"Can't mute %s now.", player->name);
			sl.note_until = system_milliseconds() + NOTE_MS;
		}
		platform_log("ae server lobby: mute '%s': %s", player->name, sent ? "done" : "not done");
		return;
	case CARD_KICK:
		sent = ae_server_ingame_act(AE_INGAME_KICK, player->machine, 0, player->name);
		note_set(sent ? "Asking the server..." : "The server can't be asked now.");
		platform_log("ae server lobby: asks the server to kick '%s': %s", player->name, sent ? "sent" : "not sent");
		return;
	case CARD_RIVAL:
	{
		int on = !ae_record_is_rival(NULL, owner_name(), player->name);

		ae_record_store_rival(owner_name(), player->name, on);
		snprintf(sl.note, sizeof(sl.note), on ? "%s is your rival: the post-game report marks them." :
			"%s is no longer your rival.", player->name);
		sl.note_until = system_milliseconds() + NOTE_MS;
		platform_log("ae server lobby: rival '%s' %s", player->name, on ? "on" : "off");
		return;
	}
	default:
	{
		struct ae_dialog_spec spec;
		char title[64];

		memset(&spec, 0, sizeof(spec));
		snprintf(title, sizeof(title), "Ban %s?", player->name);
		spec.kind = AE_DIALOG_CONFIRM;
		spec.title = title;
		spec.body = "They leave the server now and can't join it again for the time picked. The web page's BANS undo it.";
		spec.choices[0] = ae_string(AE_STR_CANCEL_CAPS);
		spec.choices[1] = "BAN";
		spec.choice_count = 2;
		spec.has_cancel = 1;
		spec.destructive_choices = 2u;
		for (index = 0; index < AE_INGAME_BAN_LENGTHS && index < AE_DIALOG_CHIPS; index++)
		{
			static char yes[AE_INGAME_BAN_LENGTHS][64];

			spec.chips[index] = ae_ingame_ban_word(index);
			ae_ingame_ban_yes(player->name, index, yes[index], (int)sizeof(yes[index]));
			spec.chip_labels[index] = yes[index];
		}
		spec.chip_count = AE_INGAME_BAN_LENGTHS;
		spec.chip_first = AE_INGAME_BAN_FIRST;
		spec.chip_choice = 1;
		spec.picked_chip = ban_picked;
		spec.density = sl.flow.density;
		spec.bounds = sl.flow.frame.rect;
		ae_dialog_open(&spec, AE_OWNER_ANY);
		return;
	}
	}
}

static void card_open(short person)
{
	struct ae_lobby_player const *player;
	struct ae_ingame_wire const *wire = ae_server_ingame_info();
	struct ae_dialog_spec spec;
	unsigned int allowed = wire ? wire->allowed : 0;
	char const *team = team_game() ? "" : NULL;
	short count = 0;

	if (person < 0 || person >= sl.people_count)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	player = &sl.roster.players[sl.people[person].card];
	sl.card_machine = player->machine;
	sl.card_controller = player->controller;
	snprintf(sl.card_name, sizeof(sl.card_name), "%s", player->name);
	if (team)
		team = player->team ? "Blue Team" : "Red Team";
	memset(&spec, 0, sizeof(spec));
	if (player->local)
	{
		snprintf(sl.card_body, sizeof(sl.card_body), "%s%sThis PC.", team ? team : "", team ? ". " : "");
		if (team_game())
		{
			spec.choices[count] = "CHANGE TEAM";
			sl.card_rows[count++] = CARD_TEAM;
		}
	}
	else
	{
		snprintf(sl.card_body, sizeof(sl.card_body), "%s%sOn this server with you.", team ? team : "", team ? ". " : "");
		if (team_game() && ae_ingame_allows(allowed, AE_INGAME_MOVE))
		{
			snprintf(sl.card_move, sizeof(sl.card_move), "%s", ae_ingame_move_label(player->team));
			spec.choices[count] = sl.card_move;
			sl.card_rows[count++] = CARD_MOVE;
		}
		spec.choices[count] = !ae_server_ingame_role() && ae_moderation_muted(player->name) ? "UNMUTE IN CHAT" :
			"MUTE IN CHAT";
		sl.card_rows[count++] = CARD_MUTE;
		if (ae_ingame_allows(allowed, AE_INGAME_KICK))
		{
			spec.destructive_choices |= 1u << count;
			spec.choices[count] = "KICK FOR 2 MINUTES";
			sl.card_rows[count++] = CARD_KICK;
		}
		if (ae_ingame_allows(allowed, AE_INGAME_BAN) && count < AE_DIALOG_CHOICES - 1)
		{
			spec.destructive_choices |= 1u << count;
			spec.choices[count] = "BAN FROM THIS SERVER...";
			sl.card_rows[count++] = CARD_BAN;
		}
		else if (count < AE_DIALOG_CHOICES - 1 && owner_name()[0])
		{
			spec.choices[count] = ae_record_is_rival(NULL, owner_name(), player->name) ? "NOT MY RIVAL" : "SET AS RIVAL";
			sl.card_rows[count++] = CARD_RIVAL;
		}
	}
	sl.card_count = count;
	spec.choices[count++] = ae_string(AE_STR_CANCEL_CAPS);
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = sl.card_name;
	spec.body = sl.card_body;
	spec.choice_count = count;
	spec.safe_choice = 0;
	spec.cancel_choice = (short)(count - 1);
	spec.has_cancel = 1;
	spec.density = sl.flow.density;
	spec.bounds = sl.flow.frame.rect;
	spec.picked = card_picked;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_dialog_open(&spec, AE_OWNER_ANY);
	platform_log("ae server lobby: card of '%s'", player->name);
	ae_test_capture("server_lobby_dialog", 600);
}

/* ---- the input */

static void column_set(int column)
{
	if (column == sl.column || (column == 1 && !sl.people_count))
		return;
	sl.column = column;
	if (column == 1 && (sl.person_focus < 0 || sl.person_focus >= sl.people_count))
		sl.person_focus = 0;
	ae_sound_request(AE_SOUND_CURSOR, 0);
	platform_log("ae server lobby: column %s", column ? "panel" : "left");
	ae_test_capture("server_lobby_column", 500);
}

static void accept(void)
{
	if (sl.column == 1)
	{
		card_open(sl.person_focus);
		return;
	}
	switch (item_kind(sl.focus))
	{
	case ITEM_CHOICE:
		vote_for(sl.items[sl.focus]);
		return;
	case ITEM_TEAM:
		team_change(sl.team_controller);
		return;
	case ITEM_SERVER:
		server_open();
		return;
	}
}

/* split screen: a pad's START joins its player; a guest pad's B takes it out (never the PC's last) */
static int split_screen_press(struct ae_event const *event)
{
	short controller = ae_input_event_controller();

	if (controller < 1)
		return 0;
	if (event->action == AE_ACTION_START && !ae_lobby_controller_plays(controller))
	{
		struct ae_result result = ae_lobby_join_controller(controller);

		platform_log("ae server lobby: controller %d joins: %s", controller + 1, result.ok ? "in" : "refused");
		ae_sound_request(result.ok ? AE_SOUND_FORWARD : AE_SOUND_FAILURE, 0);
		return 1;
	}
	if (event->action == AE_ACTION_BACK && ae_lobby_controller_plays(controller) && ae_lobby_local_players() >= 2)
	{
		struct ae_result result = ae_lobby_leave_controller(controller);

		ae_sound_request(result.ok ? AE_SOUND_BACK : AE_SOUND_FAILURE, 0);
		return 1;
	}
	return 0;
}

static int page_handle(struct ae_screen *screen, struct ae_event const *event)
{
	short controller = ae_input_event_controller();

	(void)screen;
	if (split_screen_press(event))
		return 1;
	if (controller >= 0 && controller < 4 && local_player_of(controller))
		sl.team_controller = controller;
	switch (event->action)
	{
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
	{
		short step = event->action == AE_ACTION_UP ? -1 : 1;

		if (sl.column == 1)
		{
			short next = (short)(sl.person_focus + step);

			if (next >= 0 && next < sl.people_count)
			{
				sl.person_focus = next;
				ae_sound_request(AE_SOUND_CURSOR, event->repeat);
			}
		}
		else
		{
			short next = (short)(sl.focus + step);

			if (next >= 0 && next < sl.item_count)
			{
				sl.focus = next;
				ae_sound_request(AE_SOUND_CURSOR, event->repeat);
			}
		}
		return 1;
	}
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (sl.column == 0 && item_kind(sl.focus) == ITEM_TEAM)
			team_change(sl.team_controller);
		return 1;
	case AE_ACTION_ACCEPT:
		accept();
		return 1;
	case AE_ACTION_Y:
		team_change(sl.team_controller);
		return 1;
	case AE_ACTION_X:
		if (sl.column == 1)
			card_open(sl.person_focus);
		else
			card_open(sl.person_focus >= 0 ? sl.person_focus : 0);
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		column_set(event->action == AE_ACTION_TAB_NEXT ? 1 : sl.column ? 0 : 1);
		return 1;
	case AE_ACTION_BACK:
		leave_ask();
		return 1;
	default:
		return 1;
	}
}

static void page_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;

	(void)screen;
	if (ae_prompts_pointer(sl.flow.prompts, sl.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		page_handle(screen, &event);
		return;
	}
	if (!ae_hit_at(pointer->x, pointer->y, 0, &hit))
		return;
	if (hit.id == HIT_ROWS && hit.index >= 0 && hit.index < sl.item_count)
	{
		if (pointer->moved)
		{
			sl.column = 0;
			sl.focus = hit.index;
		}
		if (pointer->left_clicks)
		{
			sl.column = 0;
			sl.focus = hit.index;
			accept();
		}
	}
	else if (hit.id == HIT_PEOPLE && hit.index >= 0 && hit.index < sl.people_count)
	{
		if (pointer->moved && sl.column == 1)
			sl.person_focus = hit.index;
		if (pointer->left_clicks)
		{
			sl.column = 1;
			sl.person_focus = hit.index;
			card_open(hit.index);
		}
	}
}

/* this PC's players the menus joined to multiplayer (the join's own, or the next game's after one) asked for in the
server's lobby until they are in: what upstream's GAME LOBBY did each frame (its widget is gone under the page) */
static void players_ask(void)
{
	static unsigned long asked_at[4];
	unsigned long now = system_milliseconds();
	short controller;

	if (!sl.have_roster)
		return;
	for (controller = 0; controller < 4; controller++)
	{
		if (!player_ui_local_player_wants_to_play_multiplayer(controller) || local_player_of(controller) ||
			(asked_at[controller] && now - asked_at[controller] < 1500))
		{
			continue;
		}
		asked_at[controller] = now + (now == 0);
		platform_log("ae server lobby: this PC's player %d asked for (controller %d): %s", controller + 1, controller + 1,
			ae_lobby_add_local_player(controller).ok ? "sent" : "not sent");
	}
}

static void page_update(struct ae_screen *screen)
{
	(void)screen;
	read_state();
	players_ask();
	items_build();
	people_build();
	{
		char text[128];
		int done;

		if (ae_server_ingame_result(text, (int)sizeof(text), &done))
			note_set(text);
	}
}

static void page_leave(struct ae_screen *screen)
{
	(void)screen;
	sl.on_stack = 0;
}

static struct ae_screen_class const page_class =
{
	.name = "server lobby", .leave = page_leave, .handle = page_handle, .draw = page_draw, .pointer = page_pointer,
	.update = page_update, .start_from_anyone = 1,
};

/* ---------- opening */

int ae_screen_server_lobby_open(void)
{
	return sl.on_stack && ae_ui_holds(&sl);
}

int ae_screen_server_lobby_show(const char *how)
{
	if (ae_screen_server_lobby_open())
		return 1;
	memset(&sl, 0, sizeof(sl));
	sl.logged_state = -1;
	sl.person_focus = -1;
	sl.focus = -1;
	sl.flow.hit_rows = HIT_ROWS;
	sl.flow.hit_prompts = HIT_PROMPTS;
	snprintf(sl.how, sizeof(sl.how), "%s", how ? how : "");
	/* (a join's own lobby screen goes: this page is the lobby of a dedicated server) */
	ae_screen_create_game_joiner_close();
	ae_lobby_set_pregame_screen(&page_class, &sl);
	if (!ae_ui_push(&page_class, AE_OWNER_ANY, &sl))
	{
		platform_log("ae server lobby: could not open");
		return 0;
	}
	sl.on_stack = 1;
	sl.opened_at = system_milliseconds();
	read_state();
	items_build();
	people_build();
	platform_log("ae server lobby: open (%s)", sl.how[0] ? sl.how : "joined");
	return 1;
}

int ae_screen_server_lobby_take_pregame(void)
{
	if (!ae_server_lobby_dedicated())
		return 0;
	return ae_screen_server_lobby_show("back from a game");
}

void ae_screen_server_lobby_tick(unsigned long now)
{
	int dedicated = ae_server_lobby_dedicated(), in_lobby;

	ae_server_lobby_client_tick(now);
	if (!ae_menus_active())
		return;
	{
		static int logged = -1;
		int seen = (dedicated ? 1 : 0) | (main_menu_is_active() ? 2 : 0) | (ae_ui_depth() ? 4 : 0) |
			(game_connection() == _game_connection_network_client ? 8 : 0);

		if (seen != logged && (seen & 8))
		{
			logged = seen;
			platform_log("ae server lobby: a client: dedicated %d, menus %d, AE screens %d, to start %d", seen & 1,
				(seen >> 1) & 1, ae_ui_depth(), (int)ae_lobby_seconds_to_start());
		}
	}
	/* (this machine's client in the server's lobby: the main menu up, the game not started) */
	in_lobby = dedicated && main_menu_is_active() && ae_lobby_seconds_to_start() != AE_COUNTDOWN_SECONDS_STARTED;
	if (ae_screen_server_lobby_open())
	{
		/* (the session gone, kicked or left: the page with it; the game started: off the stack while it plays, the
		pregame take-over brings it back) */
		if (game_connection() != _game_connection_network_client)
			close_page("no longer joined");
		else if (!main_menu_is_active())
			close_page("the game started");
		return;
	}
	if (!in_lobby)
	{
		sl.joined_at = 0;
		return;
	}
	if (!sl.joined_at)
		sl.joined_at = now + (now == 0);
	/* (CREATE GAME's joiner LOBBY up: replaced at once) */
	if (ae_screen_create_game_joiner_open())
	{
		ae_screen_server_lobby_show("joined");
		return;
	}
	/* (upstream's GAME LOBBY, the menus' own join (AE's main menu left under it): once this PC's player is in, or after
	a while) */
	if (!ae_ui_depth() || (ae_ui_top() && ae_ui_top()->screen_class && ae_ui_top()->screen_class->name &&
		!strcmp(ae_ui_top()->screen_class->name, "main menu")))
	{
		struct ae_lobby_roster roster;
		short index;
		int mine = 0;

		if (ae_lobby_roster(&roster).ok)
			for (index = 0; index < roster.count; index++)
				mine |= roster.players[index].local && !roster.players[index].bot;
		if (mine || now - sl.joined_at > TAKE_OVER_MS)
		{
			ui_widgets_close_all();
			ae_screen_main_menu_close();
			ae_screen_server_lobby_show("joined (the game's lobby replaced)");
		}
	}
}
