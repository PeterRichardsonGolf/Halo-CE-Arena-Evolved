/*
AE_SCREEN_HOST_SERVER.C

MULTIPLAYER's HOST A SERVER page (0.7.7, plan Task 9; the approved mockups A-1 to A-6: owner pick 1, option A). One page
for the server's whole life (ae_host_server.h):

- before a start, HOST A SERVER: SERVER (SERVER NAME, PLAYLIST, WHO CAN FIND IT: PRIVATE by default, MAX PLAYERS, BOTS
  FILL EMPTY SPOTS), THIS PC (JOIN IT WHEN READY), and START SERVER pinned under them; the right panel checks what the
  start needs (MCC's files, the playlist, the ports) and says what a server here is;
- STARTING: the rows quiet, CANCEL; the panel follows the server's steps; B goes back while it keeps starting;
- a failed start: the panel's red edge, the reason, its output file; START SERVER again;
- running, MY SERVER: JOIN MY SERVER, COPY INVITE, OPEN ITS WEB PAGE (this PC only), the settings it runs with (quiet),
  and STOP SERVER pinned, red, asking first (CANCEL focused); the panel shows the server: up for, players, the game now,
  how it is found.

The page's look is the flow screens' (ae_setup_flow.h); the panel is MY PARTY's (a dark head bar, the body under it).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_glue_profiles.h"
#include "ae_host_server.h"
#include "ae_screen_host_server.h"
#include "ae_screen_create_game.h"
#include "ae_server_ingame.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "../src/ae_host_dedicated.h"

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
long player_ui_get_active_player_profile_index(short local_player);

enum
{
	HOST_HIT = 0x7B40,
	HIT_ROWS = HOST_HIT + 1, HIT_PROMPTS, HIT_PANEL,
};
enum
{
	ROW_NAME = AE_FLOW_ROW_OWN, ROW_PLAYLIST, ROW_FIND, ROW_PLAYERS, ROW_BOTS, ROW_JOIN_READY, ROW_START, ROW_CANCEL,
	ROW_JOIN, ROW_INVITE, ROW_WEB, ROW_STOP,
};

#define RUNNING_GREEN 0x6FD08CFFu      /* the browser's good-ping green: a running server */
#define PANEL_HEAD 0x0E1216F0u         /* MY PARTY's head bar */
#define PANEL_FILL 0x181F26D1u
#define FAILED_EDGE 0xC0392BFFu
#define PANEL_HEAD_U 40.0f
#define PANEL_PAD_U 24.0f

static struct
{
	struct ae_flow_screen flow;
	char crumb[32];
	char note[160];
	unsigned long note_until;
	/* SERVER NAME typed (AE's keyboard) */
	struct ae_text name;
	int naming;
	int confirm_stop;
	int was_state;
} page;

static struct ae_screen_class const host_class;

static float u(float spec)
{
	return ae_flow_u(&page.flow, spec);
}

static void note_set(char const *text)
{
	snprintf(page.note, sizeof(page.note), "%s", text);
	page.note_until = system_milliseconds() + 6000;
}

/* the profile player 1 plays with: "Goat's Server", cut to the game's 15 characters */
static void default_name(char *name, int size)
{
	static struct ae_profile_summary profiles[AE_PROFILES_MAXIMUM];
	long active = player_ui_get_active_player_profile_index(0);
	short count = 0, index;

	snprintf(name, (size_t)size, "My Server");
	if (!ae_profiles_list(profiles, AE_PROFILES_MAXIMUM, &count).ok)
		return;
	for (index = 0; index < count; index++)
	{
		if (profiles[index].index == active && profiles[index].name[0])
		{
			char text[64];

			snprintf(text, sizeof(text), "%s's Server", profiles[index].name);
			if (strlen(text) > 15)
				snprintf(text, sizeof(text), "%.15s", profiles[index].name);
			snprintf(name, (size_t)size, "%s", text);
			return;
		}
	}
}

static int running(void)
{
	return ae_host_server_state() == AE_HD_READY || ae_host_server_state() == AE_HD_STOPPING;
}

