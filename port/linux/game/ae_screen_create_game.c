/*
AE_SCREEN_CREATE_GAME.C

CREATE GAME, one screen with its lobby (0.3.9; ae_screen_create_game.h; the approved design, mockups-038/one-lobby
SPEC.md: 1-host-open-alt, 2-lobby-setup, 3-host-public-full, 4-host-lan, 5-joiner). Positions are fractions of the
frame, sizes u.

MULTIPLAYER > CREATE GAME hosts at once (ae_lobby_host; the last NETWORK of this run, else HOST ONLINE when internet
play is on, LAN when it is off; INVITE ONLY), waits for this PC's client (10 s at most), adds player 1, chooses the map,
applies the copy (ae_setup_edit_apply) and the lobby is live: friends can join while the host picks.

On the left, the game: MAP (CHANGE MAP for the gametype's filter; its pick comes back here), GAMETYPE (CHANGE
GAMETYPE; a new copy), GAME OPTIONS (N CHANGED: the GAME OPTIONS screen on the copy), QUICK OPTIONS (time, score,
shield type, grenades on map, sensor range, respawn time: stepped in place; then MORE OPTIONS..., the start weapons as
its summary, scrolling sideways when they do not fit), START GAME, the screen's primary action (a menu blue frame, pinned under the rows: mockups-039/start-game-button A).
Every change reaches the server at once while the lobby is live (PREGAME); before that, the copy and the map are applied as it opens. The quick options change the session's copy:
they last to the next game, the saved gametype is never written.

(0.4.2) The lobby's own rows on the left too, under LOBBY after the quick options: NETWORK (OFFLINE / LAN / HOST ONLINE: A opens its
list; a change opens the lobby again, asked first when other players are in it: they leave with the old session, told
by their own game), PUBLIC (HOST ONLINE only: listed in everyone's SERVER BROWSER; off = invite only), MAX PLAYERS
(bots included) and INVITE (HOST ONLINE: A copies the invite link). A on a quick option opens its list (the row's
name over it); left and right step it in place.

On the right, MY PARTY (0.4.2; ae_widgets.h, ae_party_list.h): over the backdrop, no panel; "In Custom Games Lobby",
the network and the count; PLAYERS (this PC's people first) and BOTS (the skill beside the count). X (F), a click on
it or RB (E) opens the party view, the focus in it: the people (team arrows on this PC's own, and on the bots for the
host), ADD PLAYER, SHUFFLE TEAMS, BOTS and BOT SKILL (the settings' bots.count and bots.skill: OFF and 1-31 in the
menus, config.toml up to 127; written at once, so the lobby's bots follow), BOT SETUP... and the bots, then SEE ALL;
it scrolls with the focus. B, LB (Q) or up past its first row goes back to the left column; down from START GAME
comes in. X again (or SEE ALL) opens LOBBY, the whole lobby in team columns (big lobbies): LB RB between the columns,
the same team arrows, B back.

BOT SETUP, a popover over the party: the classic menus' SHOOTING SKILL, MOVEMENT SKILL (AS BOT SKILL or a level),
PLAYFIGHT and BOT TEAMS, written to config.toml at once as BOTS is.

MORE OPTIONS (0.4.2; mockups-042/more-options SPEC), BOT SETUP's popover for the rules a host changes between games
that are not quick rows: LOADOUT, PRIMARY WEAPON and SECONDARY WEAPON (always shown; picking one sets LOADOUT to
CUSTOM), the WEAPON PACKS this PC has (ON / OFF, as GAME OPTIONS' WEAPON PACKS: a pack on can start), then STARTING
GRENADES, WEAPON SET, MOTION SENSOR, FRIENDLY FIRE (team games), VEHICLE SET (both teams' in a team game) and NUMBER
OF LIVES. Left and right step a row, A opens its list (a pack's A turns it on or off); live as the quick rows, the
host's player 1's alone. Wider to the left when a row would be cut (130 %), never lower than 40 u over the screen's
bottom: then its rows scroll.

START GAME starts and takes every AE screen off the stack while the game plays; the take-over
(ae_lobby_take_pregame_ui) brings this screen back after it. B leaves (asked first when other people are in the
lobby): the session closes, AE's MULTIPLAYER (or the main menu after a game) comes back.

A joiner (SERVER BROWSER, LAN GAMES, JOIN WITH INVITE) sees the same screen read only, titled LOBBY: the host's map
and gametype, WAITING FOR THE HOST TO START, the panel's LOBBY line and roster; B leaves.

(0.4.3) START GAME counts down first (mockups-042/countdown SPEC A-E, owner 10-28 "Build as recommended"): the server's
own pregame countdown, debug.ae_lobby_countdown_ms (5999: 5, 4, 3, 2, 1, then the start; 0 starts at once), which every
machine gets. The START GAME row becomes "GAME STARTS IN" and a round number chip, a 4 u bar along its foot draining
with the time left (the host's menu blue, a joiner's ice blue on its quiet row); at 0 a joiner still loading the map
holds it ("WAITING FOR PLAYERS TO LOAD THE MAP"). The host's input is locked through it but for the cancel (B, Esc, a
right click: the server's countdown held and one NONE sent, so every machine stops showing it; a guest pad's B is
refused); START does nothing. A joiner's B is refused (only the host cancels). Beeps on every machine
(ae_countdown_beep: a tick a number and GO at 0, or one soft tick a number when the gametype has PRE-GAME COUNTDOWN,
whose 3-2-1 beeps in the game). Test launches keep starting at once.

Its log: "ae create game: open", "hosting (<network>, '<map>', '<gametype>')", "hosted, player 1 in", "open failed:
<why>", lobby: open", "lobby: quick <label> <value>", "lobby: more open", "lobby: more <label> <value>", "lobby: bots <count>", "lobby: bot skill <level>",
"lobby: players <people>, bots <bots>" (as they change), "lobby: setup open", "lobby: public on|off", "lobby: max
players <n>", "lobby: shooting skill|movement skill|playfight|teams <value>", "lobby: network <network>, reopening",
"lobby: column panel|left", "lobby: see all", "lobby: start", "lobby: countdown N s (beeps|quiet: PRE-GAME COUNTDOWN
on)", "lobby: start cancelled (player 1)", "lobby: cancel refused: a guest pad|the game already started", "lobby: leave
refused: the host is starting the game", "lobby: leave", "closed"; the countdown's own (every machine): "ae lobby:
countdown N", "ae lobby: countdown held at 0 (machines loading the map)", "ae lobby: the server cancelled the countdown
(NONE)".
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "game/game_engine.h"
#include "input/input.h"
#include "halo_ui_map_list.h"
#include "../src/ae_draw.h"
#include "../src/ae_layout.h"
#include "../src/ae_platform.h"
#include "ae_bot_settings.h"
#include "ae_glue_gametype_picker.h"
#include "ae_glue_lobby.h"
#include "ae_glue_pause.h"
#include "ae_header_lines.h"
#include "ae_input.h"
#include "ae_lobby_countdown.h"
#include "ae_party_list.h"
#include "ae_glue_setup_maps.h"
#include "ae_glue_setup_options.h"
#include "ae_hooks.h"
#include "ae_screen_change_gametype.h"
#include "ae_screen_change_map.h"
#include "ae_screen_create_game.h"
#include "ae_test_run.h"
#include "ae_screen_game_options.h"
#include "ae_screen_lobby_profile.h"
#include "ae_screen_main_menu.h"
#include "ae_setup_flow.h"
#include "ae_setup_maps.h"
#include "ae_setup_options.h"
#include "ae_sound.h"
#include "ae_stage_map.h"
#include "ae_team_shuffle.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "ae_scoreboard_header.h"

void platform_log(char const *format, ...);
/* ui_widget_event_handler_functions.c (declared as menu_functions.c does) */
short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);
boolean ui_widget_port_multiplayer_map_choose(short level_index);
/* ui_widget.c (its header needs the game's math types first: as ae_hooks.c declares them) */
boolean main_menu_is_active(void);
void ui_widgets_close_all(void);
void main_screen_shell_load(void);
/* p2p.c's (internet play: the hosted game's invite, its listing) and network_server_manager.c's server settings */
int p2p_invite_link(char *link, int size);
void p2p_set_hosting_public(int public_game);
void p2p_set_hosting_password(const char *password);
void network_game_server_port_set_settings(wchar_t const *name, long maximum_players);
int config_boolean(char const *name);
long config_integer(char const *name);
char const *config_string(char const *name);
int config_write(char const *name, char const *value);

enum
{
	FLOW_HIT = 0x7F00,
	HIT_ROWS = FLOW_HIT + 1, HIT_PROMPTS, HIT_CARDS, HIT_PARTY, HIT_SETUP, HIT_SETUP_ROWS, HIT_SETUP_PROMPTS,
	HIT_ALL_ROWS, HIT_ALL_PROMPTS, HIT_MORE, HIT_MORE_ROWS, HIT_MORE_PROMPTS,
	GAMETYPES_MAXIMUM = AE_SAVED_GAMES_MAXIMUM, /* (every gametype the memory unit lists) */
	JOIN_WAIT_MS = 10000
};
/* (no split screen choice: every game takes this PC's split screen players, and one bot path, the network bots,
serves them all: owner 2026-10-22. OFFLINE (owner 2026-10-26, issue #1): upstream's own split screen session, a server
that takes no other machine: nothing listed, nobody joins, the network bots as everywhere, and the pause stops it) */
enum { NETWORK_OFFLINE, NETWORK_LAN, NETWORK_ONLINE, NETWORKS };
enum { ROW_HEADER = AE_FLOW_ROW_HEADER, ROW_OPTION = AE_FLOW_ROW_OPTION, ROW_MAP = AE_FLOW_ROW_OWN, ROW_GAMETYPE,
	ROW_OPTIONS, ROW_START, ROW_INFO, ROW_NETWORK, ROW_PUBLIC, ROW_MAXIMUM, ROW_INVITE, ROW_MORE };
/* the session: none yet, this PC's client joining it, live, its hosting failed, to be hosted again next frame (a
NETWORK change: the old one closed this frame) */
enum { START_IDLE, START_JOINING, START_DONE, START_FAILED, START_REOPEN };
enum { PRIVACY_INVITE_ONLY, PRIVACY_PUBLIC, PRIVACIES };
/* BOT SETUP's rows: the classic menus' four more bot settings */
enum { SETUP_SHOOTING, SETUP_MOVEMENT, SETUP_PLAYFIGHT, SETUP_TEAMS, SETUP_ROWS };

static const char *const network_names[NETWORKS] = { "OFFLINE", "LAN", "HOST ONLINE" };
static const char *const network_words[NETWORKS] = { "OFFLINE", "LAN", "ONLINE" };
static const int network_kinds[NETWORKS] = { AE_LOBBY_LOCAL, AE_LOBBY_LAN, AE_LOBBY_ONLINE };
static const char *const privacy_words[PRIVACIES] = { "INVITE ONLY", "PUBLIC" };
#define NETWORK_HELP "OFFLINE: this PC only, the pause stops the game. LAN: anyone on this network sees the game. " \
	"HOST ONLINE: friends join with your invite. Changing it opens the lobby again."
static const char *const privacy_helps[PRIVACIES] = {
	"Off: only players you send your invite to can join (INVITE, in the lobby).",
	"On: anyone can find and join it in the SERVER BROWSER, and your invite works too."
};
#define MAXIMUM_HELP "The most players the game takes, bots included."
#define INVITE_HELP "{A} copies your game's invite link (halo://join/...) to paste to friends. Anyone with it can join."
/* (the new controller's own START and B: pad words whatever device drove the menus last) */
#define SHUFFLE_HELP "Deals everyone onto the two teams again at random, evenly. Bots you moved are dealt too."
#define CARDS_HELP "Left and right put a player on this PC, or a bot when you host, on the other team."
#define ADD_HELP "Split screen: START on another controller adds its player (its B takes them out). With one " \
	"controller and the keyboard, {A} here first."
#define BOT_SETUP_HELP "How the bots shoot and move, whether they playfight, and their teams."
#define SEE_ALL_HELP "Everyone in the lobby, in their team columns ({X} opens it from anywhere in the party)."
#define MORE_HELP "The start weapons and the rules hosts change most between games. {A} opens them."
/* (BOTS, BOT SKILL and BOT SETUP's four rows: ae_bot_settings.h, shared with the pause menu's BOTS) */
static const short maximum_players[] = { 2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128 };
enum { MAXIMUM_PLAYERS_COUNT = sizeof(maximum_players) / sizeof(maximum_players[0]), MAXIMUM_PLAYERS_DEFAULT = 10 };
/* the party's and the popover's spacing (u: the mockups' 1080p numbers) */
#define PARTY_SECTION_MORE_U 4.0f     /* BOTS' section: this more above it than PLAYERS' */
#define ALL_STATUS_U 20.0f            /* LOBBY: the status line's middle under the content's top */
#define ALL_COLUMNS_Y_U 72.0f         /* the columns' heads under the content's top */
#define ALL_HEAD_U 36.0f
#define ALL_GAP_U 60.0f               /* between the two columns */
#define ALL_TRACK_U 4.0f              /* a column's scrollbar */
#define SETUP_PAD_U 20.0f
#define SETUP_STRIPE_U 4.0f
#define SETUP_TITLE_U 32.0f
enum { SETUP_HELP_LINES = 3 };
#define MORE_BOTTOM_U 40.0f           /* MORE OPTIONS stops this over the screen's bottom; its rows scroll */
#define MORE_CLEAR_U 24.0f            /* grown to the left, it stays this clear of the left column */

/* the network the last lobby of this run was opened on (-1: none yet) */
static int last_network = -1;

static struct
{
	short map_index;
	char map_name[64];
	int network, privacy;
	short players_index;
	/* the saved gametypes, as GAMETYPE steps through them in place */
	char gametype_names[GAMETYPES_MAXIMUM][16];
	long gametype_profiles[GAMETYPES_MAXIMUM];
	short gametype_count;
	/* the session's steps */
	int start_step;
	unsigned long start_since;
	char failed[160];          /* START_FAILED: why */
	int starting;              /* a game under way: AE's screens off the stack */
	int from_menu;             /* opened from AE's MULTIPLAYER: leaving goes back to it */
	unsigned long test_start_at; /* a test launch's (ae_screen_create_game_test_open): START GAME pressed then; 0 none */
	short test_local_players;  /* a test launch's players on this machine still to join (controllers 2 up); 0 none */
	/* (0.4.0) the pause menu's GAME OPTIONS and CHANGE MAP in game: the next game's, applied as the lobby comes back
	after the game (the server takes a gametype or a map in its pregame only) */
	int next_options, next_map;
	short next_map_index;
	char next_map_name[64];
} create;

/* a test launch's map (ae_screen_create_game_test_open), its file name; empty: none */
static char test_map[64];

static struct
{
	struct ae_flow_screen flow;
	struct ae_lobby_roster roster;
	int have_roster;
	unsigned long roster_at;
	int options_open;          /* GAME OPTIONS opened from here: its changes applied when it closes */
	int leaving;
	int joiner;                /* joined another's game: the roster, map and gametype only */
	int adding;                /* (0.3.9) ADD PLAYER on: the next new controller's START joins */
	int host_started;          /* the host pressed START: the screen stays up until the game runs (a joiner loading
	                              the map holds the countdown, and the server may cancel it) */
	char joined_how[32];       /* (a joiner's crumb) how: "JOINED BY INVITE", the browser's own words */
	int from_browser;          /* opened by SERVER BROWSER / LAN GAMES (ae_screen_browser.c): leaving goes back to it */
	char invite[160];          /* the hosted game's invite link (ONLINE), "" none yet */
	char invite_shown[48];
	/* (0.4.2) MY PARTY: its rows as built (ae_party_list.h), the party view (the focus in it, else the left
	column's), the focused row (found again by what it is when the rows change), the first row drawn (it scrolls) */
	struct ae_party_item party[AE_PARTY_ITEMS];
	short party_count, party_focus, party_first;
	int party_view;
	struct ae_party_item party_key;
	int network_wanted;        /* NETWORK's pick waiting on its question (others in the lobby) */
	/* the host typing the BOTS row's count: its text, edit and caret blink */
	int typing_bots;
	struct ae_text typed_bots;
	struct ae_field_edit typed_bots_edit;
	unsigned long typed_bots_caret_since;
	short logged_people, logged_bots;   /* the roster's counts last logged (-1: none yet) */
	int profile_picking[4];      /* the players whose join waits on their profile pick (inline cards: several pads at once) */
	int profile_signed_in[4];    /* it was signed in on the main menu (B signs it out) */
	char more_summary[48];       /* MORE OPTIONS...'s summary as last shown, and when it showed (its marquee starts) */
	unsigned long more_since;
	int lone_pad_picking;        /* the keyboard's lone pad, moved to controller 2 by its START: its profile pick open */
	int more_hover;              /* (0.4.2) the pointer over the party view's "N more" line: its hover wash */
	/* (0.4.3) the start countdown, every machine's, from the seconds its client has (ae_lobby_countdown.h); the
	START GAME row's help through it */
	struct ae_countdown countdown;
	char countdown_help[96];
} lobby;

/* BOT SETUP, the popover */
static struct
{
	short focus;
	short kinds[SETUP_ROWS];
	short count;
} setup;

/* (0.4.2) LOBBY, the whole lobby in team columns (SEE ALL): its focus (a roster index, -1 none), its column, the
first row drawn of each */
static struct
{
	struct ae_flow_screen flow;
	short focus, column;
	short first[2];
} all;

/* MORE OPTIONS, the popover: its list (options; a section's header < 0), the focus (an index, never a header) and its
option (found again when the rows change), the first and last drawn (it scrolls), the rows as drawn (layout units;
width 0: not drawn) */
enum { MORE_ENTRIES = 20, MORE_HEADER_PACKS = -1, MORE_HEADER_RULES = -2 };
static struct
{
	short entries[MORE_ENTRIES], count;
	short focus, focus_option, first, last;
	struct ae_rect rects[MORE_ENTRIES];
	char help[320];
} more;

static struct ae_screen_class const lobby_class, setup_class, all_class, more_class;

/* ---------- what the copy says */

static struct ae_setup_access access_now(void)
{
	struct ae_setup_access access;

	ae_setup_edit_access(&access);
	return access;
}

/* the lobby is open for players (the host's: hosted, this PC's player in) */
static int live(void)
{
	return !lobby.joiner && create.start_step == START_DONE && ae_lobby_owns_session();
}

/* the engine's score row */
static short score_option(int engine)
{
	switch (engine)
	{
	case AE_SETUP_ENGINE_CTF: return AE_SO_CAPTURES_TO_WIN;
	case AE_SETUP_ENGINE_KING: return AE_SO_HILL_SCORE;
	case AE_SETUP_ENGINE_ODDBALL: return AE_SO_BALL_SCORE;
	case AE_SETUP_ENGINE_RACE: return AE_SO_LAPS_TO_WIN;
	case AE_SETUP_ENGINE_BOMB: return AE_SO_ROUNDS_TO_WIN;
	}
	return AE_SO_KILLS_TO_WIN;
}

/* CHANGE MAP's filter for the copy's gametype */
static int map_filter(void)
{
	struct ae_setup_access access = access_now();

	switch (access.engine)
	{
	case AE_SETUP_ENGINE_SLAYER: return ae_setup_team(&access) ? AE_MF_TEAM_SLAYER : AE_MF_FFA_SLAYER;
	case AE_SETUP_ENGINE_KING: return AE_MF_KING;
	case AE_SETUP_ENGINE_ODDBALL: return AE_MF_BALL;
	/* (Bomb's goals are the CTF flags) */
	case AE_SETUP_ENGINE_CTF: case AE_SETUP_ENGINE_BOMB: return AE_MF_CTF;
	}
	return AE_MF_ALL;
}

