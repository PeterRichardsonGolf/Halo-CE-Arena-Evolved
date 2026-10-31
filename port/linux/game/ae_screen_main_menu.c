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

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game_engine.h"
#include "saved games/playlist_profile.h"
#include "interface/player_ui.h"
#include "../src/ae_draw.h"
#include "../src/ae_platform.h"
#include "ae_glue_lobby.h"
#include "ae_glue_profiles.h"
#include "ae_input.h"
#include "ae_motion.h"
#include "ae_hooks.h"
#include "ae_screen_browser.h"
#include "ae_screen_campaign.h"
#include "ae_screen_create_game.h"
#include "ae_screen_join_invite.h"
#include "ae_screen_lobby_profile.h"
#include "ae_screen_main_menu.h"
#include "ae_screen_profiles.h"
#include "ae_screen_settings.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_stage_map.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "ae_scoreboard_header.h"
#include "halo_ui_map_list.h"

void platform_log(char const *format, ...);
void platform_request_quit(void);
/* ui_widget_event_handler_functions.c (declared as menu_functions.c does) */
short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);
short ui_widget_port_gametypes(long *indices, short maximum, short *last_used);

enum
{
	MENU_HIT = 0x7B00,
	HIT_ROWS = MENU_HIT + 1, HIT_PROMPTS, HIT_CARDS, HIT_PARTY
};
enum { ROW_MULTIPLAYER = AE_FLOW_ROW_OWN, ROW_CAMPAIGN, ROW_SETTINGS, ROW_PROFILES, ROW_QUIT, ROW_CREATE, ROW_JOIN,
	ROW_BROWSER, ROW_LAN };

#define NOT_YET "COMING SOON"

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
/* the local player whose A opened a row (a click: player 1) */
static short accept_player;
/* (split screen) the controllers 2-4 signed in here: their players join the next lobby with player 1 */
static int signed_in[4];