static void rows_make(void)
{
	struct ae_flow_screen *flow = &page.flow;
	struct ae_hd_choices *choices = ae_host_server_choices();
	struct ae_hd_status const *status = ae_host_server_status();
	int state = ae_host_server_state(), mcc, games, tunnel, web, busy = state == AE_HD_STARTING;
	short count = 0;

	ae_host_server_checks(&mcc, &games, &tunnel, &web);
	if (running())
	{
		flow->rows[count++] = (struct ae_flow_row){ ROW_JOIN, -1, "JOIN MY SERVER", NULL,
			"Joins you to your server's game, like any player.", NULL };
		flow->rows[count++] = (struct ae_flow_row){ ROW_INVITE, -1, "COPY INVITE", "COPY LINK",
			"Copies its invite link: friends paste it in JOIN WITH INVITE.",
			status && status->invite[0] ? NULL : "LAN ONLY", .subscreen = 1 };
		flow->rows[count++] = (struct ae_flow_row){ ROW_WEB, -1, "OPEN ITS WEB PAGE", "THIS PC ONLY",
			"Its web page in your browser: players, bans, the playlist. The owner's password is in the save folder's "
			"server/owner.secret.", NULL, .subscreen = 1 };
	}
	flow->rows[count++] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "SERVER", NULL, NULL, NULL };
	snprintf(flow->value_texts[count], sizeof(flow->value_texts[count]), "%s", choices->name);
	flow->rows[count] = (struct ae_flow_row){ ROW_NAME, -1, "SERVER NAME", flow->value_texts[count],
		"Its name in lists and in the game: 15 characters. A types it.", NULL, .subscreen = 1 };
	flow->rows[count].status = running() || busy;
	if (flow->rows[count].status)
		flow->rows[count].subscreen = 0;
	count++;
	if (games)
		snprintf(flow->value_texts[count], sizeof(flow->value_texts[count]), "PLAYLIST.TXT  \xE2\x80\xA2  %d GAME%s", games,
			games == 1 ? "" : "S");
	else
		snprintf(flow->value_texts[count], sizeof(flow->value_texts[count]), "THE SERVER'S 3 GAMES");
	flow->rows[count] = (struct ae_flow_row){ ROW_PLAYLIST, -1, "PLAYLIST", flow->value_texts[count],
		"The games it plays in turn, saved as playlist.txt, the same file a server runs. A edits them.",
		NULL, .status = running() || busy };
	count++;
	flow->rows[count] = (struct ae_flow_row){ ROW_FIND, -1, "WHO CAN FIND IT", choices->public_listing ? "PUBLIC" :
		"PRIVATE", choices->public_listing ? "PUBLIC: found by its invite link and by its address (JOIN BY ADDRESS). The master servers do not list "
		"a server started here: that takes a publisher key (the hosting guide)." :
		"PRIVATE: your network's LAN list and its invite link. Never in the internet's lists.", NULL };
	flow->rows[count].status = running() || busy;
	count++;
	snprintf(flow->value_texts[count], sizeof(flow->value_texts[count]), "%d", choices->max_players);
	flow->rows[count] = (struct ae_flow_row){ ROW_PLAYERS, -1, "MAX PLAYERS", flow->value_texts[count],
		"The most players at once, bots counted: 2 to 16.", NULL };
	flow->rows[count].status = running() || busy;
	count++;
	ae_host_dedicated_bots_word(choices->bots, flow->value_texts[count], (int)sizeof(flow->value_texts[count]));
	flow->rows[count] = (struct ae_flow_row){ ROW_BOTS, -1, "BOTS FILL EMPTY SPOTS", flow->value_texts[count],
		"Bots play until this many are in, people and bots; a person who joins takes a bot's place at its next death. "
		"Bots are never rated.", NULL };
	flow->rows[count].status = running() || busy;
	count++;
	if (!running())
	{
		flow->rows[count++] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "THIS PC", NULL, NULL, NULL };
		flow->rows[count] = (struct ae_flow_row){ ROW_JOIN_READY, -1, "JOIN IT WHEN READY",
			choices->join_when_ready ? "YES" : "NO", choices->join_when_ready ? "You join it the moment it is up, like any "
			"player." : "It starts on its own; JOIN MY SERVER joins you to it later.", NULL };
		flow->rows[count].status = busy;
		count++;
	}
	if (running())
		flow->rows[count++] = (struct ae_flow_row){ ROW_STOP, -1, "STOP SERVER", NULL,
			"Ends it for everyone on it: they go back to their menus.", NULL, .primary = 1, .danger = 1 };
	else if (busy)
		flow->rows[count++] = (struct ae_flow_row){ ROW_CANCEL, -1, "CANCEL", NULL,
			"Stops the start: the server ends before anyone joins.", NULL, .primary = 1 };
	else
		flow->rows[count++] = (struct ae_flow_row){ ROW_START, -1, "START SERVER", NULL,
			"Starts the server on this PC, then joins you to it.",
			!ae_host_server_supported() ? "NOT IN THIS BUILD YET" : !tunnel ? "NO FREE PORT" : NULL, .primary = 1 };
	flow->count = count;
	/* (-1: the page's first draw puts it on the pinned row) */
	if (flow->focus >= count)
		flow->focus = (short)(count - 1);
	/* (past a header) */
	while (flow->focus >= 0 && flow->focus < count - 1 && flow->rows[flow->focus].kind == AE_FLOW_ROW_HEADER)
		flow->focus++;
}