static const char *option_value(int option)
{
	struct ae_setup_access access = access_now();

	return ae_setup_value_text(option, access.get(access.context, option));
}

/* a quick option stepped in place (live: the server's at once) */
static void option_step(int option, short direction)
{
	struct ae_setup_access access = access_now();
	struct ae_setup_row const *row = ae_setup_row(option);
	int current = access.get(access.context, option);
	short next;

	if (!row || current < 0)
		return;
	/* (PRIMARY: the stock weapons and the packs that can start, as GAME OPTIONS offers them) */
	next = ae_setup_value_step(&access, option, (short)current, direction);
	if (next == current)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_setup_edit_set(option, next);
	ae_sound_request(AE_SOUND_CURSOR, 0);
	platform_log("ae create game: lobby: quick %s %s", ae_setup_label(&access, option), ae_setup_value_text(option, next));
	if (live())
		ae_setup_edit_apply();
}

static void map_name_of(short index)
{
	char const *const *names = NULL;
	short last = 0, count = ui_widget_port_multiplayer_maps(&names, &last);
	wchar_t wide[64];
	int at;

	create.map_name[0] = 0;
	if (index < 0 || index >= count)
		return;
	if (!ae_pack_map_caption(names[index], wide, 64))
		ui_map_list_caption(names[index], wide, 64);
	for (at = 0; at < (int)sizeof(create.map_name) - 1 && wide[at]; at++)
		create.map_name[at] = wide[at] >= 0x20 && wide[at] < 0x7F ? (char)wide[at] : '?';
	create.map_name[at] = 0;
}

/* whether the game's map list's map index has its file here (a stock map is listed with or without one) */
static int map_has_file(short index)
{
	char const *const *names = NULL;
	short count = ui_widget_port_multiplayer_maps(&names, NULL);

	return index >= 0 && index < count && ae_stage_map_file_present(names[index]);
}

static void map_picked(short list_index, char const *name)
{
	create.map_index = list_index;
	snprintf(create.map_name, sizeof(create.map_name), "%s", name);
	/* (live: the server's map now; else chosen as the lobby opens) */
	if (live() && ui_widget_port_multiplayer_map_choose(list_index))
		platform_log("ae create game: lobby: map '%s'", name);
}

static void gametype_picked(short index, void *context)
{
	(void)context;
	if (index < 0 || index >= create.gametype_count)
		return;
	if (ae_setup_edit_begin(create.gametype_profiles[index]))
	{
		platform_log("ae create game: gametype '%s'", create.gametype_names[index]);
		if (live())
			ae_setup_edit_apply();
	}
}

/* CHANGE GAMETYPE's pick: a new copy of its rules (live: the server's at once) */
static void gametype_profile_picked(long profile)
{
	char name[16];

	if (!ae_setup_edit_begin(profile))
		return;
	ae_setup_edit_name(name, sizeof(name));
	platform_log("ae create game: gametype '%s'", name);
	if (live())
		ae_setup_edit_apply();
}

