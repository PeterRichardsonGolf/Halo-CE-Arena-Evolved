/*
AE_SCREEN_PAUSE.C

The AE pause menu (ae_screen_pause.h; the approved design). The rows: ae_pause_rows.h; what each does:
ae_glue_pause.h (the classic pause's engine calls) and the AE screens it opens (SETTINGS for the player, GAME OPTIONS
and CHANGE MAP for the next game, through CREATE GAME's copy).

One screen, anyone's (start_from_anyone): each local player's pause is a seat of it. A player with no pause open is
playing: only their START reaches it (it opens theirs); their other presses are the game's and are dropped here. The
screens a pause opens (SETTINGS, GAME OPTIONS, CHANGE MAP, dialogs) are its player's own (their owner set), so the
others' presses never drive them.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "../src/ae_draw.h"
#include "../src/ae_layout.h"
#include "ae_bot_settings.h"
#include "ae_glue_lobby.h"
#include "ae_glue_pause.h"
#include "ae_hooks.h"
#include "ae_pause_rows.h"
#include "ae_screen_create_game.h"
#include "ae_screen_game_options.h"
#include "ae_screen_pause.h"
#include "ae_screen_settings.h"
#include "ae_team_override.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	PAUSE_HIT = 0x7D00,
	/* (each seat's own: hits are the whole frame's) */
	HIT_ROWS = PAUSE_HIT + 1, HIT_PROMPTS = PAUSE_HIT + 0x10, HIT_SEAT_STEP = 2,
	SEATS = 4,
	LIST_ROWS = 12,
	PROMPTS = 3,
	/* a press of B or START this soon after a pause opened is the press that opened it (the keyboard's Escape is the
	pause key and BACK both, ms) */
	OPEN_GRACE_MS = 250
};
enum { PAGE_MAIN, PAGE_BOTS, PAGE_PLAYERS };

/* the player colours (the gallery's and SETTINGS'), by local player */
static unsigned int const player_colors[SEATS] = { 0xC0392BFFu, 0x2E86DEFFu, 0x7FA035FFu, 0xE6B422FFu };
/* the paused view's scrim under the panel (the mockups': one player 140 / 255, split screen 110 / 255) */
#define SCRIM_ONE 0x04070A8Cu
#define SCRIM_SPLIT 0x04070A6Eu
/* behind a full screen the pause opened: the list fade's tone (AE_COLOR_FADE), .92 */
#define BACKDROP AE_COLOR_FADE
#define BOTS_PAGE_HELP "How many bots play, 31 or fewer best: at once, in this game too."

/* the one-player layout (B), in 1080p pixels of the 16:9 frame: the list panel's left and its width at 100 % (wider
than the mockup's 552, owner 10-25: values like AS BOT SKILL and the gametype's name were cut; the view panel's text
scale tops out at its cap, so the rows gain the room; at a bigger UI SCALE it grows to the right, at most by
B_PANEL_GROWTH: at 130 % in full the card narrowed to 520 and cut the gametype's name), the card's right and top, its
least gap from the panel (its height from what it holds) */
#define B_PANEL_X 124.0f
#define B_PANEL_WIDTH 690.0f
#define B_PANEL_GROWTH 1.15f
#define B_PANEL_SHARE 0.46f
/* layout B's text scale at 1080 lines: the view panel's text keeps this share of the screen at any height (owner 10-26:
on a 1536x864 screen the view scale's 1.6 cap kept 1080p's pixel sizes in a panel the screen had shrunk, and values cut;
then "scale it down a bit more": 1.4, with the panel narrowed in step from 780) */
#define B_TEXT_SCALE_1080 1.4f
#define CARD_RIGHT 1700.0f
#define CARD_GAP 40.0f
#define CARD_TOP 250.0f
#define CARD_WIDTH_U 700.0f
/* the card's sizes (u; the mockup's at 1.6 x its kit's) */
#define CARD_STRIPE_U 4.8f
/* (ae_widgets_nav.c's: the view panel's stripe) */
#define PANEL_STRIPE_U 3.0f
#define CARD_PAD_U 28.8f
#define CARD_MAP_U 24.0f
#define CARD_TITLE_U 41.6f
#define CARD_MODE_U 27.2f
#define CARD_NAME_U 35.2f
#define CARD_SCORE_U 48.0f
#define CARD_FOOT_U 27.2f
#define CARD_CLOCK_U 35.2f
#define CARD_TEAM_BAR_U 9.6f
#define CARD_TEAM_HEIGHT_U 54.4f
#define CARD_TEAM_PITCH_U 73.6f
#define CARD_RULE_U 1.6f
#define CARD_TRACKING 0.09f
/* (the text's top to the first rule; the rule to the first score row; the last row to the foot's rule, that rule to
the foot line's middle, its middle to the card's foot) */
#define CARD_RULES_U 172.8f
#define CARD_ROWS_ABOVE_U 25.6f
#define CARD_FOOT_RULE_U 25.6f
#define CARD_FOOT_LINE_U 38.4f
#define CARD_FOOT_BELOW_U 56.8f

struct seat
{
	int open;
	int stopped;                  /* (spike) this pause stopped an offline multiplayer game */
	int game;
	short page;
	int scoreboard;
	unsigned long opened_ms;
	struct ae_pause_facts facts;
	short rows[AE_PAUSE_ROWS_MAXIMUM];
	short count;
	/* (0.7.4) the last row (QUIT) pinned under the list, apart: the list holds the rows before it, and the focus is
	on it (pin_focus) or on the list's own */
	short pinned, list_count;
	int pin_focus;
	struct ae_list_view list;
	short bot_rows[AE_BOT_SETTINGS];
	short bot_count;
	struct ae_list_view bots;
	/* (0.7.4 PLAYERS) the page's list; PLAYERS' value on the main page */
	struct ae_list_view players;
	short players_count;
	char players_text[8];
	/* the move this seat's dialog asks about (a seat's own: two seats of the host PC can ask at once), and what came of
	the last one, shown in the page's help for a few seconds */
	short move_machine, move_controller, move_to_team;
	char move_name[AE_LOBBY_NAME_SIZE], move_title[96], move_body[320], move_note[120];
	unsigned long move_note_until;
	/* the values shown (read as the panel draws) */
	char team[16], gametype[48];
	/* the row whose dialog is open, -1 none */
	short confirm_row;
	/* as last drawn (layout units): the view, the panel, the list's density */
	struct ae_rect view, panel;
	struct ae_density density;
	int drawn;
	struct ae_prompt prompts[PROMPTS];
	short prompt_count;
};

static struct
{
	struct seat seats[SEATS];
	int on_stack;
	/* CHANGE MAP's pick, waiting for its screen to close before END GAME asks: the seat, -1 none */
	short map_seat;
	short map_index;
	char map_name[64];
	/* a full screen the pause opened is up over it (SETTINGS, GAME OPTIONS, CHANGE MAP): the game under it goes
	behind a backdrop (those screens have the main menu's video behind them, here the game and its HUD) */
	int screen_over;
	/* (split screen) whose view that screen draws in (ae_draw_inset): the player who opened it */
	short screen_player;
} pause = { .map_seat = -1 };

static struct ae_screen_class const pause_class;
static void pause_unowned(struct ae_event const *event);

