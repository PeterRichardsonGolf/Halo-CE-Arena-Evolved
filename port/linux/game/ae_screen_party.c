/*
AE_SCREEN_PARTY.C

See ae_screen_party.h. The party itself is the main menu's (its signed-in controllers, ae_screen_main_menu.h's party
functions); this popover draws and edits it. Layout (spec §8, mockups 03 / 04): one panel x 1095 to 1824, y 190 to 978
of the frame, AE_COLOR_PANEL with a 1 u rule, 24 u padding; roster cards 66 u (ae_widget_roster_card); THIS PC's action
rows 36 u; the focused item's description at the foot. Owner 11-11: on a guest's card A changes its profile and Y (R)
removes it after a confirm (REMOVE chip: the same); B closes at once (the B hold read no held B on real pads).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/ae_platform.h"
#include "ae_glue_profiles.h"
#include "ae_input.h"
#include "ae_motion.h"
#include "ae_screen_lobby_profile.h"
#include "ae_screen_main_menu.h"
#include "ae_screen_party.h"
#include "ae_screen_profiles.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	PARTY_HIT = 0x76A0,
	HIT_CARDS = PARTY_HIT + 1, HIT_ITEMS, HIT_CHIPS, HIT_PROMPTS
};
/* a focusable item: a seat's card (its controller), ADD A PLAYER, PROFILES */
enum { ITEM_SEAT, ITEM_ADD, ITEM_PROFILES };

#define PANEL_LEFT (1095.0f / 1920.0f)
#define PANEL_RIGHT (1824.0f / 1920.0f)
#define PANEL_TOP (190.0f / 1080.0f)
#define PANEL_BOTTOM (978.0f / 1080.0f)
#define PAD_U 24.0f
#define CARD_GAP_U 8.0f
#define CHIP_HEIGHT_U 34.0f
#define CHIP_GAP_U 8.0f
#define CHIP_PAD_U 12.0f
#define WAIT_PULSE_MS 1200ul
#define ICE_BLUE 0xB8D6FFFFu

static struct
{
	struct ae_flow_screen flow;      /* its density and frame (the screen under it's), its hit ids */
	struct ae_flow_screen const *under;
	short kinds[8], controllers[8], count, focus;
	int waiting;                     /* ADD A PLAYER: the next START or A on a free controller seats it */
	unsigned long waiting_since;
	char description[200];
} party;

static struct ae_screen_class const party_class;

static void items_read(void)
{
	short controller;

	party.count = 0;
	for (controller = 0; controller < 4; controller++)
	{
		char name[48], sub[64];
		unsigned int color;

		if (!ae_screen_main_menu_party_seat(controller, name, (int)sizeof(name), sub, (int)sizeof(sub), &color))
			continue;
		party.kinds[party.count] = ITEM_SEAT;
		party.controllers[party.count++] = controller;
	}
	party.kinds[party.count] = ITEM_ADD;
	party.controllers[party.count++] = -1;
	party.kinds[party.count] = ITEM_PROFILES;
	party.controllers[party.count++] = -1;
	if (party.focus >= party.count)
		party.focus = (short)(party.count - 1);
	if (party.focus < 0)
		party.focus = 0;
}

static int full(void)
{
	return ae_screen_main_menu_party_count() >= 4;
}

static void description_set(void)
{
	short focus = party.focus;
	int size = ae_screen_main_menu_party_count();

	if (party.waiting)
	{
		snprintf(party.description, sizeof(party.description),
			"Press START (or A) on a free controller to seat it. Esc cancels.");
		return;
	}
	switch (party.kinds[focus])
	{
	case ITEM_SEAT:
		if (party.controllers[focus] == 0)
			snprintf(party.description, sizeof(party.description), "Player 1 on this PC. A changes your profile.");
		else
			snprintf(party.description, sizeof(party.description), "Player %d on controller %d. A: a new profile, "
				"picked on their controller. Y: remove them. B on their own controller asks to leave.",
				party.controllers[focus] + 1, party.controllers[focus] + 1);
		break;
	case ITEM_ADD:
		snprintf(party.description, sizeof(party.description), full() ? "The party is full (4/4)." :
			"Seat another player on this PC: the next START or A on a free controller joins.");
		break;
	default:
		snprintf(party.description, sizeof(party.description), "Your profiles on this PC: names, colours and controls.");
		break;
	}
	(void)size;
}

static void close_party(void)
{
	party.waiting = 0;
	ae_ui_remove(&party);
}

