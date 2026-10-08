/*
AE_SCREEN_LOBBY_TEST.C

The lobby drive (ae_screen_lobby_test.h), debug.ae_test_screen 90: AE's lobby
glue (ae_glue_lobby.h) end to end with no upstream widget, for
tools/test_ae_lobby.py. It replaces the game's menus, hosts a LAN game, adds
controller 1's player, waits for 4 players (system-link bots) or 30 s, sets
Blood Gulch and slayer, starts, ends the game 8 s in, goes back to the lobby 4 s
into the post-game, and logs the roster once it is back. Its screen draws the
roster as roster cards and the map and gametype as rows (FULL density); it is
off the stack while the game plays, and the post-game take-over
(ae_lobby_take_pregame_ui) puts it back.

Its log: "ae lobby: roster N" (before the map), "ae lobby: back in the lobby,
roster N"; the glue logs the rest ("ae lobby: hosting (LAN)", ...).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game_engine.h"
#include "interface/ui_widget.h"
#include "../src/ae_draw.h"
#include "ae_glue_lobby.h"
#include "ae_hooks.h"
#include "ae_screen_lobby_test.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	DRIVE_IDLE, DRIVE_HOST, DRIVE_ADD_PLAYER, DRIVE_WAIT_PLAYERS, DRIVE_IN_GAME, DRIVE_POSTGAME, DRIVE_BACK, DRIVE_DONE,
	WAIT_PLAYERS_MS = 30000, GAME_MS = 8000, POSTGAME_MS = 4000, PLAYERS = 4
};

static struct
{
	int step;
	unsigned long since;          /* the step's start (or the game's, in DRIVE_IN_GAME once it plays) */
	int playing;
	struct ae_lobby_roster roster;
} drive;

static struct ae_screen_class const lobby_class;

static void step_to(int step, unsigned long now)
{
	drive.step = step;
	drive.since = now;
	drive.playing = 0;
}

static boolean in_game(void)
{
	return game_engine_running() && !main_menu_is_active() && !game_engine_showing_postgame();
}

void ae_screen_lobby_test_tick(unsigned long now)
{
	switch (drive.step)
	{
	case DRIVE_HOST:
		/* (the game's menus closed: an AE screen that replaces them) */
		ae_ui_replace_menus();
		step_to(ae_lobby_host(AE_LOBBY_LAN).ok ? DRIVE_ADD_PLAYER : DRIVE_DONE, now);
		break;
	case DRIVE_ADD_PLAYER:
		/* controller 1's player, once this machine has joined its own game (or 10 s) */
		if (!ae_lobby_roster(&drive.roster).ok || (!drive.roster.joined && now - drive.since < 10000))
			break;
		step_to(ae_lobby_add_local_player(0).ok ? DRIVE_WAIT_PLAYERS : DRIVE_DONE, now);
		break;
	case DRIVE_WAIT_PLAYERS:
		if (!ae_lobby_roster(&drive.roster).ok)
		{
			step_to(DRIVE_DONE, now);
			break;
		}
		if (drive.roster.count >= PLAYERS || now - drive.since >= WAIT_PLAYERS_MS)
		{
			platform_log("ae lobby: roster %d", drive.roster.count);
			if (ae_lobby_set_map("bloodgulch").ok && ae_lobby_set_gametype("slayer").ok && ae_lobby_start().ok)
			{
				/* (off the stack while the game plays: the take-over puts it back) */
				ae_ui_remove(&drive);
				step_to(DRIVE_IN_GAME, now);
			}
			else
				step_to(DRIVE_DONE, now);
		}
		break;
	case DRIVE_IN_GAME:
		/* the game's clock from when it plays */
		if (!drive.playing && in_game())
		{
			drive.playing = 1;
			drive.since = now;
		}
		if (drive.playing && now - drive.since >= GAME_MS)
			step_to(ae_lobby_end_game().ok ? DRIVE_POSTGAME : DRIVE_DONE, now);
		break;
	case DRIVE_POSTGAME:
		if (!game_engine_showing_postgame())
			drive.since = now;
		else if (now - drive.since >= POSTGAME_MS)
			step_to(ae_lobby_back_to_pregame().ok ? DRIVE_BACK : DRIVE_DONE, now);
		break;
	case DRIVE_BACK:
		/* back at the menus with AE's screen shown (the take-over): the roster */
		if (main_menu_is_active() && ae_ui_holds(&drive) && ae_lobby_roster(&drive.roster).ok)
		{
			platform_log("ae lobby: back in the lobby, roster %d", drive.roster.count);
			step_to(DRIVE_DONE, now);
		}
		break;
	default:
		break;
	}
}