static void gametype_picker(void)
{
	if (ae_screen_change_gametype_pick(gametype_profile_picked))
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* MAP, GAMETYPE, GAME OPTIONS */
static short setup_rows(struct ae_flow_screen *flow, short count)
{
	char name[16];
	short changes;
	struct ae_setup_access access = access_now();

	ae_setup_edit_name(name, sizeof(name));
	changes = ae_setup_changes(&access);
	flow->rows[count] = (struct ae_flow_row){ ROW_MAP, -1, "MAP", create.map_name[0] ? create.map_name : "CHOOSE",
		"The maps in folders, filtered for the gametype ({LT} / {RT} change the filter)." };
	count++;
	snprintf(flow->value_texts[count], sizeof(flow->value_texts[count]), "%s", name);
	flow->rows[count] = (struct ae_flow_row){ ROW_GAMETYPE, -1, "GAMETYPE", flow->value_texts[count],
		"CHANGE GAMETYPE: the gametypes in folders, by mode. Picking one starts a new copy of its rules." };
	count++;
	if (changes)
		snprintf(flow->value_texts[count], sizeof(flow->value_texts[count]), "%d CHANGED", changes);
	else
		snprintf(flow->value_texts[count], sizeof(flow->value_texts[count]), "AS SAVED");
	flow->rows[count] = (struct ae_flow_row){ ROW_OPTIONS, -1, "GAME OPTIONS", flow->value_texts[count],
		"Every rule of the gametype, in groups. Changes are for these games; SAVE AS NEW keeps them." };
	count++;
	return count;
}

static struct ae_flow_row option_row(int option)
{
	struct ae_setup_access access = access_now();
	struct ae_flow_row row;

	memset(&row, 0, sizeof(row));
	row.kind = ROW_OPTION;
	row.option = (short)option;
	row.label = ae_setup_label(&access, option);
	row.value = option_value(option);
	row.help = ae_setup_help(option, access.get(access.context, option));
	return row;
}

/* MORE OPTIONS...'s summary: the start weapons ("PISTOL, ASSAULT RIFLE", NONE without any), or the loadout's word
when it is not CUSTOM (CATEGORY) */
static void more_summary(char *text, int size)
{
	struct ae_setup_access access = access_now();
	int loadout = access.get(access.context, AE_SO_LOADOUT);
	int primary = access.get(access.context, AE_SO_PRIMARY), secondary = access.get(access.context, AE_SO_SECONDARY);

	if (loadout >= 0 && loadout != AE_SETUP_LOADOUT_CUSTOM)
		snprintf(text, (size_t)size, "%s", ae_setup_value_text(AE_SO_LOADOUT, loadout));
	else if (primary > 0 && secondary > 0)
		snprintf(text, (size_t)size, "%s, %s", ae_setup_value_text(AE_SO_PRIMARY, primary),
			ae_setup_value_text(AE_SO_SECONDARY, secondary));
	else if (primary > 0 || secondary > 0)
		snprintf(text, (size_t)size, "%s", primary > 0 ? ae_setup_value_text(AE_SO_PRIMARY, primary) :
			ae_setup_value_text(AE_SO_SECONDARY, secondary));
	else
		snprintf(text, (size_t)size, "NONE");
}

/* ---------- the session */

static int initial_network(void)
{
	char const *wanted = getenv("HALO_AE_NETWORK");

	if (last_network >= 0 && last_network < NETWORKS)
		return last_network;
	/* (launchers and tests: offline / lan / online; online is INVITE ONLY, as CREATE GAME opens it) */
	if (wanted && !strcmp(wanted, "offline"))
		return NETWORK_OFFLINE;
	if (wanted && !strcmp(wanted, "lan"))
		return NETWORK_LAN;
	if (wanted && !strcmp(wanted, "online"))
		return NETWORK_ONLINE;
	/* (a test launch that hosts for joiners, debug.network_test "host:...": on the LAN, as before 0.7.0) */
	wanted = config_string("debug.network_test");
	if (wanted && !strncmp(wanted, "host", 4))
		return NETWORK_LAN;
	/* (0.7.0, the owner: a new lobby starts OFFLINE, this PC only; LAN or HOST ONLINE is one row away) */
	return NETWORK_OFFLINE;
}

/* the game's name in lists (player 1's, as Server Setup's) and the most players, from the roster when there is one */
static void server_settings(struct ae_lobby_roster const *roster)
{
	wchar_t name[16];
	int at;

	if (!roster || !roster->count)
	{
		network_game_server_port_set_settings(NULL, maximum_players[create.players_index]);
		return;
	}
	for (at = 0; at < 15 && roster->players[0].name[at]; at++)
		name[at] = (wchar_t)(unsigned char)roster->players[0].name[at];
	name[at] = 0;
	network_game_server_port_set_settings(name, maximum_players[create.players_index]);
}

static void open_failed(const char *why)
{
	create.start_step = START_FAILED;
	snprintf(create.failed, sizeof(create.failed), "%s", why ? why : "");
	platform_log("ae create game: open failed: %s", create.failed);
	ae_flow_error(&lobby.flow, "The lobby did not open", create.failed);
}

/* hosts on the NETWORK picked; the wait for this PC's client goes on in session_update */
static void session_open(void)
{
	struct ae_result result;
	char name[16];

	ae_setup_edit_name(name, sizeof(name));
	platform_log("ae create game: hosting (%s, '%s', '%s')", network_names[create.network], create.map_name, name);
	last_network = create.network;
	lobby.invite[0] = 0;
	lobby.invite_shown[0] = 0;
	lobby.have_roster = 0;
	create.failed[0] = 0;
	result = ae_lobby_host(network_kinds[create.network]);
	if (!result.ok)
	{
		open_failed(result.reason);
		return;
	}
	/* (ae_lobby_host hosts invite only: PUBLIC lists it, as Server Setup's LISTING; no password) */
	p2p_set_hosting_public(create.network == NETWORK_ONLINE && create.privacy == PRIVACY_PUBLIC);
	p2p_set_hosting_password(NULL);
	create.start_step = START_JOINING;
	create.start_since = system_milliseconds();
}

/* the session closed (this PC's) and opened again next frame on create.network */
static void session_reopen(void)
{
	platform_log("ae create game: lobby: network %s, reopening", network_names[create.network]);
	if (ae_lobby_owns_session())
		ae_lobby_leave();
	lobby.have_roster = 0;
	create.start_step = START_REOPEN;
}

/* the wait: this PC's client in the pregame, then player 1, the map, the copy: the lobby is live */
static void session_update(void)
{
	static struct ae_lobby_roster roster;
	static int choosing;
	struct ae_result result;

	if (lobby.joiner)
		return;
	if (create.start_step == START_REOPEN)
	{
		session_open();
		return;
	}
	if (create.start_step != START_JOINING)
		return;
	if (!ae_lobby_roster(&roster).ok || (!roster.joined && system_milliseconds() - create.start_since < JOIN_WAIT_MS))
	{
		if (system_milliseconds() - create.start_since >= JOIN_WAIT_MS)
		{
			ae_lobby_leave();
			open_failed("This PC's game did not open its lobby in 10 seconds.");
		}
		return;
	}
	/* (a map that does not load runs the game's damaged-disc loop, and these screens inside it: never this step again
	from there, which chose the map again until the stack ran out) */
	if (choosing)
		return;
	if (!map_has_file(create.map_index))
	{
		ae_lobby_leave();
		open_failed(create.map_index < 0 ? "No map in the maps folder has its file." :
			"That map's file is not in the maps folder.");
		return;
	}
	choosing = 1;
	result = ae_lobby_add_local_player(0);
	if (!result.ok)
	{
		choosing = 0;
		ae_lobby_leave();
		open_failed(result.reason);
		return;
	}
	if (!ui_widget_port_multiplayer_map_choose(create.map_index))
	{
		choosing = 0;
		ae_lobby_leave();
		open_failed("That map can't be played with others in this build.");
		return;
	}
	choosing = 0;
	ae_setup_edit_apply();
	server_settings(&roster);
	create.start_step = START_DONE;
	platform_log("ae create game: hosted, player 1 in");
	platform_log("ae create game: lobby: open");
}

/* a refusal says so where the focused row's help goes, for a few seconds (draw_party_help and the left rows' help) */
enum { REFUSAL_MS = 3500 };
static char refusal_text[96];
static unsigned long refusal_until;

static int refusal_showing(void)
{
	return refusal_until && (long)(refusal_until - system_milliseconds()) > 0;
}

/* START GAME: starts a live lobby's game; a lobby that did not open is opened again. A game the host started (pressed,
not a test launch's): its map and gametype first in RECENT (ae_glue_picker_stores.h), and (0.4.3) the countdown first
(debug.ae_lobby_countdown_ms; a test launch starts at once, as the Custom Edition restart and network_test.c do) */
static void lobby_start(int pressed)
{
	struct ae_result result;
	long countdown_ms = pressed ? config_integer("debug.ae_lobby_countdown_ms") : 0;

	if (lobby.joiner || ae_countdown_running(&lobby.countdown))
		return;
	if (create.start_step == START_FAILED || create.start_step == START_IDLE)
	{
		ae_sound_request(AE_SOUND_FORWARD, 0);
		session_open();
		return;
	}
	if (!live())
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	result = countdown_ms > 0 ? ae_lobby_start_countdown(countdown_ms) : ae_lobby_start();
	if (!result.ok)
	{
		ae_flow_error(&lobby.flow, "The game did not start", result.reason);
		return;
	}
	platform_log("ae create game: lobby: start");
	/* (the count shows at once, before the server's first number; the focus stays on START GAME, out of the party) */
	if (countdown_ms > 0)
	{
		int quiet = ae_lobby_pregame_countdown();

		ae_countdown_begin(&lobby.countdown, countdown_ms, system_milliseconds());
		lobby.party_view = 0;
		lobby.flow.focus = (short)(lobby.flow.count - 1);
		refusal_until = 0;
		platform_log("ae create game: lobby: countdown %ld s (%s)", countdown_ms / 1000,
			quiet ? "quiet: PRE-GAME COUNTDOWN on" : "beeps");
	}
	if (pressed)
	{
		char const *const *names = NULL;
		short count = ui_widget_port_multiplayer_maps(&names, NULL);

		if (create.map_index >= 0 && create.map_index < count)
			ae_gtp_record_game(names[create.map_index]);
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	/* (the screen stays up through the countdown; it leaves once the game runs, as a joiner's does: lobby_update.
	Clearing it here showed AE's main menu over the live lobby until then: release review S1) */
	lobby.host_started = 1;
	/* (a test launch's START GAME is spent once the game starts by any way: still set, it started the next game the
	moment the post-game went back to the lobby, owner's TEST RUN 2026-10-26) */
	create.test_start_at = 0;
}

/* the people in the lobby on other machines (bots and this PC's players apart) */
static int others_in_lobby(void)
{
	int index, count = 0;

	for (index = 0; lobby.have_roster && index < lobby.roster.count; index++)
		count += !lobby.roster.players[index].bot && !lobby.roster.players[index].local;
	return count;
}

/* ---------- the left column */

static void lobby_rows(void)
{
	static const short quick[] = { AE_SO_TIME_LIMIT, -1, AE_SO_SHIELDS, AE_SO_GRENADES_ON_MAP, AE_SO_SENSOR_RANGE,
		AE_SO_RESPAWN_TIME };
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_setup_access access = access_now();
	struct ae_flow_row start;
	short count = 0, index;

	/* (a joiner: the host's map and gametype, nothing to change) */
	if (lobby.joiner)
	{
		wchar_t map[64];

		/* (the map by its name, not its file's: "BLOOD GULCH"; a gametype the client is not told yet: the host's) */
		map[0] = 0;
		if (lobby.have_roster && lobby.roster.map[0])
			ae_scoreboard_map_name(lobby.roster.map, map, NUMBEROF(map));
		ae_lobby_utf16_to_utf8(flow->value_texts[0], sizeof(flow->value_texts[0]), (const unsigned short *)map,
			NUMBEROF(map));
		flow->rows[count++] = (struct ae_flow_row){ ROW_INFO, -1, "MAP", flow->value_texts[0], NULL };
		flow->rows[count++] = (struct ae_flow_row){ ROW_INFO, -1, "GAMETYPE", lobby.have_roster &&
			lobby.roster.gametype[0] ? lobby.roster.gametype : "THE HOST'S", NULL };
		/* (a quiet line: nothing is done on it, so no selection bar; its help still shows. (0.4.3) Counting, the
		countdown is drawn on it, draw_countdown: its label empty) */
		if (ae_countdown_running(&lobby.countdown))
			flow->rows[count++] = (struct ae_flow_row){ ROW_INFO, -1, "", NULL,
				lobby.countdown.phase == AE_COUNTDOWN_HELD ? "Everyone needs the map before the game starts." :
				"The host started the game. Only the host can cancel it.", NULL, 0, 0, 1 };
		else
			flow->rows[count++] = (struct ae_flow_row){ ROW_INFO, -1, "WAITING FOR THE HOST TO START", NULL,
				"The host picks the map and rules and starts the game. {B} leaves it.", NULL, 0, 0, 1 };
		flow->count = count;
		flow->focus = (short)(count - 1);
		return;
	}
	count = setup_rows(flow, 0);
	flow->rows[count++] = (struct ae_flow_row){ ROW_HEADER, -1, "QUICK OPTIONS", NULL, NULL };
	for (index = 0; index < (short)(sizeof(quick) / sizeof(quick[0])); index++)
	{
		short option = quick[index] < 0 ? score_option(access.engine) : quick[index];

		if (ae_setup_visible(&access, option))
			flow->rows[count++] = option_row(option);
	}
	/* (0.4.2) MORE OPTIONS..., the start weapons as its summary: its marquee starts again when they change */
	more_summary(flow->value_texts[count], sizeof(flow->value_texts[count]));
	if (strcmp(flow->value_texts[count], lobby.more_summary))
	{
		snprintf(lobby.more_summary, sizeof(lobby.more_summary), "%s", flow->value_texts[count]);
		lobby.more_since = ae_motion_now();
	}
	flow->rows[count] = (struct ae_flow_row){ ROW_MORE, -1, "MORE OPTIONS...", flow->value_texts[count], MORE_HELP };
	flow->rows[count].subscreen = 1;
	flow->rows[count].marquee_ms = ae_motion_now() - lobby.more_since;
	count++;
	/* (0.4.2) the lobby's own, after the game's rules: who can join, and how many */
	flow->rows[count++] = (struct ae_flow_row){ ROW_HEADER, -1, "LOBBY", NULL, NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_NETWORK, -1, "NETWORK", network_names[create.network],
		NETWORK_HELP };
	if (create.network == NETWORK_ONLINE)
		flow->rows[count++] = (struct ae_flow_row){ ROW_PUBLIC, -1, "PUBLIC",
			create.privacy == PRIVACY_PUBLIC ? "ON" : "OFF", privacy_helps[create.privacy] };
	snprintf(flow->value_texts[count], sizeof(flow->value_texts[count]), "%d", maximum_players[create.players_index]);
	flow->rows[count] = (struct ae_flow_row){ ROW_MAXIMUM, -1, "MAX PLAYERS", flow->value_texts[count], MAXIMUM_HELP };
	count++;
	if (create.network == NETWORK_ONLINE)
		flow->rows[count++] = (struct ae_flow_row){ ROW_INVITE, -1, "INVITE", lobby.invite_shown[0] ?
			lobby.invite_shown : "BEING MADE...", INVITE_HELP, NULL, 0, 1 };
	/* START GAME, the primary action; while the lobby opens (or did not) its state in the value's place */
	memset(&start, 0, sizeof(start));
	start.kind = ROW_START;
	start.option = -1;
	start.label = "START GAME";
	start.primary = 1;
	/* ((0.4.3) counting: the countdown drawn on the row, draw_countdown, its label empty; the help says the time) */
	if (ae_countdown_running(&lobby.countdown))
	{
		start.label = "";
		if (lobby.countdown.phase == AE_COUNTDOWN_HELD)
			snprintf(lobby.countdown_help, sizeof(lobby.countdown_help),
				"Everyone needs the map before the game starts. {B} cancels the start.");
		else if (lobby.countdown.seconds > 0)
			snprintf(lobby.countdown_help, sizeof(lobby.countdown_help),
				"The game starts in %d second%s. {B} cancels it.", (int)lobby.countdown.seconds,
				lobby.countdown.seconds == 1 ? "" : "s");
		else
			snprintf(lobby.countdown_help, sizeof(lobby.countdown_help), "The game is starting. {B} cancels it.");
		start.help = lobby.countdown_help;
	}
	else if (live())
		start.help = "Starts the game for everyone in the lobby. After it, everyone comes back here.";
	else if (create.start_step == START_FAILED)
	{
		start.reason = "NOT OPEN";
		start.help = "The lobby did not open: {A} opens it again (NETWORK: another network).";
	}
	else
	{
		start.reason = "OPENING";
		start.help = "The lobby is opening: START GAME works once your player is in it.";
	}
	flow->rows[count++] = start;
	flow->count = count;
	if (flow->focus >= count)
		flow->focus = (short)(count - 1);
	if (flow->focus < 0)
		flow->focus = 0;
	if (flow->rows[flow->focus].kind == ROW_HEADER)
		flow->focus++;
}

/* the hosted game's invite link to the clipboard */
static void invite_copy(void)
{
	char body[160];

	if (!lobby.invite[0])
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_host_clipboard_set(lobby.invite);
	platform_log("ae create game: lobby: invite copied");
	snprintf(body, sizeof(body), "Your game's invite link (%s) is on the clipboard: paste it to your friends.",
		lobby.invite_shown);
	{
		struct ae_dialog_spec spec;

		memset(&spec, 0, sizeof(spec));
		spec.kind = AE_DIALOG_CONFIRM;
		spec.title = "Invite copied";
		spec.body = body;
		spec.choices[0] = "OK";
		spec.choice_count = 1;
		spec.has_cancel = 1;
		spec.density = lobby.flow.density;
		spec.bounds = lobby.flow.frame.rect;
		ae_dialog_open(&spec, AE_OWNER_ANY);
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* every AE screen off the stack (a game is starting: nothing of AE's draws over it; the take-over brings the lobby
back after it, AE's main menu comes back when the session is left) */
static void clear_stack(void)
{
	while (ae_ui_depth() && ae_ui_top())
	{
		if (!ae_ui_top()->data || !ae_ui_remove(ae_ui_top()->data))
			ae_ui_pop();
	}
}

/* B: the session closed (a host's) or left (a joiner's), the screen off the stack */
static void leave_now(void)
{
	int joiner = lobby.joiner;

	platform_log("ae create game: lobby: leave");
	lobby.leaving = 1;
	if (ae_lobby_owns_session())
		ae_lobby_leave();
	if (!joiner)
	{
		create.start_step = START_IDLE;
		create.starting = 0;
	}
	ae_ui_remove(&all);
	ae_ui_remove(&more);
	ae_ui_remove(&setup);
	ae_ui_remove(&lobby);
	if (joiner)
		return;
	platform_log("ae create game: closed");
	/* (the test screen's: the game's menus back; AE's MULTIPLAYER is under one opened from it, and AE's main menu
	comes back by itself on an empty stack after a game) */
	if (!create.from_menu)
	{
		ui_widgets_close_all();
		main_screen_shell_load();
	}
}

static void leave_picked(short choice, void *context)
{
	(void)context;
	if (choice == 1)
		leave_now();
}

static void leave_or_ask(void)
{
	struct ae_dialog_spec spec;
	int others = others_in_lobby();

	/* (the host alone, with bots or this PC's players: nothing to ask) */
	if (!lobby.joiner && others <= 0)
	{
		ae_sound_request(AE_SOUND_BACK, 0);
		leave_now();
		return;
	}
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "Leave the lobby?";
	spec.body = lobby.joiner ? "You leave the host's game." :
		"The game closes for everyone in it. Its settings stay for your next CREATE GAME.";
	spec.choices[0] = "STAY";
	spec.choices[1] = "LEAVE";
	spec.choice_count = 2;
	spec.has_cancel = 1;
	spec.density = lobby.flow.density;
	spec.bounds = lobby.flow.frame.rect;
	spec.picked = leave_picked;
	ae_dialog_open(&spec, AE_OWNER_ANY);
}

static void setup_open(void);
static void more_open(void);
static int guest_blocked(char const *what);
static int guest_start_blocked(void);

/* ---------- the dropdowns (A on a value row: its values, the row's name over them) */

static const char *network_values[NETWORKS];
static char maximum_texts[MAXIMUM_PLAYERS_COUNT][8];
static const char *maximum_values[MAXIMUM_PLAYERS_COUNT];
/* a quick option's values as offered (PRIMARY / SECONDARY: the weapons that can start), and their indexes */
static struct { const char *texts[64]; short indexes[64]; short count; int mapped; } option_map;

static void dropdown_open(short row, const char *const *values, short count, short current,
	void (*picked)(short, void *), void *context)
{
	struct ae_picker_spec spec;

	memset(&spec, 0, sizeof(spec));
	spec.values = values;
	spec.count = count;
	spec.current = current;
	spec.row = lobby.flow.rects[row];
	spec.bounds = lobby.flow.frame.rect;
	spec.density = lobby.flow.density;
	spec.picked = picked;
	spec.context = context;
	spec.title = lobby.flow.rows[row].label;
	if (ae_picker_open(&spec, AE_OWNER_ANY))
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void network_change_picked(short choice, void *context);

/* NETWORK's pick: the lobby opens again on it (asked first when others are in it) */
static void network_picked(short index, void *context)
{
	int others = others_in_lobby();

	(void)context;
	if (index < 0 || index >= NETWORKS || index == create.network)
		return;
	if (others <= 0 || !live())
	{
		create.network = index;
		session_reopen();
		return;
	}
	lobby.network_wanted = index;
	{
		struct ae_dialog_spec spec;
		char body[200];

		snprintf(body, sizeof(body), "The lobby opens again on %s. The %d player%s in it now leave%s it.",
			index == NETWORK_OFFLINE ? "this PC only (OFFLINE)" : index == NETWORK_LAN ?
			"your local network (LAN)" : "the internet (HOST ONLINE)", others,
			others == 1 ? "" : "s", others == 1 ? "s" : "");
		memset(&spec, 0, sizeof(spec));
		spec.kind = AE_DIALOG_CONFIRM;
		spec.title = "Change the network?";
		spec.body = body;
		spec.choices[0] = "KEEP";
		spec.choices[1] = "CHANGE";
		spec.choice_count = 2;
		spec.has_cancel = 1;
		spec.density = lobby.flow.density;
		spec.bounds = lobby.flow.frame.rect;
		spec.picked = network_change_picked;
		ae_dialog_open(&spec, AE_OWNER_ANY);
	}
}

static void privacy_step(short direction)
{
	short next = ae_value_step(PRIVACIES, (short)create.privacy, direction);

	ae_sound_request(next == create.privacy ? AE_SOUND_FAILURE : AE_SOUND_CURSOR, 0);
	if (next == create.privacy)
		return;
	create.privacy = next;
	platform_log("ae create game: lobby: public %s", create.privacy == PRIVACY_PUBLIC ? "on" : "off");
	if (ae_lobby_owns_session() && create.network == NETWORK_ONLINE)
		p2p_set_hosting_public(create.privacy == PRIVACY_PUBLIC);
}

static void maximum_set(short next)
{
	if (next < 0 || next >= MAXIMUM_PLAYERS_COUNT || next == create.players_index)
		return;
	create.players_index = next;
	platform_log("ae create game: lobby: max players %d", maximum_players[next]);
	if (live())
		server_settings(lobby.have_roster ? &lobby.roster : NULL);
}

static void maximum_picked(short index, void *context)
{
	(void)context;
	maximum_set(index);
}

static void maximum_step(short direction)
{
	short next = ae_value_step(MAXIMUM_PLAYERS_COUNT, create.players_index, direction);

	ae_sound_request(next == create.players_index ? AE_SOUND_FAILURE : AE_SOUND_CURSOR, 0);
	maximum_set(next);
}

static void option_picked(short index, void *context)
{
	short option = (short)(intptr_t)context;
	struct ae_setup_access access = access_now();

	if (option_map.mapped)
	{
		if (index < 0 || index >= option_map.count)
			return;
		index = option_map.indexes[index];
	}
	if (index == access.get(access.context, option))
		return;
	ae_setup_edit_set(option, index);
	platform_log("ae create game: lobby: quick %s %s", ae_setup_label(&access, option), ae_setup_value_text(option, index));
	if (live())
		ae_setup_edit_apply();
}

/* a quick option's list (as GAME OPTIONS offers it: PRIMARY and SECONDARY the weapons that can start) */
static void option_dropdown(short row, short option)
{
	struct ae_setup_access access = access_now();
	struct ae_setup_row const *setup_row = ae_setup_row(option);
	short current = (short)access.get(access.context, option), index;

	if (!setup_row || current < 0 || setup_row->value_count <= 0)
		return;
	if (ae_setup_edit_reason(option))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	option_map.mapped = 0;
	if (option != AE_SO_PRIMARY && option != AE_SO_SECONDARY)
	{
		dropdown_open(row, setup_row->values, setup_row->value_count, current, option_picked, (void *)(intptr_t)option);
		return;
	}
	option_map.count = ae_setup_offered_values(&access, option, option_map.indexes,
		(short)(sizeof(option_map.indexes) / sizeof(option_map.indexes[0])));
	option_map.mapped = 1;
	for (index = 0; index < option_map.count; index++)
		option_map.texts[index] = setup_row->values[option_map.indexes[index]];
	for (index = 0; index < option_map.count && option_map.indexes[index] != current; index++)
		;
	dropdown_open(row, option_map.texts, option_map.count, index < option_map.count ? index : 0, option_picked,
		(void *)(intptr_t)option);
}

static void network_dropdown(short row)
{
	short index;

	for (index = 0; index < NETWORKS; index++)
		network_values[index] = network_names[index];
	dropdown_open(row, network_values, NETWORKS, (short)create.network, network_picked, NULL);
}

static void maximum_dropdown(short row)
{
	short index;

	for (index = 0; index < MAXIMUM_PLAYERS_COUNT; index++)
	{
		snprintf(maximum_texts[index], sizeof(maximum_texts[index]), "%d", maximum_players[index]);
		maximum_values[index] = maximum_texts[index];
	}
	dropdown_open(row, maximum_values, MAXIMUM_PLAYERS_COUNT, create.players_index, maximum_picked, NULL);
}

/* the lobby's own rows are the host's player 1's (a split-screen guest's pad sees them) */
static int lobby_row(short kind)
{
	return kind == ROW_NETWORK || kind == ROW_PUBLIC || kind == ROW_MAXIMUM;
}

static void left_accept(short row)
{
	struct ae_flow_row const *r = &lobby.flow.rows[row];

	if (lobby_row(r->kind) && guest_blocked("the lobby's setup rows"))
		return;
	switch (r->kind)
	{
	case ROW_MORE:
		if (!guest_blocked("the game's rules"))
			more_open();
		break;
	case ROW_NETWORK:
		network_dropdown(row);
		break;
	case ROW_PUBLIC:
		privacy_step(1);
		break;
	case ROW_MAXIMUM:
		maximum_dropdown(row);
		break;
	case ROW_INVITE:
		invite_copy();
		break;
	case ROW_OPTION:
		option_dropdown(row, r->option);
		break;
	case ROW_MAP:
		if (ae_screen_change_map_pick(map_filter(), create.map_index, map_picked))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case ROW_GAMETYPE:
		gametype_picker();
		break;
	case ROW_OPTIONS:
		if (ae_screen_game_options_show())
		{
			lobby.options_open = 1;
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		break;
	case ROW_START:
		if (!guest_start_blocked())
			lobby_start(1);
		break;
	default:
		break;
	}
}

/* MAP and GAMETYPE stepped in place (left / right, as their rows' arrows say): the next map of the game's list (live:
the server's at once), the next saved gametype (a new copy) */
static void map_step(short direction)
{
	char const *const *names = NULL;
	short last = 0, count = ui_widget_port_multiplayer_maps(&names, &last), next, tries;

	if (count <= 0)
		return;
	next = create.map_index < 0 ? 0 : create.map_index;
	/* (the next map that has its file: a stock map is listed without one) */
	for (tries = 0; tries < count; tries++)
	{
		next = (short)((next + direction + count) % count);
		if (ae_stage_map_file_present(names[next]))
			break;
	}
	if (tries == count)
		return;
	create.map_index = next;
	map_name_of(next);
	ae_sound_request(AE_SOUND_CURSOR, 0);
	if (live() && ui_widget_port_multiplayer_map_choose(next))
		platform_log("ae create game: lobby: map '%s'", create.map_name);
}

static void gametype_step(short direction)
{
	short current = -1;

	create.gametype_count = ae_setup_gametypes(create.gametype_names, create.gametype_profiles, GAMETYPES_MAXIMUM,
		&current);
	if (create.gametype_count <= 0)
		return;
	ae_sound_request(AE_SOUND_CURSOR, 0);
	gametype_picked((short)(((current < 0 ? 0 : current) + direction + create.gametype_count) % create.gametype_count),
		NULL);
}

static void left_step(short row, short direction)
{
	struct ae_flow_row const *r = &lobby.flow.rows[row];

	if (lobby_row(r->kind) && guest_blocked("the lobby's setup rows"))
		return;
	/* (NETWORK opens its list: each step would open the lobby again) */
	if (r->kind == ROW_NETWORK)
		network_dropdown(row);
	else if (r->kind == ROW_PUBLIC)
		privacy_step(direction);
	else if (r->kind == ROW_MAXIMUM)
		maximum_step(direction);
	else if (r->kind == ROW_MAP)
		map_step(direction);
	else if (r->kind == ROW_GAMETYPE)
		gametype_step(direction);
	else if (r->kind == ROW_OPTION)
		option_step(r->option, direction);
}

/* ---------- the panel */

/* ADD PLAYER on or off (the game's ADD PLAYER state with it: a lone pad moves to its own controller) */
static void adding_set(int adding)
{
	lobby.adding = adding != 0;
	ae_lobby_adding_player(lobby.adding);
	platform_log("ae create game: lobby: add player %s", lobby.adding ? "on" : "off");
}

/* (0.3.9) split screen: START on a controller (2-4) with no player here joins it; B on one with a player takes it
out (player 1's controller, the keyboard's, and the pointer keep the lobby's own START and B). 1: taken */
/* (0.4.0) the BOTS and BOT SKILL rows, BOT SETUP and the lobby's own rows are the host's player 1's: a split-screen
guest's pad (any controller past the first) sees their values and cannot change them */
static int guest_pad(void)
{
	return ae_input_event_controller() >= 1;
}

static int guest_blocked(char const *what)
{
	if (!guest_pad())
		return 0;
	platform_log("ae create game: lobby: %s are the host's: a guest cannot change them", what);
	snprintf(refusal_text, sizeof(refusal_text), "Only player 1 changes %s.", what);
	refusal_until = system_milliseconds() + REFUSAL_MS;
	ae_sound_request(AE_SOUND_FAILURE, 0);
	return 1;
}

/* (0.4.2, owner) only player 1 starts the game: a guest pad's START or A on START GAME is refused, said in the help */
static int guest_start_blocked(void)
{
	if (!guest_pad())
		return 0;
	platform_log("ae create game: lobby: START GAME is the host's: a guest cannot start the game");
	snprintf(refusal_text, sizeof(refusal_text), "Only player 1 starts the game.");
	refusal_until = system_milliseconds() + REFUSAL_MS;
	ae_sound_request(AE_SOUND_FAILURE, 0);
	return 1;
}

/* (0.4.3, owner 2026-10-28) a pad joins only where the lobby is managed: MY PARTY's party view or the whole LOBBY;
anywhere else its START is refused, the help saying where. 1: refused */
static int join_refused(int join_here)
{
	if (join_here)
		return 0;
	platform_log("ae create game: lobby: a pad's START outside MY PARTY: no join");
	snprintf(refusal_text, sizeof(refusal_text), "Open MY PARTY to add a player.");
	refusal_until = system_milliseconds() + REFUSAL_MS;
	ae_sound_request(AE_SOUND_FAILURE, 0);
	return 1;
}

/* (0.4.3) the host cancels the start countdown: the server's countdown held and one NONE sent to every machine
(ae_lobby_cancel_start); START GAME back at once, the help saying so. Player 1's alone: a guest pad's B is refused as
its START is (guest_start_blocked). Too late once the server sent the start: every machine is loading the game */
static void countdown_cancel(void)
{
	struct ae_result result;

	if (guest_pad())
	{
		platform_log("ae create game: lobby: cancel refused: a guest pad");
		snprintf(refusal_text, sizeof(refusal_text), "Only player 1 cancels the start.");
		refusal_until = system_milliseconds() + REFUSAL_MS;
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	result = ae_lobby_cancel_start();
	if (!result.ok)
	{
		platform_log("ae create game: lobby: cancel refused: the game already started");
		return;
	}
	ae_countdown_cancel(&lobby.countdown, system_milliseconds());
	lobby.host_started = 0;
	snprintf(refusal_text, sizeof(refusal_text), "Start cancelled. START GAME counts down again.");
	refusal_until = system_milliseconds() + REFUSAL_MS;
	ae_sound_request(AE_SOUND_BACK, 0);
	platform_log("ae create game: lobby: start cancelled (player 1)");
}

/* (0.4.3) presses while the start counts down (mockups-042/countdown SPEC 4). The host: B (Esc, a right click: its
BACK) cancels it; every other press does nothing, silently (the rows, the party, X, Y, LB RB, a pad joining or leaving,
START: the map, gametype and teams every machine loads must not change under it). A joiner: its B is refused (only the
host cancels), and its pads' START and B and Y change nothing either; it can still look at the party and LOBBY.
1: taken */
static int countdown_press(struct ae_event const *event)
{
	if (!ae_countdown_running(&lobby.countdown))
		return 0;
	if (!lobby.joiner)
	{
		if (event->action == AE_ACTION_BACK)
			countdown_cancel();
		return 1;
	}
	if (event->action == AE_ACTION_BACK && !lobby.party_view)
	{
		platform_log("ae create game: lobby: leave refused: the host is starting the game");
		snprintf(refusal_text, sizeof(refusal_text), "The host started the game. Only the host can cancel it.");
		refusal_until = system_milliseconds() + REFUSAL_MS;
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return 1;
	}
	return event->action == AE_ACTION_Y || event->action == AE_ACTION_START ||
		(event->action == AE_ACTION_BACK && ae_input_event_controller() >= 1);
}

/* (0.4.3) the countdown followed from the seconds this machine's client has, each frame: each number logged and beeped
(every machine), the hold logged, a cancel this machine did not make said in the help */
static void countdown_follow(unsigned long now)
{
	short seconds = ae_lobby_owns_session() ? ae_lobby_seconds_to_start() : AE_COUNTDOWN_SECONDS_NONE;

	switch (ae_countdown_update(&lobby.countdown, seconds, now))
	{
	case AE_COUNTDOWN_NUMBER:
	case AE_COUNTDOWN_ZERO:
		platform_log("ae lobby: countdown %d", (int)lobby.countdown.seconds);
		ae_glue_sound_countdown(ae_countdown_beep(lobby.countdown.seconds, ae_lobby_pregame_countdown()),
			lobby.countdown.seconds);
		break;
	case AE_COUNTDOWN_HOLD:
		platform_log("ae lobby: countdown held at 0 (machines loading the map)");
		break;
	case AE_COUNTDOWN_CANCELLED:
		/* (the host's B on a joiner; on the host, the server's own: the lobby can no longer start) */
		platform_log("ae lobby: the server cancelled the countdown (NONE)");
		lobby.host_started = 0;
		snprintf(refusal_text, sizeof(refusal_text), "%s", lobby.joiner ? "The host cancelled the start." :
			"Start cancelled: not enough players.");
		refusal_until = now + REFUSAL_MS;
		ae_sound_request(AE_SOUND_BACK, 0);
		break;
	default:
		break;
	}
}

static void lobby_profile_done(short player, int picked, void *context)
{
	struct ae_result result;
	int signed_in = player >= 0 && player < 4 && lobby.profile_signed_in[player];
	int lone_pad = player == 1 && lobby.lone_pad_picking;

	(void)context;
	if (player < 0 || player >= 4)
		return;
	lobby.profile_picking[player] = 0;
	lobby.profile_signed_in[player] = 0;
	if (lone_pad)
		lobby.lone_pad_picking = 0;
	if (!picked)
	{
		if (signed_in)
			ae_screen_main_menu_signed_in_cancel(player);
		/* (the lone pad not joining goes back to the keyboard's controller) */
		if (lone_pad)
			adding_set(0);
		return;
	}
	result = ae_lobby_join_controller(player);
	if (!result.ok)
	{
		ae_flow_error(&lobby.flow, "The player did not join", result.reason);
		if (signed_in)
			ae_screen_main_menu_signed_in_cancel(player);
		if (lone_pad)
			adding_set(0);
		return;
	}
	lobby.adding = 0;
	if (signed_in)
		ae_screen_main_menu_signed_in_joined(player);
}

static int lobby_profile_open(short player, int signed_in)
{
	if (player < 0 || player >= 4 || lobby.profile_picking[player] || ae_lobby_controller_plays(player))
		return 0;
	if (!ae_screen_lobby_profile_open(player, &lobby.flow, lobby_profile_done, NULL))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return 0;
	}
	lobby.profile_picking[player] = 1;
	lobby.profile_signed_in[player] = signed_in;
	return 1;
}

/* A lone gamepad shares controller 1 with the keyboard: once the keyboard or mouse has been used, that pad's START
joins it as player 2 (as the main menu signs it in): ADD PLAYER's state moves it onto controller 2, its profile card
opens there. The pad as player 1's only device (no keyboard use) keeps START GAME. Not in a TEST RUN: the tester's
keys (the verdicts, a note) and his pad are one person (owner 2026-10-28: a pad's START mid-setup made him a second
player, split screen). Only in MY PARTY's party view or the whole LOBBY (join_here; join_refused). 1: taken */
static int lone_pad_press(struct ae_event const *event, int join_here)
{
	if (ae_test_run_active() || ae_input_event_controller() != 0 || event->action != AE_ACTION_START ||
		lobby.lone_pad_picking || lobby.profile_picking[1] || ae_lobby_controller_plays(1) ||
		!ae_platform_keyboard_used() || !ae_platform_pad_start_held())
		return 0;
	if (join_refused(join_here))
		return 1;
	platform_log("ae create game: lobby: the keyboard's lone pad joins (controller 2)");
	adding_set(1);
	lobby.lone_pad_picking = 1;
	if (!lobby_profile_open(1, 0))
	{
		lobby.lone_pad_picking = 0;
		adding_set(0);
	}
	return 1;
}

/* join_here: MY PARTY's party view or the whole LOBBY has the focus, where a new pad's START joins (join_refused) */
static int split_screen_press(struct ae_event const *event, int join_here)
{
	short controller = ae_input_event_controller();
	struct ae_result result;

	if (lone_pad_press(event, join_here))
		return 1;
	if (controller < 1)
		return 0;
	/* (a pad choosing its profile: its inline card in the roster takes its events) */
	if (ae_screen_lobby_profile_event(controller, event))
		return 1;
	if (event->action == AE_ACTION_START && !ae_lobby_controller_plays(controller))
	{
		if (!join_refused(join_here))
			lobby_profile_open(controller, 0);
		return 1;
	}
	if (event->action == AE_ACTION_BACK && ae_lobby_controller_plays(controller) && ae_lobby_local_players() >= 2)
	{
		result = ae_lobby_leave_controller(controller);
		ae_sound_request(result.ok ? AE_SOUND_BACK : AE_SOUND_FAILURE, 0);
		return 1;
	}
	if (event->action == AE_ACTION_BACK && lobby.adding && !ae_lobby_controller_plays(controller))
	{
		adding_set(0);
		ae_sound_request(AE_SOUND_BACK, 0);
		return 1;
	}
	return 0;
}

/* whether the roster's card has team arrows: a team game, this PC's own player, or a bot while this machine hosts */
static int card_arrows(short card)
{
	struct ae_lobby_player const *player;

	if (!lobby.have_roster || !lobby.roster.team_game || card < 0 || card >= lobby.roster.count)
		return 0;
	player = &lobby.roster.players[card];
	if (player->bot)
		return !lobby.joiner && live();
	return player->local;
}

/* the card's player on the other team: their own request to the host, or the host's bot held there */
static void card_team_step(short card)
{
	struct ae_lobby_player const *player;
	int moved;

	if (!card_arrows(card))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	player = &lobby.roster.players[card];
	moved = player->bot ? ae_lobby_change_bot_team(player->machine, player->controller) :
		ae_lobby_change_team(player->controller);
	platform_log("ae create game: lobby: %s team %s", player->bot ? "bot" : "player", moved ? "changed" : "not changed");
	ae_sound_request(moved ? AE_SOUND_FORWARD : AE_SOUND_FAILURE, 0);
}

/* SHUFFLE TEAMS: the host deals everyone again (ae_team_shuffle.h) */
static void teams_shuffle(void)
{
	int dealt = ae_team_shuffle();

	platform_log("ae create game: lobby: shuffle teams %s", dealt ? "dealt" : "refused");
	ae_sound_request(dealt ? AE_SOUND_FORWARD : AE_SOUND_FAILURE, 0);
}

static void bot_typing_finish(int keep)
{
	if (!lobby.typing_bots)
		return;
	lobby.typing_bots = 0;
	if (!keep || !ae_bot_setting_typed(lobby.typed_bots.text, "ae create game: lobby"))
	{
		platform_log("ae create game: lobby: bot count typing dropped%s", keep ? " (no number)" : "");
		return;
	}
	platform_log("ae create game: lobby: bot count typed '%s'", lobby.typed_bots.text);
}

static void bot_typing_done(int keep, void *context)
{
	(void)context;
	bot_typing_finish(keep);
}

static void bot_typing_end(int keep)
{
	if (!lobby.typing_bots)
		return;
	ae_field_edit_end(&lobby.typed_bots_edit, keep);
	bot_typing_finish(keep);
	ae_sound_request(keep ? AE_SOUND_FORWARD : AE_SOUND_BACK, 0);
}

static void bot_typing_begin(void)
{
	const char *shown = ae_bot_setting_text(AE_BOT_COUNT);

	if (lobby.typing_bots)
		return;
	/* OFF starts empty, as SETTINGS' SAME does; typing replaces the selected count. */
	if (!ae_bot_count_typing_ok(shown))
		shown = "";
	ae_text_init(&lobby.typed_bots, shown, 3, 0);
	lobby.typed_bots_edit.done = bot_typing_done;
	lobby.typed_bots_edit.context = NULL;
	lobby.typed_bots_edit.holder = &lobby;
	lobby.typing_bots = 1;
	ae_field_edit_begin(&lobby.typed_bots_edit, &lobby.typed_bots);
	ae_text_select_all(&lobby.typed_bots);
	lobby.typed_bots_caret_since = ae_motion_now();
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("ae create game: lobby: bot count typing from '%s'", shown);
}

static void bot_typing_update(void)
{
	struct ae_text before;
	int ok;

	if (!lobby.typing_bots || !lobby.typed_bots_edit.active)
		return;
	before = lobby.typed_bots;
	ok = ae_field_edit_keys(&lobby.typed_bots_edit);
	if (!ae_bot_count_typing_ok(lobby.typed_bots.text))
	{
		lobby.typed_bots = before;
		ok = 0;
	}
	if (memcmp(&before, &lobby.typed_bots, sizeof(before)))
		lobby.typed_bots_caret_since = ae_motion_now();
	if (!ok)
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

/* ---------- MY PARTY (0.4.2) */

static void all_open(void);

static void party_build(void)
{
	struct ae_party_rules rules;

	memset(&rules, 0, sizeof(rules));
	rules.view = lobby.party_view;
	rules.host = !lobby.joiner;
	rules.add = ae_lobby_local_players() < 4;
	rules.shuffle = live() && ae_team_shuffle_shown();
	rules.bot_setup = ae_bot_count() > 0;
	lobby.party_count = ae_party_items(lobby.have_roster ? &lobby.roster : NULL, &rules, lobby.party);
	lobby.party_focus = lobby.party_view ? ae_party_find(lobby.party, lobby.party_count, lobby.party_key,
		lobby.party_focus) : -1;
	if (lobby.party_focus >= 0)
		lobby.party_key = lobby.party[lobby.party_focus];
	else
		lobby.party_view = 0;
}

/* the party view on, its focus on key's row (NULL: its first) */
static void party_open(struct ae_party_item const *key)
{
	lobby.party_view = 1;
	lobby.party_focus = 0;
	if (key)
		lobby.party_key = *key;
	else
		lobby.party_key.kind = lobby.party_key.card = -1;
	party_build();
	if (!lobby.party_view)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
	platform_log("ae create game: lobby: column panel");
}

static void party_close(void)
{
	if (!lobby.party_view)
		return;
	lobby.party_view = 0;
	lobby.party_focus = -1;
	platform_log("ae create game: lobby: column left");
}

/* (0.4.2, owner) START never starts the game from wherever the focus is: it brings the focus to START GAME (out of the
party view). (0.4.3, owner 2026-10-28) Not even there: only the START GAME row itself (A, Enter or a click on it) starts
the game, so no pad's START (a split-screen player's included) can start it */
static void start_press(void)
{
	struct ae_flow_screen *flow = &lobby.flow;

	if (lobby.joiner || flow->count <= 0 || guest_start_blocked())
		return;
	if (!lobby.party_view && flow->focus == flow->count - 1)
		return;
	party_close();
	flow->focus = (short)(flow->count - 1);
	ae_sound_request(AE_SOUND_CURSOR, 0);
	platform_log("ae create game: lobby: START to START GAME");
}

static void party_focus_set(short index)
{
	if (index < 0 || index >= lobby.party_count)
		return;
	lobby.party_focus = index;
	lobby.party_key = lobby.party[index];
}

static const char *party_help(short index)
{
	if (index < 0 || index >= lobby.party_count)
		return NULL;
	switch (lobby.party[index].kind)
	{
	case AE_PARTY_ITEM_PERSON:
	case AE_PARTY_ITEM_BOT: return card_arrows(lobby.party[index].card) ? CARDS_HELP : NULL;
	case AE_PARTY_ITEM_ADD: return ADD_HELP;
	case AE_PARTY_ITEM_SHUFFLE: return SHUFFLE_HELP;
	case AE_PARTY_ITEM_BOTS: return ae_bot_setting_help(AE_BOT_COUNT);
	case AE_PARTY_ITEM_BOT_SKILL: return ae_bot_setting_help(AE_BOT_SKILL);
	case AE_PARTY_ITEM_BOT_SETUP: return BOT_SETUP_HELP;
	case AE_PARTY_ITEM_SEE_ALL: return SEE_ALL_HELP;
	}
	return NULL;
}

/* a player's or bot's row: team arrows (a bot's are the host's player 1's) */
static void party_team_step(short card)
{
	if (card >= 0 && card < lobby.roster.count && lobby.roster.players[card].bot && guest_blocked("the bots' teams"))
		return;
	card_team_step(card);
}

static void party_accept(short index)
{
	if (index < 0 || index >= lobby.party_count)
		return;
	switch (lobby.party[index].kind)
	{
	case AE_PARTY_ITEM_PERSON:
	case AE_PARTY_ITEM_BOT:
		/* (A changes the team too, as left and right do) */
		if (card_arrows(lobby.party[index].card))
			party_team_step(lobby.party[index].card);
		break;
	case AE_PARTY_ITEM_ADD:
		adding_set(!lobby.adding);
		ae_sound_request(lobby.adding ? AE_SOUND_FORWARD : AE_SOUND_BACK, 0);
		break;
	case AE_PARTY_ITEM_SHUFFLE:
		if (!guest_blocked("the team shuffle"))
			teams_shuffle();
		break;
	case AE_PARTY_ITEM_BOT_SETUP:
		if (!guest_blocked("the bot rows"))
			setup_open();
		break;
	case AE_PARTY_ITEM_SEE_ALL:
		all_open();
		break;
	default:
		break;
	}
}

static void party_step(short index, short direction)
{
	short kind;

	if (index < 0 || index >= lobby.party_count)
		return;
	kind = lobby.party[index].kind;
	if (kind == AE_PARTY_ITEM_PERSON || kind == AE_PARTY_ITEM_BOT)
		party_team_step(lobby.party[index].card);
	else if ((kind == AE_PARTY_ITEM_BOTS || kind == AE_PARTY_ITEM_BOT_SKILL) && !guest_blocked("the bot rows"))
		ae_bot_setting_step(kind == AE_PARTY_ITEM_BOT_SKILL ? AE_BOT_SKILL : AE_BOT_COUNT, direction,
			"ae create game: lobby");
}

/* where the party is ("In Custom Games Lobby • LAN", a joiner's "In <host>'s Lobby") and its count ("7/16"; the
lobby opening or not open) */
static void party_where(char *where, int where_size, char *count, int count_size)
{
	short players = lobby.have_roster ? lobby.roster.count : 0, index;

	count[0] = 0;
	if (lobby.joiner)
	{
		char const *host = NULL;

		for (index = 0; lobby.have_roster && index < lobby.roster.count; index++)
			if (lobby.roster.players[index].host)
				host = lobby.roster.players[index].name;
		if (host && *host)
			snprintf(where, where_size, "In %s's Lobby", host);
		else
			snprintf(where, where_size, "In the host's Lobby");
		if (lobby.have_roster && lobby.roster.maximum_players > 0)
			snprintf(count, count_size, "%d/%d", players, lobby.roster.maximum_players);
		return;
	}
	if (create.network == NETWORK_ONLINE)
		snprintf(where, where_size, "In Custom Games Lobby \xE2\x80\xA2 ONLINE \xE2\x80\xA2 %s", privacy_words[create.privacy]);
	else
		snprintf(where, where_size, "In Custom Games Lobby \xE2\x80\xA2 %s", network_words[create.network]);
	if (create.start_step == START_FAILED)
		snprintf(count, count_size, "NOT OPEN");
	else if (!live())
		snprintf(count, count_size, "OPENING...");
	else
		snprintf(count, count_size, "%d/%d", players, maximum_players[create.players_index]);
}

/* the BOTS row's typed count: a well at the row's right, its selection, its caret */
static void draw_bots_well(float right, float y, float height)
{
	struct ae_density const *d = &lobby.flow.density;
	float text = ae_size_text(d) * 0.8f, well_width = ae_draw_text_width(AE_FONT_ROW, text, "000") + 24.0f * d->unit;
	float well_x = right - well_width, center = y + height * 0.5f, cap = ae_draw_cap_height(AE_FONT_ROW, text);
	short from, to;

	ae_draw_rect(well_x, y + 4.0f * d->unit, well_width, height - 8.0f * d->unit, 0.0f, AE_COLOR_WELL);
	ae_draw_outline(well_x, y + 4.0f * d->unit, well_width, height - 8.0f * d->unit, 0.0f, 2.0f * d->unit,
		AE_COLOR_ACCENT);
	if (ae_text_selection(&lobby.typed_bots, &from, &to))
	{
		char left_text[4], right_text[4];
		float left, right_edge;

		snprintf(left_text, sizeof(left_text), "%.*s", (int)from, lobby.typed_bots.text);
		snprintf(right_text, sizeof(right_text), "%.*s", (int)to, lobby.typed_bots.text);
		left = left_text[0] ? ae_draw_text_width(AE_FONT_ROW, text, left_text) : 0.0f;
		right_edge = right_text[0] ? ae_draw_text_width(AE_FONT_ROW, text, right_text) : 0.0f;
		ae_draw_rect(well_x + 12.0f * d->unit + left, center - height * 0.22f, right_edge - left, height * 0.44f, 0.0f,
			AE_COLOR_TEXT_SELECT);
	}
	if (lobby.typed_bots.text[0])
		ae_draw_text(AE_FONT_ROW, text, well_x + 12.0f * d->unit, center - cap * 0.5f, AE_ALIGN_LEFT, AE_COLOR_TITLE,
			lobby.typed_bots.text);
	if (ae_caret_visible(lobby.typed_bots_caret_since))
	{
		char before[4];
		float caret;

		snprintf(before, sizeof(before), "%.*s", (int)lobby.typed_bots.caret, lobby.typed_bots.text);
		caret = before[0] ? ae_draw_text_width(AE_FONT_ROW, text, before) : 0.0f;
		ae_draw_rect(well_x + 12.0f * d->unit + caret, center - height * 0.22f, 2.0f * d->unit, height * 0.44f, 0.0f,
			AE_COLOR_ACCENT);
	}
}

/* a player's or bot's row as the party draws it */
static void party_player_row(short card, int focused, struct ae_party_row *row)
{
	struct ae_lobby_player const *player = &lobby.roster.players[card];
	unsigned int team_color = player->team ? 0x2E86DEFFu : 0xC0392BFFu;

	memset(row, 0, sizeof(*row));
	row->kind = player->bot ? AE_PARTY_BOT : AE_PARTY_PERSON;
	row->name = player->name;
	row->slot = player->slot;
	/* (a team game's: the team's colour, as in game; else their own armour colour) */
	row->color = lobby.roster.team_game ? team_color : player->color;
	row->sub_line = player->local ? "This PC" : "Joined";
	if (player->host)
		row->badge = ae_string(AE_STR_BADGE_HOST);
	if (focused)
	{
		row->flags |= AE_CARD_FOCUSED;
		if (card_arrows(card))
		{
			row->flags |= AE_CARD_ARROWS;
			row->team_name = player->team ? "BLUE" : "RED";
		}
	}
}

/* the BOTS row's value: the setting, or the seats left when the lobby fits fewer (the setting keeps the wanted count;
one seat stays free, as ae_lan_bots_target keeps it) */
static const char *bots_fit_text(char *text, size_t size)
{
	long wanted = ae_bot_count(), seats;

	if (!lobby.have_roster)
		return ae_bot_setting_text(AE_BOT_COUNT);
	seats = (lobby.roster.maximum_players > 0 ? lobby.roster.maximum_players : maximum_players[create.players_index]) -
		lobby.roster.people - 1;
	if (seats < 0)
		seats = 0;
	if (wanted <= seats)
		return ae_bot_setting_text(AE_BOT_COUNT);
	if (seats == 0)
		return "OFF";
	snprintf(text, size, "%ld", seats);
	return text;
}

/* the party from y down to bottom: its head, PLAYERS (the pads choosing their profiles after this PC's last person),
BOTS; folded, what fits and SEE ALL when more does not; the party view, a window of the rows round the focus */
static void draw_party(float x, float y, float width, float bottom)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_density const *d = &flow->density;
	float heights[AE_PARTY_ITEMS], choosing = 0.0f, gap = ae_flow_u(flow, AE_FLOW_CARD_GAP_U);
	float section = ae_flow_u(flow, 34.0f), row_gap = ae_flow_u(flow, 6.0f), more = ae_widget_more_line(d), room, indent;
	char where[96], count_text[24], label[64], bots_text[8];
	short index, first = 0, last_local = -1, shown, player;
	short people = lobby.have_roster ? lobby.roster.people : 0;
	short bots = lobby.have_roster ? (short)(lobby.roster.count - lobby.roster.people) : 0;
	int choosing_drawn = 0;

	indent = ae_flow_u(flow, AE_PARTY_INDENT_U);
	party_where(where, sizeof(where), count_text, sizeof(count_text));
	y += ae_widget_party_head(d, x, y, width, where, count_text, HIT_PARTY);
	for (player = 1; player < 4; player++)
	{
		float height = ae_screen_lobby_profile_card_height(d, player);

		if (height > 0.0f)
			choosing += height + gap;
	}
	for (index = 0; index < lobby.party_count; index++)
	{
		short kind = lobby.party[index].kind;

		heights[index] = ae_party_row_height(d, kind == AE_PARTY_ITEM_PERSON ? AE_PARTY_PERSON : AE_PARTY_ACTION) +
			row_gap;
		if (ae_party_section_starts(lobby.party, lobby.party_count, index))
			heights[index] += section + (index > 0 ? ae_flow_u(flow, PARTY_SECTION_MORE_U) : 0.0f);
		if (kind == AE_PARTY_ITEM_PERSON && lobby.roster.players[lobby.party[index].card].local)
			last_local = index;
	}
	/* (folded: room kept for SEE ALL's row; the party view: for "N more") */
	room = bottom - y - choosing - (lobby.party_view ? more : ae_party_row_height(d, AE_PARTY_ACTION) + row_gap);
	if (lobby.party_view)
	{
		first = ae_party_first(heights, lobby.party_count, lobby.party_focus, lobby.party_first, room);
		lobby.party_first = first;
		if (first > 0)
			ae_widget_more(d, x + indent + (width - indent) * 0.5f, y - more * 0.75f, 1, first);
	}
	/* (this PC's last person scrolled off the top: the choosing pads' cards first) */
	if (last_local < first && choosing > 0.0f)
	{
		for (player = 1; player < 4; player++)
		{
			float height = ae_screen_lobby_profile_card_height(d, player);

			if (height > 0.0f)
				y += ae_screen_lobby_profile_card_draw(d, player, x + indent, y, width - indent, HIT_CARDS) + gap;
		}
		choosing_drawn = 1;
	}
	for (index = first; index < lobby.party_count; index++)
	{
		struct ae_party_item const *item = &lobby.party[index];
		struct ae_party_row row;
		int focused = index == lobby.party_focus;

		if (room < heights[index] && index > first)
			break;
		room -= heights[index];
		if (ae_party_section_starts(lobby.party, lobby.party_count, index))
		{
			if (index > 0)
				y += ae_flow_u(flow, PARTY_SECTION_MORE_U);
			if (item->kind == AE_PARTY_ITEM_PERSON || item->kind == AE_PARTY_ITEM_ADD)
				snprintf(label, sizeof(label), "PLAYERS  %d", people);
			else if (!lobby.party_view && !lobby.joiner)
				snprintf(label, sizeof(label), "BOTS  %d  \xE2\x80\xA2  %s", bots, ae_bot_setting_text(AE_BOT_SKILL));
			else
				snprintf(label, sizeof(label), "BOTS  %d", bots);
			y += ae_widget_party_section(d, x, y, width, label);
		}
		memset(&row, 0, sizeof(row));
		row.kind = AE_PARTY_ACTION;
		if (focused)
			row.flags |= AE_CARD_FOCUSED;
		switch (item->kind)
		{
		case AE_PARTY_ITEM_PERSON:
		case AE_PARTY_ITEM_BOT:
			party_player_row(item->card, focused, &row);
			break;
		case AE_PARTY_ITEM_ADD:
			row.kind = AE_PARTY_DASHED;
			row.name = "+  ADD PLAYER";
			row.value = lobby.adding ? "PRESS START ON IT" : NULL;
			break;
		case AE_PARTY_ITEM_SHUFFLE:
			row.name = "SHUFFLE TEAMS";
			break;
		case AE_PARTY_ITEM_BOTS:
			row.name = "BOTS";
			row.value = lobby.typing_bots ? NULL : bots_fit_text(bots_text, sizeof(bots_text));
			break;
		case AE_PARTY_ITEM_BOT_SKILL:
			row.name = "BOT SKILL";
			row.value = ae_bot_setting_text(AE_BOT_SKILL);
			break;
		case AE_PARTY_ITEM_BOT_SETUP:
			row.name = "BOT SETUP...";
			break;
		default:
			row.kind = AE_PARTY_DASHED;
			snprintf(label, sizeof(label), "SEE ALL %d IN LOBBY", lobby.have_roster ? lobby.roster.count : 0);
			row.name = label;
			break;
		}
		{
			float height = ae_widget_party_row(d, x, y, width, &row, HIT_PARTY, index);

			if (item->kind == AE_PARTY_ITEM_BOTS && lobby.typing_bots)
				draw_bots_well(x + width - ae_flow_u(flow, 16.0f), y, height - row_gap);
			y += height;
		}
		if (index == last_local && !choosing_drawn)
		{
			for (player = 1; player < 4; player++)
			{
				float height = ae_screen_lobby_profile_card_height(d, player);

				if (height > 0.0f)
					y += ae_screen_lobby_profile_card_draw(d, player, x + indent, y, width - indent, HIT_CARDS) + gap;
			}
			choosing_drawn = 1;
		}
	}
	shown = index;
	if (shown >= lobby.party_count)
		return;
	if (lobby.party_view)
	{
		/* (the "N more" line opens the whole lobby too, as SEE ALL does folded) */
		if (lobby.more_hover)
			ae_draw_rect(x + indent, y, width - indent, more, 0.0f, AE_COLOR_HOVER);
		ae_widget_more(d, x + indent + (width - indent) * 0.5f, y, 0, lobby.party_count - shown);
		ae_hit_add(x + indent, y, width - indent, more, HIT_PARTY, AE_PART_CARD, -2);
		return;
	}
	/* folded, more than fits: SEE ALL opens the whole lobby */
	{
		struct ae_party_row row;

		memset(&row, 0, sizeof(row));
		row.kind = AE_PARTY_DASHED;
		snprintf(label, sizeof(label), "SEE ALL %d IN LOBBY", lobby.have_roster ? lobby.roster.count : 0);
		row.name = label;
		ae_widget_party_row(d, x, y, width, &row, HIT_PARTY, -2);
	}
}

/* the focused party row's help where the rows' help goes (ae_flow_draw_rows' place and rule: under the rows on the
left, when there is room under them) */
static void draw_party_help(float width)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_density const *d = &flow->density;
	float x = ae_flow_left_x(flow), body = ae_flow_u(flow, d->metrics->body), line = ae_flow_u(flow, d->metrics->body_line);
	float help_gap = ae_flow_u(flow, 28.0f), help_y = ae_flow_content_bottom(flow) - line * 2.0f, rows_end;
	char lines[2][160];
	char expanded[320];
	const char *help;
	int index, count;
	short last;

	if (!lobby.party_view || flow->count <= 0)
		return;
	help = refusal_showing() ? refusal_text : party_help(lobby.party_focus);
	if (!help)
		return;
	/* (the last row drawn: the rows may scroll) */
	for (last = (short)(flow->count - 1); last > 0 && flow->rects[last].width <= 0.0f; last--)
		;
	rows_end = flow->rects[last].y + flow->rects[last].height + ae_flow_u(flow, d->metrics->gap);
	if (help_y < rows_end + help_gap)
		return;
	ae_draw_rect(x, help_y - help_gap * 0.5f, width, ae_flow_u(flow, AE_FLOW_RULE_U), 0.0f, AE_COLOR_RULE);
	count = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(help, expanded, sizeof(expanded)), width, lines, 2);
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, x, help_y + line * (float)index, AE_ALIGN_LEFT,
			!refusal_showing() && lobby.party[lobby.party_focus].kind == AE_PARTY_ITEM_BOTS && ae_bot_setting_warns(AE_BOT_COUNT) ?
			AE_COLOR_WARNING : AE_COLOR_TEXT, lines[index]);
}

/* (0.4.3) the start countdown on its row, the last of the left column (mockups-042/countdown A, B, E): the host's
START GAME (the focus's white bar and ice-blue notch, drawn by the row with its label empty), a joiner's quiet row.
Centred, "GAME STARTS IN" and a round number chip (the full chip's 34 u: a dark disc and white digit on the bar; an
ice-blue ring off it, not lit: not the joiner's to act on), and a 4 u bar along the row's foot from the notch, draining
right to left with the time left (the host's menu blue, the primary action's frame colour; a joiner's ice blue). Held
at 0: the label alone, no bar */
#define COUNTDOWN_GAP_U 14.0f         /* the label to its chip */
#define COUNTDOWN_DIGIT_U 22.0f
#define COUNTDOWN_RING_U 2.0f
#define COUNTDOWN_DRAIN_U 4.0f
#define COUNTDOWN_NOTCH_U 6.0f        /* (ae_widgets.c's NOTCH_U: the focused row's ice-blue notch) */
static void draw_countdown(void)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_density const *d = &flow->density;
	short row = (short)(flow->count - 1);
	struct ae_rect rect;
	int lit = !lobby.joiner;
	float text = ae_size_text(d), digit = ae_flow_u(flow, COUNTDOWN_DIGIT_U), chip = ae_flow_u(flow, d->metrics->chip);
	float gap = ae_flow_u(flow, COUNTDOWN_GAP_U), notch = lit ? ae_flow_u(flow, COUNTDOWN_NOTCH_U) : 0.0f;
	float drain = ae_flow_u(flow, COUNTDOWN_DRAIN_U), center_y, label_width, left, x;
	unsigned int ink = lit ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TEXT;
	char number[8];

	if (!ae_countdown_running(&lobby.countdown) || row < 0 || flow->rects[row].width <= 0.0f)
		return;
	rect = flow->rects[row];
	center_y = rect.y + rect.height * 0.5f;
	if (lobby.countdown.phase == AE_COUNTDOWN_HELD)
	{
		ae_draw_text(AE_FONT_ROW, text, rect.x + rect.width * 0.5f,
			center_y - ae_draw_cap_height(AE_FONT_ROW, text) * 0.5f, AE_ALIGN_CENTER, ink,
			"WAITING FOR PLAYERS TO LOAD THE MAP");
		return;
	}
	left = ae_countdown_left(&lobby.countdown, system_milliseconds());
	if (left > 0.0f)
		ae_draw_rect(rect.x + notch, rect.y + rect.height - drain, (rect.width - notch) * left, drain, 0.0f,
			lit ? AE_COLOR_WARNING : AE_COLOR_ACCENT);
	label_width = ae_draw_text_width(AE_FONT_ROW, text, "GAME STARTS IN");
	x = rect.x + (rect.width - (label_width + gap + chip)) * 0.5f;
	ae_draw_text(AE_FONT_ROW, text, x, center_y - ae_draw_cap_height(AE_FONT_ROW, text) * 0.5f, AE_ALIGN_LEFT, ink,
		"GAME STARTS IN");
	x += label_width + gap;
	if (lit)
		ae_draw_rect(x, center_y - chip * 0.5f, chip, chip, chip * 0.5f, AE_COLOR_SELECTION_TEXT);
	else
	{
		ae_draw_rect(x, center_y - chip * 0.5f, chip, chip, chip * 0.5f, AE_COLOR_PANEL);
		ae_draw_outline(x, center_y - chip * 0.5f, chip, chip, chip * 0.5f, ae_flow_u(flow, COUNTDOWN_RING_U),
			AE_COLOR_ACCENT);
	}
	snprintf(number, sizeof(number), "%d", (int)lobby.countdown.seconds);
	ae_draw_text(AE_FONT_ROW, digit, x + chip * 0.5f, center_y - ae_draw_cap_height(AE_FONT_ROW, digit) * 0.5f,
		AE_ALIGN_CENTER, lit ? AE_COLOR_SELECTION : AE_COLOR_TITLE, number);
}

/* ---------- the screen */

static void lobby_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &lobby.flow;
	char crumb[96];
	float party_x, inset;
	short kind;

	(void)instance;
	ae_flow_full_density(flow);
	lobby_rows();
	party_build();
	if (lobby.joiner)
		snprintf(crumb, sizeof(crumb), "%s", lobby.joined_how[0] ? lobby.joined_how : "JOINED BY INVITE");
	else
		snprintf(crumb, sizeof(crumb), "%s", network_words[create.network]);
	ae_flow_title(flow, lobby.joiner ? "LOBBY" : "CREATE GAME", crumb);
	/* (the party view has the focus: no row on the left lit, nor its help) */
	{
		short focus = flow->focus;
		char const *help = focus >= 0 && focus < flow->count ? flow->rows[focus].help : NULL;

		if (lobby.party_view)
			flow->focus = -1;
		else if (help && refusal_showing())
			flow->rows[focus].help = refusal_text;
		ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
		flow->focus = focus;
		if (focus >= 0 && focus < flow->count)
			flow->rows[focus].help = help;
	}
	draw_countdown();
	draw_party_help(flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
	/* (MY PARTY over the backdrop, inside where the panel was) */
	inset = ae_flow_u(flow, AE_FLOW_INSET_U);
	party_x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X + inset;
	draw_party(party_x, ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y), ae_flow_right_x(flow) - inset - party_x,
		ae_flow_content_bottom(flow));
	kind = lobby.party_view ? lobby.party[lobby.party_focus].kind : -1;
	flow->prompt_count = 0;
	/* ((0.4.3) the start counting down: the host's one press, B; nothing for a joiner to press, mockups-042/countdown
	A, B) */
	if (ae_countdown_running(&lobby.countdown))
	{
		if (!lobby.joiner)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Cancel start", AE_ACTION_BACK);
		ae_flow_draw_prompts(flow);
		return;
	}
	if (lobby.typing_bots)
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Keep", AE_ACTION_ACCEPT);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Cancel", AE_ACTION_BACK);
		ae_flow_draw_prompts(flow);
		return;
	}
	if (!lobby.joiner || lobby.party_view)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, kind == AE_PARTY_ITEM_BOTS &&
			ae_ui_last_device() == AE_DEVICE_KEYBOARD_MOUSE ? "Type a value" : ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, lobby.party_view ? ae_string(AE_STR_BACK) : "Leave",
		AE_ACTION_BACK);
	if (lobby.have_roster && lobby.roster.team_game)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Change team", AE_ACTION_Y);
	/* (a row with team arrows has the focus: "< > Team", the pad's d-pad left and right as the pair) */
	if ((kind == AE_PARTY_ITEM_PERSON || kind == AE_PARTY_ITEM_BOT) && card_arrows(lobby.party[lobby.party_focus].card))
	{
		flow->prompts[flow->prompt_count] = ae_flow_prompt(AE_BUTTON_DPAD_LEFT, "Team", AE_ACTION_LEFT);
		flow->prompts[flow->prompt_count++].pair = 1;
	}
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, lobby.party_view ? "See all" : "Party", AE_ACTION_X);
	if (!lobby.joiner)
	{
		/* (0.3.9: "LB RB Column", paired as CHANGE MAP's "LB RB Filter": the left column or the party) */
		flow->prompts[flow->prompt_count] = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Column", AE_ACTION_TAB_PREVIOUS);
		flow->prompts[flow->prompt_count++].pair = 1;
		/* (0.4.2: no "Start game" prompt: START only brings the focus to START GAME, start_press) */
	}
	ae_flow_draw_prompts(flow);
}

