/*
AE_SCREEN_CAMPAIGN.C

AE's CAMPAIGN (ae_screen_campaign.h; the mockup's board 7): CONTINUE
(the saved game, when there is one), SPLIT SCREEN (0.9.4: co-op for up to four on this PC) and the ten levels on the left, every
one playable (0.9.3); on the right the level's name, the difficulties it is finished on, DIFFICULTY, and the PLAYERS panel:
MY PARTY's rows (ae_widgets.h), player 1, and the seats of players 2-4 (pads on controllers 2-4). A pad's START asks for a
seat (its profile card, the lobby's), its B leaves it; a lone gamepad shared with the keyboard joins from START too (ADD
PLAYER's state moves it to its own controller). SPLIT SCREEN's A does the same for the keyboard and the mouse (arms the state,
or sends the last player away). A plays the focused level at the difficulty shown, for everyone seated; X (F, or the footer's
HOST ONLINE) hosts it as a co-op game others join (ae_screen_coop_lobby.h): the seated players of this PC play in it too;
CONTINUE is the saved game, solo only (a game of one does not go on with more); left / right or Y change the difficulty; B
back. The rules are ae_coop_rules.c's; the game's calls are menu_functions.c's co-op ("port coop begin" / "port coop player
2"), in ae_glue_campaign.c.

Its log: the glue's ("ae campaign: ..."), "ae campaign: open (<profile>)".
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "interface/player_ui.h"
#include "../src/ae_draw.h"
#include "../src/ae_platform.h"
#include "ae_coop_rules.h"
#include "ae_glue_campaign.h"
#include "ae_glue_lobby.h"
#include "ae_glue_profiles.h"
#include "ae_hooks.h"
#include "ae_input.h"
#include "ae_screen_campaign.h"
#include "ae_screen_coop_lobby.h"
#include "ae_screen_lobby_profile.h"
#include "ae_screen_main_menu.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	CAMPAIGN_HIT = 0x7A00,
	HIT_ROWS = CAMPAIGN_HIT + 1, HIT_PROMPTS, HIT_SIDE, HIT_CARDS, HIT_PARTY
};
enum { ROW_HEADER = AE_FLOW_ROW_HEADER, ROW_CONTINUE = AE_FLOW_ROW_OWN, ROW_COOP, ROW_LEVEL };

static struct
{
	struct ae_flow_screen flow;
	struct ae_campaign campaign;
	short difficulty;
	short level_of_row[AE_FLOW_ROWS];
	char continue_text[48];
	struct ae_coop_seats seats;
	/* (the state this screen turned on: ADD PLAYER's, for the lone gamepad; the lone pad's card is open) */
	int armed_here, lone_pad_picking;
	char names[AE_COOP_CONTROLLERS][48], subs[AE_COOP_CONTROLLERS][48];
	char players_text[16];
} screen;

static void rows_make(void)
{
	struct ae_flow_screen *flow = &screen.flow;
	short count = 0, level;

	if (screen.campaign.saved_level >= 0)
	{
		snprintf(screen.continue_text, sizeof(screen.continue_text), "LEVEL %d \xE2\x80\xA2 %s",
			screen.campaign.saved_level + 1, ae_campaign_difficulty_name(screen.campaign.saved_difficulty));
		screen.level_of_row[count] = -1;
		/* (its difficulty is the saved game's: shown, never changed here) */
		flow->rows[count++] = (struct ae_flow_row){ ROW_CONTINUE, -1, "CONTINUE", screen.continue_text,
			"Goes on from your saved game, at its difficulty.",
			ae_coop_continue_allowed(&screen.seats) ? NULL : "SOLO ONLY", 0, 1 };
	}
	snprintf(screen.players_text, sizeof(screen.players_text), "%s", ae_coop_players_text(ae_coop_players(&screen.seats)));
	screen.level_of_row[count] = -1;
	flow->rows[count++] = (struct ae_flow_row){ ROW_COOP, -1, "SPLIT SCREEN", screen.players_text,
		ae_coop_second(&screen.seats) >= 0 ? "A sends the last player away; a player's own pad B does too." :
		"A adds a player on a pad (its START joins too, up to four); a pad's B leaves.", NULL, 0, 1 };
	screen.level_of_row[count] = -1;
	flow->rows[count++] = (struct ae_flow_row){ ROW_HEADER, -1, "LEVELS", NULL, NULL, NULL };
	for (level = 0; level < AE_CAMPAIGN_LEVELS && count < AE_FLOW_ROWS; level++)
	{
		screen.level_of_row[count] = level;
		flow->rows[count++] = (struct ae_flow_row){ ROW_LEVEL, -1, screen.campaign.levels[level].name, NULL,
			"{A} plays it at the difficulty on the right; {X} hosts it online; left / right change the difficulty.", NULL };
	}
	flow->count = count;
	if (flow->focus >= count)
		flow->focus = (short)(count - 1);
	if (flow->rows[flow->focus].kind == ROW_HEADER)
		flow->focus++;
}

