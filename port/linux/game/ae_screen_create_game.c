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
shield type, primary weapon with a custom loadout, grenades on map, sensor range, respawn time: stepped in place),
START GAME, the screen's primary action (menu blue). Every change reaches the server at once while the lobby is live
(PREGAME); before that, the copy and the map are applied as it opens. The quick options change the session's copy:
they last to the next game, the saved gametype is never written.

On the right, the lobby panel: LOBBY (network, privacy, players / most players; A or X opens LOBBY SETUP), INVITE
(HOST ONLINE: A copies the invite link), PLAYERS counting people (bots apart) and the roster's cards, and BOTS and BOT
SKILL at its foot (the settings' bots.count and bots.skill: OFF and 1-31 in the menus, config.toml up to 127; written
at once, so the lobby's bots follow). The focus goes down from START GAME into the panel and up out of it; RB (E)
jumps to the panel (its row last focused) and LB (Q) back to the left column's row ("LB RB Column", 0.3.9).

LOBBY SETUP, a popover over the panel: NETWORK (LAN / HOST ONLINE), PUBLIC (HOST ONLINE only: listed in everyone's
SERVER BROWSER; off = invite only) and MAX PLAYERS (bots included); with bots (0.3.9), under a BOTS heading, the
classic menus' SHOOTING SKILL, MOVEMENT SKILL (AS BOT SKILL or a level), PLAYFIGHT and BOT TEAMS, written to
config.toml at once as the panel's BOTS are. PUBLIC and MAX PLAYERS reach the session at once;
a NETWORK change opens the lobby again on the new network when the popover closes (asked first when other players are
in it: they leave with the old session, told by their own game).

START GAME starts and takes every AE screen off the stack while the game plays; the take-over
(ae_lobby_take_pregame_ui) brings this screen back after it. B leaves (asked first when other people are in the
lobby): the session closes, AE's MULTIPLAYER (or the main menu after a game) comes back.

A joiner (SERVER BROWSER, LAN GAMES, JOIN WITH INVITE) sees the same screen read only, titled LOBBY: the host's map
and gametype, WAITING FOR THE HOST TO START, the panel's LOBBY line and roster; B leaves.

Its log: "ae create game: open", "hosting (<network>, '<map>', '<gametype>')", "hosted, player 1 in", "open failed:
<why>", "lobby: open", "lobby: quick <label> <value>", "lobby: bots <count>", "lobby: bot skill <level>",
"lobby: players <people>, bots <bots>" (as they change), "lobby: setup open", "lobby: public on|off", "lobby: max
players <n>", "lobby: shooting skill|movement skill|playfight|teams <value>", "lobby: network <network>, reopening",
"lobby: column panel|left", "lobby: start", "lobby: leave", "closed".
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
#include "ae_glue_lobby.h"
#include "ae_header_lines.h"
#include "ae_input.h"
#include "ae_glue_setup_maps.h"
#include "ae_glue_setup_options.h"
#include "ae_hooks.h"
#include "ae_screen_change_gametype.h"
#include "ae_screen_change_map.h"
#include "ae_screen_create_game.h"
#include "ae_screen_game_options.h"
#include "ae_setup_flow.h"
#include "ae_setup_maps.h"
#include "ae_setup_options.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

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
	HIT_ROWS = FLOW_HIT + 1, HIT_PROMPTS, HIT_CARDS, HIT_PANEL_ROWS, HIT_SETUP, HIT_SETUP_ROWS, HIT_SETUP_PROMPTS,
	GAMETYPES_MAXIMUM = AE_SAVED_GAMES_MAXIMUM, /* (every gametype the memory unit lists) */
	JOIN_WAIT_MS = 10000
};
/* (no split screen choice: every game takes this PC's split screen players, and one bot path, the network bots,
serves them all: owner 2026-10-22) */
enum { NETWORK_LAN, NETWORK_ONLINE, NETWORKS };
enum { ROW_HEADER = AE_FLOW_ROW_HEADER, ROW_OPTION = AE_FLOW_ROW_OPTION, ROW_MAP = AE_FLOW_ROW_OWN, ROW_GAMETYPE,
	ROW_OPTIONS, ROW_START, ROW_INFO };
/* the session: none yet, this PC's client joining it, live, its hosting failed, to be hosted again next frame (a
NETWORK change: the old one closed this frame) */
enum { START_IDLE, START_JOINING, START_DONE, START_FAILED, START_REOPEN };
enum { PRIVACY_INVITE_ONLY, PRIVACY_PUBLIC, PRIVACIES };
/* the panel's rows: LOBBY and INVITE at its top, BOTS and BOT SKILL at its foot */
/* (0.3.9) ADD PLAYER under them: split screen, a controller's START joins it (a lone pad needs ADD PLAYER first) */
enum { PANEL_LOBBY, PANEL_INVITE, PANEL_ADD, PANEL_BOTS, PANEL_BOT_SKILL, PANEL_ROWS };
/* LOBBY SETUP's rows; (0.3.9) under a BOTS heading while there are bots, the classic menus' four more bot settings */
enum { SETUP_NETWORK, SETUP_PUBLIC, SETUP_MAXIMUM, SETUP_SHOOTING, SETUP_MOVEMENT, SETUP_PLAYFIGHT, SETUP_TEAMS,
	SETUP_ROWS };

static const char *const network_names[NETWORKS] = { "LAN", "HOST ONLINE" };
static const char *const network_words[NETWORKS] = { "LAN", "ONLINE" };
static const int network_kinds[NETWORKS] = { AE_LOBBY_LAN, AE_LOBBY_ONLINE };
static const char *const privacy_words[PRIVACIES] = { "INVITE ONLY", "PUBLIC" };
#define NETWORK_HELP "HOST ONLINE: friends join with your invite. LAN: anyone on this network sees the game. " \
	"Changing it opens the lobby again."