static void lobby_update(struct ae_screen *instance)
{
	unsigned long now = system_milliseconds();
	short signed_in;

	(void)instance;
	session_update();
	bot_typing_update();
	countdown_follow(now);
	/* Players signed in on the main menu (their profiles picked there) join after player 1. */
	signed_in = ae_screen_main_menu_signed_in_waiting();
	if (signed_in != NONE && !lobby.profile_picking[signed_in] && ae_lobby_controller_plays(0) && ae_lobby_owns_session() &&
		(lobby.joiner || live()))
	{
		struct ae_result joined = ae_lobby_join_controller(signed_in);

		platform_log("ae create game: lobby: player %d (signed in) joins: %s", signed_in + 1,
			joined.ok ? "ok" : joined.reason);
		if (joined.ok)
			ae_screen_main_menu_signed_in_joined(signed_in);
		else
			ae_screen_main_menu_signed_in_cancel(signed_in);
	}
	/* (0.4.0) a joiner whose game is gone at the main menu (the host left, or the pregame join failed: a map missing):
	back to where it joined from, once the game's error over this screen is closed (ae_glue_errors.c), not left in a
	lobby of 0 players */
	if (lobby.joiner && !lobby.leaving && !ae_lobby_owns_session() && main_menu_is_active())
	{
		platform_log("ae create game: lobby: the game is gone");
		leave_now();
		return;
	}
	/* (a test launch's split screen: controllers 2 up join as their START would, once the lobby is live) */
	if (create.test_local_players > 1 && live() && !lobby.host_started)
	{
		short controller;

		for (controller = 1; controller < create.test_local_players && controller < 4; controller++)
		{
			struct ae_result joined = ae_lobby_join_controller(controller);

			platform_log("ae create game: test launch: player %d joins: %s", controller + 1,
				joined.ok ? "ok" : joined.reason);
		}
		create.test_local_players = 0;
	}
	/* (a test launch: START GAME pressed for it once the lobby is live and its seconds are up) */
	if (create.test_start_at && now >= create.test_start_at && live() && !lobby.host_started)
	{
		create.test_start_at = 0;
		platform_log("ae create game: test launch starts the game");
		lobby_start(0);
	}
	/* (GAME OPTIONS closed over the lobby: its changes to the server) */
	if (lobby.options_open && ae_ui_top() && ae_ui_top()->data == &lobby)
	{
		lobby.options_open = 0;
		if (live())
			ae_setup_edit_apply();
	}
	/* (0.4.0: the pause menu's changes made in game, now the lobby is back: the next map, then the copy) */
	if ((create.next_map || create.next_options) && live() && main_menu_is_active() && ae_ui_top() &&
		ae_ui_top()->data == &lobby)
	{
		if (create.next_map)
		{
			create.next_map = 0;
			platform_log("ae create game: lobby: the map picked in game");
			map_picked(create.next_map_index, create.next_map_name);
		}
		if (create.next_options)
		{
			create.next_options = 0;
			platform_log("ae create game: lobby: the options changed in game");
			ae_setup_edit_apply();
		}
	}
	/* (0.7.0: the pause's RESTART GAME, the lobby back with its picks applied: the next game at once) */
	if (live() && main_menu_is_active() && ae_ui_top() && ae_ui_top()->data == &lobby && ae_glue_pause_restart_take())
	{
		platform_log("ae create game: lobby: RESTART GAME starts the next game");
		lobby_start(0);
	}
	if (now - lobby.roster_at >= 250)
	{
		lobby.roster_at = now;
		lobby.have_roster = ae_lobby_owns_session() && ae_lobby_roster(&lobby.roster).ok;
		if (lobby.have_roster && (lobby.roster.people != lobby.logged_people ||
			lobby.roster.count - lobby.roster.people != lobby.logged_bots))
		{
			lobby.logged_people = lobby.roster.people;
			lobby.logged_bots = (short)(lobby.roster.count - lobby.roster.people);
			platform_log("ae create game: lobby: players %d, bots %d", lobby.logged_people, lobby.logged_bots);
		}
		/* (the invite: made once the game is hosted for the internet; shown by its first digits only) */
		if (create.network == NETWORK_ONLINE && !lobby.joiner)
		{
			if (!config_boolean("network.online"))
				snprintf(lobby.invite_shown, sizeof(lobby.invite_shown), "INTERNET PLAY IS OFF");
			else if (!live() || !p2p_invite_link(lobby.invite, sizeof(lobby.invite)) || !lobby.invite[0])
			{
				lobby.invite[0] = 0;
				snprintf(lobby.invite_shown, sizeof(lobby.invite_shown), "BEING MADE...");
			}
			else
			{
				char const *digits = strstr(lobby.invite, "join/");

				snprintf(lobby.invite_shown, sizeof(lobby.invite_shown), "COPY (%.8s...)", digits ? digits + 5 :
					lobby.invite);
			}
		}
	}
	/* (a joiner, or the host after START: the game runs; no AE screen over the game, the take-over brings the
	lobby back) */
	if ((lobby.joiner || lobby.host_started) && game_engine_running() && !main_menu_is_active() &&
		!game_engine_showing_postgame())
	{
		platform_log(lobby.joiner ? "ae create game: lobby: the host started" : "ae create game: lobby: the game runs");
		lobby.host_started = 0;
		ae_countdown_reset(&lobby.countdown);
		create.starting = 1;
		clear_stack();
	}
}

