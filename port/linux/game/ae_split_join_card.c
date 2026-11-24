/*
AE_SPLIT_JOIN_CARD.C

See ae_split_join_card.h. 1080 tall layout units, as the AE menus' other cards: a 560 wide panel in the upper middle (the HUD's
top corners and the bottom strip stay clear) on AE's view-panel fill with a 4 u top edge in the seat's colour; the seat's
64 u tile (that colour, the seat number) at the left; PLAYER n and the name tracked and muted, the line in the title face,
the prompts in the footer's own glyphs. A refused card is 640 wide and keeps the tile.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "game/game.h"
#include "game/players.h"
#include "input/input.h"
#include "../src/ae_draw.h"
#include "../src/ae_layout.h"
#include "../src/ae_platform.h"
#include "ae_glue_split_join.h"
#include "ae_hooks.h"
#include "ae_sound.h"
#include "ae_split_join_card.h"
#include "ae_split_join_rules.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum { CONTROLLERS = 4, MAXIMUM_OPEN_MILLISECONDS = 12000 };

#define CARD_WIDTH_U 560.0f
#define REFUSED_WIDTH_U 640.0f
#define CARD_HEIGHT_U 176.0f
#define CARD_TOP_U 150.0f
#define STRIPE_U 4.0f
#define PAD_U 28.0f
#define TILE_U 64.0f
#define TILE_TOP_U 36.0f
#define TEXT_GAP_U 24.0f
#define FOOT_U 36.0f
#define REFUSED_FOOT_U 28.0f
#define LABEL_U 20.0f
#define LINE_U 38.0f
#define REASON_U 20.0f
#define PROMPT_U 28.0f
#define PROMPT_GAP_U 32.0f

/* the player colours, by controller (the pause menu's) */
static unsigned int const player_colors[CONTROLLERS] = { 0xC0392BFFu, 0x2E86DEFFu, 0x7FA035FFu, 0xE6B422FFu };

static struct
{
	int open[CONTROLLERS];
	int reason[CONTROLLERS];
	unsigned long opened_at[CONTROLLERS];
	int held[CONTROLLERS][3];   /* A, B, START as read last frame */
	int staged[CONTROLLERS];
} card;

static int menus_on(void)
{
	return ae_menus_active();
}

static void close_card(short controller, char const *why)
{
	card.open[controller] = 0;
	platform_log("split join: controller %d card closed (%s)", (int)controller + 1, why);
}

static int pad_font(short controller);

static void open_card(short controller, int reason)
{
	card.open[controller] = 1;
	card.reason[controller] = reason;
	card.opened_at[controller] = system_milliseconds();
	card.held[controller][0] = card.held[controller][1] = card.held[controller][2] = 1;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("split join: controller %d card open: %s (prompts %s)", (int)controller + 1,
		reason ? ae_split_join_reason_text(reason) : "PRESS A TO JOIN",
		pad_font(controller) == AE_FONT_PLAYSTATION ? "playstation" : pad_font(controller) == AE_FONT_NINTENDO ? "nintendo" :
		pad_font(controller) == AE_FONT_UNIVERSAL ? "universal" : "xbox");
}

/* a card is of a game with a split screen to join: the reasons that say it is none are no card at all */
static int shows_card(int reason)
{
	return reason != AE_SPLIT_JOIN_NOT_MULTIPLAYER && reason != AE_SPLIT_JOIN_REPLAY && reason != AE_SPLIT_JOIN_IN_GAME &&
		reason != AE_SPLIT_JOIN_NO_PAD;
}

int ae_split_join_card_open(short controller)
{
	return controller >= 0 && controller < CONTROLLERS && card.open[controller];
}

int ae_split_join_card_stage(short controller)
{
	int reason;
	short other;

	if (controller < 1 || controller >= CONTROLLERS)
		return 0;
	/* (a staged card stands alone: the others open now are put away) */
	for (other = 1; other < CONTROLLERS; other++)
		card.open[other] = other == controller ? card.open[other] : 0;
	card.staged[controller] = 1;
	reason = ae_split_join_reason(controller, 1);
	if (!shows_card(reason))
		return 0;
	open_card(controller, reason);
	return 1;
}