static const char *game_name(int game)
{
	switch (game)
	{
	case AE_PAUSE_CAMPAIGN: return "campaign";
	case AE_PAUSE_COOP: return "co-op";
	case AE_PAUSE_ALONE: return "alone";
	}
	return "multiplayer";
}

/* (spike) an offline multiplayer game stopped by the pauses open */
static int offline_stopped;

static int any_open(void)
{
	short index;

	for (index = 0; index < SEATS; index++)
	{
		if (pause.seats[index].open)
			return 1;
	}
	return 0;
}

/* ---------- PLAYERS (0.7.4): the host puts a player or a bot on the other team */

/* the page's rows: a team's header, then its players (the roster's, each with its team's colour on the row's edge) */
struct player_entry
{
	int header, bot, host, local, last;
	short team, machine, controller;
	char name[AE_LOBBY_NAME_SIZE];
};
static struct
{
	struct player_entry entries[AE_LOBBY_MAXIMUM_PLAYERS + 2];
	short count;
	struct ae_lobby_roster roster;
} listed;

#define TEAM_RED 0xC0392BFFu
#define TEAM_BLUE 0x2E86DEFFu

static char const *team_word(int team)
{
	return team ? "BLUE" : "RED";
}

/* the roster as the page's rows (read again as the page draws: a bot or a player may have joined or left) */
static void players_read(void)
{
	short team, index;

	listed.count = 0;
	if (!ae_lobby_roster(&listed.roster).ok)
		return;
	for (team = 0; team < 2; team++)
	{
		short first = listed.count;

		listed.entries[listed.count].header = 1;
		listed.entries[listed.count++].team = team;
		for (index = 0; index < listed.roster.count; index++)
		{
			struct ae_lobby_player const *player = &listed.roster.players[index];
			struct player_entry *entry = &listed.entries[listed.count];

			if (player->team != team)
				continue;
			memset(entry, 0, sizeof(*entry));
			entry->team = team;
			entry->bot = player->bot;
			entry->host = player->host;
			entry->local = player->local;
			entry->machine = player->machine;
			entry->controller = player->controller;
			snprintf(entry->name, sizeof(entry->name), "%s", player->name);
			entry->last = ae_team_override_last_of_team(player->machine, player->controller);
			listed.count++;
		}
		/* (a team nobody is on: no header either) */
		if (listed.count == first + 1)
			listed.count = first;
	}
}

static int players_header(short index)
{
	return index >= 0 && index < listed.count && listed.entries[index].header;
}

/* a focus that landed on a header moved on the way it was going, else back */
static short players_skip(short focus, short direction)
{
	short step = direction < 0 ? -1 : 1, at;

	if (focus < 0 || focus >= listed.count || !players_header(focus))
		return focus;
	for (at = (short)(focus + step); at >= 0 && at < listed.count; at = (short)(at + step))
		if (!players_header(at))
			return at;
	for (at = (short)(focus - step); at >= 0 && at < listed.count; at = (short)(at - step))
		if (!players_header(at))
			return at;
	return focus;
}

/* ---------- the rows */

static void rows_read(struct seat *seat, short player)
{
	short focus = seat->list.list.focus, count, index;

	ae_glue_pause_facts(player, seat->game, &seat->facts);
	count = ae_pause_rows(&seat->facts, seat->rows, AE_PAUSE_ROWS_MAXIMUM);
	seat->pinned = ae_pause_row_pinned(seat->rows, count);
	if (!seat->pinned)
		seat->pin_focus = 0;
	if (count != seat->count)
	{
		seat->count = count;
		seat->list_count = (short)(count - seat->pinned);
		ae_list_view_init(&seat->list, seat->list_count, LIST_ROWS);
		ae_list_set_focus(&seat->list.list, focus >= 0 && focus < seat->list_count ? focus : 0);
	}
	seat->list.list.focus = ae_pause_row_skip(seat->rows, seat->list_count, seat->list.list.focus, 1);
	/* (BOTS: the count and the skill, and with bots the four LOBBY SETUP has; BOT TEAMS in a team game) */
	count = 0;
	seat->bot_rows[count++] = AE_BOT_COUNT;
	seat->bot_rows[count++] = AE_BOT_SKILL;
	if (ae_bot_count() > 0)
	{
		seat->bot_rows[count++] = AE_BOT_SHOOTING;
		seat->bot_rows[count++] = AE_BOT_MOVEMENT;
		seat->bot_rows[count++] = AE_BOT_PLAYFIGHT;
		if (seat->facts.teams)
			seat->bot_rows[count++] = AE_BOT_TEAMS;
	}
	if (count != seat->bot_count)
	{
		focus = seat->bots.list.focus;
		seat->bot_count = count;
		ae_list_view_init(&seat->bots, count, LIST_ROWS);
		ae_list_set_focus(&seat->bots.list, focus >= 0 && focus < count ? focus : 0);
	}
	for (index = 0; index < seat->count; index++)
	{
		if (seat->rows[index] == AE_PAUSE_TEAMS)
			ae_glue_pause_team(player, seat->team, sizeof(seat->team));
		else if (seat->rows[index] == AE_PAUSE_OPTIONS)
			ae_glue_pause_gametype(seat->gametype, sizeof(seat->gametype));
		else if (seat->rows[index] == AE_PAUSE_PLAYERS)
		{
			/* (the page's rows too: its list keeps its focus while the roster changes under it) */
			players_read();
			snprintf(seat->players_text, sizeof(seat->players_text), "%d", (int)listed.roster.count);
			if (listed.count != seat->players_count)
			{
				focus = seat->players.list.focus;
				seat->players_count = listed.count;
				ae_list_view_init(&seat->players, listed.count, LIST_ROWS);
				ae_list_set_focus(&seat->players.list, focus >= 0 && focus < listed.count ? focus : 0);
			}
			seat->players.list.focus = players_skip(seat->players.list.focus, 1);
		}
	}
}

static int focused_row(struct seat const *seat)
{
	short focus = seat->list.list.focus;

	if (seat->pin_focus && seat->pinned)
		return seat->rows[seat->count - 1];
	return focus >= 0 && focus < seat->list_count ? seat->rows[focus] : -1;
}

static const char *row_value(struct seat const *seat, int row)
{
	switch (row)
	{
	case AE_PAUSE_TEAMS: return seat->team[0] ? seat->team : NULL;
	case AE_PAUSE_OPTIONS: return seat->gametype[0] ? seat->gametype : NULL;
	case AE_PAUSE_BOTS: return ae_bot_setting_text(AE_BOT_COUNT);
	case AE_PAUSE_PLAYERS: return seat->players_text[0] ? seat->players_text : NULL;
	case AE_PAUSE_FRIENDLY_FIRE: return ae_glue_pause_friendly_fire() ? "ON" : "OFF";
	}
	return NULL;
}

/* ---------- opening and closing */

static short seat_player(struct seat const *seat)
{
	return (short)(seat - pause.seats);
}

static short seat_hit_rows(struct seat const *seat)
{
	return (short)(HIT_ROWS + HIT_SEAT_STEP * (seat - pause.seats));
}

