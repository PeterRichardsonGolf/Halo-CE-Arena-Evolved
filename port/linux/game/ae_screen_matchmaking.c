/*
AE_SCREEN_MATCHMAKING.C

See ae_screen_matchmaking.h. The rows are AE's flow (ae_setup_flow.h): owner 11-11, one row a playlist (CASUAL, BTB,
COMP...): the focused one is the playlist, A (or START, or a click) joins it. The playlist's card under them; MY PARTY and LAST GAME on the right are the main
menu's (ae_screen_main_menu_party_draw). Servers' players come from the public listings the server browser follows
(browser_follow_listings while this screen is up); a server is matched by its address, else by its port (the AE
servers' ports differ; a master may list a server by its IP).

Test hook: HALO_AE_TEST_MM_SEEN="address=players/max[,address=players/max ...]" stands in for the listings (an address
with "-" for players: heard of, not answering).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/ae_listing_build.h"
#include "../src/browser.h"
#include "../src/p2p.h"
#include "../src/ae_shipped_files.h"
#include "ae_address_rules.h"
#include "ae_map_download.h"
#include "ae_dev_mode.h"
#include "ae_glue_lobby.h"
#include "ae_service_record.h"
#include "ae_glue_social.h"
#include "ae_glue_social_game.h"
#include "ae_social_rules.h"
#include "ae_social_view_rules.h"
#include "ae_career_record.h"
#include "ae_glue_career.h"
#include "ae_screen_career.h"
#include "ae_screen_social_lobby.h"
#include "ae_hooks.h"
#include "ae_input.h"
#include "ae_matchmaking_rules.h"
#include "ae_motion.h"
#include "ae_screen_create_game.h"
#include "ae_screen_main_menu.h"
#include "ae_screen_matchmaking.h"
#include "ae_screen_party.h"
#include "ae_screen_party_panel.h"
#include "ae_glue_party.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);
char const *config_string(char const *name);
int config_write(const char *name, const char *value);

/* (0.9.3) the two uses of this screen: AE TOURNAMENT (offline playlists, social.txt) and MATCHMAKING (online, matchmaking.txt) */
enum { TAB_SOCIAL = 0, TAB_ONLINE = 1 };
/* (0.9.3) the tournament's CASUAL / COMP chips: LB / RB or a click between them */
enum { HIT_CHIPS_FILTER = 0x7690, FILTER_CASUAL = 0, FILTER_COMP = 1 };

enum
{
	MM_HIT = 0x7680,
	HIT_ROWS = MM_HIT + 1, HIT_PROMPTS
};
enum { ROW_PLAYLIST = AE_FLOW_ROW_OWN };
enum { MM_IDLE, MM_SEARCHING, MM_JOINING };

#define PLAYLISTS_MAXIMUM 8
#define SEEN_MAXIMUM 32
#define LISTINGS_MAXIMUM 64
/* the searching card's edge: menu blue to ice blue and back (1.2 s); the indeterminate bar's run */
#define PULSE_MS 1200ul
#define BAR_RUN_MS 1400ul
#define ICE_BLUE 0xB8D6FFFFu
/* the card: its height, insets (u) */
#define CARD_HEIGHT_U 168.0f
#define CARD_GAP_U 16.0f

static struct
{
	struct ae_flow_screen flow;
	const char *crumb;
	struct ae_mm_playlist playlists[PLAYLISTS_MAXIMUM];
	int count, current;
	struct ae_mm_server_seen seen[SEEN_MAXIMUM];
	unsigned long seen_build[SEEN_MAXIMUM]; /* its listing's AE build (ae_listing_build.h), 0: not known */
	int seen_count;
	int state, tried, trying;
	/* (owner 11-11) this search's turned-down joins: how many, the last reason, the other build that turned it down */
	int refused;
	char refused_reason[96];
	unsigned long refused_build;
	unsigned long search_start;
	char line[160];
	char value[32];
	struct ae_address address;
	/* (0.9.3) the use of the screen, the tournament playlist focused */
	int tab, social_current;
	int filter;                 /* FILTER_*: which tournament playlists are rows */
	int shown[AE_SOCIAL_PLAYLISTS];  /* the playlists of that filter (indexes into social.txt's) */
	int shown_count;
	struct ae_chip chips[2];
	/* (0.9.4) the calibration question: the local player being asked (-1: none open), who was asked (or turned it down) this
	time the screen was open, and whether every signed-in player has been through it */
	int ask_player, ask_done;
	char asked[4];
} mm;

static struct ae_screen_class const mm_class;

static int party_size(void)
{
	short player;
	int size = 1;

	for (player = 1; player < 4; player++)
		size += ae_screen_main_menu_signed_in(player) != 0;
	return size;
}

/* matchmaking.txt beside the game (ae_shipped_files.h), else the playlists this build knows (a build that ships no file:
Android, the web) */
static void playlists_read(void)
{
	static const char built_in[] =
		"casual|CASUAL|Relaxed matches. Jump in, no setup.|play.arenaevolved.com:10660\n"
		"btb|BTB|Big team battles with vehicles.|btb.arenaevolved.com:10672\n"
		"comp|COMP|Competitive rules, small teams.|comp.arenaevolved.com:10683\n"
		"classic|CLASSIC COMP|The modded discs' 2v2 classic competitive.|nhe.arenaevolved.com:26671\n"
		"community|MODDED & COMMUNITY|Modded and community maps; MCC Workshop maps need your MCC install.|"
		"mcc.haloae.com:18191\n";
	static char text[8192];

	mm.count = ae_shipped_file_read("matchmaking.txt", text, (long)sizeof(text)) > 0 ?
		ae_mm_playlists_parse(text, mm.playlists, PLAYLISTS_MAXIMUM) : 0;
	if (!mm.count)
	{
		platform_log("ae matchmaking: no matchmaking.txt: the built-in playlists");
		mm.count = ae_mm_playlists_parse(built_in, mm.playlists, PLAYLISTS_MAXIMUM);
	}
}

static struct ae_mm_playlist const *playlist(void)
{
	return mm.current >= 0 && mm.current < mm.count ? &mm.playlists[mm.current] : NULL;
}

/* ---------- (0.9.3) AE TOURNAMENT */

static struct ae_social_playlist const *social_at(int index)
{
	return ae_social_playlist_at(index);
}

static struct ae_social_playlist const *social_playlist(void)
{
	return social_at(mm.social_current);
}

/* the playlists of a filter: COMP is kind comp, CASUAL every other; DIGSITE ("# shown: with maps") only with its Workshop pack here */
static int filter_of(struct ae_social_playlist const *list)
{
	return list->kind == AE_SOCIAL_KIND_COMP ? FILTER_COMP : FILTER_CASUAL;
}

static void shown_build(void)
{
	int index;

	mm.shown_count = 0;
	for (index = 0; index < ae_social_playlist_count() && mm.shown_count < AE_SOCIAL_PLAYLISTS; index++)
		if (filter_of(social_at(index)) == mm.filter && ae_social_playlist_shown(social_at(index), ae_social_map_here_shown, NULL))
			mm.shown[mm.shown_count++] = index;
}

/* the row of a playlist in the shown set, -1 when it is not shown */
static int shown_row(int playlist_index)
{
	int row;

	for (row = 0; row < mm.shown_count; row++)
		if (mm.shown[row] == playlist_index)
			return row;
	return -1;
}