static void accept(void)
{
	short focus = party.focus;

	switch (party.kinds[focus])
	{
	case ITEM_SEAT:
		if (ae_screen_main_menu_party_change(party.under, party.controllers[focus]))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case ITEM_ADD:
		if (full())
		{
			platform_log("ae party: full (4/4)");
			ae_sound_request(AE_SOUND_FAILURE, 0);
			break;
		}
		party.waiting = 1;
		party.waiting_since = ae_motion_now();
		platform_log("ae party: add a player: waiting");
		ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	default:
		if (ae_screen_profiles_open())
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	}
}

static unsigned int pulse(unsigned long since)
{
	float t = (float)((ae_motion_now() - since) % WAIT_PULSE_MS) / (float)WAIT_PULSE_MS;
	unsigned int from = AE_COLOR_WARNING, to = ICE_BLUE, color = 0xFFu;
	int shift;

	t = ae_motion_reduced() ? 0.0f : t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
	for (shift = 24; shift >= 8; shift -= 8)
	{
		float a = (float)((from >> shift) & 0xFFu), b = (float)((to >> shift) & 0xFFu);

		color |= (unsigned int)(a + (b - a) * t + 0.5f) << shift;
	}
	return color;
}

/* a chip: square-cornered, its label in 16 u; REMOVE's red, filled left to right by the hold */
static float chip_draw(struct ae_density const *d, float right, float center_y, const char *label, int danger,
	float fill, short index)
{
	struct ae_flow_screen const *flow = &party.flow;
	float size = ae_size(d, 16.0f, 12.0f), height = ae_flow_u(flow, CHIP_HEIGHT_U), pad = ae_flow_u(flow, CHIP_PAD_U);
	float width = ae_draw_text_width(AE_FONT_ROW, size, label) + 2.0f * pad, x = right - width, y = center_y - height * 0.5f;
	unsigned int ink = danger ? AE_COLOR_DESTRUCTIVE_FRAME : AE_COLOR_SELECTION_TEXT;

	ae_draw_rect(x, y, width, height, 0.0f, 0xFFFFFFFFu);
	if (danger && fill > 0.0f)
		ae_draw_rect(x, y, width * fill, height, 0.0f, AE_COLOR_DESTRUCTIVE_FRAME);
	ae_draw_outline(x, y, width, height, 0.0f, ae_flow_u(flow, 1.0f), ink);
	ae_draw_text(AE_FONT_ROW, size, x + pad, ae_flow_cap_top(AE_FONT_ROW, size, center_y), AE_ALIGN_LEFT,
		danger && fill >= 0.5f ? 0xFFFFFFFFu : ink, label);
	ae_hit_add(x, y, width, height, HIT_CHIPS, AE_PART_CHIP, index);
	return width;
}

