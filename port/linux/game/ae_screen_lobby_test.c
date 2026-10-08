/*
AE_SCREEN_LOBBY_TEST.C

The lobby drives (ae_screen_lobby_test.h), debug.ae_test_screen 90-95: AE's
lobby glue (ae_glue_lobby.h) end to end with no upstream widget, for
tools/test_ae_lobby.py, and (94) the profiles' glue (ae_glue_profiles.h). 90
replaces the game's menus, hosts a LAN game, adds controller 1's player, waits
for 4 players (system-link bots) or 30 s, sets Blood Gulch and slayer, starts,
ends the game 8 s in, goes back to the lobby 4 s into the post-game, and logs
the roster once it is back. 92 does the same for 2 players and then logs the
roster as it changes; 91 joins the first LAN game, waits for 2 players, and
once back from the host's game leaves; 93 hosts LOCAL and 95 ONLINE, logging
the roster as it changes. 94 (the profile drive) makes a profile, gives it to
player 1, saves controls and a colour, reads the profile back from its file,
then makes player 2 a guest whose settings are never saved. Its screen draws
the roster as roster cards and the map and gametype as rows (FULL density); it
is off the stack while the game plays, and the post-game take-over
(ae_lobby_take_pregame_ui) puts it back.

Its log: "ae lobby: roster N" (before the map), "ae lobby: back in the lobby,
roster N"; the glue logs the rest ("ae lobby: hosting (LAN)", ...). The
profile drive's: "ae profiles: ..." (below and the glue's).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game_engine.h"
#include "interface/player_ui.h"
#include "interface/ui_widget.h"
#include "saved games/player_profile.h"
#include "../src/ae_draw.h"
#include "ae_glue_lobby.h"
#include "ae_glue_profiles.h"
#include "ae_hooks.h"
#include "ae_screen_lobby_test.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	DRIVE_IDLE, DRIVE_HOST, DRIVE_JOIN, DRIVE_JOINING, DRIVE_ADD_PLAYER, DRIVE_WAIT_PLAYERS, DRIVE_IN_GAME,
	DRIVE_POSTGAME, DRIVE_BACK, DRIVE_WATCH, DRIVE_CLIENT_GAME, DRIVE_CLIENT_BACK, DRIVE_PROFILES, DRIVE_DONE,
	WAIT_PLAYERS_MS = 30000, GAME_MS = 8000, POSTGAME_MS = 4000
};

static struct
{
	int value, step;
	unsigned long since;          /* the step's start (or the game's, in DRIVE_IN_GAME once it plays) */
	int playing, players, logged_count, have_roster;
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

/* the roster's players, logged with what the glue says of them (this machine's, the host's) */
static void log_players(void)
{
	short index;

	for (index = 0; index < drive.roster.count; index++)
		platform_log("ae lobby: player %d '%s' local %d host %d", drive.roster.players[index].slot,
			drive.roster.players[index].name, drive.roster.players[index].local, drive.roster.players[index].host);
}

/* the roster now, read only while AE's session is there (the glue logs a failure: not every frame) */
static boolean read_roster(void)
{
	drive.have_roster = ae_lobby_owns_session() && ae_lobby_roster(&drive.roster).ok;
	return drive.have_roster;
}

static int host_kind(void)
{
	return drive.value == 93 ? AE_LOBBY_LOCAL : drive.value == 95 ? AE_LOBBY_ONLINE : AE_LOBBY_LAN;
}

/* (the start: Blood Gulch slayer; off the stack while the game plays, the take-over puts it back) */
static void start_game(unsigned long now)
{
	if (ae_lobby_set_map("bloodgulch").ok && ae_lobby_set_gametype("slayer").ok && ae_lobby_start().ok)
	{
		ae_ui_remove(&drive);
		step_to(DRIVE_IN_GAME, now);
	}
	else
		step_to(DRIVE_DONE, now);
}

/* the profiles listed: their count, and each as the list has it */
static short log_profiles(const char *when)
{
	static struct ae_profile_summary profiles[AE_PROFILES_MAXIMUM];
	short count = 0, index;

	if (!ae_profiles_list(profiles, AE_PROFILES_MAXIMUM, &count).ok)
		return -1;
	platform_log("ae profiles: count %d%s", count, when);
	for (index = 0; index < count; index++)
		platform_log("ae profiles: '%s' colour %08X layout %d", profiles[index].name, profiles[index].color,
			profiles[index].layout);
	return count;
}