static char const *const filter_names[2] = { "casual", "comp" };

/* a filter on: its playlists as rows, the focus on the first (the card follows), the choice kept (menus.last_social is the
   focused playlist: the next time the screen opens it is on its filter) */
static void filter_set(int filter)
{
	if (filter == mm.filter || mm.state != MM_IDLE)
		return;
	mm.filter = filter;
	shown_build();
	mm.flow.first = 0;
	mm.flow.focus = 0;
	mm.line[0] = 0;
	if (mm.shown_count > 0)
		mm.social_current = mm.shown[0];
	platform_log("ae tournament: filter %s: %d playlists", filter_names[filter], mm.shown_count);
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static int social_party_ok(struct ae_social_playlist const *list)
{
	/* (the party's seats: the local players and, when it has them, the bots partied up with) */
	if (ae_party_panel_wanted())
	{
		int source;
		struct ae_party const *party = ae_glue_party(&source);

		return ae_party_search_seats(party, source) <= ae_social_party_limit(list);
	}
	return ae_social_party_size() <= ae_social_party_limit(list);
}

static char const *social_mode_word(int mode)
{
	return mode <= 0 ? "AUTO" : ae_social_skill_name(mode);
}

static int social_key(char *key, size_t size)
{
	return ae_social_screen_key(0, key, size);
}

static void social_mode_step(int direction)
{
	struct ae_social_playlist const *list = social_playlist();
	int mode;

	if (!list)
		return;
	mode = ae_social_mode(list->id) + direction;
	if (mode < 0 || mode > AE_SOCIAL_SKILL_STEPS)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_social_mode_set(list->id, mode);
	ae_social_modes_save();
	platform_log("ae tournament: %s bot skill %s", list->id, social_mode_word(mode));
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void social_find(void)
{
	struct ae_social_playlist const *list = social_playlist();

	if (!list)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (!social_party_ok(list))
	{
		ae_flow_nudge(&mm.flow, mm.flow.focus);
		return;
	}
	platform_log("ae tournament: %s: a game", list->id);
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_screen_social_lobby_open(list->id, 0, NULL, 0);
}

static unsigned short address_port(const char *text)
{
	struct ae_address parsed;

	return ae_address_parse(text, ae_lobby_host_port(), &parsed) == AE_ADDRESS_OK ? parsed.port : 0;
}

/* the servers heard: the test hook's, else the public listings' (AE's servers, by the playlist's addresses: the
listing's address as given, else the same port) */
static void seen_read(void)
{
	char const *test = getenv("HALO_AE_TEST_MM_SEEN");
	struct ae_mm_playlist const *p = playlist();

	mm.seen_count = 0;
	if (test && *test)
	{
		char text[512], *entry, *next;

		snprintf(text, sizeof(text), "%s", test);
		for (entry = text; entry && *entry && mm.seen_count < SEEN_MAXIMUM; entry = next)
		{
			struct ae_mm_server_seen *seen = &mm.seen[mm.seen_count];
			char *equals;

			next = strchr(entry, ',');
			if (next)
				*next++ = 0;
			equals = strchr(entry, '=');
			if (!equals)
				continue;
			*equals = 0;
			memset(seen, 0, sizeof(*seen));
			mm.seen_build[mm.seen_count] = 0;
			snprintf(seen->address, sizeof(seen->address), "%s", entry);
			seen->reachable = equals[1] != '-';
			if (sscanf(equals + 1, "%d/%d", &seen->players, &seen->max_players) != 2)
				seen->max_players = 16;
			mm.seen_count++;
		}
		return;
	}
	if (!p)
		return;
	{
		static struct p2p_listing listings[LISTINGS_MAXIMUM];
		int read = p2p_lobby_games(listings, LISTINGS_MAXIMUM), index, server;

		for (server = 0; server < p->address_count && mm.seen_count < SEEN_MAXIMUM; server++)
		{
			unsigned short port = address_port(p->addresses[server]);

			for (index = 0; index < read; index++)
			{
				struct p2p_listing const *listing = &listings[index];

				if (!listing->ae_address[0] || (strcmp(listing->ae_address, p->addresses[server]) &&
					(!port || address_port(listing->ae_address) != port)))
				{
					continue;
				}
				snprintf(mm.seen[mm.seen_count].address, sizeof(mm.seen[0].address), "%s", p->addresses[server]);
				mm.seen[mm.seen_count].players = listing->player_count;
				mm.seen[mm.seen_count].max_players = listing->maximum_player_count;
				mm.seen[mm.seen_count].reachable = 1;
				mm.seen_build[mm.seen_count] = listing->ae_build;
				mm.seen_count++;
				break;
			}
		}
	}
}

static struct ae_mm_server_seen const *seen_of(const char *address)
{
	int index;

	for (index = 0; index < mm.seen_count; index++)
		if (!strcmp(mm.seen[index].address, address))
			return &mm.seen[index];
	return NULL;
}

static unsigned long seen_build_of(const char *address)
{
	int index;

	for (index = 0; index < mm.seen_count; index++)
		if (!strcmp(mm.seen[index].address, address))
			return mm.seen_build[index];
	return 0;
}

const char *updater_version(void);

/* this build as a listing names it (1 << 24 | major << 16 | minor << 8 | patch) */
static unsigned long own_build(void)
{
	unsigned char stamp[7];

	ae_listing_build_write(stamp, updater_version());
	return ae_listing_build_read(stamp);
}

static void build_text(unsigned long build, char *text, int size)
{
	snprintf(text, (size_t)size, "%lu.%lu.%lu", (build >> 16) & 0xFFul, (build >> 8) & 0xFFul, build & 0xFFul);
}

/* "AE #n" for a playlist's server: the n-th of the AE servers' list (the site's numbering: CASUAL #1, BTB #2, COMP #3),
else its address */
static void server_name(const char *address, char *text, int size)
{
	static struct { unsigned short port; const char *name; } const names[] =
		{ { 10660, "AE #1" }, { 10672, "AE #2" }, { 10683, "AE #3" }, { 18191, "AE #4" }, { 26671, "AE #5" } };
	unsigned short port = address_port(address);
	int index;

	for (index = 0; index < (int)(sizeof(names) / sizeof(names[0])); index++)
	{
		if (names[index].port == port)
		{
			snprintf(text, (size_t)size, "%s", names[index].name);
			return;
		}
	}
	snprintf(text, (size_t)size, "%s", address);
}

static void search_end(const char *line)
{
	mm.state = MM_IDLE;
	snprintf(mm.line, sizeof(mm.line), "%s", line);
}

/* the next server of the playlist: joined, or the search's end */
static void search_next(void)
{
	struct ae_mm_playlist const *p = playlist();
	struct ae_result result;
	char name[64];
	int pick;

	if (!p)
	{
		search_end("There are no playlists in this install.");
		return;
	}
	seen_read();
	pick = ae_mm_playlist_pick(p, mm.seen, mm.seen_count, mm.tried, party_size());
	if (pick < 0)
	{
		char line[160];

		/* (owner 11-11: why, when the servers turned the join down, not "no room") */
		if (mm.refused && mm.refused_build)
		{
			char theirs[16];

			build_text(mm.refused_build, theirs, (int)sizeof(theirs));
			platform_log("ae matchmaking: turned down by build %s (%s), this is %s", theirs, p->label,
				updater_version());
			snprintf(line, sizeof(line), "The %s servers run Arena Evolved %s and take only that version. This one is %s.",
				p->label, theirs, updater_version());
		}
		else if (mm.refused)
		{
			platform_log("ae matchmaking: turned down (%s): %s", p->label, mm.refused_reason);
			/* (the host sent no word of its own: a version apart is the likely cause, the listing did not say) */
			if (!strcmp(mm.refused_reason, ae_string(AE_STR_ERR_REFUSED)))
				snprintf(line, sizeof(line), "The %s servers turned the join down. They may run another version of "
					"Arena Evolved; this one is %s.", p->label, updater_version());
			else
				snprintf(line, sizeof(line), "The %s servers turned the join down: %s.", p->label, mm.refused_reason);
		}
		else
		{
			platform_log("ae matchmaking: none with room (%s)", p->label);
			snprintf(line, sizeof(line), "No %s server has room right now.", p->label);
		}
		search_end(line);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	mm.tried |= 1 << pick;
	mm.trying = pick;
	server_name(p->addresses[pick], name, (int)sizeof(name));
	if (ae_address_parse(p->addresses[pick], ae_lobby_host_port(), &mm.address) != AE_ADDRESS_OK)
	{
		platform_log("ae matchmaking: %s: not an address, next", p->addresses[pick]);
		return;
	}
	result = ae_lobby_join_host(mm.address.host, mm.address.port);
	if (!result.ok)
	{
		platform_log("ae matchmaking: %s: %s, next", p->addresses[pick], result.reason);
		snprintf(mm.line, sizeof(mm.line), "%s did not answer, trying the next %s server", name, p->label);
		return;
	}
	mm.state = MM_JOINING;
	platform_log("ae matchmaking: joining %s: %s", p->label, p->addresses[pick]);
	snprintf(mm.line, sizeof(mm.line), "Joining %s...", name);
}

static void search_update(void)
{
	static struct ae_lobby_roster roster;
	struct ae_result failure;
	struct ae_mm_playlist const *p = playlist();
	int state;

	if (mm.state == MM_SEARCHING)
	{
		search_next();
		return;
	}
	if (mm.state != MM_JOINING || !p)
		return;
	state = ae_lobby_join_state(&failure);
	if (state == 0)
		return;
	if (state < 0)
	{
		char name[64];

		server_name(p->addresses[mm.trying], name, (int)sizeof(name));
		platform_log("ae matchmaking: %s: %s, next", p->addresses[mm.trying],
			failure.reason[0] ? failure.reason : "the join was left");
		{
			unsigned long theirs = seen_build_of(p->addresses[mm.trying]);

			/* (no answer is not a refusal: the search's end reads as before for it) */
			if (!failure.reason[0] || !strcmp(failure.reason, ae_string(AE_STR_ERR_NO_ANSWER)))
				snprintf(mm.line, sizeof(mm.line), "%s did not answer, trying the next %s server", name, p->label);
			else
			{
				mm.refused++;
				snprintf(mm.refused_reason, sizeof(mm.refused_reason), "%s", failure.reason);
				if (theirs && theirs != own_build())
					mm.refused_build = theirs;
				snprintf(mm.line, sizeof(mm.line), "%s turned the join down, trying the next %s server", name,
					p->label);
			}
		}
		mm.state = MM_SEARCHING;
		return;
	}
	if (!ae_lobby_roster(&roster).ok || !roster.joined)
		return;
	if (!ae_lobby_add_local_player(0).ok)
	{
		ae_lobby_leave();
		platform_log("ae matchmaking: %s: full when joined, next", p->addresses[mm.trying]);
		mm.state = MM_SEARCHING;
		return;
	}
	platform_log("ae matchmaking: joined %s", mm.address.text);
	mm.state = MM_IDLE;
	mm.line[0] = 0;
	ae_screen_create_game_lobby_joined("MATCHMAKING");
}

static void search_start(void)
{
	struct ae_mm_playlist const *p = playlist();

	if (!p || mm.state != MM_IDLE)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	mm.tried = 0;
	mm.trying = -1;
	mm.refused = 0;
	mm.refused_reason[0] = 0;
	mm.refused_build = 0;
	mm.state = MM_SEARCHING;
	mm.search_start = ae_motion_now();
	snprintf(mm.line, sizeof(mm.line), "Joining %s...", p->label);
	platform_log("ae matchmaking: searching %s (a party of %d)", p->label, party_size());
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void search_cancel(void)
{
	if (mm.state == MM_JOINING)
		ae_lobby_leave();
	mm.state = MM_IDLE;
	mm.line[0] = 0;
	platform_log("ae matchmaking: cancelled");
	ae_sound_request(AE_SOUND_BACK, 0);
}

static void rows_social(void)
{
	struct ae_flow_screen *flow = &mm.flow;
	static char help[AE_SOCIAL_PLAYLISTS][192];
	short count = 0;
	int index;

	for (index = 0; index < mm.shown_count && count < AE_SOCIAL_PLAYLISTS; index++)
	{
		struct ae_social_playlist const *list = social_at(mm.shown[index]);
		int ok = social_party_ok(list);

		help[index][0] = 0;
		/* (a playlist the party does not fit is muted; the footer's A and the party's search line say how many fit: a help line here met the card under the rows) */
		if (!ok && mm.tab != TAB_SOCIAL)
			snprintf(help[index], sizeof(help[index]), "A PARTY OF %d AT MOST", ae_social_party_limit(list));
		flow->rows[count] = (struct ae_flow_row){ ROW_PLAYLIST, -1, list->label, social_mode_word(ae_social_mode(list->id)),
			ok ? NULL : help[index], NULL };
		flow->rows[count++].muted = (short)!ok;
	}
	if (!count)
		flow->rows[count++] = (struct ae_flow_row){ ROW_PLAYLIST, -1, "NO PLAYLISTS", NULL, "This install has no tournament playlists.",
			NULL };
	flow->count = count;
}

static void rows(void)
{
	struct ae_flow_screen *flow = &mm.flow;
	static char help[PLAYLISTS_MAXIMUM][192];
	short count = 0;
	int index;

	if (mm.tab == TAB_SOCIAL)
	{
		rows_social();
		return;
	}

	for (index = 0; index < mm.count && count < PLAYLISTS_MAXIMUM; index++)
	{
		struct ae_mm_playlist const *p = &mm.playlists[index];

		snprintf(help[index], sizeof(help[index]), "%s A joins with your whole party (%d player%s).", p->description,
			party_size(), party_size() == 1 ? "" : "s");
		flow->rows[count] = (struct ae_flow_row){ ROW_PLAYLIST, -1, p->label,
			mm.state != MM_IDLE && index == mm.current ? "JOINING" : NULL, help[index], NULL };
		flow->rows[count++].muted = (short)(mm.state != MM_IDLE && index != mm.current);
	}
	if (!count)
		flow->rows[count++] = (struct ae_flow_row){ ROW_PLAYLIST, -1, "NO PLAYLISTS", NULL,
			"This install has no playlists.", NULL };
	flow->count = count;
}

/* (owner 11-11) the focused row is the playlist */
static void playlist_focus(short row)
{
	if (mm.tab == TAB_SOCIAL)
	{
		if (row >= 0 && row < mm.shown_count && mm.shown[row] != mm.social_current)
		{
			mm.social_current = mm.shown[row];
			platform_log("ae tournament: playlist %s", social_playlist()->id);
		}
		return;
	}
	if (row < 0 || row >= mm.count || row == mm.current || mm.state != MM_IDLE)
		return;
	mm.current = row;
	mm.line[0] = 0;
	platform_log("ae matchmaking: playlist %s", mm.playlists[mm.current].label);
}

/* ---------- (0.9.3) AE TOURNAMENT's card and rank badges */

/* a small square chip: the filled accent when on, an outline when not */
static float small_chip(struct ae_flow_screen *flow, float x, float center_y, char const *label, int on)
{
	float size = ae_flow_u(flow, 16.0f), h = ae_flow_u(flow, 26.0f), pad = ae_flow_u(flow, 12.0f);
	float w = ae_draw_text_tracked_width(AE_FONT_ROW, size, 0.06f, label) + 2.0f * pad;

	if (on)
		ae_draw_rect(x, center_y - h * 0.5f, w, h, 0.0f, AE_COLOR_ACCENT);
	else
		ae_draw_outline(x, center_y - h * 0.5f, w, h, 0.0f, ae_flow_u(flow, 1.0f), AE_COLOR_MUTED);
	ae_draw_text_tracked(AE_FONT_ROW, size, 0.06f, x + pad, ae_flow_cap_top(AE_FONT_ROW, size, center_y), AE_ALIGN_LEFT,
		on ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TITLE, label);
	return w;
}

/* the two filter chips over the rows, CASUAL and COMP, and the muted line after them; the rows start below (rows_top_offset_u) */
static void filter_chips_draw(struct ae_flow_screen *flow)
{
	struct ae_rect rects[2];
	float x = ae_flow_left_x(flow), y = ae_flow_u(flow, 190.0f), room = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH, end, minor;
	char const *line = mm.filter == FILTER_CASUAL ? "Offline games against bots. Skill ranks and XP stay on this PC." :
		"Offline bot games on competitive rules. Skill ranks and XP stay on this PC.";

	mm.chips[0] = (struct ae_chip){ "CASUAL", -1, mm.filter == FILTER_CASUAL ? AE_CHIP_ON : 0 };
	mm.chips[1] = (struct ae_chip){ "COMP", -1, mm.filter == FILTER_COMP ? AE_CHIP_ON : 0 };
	ae_chips_layout(&flow->density, room, mm.chips, 2, rects);
	ae_widget_chips(&flow->density, x, y, room, mm.chips, 2, HIT_CHIPS_FILTER);
	end = x + rects[1].x + rects[1].width + ae_flow_u(flow, 24.0f);
	minor = ae_size_minor(&flow->density);
	ae_draw_text(AE_FONT_BODY, minor, end, ae_flow_cap_top(AE_FONT_BODY, minor, y + rects[0].height * 0.5f), AE_ALIGN_LEFT,
		AE_COLOR_MUTED, line);
}

/* the rank badge after a row's label: a muted outlined "RANK 14" */
static void rank_badges_draw(struct ae_flow_screen *flow)
{
	float label_size = ae_size_text(&flow->density), small = ae_flow_u(flow, 15.0f);
	char key[AE_SR_KEY];
	int index, have = social_key(key, sizeof(key));

	for (index = 0; index < flow->count && index < mm.shown_count; index++)
	{
		struct ae_social_playlist const *list = social_at(mm.shown[index]);
		struct ae_rect const *r = &flow->rects[index];
		char text[24];
		float label_w, w, h = ae_flow_u(flow, 22.0f), x, cy;

		if (r->width <= 0.0f || !list)
			continue;
		{
			struct ae_career_social const *entry = have ? ae_career_social_find(ae_career_store(), key, list->id) : NULL;

			ae_social_view_rank_badge(entry ? entry->games : 0, have ? ae_social_profile_rank(key, list->id) : 1, text, (int)sizeof(text));
		}
		if (!text[0])
			continue;
		label_w = ae_draw_text_width(AE_FONT_ROW, label_size, list->label);
		w = ae_draw_text_tracked_width(AE_FONT_ROW, small, 0.06f, text) + ae_flow_u(flow, 16.0f);
		x = r->x + ae_flow_u(flow, 20.0f) + label_w + ae_flow_u(flow, 14.0f);
		cy = r->y + r->height * 0.5f;
		ae_draw_outline(x, cy - h * 0.5f, w, h, 0.0f, ae_flow_u(flow, 1.0f), index == flow->focus ? AE_COLOR_SELECTION_TEXT :
			AE_COLOR_MUTED);
		ae_draw_text_tracked(AE_FONT_ROW, small, 0.06f, x + ae_flow_u(flow, 8.0f), ae_flow_cap_top(AE_FONT_ROW, small, cy),
			AE_ALIGN_LEFT, index == flow->focus ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED, text);
	}
}

/* the card's height: 286 u, 42 more with the "get the rest" lines, 34 more with the DEV line */
static float social_card_height(struct ae_social_playlist const *list, struct ae_social_counts const *counts)
{
	struct ae_map_download_status download;

	return 286.0f + (ae_social_view_get_rest(list, counts, NULL, 0, NULL, 0) || ae_map_download_status(&download) ? 58.0f :
		0.0f) + (ae_dev_mode_on() ? 34.0f : 0.0f);
}

static void social_counts_of(struct ae_social_playlist const *list, struct ae_social_counts *counts)
{
	ae_social_playlist_counts(list, ae_social_map_here_shown, NULL, counts);
}

static void social_card_draw(float x, float y, float width)
{
	struct ae_flow_screen *flow = &mm.flow;
	struct ae_density const *d = &flow->density;
	struct ae_social_playlist const *list = social_playlist();
	struct ae_social_counts counts;
	struct ae_map_download_status download;
	float inset = ae_flow_u(flow, AE_FLOW_INSET_U), minor = ae_size_minor(d), body = ae_size_text(d), corner = ae_flow_u(flow, d->metrics->corner);
	float height, at, col = x + ae_flow_u(flow, 213.0f), foot;
	char text[200], key[AE_SR_KEY];
	int mode, have = social_key(key, sizeof(key));
	long exp;

	if (!list)
		return;
	social_counts_of(list, &counts);
	height = ae_flow_u(flow, social_card_height(list, &counts));
	mode = ae_social_mode(list->id);
	exp = have ? ae_social_profile_exp(key, list->id) : 0;
	ae_draw_rect(x, y, width, height, corner, AE_COLOR_PANEL);
	ae_draw_outline(x, y, width, height, corner, ae_flow_u(flow, AE_FLOW_RULE_U), AE_COLOR_RULE);
	ae_draw_text(AE_FONT_ROW, ae_flow_u(flow, 32.0f), x + inset, y + ae_flow_u(flow, 20.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE,
		list->label);
	ae_draw_text(AE_FONT_BODY, body, x + inset, y + ae_flow_u(flow, 68.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT, list->about);
	{
		int distinct = counts.pack_present + (counts.stock_in_use || counts.pack_total == 0 ? counts.stock_present : 0);

		ae_social_view_shape(list, distinct, text, (int)sizeof(text));
		ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x + inset, y + ae_flow_u(flow, 104.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
	}
	/* BOT SKILL: the two chips mirror the row's value (they are not focusable), and what it means */
	at = y + ae_flow_u(flow, 140.0f);
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x + inset, ae_flow_cap_top(AE_FONT_BODY, minor, at + ae_flow_u(flow, 8.0f)),
		AE_ALIGN_LEFT, AE_COLOR_MUTED, "BOT SKILL");
	{
		float cx = col, w;

		w = small_chip(flow, cx, at + ae_flow_u(flow, 8.0f), "AUTO", mode == 0);
		cx += w + ae_flow_u(flow, 8.0f);
		w = small_chip(flow, cx, at + ae_flow_u(flow, 8.0f), "FIXED", mode != 0);
		cx += w + ae_flow_u(flow, 16.0f);
		ae_social_view_bot_line(mode, text, (int)sizeof(text));
		ae_draw_text(AE_FONT_BODY, body, cx, ae_flow_cap_top(AE_FONT_BODY, body, at + ae_flow_u(flow, 8.0f)), AE_ALIGN_LEFT,
			AE_COLOR_TEXT, text);
	}
	/* MAPS ON THIS PC */
	at = y + ae_flow_u(flow, 176.0f);
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x + inset, ae_flow_cap_top(AE_FONT_BODY, minor, at + ae_flow_u(flow, 8.0f)),
		AE_ALIGN_LEFT, AE_COLOR_MUTED, "MAPS ON THIS PC");
	ae_social_view_maps(&counts, text, (int)sizeof(text));
	ae_draw_text(AE_FONT_ROW, body, col, ae_flow_cap_top(AE_FONT_ROW, body, at + ae_flow_u(flow, 8.0f)), AE_ALIGN_LEFT, AE_COLOR_TEXT,
		text);
	foot = 212.0f;
	/* (0.9.3) GET MAPS: while it runs, its progress line and bar in the "get the rest" lines' place; then its outcome
	for a while (a failure in the warning colour) */
	if (ae_map_download_status(&download))
	{
		float bar_y = y + ae_flow_u(flow, foot + 38.0f), bar_w = width - 2.0f * inset;

		if (download.running)
		{
			ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x + inset,
				ae_flow_cap_top(AE_FONT_BODY, minor, y + ae_flow_u(flow, foot + 6.0f)), AE_ALIGN_LEFT, AE_COLOR_TEXT,
				download.line);
			ae_draw_rect(x + inset, bar_y, bar_w, ae_flow_u(flow, 6.0f), 0.0f, AE_COLOR_RULE);
			if (download.total > 0)
				ae_draw_rect(x + inset, bar_y, bar_w * (float)((double)download.bytes / (double)download.total),
					ae_flow_u(flow, 6.0f), 0.0f, AE_COLOR_ACCENT);
		}
		else
			ae_draw_text(AE_FONT_BODY, minor, x + inset, ae_flow_cap_top(AE_FONT_BODY, minor, y + ae_flow_u(flow, foot + 6.0f)),
				AE_ALIGN_LEFT, ae_map_download_outcome_failed(download.outcome) ? AE_COLOR_WARNING : AE_COLOR_TEXT,
				download.outcome_text);
		foot += 58.0f;
	}
	else
	{
		char first[100], second[100];

		if (ae_social_view_get_rest(list, &counts, first, (int)sizeof(first), second, (int)sizeof(second)))
		{
			ae_draw_text(AE_FONT_BODY, minor, x + inset, ae_flow_cap_top(AE_FONT_BODY, minor, y + ae_flow_u(flow, foot + 6.0f)),
				AE_ALIGN_LEFT, AE_COLOR_MUTED, first);
			ae_draw_text(AE_FONT_BODY, minor, x + inset, ae_flow_cap_top(AE_FONT_BODY, minor, y + ae_flow_u(flow, foot + 34.0f)),
				AE_ALIGN_LEFT, AE_COLOR_MUTED, second);
			foot += 58.0f;
		}
	}
	if (ae_dev_mode_on())
	{
		ae_social_view_dev_line(have ? ae_social_profile_match_points(key, list->id) : AE_SOCIAL_MATCH_START, text,
			(int)sizeof(text));
		ae_draw_text(AE_FONT_BODY, minor, x + inset, ae_flow_cap_top(AE_FONT_BODY, minor, y + ae_flow_u(flow, foot + 6.0f)),
			AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
		foot += 34.0f;
	}
	/* the foot: a rule, the rank, the bar and the wins to the next rank */
	{
		float rule_y = y + ae_flow_u(flow, foot + 11.0f), mid = y + ae_flow_u(flow, foot + 37.0f);
		float bar_x = x + width * 0.42f, bar_w = width - (bar_x - x) - inset;
		char rank_text[32];

		ae_draw_rect(x + inset, rule_y, width - 2.0f * inset, ae_flow_u(flow, 1.0f), 0.0f, AE_COLOR_RULE);
		{
			struct ae_career_social const *entry = have ? ae_career_social_find(ae_career_store(), key, list->id) : NULL;
			int has_games = ae_social_view_foot_rank(entry ? entry->games : 0, ae_social_rank(exp), rank_text, (int)sizeof(rank_text));

			ae_draw_text(AE_FONT_ROW, ae_flow_u(flow, 28.0f), x + inset, ae_flow_cap_top(AE_FONT_ROW, ae_flow_u(flow, 28.0f), mid),
				AE_ALIGN_LEFT, AE_COLOR_TITLE, rank_text);
			if (!has_games)
				return;
		}
		ae_draw_rect(bar_x, mid - ae_flow_u(flow, 3.0f), bar_w, ae_flow_u(flow, 6.0f), 0.0f, AE_COLOR_RULE);
		ae_draw_rect(bar_x, mid - ae_flow_u(flow, 3.0f), bar_w * (float)ae_social_rank_percent(exp) / 100.0f, ae_flow_u(flow, 6.0f),
			0.0f, AE_COLOR_ACCENT);
		ae_social_view_wins_to(exp, text, (int)sizeof(text));
		ae_draw_text_tracked(AE_FONT_BODY, ae_flow_u(flow, 16.0f), 0.09f, x + width - inset,
			ae_flow_cap_top(AE_FONT_BODY, ae_flow_u(flow, 16.0f), mid + ae_flow_u(flow, 22.0f)), AE_ALIGN_RIGHT, AE_COLOR_MUTED, text);
	}
}

static unsigned int pulse_color(unsigned long now)
{
	unsigned long at = (now - mm.search_start) % PULSE_MS;
	float t = (float)at / (float)PULSE_MS;
	unsigned int from = AE_COLOR_WARNING, to = ICE_BLUE, color = 0xFFu;
	int shift;

	t = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
	if (ae_motion_reduced())
		t = 0.0f;
	for (shift = 24; shift >= 8; shift -= 8)
	{
		float a = (float)((from >> shift) & 0xFFu), b = (float)((to >> shift) & 0xFFu);

		color |= (unsigned int)(a + (b - a) * t + 0.5f) << shift;
	}
	return color;
}

/* the playlist's card: its label, description and server; searching: the edge pulsing, a running bar, the time */
static void card_draw(float x, float y, float width)
{
	struct ae_flow_screen *flow = &mm.flow;
	struct ae_density const *d = &flow->density;
	struct ae_mm_playlist const *p = playlist();
	float height = ae_flow_u(flow, CARD_HEIGHT_U), inset = ae_flow_u(flow, AE_FLOW_INSET_U);
	float minor = ae_size_minor(d), body = ae_size_text(d), corner = ae_flow_u(flow, d->metrics->corner);
	unsigned long now = ae_motion_now();
	char text[160], name[64];

	if (!p)
		return;
	ae_draw_rect(x, y, width, height, corner, AE_COLOR_PANEL);
	if (mm.state != MM_IDLE)
		ae_draw_outline(x, y, width, height, corner, ae_flow_u(flow, 2.0f), pulse_color(now));
	else
		ae_draw_outline(x, y, width, height, corner, ae_flow_u(flow, AE_FLOW_RULE_U), AE_COLOR_RULE);
	ae_draw_text(AE_FONT_ROW, ae_flow_u(flow, 32.0f), x + inset, y + ae_flow_u(flow, 20.0f), AE_ALIGN_LEFT,
		AE_COLOR_TITLE, mm.state != MM_IDLE ? mm.line : p->label);
	ae_draw_text(AE_FONT_BODY, body, x + inset, y + ae_flow_u(flow, 68.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT,
		p->description);
	if (mm.state != MM_IDLE)
	{
		unsigned long seconds = (now - mm.search_start) / 1000ul;
		float bar_y = y + ae_flow_u(flow, 130.0f), bar_h = ae_flow_u(flow, 6.0f), run = width - 2.0f * inset;
		float at = ae_motion_reduced() ? 0.0f : (float)((now - mm.search_start) % BAR_RUN_MS) / (float)BAR_RUN_MS;

		snprintf(text, sizeof(text), "SEARCHING  %lu:%02lu", seconds / 60ul, seconds % 60ul);
		ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x + inset, y + ae_flow_u(flow, 104.0f), AE_ALIGN_LEFT,
			AE_COLOR_MUTED, text);
		ae_draw_rect(x + inset, bar_y, run, bar_h, 0.0f, AE_COLOR_RULE);
		ae_draw_rect(x + inset + run * at * 0.75f, bar_y, run * 0.25f, bar_h, 0.0f, AE_COLOR_ACCENT);
		return;
	}
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x + inset, y + ae_flow_u(flow, 104.0f), AE_ALIGN_LEFT,
		AE_COLOR_MUTED, "SERVER");
	server_name(p->addresses[0], name, (int)sizeof(name));
	{
		struct ae_mm_server_seen const *seen = seen_of(p->addresses[0]);
		float bar_x = x + width * 0.42f, bar_y = y + ae_flow_u(flow, 140.0f), bar_w = width * 0.58f - inset;

		if (seen && seen->max_players > 0)
			snprintf(text, sizeof(text), "%s \xE2\x80\xA2 %d / %d players", name, seen->players, seen->max_players);
		else
			snprintf(text, sizeof(text), "%s \xE2\x80\xA2 not heard from yet", name);
		ae_draw_text(AE_FONT_ROW, body, x + inset, y + ae_flow_u(flow, 128.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
		ae_draw_rect(bar_x, bar_y, bar_w, ae_flow_u(flow, 6.0f), 0.0f, AE_COLOR_RULE);
		if (seen && seen->max_players > 0)
			ae_draw_rect(bar_x, bar_y, bar_w * (float)seen->players / (float)seen->max_players, ae_flow_u(flow, 6.0f), 0.0f,
				AE_COLOR_ACCENT);
	}
	if (mm.line[0])
		ae_draw_text(AE_FONT_BODY, minor, x, y + height + ae_flow_u(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT, mm.line);
}

/* (0.9.3) GET MAPS running: B cancels it, nothing else is taken */
static int get_maps_running(void)
{
	struct ae_map_download_status download;

	return ae_map_download_status(&download) && download.running;
}

static void get_maps_start(void)
{
	struct ae_map_download_status download;

	if (ae_map_download_start())
	{
		ae_sound_request(AE_SOUND_FORWARD, 0);
		platform_log("ae tournament: GET MAPS");
		return;
	}
	ae_sound_request(AE_SOUND_FAILURE, 0);
	if (ae_map_download_status(&download))
		platform_log("ae tournament: GET MAPS not started: %s", download.outcome_text);
}

static void mm_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &mm.flow;
	float width;

	(void)instance;
	ae_flow_full_density(flow);
	if (mm.state == MM_IDLE && mm.tab != TAB_SOCIAL)
		seen_read();
	rows();
	ae_flow_title(flow, mm.tab == TAB_SOCIAL ? "AE TOURNAMENT" : "MATCHMAKING", mm.crumb);
	width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH;
	/* (0.9.3 the CASUAL / COMP chips head the tournament's rows) */
	flow->rows_top_offset_u = mm.tab == TAB_SOCIAL ? 50.0f : 0.0f;
	if (mm.tab == TAB_SOCIAL && mm.state == MM_IDLE)
		filter_chips_draw(flow);
	/* (0.9.3: the tournament's rows scroll: the rows scroll with the focus above the card, which keeps its room) */
	if (mm.tab == TAB_SOCIAL && mm.state == MM_IDLE && social_playlist())
	{
		struct ae_social_counts card_counts;
		float whole = flow->frame.rect.height, top = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y), offset = flow->rows_top_offset_u;

		social_counts_of(social_playlist(), &card_counts);
		flow->frame.rect.height -= ae_flow_u(flow, social_card_height(social_playlist(), &card_counts) + CARD_GAP_U) +
			ae_widget_more_line(&flow->density);
		/* (the rows start where they did: the content's top follows the frame's height) */
		flow->rows_top_offset_u += (top - ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y)) / ae_flow_u(flow, 1.0f);
		ae_flow_draw_rows(flow, width);
		flow->frame.rect.height = whole;
		flow->rows_top_offset_u = offset;
		/* (the "N more" line under scrolled rows is part of what the card clears) */
		if (flow->count > 0 && flow->rects[flow->count - 1].width <= 0.0f)
			flow->rows_end += ae_widget_more_line(&flow->density);
	}
	else
		ae_flow_draw_rows(flow, width);
	if (mm.tab == TAB_SOCIAL && mm.state == MM_IDLE)
	{
		rank_badges_draw(flow);
		social_card_draw(ae_flow_left_x(flow), flow->rows_end + ae_flow_u(flow, CARD_GAP_U), width);
	}
	else
		card_draw(ae_flow_left_x(flow), flow->rows_end + ae_flow_u(flow, CARD_GAP_U), width);
	if (mm.tab == TAB_SOCIAL && social_playlist())
		ae_party_panel_set_limit(ae_social_party_limit(social_playlist()));
	/* (MY PARTY is one panel: this PC's players as ever, then, when bots or friends are in the party, their rows and the search line under them) */
	if (mm.tab == TAB_SOCIAL && mm.state == MM_IDLE && social_playlist() && ae_party_panel_wanted())
	{
		int source, limit = ae_social_party_limit(social_playlist());
		struct ae_party const *party = ae_glue_party(&source);

		ae_screen_main_menu_party_seats(ae_party_search_seats(party, source), limit);
		ae_screen_main_menu_party_draw(flow, "In AE Tournament");
		ae_screen_main_menu_party_seats(0, 0);
		ae_party_panel_draw_below(flow, ae_screen_main_menu_party_end_y(), limit);
	}
	else
		ae_screen_main_menu_party_draw(flow, mm.state != MM_IDLE && playlist() ? "Joining a playlist" :
			mm.tab == TAB_SOCIAL ? "In AE Tournament" : "In Matchmaking");
	flow->prompt_count = 0;
	if (mm.state != MM_IDLE || get_maps_running())
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Cancel", AE_ACTION_BACK);
	}
	else
	{
		static char find_label[40];
		struct ae_party const *party;
		int source, party_seats;

		party = ae_glue_party(&source);
		party_seats = ae_party_search_seats(party, source);
		if (mm.tab == TAB_SOCIAL && social_playlist() && !social_party_ok(social_playlist()))
			snprintf(find_label, sizeof(find_label), "Party of %d: %d at most", ae_party_panel_wanted() ? party_seats : ae_social_party_size(),
				ae_social_party_limit(social_playlist()));
		else if (mm.tab == TAB_SOCIAL && party_seats > 1 && social_playlist() && ae_party_panel_wanted())
			snprintf(find_label, sizeof(find_label), "Find a game (party of %d)", party_seats);
		else
			snprintf(find_label, sizeof(find_label), "%s", mm.tab == TAB_SOCIAL ? "Find a game" : "Join");
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, find_label, AE_ACTION_ACCEPT);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Party", AE_ACTION_X);
		if (mm.tab == TAB_SOCIAL)
		{
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Career", AE_ACTION_Y);
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_DPAD_LEFT, "Bot skill", AE_ACTION_LEFT);
			flow->prompts[flow->prompt_count - 1].pair = 1;
			flow->prompts[flow->prompt_count] = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Casual / Comp", AE_ACTION_TAB_PREVIOUS);
			flow->prompts[flow->prompt_count++].pair = 1;
			/* (0.9.3 GET MAPS: the playlists' maps this PC lacks, from the AE servers) */
			if (ae_map_download_missing() > 0)
				flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_MARK_RIGHT_STICK, "Get maps", AE_ACTION_THUMB);
		}
	}
	/* (0.8.2: under the PARTY manager its footer is the only one) */
	if (!ae_screen_party_covers(flow))
		ae_flow_draw_prompts(flow);
}

