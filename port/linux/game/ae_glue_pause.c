/*
AE_GLUE_PAUSE.C

The AE pause menu's hooks and its contact with the game (ae_pause.h, ae_glue_pause.h). The classic pause's own
choices are kept: which game pauses it (ui_widget.c ui_check_for_pause_game: a network game's pause is the player's
own and pauses nothing, game_engine_allow_pause permitting; a campaign's pauses the game), and each row's engine call.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/event_manager.h"
#include "interface/player_ui.h"
#include "interface/ui_widget.h"
#include "main/main.h"
#include "networking/network_game_globals.h"
#include "networking/network_connection.h"
#include "saved games/arena_gametypes.h"
#include "saved games/game_state.h"
#include "sound/sound_manager.h"
#include "tag_files/tag_groups.h"
#include "network_coop.h"
#include "team_change.h"
#include "ae_glue_pause.h"
#include "ae_glue_postgame.h"
#include "ae_header_lines.h"
#include "ae_hooks.h"
#include "ae_pause.h"
#include "ae_mode_guard_rules.h"
#include "ae_screen_create_game.h"
#include "ae_screen_pause.h"
#include "ae_strip_engine.h"
#include "ae_scoreboard_header.h"

void platform_log(char const *format, ...);
int config_boolean(char const *name);
int config_write(char const *name, char const *value);
short main_get_window_count(void);

/* (ui_widget_definitions.h's group tag, as menu_tags.c defines it) */
#define UI_WIDGET_DEFINITION_TAG 'DeLa'
#define SOLO_PAUSE_SCREEN "ui\\shell\\solo_game\\pause_game\\pause_game"
#define ALONE_PAUSE_SCREEN "ui\\shell\\multiplayer_game\\pause_game\\1p_pause_game"
/* (ui_widget.c: the buttons a closing widget's player lets go of first) */
#define CLOSED_INHIBIT_FLAGS 0x0FFF

/* the team colours of the AE menus (the gallery's player colours: red, blue) */
#define TEAM_RED 0xC0392BFFu
#define TEAM_BLUE 0x2E86DEFFu
#define FFA_OTHER 0x84939FFFu
#define FFA_ME 0x5FC8ECFFu

/* the time and sound this pause stopped (a campaign's), to start again as it closes */
static struct
{
	boolean time;
	boolean sound;
	/* (an OFFLINE game stopped: its own client and server hear nothing from each other meanwhile; the engine's
	global_connection_dont_timeout keeps them from timing out (no other machine is in that session)) */
	boolean connections;
	boolean dont_timeout_before;
} stopped;

/* ---------- the hooks */