static int lobby_handle(struct ae_screen *instance, struct ae_event const *event)
{
	struct ae_flow_screen *flow = &lobby.flow;
	short direction;

	(void)instance;
	lobby_rows();
	party_build();
	if (countdown_press(event))
		return 1;
	if (lobby.typing_bots)
	{
		switch (event->action)
		{
		case AE_ACTION_ACCEPT:
		case AE_ACTION_START:
			bot_typing_end(1);
			break;
		case AE_ACTION_BACK:
			bot_typing_end(0);
			break;
		case AE_ACTION_LEFT:
		case AE_ACTION_RIGHT:
			ae_text_move(&lobby.typed_bots, event->action == AE_ACTION_RIGHT ? 1 : -1, 0);
			lobby.typed_bots_caret_since = ae_motion_now();
			break;
		default:
			break;
		}
		return 1;
	}
	if (split_screen_press(event, lobby.party_view))
		return 1;
	switch (event->action)
	{
	case AE_ACTION_Y:
		{
			short controller = ae_input_event_controller();

			if (controller < 0)
				controller = event->player;
			ae_sound_request(ae_lobby_change_team(controller) ? AE_SOUND_FORWARD : AE_SOUND_FAILURE, 0);
		}
		return 1;
	case AE_ACTION_BACK:
		/* (the party view: back to the left column; else the lobby left) */
		if (lobby.party_view)
		{
			party_close();
			ae_sound_request(AE_SOUND_BACK, 0);
			return 1;
		}
		leave_or_ask();
		return 1;
	/* X (F): the party view, then the whole lobby */
	case AE_ACTION_X:
		if (lobby.party_view)
			all_open();
		else
			party_open(&lobby.party_key);
		return 1;
	/* (0.3.9: RB (E) to the party, on its row last focused; LB (Q) back to the left column, on the row left there) */
	case AE_ACTION_TAB_NEXT:
	case AE_ACTION_TAB_PREVIOUS:
		if (event->action == AE_ACTION_TAB_NEXT ? lobby.party_view : !lobby.party_view)
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return 1;
		}
		if (event->action == AE_ACTION_TAB_NEXT)
			party_open(&lobby.party_key);
		else
		{
			party_close();
			ae_sound_request(AE_SOUND_CURSOR, 0);
		}
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		direction = event->action == AE_ACTION_UP ? -1 : 1;
		if (lobby.party_view)
		{
			short next = (short)(lobby.party_focus + direction);

			/* (up past its first row: the left column's) */
			if (next < 0)
			{
				party_close();
				ae_sound_request(AE_SOUND_CURSOR, event->repeat);
				return 1;
			}
			if (next >= lobby.party_count)
				return 1;
			party_focus_set(next);
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
			return 1;
		}
		/* (down from the last row into the party) */
		if (!ae_flow_focus_step(flow, direction, event->repeat) && direction > 0)
			party_open(NULL);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		direction = event->action == AE_ACTION_RIGHT ? 1 : -1;
		if (lobby.party_view)
			party_step(lobby.party_focus, direction);
		else if (!lobby.joiner)
			left_step(flow->focus, direction);
		return 1;
	case AE_ACTION_ACCEPT:
		if (lobby.party_view)
		{
			if (lobby.party[lobby.party_focus].kind == AE_PARTY_ITEM_BOTS && event->device == AE_DEVICE_KEYBOARD_MOUSE)
			{
				if (!guest_blocked("the bot rows"))
					bot_typing_begin();
			}
			else
				party_accept(lobby.party_focus);
		}
		else if (!lobby.joiner)
			left_accept(flow->focus);
		return 1;
	case AE_ACTION_START:
		start_press();
		return 1;
	default:
		return 1;
	}
}