/* ---------- (0.9.4) CALIBRATION: how much Halo a profile has played, asked once */

static void ask_picked(short choice, void *context);

/* the local players' profile keys that are due the question: signed in (player 1 always) with no answer yet, not asked this time */
static int ask_due(int player, char *key, size_t size)
{
	if (player < 0 || player >= 4 || mm.asked[player] || (player > 0 && !ae_screen_main_menu_signed_in((short)player)))
		return 0;
	if (!ae_social_screen_key((short)player, key, size) || !key[0])
		return 0;
	return ae_social_profile_experience(key) == AE_SOCIAL_EXPERIENCE_UNSET;
}

static void ask_next(void)
{
	static char body[320];
	static char labels[3][48];
	struct ae_dialog_spec spec;
	char key[64], name[64];
	int player, experience;

	for (player = 0; player < 4; player++)
		if (ask_due(player, key, sizeof(key)))
			break;
	if (player >= 4)
	{
		mm.ask_done = 1;
		mm.ask_player = -1;
		return;
	}
	if (!ae_social_profile_name((short)player, name, sizeof(name)))
		snprintf(name, sizeof(name), "PLAYER %d", player + 1);
	/* (split screen: the dialog names whose answer it is) */
	if (ae_social_party_size() > 1)
		snprintf(body, sizeof(body), "For %s (player %d). Your first %d games in each playlist adjust this either way.", name,
			player + 1, (int)AE_SOCIAL_PLACEMENT_GAMES);
	else
		snprintf(body, sizeof(body), "Your first %d games in each playlist adjust this either way.", (int)AE_SOCIAL_PLACEMENT_GAMES);
	for (experience = AE_SOCIAL_EXPERIENCE_NEW; experience < NUMBER_OF_AE_SOCIAL_EXPERIENCES; experience++)
		snprintf(labels[experience - 1], sizeof(labels[0]), "%s: starts at %s bots", ae_social_experience_name(experience),
			ae_social_experience_skill(experience));
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "HOW MUCH HALO HAVE YOU PLAYED?";
	spec.body = body;
	for (experience = 0; experience < 3; experience++)
		spec.choices[experience] = labels[experience];
	spec.choice_count = 3;
	spec.safe_choice = 1;
	spec.cancel_choice = -1;
	spec.has_cancel = 1;
	spec.density = mm.flow.density;
	spec.bounds = mm.flow.frame.rect;
	spec.picked = ask_picked;
	spec.context = NULL;
	mm.ask_player = player;
	if (!ae_dialog_open(&spec, AE_OWNER_ANY))
	{
		mm.ask_player = -1;
		return;
	}
	platform_log("ae tournament: calibration asked of player %d", player + 1);
}

