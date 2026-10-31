/*
AE_SCREEN_BROWSER.C

AE's SERVER BROWSER and LAN GAMES (ae_screen_browser.h; the mockup's board AEBrowser): one screen of two kinds. SERVER BROWSER lists the internet's games
(the game list, ../src/browser.h: browser_get_games, its public listings followed while the screen is open); LAN GAMES
lists the games this PC's network client hears on the LAN (ae_glue_lobby.h: ae_lobby_lan_search). The title and the
games' count top right; the filters between LT and RT (SERVER BROWSER: ALL, NOT FULL, HAS PLAYERS, NO PASSWORD); the
table (GAME, MAP, GAMETYPE, PLAYERS, PING), a game this build can't join dimmed; on the right the focused game's card:
its map, name, map . gametype . players, its state in plain words, its host, its players.

Input (anyone's): up / down move; A picks player 1's profile, then joins the focused game; X refreshes (LAN: searches again); Y opens JOIN BY
ADDRESS (ae_screen_join_address.h); LB opens JOIN WITH INVITE (SERVER BROWSER: ae_screen_join_invite.h, a link or code pasted or typed); LT / RT the filter; B back. While a
join goes on only B, which leaves it.
Joined: player 1 in, then the joiner's LOBBY (ae_screen_create_game.h); leaving it comes back here. A Halo PC map the
PC lacks: a downloadable one is joined (the download offer takes over once the host names it, browser_screen.c),
else the card says what is missing and A says it.

Its log: "ae browser: open (server browser|lan games)", "filter <name>: N of M", "join '<game>'", "joined", "join
failed: <why>", "closed".
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "../src/ae_draw.h"
#include "ae_glue_lobby.h"
#include "ae_header_lines.h"
#include "ae_hooks.h"
#include "ae_screen_browser.h"
#include "ae_screen_create_game.h"
#include "ae_screen_join_address.h"
#include "ae_screen_join_invite.h"
#include "ae_screen_lobby_profile.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "game_modes.h"
#include "halo_map_families.h"
#include "halo_port_limits.h"
#include "halo_server_browser.h"
#ifdef HALO_GAME_BROWSER
#include "../src/ae_lan_hint.h"
#include "../src/browser.h"
#endif

void platform_log(char const *format, ...);
int config_boolean(char const *name);

enum
{
	BROWSER_HIT = 0x7800,
	HIT_LIST = BROWSER_HIT + 1, HIT_CHIPS, HIT_PROMPTS,
	GAMES_MAXIMUM = 64,
	ROSTER_MAXIMUM = 16,
	/* (ms) the list read again this often; a lost LAN search started again after this long */
	READ_INTERVAL = 500,
	SEARCH_RETRY = 2000,
	/* (ms) with nothing heard on the LAN this long, the firewall's hint */
	LAN_HINT_AFTER = 8000
};
enum { FILTER_ALL, FILTER_NOT_FULL, FILTER_HAS_PLAYERS, FILTER_NO_PASSWORD, FILTERS };

/* the frame's positions (fractions of 1920 x 1080 at 100 %) */
#define LIST_WIDTH 0.60f
#define PANEL_X (1250.0f / 1920.0f)
/* sizes, u */
#define GLYPH_GAP_U 16.0f
#define HEADER_ROW_U 30.0f
#define CARD_RATIO 0.42f
#define PANEL_GAP_U 16.0f
#define NAME_SCALE 1.4f
#define FOOTER_ROOM_U 40.0f
/* the table's columns after GAME's (fractions of the row's width, from its right) */
#define PING_COLUMN 0.10f
#define PLAYERS_COLUMN 0.12f
#define TYPE_COLUMN 0.20f
#define MAP_COLUMN 0.22f

static char const *const filter_names[FILTERS] = { "ALL", "NOT FULL", "HAS PLAYERS", "NO PASSWORD" };

/* a game as the table and the card show it, from either list */
struct game
{
	char name[48], map[64], type[24], players[16], ping[16];
	char state[112];              /* the card's line: why it can't be joined, or what joining it does */
	char host[64];                /* the card's host line ("" none) */
	char roster[ROSTER_MAXIMUM][32];
	short roster_count, players_count, maximum_players;
	int blocked;                  /* A refuses it, with state as the reason */
	int locked;
	char invite[80];              /* SERVER BROWSER's join */
	short lan_index;              /* LAN GAMES' join */
	unsigned char lan_nonce[8];   /* LAN host identity: the discovery slot may be reused */
};