static const char *const privacy_helps[PRIVACIES] = {
	"Off: only players you send your invite to can join (INVITE, in the lobby).",
	"On: anyone can find and join it in the SERVER BROWSER, and your invite works too."
};
#define MAXIMUM_HELP "The most players the game takes, bots included."
#define LOBBY_HELP "Who can join this lobby, and how many: the network, PUBLIC and MAX PLAYERS ({X} from anywhere)."
#define INVITE_HELP "{A} copies your game's invite link (halo://join/...) to paste to friends. Anyone with it can join."
/* (the new controller's own START and B: pad words whatever device drove the menus last) */
#define ADD_HELP "Split screen: START on another controller adds its player (its B takes them out). With one " \
	"controller and the keyboard, {A} here first."
/* BOTS: OFF and 1-31 (bots.count; config.toml takes up to 127, shown as it is), BOT SKILL: bots.skill's levels (wording
and help: tools/port_settings.py's Bots screen) */
enum { BOTS_MENU_MAXIMUM = 31, BOTS_SETTING_MAXIMUM = 127, BOT_SKILLS = 6, BOT_SKILL_DEFAULT = 2 };
static const char *const bot_skill_values[BOT_SKILLS] = { "recruit", "easy", "normal", "heroic", "legendary", "demon" };
static const char *const bot_skill_names[BOT_SKILLS] = { "RECRUIT", "EASY", "NORMAL", "HEROIC", "LEGENDARY", "DEMON" };
#define BOTS_HELP "How many bots, up to 31, play your games; a person who joins a full game takes a bot's " \
	"place. With bots, {X} (LOBBY SETUP) has more."
#define BOT_SKILL_HELP "How soon the bots react, how well they aim and how far they see."
/* (0.3.9) LOBBY SETUP's bot rows: the settings bots.shooting_skill, bots.movement_skill ("same": BOT SKILL's),
bots.playfight, bots.teams; their wording tools/port_settings.py's classic BOTS screen's */
enum { BOT_PARTS = BOT_SKILLS + 1 };
static const char *const bot_part_values[BOT_PARTS] = { "same", "recruit", "easy", "normal", "heroic", "legendary",
	"demon" };
static const char *const bot_part_names[BOT_PARTS] = { "AS BOT SKILL", "RECRUIT", "EASY", "NORMAL", "HEROIC",
	"LEGENDARY", "DEMON" };
static const char *const bot_teams_values[2] = { "even", "against" };
static const char *const bot_teams_names[2] = { "EVEN", "AGAINST YOU" };
#define SHOOTING_HELP "The bots' aim, reactions, grenades and blows: as BOT SKILL, or a level of their own."
#define MOVEMENT_HELP "The bots' strafing, jumping, crouching and retreating: as BOT SKILL, or a level of their own."
static const char *const playfight_helps[2] = {
	"Off: the bots fight back.",
	"On: the bots move, strafe and aim at you, but never shoot, throw or strike: aim practice. At once, in game too."
};
static const char *const bot_teams_helps[2] = {
	"In team games, bots on both teams, evening them with the players.",
	"In team games, every bot on the team the players are not on."
};
static const short maximum_players[] = { 2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128 };
enum { MAXIMUM_PLAYERS_COUNT = sizeof(maximum_players) / sizeof(maximum_players[0]), MAXIMUM_PLAYERS_DEFAULT = 10 };
/* the panel's and the popover's spacing (u: the mockups' 1080p numbers) */
#define PANEL_HEADING_ABOVE_U 34.0f   /* the last top row's bottom to the PLAYERS line's middle */
#define PANEL_CARDS_ABOVE_U 56.0f     /* the last top row's bottom to the first card */
#define LOBBY_DOT_U 5.0f
#define LOBBY_DOT_GAP_U 12.0f
#define SETUP_PAD_U 20.0f
#define SETUP_STRIPE_U 4.0f
#define SETUP_TITLE_U 32.0f
enum { SETUP_HELP_LINES = 3 };   /* (130 %: the NETWORK help takes three) */

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
} create;

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
	/* the panel's rows as drawn (PANEL_*), the focused one's index in them, -1 none (the left column's) */
	short panel_kinds[PANEL_ROWS];
	short panel_count, panel_focus;
	short panel_last;          /* (0.3.9) the panel row RB comes back to: the one last focused there */
	struct ae_rect panel_rects[PANEL_ROWS];
	short logged_people, logged_bots;   /* the roster's counts last logged (-1: none yet) */
} lobby;

/* LOBBY SETUP, the popover */
static struct
{
	int network;               /* NETWORK as picked: the lobby opens again on it when the popover closes */
	short focus;
	short kinds[SETUP_ROWS];
	short count;
} setup;

static struct ae_screen_class const lobby_class, setup_class;

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
	ui_map_list_caption(names[index], wide, 64);
	for (at = 0; at < (int)sizeof(create.map_name) - 1 && wide[at]; at++)
		create.map_name[at] = wide[at] >= 0x20 && wide[at] < 0x7F ? (char)wide[at] : '?';
	create.map_name[at] = 0;
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
		"Every rule of the gametype, in Halo 2's groups. Changes are for these games; SAVE AS NEW keeps them." };
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

/* ---------- BOTS and BOT SKILL (the settings) */

static long bots_count(void)
{
	long count = config_integer("bots.count");

	return count < 0 ? 0 : count > BOTS_SETTING_MAXIMUM ? BOTS_SETTING_MAXIMUM : count;
}

