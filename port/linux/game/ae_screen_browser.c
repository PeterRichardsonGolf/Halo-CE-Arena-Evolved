/*
AE_SCREEN_BROWSER.C

AE's SERVER BROWSER and LAN GAMES (ae_screen_browser.h; the mockup's board AEBrowser): one screen of two kinds. SERVER BROWSER lists the internet's games
(the game list, ../src/browser.h: browser_get_games, its public listings followed while the screen is open); LAN GAMES
lists the games this PC's network client hears on the LAN (ae_glue_lobby.h: ae_lobby_lan_search). The title and the
games' count top right; the filters between LT and RT (SERVER BROWSER: ALL, NOT FULL, HAS PLAYERS, NO PASSWORD); the
table (GAME, MAP, GAMETYPE, PLAYERS, PING), a game this build can't join dimmed; on the right the focused game's card:
its map, name, map . gametype . players, its state in plain words, its host, its players.

Input (anyone's): up / down move; A picks player 1's profile, then joins the focused game; X refreshes (LAN: searches again); Y opens JOIN BY
ADDRESS (ae_screen_join_address.h); LB / RB step the gametype chips (ALL, SLAYER, CTF, KING, ODDBALL), LT, RT and RS (G on a keyboard) switch EMPTY, FULL and AE GAMES: both lists, kept in the profile; B back. While a
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
#include "ae_join_helper.h"
#include "ae_screen_browser.h"
#include "ae_screen_create_game.h"
#include "ae_screen_join_address.h"
#include "ae_screen_join_invite.h"
#include "ae_server_listing_rules.h"
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
#include "halo_ui_map_list.h"
#ifdef HALO_GAME_BROWSER
#include "../src/ae_lan_hint.h"
#include "../src/browser.h"
#include "../src/p2p.h"
#endif

void platform_log(char const *format, ...);
int config_boolean(char const *name);
int config_write(char const *name, char const *value);
int config_text(char const *name, char *text, size_t size);

enum
{
	BROWSER_HIT = 0x7800,
	HIT_LIST = BROWSER_HIT + 1, HIT_CHIPS, HIT_PROMPTS, HIT_SWITCH_FIRST,
	/* (0.7.8: 128, was 64: the public masters' and the brokers' games filled 64 before the AE master's were read) */
	GAMES_MAXIMUM = 128,
	/* (the public listings read at most: the lobby's own table, p2p_lobby.c MAXIMUM_GAMES) */
	LISTINGS_MAXIMUM = 256,
	ROSTER_MAXIMUM = 16,
	/* (ms) the list read again this often; a lost LAN search started again after this long */
	READ_INTERVAL = 500,
	SEARCH_RETRY = 2000,
	/* (ms) with nothing heard on the LAN this long, the firewall's hint */
	LAN_HINT_AFTER = 8000
};
/* (0.7.5) LB / RB step the gametype chips; LT, RT and RS (G on a keyboard, Page Up / Page Down for LT / RT) switch EMPTY,
FULL and AE GAMES. Each is kept in the player's profile (display.server_*), as the game was left */
enum { FILTER_ALL, FILTER_SLAYER, FILTER_CTF, FILTER_KING, FILTER_ODDBALL, FILTERS };
enum { SWITCH_EMPTY, SWITCH_FULL, SWITCH_AE, SWITCHES };

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

static char const *const filter_names[FILTERS] = { "ALL", "SLAYER", "CTF", "KING", "ODDBALL" };
/* (the engines each gametype chip keeps: type_name's order) */
static short const filter_engines[FILTERS] = { 0, 2, 1, 4, 3 };
static char const *const filter_keys[FILTERS] = { "all", "slayer", "ctf", "king", "oddball" };
static char const *const switch_names[SWITCHES] = { "EMPTY", "FULL", "AE GAMES" };
static char const *const switch_keys[SWITCHES] = { "display.server_empty", "display.server_full", "display.server_ae_games" };