static struct
{
	struct ae_flow_screen flow;
	int kind, filter;
	struct game games[GAMES_MAXIMUM];
	short count;
	short shown[GAMES_MAXIMUM], shown_count;
	struct ae_list_view list;
	struct ae_chip chips[FILTERS];
	char chip_labels[FILTERS][32];
	unsigned long read_at, search_at, opened_at;
	int list_status;              /* SERVER BROWSER: browser_list_status's */
	/* a join under way: its game's name and kind of join */
	int joining;
	char joining_name[48];
	short pending_lan_index;
	unsigned char pending_lan_nonce[8];
	char pending_invite[80];
	char focus_name[48];          /* the focused game's, kept across reads */
} screen;

static struct ae_screen_class const browser_class;

static float u(float spec_units)
{
	return ae_flow_u(&screen.flow, spec_units);
}

static float cap_top(int font, float size, float center_y)
{
	return center_y - ae_draw_cap_height(font, size) * 0.5f;
}

/* ---------- the games */

/* a game's map as players name it: a Halo PC map's name from the map list, else the stock maps' ("BLOOD GULCH"), else
its file's in capitals */
static void map_name(char const *map, char *text, size_t size)
{
	wchar_t wide[64];
	short index;

	wide[0] = 0;
	if (server_browser_map_family(map, wide, (short)NUMBEROF(wide)) == _map_family_xbox || !wide[0])
		ae_scoreboard_header_map_name(map, wide, NUMBEROF(wide));
	ae_lobby_utf16_to_utf8(text, (int)size, (const unsigned short *)wide, NUMBEROF(wide));
	for (index = 0; text[index]; index++)
	{
		if (text[index] >= 'a' && text[index] <= 'z')
			text[index] = (char)(text[index] - 'a' + 'A');
	}
}

static void type_name(short engine, int teams, unsigned char kind, char *text, size_t size)
{
	static char const *const engines[] = { "", "CTF", "SLAYER", "ODDBALL", "KING", "RACE", "BOMB" };
	char const *name = engine >= 0 && engine < (short)NUMBEROF(engines) ? engines[engine] : "";
	/* (an OpenCE gametype's kind, as its advertisement or listing says it: its short name, "SWAT", "HC CTF") */
	wchar_t const *kind_name = kind ? game_modes_kind_name(engine, kind, TRUE) : NULL;

	if (kind_name)
	{
		size_t index;

		for (index = 0; index + 1 < size && kind_name[index]; index++)
			text[index] = kind_name[index] >= 0x20 && kind_name[index] < 0x7F ? (char)kind_name[index] : '?';
		text[index] = 0;
		return;
	}

	/* (CTF and Bomb are team games only: no TEAM before them) */
	snprintf(text, size, "%s%s", teams && engine != 1 && engine != 6 && name[0] ? "TEAM " : "", name[0] ? name : "-");
}

/* the map's part of a game's state: missing and not downloadable blocks it; downloadable says so */
static void map_state(struct game *game, char const *map)
{
	wchar_t message[112];

	if (!server_browser_map_blocked(map, message, (short)NUMBEROF(message)))
		return;
	ae_lobby_utf16_to_utf8(game->state, sizeof(game->state), (const unsigned short *)message, NUMBEROF(message));
	if (!server_browser_map_downloadable(map))
		game->blocked = 1;
}

/* a host this build does not join: why, as the game lists word it */
static int version_state(struct game *game, int join_state, unsigned int version)
{
	char const *format = halo_port_join_reason_format(join_state, version);

	if (!format)
		return 0;
	snprintf(game->state, sizeof(game->state), format, version);
	game->blocked = 1;
	return 1;
}

static void players_text(struct game *game)
{
	snprintf(game->players, sizeof(game->players), "%d / %d", game->players_count, game->maximum_players);
}

/* the default state: full, or ready */
static void ready_state(struct game *game, int open)
{
	if (game->state[0])
		return;
	if (!open)
	{
		snprintf(game->state, sizeof(game->state), "Not taking players right now.");
		game->blocked = 1;
	}
	else if (game->maximum_players > 0 && game->players_count >= game->maximum_players)
	{
		snprintf(game->state, sizeof(game->state), "Full.");
		game->blocked = 1;
	}
	else
		snprintf(game->state, sizeof(game->state), game->locked ? "Ready to join. It has a password." :
			"Ready to join.");
}