void ae_split_join_card_tick(void)
{
	short controller;

	ae_split_join_tick();
	for (controller = 1; controller < CONTROLLERS; controller++)
	{
		const struct gamepad_state *pad;
		int a, b, start, real = input_has_gamepad(controller);

		if (!menus_on() || (!real && !card.open[controller]))
		{
			card.open[controller] = 0;
			continue;
		}
		pad = real ? input_get_gamepad_state(controller) : NULL;
		a = pad && pad->analog_buttons[_gamepad_analog_button_a] > 0;
		b = pad && pad->analog_buttons[_gamepad_analog_button_b] > 0;
		start = pad && pad->buttons[_gamepad_binary_button_start] != 0;
		if (card.open[controller])
		{
			int reason = ae_split_join_reason(controller, card.staged[controller]);

			if (local_player_exists(controller) || ae_split_join_waiting(controller))
				close_card(controller, "joined");
			else if (system_milliseconds() - card.opened_at[controller] > MAXIMUM_OPEN_MILLISECONDS)
				close_card(controller, "timed out");
			else if ((b && !card.held[controller][1]) || (start && !card.held[controller][2]))
			{
				ae_sound_request(AE_SOUND_BACK, 0);
				close_card(controller, "cancelled");
			}
			else if (a && !card.held[controller][0])
			{
				if (reason == AE_SPLIT_JOIN_OK && !ae_split_join_request(controller, card.staged[controller]))
				{
					ae_sound_request(AE_SOUND_FORWARD, 0);
					close_card(controller, "asked");
				}
				else
				{
					ae_sound_request(AE_SOUND_BACK, 0);
					close_card(controller, "refused");
				}
			}
			else
				card.reason[controller] = reason;
		}
		else if (start && !card.held[controller][2] && !local_player_exists(controller))
		{
			int reason = ae_split_join_reason(controller, 0);

			if (shows_card(reason))
				open_card(controller, reason);
		}
		card.held[controller][0] = a;
		card.held[controller][1] = b;
		card.held[controller][2] = start;
	}
}

/* (xinput_sdl.c) the family of the pad on a port: 1 an Xbox-like pad, 2 a PlayStation pad, 3 a Nintendo pad, -1 none */
int halo_gamepad_scheme(int port);

/* the glyph font of the controller's own pad (its player's BUTTON PROMPTS choice when they made one), never the last device
used: the card belongs to this controller; an Xbox pad's when the pad reports no family */
static int pad_font(short controller)
{
	switch (ae_platform_button_prompts(controller))
	{
	case 1: return AE_FONT_XBOX;
	case 2: return AE_FONT_PLAYSTATION;
	case 3: return AE_FONT_NINTENDO;
	case 4: return AE_FONT_UNIVERSAL;
	}
	switch (halo_gamepad_scheme(controller))
	{
	case 2: return AE_FONT_PLAYSTATION;
	case 3: return AE_FONT_NINTENDO;
	}
	return AE_FONT_XBOX;
}

/* a prompt's glyph (in the font) and label from x; returns the right edge */
static float prompt(int font, int button, char const *label, float x, float middle, float scale)
{
	float size = PROMPT_U * scale;
	float top = 0.0f, bottom = size;

	ae_draw_button_ink(font, button, size, &top, &bottom);
	x += ae_draw_button(font, button, size, x, middle - (top + bottom) * 0.5f, AE_COLOR_PROMPT_WHITE);
	x += 10.0f * scale;
	ae_draw_text(AE_FONT_BODY, 22.0f * scale, x, middle - ae_draw_cap_height(AE_FONT_BODY, 22.0f * scale) * 0.5f,
		AE_ALIGN_LEFT, AE_COLOR_TEXT, label);
	return x + ae_draw_text_width(AE_FONT_BODY, 22.0f * scale, label);
}

/* text of a font whose capitals are centred on middle (ae_draw_text takes the capitals' top) */
static float centred_text(int font, float size, float x, float middle, int align, unsigned int rgba, char const *text)
{
	return ae_draw_text(font, size, x, middle - ae_draw_cap_height(font, size) * 0.5f, align, rgba, text);
}