/* the right panel's lines: a check (a tick or a cross and its words) */
static float check_line(float x, float y, float width, int good, char const *text)
{
	float size = u(18.0f), mark = u(14.0f);
	char fitted[160];

	/* (a dot: green ready, red not, muted neither; the words say which) */
	{
		float dot = u(good ? 10.0f : 6.0f);

		ae_draw_rect(x + (mark - dot) * 0.5f, y - dot * 0.5f, dot, dot, dot * 0.5f,
			good > 0 ? RUNNING_GREEN : good < 0 ? AE_COLOR_DESTRUCTIVE_INK : AE_COLOR_MUTED);
	}
	ae_fit_text(AE_FONT_BODY, size, text, width - mark * 2.0f, fitted, sizeof(fitted));
	ae_draw_text(AE_FONT_BODY, size, x + mark * 1.9f, ae_flow_cap_top(AE_FONT_BODY, size, y), AE_ALIGN_LEFT, AE_COLOR_TEXT,
		fitted);
	return u(34.0f);
}

static float pair_line(float x, float y, float width, char const *label, char const *value)
{
	float size = u(18.0f);
	char fitted[96];

	ae_draw_text(AE_FONT_BODY, size, x, ae_flow_cap_top(AE_FONT_BODY, size, y), AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	ae_fit_text(AE_FONT_ROW, size, value, width * 0.62f, fitted, sizeof(fitted));
	ae_draw_text(AE_FONT_ROW, size, x + width, ae_flow_cap_top(AE_FONT_ROW, size, y), AE_ALIGN_RIGHT, AE_COLOR_TITLE, fitted);
	return u(32.0f);
}

static float wrapped(float x, float y, float width, float size, unsigned int color, char const *text)
{
	char lines[4][160];
	int count = ae_wrap_text(AE_FONT_BODY, size, text, width, lines, 4), index;

	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, size, x, ae_flow_cap_top(AE_FONT_BODY, size, y + index * size * 1.45f), AE_ALIGN_LEFT,
			color, lines[index]);
	return count * size * 1.45f;
}