static short focused_level(void)
{
	short focus = screen.flow.focus;

	return focus >= 0 && focus < screen.flow.count ? screen.level_of_row[focus] : -1;
}

static int continue_focused(void);

/* a seat's profile: its name and colour (a guest's "Guest n" when none is saved) */
static void seat_profile(short controller, char *name, int size, unsigned int *color)
{
	static struct ae_profile_summary profiles[AE_PROFILES_MAXIMUM];
	short count = 0, index;
	long active = player_ui_get_active_player_profile_index(controller);

	snprintf(name, (size_t)size, "Guest %d", controller + 1);
	*color = 0x7F8C8DFFu;
	if (!ae_profiles_list(profiles, AE_PROFILES_MAXIMUM, &count).ok)
		return;
	for (index = 0; active != NONE && index < count; index++)
	{
		if (profiles[index].index == active)
		{
			snprintf(name, (size_t)size, "%s", profiles[index].name);
			*color = profiles[index].color;
		}
	}
}

/* PLAYERS: MY PARTY's rows (the main menu's): player 1, the seats of players 2-4 (in the order of their pads) or the card of
the pad choosing its profile, the open card ("Press START to add a player (split screen)") while a seat is free */
static void players_panel(struct ae_flow_screen *flow, float x, float y, float width)
{
	struct ae_density const *d = &flow->density;
	float inset = ae_flow_u(flow, AE_FLOW_INSET_U), indent = ae_flow_u(flow, AE_PARTY_INDENT_U);
	float gap = ae_flow_u(flow, AE_FLOW_CARD_GAP_U);
	short controller, players = ae_coop_players(&screen.seats), place = 1;
	char count[8];
	struct ae_party_row row;

	x += inset;
	width -= 2.0f * inset;
	snprintf(count, sizeof(count), "%d/%d", players, AE_COOP_PLAYERS_MAX);
	y += ae_widget_party_head(d, x, y, width, players > 1 ? "Co-op \xE2\x80\xA2 split screen on this PC" :
		"Campaign \xE2\x80\xA2 this PC", count, HIT_PARTY);
	memset(&row, 0, sizeof(row));
	row.kind = AE_PARTY_PERSON;
	seat_profile(0, screen.names[0], sizeof(screen.names[0]), &row.color);
	if (screen.campaign.have_profile && !ae_profile_is_guest(0))
		snprintf(screen.names[0], sizeof(screen.names[0]), "%s", screen.campaign.profile);
	snprintf(screen.subs[0], sizeof(screen.subs[0]), "%s",
		ae_ui_last_device() == AE_DEVICE_KEYBOARD_MOUSE ? "Player 1 \xE2\x80\xA2 keyboard" : "Player 1 \xE2\x80\xA2 this PC");
	row.name = screen.names[0];
	row.sub_line = screen.subs[0];
	row.slot = 1;
	y += ae_widget_party_row(d, x, y, width, &row, HIT_PARTY, 0);
	for (controller = 1; controller < AE_COOP_CONTROLLERS; controller++)
	{
		float height, hint;

		if (!screen.seats.seated[controller])
		{
			height = ae_screen_lobby_profile_card_draw(d, controller, x + indent, y, width - indent, HIT_CARDS);
			if (height > 0.0f)
				y += height + gap;
			continue;
		}
		memset(&row, 0, sizeof(row));
		row.kind = AE_PARTY_PERSON;
		seat_profile(controller, screen.names[controller], sizeof(screen.names[controller]), &row.color);
		snprintf(screen.subs[controller], sizeof(screen.subs[controller]), "Player %d \xE2\x80\xA2 Controller %d", place + 1,
			controller + 1);
		row.name = screen.names[controller];
		row.sub_line = screen.subs[controller];
		row.slot = (short)(place + 1);
		hint = ae_screen_lobby_profile_hint(d, AE_BUTTON_B, "Leave", 0.0f, 0.0f, 0);
		row.right_room = hint;
		height = ae_widget_party_row(d, x, y, width, &row, HIT_PARTY, place);
		ae_screen_lobby_profile_hint(d, AE_BUTTON_B, "Leave", x + width - ae_flow_u(flow, 16.0f) - hint,
			y + ae_party_row_height(d, AE_PARTY_PERSON) * 0.5f, 1);
		y += height;
		place++;
	}
	if (players < AE_COOP_PLAYERS_MAX && ae_screen_lobby_profile_choosing_count() == 0)
		ae_widget_open_card(d, x + indent, y, width - indent, 0, HIT_CARDS);
}