boolean ae_pause_start_pressed(short controller_index, boolean network_game)
{
	short count = local_player_count();
	int game;

	if (!ae_menus_active() || controller_index < 0 || controller_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		return FALSE;
	/* (a classic widget of the player's up, an error's: the classic pause goes on deciding) */
	if (ui_widgets_active_for_local_player(controller_index))
		return FALSE;
	if (network_game)
	{
		if (!game_engine_allow_pause())
			return FALSE;
		game = network_coop_active() ? AE_PAUSE_COOP : AE_PAUSE_MULTIPLAYER;
	}
	else
	{
		/* (the classic opens one pause for one or two players, while the game's time runs; three or more it calls
		confused: left to it) */
		if (count > 2 || ui_widgets_active() || game_time_get_paused())
			return FALSE;
		game = tag_loaded(UI_WIDGET_DEFINITION_TAG, SOLO_PAUSE_SCREEN) == NONE &&
			tag_loaded(UI_WIDGET_DEFINITION_TAG, ALONE_PAUSE_SCREEN) != NONE ? AE_PAUSE_ALONE : AE_PAUSE_CAMPAIGN;
	}
	if (!ae_screen_pause_open(controller_index, game))
		return FALSE;
	/* (the START that opened it is not the screen's first press) */
	event_manager_flush();
	return TRUE;
}

boolean ae_pause_player_open(short local_player_index)
{
	return ae_screen_pause_holds(local_player_index) ? TRUE : FALSE;
}

boolean ae_pause_scoreboard_shown(short local_player_index)
{
	return ae_screen_pause_scoreboard(local_player_index) ? TRUE : FALSE;
}

/* (the round games are the Bomb engine's: only in the builds with the game browser's modes, as team_change.c) */
static boolean bomb_rounds(void)
{
#ifdef HALO_GAME_BROWSER
	return game_engine_bomb_rounds();
#else
	return FALSE;
#endif
}

/* ---------- facts */

void ae_glue_pause_facts(short local_player, int game, struct ae_pause_facts *facts)
{
	memset(facts, 0, sizeof(*facts));
	(void)local_player;
	facts->game = game;
	facts->host = global_network_game_server_get() != NULL;
	facts->teams = game == AE_PAUSE_MULTIPLAYER && game_engine_running() && team_change_pause_offered();
	facts->lobby = facts->host && ae_screen_create_game_hosting();
	facts->rounds = game == AE_PAUSE_MULTIPLAYER && facts->host && game_engine_running() && bomb_rounds();
	facts->local_players = local_player_count();
}

int ae_glue_pause_view(short local_player, short *view, short *views, short *spans_two_columns)
{
	short index = 0, at;

	*view = 0;
	*views = main_get_window_count();
	*spans_two_columns = 0;
	if (!local_player_exists(local_player))
		return 0;
	if (*views <= 1)
	{
		*views = 1;
		return 1;
	}
	for (at = local_player_get_next(NONE); at != NONE && at != local_player; at = local_player_get_next(at))
		index++;
	*view = index;
	/* (three views: the first spans the top, as compute_window_bounds lays them) */
	*spans_two_columns = *views == 3 && index == 0;
	return 1;
}

int ae_glue_pause_still_on(short local_player, int game)
{
	if (!game_in_progress() || main_menu_is_active() || !local_player_exists(local_player))
		return 0;
	if (game == AE_PAUSE_MULTIPLAYER && (game_engine_showing_postgame() || game_engine_game_over()))
		return 0;
	return 1;
}

static void narrow(wchar_t const *wide, char *text, int size)
{
	int at;

	for (at = 0; at < size - 1 && wide && wide[at]; at++)
		text[at] = wide[at] >= 0x20 && wide[at] < 0x7F ? (char)wide[at] : wide[at] == 0x00B7 ? '-' : '?';
	text[at] = 0;
}

void ae_glue_pause_team(short local_player, char *text, int size)
{
	struct ae_strip_snapshot snapshot;

	text[0] = 0;
	if (ae_strip_engine_snapshot(local_player, &snapshot) && snapshot.teams && snapshot.me.team_index != NONE)
		snprintf(text, size, "%s", snapshot.me.team_index == 0 ? "RED" : "BLUE");
}

void ae_glue_pause_gametype(char *text, int size)
{
	struct game_variant *variant = game_engine_get_variant();
	wchar_t stored[NUMBEROF(variant->human_readable_game_description) + 1], display[48];

	text[0] = 0;
	if (!variant)
		return;
	csmemcpy(stored, variant->human_readable_game_description, sizeof(variant->human_readable_game_description));
	stored[NUMBEROF(stored) - 1] = 0;
	if (!arena_gametype_own_display_name_for_stored_name(stored, variant, display, NUMBEROF(display)))
		arena_gametype_display_name(stored, display, NUMBEROF(display));
	display[NUMBEROF(display) - 1] = 0;
	narrow(display, text, size);
	/* (0.7.0: a built-in variant launched by a test has no stored name: its mode's) */
	if (!text[0])
		snprintf(text, size, "%s", ae_mode_guard_mode_name(variant->game_engine_index));
}

/* ---------- the game card */

static void time_text(long seconds, char *text, int size)
{
	if (seconds < 0)
		seconds = 0;
	snprintf(text, size, "%ld:%02ld", seconds / 60, seconds % 60);
}

static const char *mode_name(struct ae_strip_snapshot const *snapshot)
{
	switch (snapshot->mode)
	{
	case _ae_strip_slayer: return "Slayer";
	case _ae_strip_ctf: return "Capture the Flag";
	case _ae_strip_oddball: return "Oddball";
	case _ae_strip_king: return "King of the Hill";
	case _ae_strip_race: return "Race";
	case _ae_strip_rally: return "Rally";
	case _ae_strip_juggernaut: return "Juggernaut";
	case _ae_strip_accumulation: return snapshot->reverse_tag ? "Reverse Tag" : "Accumulation";
	case _ae_strip_elimination: return "Elimination";
	case _ae_strip_phantoms: return "Phantoms";
	case _ae_strip_rounds: return "Rounds";
	}
	return "";
}

static void score_text(struct ae_strip_snapshot const *snapshot, struct ae_strip_side_snapshot const *side,
	char *text, int size)
{
	if (snapshot->mode == _ae_strip_rounds)
		snprintf(text, size, "%ld", side->rounds_won);
	else if (snapshot->timed || snapshot->mode == _ae_strip_oddball || snapshot->mode == _ae_strip_king ||
		snapshot->mode == _ae_strip_accumulation)
		time_text(side->score, text, size);
	else
		snprintf(text, size, "%ld", side->score);
}

void ae_glue_pause_card(short local_player, struct ae_pause_card *card)
{
	struct ae_strip_snapshot snapshot;
	struct ae_strip_side_snapshot const *sides[AE_PAUSE_CARD_ROWS];
	wchar_t map[48];
	char const *path = main_get_multiplayer_map_name();
	char goal[24];
	short row;

	memset(card, 0, sizeof(*card));
	if (!path || !path[0])
		path = main_get_map_name();
	ae_scoreboard_map_name(path, map, NUMBEROF(map));
	narrow(map, card->map, sizeof(card->map));
	ae_glue_pause_gametype(card->title, sizeof(card->title));
	if (!ae_strip_engine_snapshot(local_player, &snapshot))
		return;
	/* (the mode line: the mode, then the goal) */
	goal[0] = 0;
	if (snapshot.mode == _ae_strip_rounds && snapshot.rounds_to_win > 0)
		snprintf(goal, sizeof(goal), "first to %ld", snapshot.rounds_to_win);
	else if (snapshot.score_to_win > 0 && (snapshot.timed || snapshot.mode == _ae_strip_oddball ||
		snapshot.mode == _ae_strip_king || snapshot.mode == _ae_strip_accumulation))
	{
		char time[16];

		time_text(snapshot.score_to_win, time, sizeof(time));
		snprintf(goal, sizeof(goal), "first to %s", time);
	}
	else if (snapshot.score_to_win > 0)
		snprintf(goal, sizeof(goal), "first to %ld", snapshot.score_to_win);
	else
		snprintf(goal, sizeof(goal), "no score limit");
	snprintf(card->mode, sizeof(card->mode), "%s  \xC2\xB7  %s", mode_name(&snapshot), goal);
	/* (teams: red then blue; FFA: the leader, then the second, the player among them as the strip says) */
	if (snapshot.teams)
	{
		boolean me_red = snapshot.me.team_index == 0;

		sides[0] = me_red ? &snapshot.me : &snapshot.other;
		sides[1] = me_red ? &snapshot.other : &snapshot.me;
		for (row = 0; row < AE_PAUSE_CARD_ROWS; row++)
		{
			snprintf(card->names[row], sizeof(card->names[row]), "%s", row == 0 ? "RED" : "BLUE");
			card->colors[row] = row == 0 ? TEAM_RED : TEAM_BLUE;
			card->mine[row] = sides[row] == &snapshot.me;
		}
	}
	else
	{
		sides[0] = snapshot.i_lead ? &snapshot.me : &snapshot.other;
		sides[1] = snapshot.i_lead ? &snapshot.other : &snapshot.me;
		for (row = 0; row < AE_PAUSE_CARD_ROWS; row++)
		{
			char name[24], place[8];
			wchar_t wide_place[8];

			card->mine[row] = sides[row] == &snapshot.me;
			narrow(sides[row]->name, name, sizeof(name));
			ae_strip_format_place(sides[row]->place > 0 ? sides[row]->place : row + 1, wide_place, NUMBEROF(wide_place));
			narrow(wide_place, place, sizeof(place));
			snprintf(card->names[row], sizeof(card->names[row]), "%s  %s", place,
				card->mine[row] ? "YOU" : name[0] ? name : "-");
			card->colors[row] = card->mine[row] ? FFA_ME : FFA_OTHER;
		}
	}
	for (row = 0; row < AE_PAUSE_CARD_ROWS; row++)
		score_text(&snapshot, sides[row], card->scores[row], sizeof(card->scores[row]));
	card->rows = AE_PAUSE_CARD_ROWS;
	/* (FFA with nobody else in the game yet: the player's row alone) */
	if (!snapshot.teams && !snapshot.other.name[0])
	{
		card->rows = 1;
		if (!card->mine[0])
		{
			memcpy(card->names[0], card->names[1], sizeof(card->names[0]));
			memcpy(card->scores[0], card->scores[1], sizeof(card->scores[0]));
			card->colors[0] = card->colors[1];
			card->mine[0] = 1;
		}
	}
	/* (the foot: the round, or what the clock counts; the clock) */
	if (snapshot.mode == _ae_strip_rounds)
		snprintf(card->status, sizeof(card->status), "ROUND %ld",
			snapshot.me.rounds_won + snapshot.other.rounds_won + 1);
	else if (snapshot.clock_hidden)
		card->status[0] = 0;
	else
		snprintf(card->status, sizeof(card->status), "%s", snapshot.clock_counts_up ? "TIME PLAYED" : "TIME LEFT");
	if (!snapshot.clock_hidden)
		time_text(snapshot.clock_seconds, card->clock, sizeof(card->clock));
}

/* ---------- actions */

/* (issue #1: "an offline mode ... where pause pauses everything") an OFFLINE game (CREATE GAME's NETWORK: upstream's
split screen session, a server that takes no other machine) stops while paused. Only that one: a LAN or online game,
even with only this PC's players in it, keeps running (the client's connection would go stale and drop:
network_client_manager.c skips that check only in the split screen session). HALO_PAUSE_STOPS=0 keeps OFFLINE running */
int ae_glue_pause_offline(void)
{
	char const *setting = getenv("HALO_PAUSE_STOPS");

	return !(setting && !strcmp(setting, "0")) && network_game_is_splitscreen_local();
}

void ae_glue_pause_game_time(int paused)
{
	if (paused)
	{
		stopped.time = !game_time_get_paused();
		if (stopped.time)
			game_time_set_paused(TRUE);
		if (stopped.time && network_game_is_splitscreen_local() && !stopped.connections)
		{
			stopped.connections = TRUE;
			stopped.dont_timeout_before = global_connection_dont_timeout;
			global_connection_dont_timeout = TRUE;
		}
		sound_pause(TRUE);
		stopped.sound = TRUE;
		platform_log("ae pause: the game's time %s", stopped.time ? "stopped" : "was stopped already");
		return;
	}
	if (stopped.time && game_time_get_paused())
		game_time_set_paused(FALSE);
	if (stopped.sound)
		sound_pause(FALSE);
	if (stopped.connections)
	{
		global_connection_dont_timeout = stopped.dont_timeout_before;
		stopped.connections = FALSE;
	}
	if (stopped.time)
		platform_log("ae pause: the game's time runs again");
	stopped.time = stopped.sound = FALSE;
}

void ae_glue_pause_closed(short local_player)
{
	if (local_player_exists(local_player))
		player_control_inhibit_buttons(local_player, CLOSED_INHIBIT_FLAGS, TRUE);
}

int ae_glue_pause_change_teams(short local_player)
{
	if (!game_engine_running())
	{
		platform_log("ae pause: CHANGE TEAMS with no game running");
		return 0;
	}
	team_change_ask(local_player);
	return 1;
}

int ae_glue_pause_end_game(void)
{
	if (!global_network_game_server_get() || !game_engine_running())
	{
		platform_log("ae pause: END GAME not taken (not the host, or no game running)");
		return 0;
	}
	game_engine_end_game();
	return 1;
}

/* ---------- RESTART GAME and RESTART ROUND (0.7.0, the host's; the owner: the map and options picked in the pause
want them). A restart is END GAME, then the post-game left at once (CONTINUE: the server back to the pregame), then the
lobby applies the pause's picks and starts the next game itself (ae_screen_create_game.c): every machine goes through
the usual end and start. A restart not taken within RESTART_MS is dropped (the session closed meanwhile). */

enum { RESTART_NONE, RESTART_ENDED, RESTART_IN_LOBBY, RESTART_MS = 60000 };
static int restart_state;
static unsigned long restart_at;

#ifdef HALO_GAME_BROWSER
boolean ae_bomb_restart_round(void);
#endif

int ae_glue_pause_restart_game(void)
{
	if (!ae_glue_pause_end_game())
		return 0;
	restart_state = RESTART_ENDED;
	restart_at = system_milliseconds();
	platform_log("ae pause: RESTART GAME: the game ended, the next starts from the lobby");
	return 1;
}

void ae_glue_pause_restart_tick(void)
{
	if (restart_state != RESTART_NONE && system_milliseconds() - restart_at > RESTART_MS)
	{
		platform_log("ae pause: RESTART GAME dropped (not under way after %d s)", RESTART_MS / 1000);
		restart_state = RESTART_NONE;
	}
	if (restart_state == RESTART_ENDED && game_engine_showing_postgame() && ae_glue_postgame_continue())
		restart_state = RESTART_IN_LOBBY;
}

int ae_glue_pause_restart_take(void)
{
	if (restart_state != RESTART_IN_LOBBY)
		return 0;
	restart_state = RESTART_NONE;
	return 1;
}

int ae_glue_pause_restart_round(void)
{
	if (!global_network_game_server_get() || !game_engine_running() || !bomb_rounds() ||
#ifdef HALO_GAME_BROWSER
		!ae_bomb_restart_round())
#else
		TRUE)
#endif
	{
		platform_log("ae pause: RESTART ROUND not taken (not the host, no round game, or between rounds)");
		return 0;
	}
	platform_log("ae pause: RESTART ROUND: the round starts again");
	return 1;
}