static void panel_draw(void)
{
	struct ae_flow_screen *flow = &page.flow;
	struct ae_frame const *frame = &flow->frame;
	struct ae_hd_status const *status = ae_host_server_status();
	struct ae_hd_running const *run = ae_host_server_running();
	int state = ae_host_server_state(), mcc, games, tunnel, web;
	float x = frame->rect.x + AE_FLOW_PANEL_X * frame->rect.width + u(23.0f), right = ae_flow_right_x(flow) - u(23.0f);
	float width = right - x, top = ae_frame_y(frame, AE_FLOW_CONTENT_Y), pad = u(PANEL_PAD_U), y, inner = width - 2.0f * pad;
	float body = u(18.0f), head = u(PANEL_HEAD_U), height;
	char text[200], value[96];
	char const *word;
	unsigned int word_color;

	ae_host_server_checks(&mcc, &games, &tunnel, &web);
	switch (state)
	{
	case AE_HD_READY: word = "RUNNING"; word_color = RUNNING_GREEN; break;
	case AE_HD_STARTING: word = "STARTING"; word_color = AE_COLOR_ACCENT; break;
	case AE_HD_STOPPING: word = "STOPPING"; word_color = AE_COLOR_ACCENT; break;
	case AE_HD_FAILED: word = "DID NOT START"; word_color = AE_COLOR_DESTRUCTIVE_INK; break;
	default: word = "Not running"; word_color = AE_COLOR_MUTED; break;
	}
	height = state == AE_HD_READY ? u(350.0f) : state == AE_HD_FAILED ? u(320.0f) : state == AE_HD_STARTING ? u(290.0f) :
		u(372.0f);
	ae_draw_rect(x, top, width, height, 0.0f, PANEL_FILL);
	ae_draw_rect(x, top, width, head, 0.0f, PANEL_HEAD);
	if (state == AE_HD_FAILED)
		ae_draw_outline(x, top, width, height, 0.0f, u(2.0f), FAILED_EDGE);
	ae_draw_text(AE_FONT_ROW, u(18.0f), x + u(16.0f), ae_flow_cap_top(AE_FONT_ROW, u(18.0f), top + head * 0.5f),
		AE_ALIGN_LEFT, AE_COLOR_TITLE, "SERVER");
	ae_draw_text(state == AE_HD_IDLE ? AE_FONT_BODY : AE_FONT_ROW, u(16.0f), right - u(16.0f),
		ae_flow_cap_top(AE_FONT_ROW, u(16.0f), top + head * 0.5f), AE_ALIGN_RIGHT, word_color, word);
	if (state == AE_HD_READY)
		ae_draw_rect(right - u(16.0f) - ae_draw_text_width(AE_FONT_ROW, u(16.0f), word) - u(16.0f),
			top + head * 0.5f - u(5.0f), u(10.0f), u(10.0f), u(5.0f), RUNNING_GREEN);
	ae_hit_add(x, top, width, height, HIT_PANEL, AE_PART_CARD, -1);
	x += pad;
	y = top + head + pad + u(8.0f);
	if (state == AE_HD_READY && status)
	{
		ae_fit_text(AE_FONT_ROW, u(26.0f), status->name[0] ? status->name : (run ? run->name : ""), inner, value,
			sizeof(value));
		ae_draw_text(AE_FONT_ROW, u(26.0f), x, ae_flow_cap_top(AE_FONT_ROW, u(26.0f), y + u(6.0f)), AE_ALIGN_LEFT,
			AE_COLOR_TITLE, value);
		y += u(52.0f);
		ae_ingame_up_text((unsigned int)(status->uptime / 60), value, (int)sizeof(value));
		y += pair_line(x, y, inner, "Up for", value);
		snprintf(value, sizeof(value), "%d of %d", status->players, status->max_players);
		y += pair_line(x, y, inner, "Players", value);
		{
			char map[48], gametype[48];

			ae_server_ingame_map_caption(status->map, map, (int)sizeof(map));
			ae_ingame_gametype_word(status->gametype, gametype, (int)sizeof(gametype));
			snprintf(value, sizeof(value), "%s  \xE2\x80\xA2  %s", map, gametype);
			y += pair_line(x, y, inner, !strcmp(status->state, "lobby") ? "In its lobby" : "Now", value);
		}
		y += pair_line(x, y, inner, "Found by", ae_host_server_choices()->public_listing ? "browser, invite and LAN" :
			status->invite[0] ? "invite and LAN" : "LAN");
		snprintf(value, sizeof(value), "%d (game), %d (web page, this PC only)", run ? run->tunnel_port : 0,
			run ? run->web_port : 0);
		y += pair_line(x, y, inner, "Ports", value);
		y += u(14.0f);
		wrapped(x, y, inner, body * 0.9f, AE_COLOR_MUTED, "It keeps running when you leave the game or close Arena "
			"Evolved. STOP SERVER ends it for everyone on it.");
		return;
	}
	if (state == AE_HD_READY || state == AE_HD_STOPPING)
	{
		wrapped(x, y, inner, body, AE_COLOR_TEXT, state == AE_HD_STOPPING ? "It is stopping: the players on it go back to "
			"their menus." : "It is up. Its status shows here in a moment.");
		return;
	}
	if (state == AE_HD_FAILED)
	{
		y += wrapped(x, y, inner, u(20.0f), AE_COLOR_TITLE, ae_host_server_reason()[0] ? ae_host_server_reason() :
			"The server did not start.");
		y += u(18.0f);
		y += wrapped(x, y, inner, body * 0.9f, AE_COLOR_TEXT, "Look at its output for why, fix it, and START SERVER again:");
		y += u(8.0f);
		wrapped(x, y, inner, body * 0.85f, AE_COLOR_MUTED, ae_host_server_log_path());
		return;
	}
	if (state == AE_HD_STARTING)
	{
		y += wrapped(x, y, inner, body, AE_COLOR_TEXT, "Starting on this PC with no window. You join it the moment it is "
			"up.") + u(20.0f);
		y += check_line(x, y, inner, 1, "Its settings written");
		snprintf(text, sizeof(text), "Started (game port %d, web page %d)", run ? run->tunnel_port : 0, run ?
			run->web_port : 0);
		y += check_line(x, y, inner, 1, text);
		y += check_line(x, y, inner, status && ae_host_dedicated_status_ready(status) ? 1 : 0, status ?
			"Loading its first map" : "Waiting for its first status");
		return;
	}
	y += wrapped(x, y, inner, body, AE_COLOR_TEXT, "Runs on this PC with no window. You join it like any player; it keeps "
		"going when you leave the game, until you stop it.") + u(18.0f);
	ae_draw_text_tracked(AE_FONT_BODY, u(14.0f), 0.12f, x, ae_flow_cap_top(AE_FONT_BODY, u(14.0f), y), AE_ALIGN_LEFT,
		AE_COLOR_MUTED, "READY TO START");
	y += u(30.0f);
	y += check_line(x, y, inner, mcc > 0 ? 1 : mcc < 0 ? -1 : 0, mcc > 0 ? "MCC files: found where SETTINGS points" :
		mcc < 0 ? "MCC files: not found where SETTINGS points" : "MCC files: none set (stock and community maps only)");
	snprintf(text, sizeof(text), games ? "Playlist: %d games from playlist.txt" : "Playlist: the server's own 3 games",
		games);
	y += check_line(x, y, inner, 1, text);
	if (tunnel)
		snprintf(text, sizeof(text), "Game port %d and web page port %d are free", tunnel, web);
	else
		snprintf(text, sizeof(text), "No free port: 2303-2330 and 8080-8099 are taken");
	y += check_line(x, y, inner, tunnel ? 1 : -1, text);
	y += u(14.0f);
	wrapped(x, y, inner, body * 0.85f, AE_COLOR_MUTED, "Each game in the playlist says who can join it. The next game is "
		"picked from the ones everyone on it can join.");
}