static void party_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &party.flow;
	struct ae_density const *d;
	float left, right, top, bottom, pad, x, width, y, minor, body;
	char count[24], name[48], sub[64];
	short index, controller;

	(void)instance;
	ae_flow_full_density(flow);
	d = &flow->density;
	items_read();
	description_set();
	left = flow->frame.rect.x + flow->frame.rect.width * PANEL_LEFT;
	right = flow->frame.rect.x + flow->frame.rect.width * PANEL_RIGHT;
	top = ae_frame_y(&flow->frame, PANEL_TOP);
	bottom = ae_frame_y(&flow->frame, PANEL_BOTTOM);
	pad = ae_flow_u(flow, PAD_U);
	minor = ae_size_minor(d);
	body = ae_size_text(d);
	ae_draw_rect(left, top, right - left, bottom - top, 0.0f, AE_COLOR_PANEL);
	ae_draw_outline(left, top, right - left, bottom - top, 0.0f, ae_flow_u(flow, AE_FLOW_RULE_U), AE_COLOR_RULE);
	x = left + pad;
	width = right - left - 2.0f * pad;
	y = top + pad;
	/* (the head: MY PARTY, its count; the line under it) */
	ae_draw_text(AE_FONT_ROW, ae_flow_u(flow, 24.0f), x, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, "MY PARTY");
	snprintf(count, sizeof(count), "%d/4 ON THIS PC", ae_screen_main_menu_party_count());
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x + width, y + ae_flow_u(flow, 4.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED,
		count);
	y += ae_flow_u(flow, 40.0f);
	ae_draw_text(AE_FONT_BODY, ae_size(d, 18.0f, 12.0f), x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED,
		"In Main Menu. Up to 4 players on this PC (split screen).");
	y += ae_flow_u(flow, 36.0f);
	y += ae_widget_group(d, x - ae_flow_u(flow, 8.0f), y, width, "PLAYERS");
	/* (the seats' cards; a pad choosing its profile, its card; the open card) */
	for (index = 0; index < party.count; index++)
	{
		struct ae_roster_card card;
		float height;

		if (party.kinds[index] != ITEM_SEAT)
			continue;
		controller = party.controllers[index];
		memset(&card, 0, sizeof(card));
		ae_screen_main_menu_party_seat(controller, name, (int)sizeof(name), sub, (int)sizeof(sub), &card.color);
		card.name = name;
		card.sub_line = sub;
		card.slot = (short)(controller + 1);
		if (index == party.focus && !party.waiting)
			card.flags |= AE_CARD_FOCUSED;
		height = ae_widget_roster_card(d, x, y, width, &card, HIT_CARDS, index);
		if (index == party.focus && !party.waiting)
		{
			float chip_right = x + width - ae_flow_u(flow, 12.0f), center = y + height * 0.5f;

			if (controller != 0)
				chip_right -= chip_draw(d, chip_right, center, "REMOVE", 1, 0.0f, 1) +
					ae_flow_u(flow, CHIP_GAP_U);
			chip_draw(d, chip_right, center, "CHANGE PROFILE", 0, 0.0f, 0);
		}
		y += height + ae_flow_u(flow, CARD_GAP_U);
	}
	for (controller = 1; controller < 4; controller++)
	{
		float height = ae_screen_lobby_profile_card_draw(d, controller, x, y, width, HIT_CARDS);

		if (height > 0.0f)
			y += height + ae_flow_u(flow, CARD_GAP_U);
	}
	if (!full())
	{
		float height = ae_widget_open_card(d, x, y, width, 0, HIT_CARDS);

		if (party.waiting)
			ae_draw_outline(x, y, width, height - ae_flow_u(flow, 6.0f), 0.0f, ae_flow_u(flow, 2.0f),
				pulse(party.waiting_since));
		y += height;
	}
	/* (THIS PC: ADD A PLAYER, PROFILES) */
	y += ae_flow_u(flow, 12.0f);
	y += ae_widget_group(d, x - ae_flow_u(flow, 8.0f), y, width, "THIS PC");
	for (index = 0; index < party.count; index++)
	{
		struct ae_party_row row;
		static char profiles_value[24];

		if (party.kinds[index] == ITEM_SEAT)
			continue;
		memset(&row, 0, sizeof(row));
		row.kind = AE_PARTY_ACTION;
		if (party.kinds[index] == ITEM_ADD)
		{
			row.name = full() ? "THE PARTY IS FULL" : "ADD A PLAYER";
		}
		else
		{
			static struct ae_profile_summary profiles[AE_PROFILES_MAXIMUM];
			short profile_count = 0;

			(void)ae_profiles_list(profiles, AE_PROFILES_MAXIMUM, &profile_count);
			snprintf(profiles_value, sizeof(profiles_value), "%d ON THIS PC", profile_count);
			row.name = "PROFILES";
			row.value = profiles_value;
		}
		if (index == party.focus && !party.waiting)
			row.flags |= AE_CARD_FOCUSED;
		y += ae_widget_party_row(d, x, y, width, &row, HIT_ITEMS, index);
	}
	/* (the foot: a rule, the focused item's description in 2 lines) */
	{
		char lines[2][160];
		float line = ae_flow_u(flow, 30.0f), foot = bottom - pad - line * 2.0f;
		int count_lines = ae_wrap_text(AE_FONT_BODY, body, party.description, width, lines, 2), at;

		ae_draw_rect(x, foot - ae_flow_u(flow, 14.0f), width, ae_flow_u(flow, AE_FLOW_RULE_U), 0.0f, AE_COLOR_RULE);
		for (at = 0; at < count_lines; at++)
			ae_draw_text(AE_FONT_BODY, body, x, foot + line * (float)at, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[at]);
	}
	/* (owner 11-11: the pad's buttons named, as every AE screen's prompts) */
	flow->prompt_count = 0;
	if (!party.waiting)
	{
		int guest = party.kinds[party.focus] == ITEM_SEAT && party.controllers[party.focus] > 0;

		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, party.kinds[party.focus] == ITEM_SEAT ?
			"Change profile" : ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
		if (guest)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Remove", AE_ACTION_Y);
	}
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

/* (a guest's own B asks to leave from here too: ae_screen_main_menu.c) */
static void party_update(struct ae_screen *instance)
{
	(void)instance;
	ae_screen_main_menu_party_update();
}