/* (0.7.4) the pinned row's hit: the seat's next id */
static short seat_hit_pinned(struct seat const *seat)
{
	return (short)(seat_hit_rows(seat) + 1);
}

static short seat_hit_prompts(struct seat const *seat)
{
	return (short)(HIT_PROMPTS + HIT_SEAT_STEP * (seat - pause.seats));
}

/* the seat's pause shut (its player plays again); the screen goes with the last */
static void seat_close(struct seat *seat, int logged)
{
	short player = seat_player(seat);

	if (!seat->open)
		return;
	seat->open = 0;
	seat->scoreboard = 0;
	seat->page = PAGE_MAIN;
	ae_glue_pause_closed(player);
	/* (an offline multiplayer game: the time runs again once no one's pause is open) */
	if (seat->game == AE_PAUSE_CAMPAIGN || (offline_stopped && !any_open()))
	{
		offline_stopped = 0;
		ae_glue_pause_game_time(0);
	}
	if (logged)
		platform_log("ae pause: player %d resumes", player + 1);
	if (!any_open() && pause.on_stack)
	{
		pause.on_stack = 0;
		/* (with whatever it opened: its dialogs, SETTINGS) */
		while (ae_ui_holds(&pause) && ae_ui_top())
		{
			if (ae_ui_top()->data == &pause)
			{
				ae_ui_remove(&pause);
				break;
			}
			if (!ae_ui_top()->data || !ae_ui_remove(ae_ui_top()->data))
				ae_ui_pop();
		}
	}
}

static void seat_open(short player, int game)
{
	struct seat *seat = &pause.seats[player];

	memset(seat, 0, sizeof(*seat));
	seat->open = 1;
	seat->game = game;
	seat->confirm_row = -1;
	seat->opened_ms = system_milliseconds();
	seat->count = -1;
	seat->bot_count = -1;
	rows_read(seat, player);
	ae_list_set_focus(&seat->list.list, 0);
	seat->list.previous_focus = 0;
	if (game == AE_PAUSE_CAMPAIGN)
		ae_glue_pause_game_time(1);
	else if (ae_glue_pause_offline())
	{
		seat->stopped = 1;
		/* (once: a second player's pause in split screen finds it stopped) */
		if (!offline_stopped)
			ae_glue_pause_game_time(1);
		offline_stopped = 1;
	}
	platform_log("ae pause: open player %d (%s%s)", player + 1, game_name(game), seat->stopped ? ", offline: stopped" : "");
}

int ae_screen_pause_open(short local_player, int game)
{
	if (local_player < 0 || local_player >= SEATS)
		return 0;
	/* (over the game only: another AE screen up is not the pause's) */
	if (ae_ui_depth() && !ae_ui_holds(&pause))
		return 0;
	if (!pause.on_stack)
	{
		ae_ui_set_unowned(pause_unowned);
		memset(&pause.seats, 0, sizeof(pause.seats));
		pause.map_seat = -1;
		if (!ae_ui_push(&pause_class, AE_OWNER_ANY, &pause))
			return 0;
		pause.on_stack = 1;
	}
	seat_open(local_player, game);
	ae_sound_request(AE_SOUND_FORWARD, 0);
	return 1;
}

int ae_screen_pause_holds(short local_player)
{
	return pause.on_stack && local_player >= 0 && local_player < SEATS && pause.seats[local_player].open;
}

int ae_screen_pause_scoreboard(short local_player)
{
	return ae_screen_pause_holds(local_player) && pause.seats[local_player].scoreboard;
}

int ae_screen_pause_leaves_pointer(void)
{
	return pause.on_stack && ae_ui_top() && ae_ui_top()->data == &pause && !pause.seats[0].open;
}

/* ---------- the rows' actions */

/* a screen the pause opened is its player's alone */
static void own_top(short player)
{
	struct ae_screen *top = ae_ui_top();

	if (top && top->data != &pause)
	{
		top->owner = player;
		pause.screen_over = 1;
		pause.screen_player = player;
	}
}

static void players_moved(struct seat *seat);
static void confirmed(short choice, void *context);

static void confirmed(short choice, void *context)
{
	struct seat *seat = context;
	short player = seat_player(seat);
	int row = seat->confirm_row, game = seat->game;

	seat->confirm_row = -1;
	if (choice != 1 || !seat->open)
		return;
	platform_log("ae pause: player %d %s confirmed", player + 1, ae_pause_row_label(row, &seat->facts));
	switch (row)
	{
	case AE_PAUSE_END:
	case AE_PAUSE_MAP:
		if (row == AE_PAUSE_MAP)
			ae_screen_create_game_next_map(pause.map_index, pause.map_name);
		if (ae_glue_pause_end_game())
		{
			short index;

			for (index = 0; index < SEATS; index++)
				seat_close(&pause.seats[index], 0);
		}
		break;
	case AE_PAUSE_QUIT:
		seat_close(seat, 0);
		ae_glue_pause_quit(player, game);
		break;
	case AE_PAUSE_REVERT:
		seat_close(seat, 0);
		ae_glue_pause_revert();
		break;
	case AE_PAUSE_RESTART_GAME:
	case AE_PAUSE_RESTART_ROUND:
		if (row == AE_PAUSE_RESTART_GAME ? ae_glue_pause_restart_game() : ae_glue_pause_restart_round())
		{
			short index;

			for (index = 0; index < SEATS; index++)
				seat_close(&pause.seats[index], 0);
		}
		break;
	case AE_PAUSE_PLAYERS:
		players_moved(seat);
		break;
	case AE_PAUSE_TEAMS:
		/* (the classic's: asked, and the pause closes either way; team_change.c says what came of it) */
		ae_glue_pause_change_teams(player);
		seat_close(seat, 1);
		break;
	}
}

/* the move confirmed: team_change_move through the override, one chat line to everyone, the page read again */
static void players_moved(struct seat *seat)
{
	char text[96];
	char const *host = "";
	int result, index;

	for (index = 0; index < listed.roster.count; index++)
		if (listed.roster.players[index].host && !listed.roster.players[index].bot)
			host = listed.roster.players[index].name;
	/* (the chat line and the moved player's notice are the override's: at once, or when a round game's death makes the move) */
	result = ae_team_override_game(seat->move_machine, seat->move_controller, seat->move_to_team, seat->move_name, host,
		text, sizeof(text));
	platform_log("ae pause: player %d moved '%s' to the %s team: %s", seat_player(seat) + 1, seat->move_name,
		team_word(seat->move_to_team), text);
	seat->move_note_until = system_milliseconds() + 4000;
	if (result < 0 || result > 1)
	{
		/* (refused: the page says why in its help for a few seconds) */
		snprintf(seat->move_note, sizeof(seat->move_note), "Can't move %s: %s.", seat->move_name, text);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		rows_read(seat, seat_player(seat));
		return;
	}
	if (result == 1)
		snprintf(seat->move_note, sizeof(seat->move_note), "%s joins the %s Team at their next death.", seat->move_name,
			seat->move_to_team ? "Blue" : "Red");
	else
		seat->move_note[0] = 0;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	rows_read(seat, seat_player(seat));
}