static void name_done(int keep, void *context)
{
	struct ae_hd_choices *choices = ae_host_server_choices();
	char kept[AE_HD_NAME];
	int length = 0, index;

	(void)context;
	page.naming = 0;
	if (!keep)
		return;
	for (index = 0; page.name.text[index] && length < AE_HD_NAME - 1; index++)
		if ((unsigned char)page.name.text[index] >= 0x20 && (unsigned char)page.name.text[index] < 0x7F)
			kept[length++] = page.name.text[index];
	kept[length] = 0;
	if (length)
		snprintf(choices->name, sizeof(choices->name), "%s", kept);
	platform_log("ae host server: name '%s'", choices->name);
}

static void name_type(void)
{
	struct ae_keyboard_spec spec;
	struct ae_flow_screen *flow = &page.flow;

	memset(&spec, 0, sizeof(spec));
	ae_text_init(&page.name, ae_host_server_choices()->name, AE_HD_NAME - 1, 0);
	spec.text = &page.name;
	spec.field = flow->rects[flow->focus];
	spec.bounds = flow->frame.rect;
	spec.density = flow->density;
	spec.done = name_done;
	if (ae_keyboard_open(&spec, AE_OWNER_ANY))
		page.naming = 1;
}

static void stop_picked(short choice, void *context)
{
	(void)context;
	page.confirm_stop = 0;
	if (choice != 1)
		return;
	if (ae_host_server_stop(0))
		note_set("Stopping it: the players on it go back to their menus.");
	else
		note_set("It could not be asked to stop.");
}

