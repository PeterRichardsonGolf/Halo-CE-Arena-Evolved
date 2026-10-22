/*
AE_SCREEN_CREATE_GAME.C

The game setup spike's CREATE GAME and LOBBY screens (ae_screen_create_game.h; the mockup's boards 1 and 3,
https://claude.ai/artifact/DNum7ugkfszzX99GuZEpsD). Both are rows on the left, as the gallery's lobby page; positions
are fractions of the frame, sizes u.

CREATE GAME: MAP (CHANGE MAP for the gametype's filter; its pick comes back here), GAMETYPE (CHANGE GAMETYPE; its pick
comes back here: a new copy), GAME OPTIONS (N CHANGED: the GAME OPTIONS screen on the copy), TIME LIMIT and the score in place,
NETWORK (SPLIT SCREEN, LAN, HOST ONLINE), BOTS and BOT SKILL (the settings' bots.count and bots.skill, as the old
menus' Bots screen writes them: OFF and 1-31 in the menus, config.toml up to 127), START GAME. START GAME hosts (ae_lobby_host), waits for this PC's client
(10 s at most), adds player 1, chooses the map, applies the copy (ae_setup_edit_apply) and opens the LOBBY. B closes
it: the game's main menu comes back.

LOBBY (AE's pregame screen: ae_lobby_set_pregame_screen): MAP and GAMETYPE as in CREATE GAME, QUICK OPTIONS (time,
score, shield type, primary weapon with a custom loadout, grenades on map, sensor range, respawn time: stepped in place
and applied to the server at once, PREGAME), START GAME; the roster's cards on the right, PLAYERS counting people (bots
apart), and under them BOTS and BOT SKILL (the host's: down from START GAME; written at once, so the lobby's bots
follow, as the old menus' live BOT COUNT; the rows on the left have no room for two more with their help). START GAME starts and takes
every AE screen off the stack while the game plays; the take-over (ae_lobby_take_pregame_ui) brings the LOBBY back
after it. B asks, then leaves the session and goes back to CREATE GAME. The quick options change the session's copy:
they last to the next game, the saved gametype is never written.

Its log: "ae create game: open", "start (<network>, '<map>', '<gametype>')", "hosted, player 1 in", "start failed:
<why>", "lobby: open", "lobby: quick <label> <value>", "[lobby: ]bots <count>", "[lobby: ]bot skill <level>", "lobby: start",
"lobby: leave", "closed".
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
#include "../src/ae_platform.h"
#include "ae_glue_lobby.h"
#include "ae_header_lines.h"
#include "ae_glue_setup_maps.h"
#include "ae_glue_setup_options.h"
#include "ae_hooks.h"
#include "ae_screen_change_gametype.h"
#include "ae_screen_change_map.h"
#include "ae_screen_create_game.h"
#include "ae_screen_game_options.h"
#include "ae_screen_join_invite.h"
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
	HIT_ROWS = FLOW_HIT + 1, HIT_PROMPTS, HIT_CARDS, HIT_BOT_ROWS,
	GAMETYPES_MAXIMUM = AE_SAVED_GAMES_MAXIMUM, /* (every gametype the memory unit lists) */
	JOIN_WAIT_MS = 10000
};
enum { NETWORK_LOCAL, NETWORK_LAN, NETWORK_ONLINE, NETWORKS };
enum { ROW_HEADER = AE_FLOW_ROW_HEADER, ROW_OPTION = AE_FLOW_ROW_OPTION, ROW_MAP = AE_FLOW_ROW_OWN, ROW_GAMETYPE,
	ROW_OPTIONS, ROW_NETWORK, ROW_START, ROW_PRIVACY, ROW_MAXIMUM_PLAYERS, ROW_JOIN, ROW_INVITE, ROW_INFO, ROW_BOTS,
	ROW_BOT_SKILL };
enum { START_IDLE, START_JOINING, START_DONE };
enum { PRIVACY_INVITE_ONLY, PRIVACY_PUBLIC, PRIVACIES };