static void draw_side(void)
{
	struct ae_flow_screen *flow = &screen.flow;
	struct ae_density const *d = &flow->density;
	float x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X, width = ae_flow_right_x(flow) - x;
	float y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y), text = ae_size_text(d), minor = ae_size_minor(d);
	float gap = ae_flow_u(flow, d->metrics->gap);
	short level = focused_level();
	char finished[96];
	struct ae_row row;
	int difficulty, length = 0;

	if (level < 0 && screen.campaign.saved_level >= 0)
		level = screen.campaign.saved_level;
	if (level < 0)
		return;
	ae_draw_text(AE_FONT_ROW, text * 1.5f, x, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, screen.campaign.levels[level].name);
	y += ae_draw_cap_height(AE_FONT_ROW, text * 1.5f) + ae_flow_u(flow, 18.0f);
	finished[0] = 0;
	for (difficulty = 1; difficulty < AE_CAMPAIGN_DIFFICULTIES; difficulty++)
	{
		if (screen.campaign.levels[level].finished[difficulty])
			length += snprintf(finished + length, sizeof(finished) - (size_t)length, "%s%s", length ? ", " : "",
				ae_campaign_difficulty_name((short)difficulty));
	}
	ae_draw_text(AE_FONT_BODY, minor, x, y, AE_ALIGN_LEFT, finished[0] ? AE_COLOR_ACCENT : AE_COLOR_MUTED,
		finished[0] ? finished : screen.campaign.levels[level].reached ? "Not finished yet" : "Not played yet");
	if (finished[0])
		ae_draw_text(AE_FONT_BODY, minor, x + ae_draw_text_width(AE_FONT_BODY, minor, finished) + ae_flow_u(flow, 10.0f), y,
			AE_ALIGN_LEFT, AE_COLOR_MUTED, "finished");
	y += ae_flow_u(flow, 44.0f);
	memset(&row, 0, sizeof(row));
	row.label = "DIFFICULTY";
	row.value = ae_campaign_difficulty_name(continue_focused() ? screen.campaign.saved_difficulty : screen.difficulty);
	row.flags = continue_focused() ? AE_ROW_STATUS : AE_ROW_FOCUSED;
	y += ae_widget_row(d, x, y, width, &row, HIT_SIDE, 0) + gap;
	y += ae_flow_u(flow, 18.0f);
	players_panel(flow, x, y, width);
}

/* whether CONTINUE has the focus: it plays the saved game's difficulty, so DIFFICULTY shows that one and doesn't change
(0.3.9: it showed the next new game's, LEGENDARY beside CONTINUE's NORMAL) */
static int continue_focused(void)
{
	short focus = screen.flow.focus;

	return focus >= 0 && focus < screen.flow.count && screen.flow.rows[focus].kind == ROW_CONTINUE;
}

/* whether SPLIT SCREEN has the focus */
static int coop_focused(void)
{
	short focus = screen.flow.focus;

	return focus >= 0 && focus < screen.flow.count && screen.flow.rows[focus].kind == ROW_COOP;
}

static void difficulty_step(short direction)
{
	short next = ae_value_step(AE_CAMPAIGN_DIFFICULTIES, screen.difficulty, direction);

	if (continue_focused())
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}

	ae_sound_request(next == screen.difficulty ? AE_SOUND_FAILURE : AE_SOUND_CURSOR, 0);
	screen.difficulty = next;
}