/* a game as the table and the card show it, from either list */
struct game
{
	char name[48], map[64], type[24], players[32], ping[16];
	char state[112];              /* the card's line: why it can't be joined, or what joining it does */
	char host[64];                /* the card's host line ("" none) */
	char roster[ROSTER_MAXIMUM][32];
	short roster_count, players_count, maximum_players;
	int blocked;                  /* A refuses it, with state as the reason */
	int locked;
	short engine;                 /* the gametype's engine (type_name's order), the chips' */
	int ae;                       /* an Arena Evolved host (AE GAMES) */
	int cause;                    /* (0.7.5) the join helper's cause when blocked (ae_join_helper_rules.h), AE_JH_GENERIC: the state's words */
	char map_file[64];            /* the host's map name (a Workshop map's <name>@ws<item>) */
	char invite[80];              /* SERVER BROWSER's join */
	char join_word[16];           /* (0.7.7) WHO CAN JOIN NOW, the badge's word ("" not said) */
	int join_mixed;               /* (0.7.7) the playlist holds other kinds of game too */
	short bots;                   /* (0.7.7) the bots the server said it holds (below 0: not said); players_count are the people */
	char address[48];             /* (0.7.7) the AE master's address for it ("host:port"; "" none): joined straight to */
	short lan_index;              /* LAN GAMES' join */
	unsigned char lan_nonce[8];   /* LAN host identity: the discovery slot may be reused */
};

static struct
{
	struct ae_flow_screen flow;
	int kind, filter;
	int on[SWITCHES];             /* EMPTY and FULL games shown, only AE games shown */
	struct game games[GAMES_MAXIMUM];
	unsigned char listing_hash[GAMES_MAXIMUM][16];   /* (0.7.7) the public listing rows' key hashes */
	unsigned char listing_hash_valid[GAMES_MAXIMUM];
	short count;
	short shown[GAMES_MAXIMUM], shown_count;
	struct ae_list_view list;
	struct ae_chip chips[FILTERS + SWITCHES];
	char chip_labels[FILTERS][32];
	unsigned long read_at, search_at, opened_at;
	int list_status;              /* SERVER BROWSER: browser_list_status's */
	/* a join under way: its game's name and kind of join */
	int joining;
	short click_item;                   /* (0.7.6) the row of the last click (double-click joins) */
	unsigned long click_at;
	char joining_name[48];
	short pending_lan_index;
	unsigned char pending_lan_nonce[8];
	char pending_invite[80];
	char pending_address[48];
	char focus_name[48];          /* the focused game's, kept across reads */
	char focus_key[256];          /* (0.7.8) the focused game's identity (name, invite, address, LAN nonce): kept across reads */
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

short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);

/* a Workshop map's title (<name>@ws<item>: the listing's 32 characters may have cut its name): the map of the same
Workshop item on this machine, by its row's name (its Workshop title); none when this machine does not have it */
static boolean workshop_caption(char const *map, wchar_t *wide, long count)
{
	char const *const *names = NULL;
	char const *item = map ? strstr(map, "@ws") : NULL;
	short index, maps;

	if (!item || !item[3])
		return FALSE;
	maps = ui_widget_port_multiplayer_maps(&names, NULL);
	for (index = 0; names && index < maps; index++)
	{
		char const *own = names[index] ? strstr(names[index], "@ws") : NULL;

		if (own && !_stricmp(own, item))
		{
			wide[0] = 0;
			ui_map_list_caption(names[index], wide, count);
			return (boolean)(wide[0] != 0);
		}
	}
	return FALSE;
}

/* a game's map as players name it: a Workshop map's title when this machine has it, a Halo PC map's name from the map
list, else the stock maps' ("BLOOD GULCH"), else its file's in capitals */
static void map_name(char const *map, char *text, size_t size)
{
	wchar_t wide[64];
	short index;

	wide[0] = 0;
	if (!workshop_caption(map, wide, NUMBEROF(wide)) &&
		(server_browser_map_family(map, wide, (short)NUMBEROF(wide)) == _map_family_xbox || !wide[0]))
	{
		ae_scoreboard_header_map_name(map, wide, NUMBEROF(wide));
	}
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
	{
		game->blocked = 1;
		game->cause = map && strstr(map, "@ws") ? AE_JH_WORKSHOP : AE_JH_MAP;
		snprintf(game->map_file, sizeof(game->map_file), "%s", map ? map : "");
	}
}

