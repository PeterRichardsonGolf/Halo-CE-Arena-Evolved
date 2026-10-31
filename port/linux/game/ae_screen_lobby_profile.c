/*
AE_SCREEN_LOBBY_PROFILE.C

The profile chooser shown before a local player joins AE's lobby, or signs in on the main menu. Saved profiles another
player on this PC already uses are left out, GUEST is always offered, and the profile (or guest) the automatic join
used before is focused.

Player 1 (a browser or invite join) uses the menus' value picker, owned by that player. Players 2-4 (split screen) choose
INLINE: a card of their own in the PLAYERS panel (the main menu's, the lobby's roster; the approved mockups-042
split-profile), a menu-blue frame, "Choose a profile", the player and controller, the profiles as rows and the
button hints. The pads choose AT ONCE, each with its own card and only its own pad's events; the choices are rebuilt every
frame from what the others hold, so the profile one pad picks is gone from the others' lists at once, and a focus left
on a profile that went moves to the choice that took its place. A card has NEW PROFILE before GUEST: its A opens PROFILES'
NEW PROFILE page, owned by that pad (ae_screen_profiles_new_for), and the profile made there is the player's.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "interface/player_ui.h"
#include "../src/ae_draw.h"
#include "ae_glue_lobby.h"
#include "ae_glue_profiles.h"
#include "ae_lobby_profile_choices.h"
#include "ae_screen_lobby_profile.h"
#include "ae_screen_main_menu.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_screen_profiles.h"
#include "ae_style.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum { PROFILE_CHOICES = AE_PROFILES_MAXIMUM + 2, CARDS = 3, HIT_STRIDE = 100 };
/* the card's spec units (the mockup's pixels at 1080p) */
#define CARD_ROW_PER_ROW 0.8f      /* a profile row's height, of the menus' row */
#define CARD_ROWS_ABOVE_U 2.0f     /* the header's bottom to the first profile row */
#define CARD_FOOT_U 52.0f          /* the last row's bottom (and its gap) to the card's bottom, the hints in it */
#define CARD_HINTS_U 18.0f         /* the last row's bottom to the hints' middle */
#define CARD_FRAME_U 2.0f
#define CARD_ROW_INSET_U 18.0f
#define CARD_SWATCH_U 16.0f
#define CARD_NAME_GAP_U 18.0f
#define CHIP_RADIUS_U 10.0f
#define CHIP_GAP_U 7.0f
#define HINT_GAP_U 22.0f
#define GUEST_COLOR 0x7F8C8DFFu
#define NEW_COLOR 0x2894FEFFu       /* the menu blue: a profile still to make */

/* a player's choices: the saved profiles another local player does not hold, then (a card's) NEW PROFILE, then GUEST */
struct list
{
	struct ae_profile_summary profiles[AE_PROFILES_MAXIMUM];
	int choices[PROFILE_CHOICES];
	const char *values[PROFILE_CHOICES];
	unsigned int colors[PROFILE_CHOICES];
	short count, focus;
	int current_guest;
};

/* the popover (player 1) */
static struct
{
	struct list list;
	short player;
	int open, reset_hook;
	ae_lobby_profile_done done;
	void *context;
} pick;

/* the inline cards (players 2-4: index player - 1) */
static struct
{
	struct list list;
	int open;
	int focused;     /* the choice the focus is on, by its id (a profile's index, GUEST or NEW) */
	ae_lobby_profile_done done;
	void *context;
	int making;      /* NEW PROFILE's page is open over the screen for this player */
} cards[CARDS];