/* A, left or right on a player: ask first (it kills them); the last of a team is refused */
static void players_ask(struct seat *seat)
{
	short focus = seat->players.list.focus;
	struct player_entry const *entry;
	struct ae_dialog_spec spec;
	char const *to;

	if (focus < 0 || focus >= listed.count || listed.entries[focus].header)
		return;
	entry = &listed.entries[focus];
	if (entry->last)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (seat->confirm_row >= 0)
		return;
	to = team_word(entry->team ? 0 : 1);
	seat->move_machine = entry->machine;
	seat->move_controller = entry->controller;
	seat->move_to_team = entry->team ? 0 : 1;
	snprintf(seat->move_name, sizeof(seat->move_name), "%s", entry->name);
	snprintf(seat->move_title, sizeof(seat->move_title), "Move %s to %s?", entry->name, to);
	snprintf(seat->move_body, sizeof(seat->move_body), "They die now, not counted, and respawn on the %s Team. Their score "
		"stays theirs. Round games: at their next death.", seat->move_to_team ? "Blue" : "Red");
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = seat->move_title;
	spec.body = seat->move_body;
	spec.choices[0] = ae_string(AE_STR_CANCEL_CAPS);
	spec.choices[1] = "MOVE";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.density = seat->density;
	spec.bounds = seat->panel;
	spec.dim = seat->panel;
	spec.view = seat->view;
	spec.picked = confirmed;
	spec.context = seat;
	seat->confirm_row = AE_PAUSE_PLAYERS;
	if (!ae_dialog_open(&spec, seat_player(seat)))
	{
		seat->confirm_row = -1;
		ae_sound_request(AE_SOUND_FAILURE, 0);
	}
}

/* the row's dialog, in the seat's panel under its header */
static void confirm(struct seat *seat, int row)
{
	struct ae_dialog_spec spec;
	const char *title, *body, *yes;

	if (!ae_pause_row_confirm(row, &seat->facts, &title, &body, &yes))
		return;
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = title;
	spec.body = body;
	spec.choices[0] = ae_string(AE_STR_CANCEL_CAPS);
	spec.choices[1] = yes;
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.density = seat->density;
	spec.bounds = seat->panel;
	spec.dim = seat->panel;
	spec.view = seat->view;
	spec.picked = confirmed;
	spec.context = seat;
	seat->confirm_row = (short)row;
	if (!ae_dialog_open(&spec, seat_player(seat)))
	{
		seat->confirm_row = -1;
		ae_sound_request(AE_SOUND_FAILURE, 0);
	}
}

static void map_picked(short list_index, char const *name)
{
	short index;

	/* (the seat that opened CHANGE MAP: the one whose pause holds it, player order) */
	for (index = 0; index < SEATS && pause.map_seat < 0; index++)
	{
		if (pause.seats[index].open && pause.seats[index].facts.host)
			pause.map_seat = index;
	}
	pause.map_index = list_index;
	snprintf(pause.map_name, sizeof(pause.map_name), "%s", name ? name : "");
	platform_log("ae pause: next map '%s' picked", pause.map_name);
}