static void ask_picked(short choice, void *context)
{
	char key[64];
	int player = mm.ask_player;

	(void)context;
	mm.ask_player = -1;
	if (player < 0 || player >= 4)
		return;
	/* (B: asked again next time; not again before the screen is left) */
	mm.asked[player] = 1;
	if (choice >= 0 && choice < 3 && ae_social_screen_key((short)player, key, sizeof(key)))
	{
		if (ae_social_profile_experience_set(key, AE_SOCIAL_EXPERIENCE_NEW + choice, 0))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		else
			ae_sound_request(AE_SOUND_FAILURE, 0);
	}
	else
		platform_log("ae tournament: calibration of player %d left for next time", player + 1);
}

static void mm_update(struct ae_screen *instance)
{
	(void)instance;
	/* (the question, when this is the tournament and nothing else is up: the next due player's, once the screen is on top) */
	if (mm.tab == TAB_SOCIAL && !mm.ask_done && mm.ask_player < 0 && mm.flow.frame.rect.width > 0.0f && mm.state == MM_IDLE && !get_maps_running())
		ask_next();
	search_update();
	ae_screen_main_menu_party_update();
}

/* LB / RB: the playlist before / after, round the list (as before the tournament had its own row) */
static void playlist_step(int direction)
{
	int count = mm.tab == TAB_SOCIAL ? ae_social_playlist_count() : mm.count;

	/* (0.9.3 the tournament: LB / RB are the CASUAL / COMP chips) */
	if (mm.tab == TAB_SOCIAL)
	{
		filter_set(mm.filter == FILTER_CASUAL ? FILTER_COMP : FILTER_CASUAL);
		return;
	}
	if (count < 2 || mm.state != MM_IDLE)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	mm.flow.focus = (short)(((mm.tab == TAB_SOCIAL ? mm.social_current : mm.current) + direction + count) % count);
	mm.line[0] = 0;
	playlist_focus(mm.flow.focus);
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static int mm_handle(struct ae_screen *instance, struct ae_event const *event)
{
	struct ae_flow_screen *flow = &mm.flow;

	(void)instance;
	/* (review 0.8.1: no party changes while a search runs: a guest's B asked over it and froze the search, a new
	guest's START grew the party after its server was picked) */
	if (mm.state != MM_IDLE)
	{
		if (event->action == AE_ACTION_BACK && ae_input_event_controller() <= 0)
			search_cancel();
		return 1;
	}
	if (get_maps_running())
	{
		if (event->action == AE_ACTION_BACK)
		{
			ae_map_download_cancel();
			ae_sound_request(AE_SOUND_BACK, 0);
		}
		return 1;
	}
	if (ae_screen_main_menu_party_press(flow, event))
		return 1;
	switch (event->action)
	{
	case AE_ACTION_THUMB:
		if (mm.tab == TAB_SOCIAL && ae_map_download_missing() > 0)
			get_maps_start();
		return 1;
	case AE_ACTION_X:
		/* (R2-04: X Party, the PARTY manager over the lobby) */
		if (ae_screen_party_open(flow))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		playlist_step(event->action == AE_ACTION_TAB_NEXT ? 1 : -1);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (mm.tab == TAB_SOCIAL)
			social_mode_step(event->action == AE_ACTION_RIGHT ? 1 : -1);
		return 1;
	case AE_ACTION_Y:
		if (mm.tab == TAB_SOCIAL)
		{
			ae_sound_request(AE_SOUND_FORWARD, 0);
			ae_screen_career_open("AE TOURNAMENT", 0, mm.tab == TAB_SOCIAL && social_playlist() ? social_playlist()->id : NULL);
		}
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		playlist_focus(flow->focus);
		return 1;
	case AE_ACTION_START:
	case AE_ACTION_ACCEPT:
		playlist_focus(flow->focus);
		if (mm.tab == TAB_SOCIAL)
			social_find();
		else
			search_start();
		return 1;
	case AE_ACTION_BACK:
		return 0;
	default:
		return 1;
	}
}

static void mm_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_flow_screen *flow = &mm.flow;
	struct ae_event event;
	short row;

	if (ae_prompts_pointer(flow->prompts, flow->prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK && mm.state == MM_IDLE)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			mm_handle(instance, &event);
		return;
	}
	if (mm.state != MM_IDLE)
		return;
	/* (MY PARTY: a pad's profile card, a click opening the PARTY manager) */
	if (ae_screen_main_menu_party_pointer(flow, pointer))
		return;
	{
		struct ae_hit hit;

		/* (the chips: a click on one is the filter) */
		if (mm.tab == TAB_SOCIAL && pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit) &&
			hit.id == HIT_CHIPS_FILTER && hit.index >= 0 && hit.index < 2)
		{
			filter_set(hit.index == 0 ? FILTER_CASUAL : FILTER_COMP);
			return;
		}
	}
	row = ae_flow_rows_pointer(flow, pointer);
	if (pointer->moved)
		playlist_focus(flow->focus);
	if (row == -2 || row == -3)
	{
		if (mm.tab == TAB_SOCIAL)
			social_mode_step(row == -3 ? 1 : -1);
		return;
	}
	if (row >= 0)
	{
		playlist_focus(row);
		if (mm.tab == TAB_SOCIAL)
			social_find();
		else
			search_start();
	}
}