static void remove_ask(void)
{
	if (party.kinds[party.focus] != ITEM_SEAT || party.controllers[party.focus] <= 0)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (ae_screen_main_menu_party_remove_ask(party.under, party.controllers[party.focus], 0))
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

static int party_handle(struct ae_screen *instance, struct ae_event const *event)
{
	short controller = ae_input_event_controller();

	(void)instance;
	/* (ADD A PLAYER waiting: a free controller's A seats it as its START would) */
	if (party.waiting && controller >= 1 && controller <= 3 && event->action == AE_ACTION_ACCEPT &&
		!ae_screen_main_menu_party_seat(controller, NULL, 0, NULL, 0, NULL))
	{
		struct ae_event start = *event;

		start.action = AE_ACTION_START;
		party.waiting = 0;
		return ae_screen_main_menu_party_press_here(party.under, &start);
	}
	if (controller >= 1 && controller <= 3)
	{
		if (event->action == AE_ACTION_START && full())
		{
			platform_log("ae party: full (4/4)");
			snprintf(party.description, sizeof(party.description), "The party is full (4/4).");
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return 1;
		}
		if (event->action == AE_ACTION_START)
			party.waiting = 0;
		ae_screen_main_menu_party_press_here(party.under, event);
		return 1;
	}
	/* (player 1's START: a lone pad sharing the keyboard's controller joins as player 2, as on MY PARTY) */
	if (event->action == AE_ACTION_START)
	{
		if (!ae_screen_main_menu_party_press_here(party.under, event))
			ae_sound_request(AE_SOUND_FAILURE, 0);
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		if (party.waiting)
		{
			party.waiting = 0;
			platform_log("ae party: add a player: cancelled");
			ae_sound_request(AE_SOUND_BACK, 0);
			return 1;
		}
		ae_sound_request(AE_SOUND_BACK, 0);
		close_party();
		return 1;
	case AE_ACTION_Y:
		if (!party.waiting)
			remove_ask();
		return 1;
	case AE_ACTION_X:
		ae_sound_request(AE_SOUND_BACK, 0);
		close_party();
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		if (party.waiting)
			return 1;
		{
			short next = (short)(party.focus + (event->action == AE_ACTION_UP ? -1 : 1));

			if (next >= 0 && next < party.count)
			{
				party.focus = next;
				ae_sound_request(AE_SOUND_CURSOR, event->repeat);
			}
		}
		return 1;
	case AE_ACTION_ACCEPT:
		if (!party.waiting)
			accept();
		return 1;
	default:
		return 1;
	}
}

static void party_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_hit hit;
	struct ae_event event;

	if (ae_prompts_pointer(party.flow.prompts, party.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		event.player = pointer->player;
		party_handle(instance, &event);
		return;
	}
	if (pointer->right_clicks)
	{
		ae_sound_request(AE_SOUND_BACK, 0);
		close_party();
		return;
	}
	/* (a guest's profile card open inline: its rows take the mouse, as MY PARTY's do) */
	if (ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_CARDS && hit.part == AE_PART_ITEM &&
		ae_screen_lobby_profile_choosing_count())
	{
		ae_screen_lobby_profile_pointer(hit.index, pointer->moved, pointer->left_clicks);
		return;
	}
	if (!ae_hit_at(pointer->x, pointer->y, 0, &hit) || (hit.id != HIT_CARDS && hit.id != HIT_ITEMS && hit.id != HIT_CHIPS))
	{
		if (pointer->left_clicks)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			close_party();
		}
		return;
	}
	if (hit.id == HIT_CHIPS)
	{
		/* (CHANGE PROFILE or REMOVE clicked: REMOVE asks first) */
		if (pointer->left_clicks && hit.index == 0)
			accept();
		else if (pointer->left_clicks && hit.index == 1)
			remove_ask();
		return;
	}
	if (hit.index >= 0 && hit.index < party.count)
	{
		if (pointer->moved && !party.waiting)
			party.focus = hit.index;
		if (pointer->left_clicks && !party.waiting)
		{
			party.focus = hit.index;
			accept();
		}
	}
}

static void party_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae party: closed");
}

static struct ae_screen_class const party_class =
{
	.name = "party", .leave = party_leave, .handle = party_handle, .draw = party_draw, .pointer = party_pointer,
	.update = party_update, .start_from_anyone = 1, .popover = 1,
};

int ae_screen_party_open(struct ae_flow_screen const *under)
{
	if (!under || ae_ui_holds(&party))
		return 0;
	memset(&party, 0, sizeof(party));
	party.under = under;
	party.flow.hit_rows = HIT_ITEMS;
	party.flow.hit_prompts = HIT_PROMPTS;
	/* (every controller's presses reach it: player 1's move it, the others' START and their profile cards are theirs) */
	if (!ae_ui_push(&party_class, AE_OWNER_ANY, &party))
		return 0;
	items_read();
	platform_log("ae party: open (%d/4)", ae_screen_main_menu_party_count());
	return 1;
}

void ae_screen_party_close(void)
{
	(void)ae_ui_remove(&party);
}