#ifdef HALO_GAME_BROWSER
static void internet_read(void)
{
	static struct browser_game listed[BROWSER_MAXIMUM_GAMES];
	int count = browser_get_games(listed, BROWSER_MAXIMUM_GAMES), index, player;

	screen.list_status = browser_list_status();
	screen.count = 0;
	for (index = 0; index < count && screen.count < GAMES_MAXIMUM; index++)
	{
		struct browser_game const *source = &listed[index];
		struct game *game = &screen.games[screen.count];
		int join_state = halo_port_advertised_join_state(source->version, HALO_PORT_ADVERTISED_DISTRIBUTED_FLAG,
			(unsigned int)delta_legacy_minimum(), (unsigned int)delta_legacy_maximum());

		/* (co-op games: CAMPAIGN's, never listed here) */
		if (browser_map_is_campaign(source->map))
			continue;
		memset(game, 0, sizeof(*game));
		ae_lobby_utf16_to_utf8(game->name, sizeof(game->name), source->name, BROWSER_NAME_LENGTH);
		map_name(source->map, game->map, sizeof(game->map));
		type_name(source->engine, source->teams, source->kind, game->type, sizeof(game->type));
		game->players_count = source->players;
		game->maximum_players = source->maximum_players;
		players_text(game);
		if (source->ping < 0)
			snprintf(game->ping, sizeof(game->ping), "-");
		else
			snprintf(game->ping, sizeof(game->ping), "%s%d", source->ping_estimated ? "~" : "", source->ping);
		game->locked = source->locked;
		snprintf(game->invite, sizeof(game->invite), "%s", source->invite);
		game->lan_index = -1;
		if (!version_state(game, join_state, source->version))
			map_state(game, source->map);
		ready_state(game, source->open);
		snprintf(game->host, sizeof(game->host), "%s%s%s%s", source->hosting == BROWSER_HOSTING_DEDICATED ?
			"Dedicated server" : "Hosted by a player", source->region[0] ? " \xE2\x80\xA2 " : "", source->region,
			source->ping >= 0 && source->ping_estimated ? " \xE2\x80\xA2 ping estimated" : "");
		for (player = 0; player < source->roster_count && player < BROWSER_LISTED_ROSTER &&
			game->roster_count < ROSTER_MAXIMUM; player++)
		{
			ae_lobby_utf16_to_utf8(game->roster[game->roster_count++], sizeof(game->roster[0]),
				source->roster[player].name, BROWSER_PLAYER_NAME_LENGTH);
		}
		screen.count++;
	}
}
#endif

static void lan_read(void)
{
	static struct ae_lan_game heard[AE_LAN_GAMES];
	short count = ae_lobby_lan_games(heard, AE_LAN_GAMES), index;

	screen.count = 0;
	for (index = 0; index < count && screen.count < GAMES_MAXIMUM; index++)
	{
		struct ae_lan_game const *source = &heard[index];
		struct game *game = &screen.games[screen.count++];

		memset(game, 0, sizeof(*game));
		snprintf(game->name, sizeof(game->name), "%s", source->name);
		map_name(source->map, game->map, sizeof(game->map));
		type_name(source->engine, source->teams, source->kind, game->type, sizeof(game->type));
		game->players_count = source->players;
		game->maximum_players = source->maximum_players;
		players_text(game);
		snprintf(game->ping, sizeof(game->ping), "LAN");
		game->lan_index = source->index;
		memcpy(game->lan_nonce, source->nonce, sizeof(game->lan_nonce));
		if (!version_state(game, source->join_state, source->version))
			map_state(game, source->map);
		ready_state(game, 1);
		snprintf(game->host, sizeof(game->host), "%s", source->in_progress ? "On your network \xE2\x80\xA2 playing now" :
			"On your network \xE2\x80\xA2 in its lobby");
	}
}

static int filter_keeps(struct game const *game, int filter)
{
	switch (filter)
	{
	case FILTER_NOT_FULL: return !game->maximum_players || game->players_count < game->maximum_players;
	case FILTER_HAS_PLAYERS: return game->players_count > 0;
	case FILTER_NO_PASSWORD: return !game->locked;
	default: return 1;
	}
}

static short filter_count(int filter)
{
	short count = 0, index;

	for (index = 0; index < screen.count; index++)
		count += (short)filter_keeps(&screen.games[index], filter);
	return count;
}

/* the games read again, filtered; the focus kept on the same game where it still is */
static void games_read(void)
{
	short index, focus = -1;

#ifdef HALO_GAME_BROWSER
	if (screen.kind == AE_BROWSER_INTERNET)
		internet_read();
	else
#endif
	if (screen.kind == AE_BROWSER_LAN)
		lan_read();
	screen.shown_count = 0;
	for (index = 0; index < screen.count; index++)
	{
		if (!filter_keeps(&screen.games[index], screen.filter))
			continue;
		if (screen.focus_name[0] && !strcmp(screen.games[index].name, screen.focus_name))
			focus = screen.shown_count;
		screen.shown[screen.shown_count++] = index;
	}
	ae_list_set_count(&screen.list.list, screen.shown_count);
	if (focus >= 0 && focus != screen.list.list.focus)
		ae_list_set_focus(&screen.list.list, focus);
	screen.read_at = system_milliseconds();
}