static const char *bots_value(void)
{
	static char text[8];
	long count = bots_count();

	if (!count)
		return "OFF";
	snprintf(text, sizeof(text), "%ld", count);
	return text;
}

static short bot_skill(void)
{
	char const *now = config_string("bots.skill");
	short index;

	for (index = 0; now && index < BOT_SKILLS; index++)
	{
		if (!strcmp(now, bot_skill_values[index]))
			return index;
	}
	return BOT_SKILL_DEFAULT;
}

/* BOTS (row 0) or BOT SKILL (1) stepped and written (the lobby's bots follow the setting at once) */
static void bots_step(int row, short direction)
{
	char text[8];
	long count = bots_count();
	short current, next;

	if (row == 0)
	{
		/* (more than the menus' 31, from config.toml: left comes back to 31, right goes no further) */
		current = (short)(count > BOTS_MENU_MAXIMUM ? BOTS_MENU_MAXIMUM + 1 : count);
		next = ae_value_step(BOTS_MENU_MAXIMUM + 1, current > BOTS_MENU_MAXIMUM ? BOTS_MENU_MAXIMUM : current,
			direction);
		if (current > BOTS_MENU_MAXIMUM && direction > 0)
			next = current;
		snprintf(text, sizeof(text), "%d", next);
		if (next == current || !config_write("bots.count", text))
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
		platform_log("ae create game: lobby: bots %s", bots_value());
	}
	else
	{
		current = bot_skill();
		next = ae_value_step(BOT_SKILLS, current, direction);
		if (next == current || !config_write("bots.skill", bot_skill_values[next]))
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
		platform_log("ae create game: lobby: bot skill %s", bot_skill_names[next]);
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

/* ---------- the session */

static int initial_network(void)
{
	if (last_network >= 0 && last_network < NETWORKS)
		return last_network;
	return config_boolean("network.online") ? NETWORK_ONLINE : NETWORK_LAN;
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
	result = ae_lobby_add_local_player(0);
	if (!result.ok)
	{
		ae_lobby_leave();
		open_failed(result.reason);
		return;
	}
	if (create.map_index < 0 || !ui_widget_port_multiplayer_map_choose(create.map_index))
	{
		ae_lobby_leave();
		open_failed("That map can't be played with others in this build.");
		return;
	}
	ae_setup_edit_apply();
	server_settings(&roster);
	create.start_step = START_DONE;
	platform_log("ae create game: hosted, player 1 in");
	platform_log("ae create game: lobby: open");
}

/* START GAME: starts a live lobby's game; a lobby that did not open is opened again */
static void lobby_start(void)
{
	struct ae_result result;

	if (lobby.joiner)
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
	result = ae_lobby_start();
	if (!result.ok)
	{
		ae_flow_error(&lobby.flow, "The game did not start", result.reason);
		return;
	}
	platform_log("ae create game: lobby: start");
	ae_sound_request(AE_SOUND_FORWARD, 0);
	/* (the screen stays up through the countdown; it leaves once the game runs, as a joiner's does: lobby_update.
	Clearing it here showed AE's main menu over the live lobby until then: release review S1) */
	lobby.host_started = 1;
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
	static const short quick[] = { AE_SO_TIME_LIMIT, -1, AE_SO_SHIELDS, AE_SO_PRIMARY, AE_SO_GRENADES_ON_MAP,
		AE_SO_SENSOR_RANGE, AE_SO_RESPAWN_TIME };
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
			ae_scoreboard_header_map_name(lobby.roster.map, map, NUMBEROF(map));
		ae_lobby_utf16_to_utf8(flow->value_texts[0], sizeof(flow->value_texts[0]), (const unsigned short *)map,
			NUMBEROF(map));
		flow->rows[count++] = (struct ae_flow_row){ ROW_INFO, -1, "MAP", flow->value_texts[0], NULL };
		flow->rows[count++] = (struct ae_flow_row){ ROW_INFO, -1, "GAMETYPE", lobby.have_roster &&
			lobby.roster.gametype[0] ? lobby.roster.gametype : "THE HOST'S", NULL };
		flow->rows[count++] = (struct ae_flow_row){ ROW_INFO, -1, "WAITING FOR THE HOST TO START", NULL,
			"The host picks the map and rules and starts the game. {B} leaves it." };
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
	/* START GAME, the primary action; while the lobby opens (or did not) its state in the value's place */
	memset(&start, 0, sizeof(start));
	start.kind = ROW_START;
	start.option = -1;
	start.label = "START GAME";
	start.primary = 1;
	if (live())
		start.help = "Starts the game for everyone in the lobby. After it, everyone comes back here.";
	else if (create.start_step == START_FAILED)
	{
		start.reason = "NOT OPEN";
		start.help = "The lobby did not open: {A} opens it again ({X}: LOBBY SETUP, another network).";
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

static void left_accept(short row)
{
	struct ae_flow_row const *r = &lobby.flow.rows[row];

	switch (r->kind)
	{
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
		lobby_start();
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
	short last = 0, count = ui_widget_port_multiplayer_maps(&names, &last), next;

	if (count <= 0)
		return;
	next = (short)(((create.map_index < 0 ? 0 : create.map_index) + direction + count) % count);
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

	if (r->kind == ROW_MAP)
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
static int split_screen_press(struct ae_event const *event)
{
	short controller = ae_input_event_controller();
	struct ae_result result;

	if (controller < 1)
		return 0;
	if (event->action == AE_ACTION_START && !ae_lobby_controller_plays(controller))
	{
		result = ae_lobby_join_controller(controller);
		if (!result.ok)
		{
			ae_flow_error(&lobby.flow, "The player did not join", result.reason);
			return 1;
		}
		lobby.adding = 0;
		ae_sound_request(AE_SOUND_FORWARD, 0);
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

static void panel_rows(void)
{
	lobby.panel_count = 0;
	lobby.panel_kinds[lobby.panel_count++] = PANEL_LOBBY;
	if (lobby.joiner)
		return;
	if (create.network == NETWORK_ONLINE)
		lobby.panel_kinds[lobby.panel_count++] = PANEL_INVITE;
	if (ae_lobby_local_players() < 4)
		lobby.panel_kinds[lobby.panel_count++] = PANEL_ADD;
	lobby.panel_kinds[lobby.panel_count++] = PANEL_BOTS;
	lobby.panel_kinds[lobby.panel_count++] = PANEL_BOT_SKILL;
	if (lobby.panel_focus >= lobby.panel_count)
		lobby.panel_focus = (short)(lobby.panel_count - 1);
}

static const char *panel_help(short kind)
{
	switch (kind)
	{
	case PANEL_LOBBY: return LOBBY_HELP;
	case PANEL_INVITE: return INVITE_HELP;
	case PANEL_ADD: return ADD_HELP;
	case PANEL_BOTS: return BOTS_HELP;
	case PANEL_BOT_SKILL: return BOT_SKILL_HELP;
	}
	return NULL;
}

static void panel_accept(short index)
{
	if (index < 0 || index >= lobby.panel_count)
		return;
	switch (lobby.panel_kinds[index])
	{
	case PANEL_LOBBY:
		setup_open();
		break;
	case PANEL_INVITE:
		invite_copy();
		break;
	case PANEL_ADD:
		adding_set(!lobby.adding);
		ae_sound_request(lobby.adding ? AE_SOUND_FORWARD : AE_SOUND_BACK, 0);
		break;
	default:
		break;
	}
}

static void panel_step(short index, short direction)
{
	if (index < 0 || index >= lobby.panel_count)
		return;
	if (lobby.panel_kinds[index] == PANEL_BOTS || lobby.panel_kinds[index] == PANEL_BOT_SKILL)
		bots_step(lobby.panel_kinds[index] == PANEL_BOT_SKILL, direction);
}

/* the LOBBY line's parts (network, privacy, players / most), at most 3; their count */
static int lobby_parts(char parts[3][24])
{
	int count = 0;
	short players = lobby.have_roster ? lobby.roster.count : 0;

	if (lobby.joiner)
	{
		/* (the browser's LAN GAMES says how it joined; any other join is the internet's) */
		snprintf(parts[count++], 24, "%s", strstr(lobby.joined_how, "NETWORK") ? "LAN" : "ONLINE");
		if (lobby.have_roster && lobby.roster.maximum_players > 0)
			snprintf(parts[count++], 24, "%d/%d", players, lobby.roster.maximum_players);
		return count;
	}
	if (create.start_step == START_FAILED)
	{
		snprintf(parts[count++], 24, "%s", network_words[create.network]);
		snprintf(parts[count++], 24, "NOT OPEN");
		return count;
	}
	if (!live())
	{
		snprintf(parts[count++], 24, "%s", network_words[create.network]);
		snprintf(parts[count++], 24, "OPENING...");
		return count;
	}
	snprintf(parts[count++], 24, "%s", network_words[create.network]);
	if (create.network == NETWORK_ONLINE)
		snprintf(parts[count++], 24, "%s", privacy_words[create.privacy]);
	snprintf(parts[count++], 24, "%d/%d", players, maximum_players[create.players_index]);
	return count;
}

/* a panel row (the LOBBY line's parts drawn with drawn dots between, as the title crumb: the font's middle dot sits
off centre); returns its height */
static float draw_panel_row(short index, float x, float y, float width)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_density const *d = &flow->density;
	short kind = lobby.panel_kinds[index];
	int focused = index == lobby.panel_focus;
	struct ae_row widget;
	struct ae_row_layout layout;
	float height;

	memset(&widget, 0, sizeof(widget));
	if (focused)
		widget.flags |= AE_ROW_FOCUSED;
	switch (kind)
	{
	case PANEL_LOBBY:
		widget.label = "LOBBY";
		widget.flags |= lobby.joiner ? AE_ROW_STATUS : AE_ROW_SUBSCREEN;
		break;
	case PANEL_INVITE:
		widget.label = "INVITE";
		widget.value = lobby.invite_shown[0] ? lobby.invite_shown : "BEING MADE...";
		widget.flags |= lobby.invite[0] ? AE_ROW_LINK : AE_ROW_STATUS;
		break;
	case PANEL_ADD:
		widget.label = "ADD PLAYER";
		widget.value = lobby.adding ? "PRESS START ON IT" : NULL;
		break;
	case PANEL_BOTS:
		widget.label = "BOTS";
		widget.value = bots_value();
		break;
	default:
		widget.label = "BOT SKILL";
		widget.value = bot_skill_names[bot_skill()];
		break;
	}
	height = ae_widget_row(d, x, y, width, &widget, HIT_PANEL_ROWS, index);
	lobby.panel_rects[index] = (struct ae_rect){ x, y, width, height };
	if (kind == PANEL_LOBBY)
	{
		char parts[3][24];
		int count = lobby_parts(parts), part;
		float size = ae_size_minor(d), center = y + height * 0.5f, dot = ae_flow_u(flow, LOBBY_DOT_U);
		float gap = ae_flow_u(flow, LOBBY_DOT_GAP_U), right;
		unsigned int color = focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TEXT;

		ae_row_layout(d, width, &widget, &layout);
		right = x + layout.value_x;
		for (part = count - 1; part >= 0; part--)
		{
			ae_draw_text(AE_FONT_BODY, size, right, ae_flow_cap_top(AE_FONT_BODY, size, center), AE_ALIGN_RIGHT, color,
				parts[part]);
			right -= ae_draw_text_width(AE_FONT_BODY, size, parts[part]);
			if (part > 0)
			{
				ae_widget_separator(d, right - gap - dot * 0.5f, center, LOBBY_DOT_U,
					focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED);
				right -= gap * 2.0f + dot;
			}
		}
	}
	return height;
}

/* the focused panel row's help where the rows' help goes (ae_flow_draw_rows' place and rule: under the rows on the
left, when there is room under them) */
static void draw_panel_help(float width)
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

	if (lobby.panel_focus < 0 || lobby.panel_focus >= lobby.panel_count || flow->count <= 0)
		return;
	help = panel_help(lobby.panel_kinds[lobby.panel_focus]);
	if (!help)
		return;
	/* (the last row drawn: the rows may scroll at 130 %) */
	for (last = (short)(flow->count - 1); last > 0 && flow->rects[last].width <= 0.0f; last--)
		;
	rows_end = flow->rects[last].y + flow->rects[last].height + ae_flow_u(flow, d->metrics->gap);
	if (help_y < rows_end + help_gap)
		return;
	ae_draw_rect(x, help_y - help_gap * 0.5f, width, ae_flow_u(flow, AE_FLOW_RULE_U), 0.0f, AE_COLOR_RULE);
	count = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(help, expanded, sizeof(expanded)), width, lines, 2);
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, x, help_y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
}

static void draw_panel(float x, float y, float width, float bottom)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_density const *d = &flow->density;
	float inset = ae_flow_u(flow, AE_FLOW_INSET_U), minor = ae_size_minor(d), gap = ae_flow_u(flow, d->metrics->gap);
	float ix = x + inset, iw = width - 2.0f * inset, row_y = y + inset, card_y, cards_bottom = bottom - inset;
	float more = ae_widget_more_line(d);
	char heading[48];
	short index, people = lobby.have_roster ? lobby.roster.people : 0;
	short bots = lobby.have_roster ? (short)(lobby.roster.count - lobby.roster.people) : 0;

	ae_draw_rect(x, y, width, bottom - y, ae_flow_u(flow, d->metrics->corner), AE_COLOR_PANEL);
	ae_draw_outline(x, y, width, bottom - y, ae_flow_u(flow, d->metrics->corner), ae_flow_u(flow, AE_FLOW_RULE_U),
		AE_COLOR_RULE);
	/* LOBBY and INVITE at the top */
	for (index = 0; index < lobby.panel_count; index++)
	{
		short kind = lobby.panel_kinds[index];

		if (kind != PANEL_LOBBY && kind != PANEL_INVITE && kind != PANEL_ADD)
			continue;
		row_y += draw_panel_row(index, ix, row_y, iw) + gap;
	}
	row_y -= gap;
	/* BOTS and BOT SKILL at the foot (the host's) */
	if (!lobby.joiner)
	{
		float foot = bottom - inset - 2.0f * ae_size_row(d) - gap;

		cards_bottom = foot;
		for (index = 0; index < lobby.panel_count; index++)
		{
			short kind = lobby.panel_kinds[index];

			if (kind != PANEL_BOTS && kind != PANEL_BOT_SKILL)
				continue;
			foot += draw_panel_row(index, ix, foot, iw) + gap;
		}
	}
	/* (people, not machines or bots: a split screen guest is one; the bots apart) */
	if (bots > 0)
		snprintf(heading, sizeof(heading), "PLAYERS  %d   BOTS  %d", people, bots);
	else
		snprintf(heading, sizeof(heading), "PLAYERS  %d", people);
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, ix, ae_flow_cap_top(AE_FONT_BODY, minor,
		row_y + ae_flow_u(flow, PANEL_HEADING_ABOVE_U)), AE_ALIGN_LEFT, AE_COLOR_MUTED, heading);
	card_y = row_y + ae_flow_u(flow, PANEL_CARDS_ABOVE_U);
	for (index = 0; lobby.have_roster && index < lobby.roster.count; index++)
	{
		static const unsigned int colors[4] = { 0xC0392BFFu, 0x2E86DEFFu, 0x7FA035FFu, 0xE6B422FFu };
		struct ae_lobby_player const *player = &lobby.roster.players[index];
		struct ae_roster_card card;
		float pitch = ae_flow_u(flow, d->metrics->card) + ae_flow_u(flow, AE_FLOW_CARD_GAP_U);

		/* (no room for this one, and a line saying how many are left out when more follow) */
		if (card_y + pitch > cards_bottom - (index < lobby.roster.count - 1 ? more : 0.0f))
		{
			ae_widget_more(d, ix + iw * 0.5f, card_y, 0, lobby.roster.count - index);
			break;
		}
		memset(&card, 0, sizeof(card));
		card.name = player->name;
		card.slot = player->slot;
		card.color = colors[(player->slot - 1) & 3];
		card.sub_line = player->bot ? "Bot" : player->local ? "This PC" : "Joined";
		if (lobby.roster.team_game)
		{
			card.flags |= AE_CARD_TEAM;
			card.team_name = player->team ? "BLUE" : "RED";
			card.team_color = player->team ? 0x2E86DEFFu : 0xC0392BFFu;
		}
		if (player->host)
			card.flags |= AE_CARD_HOST;
		card_y += ae_widget_roster_card(d, ix, card_y, iw, &card, HIT_CARDS, index) + ae_flow_u(flow, AE_FLOW_CARD_GAP_U);
	}
}

/* ---------- the screen */

static void lobby_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &lobby.flow;
	char crumb[96];
	float panel_x;

	(void)instance;
	ae_flow_full_density(flow);
	lobby_rows();
	panel_rows();
	if (lobby.joiner)
		snprintf(crumb, sizeof(crumb), "%s", lobby.joined_how[0] ? lobby.joined_how : "JOINED BY INVITE");
	else
		snprintf(crumb, sizeof(crumb), "%s", network_words[create.network]);
	ae_flow_title(flow, lobby.joiner ? "LOBBY" : "CREATE GAME", crumb);
	if (lobby.joiner)
		lobby.panel_focus = -1;
	/* (a panel row has the focus: no row on the left lit, nor its help) */
	{
		short focus = flow->focus;

		if (lobby.panel_focus >= 0)
			flow->focus = -1;
		ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
		flow->focus = focus;
	}
	draw_panel_help(flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
	panel_x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X;
	draw_panel(panel_x, ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y), ae_flow_right_x(flow) - panel_x,
		ae_flow_content_bottom(flow));
	flow->prompt_count = 0;
	if (!lobby.joiner)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Leave", AE_ACTION_BACK);
	if (!lobby.joiner)
	{
		/* (0.3.9: "LB RB Column", paired as CHANGE MAP's "LB RB Filter": the left column or the lobby panel) */
		flow->prompts[flow->prompt_count] = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Column", AE_ACTION_TAB_PREVIOUS);
		flow->prompts[flow->prompt_count++].pair = 1;
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Lobby setup", AE_ACTION_X);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_START, "Start game", AE_ACTION_START);
	}
	ae_flow_draw_prompts(flow);
}

static void lobby_update(struct ae_screen *instance)
{
	unsigned long now = system_milliseconds();

	(void)instance;
	session_update();
	/* (GAME OPTIONS closed over the lobby: its changes to the server) */
	if (lobby.options_open && ae_ui_top() && ae_ui_top()->data == &lobby)
	{
		lobby.options_open = 0;
		if (live())
			ae_setup_edit_apply();
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
	panel_rows();
	if (lobby.panel_focus >= 0)
		lobby.panel_last = lobby.panel_focus;
	if (split_screen_press(event))
		return 1;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		leave_or_ask();
		return 1;
	/* (0.3.9: RB (E) to the lobby panel, on its row last focused; LB (Q) back to the left column, on the row left
	there: no scrolling down past START GAME to reach the lobby) */
	case AE_ACTION_TAB_NEXT:
	case AE_ACTION_TAB_PREVIOUS:
		if (lobby.joiner)
			return 1;
		if (event->action == AE_ACTION_TAB_NEXT ? lobby.panel_focus >= 0 : lobby.panel_focus < 0)
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return 1;
		}
		lobby.panel_focus = event->action == AE_ACTION_TAB_PREVIOUS ? -1 :
			lobby.panel_last >= 0 && lobby.panel_last < lobby.panel_count ? lobby.panel_last : 0;
		ae_sound_request(AE_SOUND_CURSOR, 0);
		platform_log("ae create game: lobby: column %s", lobby.panel_focus >= 0 ? "panel" : "left");
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		if (lobby.joiner)
			return 1;
		direction = event->action == AE_ACTION_UP ? -1 : 1;
		/* (the panel: down from START GAME into it, up out of it from its first row) */
		if (lobby.panel_focus >= 0)
		{
			short next = (short)(lobby.panel_focus + direction);

			if (next >= lobby.panel_count)
				return 1;
			lobby.panel_focus = next < 0 ? -1 : next;
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
			return 1;
		}
		if (!ae_flow_focus_step(flow, direction, event->repeat) && direction > 0)
		{
			lobby.panel_focus = 0;
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		}
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (lobby.joiner)
			return 1;
		direction = event->action == AE_ACTION_RIGHT ? 1 : -1;
		if (lobby.panel_focus >= 0)
			panel_step(lobby.panel_focus, direction);
		else
			left_step(flow->focus, direction);
		return 1;
	case AE_ACTION_ACCEPT:
		if (lobby.joiner)
			return 1;
		if (lobby.panel_focus >= 0)
			panel_accept(lobby.panel_focus);
		else
			left_accept(flow->focus);
		return 1;
	case AE_ACTION_START:
		lobby_start();
		return 1;
	case AE_ACTION_X:
		if (!lobby.joiner)
			setup_open();
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

	if (ae_prompts_pointer(lobby.flow.prompts, lobby.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		lobby_handle(instance, &event);
		return;
	}
	if (lobby.joiner)
		return;
	/* (the panel's rows: hover focuses, a click takes the row, their arrows step) */
	if (ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_PANEL_ROWS && hit.index >= 0 &&
		hit.index < lobby.panel_count)
	{
		if (pointer->moved)
			lobby.panel_focus = hit.index;
		if (pointer->left_clicks)
		{
			if (hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT)
				panel_step(hit.index, hit.part == AE_PART_ARROW_RIGHT ? 1 : -1);
			else
				panel_accept(hit.index);
		}
		return;
	}
	if (pointer->moved && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_ROWS)
		lobby.panel_focus = -1;
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
	memset(&lobby, 0, sizeof(lobby));
	lobby.joiner = joiner;
	lobby.panel_focus = -1;
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

/* ---------- LOBBY SETUP */

/* (0.3.9) a bot setting's value index among its values (the default's when the setting has none of them) */
static short setting_index(const char *name, const char *const *values, short count, short fallback)
{
	char const *now = config_string(name);
	short index;

	for (index = 0; now && index < count; index++)
	{
		if (!strcmp(now, values[index]))
			return index;
	}
	return fallback;
}

/* the bot rows' value now: SHOOTING SKILL, MOVEMENT SKILL (AS BOT SKILL, RECRUIT..), PLAYFIGHT (OFF, ON), TEAMS */
static short bot_setting_now(short kind)
{
	switch (kind)
	{
	case SETUP_SHOOTING: return setting_index("bots.shooting_skill", bot_part_values, BOT_PARTS, 0);
	case SETUP_MOVEMENT: return setting_index("bots.movement_skill", bot_part_values, BOT_PARTS, 0);
	case SETUP_PLAYFIGHT: return (short)(config_boolean("bots.playfight") != 0);
	default: return setting_index("bots.teams", bot_teams_values, 2, 0);
	}
}

static const char *bot_setting_text(short kind)
{
	short now = bot_setting_now(kind);

	switch (kind)
	{
	case SETUP_SHOOTING:
	case SETUP_MOVEMENT:
		return bot_part_names[now];
	case SETUP_PLAYFIGHT:
		return now ? "ON" : "OFF";
	default:
		return bot_teams_names[now];
	}
}

/* a bot row stepped and written to config.toml at once (the bots read it as it changes: PLAYFIGHT in game too) */
static void bot_setting_step(short kind, short direction)
{
	static const char *const names[] = { "bots.shooting_skill", "bots.movement_skill", "bots.playfight", "bots.teams" };
	static const char *const logged[] = { "shooting skill", "movement skill", "playfight", "teams" };
	static const char *const on_off[2] = { "false", "true" };
	short now = bot_setting_now(kind), count, next;
	short which = (short)(kind - SETUP_SHOOTING);
	const char *value;

	count = kind == SETUP_SHOOTING || kind == SETUP_MOVEMENT ? BOT_PARTS : 2;
	next = ae_value_step(count, now, direction);
	value = kind == SETUP_SHOOTING || kind == SETUP_MOVEMENT ? bot_part_values[next] :
		kind == SETUP_PLAYFIGHT ? on_off[next] : bot_teams_values[next];
	if (next == now || !config_write(names[which], value))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
	platform_log("ae create game: lobby: %s %s", logged[which], bot_setting_text(kind));
}

static void setup_rows_now(void)
{
	setup.count = 0;
	setup.kinds[setup.count++] = SETUP_NETWORK;
	/* (LAN: always seen on the local network, no PUBLIC) */
	if (setup.network == NETWORK_ONLINE)
		setup.kinds[setup.count++] = SETUP_PUBLIC;
	setup.kinds[setup.count++] = SETUP_MAXIMUM;
	/* (the bot rows only while there are bots: BOTS OFF leaves the popover as it was) */
	if (bots_count() > 0)
	{
		setup.kinds[setup.count++] = SETUP_SHOOTING;
		setup.kinds[setup.count++] = SETUP_MOVEMENT;
		setup.kinds[setup.count++] = SETUP_PLAYFIGHT;
		setup.kinds[setup.count++] = SETUP_TEAMS;
	}
	if (setup.focus >= setup.count)
		setup.focus = (short)(setup.count - 1);
	if (setup.focus < 0)
		setup.focus = 0;
}

static void setup_step(short index, short direction)
{
	short next;

	if (index < 0 || index >= setup.count)
		return;
	switch (setup.kinds[index])
	{
	case SETUP_NETWORK:
		next = ae_value_step(NETWORKS, (short)setup.network, direction);
		ae_sound_request(next == setup.network ? AE_SOUND_FAILURE : AE_SOUND_CURSOR, 0);
		setup.network = next;
		setup_rows_now();
		break;
	case SETUP_PUBLIC:
		next = ae_value_step(PRIVACIES, (short)create.privacy, direction);
		ae_sound_request(next == create.privacy ? AE_SOUND_FAILURE : AE_SOUND_CURSOR, 0);
		if (next == create.privacy)
			break;
		create.privacy = next;
		platform_log("ae create game: lobby: public %s", create.privacy == PRIVACY_PUBLIC ? "on" : "off");
		/* (the session now, when it is the internet's: a NETWORK change opens it again with this) */
		if (ae_lobby_owns_session() && create.network == NETWORK_ONLINE)
			p2p_set_hosting_public(create.privacy == PRIVACY_PUBLIC);
		break;
	case SETUP_SHOOTING:
	case SETUP_MOVEMENT:
	case SETUP_PLAYFIGHT:
	case SETUP_TEAMS:
		bot_setting_step(setup.kinds[index], direction);
		break;
	default:
		next = ae_value_step(MAXIMUM_PLAYERS_COUNT, create.players_index, direction);
		ae_sound_request(next == create.players_index ? AE_SOUND_FAILURE : AE_SOUND_CURSOR, 0);
		if (next == create.players_index)
			break;
		create.players_index = next;
		platform_log("ae create game: lobby: max players %d", maximum_players[next]);
		if (live())
			server_settings(lobby.have_roster ? &lobby.roster : NULL);
		break;
	}
}

static void network_change_picked(short choice, void *context)
{
	(void)context;
	if (choice != 1)
		return;
	create.network = setup.network;
	session_reopen();
}

/* B: the popover closes; a NETWORK change opens the lobby again (asked first when others are in it) */
static void setup_done(void)
{
	int others = others_in_lobby();

	ae_sound_request(AE_SOUND_BACK, 0);
	if (ae_ui_top() && ae_ui_top()->data == &setup)
		ae_ui_pop();
	else
		ae_ui_remove(&setup);
	if (setup.network == create.network)
		return;
	if (others <= 0 || !live())
	{
		create.network = setup.network;
		session_reopen();
		return;
	}
	{
		struct ae_dialog_spec spec;
		char body[200];

		snprintf(body, sizeof(body), "The lobby opens again on %s. The %d player%s in it now leave%s it.",
			setup.network == NETWORK_LAN ? "your local network (LAN)" : "the internet (HOST ONLINE)", others,
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

static const char *setup_help(short kind);

/* the focused row's help as wrapped in the popover; its line count */
static int setup_help_lines(float width, char lines[SETUP_HELP_LINES][160])
{
	struct ae_flow_screen *flow = &lobby.flow;

	char expanded[320];

	return ae_wrap_text(AE_FONT_BODY, ae_flow_u(flow, flow->density.metrics->body),
		ae_help_expand(setup_help(setup.kinds[setup.focus]), expanded, sizeof(expanded)), width, lines, SETUP_HELP_LINES);
}

static const char *setup_help(short kind)
{
	switch (kind)
	{
	case SETUP_NETWORK: return NETWORK_HELP;
	case SETUP_PUBLIC: return privacy_helps[create.privacy];
	case SETUP_SHOOTING: return SHOOTING_HELP;
	case SETUP_MOVEMENT: return MOVEMENT_HELP;
	case SETUP_PLAYFIGHT: return playfight_helps[bot_setting_now(SETUP_PLAYFIGHT)];
	case SETUP_TEAMS: return bot_teams_helps[bot_setting_now(SETUP_TEAMS)];
	}
	return MAXIMUM_HELP;
}

/* the popover's box over the panel (layout units), its height from what it holds */
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
	/* (the BOTS heading over the bot rows) */
	if (setup.count > 0 && setup.kinds[setup.count - 1] == SETUP_TEAMS)
		box.height += ae_flow_u(flow, d->metrics->group_height);
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
		AE_COLOR_TITLE, "LOBBY SETUP");
	y += title * 1.4f + gap;
	for (index = 0; index < setup.count; index++)
	{
		struct ae_row widget;
		char players[8];

		memset(&widget, 0, sizeof(widget));
		if (index == setup.focus)
			widget.flags |= AE_ROW_FOCUSED;
		/* (the bot rows under their heading, as the settings' sections) */
		if (setup.kinds[index] == SETUP_SHOOTING)
			y += ae_widget_group(d, x, y, width, "BOTS");
		switch (setup.kinds[index])
		{
		case SETUP_SHOOTING:
			widget.label = "SHOOTING SKILL";
			widget.value = bot_setting_text(SETUP_SHOOTING);
			break;
		case SETUP_MOVEMENT:
			widget.label = "MOVEMENT SKILL";
			widget.value = bot_setting_text(SETUP_MOVEMENT);
			break;
		case SETUP_PLAYFIGHT:
			widget.label = "PLAYFIGHT";
			widget.value = bot_setting_text(SETUP_PLAYFIGHT);
			break;
		case SETUP_TEAMS:
			widget.label = "BOT TEAMS";
			widget.value = bot_setting_text(SETUP_TEAMS);
			break;
		case SETUP_NETWORK:
			widget.label = "NETWORK";
			widget.value = network_names[setup.network];
			break;
		case SETUP_PUBLIC:
			widget.label = "PUBLIC";
			widget.value = create.privacy == PRIVACY_PUBLIC ? "ON" : "OFF";
			break;
		default:
			snprintf(players, sizeof(players), "%d", maximum_players[create.players_index]);
			widget.label = "MAX PLAYERS";
			widget.value = players;
			break;
		}
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
		/* (NETWORK and PUBLIC: A takes the next value; MAX PLAYERS steps with left / right; the bot rows: A takes the
		next value, round past the last) */
		if (setup.kinds[setup.focus] >= SETUP_SHOOTING)
			setup_step(setup.focus, 1);
		else if (setup.kinds[setup.focus] != SETUP_MAXIMUM)
		{
			short values = setup.kinds[setup.focus] == SETUP_NETWORK ? NETWORKS : PRIVACIES;
			short now = setup.kinds[setup.focus] == SETUP_NETWORK ? (short)setup.network : (short)create.privacy;

			setup_step(setup.focus, now + 1 >= values ? -1 : 1);
		}
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
	if (!ae_hit_at(pointer->x, pointer->y, 0, &hit) || hit.id != HIT_SETUP_ROWS || hit.index < 0 || hit.index >= setup.count)
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
	.name = "lobby setup", .handle = setup_handle, .draw = setup_draw, .pointer = setup_pointer, .popover = 1,
};

static void setup_open(void)
{
	if (lobby.joiner || ae_ui_holds(&setup))
		return;
	memset(&setup, 0, sizeof(setup));
	setup.network = create.network;
	setup_rows_now();
	if (!ae_ui_push(&setup_class, AE_OWNER_ANY, &setup))
		return;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	platform_log("ae create game: lobby: setup open");
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
	short last = 0;

	memset(&create, 0, sizeof(create));
	create.network = initial_network();
	/* (owner: the lobby opens private, invite only; PUBLIC lists it) */
	create.privacy = PRIVACY_INVITE_ONLY;
	create.players_index = MAXIMUM_PLAYERS_DEFAULT;
	ui_widget_port_multiplayer_maps(&names, &last);
	/* (the maps' facts read on the worker now, before CHANGE MAP wants them) */
	ae_setup_maps_prefetch();
	create.map_index = last;
	map_name_of(last);
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