static void rows_main(void)
{
	struct ae_flow_screen *flow = &main_page.flow;
	short count = 0;

	flow->rows[count++] = (struct ae_flow_row){ ROW_MULTIPLAYER, -1, "MULTIPLAYER", NULL,
		"Create a game, join one with an invite, or find one in the server browser.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_CAMPAIGN, -1, "CAMPAIGN", NULL,
		"Halo's campaign: the levels you have reached, your saved game, a difficulty.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_SETTINGS, -1, "SETTINGS", NULL,
		"Controls, video, view, audio, HUD, chat, online, accessibility and system.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_PROFILES, -1, "PROFILES", NULL,
		"Your profiles: names, colours and controls, and who plays as player 1.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_QUIT, -1, "QUIT", NULL, "Leaves the game.", NULL };
	flow->count = count;
}

static void rows_multiplayer(void)
{
	struct ae_flow_screen *flow = &multiplayer_page.flow;
	short count = 0;

	flow->rows[count++] = (struct ae_flow_row){ ROW_CREATE, -1, "CREATE GAME", NULL,
		"Opens your lobby at once, friends join while you pick the map, the gametype and its rules.",
		NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_JOIN, -1, "JOIN WITH INVITE", NULL,
		"Paste or type the invite link or code a friend sent you, and join their game.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_BROWSER, -1, "SERVER BROWSER", NULL,
		"The internet's games, refreshed in place.",
		ae_screen_browser_available(AE_BROWSER_INTERNET) ? NULL : "NOT IN THIS BUILD" };
	flow->rows[count++] = (struct ae_flow_row){ ROW_LAN, -1, "JOIN LAN GAMES", NULL,
		"Games on your network, no internet needed.", NULL };
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

/* a START's profile pick (ae_screen_lobby_profile.h) is over: picked signs the player in with it, B leaves them out */
static void signed_in_profile_done(short player, int picked, void *context)
{
	int lone_pad = player == 1 && lone_pad_picking;

	(void)context;
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
static int signed_in_press(struct ae_flow_screen const *flow, struct ae_event const *event, int join_here)
{
	short controller = ae_input_event_controller();

	/* (a pad choosing its profile: its card takes its events; the other pads and player 1's menu go on) */
	if (controller >= 1 && controller <= 3 && ae_screen_lobby_profile_event(controller, event))
		return 1;
	if (controller == 0 && event->action == AE_ACTION_START && !signed_in[1] && !lone_pad_picking &&
		ae_platform_keyboard_used() && ae_platform_pad_start_held())
	{
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
		signed_in[controller] = 0;
		if (controller == 1 && lone_pad_signed_in)
		{
			ae_lobby_adding_player(0);
			lone_pad_signed_in = 0;
		}
		platform_log("ae main menu: player %d signed out", controller + 1);
		ae_sound_request(AE_SOUND_BACK, 0);
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

	x += inset;
	width -= 2.0f * inset;
	snprintf(where, sizeof(where), signed_count ? "In Main Menu \xE2\x80\xA2 split screen on this PC" : "In Main Menu");
	snprintf(count, sizeof(count), "%d/4", 1 + signed_count);
	y += ae_widget_party_head(d, x, y, width, where, count, HIT_PARTY);
	/* (the page's own party state: the main menu's or MULTIPLAYER's) */
	{
		short *party = flow == &main_page.flow ? main_page.party : multiplayer_page.party;
		short *party_count = flow == &main_page.flow ? &main_page.party_count : &multiplayer_page.party_count;
		short focus = flow == &main_page.flow ? main_page.party_focus : multiplayer_page.party_focus;

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
		return;
	platform_log("ae main menu: quit");
	platform_request_quit();
}

static void quit_ask(struct ae_flow_screen *flow)
{
	struct ae_dialog_spec spec;

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
	platform_log("ae main menu: %s", r->label);
	if (r->reason)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
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
	{
		/* (the party has the focus: no row on the left lit) */
		short focus = flow->focus, party_focus = multiplayer ? multiplayer_page.party_focus : main_page.party_focus;

		if (party_focus >= 0)
			flow->focus = -1;
		else if (focus >= 0 && focus < flow->count && join_refused_until &&
			(long)(join_refused_until - ae_motion_now()) > 0)
			flow->rows[focus].help = "Open MY PARTY to add a player.";
		ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
		flow->focus = focus;
		draw_side(flow);
		flow->prompt_count = 0;
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
		if (multiplayer || party_focus >= 0)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, party_focus >= 0 ? "Menu" : "Party",
			AE_ACTION_X);
	}
	ae_flow_draw_prompts(flow);
}

static int menu_handle(struct ae_screen *instance, struct ae_event const *event)
{
	int multiplayer = instance->data == &multiplayer_page;
	struct ae_flow_screen *flow = multiplayer ? &multiplayer_page.flow : &main_page.flow;
	short *focus = multiplayer ? &multiplayer_page.party_focus : &main_page.party_focus;
	short *party = multiplayer ? multiplayer_page.party : main_page.party;
	short count = multiplayer ? multiplayer_page.party_count : main_page.party_count;

	if (signed_in_press(flow, event, *focus >= 0))
		return 1;
	if (*focus >= count)
		*focus = (short)(count - 1);
	/* (0.4.2) MY PARTY: X (F) in and out; B back to the rows */
	if (event->action == AE_ACTION_X)
	{
		*focus = *focus >= 0 ? -1 : 0;
		ae_sound_request(AE_SOUND_CURSOR, 0);
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
	switch (event->action)
	{
	case AE_ACTION_BACK:
		/* (the main menu stays: B there does nothing, QUIT is a row) */
		return multiplayer ? 0 : 1;
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

		if (hit.index >= 0 && hit.index < count && (pointer->moved || pointer->left_clicks))
			*focus = hit.index;
		if (pointer->left_clicks && hit.index >= 0 && hit.index < count)
			party_accept(party[hit.index]);
		else if (pointer->left_clicks && *focus < 0)
			*focus = 0;
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

static void menu_enter(struct ae_screen *instance)
{
	/* (the page's facts again: a game may have changed the last map or gametype) */
	if (instance->data == &main_page)
		facts_read();
}

static struct ae_screen_class const menu_class =
{
	.name = "main menu", .enter = menu_enter, .handle = menu_handle, .draw = menu_draw, .pointer = menu_pointer,
};

static int multiplayer_push(void)
{
	memset(&multiplayer_page.flow, 0, sizeof(multiplayer_page.flow));
	multiplayer_page.party_focus = -1;
	multiplayer_page.flow.hit_rows = HIT_ROWS;
	multiplayer_page.flow.hit_prompts = HIT_PROMPTS;
	multiplayer_page.multiplayer = 1;
	return ae_ui_push(&menu_class, AE_OWNER_ANY, &multiplayer_page);
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
	ae_ui_replace_menus();
	(void)ae_settings_ui_scale();
	return 1;
}

void ae_screen_main_menu_close(void)
{
	int closed = ae_ui_remove(&multiplayer_page);

	if (ae_ui_remove(&main_page) || closed)
		platform_log("ae main menu: closed (the game left its main menu)");
}
