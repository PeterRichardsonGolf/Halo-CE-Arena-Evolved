/*
AE_SCREEN_MAIN_MENU.C

AE's MAIN MENU and MULTIPLAYER (ae_screen_main_menu.h; the mockup's boards 5 and 6). Rows on the left (ae_setup_flow.h), on the right MY PARTY (0.4.2,
ae_widgets.h: "In Main Menu", player 1's row, the signed-in players', "START to add a player") and the last game; the
prompts. X (F) or a click on the party moves the focus into it: A on player 1's row opens PROFILES, on a signed-in
player's signs them out; B back to the rows. QUIT asks first. SPLIT SCREEN (0.3.9): START on another
controller (2-4) asks its player for a profile (ae_screen_lobby_profile.h: an inline card under player 1's, one for each
pad choosing, all at once) and signs them in with it here (a card under player 1's, its profile's name or a guest's), its
B signs them out; the next lobby takes them in after player 1.
MULTIPLAYER's CREATE GAME opens CREATE GAME (ae_screen_create_game.h) over it, JOIN WITH INVITE its field (ae_screen_join_invite.h), SERVER BROWSER and
LAN GAMES theirs (ae_screen_browser.h); the
main menu's SETTINGS, CAMPAIGN and PROFILES open theirs (ae_screen_settings.h, ...).

Its log: "ae main menu: open", "ae main menu: <item>", "ae main menu: quit", "ae main menu: player <n> signed in" /
"signed out".
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "game/game_engine.h"
#include "saved games/playlist_profile.h"
#include "interface/player_ui.h"
#include "../src/ae_draw.h"
#include "../src/ae_platform.h"
#include "ae_glue_lobby.h"
#include "ae_glue_profiles.h"
#include "ae_defaults_offer.h"
#include "ae_mcc_offer.h"
#include "ae_input.h"
#include "ae_main_menu_rules.h"
#include "ae_motion.h"
#include "ae_hooks.h"
#include "ae_screen_browser.h"
#include "ae_screen_campaign.h"
#include "ae_screen_create_game.h"
#include "ae_screen_join_address.h"
#include "ae_screen_join_invite.h"
#include "ae_screen_lobby_profile.h"
#include "ae_screen_main_menu.h"
#include "ae_screen_matchmaking.h"
#include "ae_screen_motd.h"
#include "ae_screen_party.h"
#include "ae_screen_tab_menu.h"
#include "ae_screen_profiles.h"
#include "ae_screen_settings.h"
#include "ae_settings_rows.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_stage_map.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "ae_scoreboard_header.h"
#include "ae_host_server.h"
#include "ae_screen_host_server.h"
#include "ae_server_ingame.h"
#include "halo_ui_map_list.h"

void platform_log(char const *format, ...);
void platform_request_quit(void);
const char *build_identity_version(void);
long config_integer(char const *name);
int config_write(const char *name, const char *value);
/* ui_widget_event_handler_functions.c (declared as menu_functions.c does) */
short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);
short ui_widget_port_gametypes(long *indices, short maximum, short *last_used);

enum
{
	MENU_HIT = 0x7B00,
	HIT_ROWS = MENU_HIT + 1, HIT_PROMPTS, HIT_CARDS, HIT_PARTY
};
enum { ROW_MULTIPLAYER = AE_FLOW_ROW_OWN, ROW_CAMPAIGN, ROW_SETTINGS, ROW_PROFILES, ROW_QUIT, ROW_CREATE, ROW_JOIN,
	ROW_BROWSER, ROW_LAN, ROW_HOST_SERVER, ROW_MOTD, ROW_WHATS_NEW, ROW_COMING_SOON, ROW_MATCHMAKING, ROW_ADDRESS };

/* (menus phase 1) the main menu's rows by ae_main_menu_rules.h's order: their kinds and descriptions (spec §5; copy in
one place for the owner to edit). COMING SOON's rows say only "Coming soon." */
static struct { short kind; const char *help; } const main_rows[AE_MAIN_ROWS] =
{
	{ ROW_CAMPAIGN, "The story: alone, split screen or online, up to 4." },
	{ ROW_MULTIPLAYER, "Find a game on the AE servers, the browser or LAN." },
	{ ROW_CREATE, "Your own game: map, game type, options, then host." },
	{ ROW_SETTINGS, "Video, audio, controls, network and menu settings." },
	{ ROW_MOTD, "The latest word from the Arena Evolved team." },
	{ ROW_WHATS_NEW, "What changed in each release, newest first." },
	{ ROW_COMING_SOON, "Coming soon." },
	{ ROW_COMING_SOON, "Coming soon." },
	{ ROW_COMING_SOON, "Coming soon." },
	{ ROW_COMING_SOON, "Coming soon." },
};
#define COMING_SOON_LABEL "COMING SOON"

/* a main row's place on the page: COMING SOON's label sits before its first row */
static short main_flow_index(int row)
{
	return (short)(row >= AE_MAIN_FIREFIGHT ? row + 1 : row);
}

static int main_row_of(short flow_index)
{
	return flow_index > AE_MAIN_FIREFIGHT ? flow_index - 1 : flow_index == AE_MAIN_FIREFIGHT ? -1 : flow_index;
}

static struct
{
	struct ae_flow_screen flow;
	int multiplayer;            /* the MULTIPLAYER page (its own screen over the main menu) */
	char player[48];
	unsigned int player_color;
	char last_game[96];
	/* (0.4.2) MY PARTY's rows as drawn (player 1's, then the signed-in controllers'), the focused one (-1: the rows') */
	short party[4], party_count, party_focus;
} main_page, multiplayer_page;

static struct ae_screen_class const menu_class;
static int my_server_card(struct ae_flow_screen *flow, float x, float width, float y, float bottom);
/* the local player whose A opened a row (a click: player 1) */
static short accept_player;
/* (split screen) the controllers 2-4 signed in here: their players join the next lobby with player 1 */
static int signed_in[4];