static void lobby_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	short row;
	int at;

	if (lobby.typing_bots)
	{
		if (ae_prompts_pointer(lobby.flow.prompts, lobby.flow.prompt_count, pointer, HIT_PROMPTS, &event))
			lobby_handle(instance, &event);
		else if (pointer->left_clicks)
			bot_typing_end(1);
		return;
	}
	if (ae_prompts_pointer(lobby.flow.prompts, lobby.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		lobby_handle(instance, &event);
		return;
	}
	/* ((0.4.3) the start counting down: the host's clicks do nothing, its footer's "Cancel start" and a right click
	(its BACK) apart; a joiner still looks at the party) */
	if (ae_countdown_running(&lobby.countdown) && !lobby.joiner)
		return;
	at = ae_hit_at(pointer->x, pointer->y, 0, &hit);
	/* (the party view's "N more" line lit while the pointer is over it, as a row's hover) */
	if (pointer->moved)
		lobby.more_hover = at && hit.id == HIT_PARTY && hit.index == -2 && lobby.party_view;
	/* (a pad's inline profile card: hover focuses its row, a click picks) */
	if (ae_screen_lobby_profile_choosing_count() && at && hit.id == HIT_CARDS && hit.part == AE_PART_ITEM)
	{
		ae_screen_lobby_profile_pointer(hit.index, pointer->moved, pointer->left_clicks);
		return;
	}
	/* (0.4.2) MY PARTY: folded, a click opens the party view on the row clicked (SEE ALL the whole lobby); in the party
	view, hover focuses, a click takes the row, its arrows change the team */
	if (at && hit.id == HIT_PARTY)
	{
		/* (SEE ALL, the party view's "N more" line, and the head once the view is open: the whole lobby) */
		if (hit.index == -2 || (hit.index < 0 && lobby.party_view))
		{
			if (pointer->left_clicks)
			{
				platform_log("ae create game: lobby: all (clicked)");
				all_open();
			}
			return;
		}
		if (!lobby.party_view)
		{
			if (pointer->left_clicks)
				party_open(hit.index >= 0 && hit.index < lobby.party_count ? &lobby.party[hit.index] : NULL);
			return;
		}
		if (hit.index < 0 || hit.index >= lobby.party_count)
			return;
		if (pointer->moved || pointer->left_clicks)
			party_focus_set(hit.index);
		if (!pointer->left_clicks)
			return;
		if (hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT)
			party_step(hit.index, hit.part == AE_PART_ARROW_RIGHT ? 1 : -1);
		else if (lobby.party[hit.index].kind == AE_PARTY_ITEM_BOTS && !pointer->touch)
		{
			if (!guest_blocked("the bot rows"))
				bot_typing_begin();
		}
		else
			party_accept(hit.index);
		return;
	}
	if (lobby.joiner)
		return;
	/* (the left column's rows: hovering them takes the focus back from the party) */
	if (pointer->moved && at && hit.id == HIT_ROWS && lobby.party_view)
		party_close();
	row = ae_flow_rows_pointer(&lobby.flow, pointer);
	if (row <= -2)
		left_step(lobby.flow.focus, row == -3 ? 1 : -1);
	else if (row >= 0)
		left_accept(row);
}

static void lobby_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae create game: lobby: %s", lobby.leaving ? "left" : "off the stack");
	lobby.leaving = 0;
	if (lobby.typing_bots)
	{
		ae_field_edit_end(&lobby.typed_bots_edit, 0);
		lobby.typing_bots = 0;
	}
	if (lobby.adding)
		adding_set(0);
}

static struct ae_screen_class const lobby_class =
{
	.name = "create game", .leave = lobby_leave, .handle = lobby_handle, .draw = lobby_draw, .pointer = lobby_pointer,
	.update = lobby_update, .start_from_anyone = 1,
};

static void lobby_open(int joiner)
{
	/* (a pad choosing its profile on the main menu: its card is the menu's, it signs in again here) */
	ae_screen_lobby_profile_cancel_all();
	memset(&lobby, 0, sizeof(lobby));
	lobby.joiner = joiner;
	lobby.party_focus = -1;
	lobby.party_key.kind = lobby.party_key.card = -1;
	lobby.network_wanted = -1;
	lobby.logged_people = lobby.logged_bots = -1;
	lobby.flow.hit_rows = HIT_ROWS;
	lobby.flow.hit_prompts = HIT_PROMPTS;
	ae_lobby_set_pregame_screen(&lobby_class, &lobby);
	if (!ae_ui_push(&lobby_class, AE_OWNER_ANY, &lobby))
	{
		platform_log("ae create game: lobby: could not open");
		return;
	}
	/* (the host on MAP, the first row; a joiner on WAITING, the last) */
	lobby_rows();
	lobby.flow.focus = joiner ? (short)(lobby.flow.count - 1) : 0;
	if (joiner)
		platform_log("ae create game: lobby: open");
}

/* the host's screen pushed and the lobby opening on the network picked */
static int host_open(void)
{
	lobby_open(0);
	if (!ae_ui_holds(&lobby))
		return 0;
	platform_log("ae create game: open");
	session_open();
	return 1;
}

/* ---------- BOT SETUP */

/* a BOT SETUP row's setting (ae_bot_settings.h) */
static int bot_kind(short kind)
{
	return kind == SETUP_SHOOTING ? AE_BOT_SHOOTING : kind == SETUP_MOVEMENT ? AE_BOT_MOVEMENT :
		kind == SETUP_PLAYFIGHT ? AE_BOT_PLAYFIGHT : AE_BOT_TEAMS;
}

static void setup_rows_now(void)
{
	setup.count = 0;
	setup.kinds[setup.count++] = SETUP_SHOOTING;
	setup.kinds[setup.count++] = SETUP_MOVEMENT;
	setup.kinds[setup.count++] = SETUP_PLAYFIGHT;
	setup.kinds[setup.count++] = SETUP_TEAMS;
	if (setup.focus >= setup.count)
		setup.focus = (short)(setup.count - 1);
	if (setup.focus < 0)
		setup.focus = 0;
}

static void setup_step(short index, short direction)
{
	if (index < 0 || index >= setup.count)
		return;
	ae_bot_setting_step(bot_kind(setup.kinds[index]), direction, "ae create game: lobby");
}

/* the NETWORK pick's question answered: CHANGE opens the lobby again on it */
static void network_change_picked(short choice, void *context)
{
	(void)context;
	if (choice != 1 || lobby.network_wanted < 0 || lobby.network_wanted >= NETWORKS)
		return;
	create.network = lobby.network_wanted;
	session_reopen();
}

static void setup_done(void)
{
	ae_sound_request(AE_SOUND_BACK, 0);
	if (ae_ui_top() && ae_ui_top()->data == &setup)
		ae_ui_pop();
	else
		ae_ui_remove(&setup);
}

/* the focused row's help as wrapped in the popover; its line count */
static int setup_help_lines(float width, char lines[SETUP_HELP_LINES][160])
{
	struct ae_flow_screen *flow = &lobby.flow;
	char expanded[320];

	return ae_wrap_text(AE_FONT_BODY, ae_flow_u(flow, flow->density.metrics->body),
		ae_help_expand(ae_bot_setting_help(bot_kind(setup.kinds[setup.focus])), expanded, sizeof(expanded)), width,
		lines, SETUP_HELP_LINES);
}

/* the popover's box over the party (layout units), its height from what it holds */
static struct ae_rect setup_box(void)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_density const *d = &flow->density;
	float panel_x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X;
	float inset = ae_flow_u(flow, AE_FLOW_INSET_U), pad = ae_flow_u(flow, SETUP_PAD_U);
	float line = ae_flow_u(flow, d->metrics->body_line), gap = ae_flow_u(flow, d->metrics->gap);
	struct ae_rect box;
	char lines[SETUP_HELP_LINES][160];
	int count;

	box.x = panel_x + inset;
	box.y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y) + inset;
	box.width = ae_flow_right_x(flow) - panel_x - 2.0f * inset;
	count = setup_help_lines(box.width - 2.0f * pad, lines);
	box.height = ae_flow_u(flow, SETUP_STRIPE_U) + pad + ae_flow_u(flow, SETUP_TITLE_U) * 1.4f + gap +
		(float)setup.count * (ae_size_row(d) + gap) + ae_flow_u(flow, 8.0f) + (float)count * line + pad * 0.75f +
		ae_flow_u(flow, 48.0f) + pad * 0.5f;
	return box;
}

static void setup_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_density const *d = &flow->density;
	struct ae_rect box;
	struct ae_prompt prompts[2];
	float pad = ae_flow_u(flow, SETUP_PAD_U), title = ae_flow_u(flow, SETUP_TITLE_U), gap = ae_flow_u(flow, d->metrics->gap);
	float body = ae_flow_u(flow, d->metrics->body), line = ae_flow_u(flow, d->metrics->body_line), x, y, width;
	char lines[SETUP_HELP_LINES][160];
	static const char *const labels[SETUP_ROWS] = { "SHOOTING SKILL", "MOVEMENT SKILL", "PLAYFIGHT", "BOT TEAMS" };
	short index;
	int count;

	(void)instance;
	setup_rows_now();
	box = setup_box();
	/* the scrim (wider than the view: the popover's open scales it about the middle) and the clicks outside */
	ae_draw_rect(-(float)AE_LAYOUT_HEIGHT, -(float)AE_LAYOUT_HEIGHT, ae_draw_view_width() + 2.0f * AE_LAYOUT_HEIGHT,
		3.0f * AE_LAYOUT_HEIGHT, 0.0f, AE_COLOR_SCRIM);
	ae_hit_add(0.0f, 0.0f, ae_draw_view_width(), (float)AE_LAYOUT_HEIGHT, HIT_SETUP, AE_PART_OUTSIDE, -1);
	ae_draw_rect(box.x, box.y, box.width, box.height, ae_flow_u(flow, d->metrics->corner), AE_COLOR_POPOVER);
	ae_hit_add(box.x, box.y, box.width, box.height, HIT_SETUP, AE_PART_CARD, -1);
	ae_draw_rect(box.x, box.y, box.width, ae_flow_u(flow, SETUP_STRIPE_U), 0.0f, AE_COLOR_ACCENT);
	x = box.x + pad;
	width = box.width - 2.0f * pad;
	y = box.y + ae_flow_u(flow, SETUP_STRIPE_U) + pad;
	ae_draw_text(AE_FONT_ROW, title, x, ae_flow_cap_top(AE_FONT_ROW, title, y + title * 0.7f), AE_ALIGN_LEFT,
		AE_COLOR_TITLE, "BOT SETUP");
	y += title * 1.4f + gap;
	for (index = 0; index < setup.count; index++)
	{
		struct ae_row widget;

		memset(&widget, 0, sizeof(widget));
		if (index == setup.focus)
			widget.flags |= AE_ROW_FOCUSED;
		widget.label = labels[setup.kinds[index]];
		widget.value = ae_bot_setting_text(bot_kind(setup.kinds[index]));
		y += ae_widget_row(d, x, y, width, &widget, HIT_SETUP_ROWS, index) + gap;
	}
	y += ae_flow_u(flow, 8.0f);
	count = setup_help_lines(width, lines);
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, x, ae_flow_cap_top(AE_FONT_BODY, body, y + line * ((float)index + 0.5f)),
			AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
	y += (float)count * line + pad * 0.75f;
	prompts[0] = ae_flow_prompt(AE_BUTTON_A, ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
	prompts[1] = ae_flow_prompt(AE_BUTTON_B, "Done", AE_ACTION_BACK);
	ae_widget_prompts(d, x, y + ae_flow_u(flow, 24.0f), prompts, 2, NULL, x + width, -1, HIT_SETUP_PROMPTS);
}

static int setup_handle(struct ae_screen *instance, struct ae_event const *event)
{
	short next;

	(void)instance;
	setup_rows_now();
	if ((event->action == AE_ACTION_LEFT || event->action == AE_ACTION_RIGHT || event->action == AE_ACTION_ACCEPT) &&
		guest_blocked("the bot rows"))
		return 1;
	switch (event->action)
	{
	case AE_ACTION_BACK:
	case AE_ACTION_X:
		setup_done();
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		next = (short)(setup.focus + (event->action == AE_ACTION_UP ? -1 : 1));
		if (next >= 0 && next < setup.count)
		{
			setup.focus = next;
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		}
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		setup_step(setup.focus, event->action == AE_ACTION_RIGHT ? 1 : -1);
		return 1;
	case AE_ACTION_ACCEPT:
		/* (A takes the next value, round past the last) */
		setup_step(setup.focus, 1);
		return 1;
	default:
		return 1;
	}
}

static void setup_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	struct ae_prompt prompts[2];

	prompts[0] = ae_flow_prompt(AE_BUTTON_A, ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
	prompts[1] = ae_flow_prompt(AE_BUTTON_B, "Done", AE_ACTION_BACK);
	if (ae_prompts_pointer(prompts, 2, pointer, HIT_SETUP_PROMPTS, &event))
	{
		setup_handle(instance, &event);
		return;
	}
	if (!ae_hit_at(pointer->x, pointer->y, 0, &hit))
		return;
	/* (a click outside: closed) */
	if (hit.id == HIT_SETUP && hit.part == AE_PART_OUTSIDE && pointer->left_clicks)
	{
		setup_done();
		return;
	}
	if (hit.id != HIT_SETUP_ROWS || hit.index < 0 || hit.index >= setup.count)
		return;
	if (pointer->moved)
		setup.focus = hit.index;
	if (pointer->left_clicks)
	{
		if (hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT)
			setup_step(hit.index, hit.part == AE_PART_ARROW_RIGHT ? 1 : -1);
		else
		{
			setup.focus = hit.index;
			event.action = AE_ACTION_ACCEPT;
			setup_handle(instance, &event);
		}
	}
}

static struct ae_screen_class const setup_class =
{
	.name = "bot setup", .handle = setup_handle, .draw = setup_draw, .pointer = setup_pointer, .popover = 1,
};

static void setup_open(void)
{
	if (lobby.joiner || ae_ui_holds(&setup))
		return;
	memset(&setup, 0, sizeof(setup));
	setup_rows_now();
	if (!ae_ui_push(&setup_class, AE_OWNER_ANY, &setup))
		return;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("ae create game: lobby: setup open");
}

/* ---------- MORE OPTIONS (0.4.2) */

/* the popover's own words: PRIMARY's and SECONDARY's (a pick sets LOADOUT to CUSTOM; the packs are right under them) */
#define MORE_PRIMARY_HELP "The weapon everyone spawns with in hand; picking one sets LOADOUT to CUSTOM. The list has " \
	"the stock weapons and the packs that are on (WEAPON PACKS)."