/* ---------- the seats (ae_coop_rules.h) */

static void adding_set(int adding)
{
	screen.seats.adding = adding;
	screen.armed_here = adding;
	ae_lobby_adding_player(adding);
}

/* a pad's profile card is answered (ae_screen_lobby_profile.h): picked seats it, B leaves it out */
static void seat_done(short player, int picked, void *context)
{
	int lone_pad = player == 1 && screen.lone_pad_picking, joined;

	(void)context;
	if (screen.seats.picking > 0)
		screen.seats.picking--;
	if (lone_pad)
		screen.lone_pad_picking = 0;
	if (player < 1 || player >= AE_COOP_CONTROLLERS)
		return;
	if (!picked)
	{
		if (lone_pad)
			adding_set(0);
		platform_log("ae campaign: a player not seated (profile pick cancelled)");
		return;
	}
	joined = ae_coop_join(&screen.seats, player);
	if (joined != AE_COOP_JOIN_OK)
	{
		ae_flow_error(&screen.flow, "The player did not join", "The campaign takes four players on this PC.");
		if (lone_pad)
			adding_set(0);
		return;
	}
	platform_log("ae campaign: player %d seated (controller %d%s)", ae_coop_players(&screen.seats), player + 1,
		lone_pad ? ", the keyboard's lone pad" : "");
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* the pad of that controller asks for the seat: its card opens (1: it did) */
static int seat_card_open(short controller)
{
	if (!ae_screen_lobby_profile_open(controller, &screen.flow, seat_done, NULL))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return 0;
	}
	screen.seats.picking++;
	return 1;
}

/* a seated pad leaves (its B, or SPLIT SCREEN's A for the last seat) */
static void seat_leave(short controller)
{
	if (!ae_coop_leave(&screen.seats, controller))
		return;
	if (screen.armed_here)
		adding_set(0);
	platform_log("ae campaign: a player left (controller %d)", controller + 1);
	ae_sound_request(AE_SOUND_BACK, 0);
}

/* the last seated controller (the one SPLIT SCREEN's A sends away), -1 when only player 1 sits */
static short last_seat(void)
{
	short controller;

	for (controller = AE_COOP_CONTROLLERS - 1; controller >= 1; controller--)
		if (screen.seats.seated[controller])
			return controller;
	return -1;
}

/* the keyboard's and mouse's way: SPLIT SCREEN's A turns ADD PLAYER's state on (a lone gamepad takes its own controller and
its START joins), off again, or sends the last player away */
static void coop_row_accept(void)
{
	short last = last_seat();

	if (last >= 0)
		seat_leave(last);
	else if (screen.seats.adding)
	{
		adding_set(0);
		ae_sound_request(AE_SOUND_BACK, 0);
	}
	else
	{
		adding_set(1);
		ae_sound_request(AE_SOUND_CURSOR, 0);
	}
}

/* a controller's press for the seats; 1: taken. START of a pad without a seat asks for one, B of the seated pad leaves it
(never the screen's BACK: that is player 1's), the keyboard's lone pad joins from START (controller 1 shared) */
static int seat_press(struct ae_event const *event)
{
	short controller = ae_input_event_controller();

	if (controller >= 1 && controller < AE_COOP_CONTROLLERS && ae_screen_lobby_profile_event(controller, event))
		return 1;
	if (event->action == AE_ACTION_START &&
		ae_coop_lone_pad_joins(&screen.seats, controller, ae_platform_keyboard_used(), ae_platform_pad_start_held()))
	{
		platform_log("ae campaign: the keyboard's lone pad joins (controller 2)");
		adding_set(1);
		screen.lone_pad_picking = 1;
		if (!seat_card_open(1))
		{
			screen.lone_pad_picking = 0;
			adding_set(0);
		}
		return 1;
	}
	if (event->action == AE_ACTION_START && ae_coop_start_joins(&screen.seats, controller))
	{
		seat_card_open(controller);
		return 1;
	}
	if (event->action == AE_ACTION_BACK && ae_coop_back_is_seat(&screen.seats, controller))
	{
		seat_leave(controller);
		return 1;
	}
	if (event->action == AE_ACTION_BACK && screen.seats.adding && controller >= 1 && controller < AE_COOP_CONTROLLERS)
	{
		adding_set(0);
		ae_sound_request(AE_SOUND_BACK, 0);
		return 1;
	}
	/* (the seated player's other buttons do nothing here: the menu is player 1's) */
	return ae_coop_back_is_seat(&screen.seats, controller) || (controller >= 1 && event->action == AE_ACTION_START);
}