/* a host this build does not join: why, as the game lists word it */
static int version_state(struct game *game, int join_state, unsigned int version)
{
	char const *format = halo_port_join_reason_format(join_state, version);

	if (!format)
		return 0;
	snprintf(game->state, sizeof(game->state), format, version);
	game->blocked = 1;
	game->cause = version < (unsigned int)HALO_PORT_NETWORK_VERSION ? AE_JH_HOST_OLDER : AE_JH_UPDATE_VERSION;
	return 1;
}

static void players_text(struct game *game)
{
	ae_browser_players_text(game->players, sizeof(game->players), game->players_count, game->maximum_players, game->bots);
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
		game->cause = AE_JH_CLOSED;
	}
	else if (ae_browser_seat_flags(game->players_count, game->maximum_players, game->bots) & 1)
	{
		snprintf(game->state, sizeof(game->state), "Full.");
		game->blocked = 1;
		game->cause = AE_JH_FULL;
	}
	else
		snprintf(game->state, sizeof(game->state), game->locked ? "Ready to join. It has a password." :
			"Ready to join.");
}

#ifdef HALO_GAME_BROWSER
/* (0.7.7) the public listings (master servers: AE's, OpenCE's; the brokers): a row each, as the PC menus' browser
has them, before the game list's own games; one key hash (the invite's first half) is one row */
static int invite_hex_matches(unsigned char const *key_hash, char const *invite)
{
	int index;

	for (index = 0; index < P2P_LISTING_KEY_HASH_SIZE; index++)
	{
		unsigned int byte;

		if (!invite[2 * index] || !invite[2 * index + 1] || sscanf(invite + 2 * index, "%2x", &byte) != 1 ||
			byte != key_hash[index])
			return 0;
	}
	return 1;
}

static struct game *listing_row_for_invite(char const *invite)
{
	short index;

	for (index = 0; index < screen.count; index++)
	{
		if (screen.listing_hash_valid[index] && invite_hex_matches(screen.listing_hash[index], invite))
			return &screen.games[index];
	}
	return NULL;
}

/* whether a public listing is an Arena Evolved game (the row's ae, below) */
static int listing_is_ae(struct p2p_listing const *source)
{
	return source->ae_build || source->ae_join_now[0] || !strncmp(source->gametype, "AE ", 3);
}