static void row_accept(struct seat *seat, int row)
{
	short player = seat_player(seat);

	if (row < 0 || ae_pause_row_look(row) == AE_PAUSE_LOOK_HEADER)
		return;
	platform_log("ae pause: player %d %s", player + 1, ae_pause_row_label(row, &seat->facts));
	/* (another player's SETTINGS, GAME OPTIONS or CHANGE MAP is open: a screen or dialog of this one's would go over
	it; the rest of this pause works) */
	if (pause.screen_over && row != AE_PAUSE_RESUME && row != AE_PAUSE_SCOREBOARD && row != AE_PAUSE_BOTS &&
		row != AE_PAUSE_FRIENDLY_FIRE)
	{
		platform_log("ae pause: player %d %s waits: player %d's screen is open", player + 1,
			ae_pause_row_label(row, &seat->facts), pause.screen_player + 1);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	switch (row)
	{
	case AE_PAUSE_RESUME:
		ae_sound_request(AE_SOUND_BACK, 0);
		seat_close(seat, 1);
		return;
	case AE_PAUSE_SETTINGS:
		if (!ae_screen_settings_open(player))
			break;
		own_top(player);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	case AE_PAUSE_SCOREBOARD:
		seat->scoreboard = 1;
		ae_sound_request(AE_SOUND_FORWARD, 0);
		platform_log("ae pause: player %d scoreboard", player + 1);
		return;
	case AE_PAUSE_OPTIONS:
		if (!ae_screen_game_options_show())
			break;
		own_top(player);
		ae_screen_create_game_next_options();
		ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	case AE_PAUSE_MAP:
		pause.map_seat = -1;
		if (!ae_screen_create_game_pick_next_map(map_picked))
			break;
		own_top(player);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		return;
	case AE_PAUSE_BOTS:
		seat->page = PAGE_BOTS;
		ae_list_set_focus(&seat->bots.list, 0);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		platform_log("ae pause: player %d bots page", player + 1);
		return;
	case AE_PAUSE_PLAYERS:
		seat->page = PAGE_PLAYERS;
		rows_read(seat, player);
		ae_list_set_focus(&seat->players.list, players_skip(0, 1));
		ae_sound_request(AE_SOUND_FORWARD, 0);
		platform_log("ae pause: player %d players page (%d)", player + 1, (int)listed.roster.count);
		return;
	case AE_PAUSE_FRIENDLY_FIRE:
		ae_glue_pause_friendly_fire_switch();
		ae_sound_request(AE_SOUND_CURSOR, 0);
		return;
	/* (CHANGE TEAMS asks too: it kills the player, owner 2026-10-25) */
	case AE_PAUSE_TEAMS:
	case AE_PAUSE_END:
	case AE_PAUSE_RESTART_GAME:
	case AE_PAUSE_RESTART_ROUND:
	case AE_PAUSE_QUIT:
	case AE_PAUSE_REVERT:
		ae_sound_request(AE_SOUND_FORWARD, 0);
		confirm(seat, row);
		return;
	}
	ae_sound_request(AE_SOUND_FAILURE, 0);
}

/* ---------- input */

static void seat_event(struct seat *seat, struct ae_event const *event)
{
	short player = seat_player(seat);
	int row;

	/* (B or START: the press that opened it, from the keyboard's Escape that is both) */
	if ((event->action == AE_ACTION_BACK || event->action == AE_ACTION_START) &&
		system_milliseconds() - seat->opened_ms < OPEN_GRACE_MS)
	{
		return;
	}
	if (seat->scoreboard)
	{
		if (event->action == AE_ACTION_BACK || event->action == AE_ACTION_START || event->action == AE_ACTION_ACCEPT ||
			event->action == AE_ACTION_SELECT)
		{
			seat->scoreboard = 0;
			ae_sound_request(AE_SOUND_BACK, 0);
		}
		return;
	}
	if (seat->page == PAGE_BOTS)
	{
		short focus = seat->bots.list.focus;
		int kind = focus >= 0 && focus < seat->bot_count ? seat->bot_rows[focus] : -1;

		switch (event->action)
		{
		case AE_ACTION_BACK:
			seat->page = PAGE_MAIN;
			ae_sound_request(AE_SOUND_BACK, 0);
			return;
		case AE_ACTION_START:
			ae_sound_request(AE_SOUND_BACK, 0);
			seat_close(seat, 1);
			return;
		case AE_ACTION_LEFT:
		case AE_ACTION_RIGHT:
		case AE_ACTION_ACCEPT:
			/* (the bot count: one step a press. Each step adds or removes a bot at once, and a held direction's
			repeat (80 ms, then 40) stepped it from 5 to 13 in a second: 8 bots. The two-valued rows, PLAYFIGHT above all,
			flipped back and forth in a held press the same way) */
			if (kind >= 0 && !(event->repeat && (kind == AE_BOT_COUNT || kind == AE_BOT_PLAYFIGHT || kind == AE_BOT_TEAMS)))
				ae_bot_setting_step(kind, event->action == AE_ACTION_LEFT ? -1 : 1, "ae pause");
			rows_read(seat, player);
			return;
		default:
			ae_list_view_event(&seat->bots, event);
			return;
		}
	}
	if (seat->page == PAGE_PLAYERS)
	{
		switch (event->action)
		{
		case AE_ACTION_BACK:
			seat->page = PAGE_MAIN;
			ae_sound_request(AE_SOUND_BACK, 0);
			return;
		case AE_ACTION_START:
			ae_sound_request(AE_SOUND_BACK, 0);
			seat_close(seat, 1);
			return;
		case AE_ACTION_LEFT:
		case AE_ACTION_RIGHT:
		case AE_ACTION_ACCEPT:
			players_ask(seat);
			return;
		case AE_ACTION_UP:
		case AE_ACTION_DOWN:
		case AE_ACTION_PAGE_UP:
		case AE_ACTION_PAGE_DOWN:
		{
			short direction = event->action == AE_ACTION_UP || event->action == AE_ACTION_PAGE_UP ? -1 : 1;

			ae_list_view_event(&seat->players, event);
			ae_list_set_focus(&seat->players.list, players_skip(seat->players.list.focus, direction));
			return;
		}
		default:
			return;
		}
	}
	row = focused_row(seat);
	switch (event->action)
	{
	case AE_ACTION_BACK:
	case AE_ACTION_START:
		ae_sound_request(AE_SOUND_BACK, 0);
		seat_close(seat, 1);
		return;
	case AE_ACTION_ACCEPT:
		row_accept(seat, row);
		return;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (ae_pause_row_look(row) == AE_PAUSE_LOOK_VALUE)
			row_accept(seat, row);
		return;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
	case AE_ACTION_PAGE_UP:
	case AE_ACTION_PAGE_DOWN:
	{
		short direction = event->action == AE_ACTION_UP || event->action == AE_ACTION_PAGE_UP ? -1 : 1;

		short before = seat->list.list.focus;

		/* (0.7.4) QUIT pinned under the list: down from the list's last row goes to it, up from it back to that row) */
		if (seat->pin_focus)
		{
			if (direction < 0)
				seat->pin_focus = 0;
			return;
		}
		ae_list_view_event(&seat->list, event);
		ae_list_set_focus(&seat->list.list, ae_pause_row_skip(seat->rows, seat->list_count, seat->list.list.focus,
			direction));
		if (seat->pinned && direction > 0 && seat->list.list.focus == before)
		{
			seat->pin_focus = 1;
			ae_sound_request(AE_SOUND_CURSOR, 0);
		}
		return;
	}
	default:
		return;
	}
}

static int pause_handle(struct ae_screen *screen, struct ae_event const *event)
{
	(void)screen;
	if (event->player < 0 || event->player >= SEATS)
		return 1;
	if (pause.seats[event->player].open)
	{
		seat_event(&pause.seats[event->player], event);
		return 1;
	}
	/* (a player playing on: their START pauses them too, in a network game, where each pauses alone; anything else of
	theirs is the game's) */
	if (event->action == AE_ACTION_START)
	{
		short index;
		int game = -1;

		for (index = 0; index < SEATS; index++)
		{
			if (pause.seats[index].open)
				game = pause.seats[index].game;
		}
		if ((game == AE_PAUSE_MULTIPLAYER || game == AE_PAUSE_COOP) && ae_glue_pause_still_on(event->player, game))
		{
			seat_open(event->player, game);
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
	}
	return 1;
}

/* (split screen, one player's SETTINGS, GAME OPTIONS or CHANGE MAP the top screen: the other players' presses, dropped
there, still drive their own pauses, and their START still opens one) */
static void pause_unowned(struct ae_event const *event)
{
	if (pause.on_stack && pause.screen_over && event->player != pause.screen_player)
		pause_handle(NULL, event);
}

static void pause_pointer(struct ae_screen *screen, struct ae_pointer const *pointer)
{
	struct seat *seat = &pause.seats[0];
	struct ae_event event;
	short item;

	(void)screen;
	/* (the pointer is player 1's: their panel's, when it is open and drawn) */
	if (!seat->open || seat->scoreboard || !seat->drawn || pointer->player != 0)
		return;
	if (ae_prompts_pointer(seat->prompts, seat->prompt_count, pointer, seat_hit_prompts(seat), &event))
	{
		seat_event(seat, &event);
		return;
	}
	if (seat->page == PAGE_BOTS)
	{
		short focused = seat->bots.list.focus;

		item = ae_list_view_pointer(&seat->bots, pointer, seat_hit_rows(seat));
		/* (a click on a row that was not the focused one only focuses it: a stray click kills PLAYFIGHT's fights otherwise) */
		if (item >= 0 && item < seat->bot_count && item == focused)
			ae_bot_setting_step(seat->bot_rows[item], 1, "ae pause");
		return;
	}
	if (seat->page == PAGE_PLAYERS)
	{
		item = ae_list_view_pointer(&seat->players, pointer, seat_hit_rows(seat));
		ae_list_set_focus(&seat->players.list, players_skip(seat->players.list.focus, 1));
		if (item >= 0 && item < listed.count && !listed.entries[item].header)
		{
			ae_list_set_focus(&seat->players.list, item);
			players_ask(seat);
		}
		return;
	}
	if (seat->pinned)
	{
		struct ae_hit hit;

		if (ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == seat_hit_pinned(seat))
		{
			if (pointer->moved)
				seat->pin_focus = 1;
			if (pointer->left_clicks)
			{
				seat->pin_focus = 1;
				row_accept(seat, seat->rows[seat->count - 1]);
				return;
			}
		}
	}
	item = ae_list_view_pointer(&seat->list, pointer, seat_hit_rows(seat));
	if (seat->pin_focus && pointer->moved && seat->list.hover >= 0)
		seat->pin_focus = 0;
	/* (a hover never rests on GAME (HOST)) */
	ae_list_set_focus(&seat->list.list, ae_pause_row_skip(seat->rows, seat->list_count, seat->list.list.focus, 1));
	if (item >= 0 && item < seat->list_count && ae_pause_row_look(seat->rows[item]) != AE_PAUSE_LOOK_HEADER)
	{
		ae_list_set_focus(&seat->list.list, item);
		row_accept(seat, seat->rows[item]);
	}
}

/* ---------- drawing */

struct list_context
{
	struct seat *seat;
	struct ae_density const *density;
	short hit_rows;
};


static float cap_top(int font, float size, float center)
{
	return center - ae_draw_cap_height(font, size) * 0.5f;
}

static void main_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct list_context const *list = context;
	struct seat const *seat = list->seat;
	int row = seat->rows[item];
	struct ae_row widget;

	if (ae_pause_row_look(row) == AE_PAUSE_LOOK_HEADER)
	{
		float group = list->density->metrics->group_height * list->density->unit;

		ae_widget_group(list->density, x, y + height - group, width, ae_pause_row_label(row, &seat->facts));
		return;
	}
	memset(&widget, 0, sizeof(widget));
	widget.label = ae_pause_row_label(row, &seat->facts);
	widget.value = row_value(seat, row);
	widget.flags = flags;
	if (ae_pause_row_look(row) == AE_PAUSE_LOOK_SUB)
		widget.flags |= AE_ROW_SUBSCREEN;
	/* (0.7.4: the focus is on the pinned row: the list shows none) */
	if (seat->pin_focus)
		widget.flags &= ~(unsigned int)(AE_ROW_FOCUSED | AE_ROW_ON_BAR | AE_ROW_UNDER_BAR | AE_ROW_BAR_BY_LIST);
	if (flags & (AE_ROW_FOCUSED | AE_ROW_ON_BAR | AE_ROW_UNDER_BAR))
		widget.flags &= ~(unsigned int)AE_ROW_HOVER;
	ae_widget_row(list->density, x, y, width, &widget, list->hit_rows, item);
}

static void bots_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct list_context const *list = context;
	struct ae_row widget;
	int kind = list->seat->bot_rows[item];

	(void)height;
	memset(&widget, 0, sizeof(widget));
	widget.label = ae_bot_setting_label(kind);
	widget.value = ae_bot_setting_text(kind);
	widget.flags = flags;
	if (flags & (AE_ROW_FOCUSED | AE_ROW_ON_BAR | AE_ROW_UNDER_BAR))
		widget.flags &= ~(unsigned int)AE_ROW_HOVER;
	ae_widget_row(list->density, x, y, width, &widget, list->hit_rows, item);
}