/* the choices for a player into list (focus: today's automatic choice's); with_new: NEW PROFILE before GUEST */
static void list_build(struct list *list, short player, int with_new)
{
	int indices[AE_PROFILES_MAXIMUM], used[AE_MAXIMUM_PLAYERS - 1];
	long active;
	short profile_count = 0, used_count = 0, index, other;

	if (!ae_profiles_list(list->profiles, AE_PROFILES_MAXIMUM, &profile_count).ok)
		profile_count = 0;
	for (index = 0; index < profile_count; index++)
		indices[index] = list->profiles[index].index;
	for (other = 0; other < AE_MAXIMUM_PLAYERS; other++)
	{
		long used_profile;

		/* (player 1 always plays on this PC; players 2-4 signed in on the main menu hold their picks too; a card
		still choosing holds nothing) */
		if (other == player || (other != 0 && !ae_lobby_controller_plays(other) &&
			!player_ui_local_player_wants_to_play_multiplayer(other) && !ae_screen_main_menu_signed_in(other)))
			continue;
		used_profile = player_ui_get_active_player_profile_index(other);
		if (used_profile != NONE)
			used[used_count++] = (int)(unsigned int)used_profile;
	}
	active = player_ui_get_active_player_profile_index(player);
	list->current_guest = ae_profile_is_guest(player);
	list->count = ae_lobby_profile_choices(indices, profile_count, used, used_count, (int)(unsigned int)active,
		list->current_guest, list->choices, PROFILE_CHOICES - 1, &list->focus);
	/* (GUEST is the last choice: NEW PROFILE takes its place, GUEST and a focus on it move down one) */
	if (with_new && list->count > 0)
	{
		list->choices[list->count] = AE_LOBBY_PROFILE_GUEST;
		list->choices[list->count - 1] = AE_LOBBY_PROFILE_NEW;
		if (list->focus == list->count - 1)
			list->focus++;
		list->count++;
	}
	for (index = 0; index < list->count; index++)
	{
		list->values[index] = list->choices[index] == AE_LOBBY_PROFILE_NEW ? "NEW PROFILE" : "GUEST";
		list->colors[index] = list->choices[index] == AE_LOBBY_PROFILE_NEW ? NEW_COLOR : GUEST_COLOR;
		for (other = 0; list->choices[index] != AE_LOBBY_PROFILE_GUEST && list->choices[index] != AE_LOBBY_PROFILE_NEW &&
			other < profile_count; other++)
		{
			if (list->profiles[other].index == list->choices[index])
			{
				list->values[index] = list->profiles[other].name;
				list->colors[index] = list->profiles[other].color;
			}
		}
	}
}

/* a choice taken: the profile (or guest) is the player's now; 1 when it is */
static int choice_take(struct list const *list, short choice, short player)
{
	struct ae_result result;
	int profile = list->choices[choice];

	if (profile == AE_LOBBY_PROFILE_GUEST && list->current_guest)
	{
		result.ok = 1;
		result.reason[0] = 0;
	}
	else
	{
		result = profile == AE_LOBBY_PROFILE_GUEST ? ae_profile_guest(player) : ae_profile_switch(player, profile);
	}
	platform_log("ae lobby: player %d picked %s", player + 1,
		profile == AE_LOBBY_PROFILE_GUEST ? "GUEST" : list->values[choice]);
	return result.ok;
}

/* ---------- the popover (player 1) */

static void finish(int picked)
{
	ae_lobby_profile_done done = pick.done;
	void *context = pick.context;
	short player = pick.player;

	pick.open = 0;
	if (done)
		done(player, picked, context);
}

/* ---------- the inline cards */

static void card_finish(short player, int picked)
{
	ae_lobby_profile_done done = cards[player - 1].done;
	void *context = cards[player - 1].context;

	cards[player - 1].open = 0;
	if (done)
		done(player, picked, context);
}

static void profile_reset(void)
{
	short player;

	if (pick.open)
		finish(0);
	for (player = 1; player <= CARDS; player++)
		if (cards[player - 1].open)
			card_finish(player, 0);
}

static void profile_closed(void *context)
{
	(void)context;
	platform_log("ae lobby: player %d profile pick cancelled", pick.player + 1);
	finish(0);
}

static void profile_picked(short choice, void *context)
{
	(void)context;
	if (choice < 0 || choice >= pick.list.count)
	{
		finish(0);
		return;
	}
	finish(choice_take(&pick.list, choice, pick.player));
}

/* a card's choices again from what the others hold now; 0 when the profile its focus was on went (the focus moves to
the choice that took its place) */
static int card_refresh(short player)
{
	struct list *list = &cards[player - 1].list;
	int focused = cards[player - 1].focused, present;
	short previous = list->focus, index;

	list_build(list, player, 1);
	present = 0;
	for (index = 0; index < list->count; index++)
		present |= list->choices[index] == focused;
	list->focus = ae_lobby_profile_refocus(list->choices, list->count, focused, previous);
	cards[player - 1].focused = list->count ? list->choices[list->focus] : AE_LOBBY_PROFILE_GUEST;
	return present;
}