static void stop_ask(void)
{
	struct ae_dialog_spec spec;
	struct ae_hd_status const *status = ae_host_server_status();
	char body[200];

	memset(&spec, 0, sizeof(spec));
	snprintf(body, sizeof(body), status && status->players > 0 ? "It ends now for the %d player%s on it: they go back to "
		"their menus." : "It ends now. Nobody is on it.", status ? status->players : 0, status && status->players == 1 ?
		"" : "s");
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "Stop your server?";
	spec.body = body;
	spec.choices[0] = ae_string(AE_STR_CANCEL_CAPS);
	spec.choices[1] = "STOP SERVER";
	spec.choice_count = 2;
	spec.has_cancel = 1;
	spec.destructive_choices = 2u;
	spec.density = page.flow.density;
	spec.bounds = page.flow.frame.rect;
	spec.picked = stop_picked;
	if (ae_dialog_open(&spec, AE_OWNER_ANY))
		page.confirm_stop = 1;
}

static void row_step(short row, int direction)
{
	struct ae_hd_choices *choices = ae_host_server_choices();
	struct ae_flow_row const *r;

	if (row < 0 || row >= page.flow.count)
		return;
	r = &page.flow.rows[row];
	if (r->status || r->reason)
		return;
	switch (r->kind)
	{
	case ROW_FIND: choices->public_listing = !choices->public_listing; break;
	case ROW_PLAYERS: choices->max_players = ae_host_dedicated_players_step(choices->max_players, direction); break;
	case ROW_BOTS: choices->bots = ae_host_dedicated_bots_step(choices->bots, direction); break;
	case ROW_JOIN_READY: choices->join_when_ready = !choices->join_when_ready; break;
	default: return;
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void row_accept(short row)
{
	struct ae_flow_row const *r;
	struct ae_hd_status const *status = ae_host_server_status();

	if (row < 0 || row >= page.flow.count)
		return;
	r = &page.flow.rows[row];
	platform_log("ae host server: %s", r->label);
	if (r->reason)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	switch (r->kind)
	{
	case ROW_NAME:
		if (!r->status)
			name_type();
		else
			ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	case ROW_PLAYLIST:
		/* (0.7.9) CREATE GAME's PLAYLIST editor, the same playlist.txt; B comes back here */
		if (r->status || !ae_screen_create_game_playlist_only())
			ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	case ROW_FIND: case ROW_PLAYERS: case ROW_BOTS: case ROW_JOIN_READY:
		row_step(row, 1);
		return;
	case ROW_START:
		if (ae_host_server_start())
		{
			ae_sound_request(AE_SOUND_FORWARD, 0);
			note_set("Starting your server...");
		}
		else
			ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	case ROW_CANCEL:
		ae_host_server_stop(0);
		ae_sound_request(AE_SOUND_BACK, 0);
		return;
	case ROW_JOIN:
		if (ae_host_server_join())
		{
			ae_sound_request(AE_SOUND_FORWARD, 0);
			note_set("Joining your server...");
		}
		else
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			note_set(ae_host_server_reason());
		}
		return;
	case ROW_INVITE:
		if (status && status->invite[0])
		{
			ae_glue_clipboard_set(status->invite);
			ae_sound_request(AE_SOUND_FORWARD, 0);
			note_set("Its invite link is on your clipboard.");
		}
		return;
	case ROW_WEB:
		if (ae_host_server_web_url()[0] && ae_host_dedicated_open_url(ae_host_server_web_url()))
		{
			ae_sound_request(AE_SOUND_FORWARD, 0);
			note_set("Its web page opens in your browser (this PC only).");
		}
		else
			ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	case ROW_STOP:
		ae_sound_request(AE_SOUND_FORWARD, 0);
		stop_ask();
		return;
	}
}

static void host_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &page.flow;
	int state = ae_host_server_state();

	(void)instance;
	ae_flow_full_density(flow);
	if (state != page.was_state)
	{
		/* (running: JOIN MY SERVER first; a start or a stop: the pinned row) */
		page.was_state = state;
		flow->focus = state == AE_HD_READY ? 0 : -1;
	}
	rows_make();
	if (flow->focus < 0)
		flow->focus = (short)(flow->count - 1);
	ae_flow_title(flow, running() ? "MY SERVER" : "HOST A SERVER", page.crumb);
	/* (a passing word: what A just did, in the help's place) */
	if (page.note[0] && system_milliseconds() < page.note_until && flow->focus >= 0 && flow->focus < flow->count)
		flow->rows[flow->focus].help = page.note;
	ae_flow_draw_rows(flow, (AE_FLOW_PANEL_X - 0.07f) * flow->frame.rect.width - flow->frame.margin);
	panel_draw();
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	/* (0.7.9: left / right only where a row steps: not on SERVER NAME, PLAYLIST or the buttons) */
	if (!running() && state != AE_HD_STARTING && flow->focus >= 0 && flow->focus < flow->count &&
		(flow->rows[flow->focus].kind == ROW_FIND || flow->rows[flow->focus].kind == ROW_PLAYERS ||
		flow->rows[flow->focus].kind == ROW_BOTS || flow->rows[flow->focus].kind == ROW_JOIN_READY))
	{
		flow->prompts[flow->prompt_count] = ae_flow_prompt(AE_BUTTON_DPAD_LEFT, "Change", AE_ACTION_LEFT);
		flow->prompts[flow->prompt_count++].pair = 1;
	}
	ae_flow_draw_prompts(flow);
}