/* ---------- the screen */

static void lobby_draw(struct ae_screen *screen)
{
	struct ae_layout layout;
	struct ae_density d;
	struct ae_frame frame;
	struct ae_row row;
	float scale = ae_settings_ui_scale(), x, y, width, title;
	short index;
	char text[96];

	(void)screen;
	ae_draw_view_full();
	ae_draw_current_layout(&layout);
	ae_density_full(layout.height * layout.scale, scale, &d);
	ae_frame_compute(layout.width, scale, &frame);
	/* (the roster as the glue has it now) */
	ae_lobby_roster(&drive.roster);
	x = frame.rect.x + frame.margin;
	y = 70.0f;
	title = 60.0f * frame.unit;
	ae_draw_text(AE_FONT_TITLE, title, x, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, "LOBBY TEST");
	y += ae_draw_cap_height(AE_FONT_TITLE, title) + 60.0f * frame.unit;
	width = frame.rect.width * 0.36f;
	memset(&row, 0, sizeof(row));
	row.label = "MAP";
	row.value = drive.roster.map[0] ? drive.roster.map : "-";
	y += ae_widget_row(&d, x, y, width, &row, 0x7B01, 0) + 6.0f * frame.unit;
	row.label = "GAME TYPE";
	row.value = drive.roster.gametype[0] ? drive.roster.gametype : "-";
	y += ae_widget_row(&d, x, y, width, &row, 0x7B01, 1) + 6.0f * frame.unit;
	/* the players */
	x = frame.rect.x + frame.rect.width * 0.57f;
	y = 200.0f;
	width = frame.rect.x + frame.rect.width - frame.margin - x;
	snprintf(text, sizeof(text), "PLAYERS  %d", drive.roster.count);
	ae_draw_text(AE_FONT_BODY, ae_size_minor(&d), x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
	y += 40.0f * frame.unit;
	for (index = 0; index < drive.roster.count && index < 8; index++)
	{
		static unsigned int const colors[4] = { 0xC0392BFFu, 0x2E86DEFFu, 0x7FA035FFu, 0xE6B422FFu };
		struct ae_lobby_player const *player = &drive.roster.players[index];
		struct ae_roster_card card;
		char sub[64];

		memset(&card, 0, sizeof(card));
		card.name = player->name;
		card.slot = player->slot;
		card.color = colors[index % 4];
		snprintf(sub, sizeof(sub), player->host ? "This PC" : "Machine %d", player->machine);
		card.sub_line = sub;
		card.flags = player->host ? AE_CARD_HOST : 0;
		y += ae_widget_roster_card(&d, x, y, width, &card, 0x7B02, index) + 8.0f * frame.unit;
	}
}

static int lobby_handle(struct ae_screen *screen, struct ae_event const *event)
{
	(void)screen;
	(void)event;
	/* (a drive: input changes nothing, B included) */
	return 1;
}

static struct ae_screen_class const lobby_class =
{
	.name = "lobby test", .handle = lobby_handle, .draw = lobby_draw,
};

int ae_screen_lobby_test_open(int value)
{
	if (value != 90)
		return 0;
	memset(&drive, 0, sizeof(drive));
	ae_lobby_set_pregame_screen(&lobby_class, &drive);
	if (!ae_ui_push(&lobby_class, AE_OWNER_ANY, &drive))
		return 0;
	platform_log("ae lobby: drive (value %d)", value);
	step_to(DRIVE_HOST, 0);
	return 1;
}