/* NEW PROFILE's page closed: a profile made is the player's already and the card is done; else the card goes on */
static void card_made(short player, int made, void *context)
{
	(void)context;
	if (player < 1 || player > CARDS)
		return;
	cards[player - 1].making = 0;
	if (!ae_screen_lobby_profile_choosing(player))
		return;
	if (made)
		card_finish(player, 1);
}

static void card_pick(short player)
{
	struct list *list = &cards[player - 1].list;

	if (list->choices[list->focus] == AE_LOBBY_PROFILE_NEW)
	{
		if (ae_screen_profiles_new_for(player, card_made, NULL))
		{
			cards[player - 1].making = 1;
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else
			ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	card_finish(player, choice_take(list, list->focus, player));
}

static void card_cancel(short player)
{
	platform_log("ae lobby: player %d profile pick cancelled", player + 1);
	ae_sound_request(AE_SOUND_BACK, 0);
	card_finish(player, 0);
}

int ae_screen_lobby_profile_choosing(short local_player)
{
	return local_player >= 1 && local_player <= CARDS && cards[local_player - 1].open;
}

short ae_screen_lobby_profile_choosing_count(void)
{
	short player, count = 0;

	for (player = 1; player <= CARDS; player++)
		count += cards[player - 1].open != 0;
	return count;
}

void ae_screen_lobby_profile_cancel_all(void)
{
	short player;

	for (player = 1; player <= CARDS; player++)
		if (cards[player - 1].open)
			card_cancel(player);
}

int ae_screen_lobby_profile_event(short local_player, struct ae_event const *event)
{
	struct list *list;
	short next;

	if (!ae_screen_lobby_profile_choosing(local_player))
		return 0;
	/* (NEW PROFILE's page takes the pad's events while it is open) */
	if (cards[local_player - 1].making)
		return 1;
	list = &cards[local_player - 1].list;
	switch (event->action)
	{
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		card_refresh(local_player);
		next = (short)(list->focus + (event->action == AE_ACTION_UP ? -1 : 1));
		if (next >= 0 && next < list->count)
		{
			list->focus = next;
			cards[local_player - 1].focused = list->choices[next];
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		}
		break;
	case AE_ACTION_ACCEPT:
		/* (the profile it was on went to another pad a moment ago: the focus moved, nothing is picked) */
		if (!card_refresh(local_player))
			ae_sound_request(AE_SOUND_FAILURE, 0);
		else
			card_pick(local_player);
		break;
	case AE_ACTION_BACK:
		card_cancel(local_player);
		break;
	default:
		break;
	}
	return 1;
}

void ae_screen_lobby_profile_pointer(short hit_index, int hover, int click)
{
	short player = (short)(hit_index / HIT_STRIDE), row = (short)(hit_index % HIT_STRIDE);
	struct list *list;

	if (!ae_screen_lobby_profile_choosing(player))
		return;
	list = &cards[player - 1].list;
	if (row < 0 || row >= list->count)
		return;
	if (hover || click)
	{
		list->focus = row;
		cards[player - 1].focused = list->choices[row];
	}
	if (click)
		card_pick(player);
}

/* ---------- drawing */

float ae_screen_lobby_profile_hint(struct ae_density const *density, int button, const char *label, float x,
	float center_y, int draw)
{
	float radius = CHIP_RADIUS_U * density->unit, size = ae_size_minor(density), start = x;
	int device = ae_ui_last_device();
	const char *letter = ae_help_button_word(device, button);

	/* (the pad's face button as a round menu-blue chip with its letter knocked out; the keyboard's key cap, and a
	letter longer than one, a PlayStation's CROSS, come from the footer's own mark) */
	if (device == AE_DEVICE_KEYBOARD_MOUSE || !letter || !letter[0] || letter[1])
	{
		x += ae_widget_button_mark(density, button, x, center_y, ae_size_glyph(density), 0, draw);
	}
	else
	{
		if (draw)
		{
			ae_draw_rect(x, center_y - radius, radius * 2.0f, radius * 2.0f, radius, AE_COLOR_WARNING);
			ae_draw_text(AE_FONT_ROW, radius * 1.05f, x + radius, ae_flow_cap_top(AE_FONT_ROW, radius * 1.05f, center_y),
				AE_ALIGN_CENTER, AE_COLOR_SELECTION_TEXT, letter);
		}
		x += radius * 2.0f;
	}
	x += CHIP_GAP_U * density->unit;
	if (draw)
		ae_draw_text(AE_FONT_BODY, size, x, ae_flow_cap_top(AE_FONT_BODY, size, center_y), AE_ALIGN_LEFT, AE_COLOR_TITLE,
			label);
	x += ae_draw_text_width(AE_FONT_BODY, size, label);
	return x - start;
}

/* a card's height: its header, its rows and its foot */
static float card_height(struct ae_density const *density, struct list const *list)
{
	float row_height = ae_size_row(density) * CARD_ROW_PER_ROW, pitch = row_height + density->metrics->gap * density->unit;

	return density->metrics->card * density->unit + (float)list->count * pitch + CARD_FOOT_U * density->unit;
}

float ae_screen_lobby_profile_card_height(struct ae_density const *density, short local_player)
{
	if (!ae_screen_lobby_profile_choosing(local_player))
		return 0.0f;
	card_refresh(local_player);
	return card_height(density, &cards[local_player - 1].list);
}

float ae_screen_lobby_profile_card_draw(struct ae_density const *density, short local_player, float x, float y,
	float width, short hit_id)
{
	struct ae_metrics const *m = density->metrics;
	struct list *list;
	float unit = density->unit, header = m->card * unit, emblem = m->emblem * unit, corner = m->corner * unit;
	float row_height = ae_size_row(density) * CARD_ROW_PER_ROW, pitch = row_height + m->gap * unit;
	float inset = (header - emblem) * 0.5f, text_x = x + inset + emblem + CARD_NAME_GAP_U * unit, height;
	float name_size = ae_size_text(density), minor = ae_size_minor(density), row_size = (name_size + minor) * 0.5f;
	float rows_y, row_x, row_right, name_x, hints_x;
	char text[64];
	short index;

	if (!ae_screen_lobby_profile_choosing(local_player))
		return 0.0f;
	/* (the others may have taken a profile since the last event) */
	card_refresh(local_player);
	list = &cards[local_player - 1].list;
	height = card_height(density, list);
	ae_draw_rect(x, y, width, height, corner, AE_COLOR_ROW);
	ae_draw_outline(x, y, width, height, corner, CARD_FRAME_U * unit, AE_COLOR_WARNING);
	/* the header, a roster card's: the grey emblem with the slot, what it asks, who asks */
	ae_draw_rect(x + inset, y + inset, emblem, emblem, corner, GUEST_COLOR);
	snprintf(text, sizeof(text), "%d", local_player + 1);
	ae_draw_text(AE_FONT_ROW, emblem * 0.55f, x + inset + emblem * 0.5f,
		ae_flow_cap_top(AE_FONT_ROW, emblem * 0.55f, y + header * 0.5f), AE_ALIGN_CENTER,
		ae_emblem_number_color(GUEST_COLOR), text);
	ae_draw_text(AE_FONT_ROW, name_size, text_x, ae_flow_cap_top(AE_FONT_ROW, name_size, y + header * 0.34f),
		AE_ALIGN_LEFT, AE_COLOR_TITLE, "Choose a profile");
	snprintf(text, sizeof(text), "Player %d \xE2\x80\xA2 Controller %d", local_player + 1, local_player + 1);
	ae_draw_text(AE_FONT_BODY, minor, text_x, ae_flow_cap_top(AE_FONT_BODY, minor, y + header * 0.71f), AE_ALIGN_LEFT,
		AE_COLOR_MUTED, text);
	/* the profiles: its colour's swatch and name, the focus a white bar; GUEST says it is not saved */
	rows_y = y + header + CARD_ROWS_ABOVE_U * unit;
	row_x = text_x;
	row_right = x + width - CARD_ROW_INSET_U * unit;
	for (index = 0; index < list->count; index++)
	{
		int focused = index == list->focus, guest = list->choices[index] == AE_LOBBY_PROFILE_GUEST;
		int made = list->choices[index] == AE_LOBBY_PROFILE_NEW;
		float row_y = rows_y + (float)index * pitch, swatch = CARD_SWATCH_U * unit, center = row_y + row_height * 0.5f;
		char fitted[96];

		ae_draw_rect(row_x, row_y, row_right - row_x, row_height, 0.0f, focused ? AE_COLOR_SELECTION : AE_COLOR_ROW);
		if (made)
		{
			/* (NEW PROFILE: a "+" in the menu blue where a profile's swatch is) */
			ae_draw_rect(row_x + 12.0f * unit, center - unit, swatch, 2.0f * unit, 0.0f, list->colors[index]);
			ae_draw_rect(row_x + 12.0f * unit + swatch * 0.5f - unit, center - swatch * 0.5f, 2.0f * unit, swatch, 0.0f,
				list->colors[index]);
		}
		else
		{
			ae_draw_rect(row_x + 12.0f * unit, center - swatch * 0.5f, swatch, swatch, 0.0f, list->colors[index]);
		}
		name_x = row_x + 12.0f * unit + swatch + 14.0f * unit;
		ae_fit_text(AE_FONT_ROW, row_size, list->values[index], row_right - name_x - (guest ? 120.0f * unit : 12.0f * unit),
			fitted, sizeof(fitted));
		ae_draw_text(AE_FONT_ROW, row_size, name_x, ae_flow_cap_top(AE_FONT_ROW, row_size, center), AE_ALIGN_LEFT,
			focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TITLE, fitted);
		if (guest)
			ae_draw_text(AE_FONT_BODY, minor * 0.85f, row_right - 14.0f * unit,
				ae_flow_cap_top(AE_FONT_BODY, minor * 0.85f, center), AE_ALIGN_RIGHT,
				focused ? AE_COLOR_SELECTION_MUTED : AE_COLOR_MUTED, "not saved");
		ae_hit_add(row_x, row_y, row_right - row_x, row_height, hit_id, AE_PART_ITEM,
			(short)(local_player * HIT_STRIDE + index));
	}
	/* the buttons: A picks, B cancels */
	hints_x = text_x;
	hints_x += ae_screen_lobby_profile_hint(density, AE_BUTTON_A, "Pick", hints_x,
		rows_y + (float)list->count * pitch + CARD_HINTS_U * unit, 1) + HINT_GAP_U * unit;
	ae_screen_lobby_profile_hint(density, AE_BUTTON_B, "Cancel", hints_x,
		rows_y + (float)list->count * pitch + CARD_HINTS_U * unit, 1);
	return height;
}

/* ---------- opening */

int ae_screen_lobby_profile_open(short local_player, struct ae_flow_screen const *flow,
	ae_lobby_profile_done done, void *context)
{
	struct ae_picker_spec spec;

	if (!flow || local_player < 0 || local_player >= AE_MAXIMUM_PLAYERS || (local_player == 0 && pick.open) ||
		ae_screen_lobby_profile_choosing(local_player))
		return 0;
	if (!pick.reset_hook)
		pick.reset_hook = ae_ui_add_reset_hook(profile_reset);
	if (local_player == 0)
		ae_profile_player1_ensure();
	/* The chooser belongs to this player even before their network player exists. */
	player_ui_set_single_player_local_player_controller(local_player, local_player);
	if (local_player >= 1)
	{
		cards[local_player - 1].done = done;
		cards[local_player - 1].context = context;
		cards[local_player - 1].making = 0;
		list_build(&cards[local_player - 1].list, local_player, 1);
		cards[local_player - 1].focused = cards[local_player - 1].list.choices[cards[local_player - 1].list.focus];
		cards[local_player - 1].open = 1;
		platform_log("ae lobby: player %d profile pick (%d choices)", local_player + 1,
			cards[local_player - 1].list.count);
		return 1;
	}
	list_build(&pick.list, local_player, 0);
	pick.player = local_player;
	pick.done = done;
	pick.context = context;
	memset(&spec, 0, sizeof(spec));
	spec.values = pick.list.values;
	spec.count = pick.list.count;
	spec.current = pick.list.focus;
	spec.bounds = flow->frame.rect;
	spec.density = flow->density;
	spec.row.width = flow->frame.rect.width * 0.42f;
	spec.row.height = ae_size_row(&flow->density);
	spec.row.x = flow->frame.rect.x + (flow->frame.rect.width - spec.row.width) * 0.5f;
	spec.row.y = flow->frame.rect.y + (flow->frame.rect.height - spec.row.height) * 0.5f;
	spec.picked = profile_picked;
	spec.closed = profile_closed;
	if (!ae_picker_open(&spec, local_player))
		return 0;
	pick.open = 1;
	platform_log("ae lobby: player %d profile pick (%d choices)", local_player + 1, pick.list.count);
	return 1;
}