/* the classic menus' co-op errors, in AE's dialog */
static void coop_error(int error)
{
	char const *title, *body;

	ae_coop_error_text(error, &title, &body);
	ae_flow_error(&screen.flow, title, body);
}

static void accept(short row)
{
	struct ae_flow_row const *r;
	int started, error = AE_COOP_ERR_NONE;

	if (row < 0 || row >= screen.flow.count)
		return;
	r = &screen.flow.rows[row];
	if (r->kind == ROW_COOP)
	{
		coop_row_accept();
		return;
	}
	if (r->reason)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	/* (a card open at the start: its pad is not seated) */
	ae_screen_lobby_profile_cancel_all();
	if (r->kind == ROW_CONTINUE)
		started = ae_campaign_continue(&screen.campaign);
	else
	{
		error = ae_campaign_start_players(&screen.campaign, screen.level_of_row[row], screen.difficulty,
			ae_coop_pad_mask(&screen.seats));
		started = error == AE_COOP_ERR_NONE;
	}
	if (started)
	{
		ae_sound_request(AE_SOUND_FORWARD, 0);
		/* (the level loads: no AE screen over it, and the main menu does not open again in the frames before the
		game leaves its own; AE's main menu comes back at the main menu) */
		ae_ui_game_starting();
		while (ae_ui_depth() && ae_ui_top())
		{
			if (!ae_ui_top()->data || !ae_ui_remove(ae_ui_top()->data))
				ae_ui_pop();
		}
		return;
	}
	if (error == AE_COOP_ERR_NONE)
		error = AE_COOP_ERR_NO_PROFILE;
	coop_error(error);
}

/* X: the focused level hosted as a co-op game others join; this PC's seated players play in it too. The lobby is
ae_screen_coop_lobby.c's */
static void host_online(void)
{
	short level = focused_level();
	int error;
	char const *title, *body;

	if (level < 0)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_screen_lobby_profile_cancel_all();
	error = ae_screen_coop_lobby_host(level, screen.difficulty, ae_coop_pad_mask(&screen.seats), &screen.flow);
	if (error == AE_COOP_HOST_ERR_NONE)
	{
		ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	}
	ae_coop_host_error_text(error, &title, &body);
	ae_flow_error(&screen.flow, title, body);
}

static void campaign_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &screen.flow;
	char crumb[64];

	(void)instance;
	ae_flow_full_density(flow);
	rows_make();
	snprintf(crumb, sizeof(crumb), "%s", screen.campaign.have_profile ? screen.campaign.profile : "NO PROFILE");
	ae_flow_title(flow, "CAMPAIGN", crumb);
	ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
	draw_side();
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A,
		coop_focused() ? (ae_coop_second(&screen.seats) >= 0 ? "Remove a player" : screen.seats.adding ? "Cancel" :
		"Add a player") : "Play", AE_ACTION_ACCEPT);
	if (focused_level() >= 0)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Host online", AE_ACTION_X);
	if (!continue_focused())
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Difficulty", AE_ACTION_Y);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

static int campaign_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	if (seat_press(event))
		return 1;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(&screen.flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		difficulty_step(event->action == AE_ACTION_RIGHT ? 1 : -1);
		return 1;
	case AE_ACTION_X:
		host_online();
		return 1;
	case AE_ACTION_Y:
		if (continue_focused())
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return 1;
		}
		screen.difficulty = (short)((screen.difficulty + 1) % AE_CAMPAIGN_DIFFICULTIES);
		ae_sound_request(AE_SOUND_CURSOR, 0);
		return 1;
	case AE_ACTION_ACCEPT:
		accept(screen.flow.focus);
		return 1;
	default:
		return 1;
	}
}