#define MORE_SECONDARY_HELP "The weapon everyone spawns with on their back; picking one sets LOADOUT to CUSTOM. The " \
	"list has the stock weapons and the packs that are on (WEAPON PACKS)."
#define MORE_VEHICLES_CUSTOM_HELP "The counts of each vehicle GAME OPTIONS sets (VEHICLES)."

/* a pack's row (ae_setup_options.h's AE_SO_PACK_FIRST + slot - 1) */
static int more_pack(short option)
{
	return option >= AE_SO_PACK_FIRST && option < AE_SO_PACK_FIRST + AE_SETUP_PACKS;
}

/* whether the popover shows an option now: PRIMARY and SECONDARY whatever LOADOUT says, a pack this PC has (or one on
in the gametype, so it can be turned off), the rest as GAME OPTIONS shows them (FRIENDLY FIRE in team games only) */
static int more_shown(struct ae_setup_access const *access, short option)
{
	if (option == AE_SO_PRIMARY || option == AE_SO_SECONDARY)
		return ae_setup_applies(access, option);
	if (more_pack(option))
		return ae_setup_applies(access, option) && !ae_setup_edit_reason(option);
	return ae_setup_visible(access, option);
}

/* a row's name: VEHICLE SET for red's set (in a team game both teams' are set together) */
static const char *more_label(struct ae_setup_access const *access, short option)
{
	return option == AE_SO_VEHICLE_SET_RED ? "VEHICLE SET" : ae_setup_label(access, option);
}

/* the list, the focus on its row again */
static void more_rows_now(void)
{
	static const short options[] = { AE_SO_LOADOUT, AE_SO_PRIMARY, AE_SO_SECONDARY, MORE_HEADER_PACKS,
		AE_SO_STARTING_GRENADES, AE_SO_WEAPON_SET, AE_SO_MOTION_SENSOR, AE_SO_FRIENDLY_FIRE, AE_SO_VEHICLE_SET_RED,
		AE_SO_LIVES };
	struct ae_setup_access access = access_now();
	short index, slot, packs = 0;

	more.count = 0;
	for (index = 0; index < (short)(sizeof(options) / sizeof(options[0])); index++)
	{
		if (options[index] != MORE_HEADER_PACKS)
		{
			if (more_shown(&access, options[index]))
				more.entries[more.count++] = options[index];
			continue;
		}
		/* (the packs under their header; the rows after them under RULES, so they don't read as packs) */
		for (slot = 1; slot <= AE_SETUP_PACKS; slot++)
			packs += (short)more_shown(&access, (short)(AE_SO_PACK_FIRST + slot - 1));
		if (!packs)
			continue;
		more.entries[more.count++] = MORE_HEADER_PACKS;
		for (slot = 1; slot <= AE_SETUP_PACKS; slot++)
			if (more_shown(&access, (short)(AE_SO_PACK_FIRST + slot - 1)))
				more.entries[more.count++] = (short)(AE_SO_PACK_FIRST + slot - 1);
		more.entries[more.count++] = MORE_HEADER_RULES;
	}
	for (index = 0; index < more.count && more.entries[index] != more.focus_option; index++)
		;
	if (index < more.count)
		more.focus = index;
	if (more.focus >= more.count)
		more.focus = (short)(more.count - 1);
	if (more.focus < 0)
		more.focus = 0;
	while (more.focus < more.count - 1 && more.entries[more.focus] < 0)
		more.focus++;
	more.focus_option = more.count ? more.entries[more.focus] : -1;
}

/* the values a row offers: as GAME OPTIONS' (PRIMARY / SECONDARY the weapons that can start); VEHICLE SET's CUSTOM only
while it is the set (its counts are GAME OPTIONS' job) */
static int more_offered(struct ae_setup_access const *access, short option, short value)
{
	if (option == AE_SO_VEHICLE_SET_RED && value == AE_SETUP_VEHICLE_SET_CUSTOM &&
		access->get(access->context, option) != value)
		return 0;
	return ae_setup_value_offered(access, option, value);
}

/* the next offered value from current (round past the ends); current when it is the only one */
static short more_value_step(struct ae_setup_access const *access, short option, short current, short direction)
{
	struct ae_setup_row const *row = ae_setup_row(option);
	short next = current, tries;

	if (!row || row->value_count <= 0)
		return current;
	for (tries = 0; tries < row->value_count; tries++)
	{
		next = (short)((next + (direction < 0 ? -1 : 1) + row->value_count) % row->value_count);
		if (next == current)
			break;
		if (more_offered(access, option, next))
			return next;
	}
	return current;
}

/* a row set (live: the server's at once); a start weapon picked makes the loadout CUSTOM, a team game's VEHICLE SET
is both teams' */
static void more_set(short option, short value)
{
	struct ae_setup_access access = access_now();

	if (value == access.get(access.context, option))
		return;
	ae_setup_edit_set(option, value);
	if (option == AE_SO_VEHICLE_SET_RED && ae_setup_team(&access) && ae_setup_applies(&access, AE_SO_VEHICLE_SET_BLUE))
		ae_setup_edit_set(AE_SO_VEHICLE_SET_BLUE, value);
	platform_log("ae create game: lobby: more %s %s", more_label(&access, option), ae_setup_value_text(option, value));
	if ((option == AE_SO_PRIMARY || option == AE_SO_SECONDARY) && ae_setup_applies(&access, AE_SO_LOADOUT) &&
		access.get(access.context, AE_SO_LOADOUT) != AE_SETUP_LOADOUT_CUSTOM)
	{
		ae_setup_edit_set(AE_SO_LOADOUT, AE_SETUP_LOADOUT_CUSTOM);
		platform_log("ae create game: lobby: more %s %s", ae_setup_label(&access, AE_SO_LOADOUT),
			ae_setup_value_text(AE_SO_LOADOUT, AE_SETUP_LOADOUT_CUSTOM));
	}
	if (live())
		ae_setup_edit_apply();
}

static void more_step(short option, short direction)
{
	struct ae_setup_access access = access_now();
	int current = access.get(access.context, option);
	short next;

	/* (a pack this PC does not have can't be turned on) */
	if (current < 0 || ae_setup_edit_reason(option))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	next = more_value_step(&access, option, (short)current, direction);
	if (next == current)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
	more_set(option, next);
}

/* a row's list as offered, and their indexes (read by the open list: static) */
static struct { const char *texts[64]; short indexes[64]; short count; } more_map;

static void more_picked(short index, void *context)
{
	if (index < 0 || index >= more_map.count)
		return;
	more_set((short)(intptr_t)context, more_map.indexes[index]);
}

/* A on a value row: its list over the row, its name over the values */
static void more_dropdown(short entry)
{
	struct ae_setup_access access = access_now();
	short option = more.entries[entry], current = (short)access.get(access.context, option), value, at = 0;
	struct ae_setup_row const *row = ae_setup_row(option);
	struct ae_picker_spec spec;

	if (!row || current < 0 || more.rects[entry].width <= 0.0f)
		return;
	more_map.count = 0;
	for (value = 0; value < row->value_count && more_map.count < 64; value++)
	{
		if (!more_offered(&access, option, value))
			continue;
		if (value == current)
			at = more_map.count;
		more_map.indexes[more_map.count] = value;
		more_map.texts[more_map.count++] = row->values[value];
	}
	memset(&spec, 0, sizeof(spec));
	spec.values = more_map.texts;
	spec.count = more_map.count;
	spec.current = at;
	spec.row = more.rects[entry];
	spec.bounds = lobby.flow.frame.rect;
	spec.density = lobby.flow.density;
	spec.picked = more_picked;
	spec.context = (void *)(intptr_t)option;
	spec.title = more_label(&access, option);
	if (ae_picker_open(&spec, AE_OWNER_ANY))
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* the focused row's help (a refusal in its place for a few seconds) */
static const char *more_help(void)
{
	struct ae_setup_access access = access_now();
	short option = more.focus_option;
	int value = option >= 0 ? access.get(access.context, option) : -1;

	if (refusal_showing())
		return refusal_text;
	if (option == AE_SO_PRIMARY)
		return MORE_PRIMARY_HELP;
	if (option == AE_SO_SECONDARY)
		return MORE_SECONDARY_HELP;
	if (option == AE_SO_VEHICLE_SET_RED && value == AE_SETUP_VEHICLE_SET_CUSTOM)
		return MORE_VEHICLES_CUSTOM_HELP;
	if (more_pack(option))
	{
		const char *pack = ae_setup_pack_help(&access, option, 1, more.help, sizeof(more.help));

		return pack ? pack : "";
	}
	return option >= 0 ? ae_setup_help(option, value) : "";
}

static int more_help_lines(float width, char lines[SETUP_HELP_LINES][160])
{
	char expanded[320];

	return ae_wrap_text(AE_FONT_BODY, ae_flow_u(&lobby.flow, lobby.flow.density.metrics->body),
		ae_help_expand(more_help(), expanded, sizeof(expanded)), width, lines, SETUP_HELP_LINES);
}

/* an entry's height with the gap under it (a header's has none) */
static float more_pitch(short index)
{
	struct ae_density const *d = &lobby.flow.density;

	if (more.entries[index] < 0)
		return ae_widget_group(d, 0.0f, 0.0f, 0.0f, NULL);
	return ae_size_row(d) + ae_flow_u(&lobby.flow, d->metrics->gap);
}

/* a row as the popover draws it: the value (VEHICLE SET red's), the AE badge, the changed dot and where it started */
static void more_row(struct ae_setup_access const *access, short index, struct ae_row *widget)
{
	short option = more.entries[index];
	struct ae_setup_row const *row = ae_setup_row(option);

	memset(widget, 0, sizeof(*widget));
	widget->label = more_label(access, option);
	widget->value = ae_setup_value_text(option, access->get(access->context, option));
	if (index == more.focus)
		widget->flags |= AE_ROW_FOCUSED;
	if (row && row->source != AE_SRC_CLASSIC)
	{
		widget->flags |= AE_ROW_AE;
		widget->source_badge = ae_setup_source_badge(row);
	}
	if (access->changed(access->context, option))
	{
		widget->flags |= AE_ROW_CHANGED;
		widget->base_value = ae_setup_value_text(option, ae_setup_edit_base(option));
	}
}

/* the widest row it holds, focused (its arrows) with its widest value: the label, its badge, 16 u, the value's block */
static float more_rows_width(void)
{
	struct ae_density const *d = &lobby.flow.density;
	struct ae_setup_access access = access_now();
	float widest = 0.0f, wide = 10000.0f;
	short index, value;

	for (index = 0; index < more.count; index++)
	{
		struct ae_setup_row const *row = ae_setup_row(more.entries[index]);
		struct ae_row widget;
		struct ae_row_layout layout;
		const char *longest = "";
		float width;

		if (more.entries[index] < 0 || !row)
			continue;
		for (value = 0; value < row->value_count; value++)
		{
			if (more_offered(&access, more.entries[index], value) && ae_draw_text_width(AE_FONT_ROW, ae_size_text(d),
				row->values[value]) > ae_draw_text_width(AE_FONT_ROW, ae_size_text(d), longest))
				longest = row->values[value];
		}
		memset(&widget, 0, sizeof(widget));
		widget.label = more_label(&access, more.entries[index]);
		widget.value = longest;
		widget.flags = AE_ROW_FOCUSED | (row->source != AE_SRC_CLASSIC ? AE_ROW_AE : 0u);
		widget.source_badge = ae_setup_source_badge(row);
		ae_row_layout(d, wide, &widget, &layout);
		width = (layout.badge ? layout.badge_x + layout.badge_width : layout.label_x + layout.label_width) +
			ae_flow_u(&lobby.flow, 16.0f) + wide - layout.arrow_left_x;
		if (width > widest)
			widest = width;
	}
	return widest;
}

/* the box's height but its rows': the stripe, the title, the help's lines (all three kept: the rows stay put as the
focus moves), the prompts */
static float more_chrome(void)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_density const *d = &flow->density;
	float pad = ae_flow_u(flow, SETUP_PAD_U);

	return ae_flow_u(flow, SETUP_STRIPE_U) + pad + ae_flow_u(flow, SETUP_TITLE_U) * 1.4f +
		ae_flow_u(flow, d->metrics->gap) + ae_flow_u(flow, 8.0f) + (float)SETUP_HELP_LINES *
		ae_flow_u(flow, d->metrics->body_line) + pad * 0.75f + ae_flow_u(flow, 48.0f) + pad * 0.5f;
}

/* the box (layout units): BOT SETUP's, wider to the left when a row would be cut, no lower than MORE_BOTTOM_U over
the screen's bottom; *room: the rows' height in it */
static struct ae_rect more_box(float *room)
{
	struct ae_flow_screen *flow = &lobby.flow;
	float panel_x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X;
	float inset = ae_flow_u(flow, AE_FLOW_INSET_U), pad = ae_flow_u(flow, SETUP_PAD_U), right, needed, least, rows = 0.0f;
	float bottom = flow->frame.rect.y + flow->frame.rect.height - ae_flow_u(flow, MORE_BOTTOM_U);
	struct ae_rect box;
	short index;

	box.x = panel_x + inset;
	box.y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y) + inset;
	right = ae_flow_right_x(flow) - inset;
	needed = more_rows_width() + 2.0f * pad;
	least = ae_flow_left_x(flow) + flow->frame.rect.width * AE_FLOW_ROWS_WIDTH + ae_flow_u(flow, MORE_CLEAR_U);
	if (right - box.x < needed)
		box.x = right - needed > least ? right - needed : least;
	box.width = right - box.x;
	for (index = 0; index < more.count; index++)
		rows += more_pitch(index);
	box.height = more_chrome() + rows;
	if (box.y + box.height > bottom)
		box.height = bottom - box.y;
	*room = box.height - more_chrome();
	return box;
}

/* the last entry drawn from first in room: the "▲ N more" line's room taken when rows are above, the "▼ N more"
line's when rows are left below; first at the least */
static short more_last_from(short first, float room)
{
	float line = ae_widget_more_line(&lobby.flow.density), used;
	short index, pass;

	for (pass = 0; pass < 2; pass++)
	{
		used = (first > 0 ? line : 0.0f) + (pass ? line : 0.0f);
		for (index = first; index < more.count; index++)
		{
			used += more_pitch(index);
			if (used > room)
				break;
		}
		if (index >= more.count)
			return (short)(more.count - 1);
		if (pass)
			return index > first ? (short)(index - 1) : first;
	}
	return first;
}

/* first moved so the focus shows (its header with it when it is the first row under one), back up while all below fit */
static void more_window(float room)
{
	short focus = more.focus;

	if (more.first < 0 || more.first >= more.count)
		more.first = 0;
	if (focus < more.first)
		more.first = focus;
	if (more.first == focus && focus > 0 && more.entries[focus - 1] < 0)
		more.first = (short)(focus - 1);
	while (more.first < focus && more_last_from(more.first, room) < focus)
		more.first++;
	while (more.first > 0 && more_last_from((short)(more.first - 1), room) == more.count - 1)
		more.first--;
	more.last = more_last_from(more.first, room);
	/* (a section's header is never the last drawn: it goes with its first row) */
	while (more.last > more.first && more.entries[more.last] < 0)
		more.last--;
}

/* the rows (not headers) from first to last */
static short more_rows_between(short first, short last)
{
	short index, count = 0;

	for (index = first; index <= last; index++)
		count += more.entries[index] >= 0;
	return count;
}

static void more_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_density const *d = &flow->density;
	struct ae_setup_access access = access_now();
	struct ae_rect box;
	struct ae_prompt prompts[2];
	float pad = ae_flow_u(flow, SETUP_PAD_U), title = ae_flow_u(flow, SETUP_TITLE_U), gap = ae_flow_u(flow, d->metrics->gap);
	float body = ae_flow_u(flow, d->metrics->body), line = ae_flow_u(flow, d->metrics->body_line);
	float more_line = ae_widget_more_line(d), x, y, width, room, rows_top;
	unsigned int fade = AE_COLOR_POPOVER & 0xFFFFFF00u;
	char lines[SETUP_HELP_LINES][160];
	short index;
	int count;

	(void)instance;
	more_rows_now();
	box = more_box(&room);
	more_window(room);
	/* the scrim (wider than the view: the popover's open scales it about the middle) and the clicks outside */
	ae_draw_rect(-(float)AE_LAYOUT_HEIGHT, -(float)AE_LAYOUT_HEIGHT, ae_draw_view_width() + 2.0f * AE_LAYOUT_HEIGHT,
		3.0f * AE_LAYOUT_HEIGHT, 0.0f, AE_COLOR_SCRIM);
	ae_hit_add(0.0f, 0.0f, ae_draw_view_width(), (float)AE_LAYOUT_HEIGHT, HIT_MORE, AE_PART_OUTSIDE, -1);
	ae_draw_rect(box.x, box.y, box.width, box.height, ae_flow_u(flow, d->metrics->corner), AE_COLOR_POPOVER);
	ae_hit_add(box.x, box.y, box.width, box.height, HIT_MORE, AE_PART_CARD, -1);
	ae_draw_rect(box.x, box.y, box.width, ae_flow_u(flow, SETUP_STRIPE_U), 0.0f, AE_COLOR_ACCENT);
	x = box.x + pad;
	width = box.width - 2.0f * pad;
	y = box.y + ae_flow_u(flow, SETUP_STRIPE_U) + pad;
	ae_draw_text(AE_FONT_ROW, title, x, ae_flow_cap_top(AE_FONT_ROW, title, y + title * 0.7f), AE_ALIGN_LEFT,
		AE_COLOR_TITLE, "MORE OPTIONS");
	y += title * 1.4f + gap;
	rows_top = y;
	if (more.first > 0)
	{
		ae_widget_more(d, x + width * 0.5f, y, 1, more_rows_between(0, (short)(more.first - 1)));
		y += more_line;
	}
	for (index = 0; index < more.count; index++)
	{
		struct ae_row widget;

		more.rects[index].width = 0.0f;
		if (index < more.first || index > more.last)
			continue;
		if (more.entries[index] < 0)
		{
			y += ae_widget_group(d, x, y, width, more.entries[index] == MORE_HEADER_PACKS ? "WEAPON PACKS" : "RULES");
			continue;
		}
		more_row(&access, index, &widget);
		more.rects[index].x = x;
		more.rects[index].y = y;
		more.rects[index].width = width;
		more.rects[index].height = ae_widget_row(d, x, y, width, &widget, HIT_MORE_ROWS, index);
		y += more.rects[index].height + gap;
	}
	/* (a cut edge: its row under the list's fade, as lists have it) */
	if (more.first > 0 && more.rects[more.first].width > 0.0f)
		ae_draw_gradient(x, more.rects[more.first].y, width, more.rects[more.first].height, 0.0f, fade | 0xA0u, fade);
	if (more.last < more.count - 1)
	{
		if (more.rects[more.last].width > 0.0f)
			ae_draw_gradient(x, more.rects[more.last].y, width, more.rects[more.last].height, 0.0f, fade,
				fade | 0xA0u);
		ae_widget_more(d, x + width * 0.5f, y, 0, more_rows_between((short)(more.last + 1), (short)(more.count - 1)));
	}
	y = rows_top + room + ae_flow_u(flow, 8.0f);
	count = more_help_lines(width, lines);
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, x, ae_flow_cap_top(AE_FONT_BODY, body, y + line * ((float)index + 0.5f)),
			AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
	y += (float)SETUP_HELP_LINES * line + pad * 0.75f;
	prompts[0] = ae_flow_prompt(AE_BUTTON_A, ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
	prompts[1] = ae_flow_prompt(AE_BUTTON_B, "Done", AE_ACTION_BACK);
	ae_widget_prompts(d, x, y + ae_flow_u(flow, 24.0f), prompts, 2, NULL, x + width, -1, HIT_MORE_PROMPTS);
}

