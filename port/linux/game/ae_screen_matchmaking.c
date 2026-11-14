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
#include "ae_glue_lobby.h"
#include "ae_hooks.h"
#include "ae_input.h"
#include "ae_matchmaking_rules.h"
#include "ae_motion.h"
#include "ae_screen_create_game.h"
#include "ae_screen_main_menu.h"
#include "ae_screen_matchmaking.h"
#include "ae_screen_party.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);
char const *config_string(char const *name);
int config_write(const char *name, const char *value);

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

static void playlist_step(int direction)
{
	if (mm.count < 2 || mm.state != MM_IDLE)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	mm.current = (mm.current + direction + mm.count) % mm.count;
	mm.flow.focus = (short)mm.current;
	mm.line[0] = 0;
	platform_log("ae matchmaking: playlist %s", mm.playlists[mm.current].label);
	ae_sound_request(AE_SOUND_CURSOR, 0);
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

static void rows(void)
{
	struct ae_flow_screen *flow = &mm.flow;
	static char help[PLAYLISTS_MAXIMUM][192];
	short count = 0;
	int index;

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
	if (row < 0 || row >= mm.count || row == mm.current || mm.state != MM_IDLE)
		return;
	mm.current = row;
	mm.line[0] = 0;
	platform_log("ae matchmaking: playlist %s", mm.playlists[mm.current].label);
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

static void mm_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &mm.flow;
	float width;

	(void)instance;
	ae_flow_full_density(flow);
	if (mm.state == MM_IDLE)
		seen_read();
	rows();
	ae_flow_title(flow, "MATCHMAKING", mm.crumb);
	width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH;
	ae_flow_draw_rows(flow, width);
	card_draw(ae_flow_left_x(flow), flow->rows_end + ae_flow_u(flow, CARD_GAP_U), width);
	ae_screen_main_menu_party_draw(flow, mm.state != MM_IDLE && playlist() ? "Joining a playlist" : "In Matchmaking");
	flow->prompt_count = 0;
	if (mm.state != MM_IDLE)
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Cancel", AE_ACTION_BACK);
	}
	else
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Join", AE_ACTION_ACCEPT);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Party", AE_ACTION_X);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	}
	/* (0.8.2: under the PARTY manager its footer is the only one) */
	if (!ae_screen_party_covers(flow))
		ae_flow_draw_prompts(flow);
}

static void mm_update(struct ae_screen *instance)
{
	(void)instance;
	search_update();
	ae_screen_main_menu_party_update();
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
	if (ae_screen_main_menu_party_press(flow, event))
		return 1;
	switch (event->action)
	{
	case AE_ACTION_X:
		/* (R2-04: X Party, the PARTY manager over the lobby) */
		if (ae_screen_party_open(flow))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		playlist_step(event->action == AE_ACTION_TAB_NEXT ? 1 : -1);
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		playlist_focus(flow->focus);
		return 1;
	case AE_ACTION_START:
	case AE_ACTION_ACCEPT:
		playlist_focus(flow->focus);
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
	row = ae_flow_rows_pointer(flow, pointer);
	if (pointer->moved)
		playlist_focus(flow->focus);
	if (row >= 0)
	{
		playlist_focus(row);
		search_start();
	}
}

static void mm_leave(struct ae_screen *instance)
{
	(void)instance;
	if (mm.state != MM_IDLE)
		search_cancel();
	/* (the playlist is remembered as the screen closes: config.toml is not written for every row crossed) */
	if (playlist())
		config_write("menus.last_playlist", playlist()->id);
#ifdef HALO_GAME_BROWSER
	browser_follow_listings(0);
#endif
	platform_log("ae matchmaking: closed");
}

static struct ae_screen_class const mm_class =
{
	.name = "matchmaking", .leave = mm_leave, .handle = mm_handle, .draw = mm_draw, .pointer = mm_pointer,
	.update = mm_update,
};

int ae_screen_matchmaking_open(const char *crumb)
{
	char const *last = config_string("menus.last_playlist");
	int index;

	/* (open already: its state is not cleared under it) */
	if (ae_ui_holds(&mm))
		return 0;
	memset(&mm, 0, sizeof(mm));
	mm.crumb = crumb ? crumb : "MULTIPLAYER";
	mm.flow.hit_rows = HIT_ROWS;
	mm.flow.hit_prompts = HIT_PROMPTS;
	playlists_read();
	for (index = 0; index < mm.count; index++)
		if (last && !strcmp(mm.playlists[index].id, last))
			mm.current = index;
	/* (the focus starts on the last playlist joined) */
	mm.flow.focus = (short)mm.current;
	if (!ae_ui_push(&mm_class, AE_OWNER_ANY, &mm))
		return 0;
	/* (a build with no server browser (the web's) follows no listings) */
#ifdef HALO_GAME_BROWSER
	browser_follow_listings(1);
#endif
	platform_log("ae matchmaking: open (%d playlists, %s)", mm.count, mm.count ? mm.playlists[mm.current].label : "none");
	return 1;
}

void ae_screen_matchmaking_close(void)
{
	(void)ae_ui_remove(&mm);
}