/* (menus phase 1, layout A, owner 2026-11-10) CAMPAIGN, MULTIPLAYER, HOST GAME, SETTINGS, MESSAGE OF THE DAY, WHAT'S
NEW, then COMING SOON's label over FIREFIGHT, FORGE, THEATER, FRIENDS (focusable, muted, they open nothing yet). PROFILES
moved to MY PARTY's player 1 and the TAB menu, QUIT to B */
static void rows_main(void)
{
	struct ae_flow_screen *flow = &main_page.flow;
	short count = 0;
	int row;

	for (row = 0; row < AE_MAIN_ROWS; row++)
	{
		if (row == AE_MAIN_FIREFIGHT)
			flow->rows[count++] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, COMING_SOON_LABEL, NULL, NULL, NULL };
		flow->rows[count] = (struct ae_flow_row){ main_rows[row].kind, -1, ae_main_row_label(row), NULL,
			main_rows[row].help, NULL };
		flow->rows[count].muted = (short)ae_main_row_coming_soon(row);
		/* (the badges: NEW on an unread message, N NEW on releases not opened yet) */
		if (row == AE_MAIN_MOTD)
			flow->rows[count].value = ae_screen_motd_badge();
		else if (row == AE_MAIN_WHATS_NEW)
			flow->rows[count].value = ae_screen_whats_new_badge();
		flow->rows[count].status = flow->rows[count].value != NULL;
		count++;
	}
	flow->count = count;
}

/* the open line's list of the rows, and where the focus starts: MULTIPLAYER on the first launch, else the row used
last (config menus.last_main_row) */
static void rows_main_open(void)
{
	char said[256];
	int row, last = (int)config_integer("menus.last_main_row"), first = ae_main_first_focus(last);
	short index;

	rows_main();
	said[0] = 0;
	for (index = 0; index < main_page.flow.count; index++)
		snprintf(said + strlen(said), sizeof(said) - strlen(said), "%s%s", index ? " | " : "",
			main_page.flow.rows[index].label);
	platform_log("ae main menu: rows: %s", said);
	main_page.flow.focus = main_flow_index(first);
	row = first;
	platform_log("ae main menu: focus %s (%s)", ae_main_row_label(row), last == first ? "last used" : "first launch");
	{
		const char *badge = ae_screen_whats_new_badge();

		platform_log("ae main menu: whats new badge %s", badge ? badge : "none");
	}
}

/* the row opened, kept for the next launch's focus (only its line of config.toml is written, and only when it changes) */
static void main_row_used(short flow_index)
{
	int row = main_row_of(flow_index);
	char value[8];

	if (row < 0 || row == (int)config_integer("menus.last_main_row"))
		return;
	snprintf(value, sizeof(value), "%d", row);
	config_write("menus.last_main_row", value);
}

/* (once a layout: the window's height, UI SCALE) the rows' bottom against the line they end above: "ok", or "overlap" */
static void layout_said(struct ae_flow_screen const *flow)
{
	static int said_height, said_scale;
	int height = flow->density.pixel > 0.0f ? (int)(1080.0f / flow->density.pixel + 0.5f) : 0;
	int scale = (int)(flow->density.unit * 100.0f + 0.5f);

	if (height == said_height && scale == said_scale)
		return;
	said_height = height;
	said_scale = scale;
	platform_log("ae main menu: layout %dp %d%%: rows end %.0f, limit %.0f %s", height, scale, flow->rows_end,
		flow->rows_limit, flow->rows_end <= flow->rows_limit + 0.5f ? "ok" : "overlap");
}

/* (menus phase 1, R2-03) MULTIPLAYER: SERVER BROWSER first, MATCHMAKING, JOIN BY ADDRESS, JOIN LAN GAMES, JOIN WITH
INVITE, HOST A SERVER (MY SERVER while this PC's runs); each row's description inside it. CREATE GAME left this page:
it is the main menu's HOST GAME */
static void rows_multiplayer(void)
{
	struct ae_flow_screen *flow = &multiplayer_page.flow;
	static char server_value[48];
	short count = 0;

	flow->inline_help = 1;
	flow->rows[count++] = (struct ae_flow_row){ ROW_BROWSER, -1, ae_mp_row_label(AE_MP_BROWSER), NULL,
		"Every game on the internet, refreshed in place.",
		ae_screen_browser_available(AE_BROWSER_INTERNET) ? NULL : "NOT IN THIS BUILD" };
	flow->rows[count++] = (struct ae_flow_row){ ROW_MATCHMAKING, -1, ae_mp_row_label(AE_MP_MATCHMAKING), NULL,
		"Pick a playlist, join an AE server with your party.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_ADDRESS, -1, ae_mp_row_label(AE_MP_JOIN_ADDRESS), NULL,
		"Type a host's address or pick a recent one.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_LAN, -1, ae_mp_row_label(AE_MP_LAN), NULL,
		"Games on your network, no internet needed.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_JOIN, -1, ae_mp_row_label(AE_MP_INVITE), NULL,
		"Paste the invite link or code a friend sent you.", NULL };
	/* (0.7.7 HOST A SERVER, owner pick 1 A: MY SERVER while one of this PC's runs, B-1) */
	if (ae_screen_host_server_row(server_value, (int)sizeof(server_value)))
		flow->rows[count++] = (struct ae_flow_row){ ROW_HOST_SERVER, -1, "MY SERVER", server_value,
			"Your server on this PC: join it, copy its invite, open its web page, or stop it.", NULL, .subscreen = 1 };
	else
		flow->rows[count++] = (struct ae_flow_row){ ROW_HOST_SERVER, -1, ae_mp_row_label(AE_MP_HOST_SERVER), NULL,
			"A game on this PC that stays up when you leave it.", NULL };
	flow->count = count;
}

/* player 1's card: the name and colour of the profile player 1 plays with, read every frame (PROFILES, a pick on
another screen or a renamed profile change it while the page stays open) */
static void player1_read(void)
{
	static struct ae_profile_summary profiles[AE_PROFILES_MAXIMUM];
	long active = player_ui_get_active_player_profile_index(0);
	short count = 0, index;

	snprintf(main_page.player, sizeof(main_page.player), "PLAYER 1");
	main_page.player_color = 0xC0392BFFu;
	if (!ae_profiles_list(profiles, AE_PROFILES_MAXIMUM, &count).ok)
		return;
	for (index = 0; index < count; index++)
	{
		if (profiles[index].index == active)
		{
			snprintf(main_page.player, sizeof(main_page.player), "%s", profiles[index].name);
			main_page.player_color = profiles[index].color;
		}
	}
}