static const char *const network_names[NETWORKS] = { "SPLIT SCREEN", "LAN", "HOST ONLINE" };
static const char *const network_helps[NETWORKS] = {
	"Split screen on this PC. Nobody else can join.",
	"Players on your network join from their game list.",
	"Hosted for the internet too: invite players with your game's code."
};
static const int network_kinds[NETWORKS] = { AE_LOBBY_LOCAL, AE_LOBBY_LAN, AE_LOBBY_ONLINE };
static const char *const privacy_names[PRIVACIES] = { "INVITE ONLY", "PUBLIC" };
static const char *const privacy_helps[PRIVACIES] = {
	"Only players with your invite link join. Copy it in the lobby.",
	"Listed in the server browser for anyone to join, and joinable by invite."
};
/* BOTS: OFF and 1-31 (bots.count; config.toml takes up to 127, shown as it is), BOT SKILL: bots.skill's levels (wording
and help: tools/port_settings.py's Bots screen) */
enum { BOTS_MENU_MAXIMUM = 31, BOTS_SETTING_MAXIMUM = 127, BOT_SKILLS = 6, BOT_SKILL_DEFAULT = 2, BOT_ROWS = 2 };
static const char *const bot_skill_values[BOT_SKILLS] = { "recruit", "easy", "normal", "heroic", "legendary", "demon" };
static const char *const bot_skill_names[BOT_SKILLS] = { "RECRUIT", "EASY", "NORMAL", "HEROIC", "LEGENDARY", "DEMON" };
#define BOTS_HELP "How many bots, up to 31, play the games you host: split screen, LAN or internet. A person who " \
	"joins a full game takes a bot's place."
#define BOT_SKILL_HELP "How soon the bots react, how well they aim and how far they see."
static const short maximum_players[] = { 2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128 };
enum { MAXIMUM_PLAYERS_COUNT = sizeof(maximum_players) / sizeof(maximum_players[0]), MAXIMUM_PLAYERS_DEFAULT = 10 };

static struct
{
	struct ae_flow_screen flow;
	short map_index;
	char map_name[64];
	int network, privacy;
	short players_index;
	char players_text[8];
	/* the saved gametypes, as GAMETYPE steps through them in place */
	char gametype_names[GAMETYPES_MAXIMUM][16];
	long gametype_profiles[GAMETYPES_MAXIMUM];
	short gametype_count;
	/* START GAME's steps */
	int start_step;
	unsigned long start_since;
	int menus_closed;          /* the game's menus closed by this screen: brought back when it closes */
	int starting;              /* a start under way: leaving does not bring the game's menus back */
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
	int joiner;                /* joined another's game by invite: the roster, map and gametype only */
	char joined_how[32];       /* (a joiner's crumb) how: "JOINED BY INVITE", the browser's own words */
	int from_browser;          /* opened by SERVER BROWSER / LAN GAMES (ae_screen_browser.c): leaving goes back to it */
	char invite[160];          /* the hosted game's invite link (ONLINE), "" none yet */
	char invite_shown[48];
	short bot_focus;           /* BOTS (0) or BOT SKILL (1) under the roster has the focus, -1 none */
	struct ae_rect bot_rects[BOT_ROWS];
} lobby;

static struct ae_screen_class const create_class, lobby_class;

/* ---------- what the copy says */

static struct ae_setup_access access_now(void)
{
	struct ae_setup_access access;

	ae_setup_edit_access(&access);
	return access;
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
	case AE_SETUP_ENGINE_CTF: return AE_MF_CTF;
	}
	return AE_MF_ALL;
}

static const char *option_value(int option)
{
	struct ae_setup_access access = access_now();

	return ae_setup_value_text(option, access.get(access.context, option));
}

/* a row of the copy's option stepped in place */
static void option_step(int option, short direction, const char *who)
{
	struct ae_setup_access access = access_now();
	struct ae_setup_row const *row = ae_setup_row(option);
	int current = access.get(access.context, option);
	short next;

	if (!row || current < 0)
		return;
	next = ae_value_step(row->value_count, (short)current, direction);
	if (next == current)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_setup_edit_set(option, next);
	ae_sound_request(AE_SOUND_CURSOR, 0);
	if (who)
	{
		platform_log("ae create game: %s %s %s", who, ae_setup_label(&access, option), ae_setup_value_text(option, next));
		ae_setup_edit_apply();
	}
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
	/* (in the lobby: the server's map now) */
	if (ae_ui_holds(&lobby) && ae_lobby_owns_session())
	{
		if (ui_widget_port_multiplayer_map_choose(list_index))
			platform_log("ae create game: lobby: map '%s'", name);
	}
}

static void gametype_picked(short index, void *context)
{
	(void)context;
	if (index < 0 || index >= create.gametype_count)
		return;
	if (ae_setup_edit_begin(create.gametype_profiles[index]))
	{
		platform_log("ae create game: gametype '%s'", create.gametype_names[index]);
		if (ae_ui_holds(&lobby) && ae_lobby_owns_session())
			ae_setup_edit_apply();
	}
}

/* CHANGE GAMETYPE's pick: a new copy of its rules (in the lobby: the server's at once) */
static void gametype_profile_picked(long profile)
{
	char name[16];

	if (!ae_setup_edit_begin(profile))
		return;
	ae_setup_edit_name(name, sizeof(name));
	platform_log("ae create game: gametype '%s'", name);
	if (ae_ui_holds(&lobby) && ae_lobby_owns_session())
		ae_setup_edit_apply();
}