static void campaign_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	short row;

	if (ae_prompts_pointer(screen.flow.prompts, screen.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			campaign_handle(instance, &event);
		return;
	}
	/* (a card's profile row: hover focuses, a click picks) */
	if (ae_screen_lobby_profile_choosing_count() && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_CARDS &&
		hit.part == AE_PART_ITEM)
	{
		ae_screen_lobby_profile_pointer(hit.index, pointer->moved, pointer->left_clicks);
		return;
	}
	/* (the open card: the click is SPLIT SCREEN's A; a seated player's row: that player leaves) */
	if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit) &&
		((hit.id == HIT_CARDS && hit.part == AE_PART_CARD && !ae_screen_lobby_profile_choosing_count()) ||
		(hit.id == HIT_PARTY && hit.part == AE_PART_CARD && hit.index >= 1)))
	{
		if (hit.id == HIT_PARTY && ae_coop_controller_at(&screen.seats, (short)hit.index) >= 1)
			seat_leave(ae_coop_controller_at(&screen.seats, (short)hit.index));
		else
			coop_row_accept();
		return;
	}
	/* (DIFFICULTY's arrows on the right) */
	if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_SIDE && hit.index == 0 &&
		(hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT))
	{
		difficulty_step(hit.part == AE_PART_ARROW_RIGHT ? 1 : -1);
		return;
	}
	row = ae_flow_rows_pointer(&screen.flow, pointer);
	if (row >= 0)
		accept(row);
}

/* the screen closes (B, or the level loads): no card stays open, and a lone gamepad this screen moved goes back to the
keyboard's controller (a co-op game keeps it as player 2's: player_spawn_count) */
static void campaign_leave(struct ae_screen *instance)
{
	(void)instance;
	ae_screen_lobby_profile_cancel_all();
	if (screen.armed_here)
		adding_set(0);
}

static struct ae_screen_class const campaign_class =
{
	.name = "campaign", .leave = campaign_leave, .handle = campaign_handle, .draw = campaign_draw, .pointer = campaign_pointer,
};

int ae_screen_campaign_open(void)
{
	short controller;

	memset(&screen, 0, sizeof(screen));
	ae_coop_seats_reset(&screen.seats);
	/* (the main menu's MY PARTY: a pad signed in there sits as player 2, with its profile) */
	for (controller = 1; controller < AE_COOP_CONTROLLERS; controller++)
	{
		if (ae_screen_main_menu_signed_in(controller) && ae_coop_join(&screen.seats, controller) == AE_COOP_JOIN_OK)
			platform_log("ae campaign: player %d seated from the party (controller %d)", ae_coop_players(&screen.seats),
				controller + 1);
	}
	screen.flow.hit_rows = HIT_ROWS;
	screen.flow.hit_prompts = HIT_PROMPTS;
	ae_campaign_read(&screen.campaign);
	screen.difficulty = screen.campaign.difficulty >= 0 && screen.campaign.difficulty < AE_CAMPAIGN_DIFFICULTIES ?
		screen.campaign.difficulty : 1;
	if (!ae_ui_push(&campaign_class, AE_OWNER_ANY, &screen))
		return 0;
	/* (the first level, or CONTINUE, has the focus: not SPLIT SCREEN) */
	rows_make();
	for (screen.flow.focus = 0; screen.flow.focus < screen.flow.count - 1 &&
		(screen.flow.rows[screen.flow.focus].kind == ROW_HEADER || screen.flow.rows[screen.flow.focus].kind == ROW_COOP);
		screen.flow.focus++)
		;
	platform_log("ae campaign: open (%s)", screen.campaign.have_profile ? screen.campaign.profile : "no profile");
	return 1;
}

int ae_screen_campaign_open_staged(int players, int error)
{
	short controller;

	if (ae_ui_depth() && ae_ui_top() && ae_ui_top()->screen_class == &campaign_class)
		ae_ui_pop();
	if (!ae_screen_campaign_open())
		return 0;
	/* (guests on controllers 2 up, as pads' cards would seat them; the staged game starts with those controllers' players) */
	for (controller = 1; controller < players && controller < AE_COOP_CONTROLLERS; controller++)
	{
		if (!ae_profile_guest(controller).ok || ae_coop_join(&screen.seats, controller) != AE_COOP_JOIN_OK)
			return 0;
		platform_log("ae campaign: staged: player %d seated (guest, controller %d)", controller + 1, controller + 1);
	}
	if (error)
		coop_error(AE_COOP_ERR_SECOND_PROFILE);
	return 1;
}