static void draw_one(short controller, struct ae_layout const *layout)
{
	int reason = card.reason[controller];
	float scale = 1.0f, width = reason == AE_SPLIT_JOIN_OK ? CARD_WIDTH_U : REFUSED_WIDTH_U, height = CARD_HEIGHT_U;
	float x, y, tile_x, tile_y, tile, tx, middle;
	char label[48], number[4];
	int font = pad_font(controller);

	if (width > layout->width - 64.0f)
	{
		scale = (layout->width - 64.0f) / width;
		width *= scale;
		height *= scale;
	}
	/* (each controller's own card sits one row below the one before, so two pads' cards do not cover each other) */
	x = (layout->width - width) * 0.5f;
	y = CARD_TOP_U + (float)(controller - 1) * (height + 16.0f);
	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_VIEW_PANEL);
	ae_draw_rect(x, y, width, STRIPE_U, 0.0f, player_colors[controller]);
	/* the seat's tile: its colour with the seat number */
	tile = TILE_U * scale;
	tile_x = x + PAD_U * scale;
	tile_y = y + TILE_TOP_U * scale;
	ae_draw_rect(tile_x, tile_y, tile, tile, 2.0f, player_colors[controller]);
	snprintf(number, sizeof(number), "%d", (int)controller + 1);
	centred_text(AE_FONT_ROW, tile * 0.55f, tile_x + tile * 0.5f, tile_y + tile * 0.5f, AE_ALIGN_CENTER,
		AE_COLOR_SELECTION_TEXT, number);
	tx = tile_x + tile + TEXT_GAP_U * scale;
	middle = y + height - FOOT_U * scale;
	if (reason == AE_SPLIT_JOIN_OK)
	{
		float right;

		snprintf(label, sizeof(label), "PLAYER %d  \xC2\xB7  GUEST %d", (int)controller + 1, (int)controller + 1);
		ae_draw_text_tracked(AE_FONT_BODY, LABEL_U * scale, 0.12f, tx,
			y + 44.0f * scale - ae_draw_cap_height(AE_FONT_BODY, LABEL_U * scale) * 0.5f, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
		centred_text(AE_FONT_TITLE, LINE_U * scale, tx, y + 84.0f * scale, AE_ALIGN_LEFT, AE_COLOR_TITLE, "PRESS A TO JOIN");
		right = prompt(font, AE_BUTTON_A, "Join", tx, middle, scale);
		prompt(font, AE_BUTTON_B, "Cancel", right + PROMPT_GAP_U * scale, middle, scale);
	}
	else
	{
		snprintf(label, sizeof(label), "PLAYER %d", (int)controller + 1);
		ae_draw_text_tracked(AE_FONT_BODY, LABEL_U * scale, 0.12f, tx,
			y + 44.0f * scale - ae_draw_cap_height(AE_FONT_BODY, LABEL_U * scale) * 0.5f, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
		centred_text(AE_FONT_TITLE, LINE_U * scale, tx, y + 84.0f * scale, AE_ALIGN_LEFT, AE_COLOR_TITLE, "CANNOT JOIN");
		centred_text(AE_FONT_BODY, REASON_U * scale, tx, y + 118.0f * scale, AE_ALIGN_LEFT, AE_COLOR_DESTRUCTIVE_INK,
			ae_split_join_reason_text(reason));
		prompt(font, AE_BUTTON_B, "Close", tx, y + height - REFUSED_FOOT_U * scale, scale);
	}
}

void ae_split_join_card_draw(void)
{
	static unsigned int drawn_frame;
	static int drawn;
	struct ae_layout layout;
	unsigned int frame;
	short controller;
	int any = 0;

	for (controller = 1; controller < CONTROLLERS; controller++)
		any |= card.open[controller];
	if (!any || !ae_draw_available())
		return;
	frame = ae_draw_frame();
	if (drawn && drawn_frame == frame)
		return;
	drawn = 1;
	drawn_frame = frame;
	ae_draw_view_full();
	ae_draw_current_layout(&layout);
	for (controller = 1; controller < CONTROLLERS; controller++)
	{
		if (card.open[controller])
			draw_one(controller, &layout);
	}
}