static void gametype_picker(void)
{
	if (ae_screen_change_gametype_pick(gametype_profile_picked))
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* the rows CREATE GAME and the LOBBY both have: MAP, GAMETYPE, GAME OPTIONS */
static short setup_rows(struct ae_flow_screen *flow, short count)
{
	char name[16];
	short changes;
	struct ae_setup_access access = access_now();

	ae_setup_edit_name(name, sizeof(name));
	changes = ae_setup_changes(&access);
	flow->rows[count] = (struct ae_flow_row){ ROW_MAP, -1, "MAP", create.map_name[0] ? create.map_name : "CHOOSE",
		"The maps in folders, filtered for the gametype (LT / RT change the filter)." };
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

/* ---------- BOTS and BOT SKILL (the settings: both screens) */

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

/* BOTS (row 0) or BOT SKILL (1) stepped and written (the lobby's bots follow the setting at once); who: the log's
"lobby: " or "" */
static void bots_step(int row, short direction, const char *who)
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
		platform_log("ae create game: %sbots %s", who, bots_value());
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
		platform_log("ae create game: %sbot skill %s", who, bot_skill_names[next]);
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static struct ae_flow_row bot_row(int row)
{
	return row == 0 ? (struct ae_flow_row){ ROW_BOTS, -1, "BOTS", bots_value(), BOTS_HELP } :
		(struct ae_flow_row){ ROW_BOT_SKILL, -1, "BOT SKILL", bot_skill_names[bot_skill()], BOT_SKILL_HELP };
}

/* ---------- CREATE GAME */

static void create_rows(void)
{
	struct ae_flow_screen *flow = &create.flow;
	struct ae_setup_access access = access_now();
	short count = setup_rows(flow, 0);

	flow->rows[count++] = option_row(AE_SO_TIME_LIMIT);
	flow->rows[count++] = option_row(score_option(access.engine));
	flow->rows[count++] = (struct ae_flow_row){ ROW_NETWORK, -1, "NETWORK", network_names[create.network],
		network_helps[create.network] };
	if (create.network == NETWORK_ONLINE)
		flow->rows[count++] = (struct ae_flow_row){ ROW_PRIVACY, -1, "PRIVACY", privacy_names[create.privacy],
			privacy_helps[create.privacy] };
	if (create.network != NETWORK_LOCAL)
	{
		snprintf(create.players_text, sizeof(create.players_text), "%d", maximum_players[create.players_index]);
		flow->rows[count++] = (struct ae_flow_row){ ROW_MAXIMUM_PLAYERS, -1, "MAX PLAYERS", create.players_text,
			"The most players the game takes, bots included." };
	}
	flow->rows[count++] = bot_row(0);
	flow->rows[count++] = bot_row(1);
	flow->rows[count++] = (struct ae_flow_row){ ROW_START, -1, "START GAME", NULL,
		"Hosts the game with these settings and opens its lobby." };
	flow->rows[count++] = (struct ae_flow_row){ ROW_JOIN, -1, "JOIN WITH INVITE", NULL,
		"Paste or type the invite link or code a friend sent you, and join their game." };
	flow->count = count;
	if (flow->focus >= count)
		flow->focus = (short)(count - 1);
}

static void start_failed(const char *why)
{
	create.start_step = START_IDLE;
	create.starting = 0;
	platform_log("ae create game: start failed: %s", why);
	ae_flow_error(&create.flow, "The game did not start", why);
}

static void lobby_open(int joiner);

static void start_game(void)
{
	struct ae_result result;
	char name[16];

	if (create.map_index < 0)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_setup_edit_name(name, sizeof(name));
	platform_log("ae create game: start (%s, '%s', '%s')", network_names[create.network], create.map_name, name);
	create.starting = 1;
	result = ae_lobby_host(network_kinds[create.network]);
	if (!result.ok)
	{
		start_failed(result.reason);
		return;
	}
	/* (ae_lobby_host hosts invite only: PUBLIC lists it, as Server Setup's LISTING; no password in the spike) */
	p2p_set_hosting_public(create.network == NETWORK_ONLINE && create.privacy == PRIVACY_PUBLIC);
	p2p_set_hosting_password(NULL);
	create.menus_closed = 1;
	create.start_step = START_JOINING;
	create.start_since = system_milliseconds();
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* START GAME's wait: this PC's client in the pregame, then player 1, the map, the copy, the lobby */
static void start_update(void)
{
	static struct ae_lobby_roster roster;
	struct ae_result result;

	if (create.start_step != START_JOINING)
		return;
	if (!ae_lobby_roster(&roster).ok || (!roster.joined && system_milliseconds() - create.start_since < JOIN_WAIT_MS))
	{
		if (system_milliseconds() - create.start_since >= JOIN_WAIT_MS)
		{
			ae_lobby_leave();
			start_failed("This PC's game did not open its lobby in 10 seconds.");
		}
		return;
	}
	result = ae_lobby_add_local_player(0);
	if (!result.ok)
	{
		ae_lobby_leave();
		start_failed(result.reason);
		return;
	}
	if (!ui_widget_port_multiplayer_map_choose(create.map_index))
	{
		ae_lobby_leave();
		start_failed("That map can't be played with others in this build.");
		return;
	}
	ae_setup_edit_apply();
	/* (the game's name in lists: player 1's, as Server Setup's; the most players) */
	{
		wchar_t name[16];
		int at;

		for (at = 0; at < 15 && roster.count && roster.players[0].name[at]; at++)
			name[at] = (wchar_t)(unsigned char)roster.players[0].name[at];
		name[at] = 0;
		/* (SPLIT SCREEN: room for this PC's four players and the menus' most bots; else MAX PLAYERS) */
		network_game_server_port_set_settings(name, create.network == NETWORK_LOCAL ? 4 + BOTS_MENU_MAXIMUM :
			maximum_players[create.players_index]);
	}
	create.start_step = START_DONE;
	platform_log("ae create game: hosted, player 1 in");
	lobby_open(0);
}

static void create_accept(short row)
{
	struct ae_flow_row const *r = &create.flow.rows[row];

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
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case ROW_NETWORK:
		create.network = (create.network + 1) % NETWORKS;
		ae_sound_request(AE_SOUND_CURSOR, 0);
		break;
	case ROW_START:
		start_game();
		break;
	case ROW_JOIN:
		if (ae_screen_join_invite_open("CREATE GAME"))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case ROW_PRIVACY:
		create.privacy = (create.privacy + 1) % PRIVACIES;
		ae_sound_request(AE_SOUND_CURSOR, 0);
		break;
	default:
		break;
	}
}

/* MAP and GAMETYPE stepped in place (left / right, as their rows' arrows say): the next map of the game's list (in the
lobby, the server's at once), the next saved gametype (a new copy) */
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
	/* (in the lobby: the server's map now) */
	if (ae_ui_holds(&lobby) && ae_lobby_owns_session() && ui_widget_port_multiplayer_map_choose(next))
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

static void create_step(short row, short direction)
{
	struct ae_flow_row const *r = &create.flow.rows[row];

	if (r->kind == ROW_MAP)
		map_step(direction);
	else if (r->kind == ROW_GAMETYPE)
		gametype_step(direction);
	else if (r->kind == ROW_PRIVACY)
	{
		short next = ae_value_step(PRIVACIES, (short)create.privacy, direction);

		ae_sound_request(next == create.privacy ? AE_SOUND_FAILURE : AE_SOUND_CURSOR, 0);
		create.privacy = next;
	}
	else if (r->kind == ROW_MAXIMUM_PLAYERS)
	{
		short next = ae_value_step(MAXIMUM_PLAYERS_COUNT, create.players_index, direction);

		ae_sound_request(next == create.players_index ? AE_SOUND_FAILURE : AE_SOUND_CURSOR, 0);
		create.players_index = next;
	}
	else if (r->kind == ROW_OPTION)
		option_step(r->option, direction, NULL);
	else if (r->kind == ROW_BOTS || r->kind == ROW_BOT_SKILL)
		bots_step(r->kind == ROW_BOT_SKILL, direction, "");
	else if (r->kind == ROW_NETWORK)
	{
		short next = ae_value_step(NETWORKS, (short)create.network, direction);

		ae_sound_request(next == create.network ? AE_SOUND_FAILURE : AE_SOUND_CURSOR, 0);
		create.network = next;
	}
}

static void create_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &create.flow;
	char crumb[96], name[16];
	float panel_x, y, text, body, line;

	(void)instance;
	ae_flow_full_density(flow);
	create_rows();
	ae_setup_edit_name(name, sizeof(name));
	snprintf(crumb, sizeof(crumb), "%s", create.start_step == START_JOINING ? "HOSTING..." :
		network_names[create.network]);
	ae_flow_title(flow, "CREATE GAME", crumb);
	ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
	/* the panel: the game at a glance */
	panel_x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X;
	y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	text = ae_size_text(&flow->density);
	body = ae_flow_u(flow, flow->density.metrics->body);
	line = ae_flow_u(flow, flow->density.metrics->body_line);
	ae_draw_text(AE_FONT_ROW, text * 1.5f, panel_x, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, create.map_name[0] ? create.map_name :
		"No map yet");
	y += ae_draw_cap_height(AE_FONT_ROW, text * 1.5f) + ae_flow_u(flow, 18.0f);
	{
		struct ae_setup_access access = access_now();
		char words[160];

		snprintf(words, sizeof(words), "%s: %s %s, %s", name, ae_setup_label(&access, score_option(access.engine)),
			option_value(score_option(access.engine)), option_value(AE_SO_TIME_LIMIT));
		ae_draw_text(AE_FONT_BODY, body, panel_x, y, AE_ALIGN_LEFT, AE_COLOR_ACCENT, words);
		y += line;
		snprintf(words, sizeof(words), "%d rules changed from the saved gametype.", ae_setup_changes(&access));
		ae_draw_text(AE_FONT_BODY, body, panel_x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, words);
	}
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_START, "Start game", AE_ACTION_START);
	ae_flow_draw_prompts(flow);
}