static int host_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_ACCEPT:
		row_accept(page.flow.focus);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		row_step(page.flow.focus, event->action == AE_ACTION_LEFT ? -1 : 1);
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(&page.flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		return 1;
	default:
		return 1;
	}
}

static void host_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	short row;

	if (ae_prompts_pointer(page.flow.prompts, page.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			host_handle(instance, &event);
		return;
	}
	row = ae_flow_rows_pointer(&page.flow, pointer);
	if (row == -2 || row == -3)
		row_step(page.flow.focus, row == -2 ? -1 : 1);
	else if (row >= 0)
		row_accept(row);
}

static void host_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae host server: page closed (%s)", running() ? "the server runs on" : "no server");
}

static struct ae_screen_class const host_class =
{
	.name = "host a server", .leave = host_leave, .handle = host_handle, .draw = host_draw, .pointer = host_pointer,
};

int ae_screen_host_server_open(char const *crumb)
{
	char name[AE_HD_NAME];
	struct ae_hd_choices *choices = ae_host_server_choices();

	memset(&page.flow, 0, sizeof(page.flow));
	page.flow.hit_rows = HIT_ROWS;
	page.flow.hit_prompts = HIT_PROMPTS;
	page.flow.focus = -1;
	page.was_state = ae_host_server_state();
	page.note[0] = 0;
	snprintf(page.crumb, sizeof(page.crumb), "%s", crumb ? crumb : "MULTIPLAYER");
	/* (the first time: the name of player 1's profile) */
	if (!strcmp(choices->name, "My Server"))
	{
		default_name(name, (int)sizeof(name));
		snprintf(choices->name, sizeof(choices->name), "%s", name);
	}
	if (!ae_ui_push(&host_class, AE_OWNER_ANY, &page))
		return 0;
	platform_log("ae host server: page open (%s)", running() ? "running" : "not running");
	return 1;
}

/* the MULTIPLAYER row's value and help (B-1 / B-2): "RUNNING  •  6/16" while a server of this PC runs */
int ae_screen_host_server_row(char *value, int size)
{
	struct ae_hd_status const *status = ae_host_server_status();

	if (!running())
	{
		if (ae_host_server_state() == AE_HD_STARTING)
		{
			snprintf(value, (size_t)size, "STARTING");
			return 1;
		}
		return 0;
	}
	if (status)
		snprintf(value, (size_t)size, "RUNNING  \xE2\x80\xA2  %d/%d", status->players, status->max_players);
	else
		snprintf(value, (size_t)size, "RUNNING");
	return 1;
}