static struct game const *focused_game(void)
{
	short focus = screen.list.list.focus;

	return focus >= 0 && focus < screen.shown_count ? &screen.games[screen.shown[focus]] : NULL;
}

static void focus_taken(void)
{
	struct game const *game = focused_game();

	snprintf(screen.focus_name, sizeof(screen.focus_name), "%s", game ? game->name : "");
}

static void filter_set(int filter)
{
	screen.filter = (filter + FILTERS) % FILTERS;
	screen.focus_name[0] = 0;
	games_read();
	ae_list_set_focus(&screen.list.list, 0);
	focus_taken();
	platform_log("ae browser: filter %s: %d of %d", filter_names[screen.filter], screen.shown_count, screen.count);
}

/* ---------- joining */

static void lan_search(void)
{
	struct ae_result result = ae_lobby_lan_search();

	screen.search_at = system_milliseconds();
	if (!result.ok)
		platform_log("ae browser: lan search failed: %s", result.reason);
}

static void join_failed(const char *why)
{
	screen.joining = 0;
	platform_log("ae browser: join failed: %s", why);
	ae_flow_error(&screen.flow, "Can't join", why);
	if (screen.kind == AE_BROWSER_LAN)
		lan_search();
}

static void join_started(const char *name)
{
	screen.joining = 1;
	if (name != screen.joining_name)
		snprintf(screen.joining_name, sizeof(screen.joining_name), "%s", name);
	platform_log("ae browser: join '%s'", name);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void join_profile_done(short player, int picked, void *context)
{
	struct ae_result result;

	(void)player;
	(void)context;
	if (!picked)
		return;
	if (screen.kind == AE_BROWSER_LAN)
		result = ae_lobby_lan_join(screen.pending_lan_index, screen.pending_lan_nonce);
	else
		result = ae_lobby_join_address(screen.pending_invite);
	if (!result.ok)
	{
		join_failed(result.reason);
		return;
	}
	join_started(screen.joining_name);
}

static void join_focused(void)
{
	struct game const *game = focused_game();

	if (!game)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (game->blocked)
	{
		ae_flow_error(&screen.flow, "Can't join", game->state);
		return;
	}
	if (screen.kind != AE_BROWSER_LAN && !config_boolean("network.online"))
	{
		ae_flow_error(&screen.flow, "Internet play is off", "Turn on internet play (network.online) to join.");
		return;
	}
	screen.pending_lan_index = game->lan_index;
	memcpy(screen.pending_lan_nonce, game->lan_nonce, sizeof(screen.pending_lan_nonce));
	snprintf(screen.pending_invite, sizeof(screen.pending_invite), "%s", game->invite);
	snprintf(screen.joining_name, sizeof(screen.joining_name), "%s", game->name);
	if (!ae_screen_lobby_profile_open(0, &screen.flow, join_profile_done, NULL))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
}

/* the join under way: joined, player 1 in, the joiner's LOBBY; else why not */
static void join_update(void)
{
	static struct ae_lobby_roster roster;
	struct ae_result failure;
	int state = ae_lobby_join_state(&failure);

	if (state == 0)
		return;
	if (state < 0)
	{
		join_failed(failure.reason[0] ? failure.reason : "The join was left.");
		return;
	}
	/* (this PC's client in the pregame first: its players are taken from then) */
	if (!ae_lobby_roster(&roster).ok || !roster.joined)
		return;
	screen.joining = 0;
	if (!ae_lobby_add_local_player(0).ok)
	{
		ae_lobby_leave();
		join_failed("The game is full, or no longer takes players.");
		return;
	}
	platform_log("ae browser: joined");
	ae_screen_create_game_lobby_joined(screen.kind == AE_BROWSER_LAN ? "JOINED ON YOUR NETWORK" :
		"JOINED FROM THE BROWSER");
}

static void join_cancel(void)
{
	ae_lobby_leave();
	screen.joining = 0;
	platform_log("ae browser: join left");
	ae_sound_request(AE_SOUND_BACK, 0);
	if (screen.kind == AE_BROWSER_LAN)
		lan_search();
}

static void refresh(void)
{
	if (screen.kind == AE_BROWSER_LAN)
	{
		lan_search();
		screen.opened_at = system_milliseconds();
	}
#ifdef HALO_GAME_BROWSER
	else
	{
		/* (followed again: the list asked again) */
		browser_follow_listings(0);
		browser_follow_listings(1);
	}
#endif
	games_read();
	platform_log("ae browser: refresh");
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

/* ---------- drawing */

struct item_context { struct ae_density const *density; float name_width; };

static void list_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct item_context const *list = context;
	struct game const *game = &screen.games[screen.shown[item]];
	struct ae_row row;
	char name[64];
	float pad = u(list->density->metrics->pad), text = ae_size_text(list->density) * 0.9f, center = y + height * 0.5f;
	float right = x + width - pad, column;
	int on_bar = (flags & AE_ROW_ON_BAR) != 0;
	unsigned int ink = game->blocked ? AE_COLOR_DISABLED_TEXT : on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TEXT;

	memset(&row, 0, sizeof(row));
	ae_fit_text(AE_FONT_ROW, ae_size_text(list->density), game->name, list->name_width, name, sizeof(name));
	row.label = name;
	row.flags = flags | (game->blocked ? AE_ROW_DISABLED : 0);
	ae_widget_row(list->density, x, y, width, &row, HIT_LIST, item);
	column = right - width * PING_COLUMN;
	ae_draw_text(AE_FONT_BODY, text, right, cap_top(AE_FONT_BODY, text, center), AE_ALIGN_RIGHT, ink, game->ping);
	ae_draw_text(AE_FONT_BODY, text, column, cap_top(AE_FONT_BODY, text, center), AE_ALIGN_RIGHT, ink, game->players);
	column -= width * PLAYERS_COLUMN;
	{
		char fitted[64];

		ae_fit_text(AE_FONT_BODY, text, game->type, width * TYPE_COLUMN - u(12.0f), fitted, sizeof(fitted));
		ae_draw_text(AE_FONT_BODY, text, column - width * TYPE_COLUMN, cap_top(AE_FONT_BODY, text, center),
			AE_ALIGN_LEFT, ink, fitted);
		column -= width * TYPE_COLUMN;
		ae_fit_text(AE_FONT_BODY, text, game->map, width * MAP_COLUMN - u(12.0f), fitted, sizeof(fitted));
		ae_draw_text(AE_FONT_BODY, text, column - width * MAP_COLUMN, cap_top(AE_FONT_BODY, text, center),
			AE_ALIGN_LEFT, ink, fitted);
	}
}

/* the columns' names over the table */
static void draw_columns(float x, float y, float width)
{
	float minor = ae_size_minor(&screen.flow.density) * 0.85f, pad = u(screen.flow.density.metrics->pad);
	float right = x + width - pad, column = right - width * PING_COLUMN;

	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.12f, x + pad, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, "GAME");
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.12f, right, y, AE_ALIGN_RIGHT, AE_COLOR_MUTED,
		screen.kind == AE_BROWSER_LAN ? "" : "PING");
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.12f, column, y, AE_ALIGN_RIGHT, AE_COLOR_MUTED, "PLAYERS");
	column -= width * PLAYERS_COLUMN;
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.12f, column - width * TYPE_COLUMN, y, AE_ALIGN_LEFT, AE_COLOR_MUTED,
		"GAMETYPE");
	column -= width * TYPE_COLUMN;
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.12f, column - width * MAP_COLUMN, y, AE_ALIGN_LEFT, AE_COLOR_MUTED,
		"MAP");
}