static void players_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct list_context const *list = context;
	struct player_entry const *entry;
	struct ae_row widget;
	char value[24];
	float edge = list->density->unit * 5.0f;

	if (item < 0 || item >= listed.count)
		return;
	entry = &listed.entries[item];
	if (entry->header)
	{
		float group = list->density->metrics->group_height * list->density->unit;

		ae_widget_group(list->density, x, y + height - group, width, entry->team ? "BLUE TEAM" : "RED TEAM");
		return;
	}
	memset(&widget, 0, sizeof(widget));
	widget.label = entry->name;
	widget.flags = flags;
	if (flags & (AE_ROW_FOCUSED | AE_ROW_ON_BAR | AE_ROW_UNDER_BAR))
		widget.flags &= ~(unsigned int)AE_ROW_HOVER;
	if (entry->host)
		widget.badges[0] = entry->local ? "HOST YOU" : "HOST";
	else if (entry->local)
		widget.badges[0] = "YOU";
	if (entry->last)
	{
		snprintf(value, sizeof(value), "LAST ON %s", team_word(entry->team));
		widget.value = value;
		widget.flags |= AE_ROW_STATUS;
	}
	else if (flags & (AE_ROW_FOCUSED | AE_ROW_ON_BAR | AE_ROW_UNDER_BAR))
		widget.value = team_word(entry->team ? 0 : 1);
	ae_widget_row(list->density, x, y, width, &widget, list->hit_rows, item);
	ae_draw_rect(x, y, edge, height, 0.0f, entry->team ? TEAM_BLUE : TEAM_RED);
}

static struct ae_prompt prompt_of(int button, const char *label, int action)
{
	struct ae_prompt prompt;

	memset(&prompt, 0, sizeof(prompt));
	prompt.button = button;
	prompt.label = label;
	prompt.action = action;
	return prompt;
}

/* the list's height: the room there is, or, when every row fits in it without the "N more" line the list keeps
under its rows, as much more as that line takes (it is never drawn then: nothing scrolls) */
static float list_height(struct ae_density const *density, short count, float room)
{
	float gap = density->metrics->gap * density->unit, pitch = ae_size_row(density) + gap;
	float rows = count * pitch - gap;

	if (count > 0 && rows <= room - gap)
		return room + ae_widget_more_line(density);
	return room;
}