static void listings_read(void)
{
	static struct p2p_listing all[LISTINGS_MAXIMUM], listings[LISTINGS_MAXIMUM];
	int read = p2p_lobby_games(all, LISTINGS_MAXIMUM), count = 0, pass, index, player;

	/* (0.7.8) Arena Evolved's games first, then the others, each in the order heard: a full list never leaves out the
	AE master's games, read after the public master's) */
	for (pass = 0; pass < 2; pass++)
	{
		for (index = 0; index < read; index++)
		{
			if (listing_is_ae(&all[index]) == (pass == 0))
				listings[count++] = all[index];
		}
	}

	memset(screen.listing_hash_valid, 0, sizeof(screen.listing_hash_valid));
	for (index = 0; index < count && screen.count < GAMES_MAXIMUM; index++)
	{
		struct p2p_listing const *source = &listings[index];
		struct game *game = &screen.games[screen.count];
		int join_state = HALO_PORT_JOIN_OK;

		if (browser_map_is_campaign(source->map) || !source->name[0])
			continue;
		memset(game, 0, sizeof(*game));
		snprintf(game->name, sizeof(game->name), "%s", source->name);
		/* (0.7.8: the tag an AE-only game's name carries for other builds' browsers) */
		ae_listing_name_untag(game->name);
		map_name(source->map, game->map, sizeof(game->map));
		type_name(source->engine_type, source->has_teams, source->kind, game->type, sizeof(game->type));
		game->players_count = source->player_count;
		game->maximum_players = source->maximum_player_count;
		game->bots = source->ae_bots_known ? source->ae_bots : -1;
		game->engine = source->engine_type;
		game->ae = source->ae_build || source->ae_join_now[0] ? 1 : !strncmp(source->gametype, "AE ", 3);
		snprintf(game->join_word, sizeof(game->join_word), "%s", source->ae_join_now[0] ? ae_browser_badge_word(source->ae_join_now) : "");
		game->join_mixed = source->ae_join_mixed;
		players_text(game);
		if (source->ping < 0)
			snprintf(game->ping, sizeof(game->ping), "-");
		else
			snprintf(game->ping, sizeof(game->ping), "%d", source->ping);
		game->locked = source->locked;
		snprintf(game->invite, sizeof(game->invite), "%s", source->invite);
		snprintf(game->address, sizeof(game->address), "%s", source->ae_address);
		game->lan_index = -1;
		if (source->network_version)
			join_state = halo_port_advertised_join_state(source->network_version, HALO_PORT_ADVERTISED_DISTRIBUTED_FLAG,
				(unsigned int)delta_legacy_minimum(), (unsigned int)delta_legacy_maximum());
		if (!source->invite[0])
		{
			snprintf(game->state, sizeof(game->state), "It has a password: join it with its invite.");
			game->blocked = 1;
			game->cause = AE_JH_CLOSED;
		}
		else if (source->failed)
		{
			snprintf(game->state, sizeof(game->state), "Joining it did not work this time.");
			game->blocked = 1;
			game->cause = AE_JH_CLOSED;
		}
		else if (!version_state(game, join_state, source->network_version))
			map_state(game, source->map);
		ready_state(game, source->open);
		snprintf(game->host, sizeof(game->host), "%s%s%s", source->dedicated ? "Dedicated server" : "Hosted by a player",
			source->region[0] ? " \xE2\x80\xA2 " : "", source->region);
		(void)player;
		memcpy(screen.listing_hash[screen.count], source->key_hash, P2P_LISTING_KEY_HASH_SIZE);
		screen.listing_hash_valid[screen.count] = 1;
		screen.count++;
	}
}