/* the filters between LT and RT (SERVER BROWSER); returns the y under them */
static float draw_filters(float y)
{
	struct ae_rect rects[FILTERS];
	float glyph = ae_size_glyph(&screen.flow.density), x = ae_flow_left_x(&screen.flow), chips_x, height, end;
	float room = ae_flow_right_x(&screen.flow) - x;
	int filter;

	for (filter = 0; filter < FILTERS; filter++)
	{
		snprintf(screen.chip_labels[filter], sizeof(screen.chip_labels[filter]), "%s", filter_names[filter]);
		screen.chips[filter].label = screen.chip_labels[filter];
		screen.chips[filter].count = filter_count(filter);
		screen.chips[filter].flags = filter == screen.filter ? AE_CHIP_ON : 0;
	}
	chips_x = x + ae_draw_button_width(AE_FONT_LAST_DEVICE, AE_BUTTON_LEFT_TRIGGER, glyph) + u(GLYPH_GAP_U);
	height = ae_chips_layout(&screen.flow.density, room - (chips_x - x) - u(80.0f), screen.chips, FILTERS, rects);
	ae_draw_button(AE_FONT_LAST_DEVICE, AE_BUTTON_LEFT_TRIGGER, glyph, x, y + (rects[0].height - glyph) * 0.5f,
		AE_COLOR_KEY_CAP);
	ae_widget_chips(&screen.flow.density, chips_x, y, room - (chips_x - x) - u(80.0f), screen.chips, FILTERS, HIT_CHIPS);
	end = chips_x + rects[FILTERS - 1].x + rects[FILTERS - 1].width + u(GLYPH_GAP_U);
	ae_draw_button(AE_FONT_LAST_DEVICE, AE_BUTTON_RIGHT_TRIGGER, glyph, end,
		y + rects[FILTERS - 1].y + (rects[FILTERS - 1].height - glyph) * 0.5f, AE_COLOR_KEY_CAP);
	return y + height + u(18.0f);
}