/* player 1's profile (name and colour) and the last game (map and gametype), read as the page opens */
static void facts_read(void)
{
	short count = 0, last = 0;
	char const *const *names = NULL;
	long gametypes[AE_SAVED_GAMES_MAXIMUM];
	char map[64], gametype[16];
	wchar_t wide[64];

	/* (player 1 has no profile until a game, CAMPAIGN or SETTINGS picks one: the card named "PLAYER 1" at every start;
	the one they would pick, made player 1's now, as they do) */
	ae_profile_player1_ensure();
	player1_read();
	map[0] = gametype[0] = 0;
	count = ui_widget_port_multiplayer_maps(&names, &last);
	/* (the map CREATE GAME starts on: the one used last, else the first that has its file) */
	last = ae_stage_map_playable_index(names, count, last);
	if (last >= 0 && last < count)
	{
		int at;

		if (!ae_pack_map_caption(names[last], wide, 64))
			ui_map_list_caption(names[last], wide, 64);
		for (at = 0; at < 63 && wide[at]; at++)
			map[at] = wide[at] >= 0x20 && wide[at] < 0x7F ? (char)wide[at] : '?';
		map[at] = 0;
	}
	count = ui_widget_port_gametypes(gametypes, (short)NUMBEROF(gametypes), &last);
	if (last >= 0 && last < count)
	{
		struct game_variant variant;

		if (playlist_profile_get(gametypes[last], &variant))
		{
			int at;

			for (at = 0; at < 11 && variant.human_readable_game_description[at]; at++)
				gametype[at] = variant.human_readable_game_description[at] < 0x7F ?
					(char)variant.human_readable_game_description[at] : '?';
			gametype[at] = 0;
		}
	}
	snprintf(main_page.last_game, sizeof(main_page.last_game), "%s%s%s", map, map[0] && gametype[0] ? " \xE2\x80\xA2 " : "",
		gametype);
}

static int signed_in_count(void)
{
	int count = 0;
	short controller;

	for (controller = 1; controller < 4; controller++)
		count += signed_in[controller] != 0;
	return count;
}

/* a signed-in controller's name and colour: its profile's, or a guest's (the lobby makes it one) */
static void signed_in_profile(short controller, char *name, int name_size, unsigned int *color)
{
	static struct ae_profile_summary profiles[AE_PROFILES_MAXIMUM];
	short count = 0, index;
	long active = player_ui_get_active_player_profile_index(controller);

	snprintf(name, name_size, "Guest %d", controller + 1);
	*color = 0x7F8C8DFFu;
	if (!ae_profiles_list(profiles, AE_PROFILES_MAXIMUM, &count).ok)
		return;
	for (index = 0; active != NONE && index < count; index++)
	{
		if (profiles[index].index == active && active != player_ui_get_active_player_profile_index(0))
		{
			snprintf(name, name_size, "%s", profiles[index].name);
			*color = profiles[index].color;
		}
	}
}

/* (split screen) START on a controller 2-4 signs its player in, its B signs them out; 1: taken. A lone gamepad shares
controller 1 with the keyboard: once the keyboard or mouse has been used, that pad's START signs it in as player 2 (the
lobby's ADD PLAYER state moves it onto controller 2; its B there gives it back) */
static int lone_pad_signed_in;
/* the lone pad's START waits on its profile pick (on controller 2 already) */
static int lone_pad_picking;

/* (a seated guest's CHANGE PROFILE under way: a cancelled pick keeps the seat) */
static int changing_profile[4];