static void internet_read(void)
{
	static struct browser_game listed[BROWSER_MAXIMUM_GAMES];
	int count = browser_get_games(listed, BROWSER_MAXIMUM_GAMES), index, player;

	screen.list_status = browser_list_status();
	screen.count = 0;
	listings_read();
	for (index = 0; index < count && screen.count < GAMES_MAXIMUM; index++)
	{
		struct browser_game const *source = &listed[index];
		struct game *game = &screen.games[screen.count];

		/* (0.7.7) a game the public listings (the masters, the brokers) already show is that row) */
		if (listing_row_for_invite(source->invite))
			continue;
		int join_state = halo_port_advertised_join_state(source->version, HALO_PORT_ADVERTISED_DISTRIBUTED_FLAG,
			(unsigned int)delta_legacy_minimum(), (unsigned int)delta_legacy_maximum());

		/* (co-op games: CAMPAIGN's, never listed here) */
		if (browser_map_is_campaign(source->map))
			continue;
		memset(game, 0, sizeof(*game));
		ae_lobby_utf16_to_utf8(game->name, sizeof(game->name), source->name, BROWSER_NAME_LENGTH);
		ae_listing_name_untag(game->name);
		map_name(source->map, game->map, sizeof(game->map));
		type_name(source->engine, source->teams, source->kind, game->type, sizeof(game->type));
		game->players_count = source->players;
		game->maximum_players = source->maximum_players;
		game->engine = source->engine;
		game->ae = source->ae;
		players_text(game);
		if (source->ping < 0)
			snprintf(game->ping, sizeof(game->ping), "-");
		else
			snprintf(game->ping, sizeof(game->ping), "%s%d", source->ping_estimated ? "~" : "", source->ping);
		game->locked = source->locked;
		snprintf(game->invite, sizeof(game->invite), "%s", source->invite);
		snprintf(game->address, sizeof(game->address), "%s", source->ae_address);
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
		game->engine = source->engine;
		game->ae = source->ae;
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

/* whether a game passes the switches (EMPTY, FULL, AE GAMES) */
static int switches_keep(struct game const *game)
{
	int flags = ae_browser_seat_flags(game->players_count, game->maximum_players, game->bots);

	if (!screen.on[SWITCH_EMPTY] && (flags & 2))
		return 0;
	if (!screen.on[SWITCH_FULL] && (flags & 1))
		return 0;
	if (screen.on[SWITCH_AE] && !game->ae)
		return 0;
	return 1;
}

/* whether a game is of a gametype chip */
static int filter_keeps(struct game const *game, int filter)
{
	return filter == FILTER_ALL || game->engine == filter_engines[filter];
}

/* a chip's count: the games the switches keep that are of it */
static short filter_count(int filter)
{
	short count = 0, index;

	for (index = 0; index < screen.count; index++)
		count += (short)(switches_keep(&screen.games[index]) && filter_keeps(&screen.games[index], filter));
	return count;
}

/* (0.7.8) a game's identity across reads: its name, invite, address and LAN nonce */
static void game_key(struct game const *game, char *key, size_t size)
{
	snprintf(key, size, "%s\x1f%s\x1f%s\x1f%02x%02x%02x%02x%02x%02x%02x%02x", game->name, game->invite, game->address,
		game->lan_nonce[0], game->lan_nonce[1], game->lan_nonce[2], game->lan_nonce[3], game->lan_nonce[4],
		game->lan_nonce[5], game->lan_nonce[6], game->lan_nonce[7]);
}

/* (0.7.8) whether one game of those read has this name */
static int name_unique(char const *name)
{
	short index, found = 0;

	for (index = 0; index < screen.count && found < 2; index++)
		found += !strcmp(screen.games[index].name, name);
	return found == 1;
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
		char key[256];

		if (!switches_keep(&screen.games[index]) || !filter_keeps(&screen.games[index], screen.filter))
			continue;
		/* (0.7.8: by the game's identity; by its name only when one game has it: games of the same name pulled a
		pad's focus back up to the first of them at every read) */
		game_key(&screen.games[index], key, sizeof(key));
		if (screen.focus_key[0] && !strcmp(key, screen.focus_key))
			focus = screen.shown_count;
		else if (focus < 0 && screen.focus_name[0] && !strcmp(screen.games[index].name, screen.focus_name))
			focus = name_unique(screen.focus_name) ? screen.shown_count : (short)-1;
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
	if (game)
		game_key(game, screen.focus_key, sizeof(screen.focus_key));
	else
		screen.focus_key[0] = 0;
}

static void filters_saved(void)
{
	int index;

	config_write("display.server_filter", filter_keys[screen.filter]);
	for (index = 0; index < SWITCHES; index++)
		config_write(switch_keys[index], screen.on[index] ? "true" : "false");
}

static void filter_set(int filter)
{
	screen.filter = (filter + FILTERS) % FILTERS;
	screen.focus_name[0] = 0;
	screen.focus_key[0] = 0;
	games_read();
	ae_list_set_focus(&screen.list.list, 0);
	focus_taken();
	filters_saved();
	platform_log("ae browser: filter %s: %d of %d", filter_names[screen.filter], screen.shown_count, screen.count);
}

/* a switch turned over (the games read again, the focus kept on its game where it still is) */
static void switch_set(int which, int on)
{
	screen.on[which] = on != 0;
	games_read();
	filters_saved();
	platform_log("ae browser: %s %s: %d of %d", switch_names[which], screen.on[which] ? "on" : "off", screen.shown_count,
		screen.count);
}

/* the filters as the profile kept them */
static void filters_loaded(void)
{
	char text[24];
	int index;

	screen.filter = FILTER_ALL;
	if (config_text("display.server_filter", text, sizeof(text)))
	{
		for (index = 0; index < FILTERS; index++)
		{
			if (!strcmp(text, filter_keys[index]))
				screen.filter = index;
		}
	}
	for (index = 0; index < SWITCHES; index++)
		screen.on[index] = config_boolean(switch_keys[index]) != 0;
}

/* ---------- joining */

static void lan_search(void)
{
	struct ae_result result = ae_lobby_lan_search();

	screen.search_at = system_milliseconds();
	if (!result.ok)
		platform_log("ae browser: lan search failed: %s", result.reason);
}

static void join_profile_done(short player, int picked, void *context);

/* JOIN AGAIN / RETRY: the same game, the same way (player 1's profile is already picked) */
static void join_again(void *context)
{
	(void)context;
	join_profile_done(0, 1, NULL);
}

/* (0.7.5) a join that did not happen says why in the join helper's dialog (ae_join_helper.h) */
static void join_failed(const char *why)
{
	screen.joining = 0;
	platform_log("ae browser: join failed: %s", why);
	if (!ae_join_helper_failed(ae_lobby_join_failure_reason(), why, &screen.flow.density, join_again, NULL))
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
	else if (ae_browser_joins_direct(screen.pending_address))
	{
		/* (an address the AE master served: JOIN BY ADDRESS's way, no brokers) */
		char host[48];
		unsigned short port = 0;

		ae_listing_address_split(screen.pending_address, host, sizeof(host), &port);
		platform_log("ae browser: joining %s directly", screen.pending_address);
		result = ae_lobby_join_host(host, port);
	}
	else
		result = ae_lobby_join_address(screen.pending_invite);
	if (!result.ok)
	{
		join_failed(result.reason);
		return;
	}
	join_started(screen.joining_name);
}

static void join_focused(void);

/* the helper's JOIN AGAIN on a game that was blocked: the list read again now (a map Steam has since downloaded), then
the focused game joined as A joins it */
static void join_focused_again(void *context)
{
	(void)context;
	games_read();
	join_focused();
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
		struct ae_join_helper_request request;

		memset(&request, 0, sizeof(request));
		request.cause = game->cause;
		request.detail = game->state;
		request.map = game->map_file;
		request.item_title = game->map;
		request.density = screen.flow.density;
		/* (a Workshop map JOIN AGAIN looks for it again: the list is read again at once) */
		request.again = game->cause == AE_JH_WORKSHOP || game->cause == AE_JH_FULL || game->cause == AE_JH_CLOSED ?
			join_focused_again : NULL;
		if (!ae_join_helper_show(&request))
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
	snprintf(screen.pending_address, sizeof(screen.pending_address), "%s", screen.kind == AE_BROWSER_LAN ? "" : game->address);
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
	/* (WHO CAN JOIN NOW: a small outlined tag at the name column's end, the words ANYONE / AE / AE+MCC) */
	if (game->join_word[0])
	{
		float minor = ae_size_minor(list->density) * 0.8f, tag_width = ae_draw_text_tracked_width(AE_FONT_BODY, minor, 0.1f, game->join_word) + u(16.0f);
		float tag_right = x + list->name_width + u(12.0f) + tag_width + pad;

		ae_draw_outline(tag_right - tag_width, center - u(13.0f), tag_width, u(26.0f), u(2.0f), u(1.0f), on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT);
		ae_draw_text_tracked(AE_FONT_BODY, minor, 0.1f, tag_right - tag_width * 0.5f, cap_top(AE_FONT_BODY, minor, center),
			AE_ALIGN_CENTER, on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT, game->join_word);
	}
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

/* one step of the chip row: a chip (a gametype's or a switch), with a button mark before it or after it */
struct chip_unit
{
	struct ae_chip chip;
	int mark_before, mark_after;      /* AE_BUTTON_* / AE_MARK_*; -1 none */
	short hit;
	int group_start;                  /* the switches' first: more space before it */
};

/* the chip row (both lists): LB, the gametype chips, RB; then LT EMPTY, RT FULL and RS AE GAMES (switches). A chip
never parts from its mark across a line end; returns the y under the row */
static float draw_filters(float y)
{
	static int const switch_marks[SWITCHES] = { AE_BUTTON_LEFT_TRIGGER, AE_BUTTON_RIGHT_TRIGGER, AE_MARK_RIGHT_STICK };
	struct ae_density const *density = &screen.flow.density;
	float glyph = ae_size_glyph(density), x0 = ae_flow_left_x(&screen.flow), room = ae_flow_right_x(&screen.flow) - x0;
	float gap = u(13.0f), mark_gap = u(GLYPH_GAP_U), group_gap = u(32.0f), x = 0.0f, line = y, height = 0.0f;
	struct chip_unit units[FILTERS + SWITCHES];
	int count = 0, index;

	for (index = 0; index < FILTERS; index++)
	{
		struct chip_unit *unit = &units[count++];

		snprintf(screen.chip_labels[index], sizeof(screen.chip_labels[index]), "%s", filter_names[index]);
		unit->chip.label = screen.chip_labels[index];
		unit->chip.count = filter_count(index);
		unit->chip.flags = index == screen.filter ? AE_CHIP_ON : 0;
		unit->mark_before = index == 0 ? AE_BUTTON_LEFT_SHOULDER : -1;
		unit->mark_after = index == FILTERS - 1 ? AE_BUTTON_RIGHT_SHOULDER : -1;
		unit->hit = (short)(HIT_SWITCH_FIRST + index);
		unit->group_start = 0;
	}
	for (index = 0; index < SWITCHES; index++)
	{
		struct chip_unit *unit = &units[count++];

		unit->chip.label = switch_names[index];
		unit->chip.count = -1;
		unit->chip.flags = AE_CHIP_CHECK | (screen.on[index] ? AE_CHIP_ON : 0);
		unit->mark_before = switch_marks[index];
		unit->mark_after = -1;
		unit->hit = (short)(HIT_SWITCH_FIRST + FILTERS + index);
		unit->group_start = index == 0;
	}
	for (index = 0; index < count; index++)
	{
		struct chip_unit const *unit = &units[index];
		struct ae_rect rect;
		float before = unit->mark_before >= 0 ?
			ae_widget_button_mark(density, unit->mark_before, 0.0f, 0.0f, glyph, 0, 0) + mark_gap : 0.0f;
		float after = unit->mark_after >= 0 ?
			ae_widget_button_mark(density, unit->mark_after, 0.0f, 0.0f, glyph, 0, 0) + mark_gap : 0.0f;
		float width;

		ae_chips_layout(density, 100000.0f, &unit->chip, 1, &rect);
		width = before + rect.width + after;
		if (!index)
			height = rect.height;
		if (x > 0.0f && x + width > room)
		{
			x = 0.0f;
			line += height + gap;
		}
		else if (unit->group_start)
			x += group_gap - gap;
		if (unit->mark_before >= 0)
			ae_widget_button_mark(density, unit->mark_before, x0 + x, line + height * 0.5f, glyph, 0, 1);
		ae_widget_chips(density, x0 + x + before, line, rect.width + 1.0f, &unit->chip, 1, unit->hit);
		if (unit->mark_after >= 0)
			ae_widget_button_mark(density, unit->mark_after, x0 + x + before + rect.width + mark_gap,
				line + height * 0.5f, glyph, 0, 1);
		x += width + gap;
	}
	return line + height + u(18.0f);
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
		return "No games match the chips. {LB} / {RB} pick a gametype; {LT}, {RT} and {RS} switch EMPTY, FULL and AE GAMES.";
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
	if (game->join_word[0])
	{
		/* (WHO CAN JOIN NOW, one tag; a playlist of other kinds too: a muted "mixed") */
		float tag_minor = minor * 0.9f, tag_width = ae_draw_text_tracked_width(AE_FONT_BODY, tag_minor, 0.1f, game->join_word) + u(16.0f);

		ae_draw_text_tracked(AE_FONT_BODY, tag_minor, 0.12f, x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, "WHO CAN JOIN NOW");
		y += u(30.0f);
		ae_draw_outline(x, y - u(4.0f), tag_width, u(28.0f), u(2.0f), u(1.0f), AE_COLOR_ACCENT);
		ae_draw_text_tracked(AE_FONT_BODY, tag_minor, 0.1f, x + tag_width * 0.5f, cap_top(AE_FONT_BODY, tag_minor, y + u(10.0f)),
			AE_ALIGN_CENTER, AE_COLOR_ACCENT, game->join_word);
		if (game->join_mixed)
			ae_draw_text(AE_FONT_BODY, tag_minor, x + tag_width + u(14.0f), cap_top(AE_FONT_BODY, tag_minor, y + u(10.0f)),
				AE_ALIGN_LEFT, AE_COLOR_MUTED, "mixed playlist");
		y += u(40.0f);
	}
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
	else if (screen.shown_count != screen.count)
		snprintf(crumb, sizeof(crumb), "%d OF %d GAMES", screen.shown_count, screen.count);
	else
		snprintf(crumb, sizeof(crumb), "%d GAME%s%s", screen.count, screen.count == 1 ? "" : "S",
			screen.kind == AE_BROWSER_LAN ? " ON YOUR NETWORK" : " \xE2\x80\xA2 REFRESHED IN PLACE");
	ae_flow_title(flow, screen.kind == AE_BROWSER_LAN ? "JOIN LAN GAMES" : "SERVER BROWSER", crumb);
	top = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	top = draw_filters(top);
	bottom = ae_flow_content_bottom(flow) - u(FOOTER_ROOM_U) * 0.5f;
	list_width = flow->frame.rect.width * LIST_WIDTH;
	panel_x = flow->frame.rect.x + flow->frame.rect.width * PANEL_X;
	draw_columns(ae_flow_left_x(flow), top, list_width);
	top += u(HEADER_ROW_U);
	context.density = &flow->density;
	context.name_width = list_width * (1.0f - PING_COLUMN - PLAYERS_COLUMN - TYPE_COLUMN - MAP_COLUMN) - u(24.0f) - u(110.0f);
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
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Address or invite", AE_ACTION_Y);
		flow->prompts[flow->prompt_count] = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Gametype", AE_ACTION_TAB_PREVIOUS);
		flow->prompts[flow->prompt_count++].pair = 1;
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_LEFT_TRIGGER, "Empty", AE_ACTION_PAGE_UP);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_RIGHT_TRIGGER, "Full", AE_ACTION_PAGE_DOWN);
		flow->prompts[flow->prompt_count] = ae_flow_prompt(AE_MARK_RIGHT_STICK, "AE games", AE_ACTION_THUMB);
		flow->prompts[flow->prompt_count++].key = "G";
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
	case AE_ACTION_TAB_NEXT:
		/* (LB / RB: the gametype chips; JOIN WITH INVITE is in JOIN BY ADDRESS) */
		filter_set(screen.filter + (event->action == AE_ACTION_TAB_NEXT ? 1 : -1));
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		return 1;
	case AE_ACTION_PAGE_UP:
	case AE_ACTION_PAGE_DOWN:
	case AE_ACTION_THUMB:
	{
		/* (LT: EMPTY, RT: FULL, RS: AE GAMES) */
		int which = event->action == AE_ACTION_PAGE_UP ? SWITCH_EMPTY :
			event->action == AE_ACTION_PAGE_DOWN ? SWITCH_FULL : SWITCH_AE;

		switch_set(which, !screen.on[which]);
		ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		return 1;
	}
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
	if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id >= HIT_SWITCH_FIRST &&
		hit.id < HIT_SWITCH_FIRST + FILTERS + SWITCHES)
	{
		int chip = hit.id - HIT_SWITCH_FIRST;

		if (chip < FILTERS)
			filter_set(chip);
		else
			switch_set(chip - FILTERS, !screen.on[chip - FILTERS]);
		ae_sound_request(AE_SOUND_CURSOR, 0);
		return;
	}
	item = ae_list_view_pointer(&screen.list, pointer, HIT_LIST);
	focus_taken();
	/* (0.7.6: the pointer's hover already focuses a row, so a lone click joined by accident: one click focuses, the
	second on the same row within 0.5 s (or a touch tap) joins; Enter / A joins as ever) */
	if (item >= 0 && item < screen.shown_count && item == focus && pointer->left_clicks)
	{
		unsigned long now = ae_motion_now();

		if (pointer->touch || pointer->left_clicks > 1 || (item == screen.click_item && now - screen.click_at < 500ul))
		{
			screen.click_item = -1;
			join_focused();
		}
		else
		{
			screen.click_item = item;
			screen.click_at = now;
		}
	}
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
	screen.click_item = -1;
	screen.kind = kind;
	screen.flow.hit_prompts = HIT_PROMPTS;
	screen.opened_at = system_milliseconds();
	filters_loaded();
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

void ae_screen_browser_close(void)
{
	(void)ae_ui_remove(&screen);
}