/* the empty list's words */
static char const *empty_text(void)
{
	if (screen.kind == AE_BROWSER_LAN)
	{
		/* (this computer's firewall, read once a search (ae_lan_hint.h: ufw or firewalld on Linux): a guest's
		firewall drops the hosts' answers to the search, UDP 5151, and hides every game) */
		static unsigned long looked_for;
		static int firewall;
		static char what[32], words[200];

		if (system_milliseconds() - screen.opened_at >= LAN_HINT_AFTER)
		{
			if (looked_for != screen.opened_at)
			{
				looked_for = screen.opened_at;
				firewall = ae_lan_firewall(what, sizeof(what));
			}
			if (firewall)
			{
				snprintf(words, sizeof(words), "Nothing found yet. This computer's firewall (%s) may be hiding games: "
					"let UDP 5150-5151 and 5160 in (README > Playing over LAN).", what);
				return words;
			}
			return "Nothing found on your network yet. A host's firewall may need to let Arena Evolved through.";
		}
		return "Searching your network... Games hosted on it show here.";
	}
	if (screen.count && !screen.shown_count)
		return "No games match this filter. {LT} / {RT} change it.";
	if (!config_boolean("network.online"))
		return "Internet play is off (network.online): turn it on to find and join the internet's games.";
#ifdef HALO_GAME_BROWSER
	if (screen.list_status == BROWSER_LIST_WAITING)
		return "Asking the game list...";
	if (screen.list_status == BROWSER_LIST_FAILED)
		return "The game list did not answer. {X} asks again.";
	if (screen.list_status == BROWSER_LIST_NONE)
		return "No game list is set (network.browser_url).";
#endif
	return "No games listed right now. Host one from CREATE GAME, or join a friend's with an invite ({Y}).";
}

static void draw_empty(float x, float y, float width)
{
	char lines[3][160];
	float body = ae_size_minor(&screen.flow.density) * 1.1f, line = u(30.0f);
	char expanded[240];
	int count = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(empty_text(), expanded, sizeof(expanded)), width - u(40.0f), lines, 3),
		index;

	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, x + u(20.0f), y + u(24.0f) + line * (float)index, AE_ALIGN_LEFT,
			AE_COLOR_MUTED, lines[index]);
	/* (a host's address known: its own screen) */
	if (screen.kind == AE_BROWSER_LAN)
		ae_draw_text(AE_FONT_BODY, body, x + u(20.0f), y + u(24.0f) + line * (float)count, AE_ALIGN_LEFT, AE_COLOR_MUTED,
			ae_help_expand("Know the host's address? {Y} joins by address.", expanded, sizeof(expanded)));
}