void ae_glue_pause_quit(short local_player, int game)
{
	short controller;

	switch (game)
	{
	case AE_PAUSE_CAMPAIGN:
		/* (a multiplayer map played alone is not saved: it never takes the campaign's saved game's place) */
		if (main_get_current_solo_level() != NONE)
			game_state_save_to_persistent_storage();
		main_goto_main_menu();
		return;
	case AE_PAUSE_ALONE:
		main_goto_main_menu();
		return;
	case AE_PAUSE_COOP:
		for (controller = 0; controller < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; controller++)
			network_game_client_local_player_quit(controller);
		return;
	}
	if (!global_network_game_client_get())
	{
		main_goto_main_menu();
		return;
	}
	network_game_client_local_player_quit(local_player);
	/* (a split screen player who quit, the others staying, is not joined to the next game) */
	if (local_player_count() > 1)
		player_ui_local_player_left_multiplayer_game(local_player);
}

void ae_glue_pause_revert(void)
{
	/* (in co-op this would revert only this machine: the classic refuses it there, and so does the menu's rows) */
	if (network_coop_active())
		return;
	main_revert_map();
}

int ae_glue_pause_friendly_fire(void)
{
	return config_boolean("network.coop_own_friendly_fire") != 0;
}

void ae_glue_pause_friendly_fire_switch(void)
{
	if (!config_write("network.coop_own_friendly_fire", ae_glue_pause_friendly_fire() ? "false" : "true"))
		platform_log("ae pause: could not write network.coop_own_friendly_fire to config.toml");
}