/* a START's profile pick (ae_screen_lobby_profile.h) is over: picked signs the player in with it, B leaves them out */
static void signed_in_profile_done(short player, int picked, void *context)
{
	int lone_pad = player == 1 && lone_pad_picking;

	(void)context;
	if (player >= 1 && player <= 3 && changing_profile[player])
	{
		changing_profile[player] = 0;
		if (!picked)
		{
			signed_in[player] = 1;
			platform_log("ae party: player %d kept their profile", player + 1);
			return;
		}
	}
	if (lone_pad)
		lone_pad_picking = 0;
	if (player < 1 || player > 3)
		return;
	if (!picked)
	{
		if (lone_pad)
			ae_lobby_adding_player(0);
		platform_log("ae main menu: player %d not signed in (profile pick cancelled)", player + 1);
		return;
	}
	signed_in[player] = 1;
	if (lone_pad)
		lone_pad_signed_in = 1;
	platform_log("ae main menu: player %d signed in (%s)", player + 1,
		lone_pad ? "the keyboard's lone pad, now controller 2" : "its own controller");
	{
		char name[48];
		unsigned int color;

		signed_in_profile(player, name, (int)sizeof(name), &color);
		platform_log("ae party: player %d joined (controller %d, profile %s)", player + 1, player + 1, name);
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* (0.4.3, owner 2026-10-28) a pad signs in only while MY PARTY has the focus, as CREATE GAME's lobby: elsewhere its
START is refused, the focused row's help saying where for a moment (menu_draw) */
#define JOIN_REFUSAL_MS 2500
static unsigned long join_refused_until;

static int join_refused(int join_here)
{
	if (join_here)
		return 0;
	platform_log("ae main menu: a pad's START outside MY PARTY: no sign in");
	join_refused_until = ae_motion_now() + JOIN_REFUSAL_MS;
	ae_sound_request(AE_SOUND_FAILURE, 0);
	return 1;
}

/* join_here: MY PARTY has the focus, where a new pad's START signs it in (join_refused) */

/* join_here: a new pad's START seats it here (where MY PARTY shows); lone_pad: the keyboard's lone pad's START joins
too (the PARTY manager: elsewhere player 1's START opens the TAB menu) */
static int signed_in_press(struct ae_flow_screen const *flow, struct ae_event const *event, int join_here, int lone_pad)
{
	short controller = ae_input_event_controller();

	/* (a pad choosing its profile: its card takes its events; the other pads and player 1's menu go on) */
	if (controller >= 1 && controller <= 3 && ae_screen_lobby_profile_event(controller, event))
		return 1;
	if (controller == 0 && event->action == AE_ACTION_START && !signed_in[1] && !lone_pad_picking &&
		ae_platform_keyboard_used() && ae_platform_pad_start_held())
	{
		if (!lone_pad)
			return 0;
		if (join_refused(join_here))
			return 1;
		ae_lobby_adding_player(1);
		lone_pad_picking = 1;
		if (!ae_screen_lobby_profile_open(1, flow, signed_in_profile_done, NULL))
		{
			lone_pad_picking = 0;
			ae_lobby_adding_player(0);
			ae_sound_request(AE_SOUND_FAILURE, 0);
		}
		return 1;
	}
	if (controller < 1 || controller > 3)
		return 0;
	if (event->action == AE_ACTION_START && !signed_in[controller])
	{
		if (join_refused(join_here))
			return 1;
		if (!ae_screen_lobby_profile_open(controller, flow, signed_in_profile_done, NULL))
			ae_sound_request(AE_SOUND_FAILURE, 0);
		return 1;
	}
	if (event->action == AE_ACTION_BACK && signed_in[controller])
	{
		/* (owner 11-11: B asks first, STAY focused: a stray press keeps the seat) */
		if (!ae_screen_main_menu_party_remove_ask(flow, controller, 1))
			ae_sound_request(AE_SOUND_FAILURE, 0);
		return 1;
	}
	/* (its other buttons do nothing here: the menu is player 1's) */
	return signed_in[controller] != 0;
}

short ae_screen_main_menu_signed_in_waiting(void)
{
	short controller;

	for (controller = 1; controller < 4; controller++)
		if (signed_in[controller] && !ae_lobby_controller_plays(controller))
			return controller;
	return NONE;
}

int ae_screen_main_menu_signed_in(short local_player)
{
	return local_player >= 1 && local_player < 4 && signed_in[local_player];
}

void ae_screen_main_menu_signed_in_cancel(short local_player)
{
	if (local_player < 1 || local_player >= 4)
		return;
	signed_in[local_player] = 0;
	if (local_player == 1 && lone_pad_signed_in)
	{
		ae_lobby_adding_player(0);
		lone_pad_signed_in = 0;
	}
	platform_log("ae main menu: player %d signed out (profile pick cancelled)", local_player + 1);
}

void ae_screen_main_menu_signed_in_joined(short local_player)
{
	if (local_player < 1 || local_player >= 4)
		return;
	signed_in[local_player] = 0;
	if (local_player == 1)
		lone_pad_signed_in = 0;
}

/* MY PARTY: player 1's row, the signed-in players' (their B signs them out), the pads choosing their profiles (their
inline cards, ae_screen_lobby_profile.h), "START to add a player"; the last game at the foot */
/* (menus phase 1) MY PARTY on another screen (MATCHMAKING's lobby): its own rows' state, its head line given */
static short other_party[4], other_party_count;
static const char *other_place;

static void draw_side(struct ae_flow_screen *flow)
{
	struct ae_density const *d = &flow->density;
	float x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X, width = ae_flow_right_x(flow) - x;
	float y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y), minor = ae_size_minor(d), body = ae_size_text(d);
	float bottom = ae_flow_content_bottom(flow), box, indent = ae_flow_u(flow, AE_PARTY_INDENT_U);
	float inset = ae_flow_u(flow, AE_FLOW_INSET_U), gap = ae_flow_u(flow, AE_FLOW_CARD_GAP_U);
	static char names[4][48], subs[4][48];
	char where[64], count[8];
	short controller, signed_count = (short)signed_in_count();
	struct ae_party_row row;

	/* (0.8.2: the PARTY manager open over this page draws MY PARTY itself: this one under it showed through) */
	if (ae_screen_party_covers(flow))
		return;
	x += inset;
	width -= 2.0f * inset;
	{
		const char *place = other_place ? other_place : "In Main Menu";

		snprintf(where, sizeof(where), signed_count ? "%s \xE2\x80\xA2 split screen on this PC" : "%s", place);
	}
	snprintf(count, sizeof(count), "%d/4", 1 + signed_count);
	y += ae_widget_party_head(d, x, y, width, where, count, HIT_PARTY);
	/* (the page's own party state: the main menu's or MULTIPLAYER's) */
	{
		short *party = other_place ? other_party : flow == &main_page.flow ? main_page.party : multiplayer_page.party;
		short *party_count = other_place ? &other_party_count : flow == &main_page.flow ? &main_page.party_count :
			&multiplayer_page.party_count;
		short focus = other_place ? -1 : flow == &main_page.flow ? main_page.party_focus : multiplayer_page.party_focus;

		*party_count = 0;
		player1_read();
		memset(&row, 0, sizeof(row));
		row.kind = AE_PARTY_PERSON;
		row.name = main_page.player;
		row.slot = 1;
		row.color = main_page.player_color;
		row.sub_line = ae_ui_last_device() == AE_DEVICE_KEYBOARD_MOUSE ? "Player 1 \xE2\x80\xA2 keyboard" :
			"Player 1 \xE2\x80\xA2 this PC";
		if (focus == 0)
			row.flags |= AE_CARD_FOCUSED;
		party[(*party_count)++] = 0;
		y += ae_widget_party_row(d, x, y, width, &row, HIT_PARTY, 0);
		for (controller = 1; controller < 4; controller++)
		{
			float height, hint;

			if (!signed_in[controller])
			{
				height = ae_screen_lobby_profile_card_draw(d, controller, x + indent, y, width - indent, HIT_CARDS);
				if (height > 0.0f)
					y += height + gap;
				continue;
			}
			memset(&row, 0, sizeof(row));
			row.kind = AE_PARTY_PERSON;
			signed_in_profile(controller, names[controller], sizeof(names[controller]), &row.color);
			snprintf(subs[controller], sizeof(subs[controller]), "Player %d \xE2\x80\xA2 Controller %d", controller + 1,
				controller + 1);
			row.name = names[controller];
			row.sub_line = subs[controller];
			row.slot = (short)(controller + 1);
			if (focus == *party_count)
				row.flags |= AE_CARD_FOCUSED;
			/* (its B signs it out: the hint on the row's right) */
			hint = ae_screen_lobby_profile_hint(d, AE_BUTTON_B, "Leave", 0.0f, 0.0f, 0);
			row.right_room = hint;
			height = ae_widget_party_row(d, x, y, width, &row, HIT_PARTY, *party_count);
			ae_screen_lobby_profile_hint(d, AE_BUTTON_B, "Leave", x + width - ae_flow_u(flow, 16.0f) - hint,
				y + ae_party_row_height(d, AE_PARTY_PERSON) * 0.5f, 1);
			party[(*party_count)++] = controller;
			y += height;
		}
	}
	if (signed_count + ae_screen_lobby_profile_choosing_count() < 3)
		y += ae_widget_open_card(d, x + indent, y, width - indent, 0, HIT_CARDS);
	/* (0.7.7, owner pick 2 B-2: a server of this PC running, the main menu's card is its, in LAST GAME's place) */
	if (!other_place && flow == &main_page.flow && my_server_card(flow, x - inset, width + 2.0f * inset, y, bottom))
		return;
	if (!main_page.last_game[0])
		return;
	x -= inset;
	width += 2.0f * inset;
	box = ae_flow_u(flow, 110.0f);
	if (bottom - box < y + ae_flow_u(flow, 120.0f))
		return;
	ae_draw_rect(x, bottom - box, width, box, ae_flow_u(flow, d->metrics->corner), AE_COLOR_PANEL);
	ae_draw_outline(x, bottom - box, width, box, ae_flow_u(flow, d->metrics->corner), ae_flow_u(flow, AE_FLOW_RULE_U),
		AE_COLOR_RULE);
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x + ae_flow_u(flow, AE_FLOW_INSET_U), bottom - box +
		ae_flow_u(flow, 22.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, "LAST GAME");
	ae_draw_text(AE_FONT_ROW, body, x + ae_flow_u(flow, AE_FLOW_INSET_U), bottom - box + ae_flow_u(flow, 56.0f),
		AE_ALIGN_LEFT, AE_COLOR_TITLE, main_page.last_game);
}

/* (0.7.7 B-2) MY SERVER's card: its name and state, its players and game, how long it has been up; Y joins it. 1 if
drawn */
static int my_server_card(struct ae_flow_screen *flow, float x, float width, float y, float bottom)
{
	struct ae_density const *d = &flow->density;
	struct ae_hd_status const *status = ae_host_server_status();
	struct ae_hd_running const *running = ae_host_server_running();
	float box = ae_flow_u(flow, 150.0f), top, inset = ae_flow_u(flow, AE_FLOW_INSET_U), minor = ae_size_minor(d);
	char line[160], map[48], gametype[48], up[32], fitted[160];
	int state = ae_host_server_state();

	if (!running || (state != AE_HD_READY && state != AE_HD_STARTING))
		return 0;
	if (bottom - box < y + ae_flow_u(flow, 120.0f))
		return 1;
	top = bottom - box;
	ae_draw_rect(x, top, width, box, ae_flow_u(flow, d->metrics->corner), AE_COLOR_PANEL);
	ae_draw_rect(x, top, ae_flow_u(flow, 4.0f), box, 0.0f, 0x6FD08CFFu);
	ae_draw_outline(x, top, width, box, ae_flow_u(flow, d->metrics->corner), ae_flow_u(flow, AE_FLOW_RULE_U),
		AE_COLOR_RULE);
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x + inset, top + ae_flow_u(flow, 22.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
		"MY SERVER");
	ae_draw_text(AE_FONT_ROW, minor, x + width - inset, top + ae_flow_u(flow, 22.0f), AE_ALIGN_RIGHT,
		state == AE_HD_READY ? 0x6FD08CFFu : AE_COLOR_ACCENT, state == AE_HD_READY ? "RUNNING" : "STARTING");
	ae_draw_text(AE_FONT_ROW, ae_size_text(d), x + inset, top + ae_flow_u(flow, 56.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE,
		status && status->name[0] ? status->name : running->name);
	if (status)
	{
		ae_server_ingame_map_caption(status->map, map, (int)sizeof(map));
		ae_ingame_gametype_word(status->gametype, gametype, (int)sizeof(gametype));
		ae_ingame_up_text((unsigned int)(status->uptime / 60), up, (int)sizeof(up));
		snprintf(line, sizeof(line), "%d of %d players  \xE2\x80\xA2  %s, %s  \xE2\x80\xA2  up %s", status->players,
			status->max_players, map, gametype, up);
		ae_fit_text(AE_FONT_BODY, minor, line, width - 2.0f * inset, fitted, sizeof(fitted));
		ae_draw_text(AE_FONT_BODY, minor, x + inset, top + ae_flow_u(flow, 92.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT, fitted);
	}
	ae_draw_text(AE_FONT_BODY, minor * 0.92f, x + inset, top + ae_flow_u(flow, 122.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
		"Y joins it. MULTIPLAYER > MY SERVER stops it.");
	return 1;
}

/* a party row taken: player 1's opens PROFILES, a signed-in player's signs them out */
static void party_accept(short controller)
{
	if (controller == 0)
	{
		platform_log("ae main menu: party: profiles");
		if (ae_screen_profiles_open())
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	}
	if (controller < 1 || controller > 3 || !signed_in[controller])
		return;
	signed_in[controller] = 0;
	if (controller == 1 && lone_pad_signed_in)
	{
		ae_lobby_adding_player(0);
		lone_pad_signed_in = 0;
	}
	platform_log("ae main menu: player %d signed out", controller + 1);
	ae_sound_request(AE_SOUND_BACK, 0);
}

static void quit_picked(short choice, void *context)
{
	(void)context;
	if (choice != 1)
	{
		platform_log("ae main menu: quit cancelled");
		return;
	}
	platform_log("ae main menu: quit");
	platform_request_quit();
}

/* (0.7.7, owner pick 3: C-1 / C-2) quitting with this PC's server running: KEEP RUNNING (focused: quitting never ends
other people's game by accident), STOP WHEN EMPTY (only with people on it), STOP NOW; B stays in the menu */
static int quit_server_choices[3], quit_server_count;

static void quit_server_picked(short choice, void *context)
{
	(void)context;
	if (choice < 0 || choice >= quit_server_count)
		return;
	switch (quit_server_choices[choice])
	{
	case AE_HD_QUIT_STOP_EMPTY:
		ae_host_server_stop(1);
		break;
	case AE_HD_QUIT_STOP:
		ae_host_server_stop(0);
		break;
	default:
		break;
	}
	platform_log("ae main menu: quit (the server: %s)", ae_host_dedicated_quit_label(quit_server_choices[choice]));
	platform_request_quit();
}

static int quit_server_ask(struct ae_flow_screen *flow)
{
	struct ae_dialog_spec spec;
	struct ae_hd_status const *status = ae_host_server_status();
	struct ae_hd_running const *running = ae_host_server_running();
	static char body[240];
	int people = status ? status->players : 0, index;

	if (!running || (ae_host_server_state() != AE_HD_READY && ae_host_server_state() != AE_HD_STARTING))
		return 0;
	quit_server_count = ae_host_dedicated_quit_choices(people > 0, quit_server_choices);
	if (people > 0)
		snprintf(body, sizeof(body), "%s runs on this PC with no window. %d player%s on it now: stopping it ends their "
			"game. STOP WHEN EMPTY: it closes itself when the last player leaves.", running->name, people,
			people == 1 ? " is" : "s are");
	else
		snprintf(body, sizeof(body), "%s runs on this PC with no window. Nobody is on it now; it keeps running for "
			"anyone who joins until you stop it.", running->name);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "Your server is running";
	spec.body = body;
	for (index = 0; index < quit_server_count; index++)
		spec.choices[index] = ae_host_dedicated_quit_label(quit_server_choices[index]);
	spec.choice_count = (short)quit_server_count;
	spec.safe_choice = 0;
	/* (B: back to the menu, nothing done) */
	spec.cancel_choice = -1;
	spec.has_cancel = 1;
	spec.destructive_choices = 1u << (quit_server_count - 1);
	spec.density = flow->density;
	spec.bounds = flow->frame.rect;
	spec.picked = quit_server_picked;
	return ae_dialog_open(&spec, AE_OWNER_ANY);
}

static void quit_ask(struct ae_flow_screen *flow)
{
	struct ae_dialog_spec spec;

	if (quit_server_ask(flow))
		return;

	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "Quit Arena Evolved?";
	spec.body = "The game closes.";
	spec.choices[0] = ae_string(AE_STR_CANCEL_CAPS);
	spec.choices[1] = "QUIT";
	spec.choice_count = 2;
	spec.has_cancel = 1;
	spec.density = flow->density;
	spec.bounds = flow->frame.rect;
	spec.picked = quit_picked;
	ae_dialog_open(&spec, AE_OWNER_ANY);
}

static int multiplayer_push(void);

static void accept(int multiplayer, short row)
{
	struct ae_flow_screen *flow = multiplayer ? &multiplayer_page.flow : &main_page.flow;
	struct ae_flow_row const *r;

	if (row < 0 || row >= flow->count)
		return;
	r = &flow->rows[row];
	if (r->kind == AE_FLOW_ROW_HEADER)
		return;
	if (r->muted)
	{
		platform_log("ae main menu: %s: coming soon", r->label);
		ae_flow_nudge(flow, row);
		return;
	}
	platform_log("ae main menu: %s", r->label);
	if (r->reason)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (!multiplayer)
		main_row_used(row);
	switch (r->kind)
	{
	case ROW_MULTIPLAYER:
		if (multiplayer_push())
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case ROW_QUIT:
		quit_ask(flow);
		break;
	case ROW_CAMPAIGN:
		if (ae_screen_campaign_open())
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case ROW_SETTINGS:
		/* (for the player whose pad or keyboard pressed A) */
		if (ae_screen_settings_open(accept_player))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case ROW_PROFILES:
		if (ae_screen_profiles_open())
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case ROW_BROWSER:
	case ROW_LAN:
		if (ae_screen_browser_open(r->kind == ROW_LAN ? AE_BROWSER_LAN : AE_BROWSER_INTERNET))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case ROW_JOIN:
		if (ae_screen_join_invite_open("MULTIPLAYER"))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case ROW_MOTD:
		if (ae_screen_motd_open(1))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case ROW_WHATS_NEW:
		if (ae_screen_whats_new_open(build_identity_version()))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case ROW_MATCHMAKING:
		if (ae_screen_matchmaking_open("MULTIPLAYER"))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case ROW_ADDRESS:
		if (ae_screen_join_address_open("MULTIPLAYER"))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case ROW_HOST_SERVER:
		if (ae_screen_host_server_open("MULTIPLAYER"))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case ROW_CREATE:
		if (ae_screen_create_game_from_menu())
			ae_sound_request(AE_SOUND_FORWARD, 0);
		else
			ae_flow_error(flow, "No gametype", "There is no gametype to start a game with.");
		break;
	default:
		break;
	}
}

static void menu_draw(struct ae_screen *instance)
{
	int multiplayer = instance->data == &multiplayer_page;
	struct ae_flow_screen *flow = multiplayer ? &multiplayer_page.flow : &main_page.flow;

	ae_flow_full_density(flow);
	if (multiplayer)
		rows_multiplayer();
	else
		rows_main();
	ae_flow_title(flow, multiplayer ? "MULTIPLAYER" : "ARENA EVOLVED", multiplayer ? "MAIN MENU" : NULL);
	/* (a header is never the focus: the first row after it) */
	if (flow->focus >= 0 && flow->focus < flow->count && flow->rows[flow->focus].kind == AE_FLOW_ROW_HEADER)
		flow->focus++;
	{
		/* (the party has the focus: no row on the left lit) */
		short focus = flow->focus, party_focus = multiplayer ? multiplayer_page.party_focus : main_page.party_focus;

		if (party_focus >= 0)
			flow->focus = -1;
		else if (focus >= 0 && focus < flow->count && join_refused_until &&
			(long)(join_refused_until - ae_motion_now()) > 0)
			flow->rows[focus].help = "Open MY PARTY to add a player.";
		ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
		if (!multiplayer)
			layout_said(flow);
		flow->focus = focus;
		draw_side(flow);
		flow->prompt_count = 0;
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
		if (multiplayer || party_focus >= 0)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
		else
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Quit", AE_ACTION_BACK);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, party_focus >= 0 ? "Menu" : "Party",
			AE_ACTION_X);
		/* (0.7.7 B-2: this PC's server running, Y joins it from the main menu (its card says so); else (menus phase 1)
		Y opens the network's settings) */
		if (!multiplayer && ae_host_server_state() == AE_HD_READY)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Join my server", AE_ACTION_Y);
		else if (!multiplayer && party_focus < 0)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Network", AE_ACTION_Y);
		if (!multiplayer && party_focus < 0 && flow->prompt_count < AE_FLOW_PROMPTS)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_START, "Menu", AE_ACTION_START);
	}
	/* (0.8.2: under the PARTY manager its footer is the only one: two rows of prompts drew over each other) */
	if (!ae_screen_party_covers(flow))
		ae_flow_draw_prompts(flow);
}

static int menu_handle(struct ae_screen *instance, struct ae_event const *event)
{
	int multiplayer = instance->data == &multiplayer_page;
	struct ae_flow_screen *flow = multiplayer ? &multiplayer_page.flow : &main_page.flow;
	short *focus = multiplayer ? &multiplayer_page.party_focus : &main_page.party_focus;
	short *party = multiplayer ? multiplayer_page.party : main_page.party;
	short count = multiplayer ? multiplayer_page.party_count : main_page.party_count;

	/* (menus phase 1, owner Q11: a pad's START seats it inline in MY PARTY, which every page here shows) */
	if (signed_in_press(flow, event, 1, 0))
		return 1;
	if (*focus >= count)
		*focus = (short)(count - 1);
	/* (0.4.2) MY PARTY: X (F) in and out; B back to the rows */
	/* (0.7.7 B-2: Y joins this PC's running server, from the main menu's MY SERVER card) */
	if (event->action == AE_ACTION_Y && !multiplayer && ae_host_server_state() == AE_HD_READY)
	{
		platform_log("ae main menu: Y: join my server");
		ae_sound_request(ae_host_server_join() ? AE_SOUND_FORWARD : AE_SOUND_FAILURE, 0);
		return 1;
	}
	if (event->action == AE_ACTION_Y && !multiplayer && *focus < 0)
	{
		platform_log("ae main menu: Y: network");
		if (ae_screen_settings_open_group(AE_SETTINGS_ONLINE, 0))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return 1;
	}
	/* (menus phase 1, spec §8) X (F) opens the PARTY manager over the page */
	if (event->action == AE_ACTION_X)
	{
		*focus = -1;
		if (ae_screen_party_open(flow))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return 1;
	}
	if (*focus >= 0)
	{
		switch (event->action)
		{
		case AE_ACTION_BACK:
			*focus = -1;
			ae_sound_request(AE_SOUND_BACK, 0);
			return 1;
		case AE_ACTION_UP:
		case AE_ACTION_DOWN:
			{
				short next = (short)(*focus + (event->action == AE_ACTION_UP ? -1 : 1));

				if (next >= 0 && next < count)
				{
					*focus = next;
					ae_sound_request(AE_SOUND_CURSOR, event->repeat);
				}
			}
			return 1;
		case AE_ACTION_ACCEPT:
			party_accept(party[*focus]);
			if (*focus >= count - 1 && party[*focus] != 0)
				*focus = (short)(*focus - 1);
			return 1;
		default:
			return 1;
		}
	}
	/* (menus phase 1) START (player 1) or the keyboard's Tab: the TAB menu over the main menu */
	if (!multiplayer && (event->action == AE_ACTION_START || ((event->action == AE_ACTION_TAB_NEXT ||
		event->action == AE_ACTION_TAB_PREVIOUS) && event->device == AE_DEVICE_KEYBOARD_MOUSE)))
	{
		if (ae_screen_tab_menu_open(flow, event->player >= 0 && event->player < 4 ? event->player : 0))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		/* (menus phase 1, owner Q3) B on the main menu asks before quitting */
		if (multiplayer)
			return 0;
		platform_log("ae main menu: B: quit?");
		quit_ask(flow);
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		return 1;
	case AE_ACTION_ACCEPT:
		accept_player = event->player >= 0 && event->player < 4 ? event->player : 0;
		accept(multiplayer, flow->focus);
		return 1;
	default:
		return 1;
	}
}

static void menu_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	int multiplayer = instance->data == &multiplayer_page;
	struct ae_flow_screen *flow = multiplayer ? &multiplayer_page.flow : &main_page.flow;
	struct ae_event event;
	struct ae_hit hit;
	short row;

	/* (a card's profile row: hover focuses, a click picks) */
	if (ae_screen_lobby_profile_choosing_count() && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_CARDS &&
		hit.part == AE_PART_ITEM)
	{
		ae_screen_lobby_profile_pointer(hit.index, pointer->moved, pointer->left_clicks);
		return;
	}
	/* (0.4.2) MY PARTY's rows: hover focuses them, a click takes one (the head: the party's focus) */
	if (ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_PARTY)
	{
		short *focus = multiplayer ? &multiplayer_page.party_focus : &main_page.party_focus;
		short *party = multiplayer ? multiplayer_page.party : main_page.party;
		short count = multiplayer ? multiplayer_page.party_count : main_page.party_count;

		(void)focus;
		(void)party;
		(void)count;
		/* (menus phase 1: a click anywhere on MY PARTY opens the PARTY manager) */
		if (pointer->left_clicks && ae_screen_party_open(flow))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	}
	if (pointer->right_clicks && !multiplayer)
	{
		quit_ask(flow);
		return;
	}
	if (pointer->moved && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_ROWS)
		*(multiplayer ? &multiplayer_page.party_focus : &main_page.party_focus) = -1;
	if (ae_prompts_pointer(flow->prompts, flow->prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK && multiplayer && multiplayer_page.party_focus < 0)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			menu_handle(instance, &event);
		return;
	}
	row = ae_flow_rows_pointer(flow, pointer);
	if (row >= 0)
	{
		accept_player = pointer->player >= 0 && pointer->player < 4 ? pointer->player : 0;
		accept(multiplayer, row);
	}
}

/* (menus phase 1) the launch's MESSAGE OF THE DAY: asked for, then opened first when due (at most 4 s) */
static void menu_update(struct ae_screen *instance)
{
	if (instance->data == &main_page)
		ae_screen_motd_launch_poll();
	ae_screen_main_menu_party_update();
}

static void menu_enter(struct ae_screen *instance)
{
	/* (the page's facts again: a game may have changed the last map or gametype) */
	if (instance->data == &main_page)
		facts_read();
}

static struct ae_screen_class const menu_class =
{
	.name = "main menu", .enter = menu_enter, .handle = menu_handle, .draw = menu_draw, .pointer = menu_pointer,
	.update = menu_update,
};

static int multiplayer_push(void)
{
	char said[256];
	short index;

	memset(&multiplayer_page.flow, 0, sizeof(multiplayer_page.flow));
	rows_multiplayer();
	said[0] = 0;
	for (index = 0; index < multiplayer_page.flow.count; index++)
		snprintf(said + strlen(said), sizeof(said) - strlen(said), "%s%s", index ? " | " : "",
			multiplayer_page.flow.rows[index].label);
	platform_log("ae main menu: multiplayer rows: %s", said);
	multiplayer_page.party_focus = -1;
	multiplayer_page.flow.hit_rows = HIT_ROWS;
	multiplayer_page.flow.hit_prompts = HIT_PROMPTS;
	multiplayer_page.multiplayer = 1;
	return ae_ui_push(&menu_class, AE_OWNER_ANY, &multiplayer_page);
}

/* ---------- (menus phase 1) the PARTY manager's party (ae_screen_party.c) */

int ae_screen_main_menu_party_count(void)
{
	return 1 + signed_in_count();
}

int ae_screen_main_menu_party_seat(short controller, char *name, int name_size, char *sub, int sub_size,
	unsigned int *color)
{
	/* (no buffers: whether the seat is taken) */
	if (!name || !sub || !color)
		return controller == 0 || (controller >= 1 && controller <= 3 && signed_in[controller]);
	if (controller == 0)
	{
		player1_read();
		snprintf(name, (size_t)name_size, "%s", main_page.player);
		*color = main_page.player_color;
		snprintf(sub, (size_t)sub_size, ae_ui_last_device() == AE_DEVICE_KEYBOARD_MOUSE ? "Player 1 \xE2\x80\xA2 keyboard" :
			"Player 1 \xE2\x80\xA2 this PC");
		return 1;
	}
	if (controller < 1 || controller > 3 || !signed_in[controller])
		return 0;
	signed_in_profile(controller, name, name_size, color);
	snprintf(sub, (size_t)sub_size, "Player %d \xE2\x80\xA2 controller %d", controller + 1, controller + 1);
	return 1;
}

void ae_screen_main_menu_party_remove(short controller)
{
	char name[48];
	unsigned int color;

	if (controller < 1 || controller > 3 || !signed_in[controller])
		return;
	signed_in_profile(controller, name, (int)sizeof(name), &color);
	signed_in[controller] = 0;
	if (controller == 1 && lone_pad_signed_in)
	{
		ae_lobby_adding_player(0);
		lone_pad_signed_in = 0;
	}
	platform_log("ae party: %s left (controller %d)", name, controller + 1);
	ae_sound_request(AE_SOUND_BACK, 0);
}

static void party_remove_picked(short choice, void *context)
{
	short controller = (short)(intptr_t)context;

	if (choice != 1)
	{
		platform_log("ae party: controller %d stays", controller + 1);
		return;
	}
	ae_screen_main_menu_party_remove(controller);
}

int ae_screen_main_menu_party_remove_ask(struct ae_flow_screen const *flow, short controller, int own)
{
	static char title[80], body[160];
	struct ae_dialog_spec spec;
	char name[48];
	unsigned int color;

	if (!flow || controller < 1 || controller > 3 || !signed_in[controller])
		return 0;
	signed_in_profile(controller, name, (int)sizeof(name), &color);
	if (own)
	{
		snprintf(title, sizeof(title), "%s: leave?", name);
		snprintf(body, sizeof(body), "Controller %d leaves this PC's party. START on it joins again.", controller + 1);
	}
	else
	{
		snprintf(title, sizeof(title), "Remove %s?", name);
		snprintf(body, sizeof(body), "Controller %d leaves this PC's party. START on it joins again.", controller + 1);
	}
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = title;
	spec.body = body;
	spec.choices[0] = own ? "STAY" : ae_string(AE_STR_CANCEL_CAPS);
	spec.choices[1] = own ? "LEAVE" : "REMOVE";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.destructive_choices = 1u << 1;
	spec.density = flow->density;
	spec.bounds = flow->frame.rect;
	spec.picked = party_remove_picked;
	spec.context = (void *)(intptr_t)controller;
	platform_log("ae party: %s? (controller %d)", own ? "leave" : "remove", controller + 1);
	return ae_dialog_open(&spec, AE_OWNER_ANY);
}

int ae_screen_main_menu_party_change(struct ae_flow_screen const *flow, short controller)
{
	if (controller == 0)
		return ae_screen_profiles_open();
	if (controller < 1 || controller > 3 || !signed_in[controller])
		return 0;
	/* (a guest's new profile: its card's pick again; B there keeps the seat as it was) */
	signed_in[controller] = 0;
	changing_profile[controller] = 1;
	if (ae_screen_lobby_profile_open(controller, flow, signed_in_profile_done, NULL))
		return 1;
	changing_profile[controller] = 0;
	signed_in[controller] = 1;
	return 0;
}

int ae_screen_main_menu_party_press_here(struct ae_flow_screen const *flow, struct ae_event const *event)
{
	return signed_in_press(flow, event, 1, 1);
}

int ae_screen_main_menu_party_pointer(struct ae_flow_screen const *flow, struct ae_pointer const *pointer)
{
	struct ae_hit hit;

	if (!ae_hit_at(pointer->x, pointer->y, 0, &hit))
		return 0;
	if (hit.id == HIT_CARDS && hit.part == AE_PART_ITEM && ae_screen_lobby_profile_choosing_count())
	{
		ae_screen_lobby_profile_pointer(hit.index, pointer->moved, pointer->left_clicks);
		return 1;
	}
	if (hit.id == HIT_PARTY)
	{
		if (pointer->left_clicks && ae_screen_party_open(flow))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return 1;
	}
	return 0;
}

void ae_screen_main_menu_party_update(void)
{
	/* (0.8.1: a guest's B asks STAY / LEAVE at once; the B hold this timed is gone. Kept for its callers: the party's
	per-frame work, none today) */
}

int ae_screen_main_menu_party_fill(int count)
{
	short controller;

	if (count < 1 || count > 4)
		return 0;
	for (controller = 1; controller < 4; controller++)
		signed_in[controller] = controller < count;
	if (lone_pad_signed_in)
	{
		ae_lobby_adding_player(0);
		lone_pad_signed_in = 0;
	}
	platform_log("ae party: filled to %d (test guests on controllers 2-%d)", count, count > 1 ? count : 1);
	return 1;
}

int ae_screen_main_menu_tab_opens(void)
{
	struct ae_screen *top = ae_ui_top();

	return top && (top->data == &main_page || ae_screen_tab_menu_is(top->data));
}

int ae_screen_main_menu_quit_ask(void)
{
	if (!ae_ui_holds(&main_page))
		return 0;
	quit_ask(&main_page.flow);
	return 1;
}

void ae_screen_main_menu_party_draw(struct ae_flow_screen *flow, const char *place)
{
	other_place = place && *place ? place : "In Matchmaking";
	draw_side(flow);
	other_place = NULL;
}

int ae_screen_main_menu_party_press(struct ae_flow_screen const *flow, struct ae_event const *event)
{
	/* (MY PARTY shows on that screen too: a pad's START seats it inline) */
	return signed_in_press(flow, event, 1, 0);
}

int ae_screen_main_menu_open(void)
{
	if (ae_ui_depth())
		return 0;
	memset(&main_page.flow, 0, sizeof(main_page.flow));
	main_page.party_focus = -1;
	main_page.flow.hit_rows = HIT_ROWS;
	main_page.flow.hit_prompts = HIT_PROMPTS;
	if (!ae_ui_push(&menu_class, AE_OWNER_ANY, &main_page))
		return 0;
	platform_log("ae main menu: open");
	rows_main_open();
	/* (menus phase 1 test hook, as DEV MODE's lobby_fill: the party at that size) */
	{
		char const *fill = getenv("HALO_AE_TEST_PARTY");

		if (fill && *fill)
			ae_screen_main_menu_party_fill(atoi(fill));
	}
	ae_ui_replace_menus();
	(void)ae_settings_ui_scale();
	/* (0.7.8: a config.toml that has the old defaults is asked once about the new ones, over the main menu) */
	if (!main_page.flow.density.metrics)
		ae_flow_full_density(&main_page.flow);
	/* (0.7.9: then MCC FOUND / MCC NOT FOUND, once: after NEW SETTINGS when that one opened) */
	if (!ae_defaults_offer_open(&main_page.flow.density))
		ae_mcc_offer_open(&main_page.flow.density);
	return 1;
}

void ae_screen_main_menu_close(void)
{
	int closed = ae_ui_remove(&multiplayer_page);

	/* (0.7.8: the join screens too, wherever they are: a join into a game in progress left SERVER BROWSER, JOIN BY
	ADDRESS or the invite's screen drawn over the game until B; each logs its own close) */
	ae_screen_browser_close();
	ae_screen_matchmaking_close();
	ae_screen_motd_close();
	ae_screen_party_close();
	ae_screen_tab_menu_close();
	ae_screen_join_address_close();
	ae_screen_join_invite_close();
	if (ae_ui_remove(&main_page) || closed)
		platform_log("ae main menu: closed (the game left its main menu)");
}