static void more_done(void)
{
	ae_sound_request(AE_SOUND_BACK, 0);
	if (ae_ui_top() && ae_ui_top()->data == &more)
		ae_ui_pop();
	else
		ae_ui_remove(&more);
}

static void more_focus_set(short index)
{
	if (index < 0 || index >= more.count || more.entries[index] < 0)
		return;
	more.focus = index;
	more.focus_option = more.entries[index];
}

/* up / down past headers; an end stays the end */
static void more_focus_step(short direction, int repeat)
{
	short next = more.focus;

	do
		next = (short)(next + direction);
	while (next >= 0 && next < more.count && more.entries[next] < 0);
	if (next < 0 || next >= more.count)
		return;
	more_focus_set(next);
	ae_sound_request(AE_SOUND_CURSOR, repeat);
}

/* A: a pack turned on or off, another row's list */
static void more_accept(short index)
{
	if (index < 0 || index >= more.count || more.entries[index] < 0)
		return;
	if (more_pack(more.entries[index]))
		more_step(more.entries[index], 1);
	else
		more_dropdown(index);
}

static int more_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	more_rows_now();
	if ((event->action == AE_ACTION_LEFT || event->action == AE_ACTION_RIGHT || event->action == AE_ACTION_ACCEPT) &&
		guest_blocked("the game's rules"))
		return 1;
	switch (event->action)
	{
	case AE_ACTION_BACK:
	case AE_ACTION_X:
		more_done();
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		more_focus_step(event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (more.focus_option >= 0)
			more_step(more.focus_option, event->action == AE_ACTION_RIGHT ? 1 : -1);
		return 1;
	case AE_ACTION_ACCEPT:
		more_accept(more.focus);
		return 1;
	default:
		return 1;
	}
}

static void more_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	struct ae_prompt prompts[2];

	prompts[0] = ae_flow_prompt(AE_BUTTON_A, ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
	prompts[1] = ae_flow_prompt(AE_BUTTON_B, "Done", AE_ACTION_BACK);
	if (ae_prompts_pointer(prompts, 2, pointer, HIT_MORE_PROMPTS, &event))
	{
		more_handle(instance, &event);
		return;
	}
	/* (the wheel: the focus a row a step, the rows scrolling with it) */
	if (pointer->wheel_steps)
	{
		more_rows_now();
		more_focus_step(pointer->wheel_steps > 0 ? -1 : 1, 0);
		return;
	}
	if (!ae_hit_at(pointer->x, pointer->y, 0, &hit))
		return;
	/* (a click outside: closed) */
	if (hit.id == HIT_MORE && hit.part == AE_PART_OUTSIDE && pointer->left_clicks)
	{
		more_done();
		return;
	}
	if (hit.id != HIT_MORE_ROWS || hit.index < 0 || hit.index >= more.count || more.entries[hit.index] < 0)
		return;
	if (pointer->moved || pointer->left_clicks)
		more_focus_set(hit.index);
	if (!pointer->left_clicks || guest_blocked("the game's rules"))
		return;
	if (hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT)
		more_step(more.entries[hit.index], hit.part == AE_PART_ARROW_RIGHT ? 1 : -1);
	else
		more_accept(hit.index);
}

static struct ae_screen_class const more_class =
{
	.name = "more options", .handle = more_handle, .draw = more_draw, .pointer = more_pointer, .popover = 1,
};

static void more_open(void)
{
	if (lobby.joiner || ae_ui_holds(&more))
		return;
	memset(&more, 0, sizeof(more));
	more.focus_option = AE_SO_LOADOUT;
	more_rows_now();
	if (!ae_ui_push(&more_class, AE_OWNER_ANY, &more))
		return;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("ae create game: lobby: more open");
}

/* ---------- LOBBY, the whole lobby (0.4.2: SEE ALL; big lobbies) */

/* a player's column: a team game's team (0 red, 1 blue), else the one */
static short all_column_of(short card)
{
	return lobby.roster.team_game && lobby.roster.players[card].team ? 1 : 0;
}

static short all_columns(void)
{
	return lobby.have_roster && lobby.roster.team_game ? 2 : 1;
}

/* a column's players (roster indexes) in the party's order: this PC's people, the others, the bots */
static short all_list(short column, short *cards)
{
	static struct ae_party_item items[AE_PARTY_ITEMS];
	struct ae_party_rules rules;
	short count, index, listed = 0;

	if (!lobby.have_roster)
		return 0;
	memset(&rules, 0, sizeof(rules));
	count = ae_party_items(&lobby.roster, &rules, items);
	for (index = 0; index < count; index++)
		if (items[index].card >= 0 && all_column_of(items[index].card) == column)
			cards[listed++] = items[index].card;
	return listed;
}

static void all_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &all.flow;
	struct ae_density const *d;
	short columns = all_columns(), column;
	float top, left, right, column_width, gap, head, row_gap, pitch, size;
	char where[96], count_text[24], status[200];
	static short cards[AE_LOBBY_MAXIMUM_PLAYERS];

	(void)instance;
	ae_flow_full_density(flow);
	d = &flow->density;
	/* (its own page over the lobby: the backdrop darkened under it, as the approved draft) */
	ae_draw_rect(-(float)AE_LAYOUT_HEIGHT, -(float)AE_LAYOUT_HEIGHT, ae_draw_view_width() + 2.0f * AE_LAYOUT_HEIGHT,
		3.0f * AE_LAYOUT_HEIGHT, 0.0f, AE_COLOR_FADE);
	ae_flow_title(flow, "LOBBY", lobby.joiner ? "LOBBY" : "CREATE GAME");
	top = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	left = ae_flow_left_x(flow);
	right = ae_flow_right_x(flow);
	size = ae_flow_u(flow, 18.0f);
	party_where(where, sizeof(where), count_text, sizeof(count_text));
	{
		int people = lobby.have_roster ? lobby.roster.people : 0;
		int bots = lobby.have_roster ? lobby.roster.count - lobby.roster.people : 0;

		snprintf(status, sizeof(status), "%s%s%s \xE2\x80\xA2 %d %s, %d bot%s", where, count_text[0] ? " \xE2\x80\xA2 " : "",
			count_text, people, people == 1 ? "person" : "people", bots, bots == 1 ? "" : "s");
	}
	ae_draw_text(AE_FONT_ROW, size, left, ae_flow_cap_top(AE_FONT_ROW, size, top + ae_flow_u(flow, ALL_STATUS_U)),
		AE_ALIGN_LEFT, AE_COLOR_TEXT, status);
	gap = ae_flow_u(flow, ALL_GAP_U);
	head = ae_flow_u(flow, ALL_HEAD_U);
	row_gap = ae_flow_u(flow, 6.0f);
	pitch = ae_party_row_height(d, AE_PARTY_BOT) + row_gap;
	column_width = (right - left - gap) * 0.5f;
	/* (the focus follows its player: a team change moves it to the other column) */
	if (all.focus >= 0 && (!lobby.have_roster || all.focus >= lobby.roster.count))
		all.focus = -1;
	if (all.focus >= 0)
		all.column = all_column_of(all.focus);
	if (all.column >= columns)
		all.column = 0;
	for (column = 0; column < columns; column++)
	{
		float x = left + (float)column * (column_width + gap), y = top + ae_flow_u(flow, ALL_COLUMNS_Y_U);
		float bottom = ae_flow_content_bottom(flow) - ae_widget_more_line(d), list_top;
		short count = all_list(column, cards), visible, first, focus_at = -1, index;
		unsigned int color = columns == 2 ? (column ? 0x2E86DEFFu : 0xC0392BFFu) : AE_COLOR_ACCENT;
		char label[32];

		/* the column's head: the team's name and count on its colour */
		ae_draw_rect(x, y, column_width, head, 0.0f, (color & 0xFFFFFF00u) | 0x8Cu);
		ae_draw_rect(x, y, ae_flow_u(flow, 5.0f), head, 0.0f, color);
		ae_draw_text(AE_FONT_ROW, size, x + ae_flow_u(flow, 18.0f), ae_flow_cap_top(AE_FONT_ROW, size, y + head * 0.5f),
			AE_ALIGN_LEFT, AE_COLOR_TITLE, columns == 2 ? (column ? "BLUE TEAM" : "RED TEAM") : "PLAYERS");
		snprintf(label, sizeof(label), "%d", count);
		ae_draw_text(AE_FONT_ROW, size, x + column_width - ae_flow_u(flow, 16.0f), ae_flow_cap_top(AE_FONT_ROW, size,
			y + head * 0.5f), AE_ALIGN_RIGHT, AE_COLOR_TITLE, label);
		y += head + ae_flow_u(flow, 8.0f);
		list_top = y;
		visible = (short)((bottom - y) / pitch);
		if (visible < 1)
			visible = 1;
		for (index = 0; index < count; index++)
			if (cards[index] == all.focus)
				focus_at = index;
		first = all.first[column];
		if (focus_at >= 0 && focus_at < first)
			first = focus_at;
		if (focus_at >= first + visible)
			first = (short)(focus_at - visible + 1);
		if (first > count - visible)
			first = (short)(count - visible);
		if (first < 0)
			first = 0;
		all.first[column] = first;
		if (first > 0)
			ae_widget_more(d, x + column_width * 0.8f, y - head - ae_widget_more_line(d), 1, first);
		for (index = first; index < count && index < first + visible; index++)
		{
			struct ae_lobby_player const *player = &lobby.roster.players[cards[index]];
			struct ae_party_row row;
			int focused = cards[index] == all.focus;

			party_player_row(cards[index], focused, &row);
			/* (36 u rows: this PC's people filled, the others a tint of their colour, the bots dark) */
			row.kind = player->bot ? AE_PARTY_BOT : player->local ? AE_PARTY_SELF : AE_PARTY_MEMBER;
			if (player->local && !player->host)
				row.badge = "THIS PC";
			/* (the rows from the column's edge: no party indent) */
			y += ae_widget_party_row(d, x - ae_flow_u(flow, AE_PARTY_INDENT_U), y, column_width +
				ae_flow_u(flow, AE_PARTY_INDENT_U), &row, HIT_ALL_ROWS, cards[index]);
		}
		if (first + visible < count)
			ae_widget_more(d, x + column_width * 0.5f, y, 0, count - first - visible);
		/* its scrollbar: more than it shows */
		if (count > visible)
		{
			float track = ae_flow_u(flow, ALL_TRACK_U), height = (float)visible * pitch, thumb_y, thumb_height;

			ae_scrollbar_thumb(count, visible, (float)first, height, pitch, &thumb_y, &thumb_height);
			ae_draw_rect(x + column_width + track * 2.0f, list_top, track, height, track * 0.5f, AE_COLOR_TRACK);
			ae_draw_rect(x + column_width + track * 2.0f, list_top + thumb_y, track, thumb_height, track * 0.5f,
				AE_COLOR_MUTED);
		}
	}
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	if (columns == 2)
	{
		flow->prompts[flow->prompt_count] = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Column", AE_ACTION_TAB_PREVIOUS);
		flow->prompts[flow->prompt_count++].pair = 1;
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Change team", AE_ACTION_Y);
	}
	if (all.focus >= 0 && card_arrows(all.focus))
	{
		flow->prompts[flow->prompt_count] = ae_flow_prompt(AE_BUTTON_DPAD_LEFT, "Team", AE_ACTION_LEFT);
		flow->prompts[flow->prompt_count++].pair = 1;
	}
	ae_flow_draw_prompts(flow);
}

/* the focus moved in its column (or to the other column's nearest row) */
static void all_move(short column, short direction)
{
	static short cards[AE_LOBBY_MAXIMUM_PLAYERS];
	short count = all_list(column, cards), at = -1, index;

	for (index = 0; index < count; index++)
		if (cards[index] == all.focus)
			at = index;
	if (count <= 0)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (column != all.column || at < 0)
	{
		/* (the other column: the row at the same place, or its last) */
		at = all.first[column] + (at >= 0 ? at - all.first[all.column] : 0);
		at = at < 0 ? 0 : at >= count ? (short)(count - 1) : at;
	}
	else
	{
		at = (short)(at + direction);
		if (at < 0 || at >= count)
			return;
	}
	all.column = column;
	all.focus = cards[at];
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static int all_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	if (split_screen_press(event, 1))
		return 1;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		/* (not handled: the stack takes it off, with the back sound) */
		return 0;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		all_move(all.column, event->action == AE_ACTION_UP ? -1 : 1);
		return 1;
	case AE_ACTION_TAB_NEXT:
	case AE_ACTION_TAB_PREVIOUS:
		if (all_columns() < 2 || all.column == (event->action == AE_ACTION_TAB_NEXT ? 1 : 0))
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return 1;
		}
		all_move(event->action == AE_ACTION_TAB_NEXT ? 1 : 0, 0);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
	case AE_ACTION_ACCEPT:
		if (all.focus >= 0 && card_arrows(all.focus))
			party_team_step(all.focus);
		return 1;
	case AE_ACTION_Y:
		{
			short controller = ae_input_event_controller();

			if (controller < 0)
				controller = event->player;
			ae_sound_request(ae_lobby_change_team(controller) ? AE_SOUND_FORWARD : AE_SOUND_FAILURE, 0);
		}
		return 1;
	case AE_ACTION_START:
		/* (back to the lobby, the focus on START GAME) */
		ae_ui_pop();
		start_press();
		return 1;
	default:
		return 1;
	}
}

static void all_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;

	if (ae_prompts_pointer(all.flow.prompts, all.flow.prompt_count, pointer, HIT_ALL_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_remove(&all);
		}
		else
			all_handle(instance, &event);
		return;
	}
	if (!ae_hit_at(pointer->x, pointer->y, 0, &hit) || hit.id != HIT_ALL_ROWS || !lobby.have_roster || hit.index < 0 ||
		hit.index >= lobby.roster.count)
		return;
	if (pointer->moved || pointer->left_clicks)
	{
		all.focus = hit.index;
		all.column = all_column_of(hit.index);
	}
	if (pointer->left_clicks && (hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT))
		party_team_step(hit.index);
}

static void all_update(struct ae_screen *instance)
{
	/* (the lobby's own: the roster, the session, the game starting) */
	lobby_update(instance);
}

static struct ae_screen_class const all_class =
{
	.name = "lobby all", .handle = all_handle, .draw = all_draw, .pointer = all_pointer, .update = all_update,
	.start_from_anyone = 1,
};

static void all_open(void)
{
	if (ae_ui_holds(&all))
		return;
	memset(&all.flow, 0, sizeof(all.flow));
	all.flow.hit_rows = HIT_ALL_ROWS;
	all.flow.hit_prompts = HIT_ALL_PROMPTS;
	all.first[0] = all.first[1] = 0;
	/* (on the party's focused player, else this PC's first) */
	all.focus = lobby.party_view && lobby.party_focus >= 0 ? lobby.party[lobby.party_focus].card : -1;
	if (all.focus < 0 && lobby.have_roster)
	{
		short index;

		for (index = (short)(lobby.roster.count - 1); index >= 0; index--)
			if (lobby.roster.players[index].local && !lobby.roster.players[index].bot)
				all.focus = index;
	}
	all.column = all.focus >= 0 ? all_column_of(all.focus) : 0;
	if (!ae_ui_push(&all_class, AE_OWNER_ANY, &all))
		return;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("ae create game: lobby: see all");
}

/* ---------- the entries */

int ae_screen_create_game_lobby_joined(const char *how)
{
	lobby_open(1);
	if (!ae_ui_holds(&lobby))
		return 0;
	snprintf(lobby.joined_how, sizeof(lobby.joined_how), "%s", how ? how : "");
	lobby.from_browser = 1;
	return 1;
}

static void create_reset(void)
{
	char const *const *names = NULL;
	short last = 0, count;

	memset(&create, 0, sizeof(create));
	create.network = initial_network();
	/* (owner: the lobby opens private, invite only; PUBLIC lists it) */
	create.privacy = PRIVACY_INVITE_ONLY;
	create.players_index = MAXIMUM_PLAYERS_DEFAULT;
	count = ui_widget_port_multiplayer_maps(&names, &last);
	/* (the maps' facts read on the worker now, before CHANGE MAP wants them) */
	ae_setup_maps_prefetch();
	/* (the map used last, else the list's first: Battle Creek, listed with or without its file; one with a file) */
	create.map_index = ae_stage_map_playable_index(names, count, last);
	/* (a test launch's map, $HALO_AE_TEST_MAP: a map's file name, as debug.network_test names it) */
	{
		char const *wanted = test_map[0] ? test_map : getenv("HALO_AE_TEST_MAP");
		short index;

		for (index = 0; wanted && *wanted && index < count; index++)
		{
			char const *tail = strrchr(names[index], '\\');

			if (!csstrcasecmp(tail ? tail + 1 : names[index], wanted) || !csstrcasecmp(names[index], wanted))
			{
				create.map_index = index;
				break;
			}
		}
	}
	map_name_of(create.map_index);
}

int ae_screen_create_game_from_menu(void)
{
	if (!ae_setup_edit_open() && !ae_setup_edit_begin_named(NULL))
		return 0;
	create_reset();
	create.from_menu = 1;
	return host_open();
}

int ae_screen_create_game_open(void)
{
	if (!ae_setup_edit_begin_named(getenv("HALO_AE_TEST_GAMETYPE")))
		return 0;
	create_reset();
	ae_ui_replace_menus();
	(void)ae_settings_ui_scale();
	return host_open();
}

int ae_screen_create_game_test_open(const char *map, struct game_variant const *variant,
	struct game_variant_options const *options, short local_players, float start_seconds)
{
	char gametype[16];

	snprintf(test_map, sizeof(test_map), "%s", map ? map : "");
	if (variant && options ? !ae_setup_edit_begin_rules(variant, options) :
		!ae_setup_edit_begin_named(getenv("HALO_AE_TEST_GAMETYPE")))
		return 0;
	ae_setup_edit_name(gametype, sizeof(gametype));
	create_reset();
	ae_ui_replace_menus();
	(void)ae_settings_ui_scale();
	if (!host_open())
		return 0;
	create.test_local_players = local_players;
	create.test_start_at = system_milliseconds() + (unsigned long)(start_seconds > 0.0f ? start_seconds * 1000.0f : 0.0f) + 1;
	platform_log("ae create game: test launch (map %s, gametype '%s', %d players here, START GAME in %.0f s)", test_map,
		gametype, local_players > 1 ? (int)local_players : 1, (double)start_seconds);
	return 1;
}

/* ---------- (0.4.0) the pause menu's host rows */

int ae_screen_create_game_hosting(void)
{
	return live();
}

int ae_screen_create_game_pick_next_map(void (*picked)(short list_index, char const *name))
{
	if (!live())
		return 0;
	return ae_screen_change_map_pick(map_filter(), create.map_index, picked);
}

void ae_screen_create_game_next_map(short list_index, char const *name)
{
	create.next_map = 1;
	create.next_map_index = list_index;
	snprintf(create.next_map_name, sizeof(create.next_map_name), "%s", name ? name : "");
	platform_log("ae create game: the next game's map '%s' (picked in game)", create.next_map_name);
}

void ae_screen_create_game_next_options(void)
{
	create.next_options = 1;
}
