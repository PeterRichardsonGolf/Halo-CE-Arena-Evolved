/*
AE_SCREEN_MAIN_MENU.C

AE's MAIN MENU and MULTIPLAYER (ae_screen_main_menu.h; the mockup's boards 5 and 6,
https://claude.ai/artifact/DNum7ugkfszzX99GuZEpsD). Rows on the left (ae_setup_flow.h), on the right player 1's card,
"START to add a player" and the last game; the prompts. QUIT asks first. MULTIPLAYER's CREATE GAME and JOIN WITH INVITE
open CREATE GAME (ae_screen_create_game.h) over it; SERVER BROWSER and LAN GAMES, and the main menu's CAMPAIGN,
SETTINGS and PROFILES, are disabled with why until their AE screens are built.

Its log: "ae main menu: open", "ae main menu: <item>", "ae main menu: quit".
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game_engine.h"
#include "saved games/playlist_profile.h"
#include "interface/player_ui.h"
#include "../src/ae_draw.h"
#include "ae_glue_profiles.h"
#include "ae_hooks.h"
#include "ae_screen_create_game.h"
#include "ae_screen_main_menu.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "halo_ui_map_list.h"

void platform_log(char const *format, ...);
void platform_request_quit(void);
/* ui_widget_event_handler_functions.c (declared as menu_functions.c does) */
short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);
short ui_widget_port_gametypes(long *indices, short maximum, short *last_used);

enum
{
	MENU_HIT = 0x7B00,
	HIT_ROWS = MENU_HIT + 1, HIT_PROMPTS, HIT_CARDS
};
enum { ROW_MULTIPLAYER = AE_FLOW_ROW_OWN, ROW_CAMPAIGN, ROW_SETTINGS, ROW_PROFILES, ROW_QUIT, ROW_CREATE, ROW_JOIN,
	ROW_BROWSER, ROW_LAN };

#define NOT_YET "COMING IN 0.3.7"

static struct
{
	struct ae_flow_screen flow;
	int multiplayer;            /* the MULTIPLAYER page (its own screen over the main menu) */
	char player[48];
	unsigned int player_color;
	char last_game[96];
} main_page, multiplayer_page;

static struct ae_screen_class const menu_class;

static void rows_main(void)
{
	struct ae_flow_screen *flow = &main_page.flow;
	short count = 0;

	flow->rows[count++] = (struct ae_flow_row){ ROW_MULTIPLAYER, -1, "MULTIPLAYER", NULL,
		"Create a game, join one with an invite, or find one in the server browser.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_CAMPAIGN, -1, "CAMPAIGN", NULL,
		"Halo's campaign, alone, in split screen or in online co-op.", NOT_YET };
	flow->rows[count++] = (struct ae_flow_row){ ROW_SETTINGS, -1, "SETTINGS", NULL,
		"Game, display, interface, controls, audio, online and chat.", NOT_YET };
	flow->rows[count++] = (struct ae_flow_row){ ROW_PROFILES, -1, "PROFILES", NULL,
		"Your profiles: names, colours and controls, and who plays as player 1.", NOT_YET };
	flow->rows[count++] = (struct ae_flow_row){ ROW_QUIT, -1, "QUIT", NULL, "Leaves the game.", NULL };
	flow->count = count;
}

static void rows_multiplayer(void)
{
	struct ae_flow_screen *flow = &multiplayer_page.flow;
	short count = 0;

	flow->rows[count++] = (struct ae_flow_row){ ROW_CREATE, -1, "CREATE GAME", NULL,
		"Pick a map, a gametype and its rules, and how others join: this PC, your network, or online with an invite.",
		NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_JOIN, -1, "JOIN WITH INVITE", NULL,
		"Joins the game whose invite link or code is on your clipboard.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_BROWSER, -1, "SERVER BROWSER", NULL,
		"The internet's games, refreshed in place.", NOT_YET };
	flow->rows[count++] = (struct ae_flow_row){ ROW_LAN, -1, "LAN GAMES", NULL,
		"Games on your network, no internet needed.", NOT_YET };
	flow->count = count;
}

/* player 1's profile (name and colour) and the last game (map and gametype), read as the page opens */
static void facts_read(void)
{
	static struct ae_profile_summary profiles[AE_PROFILES_MAXIMUM];
	long active = player_ui_get_active_player_profile_index(0);
	short count = 0, index, last = 0;
	char const *const *names = NULL;
	long gametypes[128];
	char map[64], gametype[16];
	wchar_t wide[64];

	snprintf(main_page.player, sizeof(main_page.player), "PLAYER 1");
	main_page.player_color = 0xC0392BFFu;
	if (ae_profiles_list(profiles, AE_PROFILES_MAXIMUM, &count).ok)
	{
		for (index = 0; index < count; index++)
		{
			if (profiles[index].index == active)
			{
				snprintf(main_page.player, sizeof(main_page.player), "%s", profiles[index].name);
				main_page.player_color = profiles[index].color;
			}
		}
	}
	map[0] = gametype[0] = 0;
	count = ui_widget_port_multiplayer_maps(&names, &last);
	if (last >= 0 && last < count)
	{
		int at;

		ui_map_list_caption(names[last], wide, 64);
		for (at = 0; at < 63 && wide[at]; at++)
			map[at] = wide[at] >= 0x20 && wide[at] < 0x7F ? (char)wide[at] : '?';
		map[at] = 0;
	}
	count = ui_widget_port_gametypes(gametypes, 128, &last);
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

static void draw_side(struct ae_flow_screen *flow)
{
	struct ae_density const *d = &flow->density;
	struct ae_roster_card card;
	float x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X, width = ae_flow_right_x(flow) - x;
	float y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y), minor = ae_size_minor(d), body = ae_size_text(d);
	float bottom = ae_flow_content_bottom(flow), box;

	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, "PLAYERS");
	y += ae_flow_u(flow, 30.0f);
	memset(&card, 0, sizeof(card));
	card.name = main_page.player;
	card.slot = 1;
	card.color = main_page.player_color;
	card.sub_line = "Player 1 \xE2\x80\xA2 this PC";
	y += ae_widget_roster_card(d, x, y, width, &card, HIT_CARDS, 0) + ae_flow_u(flow, AE_FLOW_CARD_GAP_U);
	ae_widget_open_card(d, x, y, width, 0, HIT_CARDS);
	if (!main_page.last_game[0])
		return;
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
	case ROW_CREATE:
	case ROW_JOIN:
		if (ae_screen_create_game_from_menu(r->kind == ROW_JOIN))
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

	ae_draw_view_full();
	ae_flow_full_density(flow);
	if (multiplayer)
		rows_multiplayer();
	else
		rows_main();
	ae_flow_title(flow, multiplayer ? "MULTIPLAYER" : "ARENA EVOLVED", multiplayer ? "MAIN MENU" : NULL);
	ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
	draw_side(flow);
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
	if (multiplayer)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

static int menu_handle(struct ae_screen *instance, struct ae_event const *event)
{
	int multiplayer = instance->data == &multiplayer_page;
	struct ae_flow_screen *flow = multiplayer ? &multiplayer_page.flow : &main_page.flow;

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
	short row;

	if (ae_prompts_pointer(flow->prompts, flow->prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK && multiplayer)
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
		accept(multiplayer, row);
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
	main_page.flow.hit_rows = HIT_ROWS;
	main_page.flow.hit_prompts = HIT_PROMPTS;
	if (!ae_ui_push(&menu_class, AE_OWNER_ANY, &main_page))
		return 0;
	platform_log("ae main menu: open");
	ae_ui_replace_menus();
	(void)ae_settings_ui_scale();
	return 1;
}