static void mm_leave(struct ae_screen *instance)
{
	(void)instance;
	if (mm.state != MM_IDLE)
		search_cancel();
	/* (GET MAPS runs while this screen is up alone: its connection is not left to the menus behind) */
	ae_map_download_cancel();
	/* (the playlist is remembered as the screen closes: config.toml is not written for every row crossed) */
	if (mm.tab != TAB_SOCIAL && playlist())
		config_write("menus.last_playlist", playlist()->id);
	if (mm.tab == TAB_SOCIAL && social_playlist())
		config_write("menus.last_social", social_playlist()->id);
#ifdef HALO_GAME_BROWSER
	if (mm.tab != TAB_SOCIAL)
		browser_follow_listings(0);
#endif
	platform_log(mm.tab == TAB_SOCIAL ? "ae tournament: closed" : "ae matchmaking: closed");
}

static struct ae_screen_class const mm_class =
{
	.name = "matchmaking", .leave = mm_leave, .handle = mm_handle, .draw = mm_draw, .pointer = mm_pointer,
	.update = mm_update,
};

static int open_tab(const char *crumb, int tab, char const *social_id, char const *filter_name)
{
	int filter_force = -1;
	char const *last = config_string("menus.last_playlist");
	char const *last_social = social_id && social_id[0] ? social_id : config_string("menus.last_social");
	int index;

	/* (open already: its state is not cleared under it) */
	if (ae_ui_holds(&mm))
		return 0;
	memset(&mm, 0, sizeof(mm));
	mm.crumb = crumb ? crumb : tab == TAB_SOCIAL ? NULL : "MULTIPLAYER";
	mm.ask_player = -1;
	mm.flow.hit_rows = HIT_ROWS;
	mm.flow.hit_prompts = HIT_PROMPTS;
	playlists_read();
	for (index = 0; index < mm.count; index++)
		if (last && !strcmp(mm.playlists[index].id, last))
			mm.current = index;
	/* (0.9.3 the tournament playlist focused as it was left) */
	mm.tab = tab;
	for (index = 0; index < ae_social_playlist_count(); index++)
		if (last_social && !strcmp(social_at(index)->id, last_social))
			mm.social_current = index;
	/* (the filter: the one of the playlist focused last, or of a named one: casual with none) */
	if (social_playlist())
		mm.filter = filter_of(social_playlist());
	shown_build();
	if (mm.shown_count > 0 && shown_row(mm.social_current) < 0)
		mm.social_current = mm.shown[0];
	if (filter_name && filter_name[0] && !(social_id && social_id[0]))
		filter_force = !strcmp(filter_name, "comp") ? FILTER_COMP : !strcmp(filter_name, "casual") ? FILTER_CASUAL : -1;
	if (filter_force >= 0 && filter_force != mm.filter)
	{
		mm.filter = filter_force;
		shown_build();
		if (mm.shown_count > 0)
			mm.social_current = mm.shown[0];
	}
	/* (the focus starts on the last playlist joined) */
	mm.flow.focus = (short)(mm.tab == TAB_SOCIAL ? (shown_row(mm.social_current) >= 0 ? shown_row(mm.social_current) : 0) :
		mm.current);
	if (!ae_ui_push(&mm_class, AE_OWNER_ANY, &mm))
		return 0;
	/* (a build with no server browser (the web's) follows no listings) */
#ifdef HALO_GAME_BROWSER
	if (tab != TAB_SOCIAL)
		browser_follow_listings(1);
#endif
	if (tab == TAB_SOCIAL)
	{
		platform_log("ae tournament: open (%d playlists)", ae_social_playlist_count());
		platform_log("ae tournament: filter %s: %d playlists", filter_names[mm.filter], mm.shown_count);
	}
	else
		platform_log("ae matchmaking: open (%d playlists, %s)", mm.count, mm.count ? mm.playlists[mm.current].label : "none");
	return 1;
}

int ae_screen_matchmaking_open(const char *crumb)
{
	return open_tab(crumb, TAB_ONLINE, NULL, NULL);
}

int ae_screen_tournament_open(const char *crumb, char const *playlist_id)
{
	return open_tab(crumb, TAB_SOCIAL, playlist_id, NULL);
}

int ae_screen_tournament_party_open(void)
{
	return ae_screen_party_open(&mm.flow);
}

int ae_screen_tournament_open_filter(const char *crumb, char const *playlist_id, char const *filter)
{
	return open_tab(crumb, TAB_SOCIAL, playlist_id, filter);
}

void ae_screen_matchmaking_close(void)
{
	(void)ae_ui_remove(&mm);
}