static void create_update(struct ae_screen *instance)
{
	(void)instance;
	start_update();
}

static int create_handle(struct ae_screen *instance, struct ae_event const *event)
{
	struct ae_flow_screen *flow = &create.flow;

	(void)instance;
	/* (hosting: wait; B still leaves) */
	if (create.start_step == START_JOINING && event->action != AE_ACTION_BACK)
		return 1;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		if (create.start_step == START_JOINING)
		{
			ae_lobby_leave();
			create.start_step = START_IDLE;
			create.starting = 0;
			return 1;
		}
		return 0;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		create_step(flow->focus, event->action == AE_ACTION_RIGHT ? 1 : -1);
		return 1;
	case AE_ACTION_ACCEPT:
		create_accept(flow->focus);
		return 1;
	case AE_ACTION_START:
		start_game();
		return 1;
	default:
		return 1;
	}
}

static void create_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	short row;

	if (ae_prompts_pointer(create.flow.prompts, create.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK && create.start_step != START_JOINING)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			create_handle(instance, &event);
		return;
	}
	row = ae_flow_rows_pointer(&create.flow, pointer);
	if (row <= -2)
		create_step(create.flow.focus, row == -3 ? 1 : -1);
	else if (row >= 0)
		create_accept(row);
}

static void create_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae create game: closed");
	/* (the game's menus back, unless a game is on its way, AE's lobby takes over, or AE's MULTIPLAYER is under it) */
	if (!create.starting && !create.from_menu && !ae_lobby_owns_session())
	{
		ui_widgets_close_all();
		main_screen_shell_load();
	}
}