/* 94: the profile drive, at once (the glue's calls are all immediate) */
static void profile_drive(void)
{
	static struct ae_profile_controls const controls = { 2, 5, 1, 0 }, guest_controls = { 3, 7, 0, 1 };
	struct ae_profile_controls read;
	struct player_profile profile;
	struct ae_result result;
	short before;
	int index;

	before = log_profiles("");
	if (before < 0 || !ae_profile_new("AE TEST", &index).ok)
		return;
	/* (a second of the same name: refused, the glue logs why) */
	ae_profile_new("AE TEST", NULL);
	if (!ae_profile_switch(0, index).ok || !ae_profile_set_controls(0, &controls).ok || !ae_profile_set_color(0, 3).ok)
		return;
	/* the profile read again from its file (player_profile_get waits for the save's write) */
	if (player_profile_get((long)(unsigned int)index, &profile))
	{
		platform_log("ae profiles: read back controls %d %d %d %d colour %d",
			profile.controller_settings.button_preset, profile.controller_settings.look_sensitivity,
			profile.controller_settings.invert_look ? 1 : 0, profile.controller_settings.vibration_disabled ? 0 : 1,
			profile.primary_color_index);
	}
	if (ae_profile_get_controls(0, &read).ok)
		platform_log("ae profiles: player 1 controls %d %d %d %d in the game", read.layout, read.look_sensitivity,
			read.invert, read.vibration);
	/* upstream's edit buffer open with nobody's profile edit screen up (left over): not a lock */
	player_ui_begin_editing_profile((long)(unsigned int)index);
	if (ae_profile_set_color(0, 4).ok)
		platform_log("ae profiles: saved past an idle upstream edit buffer");
	player_ui_end_editing_profile();
	log_profiles(" before the guest");
	/* player 2 a guest: their settings this session's only */
	if (!ae_profile_guest(1).ok)
		return;
	platform_log("ae profiles: player 2 is guest %d, player 1 is guest %d", ae_profile_is_guest(1),
		ae_profile_is_guest(0));
	result = ae_profile_set_controls(1, &guest_controls);
	platform_log("ae profiles: guest controls ok %d (%s)", result.ok, result.reason);
	if (ae_profile_get_controls(1, &read).ok)
		platform_log("ae profiles: guest controls %d %d %d %d this session", read.layout, read.look_sensitivity,
			read.invert, read.vibration);
	ae_profile_set_color(1, 5);
	log_profiles(" after the guest");
	platform_log("ae profiles: done");
}

void ae_screen_lobby_test_tick(unsigned long now)
{
	switch (drive.step)
	{
	case DRIVE_HOST:
		/* (the game's menus closed: an AE screen that replaces them) */
		ae_ui_replace_menus();
		step_to(ae_lobby_host(host_kind()).ok ? DRIVE_ADD_PLAYER : DRIVE_DONE, now);
		break;
	case DRIVE_PROFILES:
		ae_ui_replace_menus();
		profile_drive();
		step_to(DRIVE_DONE, now);
		break;
	case DRIVE_JOIN:
		ae_ui_replace_menus();
		step_to(ae_lobby_join_first_available().ok ? DRIVE_JOINING : DRIVE_DONE, now);
		break;
	case DRIVE_JOINING:
	{
		int state = ae_lobby_join_state(NULL);

		if (state > 0)
			step_to(DRIVE_ADD_PLAYER, now);
		else if (state < 0)
			step_to(DRIVE_DONE, now);
		break;
	}
	case DRIVE_ADD_PLAYER:
		/* controller 1's player, once this machine has joined (its own game, or the host's; or 10 s) */
		if (!read_roster() || (!drive.roster.joined && now - drive.since < 10000))
			break;
		if (!ae_lobby_add_local_player(0).ok)
			step_to(DRIVE_DONE, now);
		/* (LOCAL and ONLINE hosts: the roster watched from here) */
		else if (drive.value == 93 || drive.value == 95)
			step_to(DRIVE_WATCH, now);
		else
			step_to(DRIVE_WAIT_PLAYERS, now);
		break;
	case DRIVE_WAIT_PLAYERS:
		if (!read_roster())
		{
			step_to(DRIVE_DONE, now);
			break;
		}
		if (drive.roster.count >= drive.players || now - drive.since >= WAIT_PLAYERS_MS)
		{
			platform_log("ae lobby: roster %d", drive.roster.count);
			log_players();
			drive.logged_count = drive.roster.count;
			/* (a joiner waits for the host's game) */
			if (drive.value == 91)
				step_to(DRIVE_CLIENT_GAME, now);
			else
				start_game(now);
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
		if (main_menu_is_active() && ae_ui_holds(&drive) && read_roster())
		{
			platform_log("ae lobby: back in the lobby, roster %d", drive.roster.count);
			drive.logged_count = drive.roster.count;
			step_to(drive.value == 92 ? DRIVE_WATCH : DRIVE_DONE, now);
		}
		break;
	case DRIVE_WATCH:
		/* the roster as it changes (players joining and leaving), first as it is */
		if (read_roster() && (drive.roster.count != drive.logged_count || drive.logged_count < 0))
		{
			platform_log("ae lobby: roster %d", drive.roster.count);
			drive.logged_count = drive.roster.count;
		}
		break;
	case DRIVE_CLIENT_GAME:
		/* a joiner: off the stack while the host's game plays */
		if (in_game())
		{
			ae_ui_remove(&drive);
			step_to(DRIVE_CLIENT_BACK, now);
		}
		break;
	case DRIVE_CLIENT_BACK:
		/* back in the lobby with AE's screen (the client's take-over): leave, a second later */
		if (!(main_menu_is_active() && ae_ui_holds(&drive)))
			drive.since = now;
		else if (now - drive.since >= 1000)
		{
			/* (a failure is the glue's to log) */
			ae_lobby_leave();
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
	/* (the roster as the tick last read it: drawing logs nothing) */
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
	if (value < 90 || value > 95)
		return 0;
	memset(&drive, 0, sizeof(drive));
	drive.value = value;
	drive.players = value == 90 ? 4 : 2;
	drive.logged_count = -1;
	ae_lobby_set_pregame_screen(&lobby_class, &drive);
	if (!ae_ui_push(&lobby_class, AE_OWNER_ANY, &drive))
		return 0;
	platform_log("ae lobby: drive (value %d)", value);
	step_to(value == 91 ? DRIVE_JOIN : value == 94 ? DRIVE_PROFILES : DRIVE_HOST, 0);
	return 1;
}