static void draw_card(float x, float y, float width, float bottom)
{
	struct game const *game = focused_game();
	float text = ae_size_text(&screen.flow.density), minor = ae_size_minor(&screen.flow.density);
	float card_height = width * CARD_RATIO, line = u(28.0f);
	char lines[4][160], facts[128];
	int count, index;

	if (screen.joining)
	{
		char joining[96], leave[64];

		snprintf(joining, sizeof(joining), "JOINING %s...", screen.joining_name);
		ae_draw_text(AE_FONT_ROW, text, x, y, AE_ALIGN_LEFT, AE_COLOR_ACCENT, joining);
		ae_draw_text(AE_FONT_BODY, minor, x, y + u(44.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			ae_help_expand("{B} leaves the join.", leave, sizeof(leave)));
		return;
	}
	if (!game)
		return;
	/* (the map's card: its name where a picture will be) */
	ae_draw_rect(x, y, width, card_height, u(2.0f), AE_COLOR_WELL);
	ae_draw_outline(x, y, width, card_height, u(2.0f), u(1.0f), AE_COLOR_RULE);
	ae_draw_text_tracked(AE_FONT_ROW, text * 1.2f, 0.06f, x + width * 0.5f, cap_top(AE_FONT_ROW, text * 1.2f,
		y + card_height * 0.5f), AE_ALIGN_CENTER, AE_COLOR_MUTED, game->map);
	y += card_height + u(PANEL_GAP_U);
	ae_draw_text(AE_FONT_ROW, text * NAME_SCALE, x, y, AE_ALIGN_LEFT, AE_COLOR_TITLE, game->name);
	y += ae_draw_cap_height(AE_FONT_ROW, text * NAME_SCALE) + u(PANEL_GAP_U);
	/* (a gametype the host has not set yet: left out) */
	if (strcmp(game->type, "-"))
		snprintf(facts, sizeof(facts), "%s \xE2\x80\xA2 %s \xE2\x80\xA2 %s", game->map, game->type, game->players);
	else
		snprintf(facts, sizeof(facts), "%s \xE2\x80\xA2 %s", game->map, game->players);
	ae_draw_text(AE_FONT_BODY, minor * 1.05f, x, y, AE_ALIGN_LEFT, AE_COLOR_ACCENT, facts);
	y += u(34.0f);
	count = ae_wrap_text(AE_FONT_BODY, minor, game->state, width, lines, 3);
	for (index = 0; index < count; index++, y += line)
		ae_draw_text(AE_FONT_BODY, minor, x, y, AE_ALIGN_LEFT, game->blocked ? AE_COLOR_WARNING : AE_COLOR_TEXT,
			lines[index]);
	if (game->host[0])
	{
		ae_draw_text(AE_FONT_BODY, minor * 0.9f, x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, game->host);
		y += line;
	}
	y += u(8.0f);
	if (y + u(60.0f) > bottom)
		return;
	y += ae_widget_group(&screen.flow.density, x, y, width, "PLAYERS");
	if (!game->roster_count)
	{
		ae_draw_text(AE_FONT_BODY, minor, x, y + u(6.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, game->players_count ?
			"Their names show once you join." : "Nobody yet.");
		return;
	}
	{
		char names[ROSTER_MAXIMUM * 34];
		int length = 0;

		names[0] = 0;
		for (index = 0; index < game->roster_count; index++)
			length += snprintf(names + length, sizeof(names) - (size_t)length, "%s%s", index ? " \xE2\x80\xA2 " : "",
				game->roster[index]);
		count = ae_wrap_text(AE_FONT_BODY, minor, names, width, lines, 4);
		for (index = 0; index < count && y + u(6.0f) + line * (float)index < bottom; index++)
			ae_draw_text(AE_FONT_BODY, minor, x, y + u(6.0f) + line * (float)index, AE_ALIGN_LEFT, AE_COLOR_TEXT,
				lines[index]);
	}
}

static void browser_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &screen.flow;
	struct item_context context;
	char crumb[64];
	float top, bottom, list_width, panel_x;

	(void)instance;
	ae_flow_full_density(flow);
	if (screen.kind == AE_BROWSER_INTERNET && !config_boolean("network.online"))
		snprintf(crumb, sizeof(crumb), "INTERNET PLAY IS OFF");
	else
		snprintf(crumb, sizeof(crumb), "%d GAME%s%s", screen.count, screen.count == 1 ? "" : "S",
			screen.kind == AE_BROWSER_LAN ? " ON YOUR NETWORK" : " \xE2\x80\xA2 REFRESHED IN PLACE");
	ae_flow_title(flow, screen.kind == AE_BROWSER_LAN ? "JOIN LAN GAMES" : "SERVER BROWSER", crumb);
	top = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	if (screen.kind == AE_BROWSER_INTERNET)
		top = draw_filters(top);
	bottom = ae_flow_content_bottom(flow) - u(FOOTER_ROOM_U) * 0.5f;
	list_width = flow->frame.rect.width * LIST_WIDTH;
	panel_x = flow->frame.rect.x + flow->frame.rect.width * PANEL_X;
	draw_columns(ae_flow_left_x(flow), top, list_width);
	top += u(HEADER_ROW_U);
	context.density = &flow->density;
	context.name_width = list_width * (1.0f - PING_COLUMN - PLAYERS_COLUMN - TYPE_COLUMN - MAP_COLUMN) - u(24.0f);
	ae_widget_list(&flow->density, &screen.list, ae_flow_left_x(flow), top, list_width, bottom - top, list_item,
		&context, HIT_LIST);
	if (!screen.shown_count)
		draw_empty(ae_flow_left_x(flow), top, list_width);
	draw_card(panel_x, ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y), ae_flow_right_x(flow) - panel_x, bottom);
	flow->prompt_count = 0;
	if (screen.joining)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Leave the join", AE_ACTION_BACK);
	else
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Join", AE_ACTION_ACCEPT);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, screen.kind == AE_BROWSER_LAN ?
			"Search again" : "Refresh", AE_ACTION_X);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Join by address", AE_ACTION_Y);
		if (screen.kind == AE_BROWSER_INTERNET)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Join with invite", AE_ACTION_TAB_PREVIOUS);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	}
	ae_flow_draw_prompts(flow);
}