static struct ae_screen_class const create_class =
{
	.name = "create game", .leave = create_leave, .handle = create_handle, .draw = create_draw,
	.pointer = create_pointer, .update = create_update,
};

static int create_push(void)
{
	create.start_step = START_IDLE;
	create.starting = 0;
	create.flow.focus = 0;
	create.flow.hit_rows = HIT_ROWS;
	create.flow.hit_prompts = HIT_PROMPTS;
	if (!ae_ui_push(&create_class, AE_OWNER_ANY, &create))
		return 0;
	platform_log("ae create game: open");
	return 1;
}

int ae_screen_create_game_lobby_joined(const char *how)
{
	lobby_open(1);
	if (!ae_ui_holds(&lobby))
		return 0;
	snprintf(lobby.joined_how, sizeof(lobby.joined_how), "%s", how ? how : "");
	lobby.from_browser = 1;
	return 1;
}

int ae_screen_create_game_from_menu(void)
{
	char const *const *names = NULL;
	short last = 0;

	memset(&create, 0, sizeof(create));
	create.network = NETWORK_LAN;
	create.privacy = config_boolean("network.host_public") ? PRIVACY_PUBLIC : PRIVACY_INVITE_ONLY;
	create.players_index = MAXIMUM_PLAYERS_DEFAULT;
	if (!ae_setup_edit_open() && !ae_setup_edit_begin_named(NULL))
		return 0;
	ui_widget_port_multiplayer_maps(&names, &last);
	ae_setup_maps_prefetch();
	create.map_index = last;
	map_name_of(last);
	create.from_menu = 1;
	return create_push();
}

int ae_screen_create_game_open(void)
{
	char const *const *names = NULL;
	short last = 0;

	memset(&create, 0, sizeof(create));
	create.network = NETWORK_LAN;
	create.privacy = config_boolean("network.host_public") ? PRIVACY_PUBLIC : PRIVACY_INVITE_ONLY;
	create.players_index = MAXIMUM_PLAYERS_DEFAULT;
	if (!ae_setup_edit_begin_named(getenv("HALO_AE_TEST_GAMETYPE")))
		return 0;
	ui_widget_port_multiplayer_maps(&names, &last);
	/* (the maps' facts read on the worker now, before CHANGE MAP wants them) */
	ae_setup_maps_prefetch();
	create.map_index = last;
	map_name_of(last);
	ae_ui_replace_menus();
	create.menus_closed = 1;
	(void)ae_settings_ui_scale();
	return create_push();
}

/* ---------- the LOBBY */