/* the seat's panel in the view (layout units) its panel is centred in: the player's own view, or (layout B) the left
part of the frame */
static void draw_panel(struct seat *seat, struct ae_rect const *in, int framed)
{
	struct ae_layout layout;
	struct ae_rect content, panel;
	struct list_context context;
	float scale = ae_settings_ui_scale(), width_px, height_px, help_y, prompts_y;
	short player = seat_player(seat);
	const char *help;
	int warn = 0;
	char expanded[256], players_help[128];

	ae_draw_current_layout(&layout);
	width_px = in->width * layout.scale;
	height_px = in->height * layout.scale;
	/* (layout B: sized with the screen, as the 1080 frame it sits in, not by the view's capped scale) */
	if (framed && height_px > 0.0f)
	{
		float view = ae_view_scale(width_px, height_px, 1.0f);

		if (view > 0.0f)
			scale *= B_TEXT_SCALE_1080 * (height_px / 1080.0f) / view;
	}
	ae_density_view(width_px, height_px, scale, &seat->density);
	ae_view_panel_rect(width_px, height_px, scale, &panel);
	seat->panel.x = in->x + panel.x / layout.scale;
	seat->panel.y = in->y + panel.y / layout.scale;
	seat->panel.width = panel.width / layout.scale;
	seat->panel.height = panel.height / layout.scale;
	ae_draw_view(in->x, in->y, in->width, in->height);
	ae_widget_view_panel(&seat->density, width_px, height_px, player, player_colors[player],
		seat->page == PAGE_BOTS ? "BOTS" : seat->page == PAGE_PLAYERS ? "PLAYERS" : "PAUSED", 0, 0, &content);
	/* (no page dots: the rows right under the header, as the mockups' view panel has them: the stripe, the pad, the
	header's emblem, the pad) */
	{
		float top = panel.y * seat->density.pixel + PANEL_STRIPE_U * seat->density.unit +
			2.0f * seat->density.metrics->pad * seat->density.unit + 1.3f * ae_size_text(&seat->density);

		if (top < content.y)
		{
			content.height += content.y - top;
			content.y = top;
		}
	}
	context.seat = seat;
	context.density = &seat->density;
	context.hit_rows = seat_hit_rows(seat);
	ae_view_panel_footer(&seat->density, width_px, height_px, &help_y, &prompts_y);
	if (seat->page == PAGE_BOTS)
	{
		short focus = seat->bots.list.focus;

		ae_widget_list(&seat->density, &seat->bots, content.x, content.y, content.width,
			list_height(&seat->density, seat->bot_count, help_y - content.y), bots_item, &context, context.hit_rows);
		help = focus >= 0 && focus < seat->bot_count ? seat->bot_rows[focus] == AE_BOT_COUNT &&
			!ae_bot_setting_warns(AE_BOT_COUNT) ? BOTS_PAGE_HELP : ae_bot_setting_help(seat->bot_rows[focus]) : "";
		warn = focus >= 0 && focus < seat->bot_count && ae_bot_setting_warns(seat->bot_rows[focus]);
	}
	else if (seat->page == PAGE_PLAYERS)
	{
		short focus = seat->players.list.focus;
		struct player_entry const *entry = focus >= 0 && focus < listed.count ? &listed.entries[focus] : NULL;

		ae_widget_list(&seat->density, &seat->players, content.x, content.y, content.width,
			list_height(&seat->density, seat->players_count, help_y - content.y), players_item, &context,
			context.hit_rows);
		if (seat->move_note[0] && system_milliseconds() < seat->move_note_until)
			help = seat->move_note;
		else if (entry && !entry->header && entry->last)
		{
			snprintf(players_help, sizeof(players_help), "%s would have nobody: the game would end. Move someone else "
				"first.", entry->team ? "Blue" : "Red");
			help = players_help;
		}
		else
			help = "Left or right: the other team. They die now (not counted) and respawn there.";
	}
	else
	{
		/* (0.7.4: QUIT pinned: its row, and the rule over it, at the foot of the list's room) */
		float room = help_y - content.y, pin_room = 0.0f;

		if (seat->pinned)
		{
			float row = ae_size_row(&seat->density), gap = seat->density.metrics->gap * seat->density.unit;
			float rule = 2.0f * seat->density.pixel;
			struct ae_row widget;

			/* (the row 2 gaps over the help, a rule 1.5 gaps over the row, a gap more over the rule) */
			pin_room = row + 4.5f * gap + rule;
			memset(&widget, 0, sizeof(widget));
			widget.label = ae_pause_row_label(seat->rows[seat->count - 1], &seat->facts);
			widget.flags = seat->pin_focus ? AE_ROW_FOCUSED : 0;
			ae_draw_rect(content.x, content.y + room - row - 3.5f * gap - rule, content.width, rule, 0.0f,
				AE_COLOR_RULE);
			ae_widget_row(&seat->density, content.x, content.y + room - row - 2.0f * gap, content.width, &widget,
				seat_hit_pinned(seat), 0);
		}
		ae_widget_list(&seat->density, &seat->list, content.x, content.y, content.width,
			list_height(&seat->density, seat->list_count, room - pin_room), main_item, &context, context.hit_rows);
		help = ae_pause_row_help(focused_row(seat), &seat->facts);
	}
	ae_widget_help_strip_colored(&seat->density, content.x, help_y, content.width,
		ae_help_expand(help, expanded, sizeof(expanded)), warn ? AE_COLOR_WARNING : AE_COLOR_MUTED);
	seat->prompt_count = 0;
	seat->prompts[seat->prompt_count++] = prompt_of(AE_BUTTON_A, seat->page == PAGE_PLAYERS ? "Move" :
		ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
	if (seat->page == PAGE_PLAYERS)
	{
		seat->prompts[seat->prompt_count] = prompt_of(AE_BUTTON_DPAD_LEFT, "Team", AE_ACTION_LEFT);
		seat->prompts[seat->prompt_count++].pair = 1;
	}
	seat->prompts[seat->prompt_count++] = prompt_of(AE_BUTTON_B, seat->page != PAGE_MAIN ? ae_string(AE_STR_BACK) :
		"Resume", AE_ACTION_BACK);
	ae_widget_prompts(&seat->density, content.x, prompts_y, seat->prompts, seat->prompt_count, NULL, content.x + content.width,
		-1, seat_hit_prompts(seat));
	seat->drawn = 1;
}

/* the game card (layout B): map, gametype, mode line, scores, round or time left; in layout units of the full view */
static void draw_card(short player, struct ae_frame const *frame, float panel_right)
{
	struct ae_pause_card card;
	float k = frame->rect.width / 1920.0f, u = frame->unit;
	float width = CARD_WIDTH_U * u, x = frame->rect.x + CARD_RIGHT * k - width, y = CARD_TOP;
	float pad = CARD_PAD_U * u, top, cy, height, inner;
	short row;

	ae_glue_pause_card(player, &card);
	/* (at a bigger UI SCALE the card keeps its right edge and its gap from the panel: it narrows) */
	if (x < panel_right + CARD_GAP * k)
	{
		x = panel_right + CARD_GAP * k;
		width = frame->rect.x + CARD_RIGHT * k - x;
	}
	inner = width - 2.0f * pad;
	/* (its height: the text's lines to the first rule, the score rows, the foot's rule and line; the mockup's) */
	height = CARD_STRIPE_U * u + pad + CARD_RULES_U * u;
	if (card.rows)
		height += CARD_ROWS_ABOVE_U * u + card.rows * CARD_TEAM_PITCH_U * u - (CARD_TEAM_PITCH_U - CARD_TEAM_HEIGHT_U) * u;
	if (card.status[0] || card.clock[0])
		height += CARD_FOOT_RULE_U * u + CARD_FOOT_LINE_U * u + CARD_FOOT_BELOW_U * u;
	else
		height += pad;
	ae_draw_rect(x, y, width, height, 2.0f * u, AE_COLOR_VIEW_PANEL);
	ae_draw_rect(x, y, width, CARD_STRIPE_U * u, 0.0f, AE_COLOR_ACCENT);
	top = y + CARD_STRIPE_U * u + pad;
	/* the map (tracked, muted), the gametype (the title), the mode line */
	cy = top + 12.8f * u;
	ae_draw_text_tracked(AE_FONT_BODY, CARD_MAP_U * u, CARD_TRACKING, x + pad, cap_top(AE_FONT_BODY, CARD_MAP_U * u, cy),
		AE_ALIGN_LEFT, AE_COLOR_MUTED, card.map);
	cy = top + 64.0f * u;
	{
		char fitted[64];

		ae_fit_text(AE_FONT_ROW, CARD_TITLE_U * u, card.title, inner, fitted, sizeof(fitted));
		ae_draw_text(AE_FONT_ROW, CARD_TITLE_U * u, x + pad, cap_top(AE_FONT_ROW, CARD_TITLE_U * u, cy), AE_ALIGN_LEFT,
			AE_COLOR_TITLE, fitted);
	}
	if (card.mode[0])
	{
		cy = top + 115.2f * u;
		ae_draw_text(AE_FONT_BODY, CARD_MODE_U * u, x + pad, cap_top(AE_FONT_BODY, CARD_MODE_U * u, cy), AE_ALIGN_LEFT,
			AE_COLOR_TEXT, card.mode);
	}
	cy = top + CARD_RULES_U * u;
	if (card.rows)
	{
		ae_draw_rect(x + pad, cy, inner, CARD_RULE_U * u, 0.0f, AE_COLOR_RULE);
		cy += CARD_ROWS_ABOVE_U * u;
		for (row = 0; row < card.rows; row++)
		{
			float mid = cy + CARD_TEAM_HEIGHT_U * u * 0.5f;
			char fitted[40];

			ae_draw_rect(x + pad, cy, CARD_TEAM_BAR_U * u, CARD_TEAM_HEIGHT_U * u, 0.0f, card.colors[row]);
			ae_fit_text(AE_FONT_ROW, CARD_NAME_U * u, card.names[row], inner - 140.0f * u, fitted, sizeof(fitted));
			ae_draw_text(AE_FONT_ROW, CARD_NAME_U * u, x + pad + 28.8f * u, cap_top(AE_FONT_ROW, CARD_NAME_U * u, mid),
				AE_ALIGN_LEFT, card.mine[row] ? AE_COLOR_TITLE : AE_COLOR_TEXT, fitted);
			ae_draw_text(AE_FONT_ROW, CARD_SCORE_U * u, x + width - pad, cap_top(AE_FONT_ROW, CARD_SCORE_U * u, mid),
				AE_ALIGN_RIGHT, AE_COLOR_TITLE, card.scores[row]);
			cy += CARD_TEAM_PITCH_U * u;
		}
		cy -= (CARD_TEAM_PITCH_U - CARD_TEAM_HEIGHT_U) * u;
	}
	if (card.status[0] || card.clock[0])
	{
		cy += CARD_FOOT_RULE_U * u;
		ae_draw_rect(x + pad, cy, inner, CARD_RULE_U * u, 0.0f, AE_COLOR_RULE);
		cy += CARD_FOOT_LINE_U * u;
		ae_draw_text(AE_FONT_BODY, CARD_FOOT_U * u, x + pad, cap_top(AE_FONT_BODY, CARD_FOOT_U * u, cy), AE_ALIGN_LEFT,
			AE_COLOR_MUTED, card.status);
		ae_draw_text(AE_FONT_ROW, CARD_CLOCK_U * u, x + width - pad, cap_top(AE_FONT_ROW, CARD_CLOCK_U * u, cy),
			AE_ALIGN_RIGHT, AE_COLOR_TEXT, card.clock);
	}
}

/* a view's rectangle in layout units (as compute_window_bounds splits the screen) */
static void view_rect(short view, short views, short spans, struct ae_rect *rect)
{
	struct ae_layout layout;
	short columns = views <= 2 ? 1 : 2, rows = views <= 1 ? 1 : 2, slot = view;

	ae_draw_current_layout(&layout);
	if (views == 3 && view > 0)
		slot = (short)(view + 1);
	rect->width = layout.width / columns * (spans ? 2 : 1);
	rect->height = layout.height / rows;
	rect->x = (slot % columns) * (layout.width / columns);
	rect->y = (slot / columns) * (layout.height / rows);
}

/* a seat's pause as it is drawn over the game */
static void draw_seat(short player)
{
	{
		struct seat *seat = &pause.seats[player];
		short view, views, spans;

		seat->drawn = 0;
		if (!seat->open || seat->scoreboard || !ae_glue_pause_view(player, &view, &views, &spans))
			return;
		view_rect(view, views, spans, &seat->view);
		ae_draw_view_full();
		if (views == 1 && seat->game == AE_PAUSE_MULTIPLAYER)
		{
			/* layout B: the list on the left, the game card on the right */
			struct ae_layout layout;
			struct ae_frame frame;
			struct ae_rect in;
			float k;

			ae_draw_current_layout(&layout);
			ae_frame_compute(layout.width, ae_settings_ui_scale(), &frame);
			k = frame.rect.width / 1920.0f;
			float panel_width = B_PANEL_WIDTH * k * (frame.unit < B_PANEL_GROWTH ? frame.unit : B_PANEL_GROWTH);

			ae_draw_rect(seat->view.x, seat->view.y, seat->view.width, seat->view.height, 0.0f, SCRIM_ONE);
			draw_card(player, &frame, frame.rect.x + B_PANEL_X * k + panel_width);
			/* (the panel's view: the panel its share of it, centred, its left at B_PANEL_X) */
			in.width = panel_width / B_PANEL_SHARE;
			in.x = frame.rect.x + B_PANEL_X * k - (in.width - panel_width) * 0.5f;
			in.y = 0.0f;
			in.height = layout.height;
			draw_panel(seat, &in, 1);
		}
		else
		{
			/* layout A: the panel centred in the player's own view */
			ae_draw_rect(seat->view.x, seat->view.y, seat->view.width, seat->view.height, 0.0f,
				views == 1 ? SCRIM_ONE : SCRIM_SPLIT);
			draw_panel(seat, &seat->view, 0);
		}
		ae_draw_view_full();
	}
}

static void pause_draw(struct ae_screen *screen)
{
	short player;

	(void)screen;
	for (player = 0; player < SEATS; player++)
		draw_seat(player);
}

/* ---------- each frame */

void ae_screen_pause_tick(void)
{
	short player;

	if (!pause.on_stack)
		return;
	for (player = 0; player < SEATS; player++)
	{
		struct seat *seat = &pause.seats[player];

		if (!seat->open)
			continue;
		if (!ae_glue_pause_still_on(player, seat->game))
		{
			platform_log("ae pause: closed (the game ended)");
			seat_close(seat, 0);
			if (!pause.on_stack)
				return;
			continue;
		}
		rows_read(seat, player);
	}
	if (ae_ui_top() && ae_ui_top()->data == &pause)
		pause.screen_over = 0;
	/* (CHANGE MAP picked and closed: END GAME asks, in the picking seat's panel) */
	if (pause.map_seat >= 0 && ae_ui_top() && ae_ui_top()->data == &pause)
	{
		struct seat *seat = &pause.seats[pause.map_seat];

		pause.map_seat = -1;
		if (seat->open)
			confirm(seat, AE_PAUSE_MAP);
	}
}

static struct ae_screen_class const pause_class =
{
	.name = "pause", .handle = pause_handle, .draw = pause_draw, .pointer = pause_pointer, .start_from_anyone = 1,
	.popover = 1,
};

/* the view of the player whose screen is over the pause, in split screen; 0 with one view */
static int screen_view(struct ae_rect *rect)
{
	short view, views, spans;

	if (!pause.on_stack || !pause.screen_over || pause.screen_player < 0 || pause.screen_player >= SEATS ||
		!ae_glue_pause_view(pause.screen_player, &view, &views, &spans) || views <= 1)
	{
		return 0;
	}
	view_rect(view, views, spans, rect);
	return 1;
}

void ae_screen_pause_backdrop(void)
{
	struct ae_layout layout;
	struct ae_rect view;
	short player;

	if (!pause.on_stack || !pause.screen_over || (ae_ui_top() && ae_ui_top()->data == &pause))
		return;
	ae_draw_view_full();
	ae_draw_set_alpha(1.0f);
	if (!screen_view(&view))
	{
		ae_draw_current_layout(&layout);
		ae_draw_rect(0.0f, 0.0f, layout.width, layout.height, 0.0f, BACKDROP);
		return;
	}
	/* (split screen: the screen in its player's view, over a backdrop there; the others' pauses as they were) */
	ae_draw_rect(view.x, view.y, view.width, view.height, 0.0f, BACKDROP);
	for (player = 0; player < SEATS; player++)
	{
		if (player != pause.screen_player)
			draw_seat(player);
	}
	ae_draw_view_full();
}

int ae_screen_pause_inset(void const *screen_data, float rect[4])
{
	struct ae_rect view;

	if (screen_data == &pause || !ae_ui_holds(&pause) || !screen_view(&view))
		return 0;
	rect[0] = view.x;
	rect[1] = view.y;
	rect[2] = view.width;
	rect[3] = view.height;
	return 1;
}