/* ---------- input */

static void browser_update(struct ae_screen *instance)
{
	unsigned long now = system_milliseconds();

	(void)instance;
	if (screen.joining)
	{
		join_update();
		return;
	}
	/* (LAN: the search lost, by a join left or a lobby left: started again) */
	if (screen.kind == AE_BROWSER_LAN && !ae_lobby_lan_searching() && ae_lobby_join_state(NULL) != 0 &&
		now - screen.search_at >= SEARCH_RETRY)
	{
		lan_search();
	}
	if (now - screen.read_at >= READ_INTERVAL)
		games_read();
}

static int browser_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	if (screen.joining)
	{
		if (event->action == AE_ACTION_BACK)
			join_cancel();
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_ACCEPT:
		join_focused();
		return 1;
	case AE_ACTION_X:
		refresh();
		return 1;
	case AE_ACTION_Y:
		if (ae_screen_join_address_open(screen.kind == AE_BROWSER_LAN ? "JOIN LAN GAMES" : "SERVER BROWSER"))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
		/* (LB: JOIN WITH INVITE, which Y had before JOIN BY ADDRESS) */
		if (screen.kind == AE_BROWSER_INTERNET && ae_screen_join_invite_open("SERVER BROWSER"))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return 1;
	case AE_ACTION_PAGE_UP:
	case AE_ACTION_PAGE_DOWN:
	case AE_ACTION_TAB_NEXT:
		/* (LT / RT, RB: the filter here, not a page) */
		if (screen.kind != AE_BROWSER_INTERNET)
			return 1;
		filter_set(screen.filter + (event->action == AE_ACTION_PAGE_DOWN || event->action == AE_ACTION_TAB_NEXT ?
			1 : -1));
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		return 1;
	default:
		break;
	}
	if (ae_list_view_event(&screen.list, event))
		focus_taken();
	return 1;
}

static void browser_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	short item, focus = screen.list.list.focus;

	if (ae_prompts_pointer(screen.flow.prompts, screen.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK && !screen.joining)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			browser_handle(instance, &event);
		return;
	}
	if (screen.joining)
		return;
	if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_CHIPS && hit.index >= 0 &&
		hit.index < FILTERS)
	{
		filter_set(hit.index);
		ae_sound_request(AE_SOUND_CURSOR, 0);
		return;
	}
	item = ae_list_view_pointer(&screen.list, pointer, HIT_LIST);
	focus_taken();
	/* (a click on the focused game joins it; on another, focuses it) */
	if (item >= 0 && item < screen.shown_count && item == focus)
		join_focused();
}

static void browser_leave(struct ae_screen *instance)
{
	(void)instance;
#ifdef HALO_GAME_BROWSER
	if (screen.kind == AE_BROWSER_INTERNET)
		browser_follow_listings(0);
#endif
	/* (the LAN's search ended; a join that went through keeps its client) */
	if (screen.kind == AE_BROWSER_LAN && ae_lobby_lan_searching())
		ae_lobby_leave();
	platform_log("ae browser: closed");
}

static struct ae_screen_class const browser_class =
{
	.name = "browser", .leave = browser_leave, .handle = browser_handle, .draw = browser_draw,
	.pointer = browser_pointer, .update = browser_update,
};

int ae_screen_browser_available(int kind)
{
#ifdef HALO_GAME_BROWSER
	return kind == AE_BROWSER_INTERNET || kind == AE_BROWSER_LAN;
#else
	return kind == AE_BROWSER_LAN;
#endif
}

int ae_screen_browser_open(int kind)
{
	if (!ae_screen_browser_available(kind))
		return 0;
	memset(&screen, 0, sizeof(screen));
	screen.kind = kind;
	screen.flow.hit_prompts = HIT_PROMPTS;
	screen.opened_at = system_milliseconds();
	ae_list_view_init(&screen.list, 0, 10);
	if (kind == AE_BROWSER_LAN)
		lan_search();
#ifdef HALO_GAME_BROWSER
	else
		browser_follow_listings(1);
#endif
	if (!ae_ui_push(&browser_class, AE_OWNER_ANY, &screen))
	{
		if (kind == AE_BROWSER_LAN && ae_lobby_lan_searching())
			ae_lobby_leave();
		return 0;
	}
	games_read();
	focus_taken();
	platform_log("ae browser: open (%s)", kind == AE_BROWSER_LAN ? "lan games" : "server browser");
	return 1;
}