static void lobby_rows(void)
{
	static const short quick[] = { AE_SO_TIME_LIMIT, -1, AE_SO_SHIELDS, AE_SO_PRIMARY, AE_SO_GRENADES_ON_MAP,
		AE_SO_SENSOR_RANGE, AE_SO_RESPAWN_TIME };
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_setup_access access = access_now();
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
			"The host picks the map and rules and starts the game. B leaves it." };
		flow->count = count;
		flow->focus = (short)(count - 1);
		return;
	}
	count = setup_rows(flow, 0);
	if (create.network == NETWORK_ONLINE)
		flow->rows[count++] = (struct ae_flow_row){ ROW_INVITE, -1, "INVITE", lobby.invite_shown,
			"A copies your game's invite link (halo://join/...) to paste to friends. Anyone with it can join." };
	flow->rows[count++] = (struct ae_flow_row){ ROW_HEADER, -1, "QUICK OPTIONS", NULL, NULL };
	for (index = 0; index < (short)(sizeof(quick) / sizeof(quick[0])); index++)
	{
		short option = quick[index] < 0 ? score_option(access.engine) : quick[index];

		if (ae_setup_visible(&access, option))
			flow->rows[count++] = option_row(option);
	}
	flow->rows[count++] = (struct ae_flow_row){ ROW_START, -1, "START GAME", NULL,
		"Starts the game for everyone in the lobby. After it, everyone comes back here." };
	flow->count = count;
	if (flow->focus >= count)
		flow->focus = (short)(count - 1);
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

static void lobby_start(void)
{
	struct ae_result result = ae_lobby_start();

	if (!result.ok)
	{
		ae_flow_error(&lobby.flow, "The game did not start", result.reason);
		return;
	}
	platform_log("ae create game: lobby: start");
	ae_sound_request(AE_SOUND_FORWARD, 0);
	/* (no AE screen over the game: the take-over brings the lobby back after it) */
	create.starting = 1;
	clear_stack();
}

static void leave_picked(short choice, void *context)
{
	(void)context;
	if (choice != 1)
		return;
	platform_log("ae create game: lobby: leave");
	lobby.leaving = 1;
	ae_lobby_leave();
	ae_ui_remove(&lobby);
	/* (back to CREATE GAME: under the lobby still, or opened again after a game took it off; a browser's join goes
	back to the browser under it) */
	create.start_step = START_IDLE;
	create.starting = 0;
	if (!lobby.from_browser && !ae_ui_holds(&create))
		create_push();
}

static void lobby_leave_ask(void)
{
	struct ae_dialog_spec spec;

	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "Leave the lobby?";
	spec.body = lobby.joiner ? "You leave the host's game." :
		"The game closes for everyone in it. Its settings stay for CREATE GAME.";
	spec.choices[0] = "STAY";
	spec.choices[1] = "LEAVE";
	spec.choice_count = 2;
	spec.has_cancel = 1;
	spec.density = lobby.flow.density;
	spec.bounds = lobby.flow.frame.rect;
	spec.picked = leave_picked;
	ae_dialog_open(&spec, AE_OWNER_ANY);
}

static void lobby_accept(short row)
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
	case ROW_INVITE:
		invite_copy();
		break;
	default:
		break;
	}
}

/* the host's BOTS and BOT SKILL at the roster panel's foot (the rows on the left have no room for two more with their
help); returns their top */
static float draw_bot_rows(float x, float width, float bottom)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_density const *d = &flow->density;
	float gap = ae_flow_u(flow, d->metrics->gap), row_height = ae_size_row(d), y, top;
	int row;

	top = y = bottom - (float)BOT_ROWS * row_height - (float)(BOT_ROWS - 1) * gap;
	for (row = 0; row < BOT_ROWS; row++)
	{
		struct ae_flow_row const source = bot_row(row);
		struct ae_row widget;

		memset(&widget, 0, sizeof(widget));
		widget.label = source.label;
		widget.value = source.value;
		if (row == lobby.bot_focus)
			widget.flags |= AE_ROW_FOCUSED;
		lobby.bot_rects[row].x = x;
		lobby.bot_rects[row].y = y;
		lobby.bot_rects[row].width = width;
		lobby.bot_rects[row].height = ae_widget_row(d, x, y, width, &widget, HIT_BOT_ROWS, (short)row);
		y += lobby.bot_rects[row].height + gap;
	}
	return top;
}

/* a bot row's help where the rows' help goes (ae_flow_draw_rows' place and rule: under the rows on the left, when
there is room under them, as for theirs) */
static void draw_bot_help(float width)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_density const *d = &flow->density;
	float x = ae_flow_left_x(flow), body = ae_flow_u(flow, d->metrics->body), line = ae_flow_u(flow, d->metrics->body_line);
	float help_gap = ae_flow_u(flow, 28.0f), help_y = ae_flow_content_bottom(flow) - line * 2.0f, rows_end;
	char lines[2][160];
	int index, count;

	if (lobby.bot_focus < 0 || lobby.bot_focus >= BOT_ROWS || flow->count <= 0)
		return;
	rows_end = flow->rects[flow->count - 1].y + flow->rects[flow->count - 1].height + ae_flow_u(flow, d->metrics->gap);
	if (help_y < rows_end + help_gap)
		return;
	ae_draw_rect(x, help_y - help_gap * 0.5f, width, ae_flow_u(flow, AE_FLOW_RULE_U), 0.0f, AE_COLOR_RULE);
	count = ae_wrap_text(AE_FONT_BODY, body, bot_row(lobby.bot_focus).help, width, lines, 2);
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, x, help_y + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[index]);
}

static void draw_roster(float x, float y, float width, float bottom)
{
	struct ae_flow_screen *flow = &lobby.flow;
	float inset = ae_flow_u(flow, AE_FLOW_INSET_U), minor = ae_size_minor(&flow->density), card_y, cards_bottom = bottom;
	char heading[48];
	short index, people = lobby.have_roster ? lobby.roster.people : 0;
	short bots = lobby.have_roster ? (short)(lobby.roster.count - lobby.roster.people) : 0;

	ae_draw_rect(x, y, width, bottom - y, ae_flow_u(flow, flow->density.metrics->corner), AE_COLOR_PANEL);
	ae_draw_outline(x, y, width, bottom - y, ae_flow_u(flow, flow->density.metrics->corner), ae_flow_u(flow, AE_FLOW_RULE_U), AE_COLOR_RULE);
	/* (people, not machines or bots: a split screen guest is one; the bots apart) */
	if (bots > 0)
		snprintf(heading, sizeof(heading), "PLAYERS  %d   BOTS  %d", people, bots);
	else
		snprintf(heading, sizeof(heading), "PLAYERS  %d", people);
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x + inset, ae_flow_cap_top(AE_FONT_BODY, minor, y + ae_flow_u(flow, 29.0f)),
		AE_ALIGN_LEFT, AE_COLOR_MUTED, heading);
	if (!lobby.joiner)
		/* (a card's pitch has its gap: the last card ends AE_FLOW_CARD_GAP_U over the rows) */
		cards_bottom = draw_bot_rows(x + inset, width - 2.0f * inset, bottom - inset);
	card_y = y + ae_flow_u(flow, 56.0f);
	for (index = 0; lobby.have_roster && index < lobby.roster.count; index++)
	{
		static const unsigned int colors[4] = { 0xC0392BFFu, 0x2E86DEFFu, 0x7FA035FFu, 0xE6B422FFu };
		struct ae_lobby_player const *player = &lobby.roster.players[index];
		struct ae_roster_card card;
		float pitch = ae_flow_u(flow, flow->density.metrics->card) + ae_flow_u(flow, AE_FLOW_CARD_GAP_U);

		if (card_y + pitch > cards_bottom)
			break;
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
		card_y += ae_widget_roster_card(&flow->density, x + inset, card_y, width - 2.0f * inset, &card, HIT_CARDS, index) +
			ae_flow_u(flow, AE_FLOW_CARD_GAP_U);
	}
}

static void lobby_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &lobby.flow;
	char crumb[96];
	float roster_x;

	(void)instance;
	ae_flow_full_density(flow);
	lobby_rows();
	snprintf(crumb, sizeof(crumb), "%s", !lobby.joiner ? network_names[create.network] : lobby.joined_how[0] ?
		lobby.joined_how : "JOINED BY INVITE");
	ae_flow_title(flow, "LOBBY", crumb);
	if (lobby.joiner)
		lobby.bot_focus = -1;
	/* (a bot row under the roster has the focus: no row on the left lit, nor its help) */
	{
		short focus = flow->focus;

		if (lobby.bot_focus >= 0)
			flow->focus = -1;
		ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
		flow->focus = focus;
	}
	draw_bot_help(flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
	roster_x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X;
	draw_roster(roster_x, ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y), ae_flow_right_x(flow) - roster_x, ae_flow_content_bottom(flow));
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Leave", AE_ACTION_BACK);
	if (!lobby.joiner)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_START, "Start game", AE_ACTION_START);
	if (!lobby.joiner && create.network == NETWORK_ONLINE)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Copy invite", AE_ACTION_X);
	ae_flow_draw_prompts(flow);
}

static void lobby_update(struct ae_screen *instance)
{
	unsigned long now = system_milliseconds();

	(void)instance;
	/* (GAME OPTIONS closed over the lobby: its changes to the server) */
	if (lobby.options_open && ae_ui_top() && ae_ui_top()->data == &lobby)
	{
		lobby.options_open = 0;
		ae_setup_edit_apply();
	}
	if (now - lobby.roster_at >= 250)
	{
		lobby.roster_at = now;
		lobby.have_roster = ae_lobby_owns_session() && ae_lobby_roster(&lobby.roster).ok;
		/* (the invite: made once the game is hosted for the internet; shown by its first digits only) */
		if (create.network == NETWORK_ONLINE && !lobby.joiner)
		{
			if (!config_boolean("network.online"))
				snprintf(lobby.invite_shown, sizeof(lobby.invite_shown), "INTERNET PLAY IS OFF");
			else if (!p2p_invite_link(lobby.invite, sizeof(lobby.invite)) || !lobby.invite[0])
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
	/* (a joiner: the host started; no AE screen over the game, the take-over brings the lobby back) */
	if (lobby.joiner && game_engine_running() && !main_menu_is_active() && !game_engine_showing_postgame())
	{
		platform_log("ae create game: lobby: the host started");
		create.starting = 1;
		clear_stack();
	}
}

static int lobby_handle(struct ae_screen *instance, struct ae_event const *event)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_flow_row const *row;

	(void)instance;
	lobby_rows();
	row = &flow->rows[flow->focus];
	switch (event->action)
	{
	case AE_ACTION_BACK:
		lobby_leave_ask();
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		/* (the bot rows under the roster: down from the last row on the left, up from them back to it) */
		if (lobby.bot_focus >= 0)
		{
			short next = (short)(lobby.bot_focus + (event->action == AE_ACTION_UP ? -1 : 1));

			if (next >= BOT_ROWS)
				return 1;
			lobby.bot_focus = next;
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
			return 1;
		}
		if (!ae_flow_focus_step(flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat) &&
			event->action == AE_ACTION_DOWN && !lobby.joiner)
		{
			lobby.bot_focus = 0;
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		}
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (lobby.joiner)
			return 1;
		if (lobby.bot_focus >= 0)
			bots_step(lobby.bot_focus, event->action == AE_ACTION_RIGHT ? 1 : -1, "lobby: ");
		else if (row->kind == ROW_OPTION)
			option_step(row->option, event->action == AE_ACTION_RIGHT ? 1 : -1, "lobby: quick");
		else if (row->kind == ROW_MAP)
			map_step(event->action == AE_ACTION_RIGHT ? 1 : -1);
		else if (row->kind == ROW_GAMETYPE)
			gametype_step(event->action == AE_ACTION_RIGHT ? 1 : -1);
		return 1;
	case AE_ACTION_ACCEPT:
		if (!lobby.joiner && lobby.bot_focus < 0)
			lobby_accept(flow->focus);
		return 1;
	case AE_ACTION_START:
		if (!lobby.joiner)
			lobby_start();
		return 1;
	case AE_ACTION_X:
		if (!lobby.joiner && create.network == NETWORK_ONLINE)
			invite_copy();
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
	/* (the bot rows under the roster: hover focuses, their arrows step) */
	if (!lobby.joiner && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_BOT_ROWS && hit.index >= 0 &&
		hit.index < BOT_ROWS)
	{
		if (pointer->moved)
			lobby.bot_focus = hit.index;
		if (pointer->left_clicks && (hit.part == AE_PART_ARROW_LEFT || hit.part == AE_PART_ARROW_RIGHT))
			bots_step(hit.index, hit.part == AE_PART_ARROW_RIGHT ? 1 : -1, "lobby: ");
		return;
	}
	if (pointer->moved && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_ROWS)
		lobby.bot_focus = -1;
	row = ae_flow_rows_pointer(&lobby.flow, pointer);
	if (row <= -2 && lobby.flow.rows[lobby.flow.focus].kind == ROW_OPTION)
		option_step(lobby.flow.rows[lobby.flow.focus].option, row == -3 ? 1 : -1, "lobby: quick");
	else if (row >= 0)
		lobby_accept(row);
}

static void lobby_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae create game: lobby: %s", lobby.leaving ? "left" : "off the stack");
	lobby.leaving = 0;
}

static struct ae_screen_class const lobby_class =
{
	.name = "setup lobby", .leave = lobby_leave, .handle = lobby_handle, .draw = lobby_draw, .pointer = lobby_pointer,
	.update = lobby_update, .start_from_anyone = 1,
};

static void lobby_open(int joiner)
{
	memset(&lobby, 0, sizeof(lobby));
	lobby.joiner = joiner;
	lobby.bot_focus = -1;
	lobby.flow.hit_rows = HIT_ROWS;
	lobby.flow.hit_prompts = HIT_PROMPTS;
	ae_lobby_set_pregame_screen(&lobby_class, &lobby);
	if (!ae_ui_push(&lobby_class, AE_OWNER_ANY, &lobby))
	{
		platform_log("ae create game: lobby: could not open");
		return;
	}
	/* (the focus on START GAME, the last row) */
	lobby_rows();
	lobby.flow.focus = (short)(lobby.flow.count - 1);
	platform_log("ae create game: lobby: open");
}
