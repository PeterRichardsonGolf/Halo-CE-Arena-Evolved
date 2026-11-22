/*
AE_SCREEN_SOCIAL_LOBBY.C

See ae_screen_social_lobby.h. Layout (the approved mockups L-1 .. L-3, units u at 1080): the left column holds the status block
(head and elapsed time, a sub line, the bar, the RANGE strip) over the roster's 50 u rows; the right panel is the game card from
MATCH FOUND on.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/ae_shipped_files.h"
#include "ae_dev_mode.h"
#include "ae_glue_social.h"
#include "ae_glue_social_game.h"
#include "ae_glue_party.h"
#include "ae_party_model_rules.h"
#include "ae_rematch_rules.h"
#include "ae_screen_party_panel.h"
#include "ae_glue_mcc_maps.h"
#include "ae_glue_setup_maps.h"
#include "ae_input.h"
#include "ae_motion.h"
#include "ae_screen_social_lobby.h"
#include "ae_service_record.h"
#include "ae_setup_flow.h"
#include "ae_social_lobby_rules.h"
#include "ae_social_view_rules.h"
#include "ae_social_vote_rules.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "ae_dev_events.h"

void platform_log(char const *format, ...);

enum
{
	HIT_LOBBY = 0x7700,
	HIT_PROMPTS = HIT_LOBBY + 1,
	HIT_TILES = HIT_LOBBY + 2,
	VISIBLE_ROWS = 10,
	JOINED_MS = AE_SOCIAL_LOBBY_STEP_MS * 6
};

#define EXP_ICE 0xB8D6FFFFu
#define ROW_PITCH_U 56.0f
#define ROW_HEIGHT_U 50.0f

static struct
{
	struct ae_flow_screen flow;
	struct ae_social_plan plan;
	struct ae_social_lobby lobby;
	char names[AE_SOCIAL_SEATS][AE_SOCIAL_LOBBY_NAME];
	char you[AE_SOCIAL_LOCAL][48];
	unsigned long last_ms;      /* the clock's last frame */
	int last_stage;
	int started;                /* GO taken */
	int picture, picture_w, picture_h;
	int rank[AE_SOCIAL_LOCAL];
	int open;
	/* the map vote (vote_on: the lobby has one) */
	int vote_on;
	struct ae_social_vote vote;
	int applied;                /* the winner is the plan's map now */
	int logged[AE_SOCIAL_SEATS];    /* a bot's vote is in the log */
	struct
	{
		int picture, w, h;
		char caption[40];
		char type[AE_SOCIAL_GAMETYPE];
		char rules[72];
	} tile[AE_SOCIAL_VOTE_CHOICES];
	short focus[AE_SOCIAL_LOCAL];   /* each local player's tile */
	short hover;                    /* the tile under the pointer, -1 none */
	long vote_len;                  /* the vote's length: AE_SOCIAL_VOTE_MS, a rematch's AE_REMATCH_VOTE_MS */
	/* (rc5) the REMATCH: the lobby after a game, its roster kept */
	struct
	{
		int on, online, dev, decided, hold;
		struct ae_social_last last;
		struct { int fresh, ping, open, staying, kills, deaths; } seat[AE_SOCIAL_SEATS];
		char news[AE_REMATCH_NEWS_LINES][96];
		int news_count, news_accent;
		short roster_focus;         /* the roster seat the focus is on (-1: the vote) */
		struct ae_still_there still;
		int input_seen;
	} pg;
	int dev_left;                   /* DEV MODE: seconds of the vote left when it opens (-1: all) */
	int dev_votes[AE_SOCIAL_VOTE_CHOICES];
	int dev_has_votes;
} lobby;

static struct ae_screen_class const lobby_class;

/* the Still-there counter lives on from one rematch lobby to the next */
static struct ae_still_there still_keep;
static int dev_hold_next = -1;
static int dev_staged_next;     /* the next lobby is a DEV MODE scene: its game is not remembered */

void ae_screen_social_lobby_dev_hold(int seconds)
{
	dev_hold_next = seconds >= 0 || seconds == AE_SOCIAL_LOBBY_DEV_HOLD_STARTING ? seconds : -1;
}

/* ---------- the vote's words and looks */

/* the roster's rows carry their team's edge: from STARTING on, and all through a rematch */
static int team_edges(void)
{
	return lobby.pg.on || lobby.lobby.stage == AE_SOCIAL_STAGE_STARTING || lobby.lobby.stage == AE_SOCIAL_STAGE_GO ||
		lobby.lobby.stage == AE_SOCIAL_STAGE_CANCELLED;
}

static int teams_shown(void)
{
	int stage = lobby.lobby.stage;

	return stage == AE_SOCIAL_STAGE_STARTING || stage == AE_SOCIAL_STAGE_GO || stage == AE_SOCIAL_STAGE_CANCELLED;
}

/* the map vote is on screen (VOTE and the winner moment) */
static int vote_view(void)
{
	return lobby.vote_on && (lobby.lobby.stage == AE_SOCIAL_STAGE_VOTE || lobby.lobby.stage == AE_SOCIAL_STAGE_WINNER);
}

/* the vote has begun (its stage, or after it): the range strip of the search is over */
static int vote_begun(void)
{
	return lobby.vote_on && (vote_view() || lobby.lobby.vote_end >= 0);
}

/* "cold_shoulder@smp" -> "COLD SHOULDER"; a Steam Workshop map ("45@ws3477809506") by its title, as the map lists show it */
static void map_caption(char const *file, char *out, size_t size)
{
	size_t at = 0;
	char title[64];

	if (strstr(file, "@ws") && ae_mcc_map_title(file, title, (int)sizeof(title)))
	{
		for (file = title; *file && at + 1 < size; file++)
			out[at++] = (char)((*file >= 'a' && *file <= 'z') ? *file - 32 : *file);
		out[at] = 0;
		return;
	}

	for (; *file && *file != '@' && at + 1 < size; file++)
		out[at++] = (char)((*file >= 'a' && *file <= 'z') ? *file - 32 : *file == '_' ? ' ' : *file);
	out[at] = 0;
}

static unsigned int with_alpha(unsigned int rgba, float alpha)
{
	return (rgba & 0xFFFFFF00u) | (unsigned int)((float)(rgba & 0xFFu) * alpha + 0.5f);
}

static unsigned int slot_color(int slot)
{
	static unsigned int const colors[AE_SOCIAL_LOCAL] = { 0xC0392BFFu, 0x2E86DEFFu, 0x7FA035FFu, 0xE6B422FFu };

	return colors[slot >= 0 && slot < AE_SOCIAL_LOCAL ? slot : 0];
}

/* "4 V 4" / "8 PLAYERS" of a playlist */
static void shape_line(struct ae_social_plan const *plan, char *line, size_t size)
{
	struct ae_social_playlist const *list = ae_social_playlist_by_id(plan->playlist);

	line[0] = 0;
	if (!list)
		return;
	if (list->teams == 2)
		snprintf(line, size, "%d V %d", list->size, list->size);
	else
		snprintf(line, size, "%d PLAYERS", list->size);
}

/* ---------- the rows */

enum { ITEM_SEAT, ITEM_HEADER, ITEM_EMPTY, ITEM_MORE };
struct item { int kind, seat, team; };

/* the roster as drawn now: before STARTING your party first, then the bots as they joined, empty seats at the end; from STARTING the
seats group under their team's header */
static int items_make(struct item *items, int max)
{
	struct ae_social_roster const *roster = &lobby.plan.roster;
	int count = 0, seat, order, team;

	if (lobby.pg.on)
	{
		/* (the kept roster: each team's seats, the local players and the party first, an open seat where a seat is free) */
		for (team = 0; team < (roster->teams > 0 ? roster->teams : 1); team++)
		{
			int pass, shown_team = roster->teams > 0 ? (roster->seat[0].team + team) % roster->teams : 0;

			for (pass = 0; pass < 2; pass++)
				for (seat = 0; seat < roster->count && count < max; seat++)
				{
					int mine = seat < roster->humans || lobby.plan.party_seat[seat];

					if ((roster->teams > 0 ? roster->seat[seat].team : 0) != shown_team || mine == (pass == 1))
						continue;
					items[count++] = (struct item){ lobby.pg.seat[seat].open ? ITEM_EMPTY : ITEM_SEAT, seat, shown_team };
				}
		}
		return count;
	}
	if (teams_shown() && roster->teams > 0)
	{
		for (team = 0; team < roster->teams && count < max; team++)
		{
			items[count++] = (struct item){ ITEM_HEADER, -1, team };
			for (seat = 0; seat < roster->count && count < max; seat++)
				if (roster->seat[seat].team == team)
					items[count++] = (struct item){ ITEM_SEAT, seat, team };
		}
		return count;
	}
	for (seat = 0; seat < roster->humans && count < max; seat++)
		items[count++] = (struct item){ ITEM_SEAT, seat, 0 };
	for (order = 0; order < lobby.lobby.order_count && count < max; order++)
		if (ae_social_lobby_seat_in(&lobby.lobby, lobby.lobby.order[order]))
			items[count++] = (struct item){ ITEM_SEAT, lobby.lobby.order[order], 0 };
	while (count < roster->count && count < max)
		items[count++] = (struct item){ ITEM_EMPTY, -1, 0 };
	return count;
}

static void seat_name(int seat, char *name, size_t size)
{
	struct ae_social_roster const *roster = &lobby.plan.roster;

	if (seat < roster->humans)
		snprintf(name, size, "%s", lobby.you[seat]);
	else
		snprintf(name, size, "%s", lobby.names[seat - roster->humans]);
}

static unsigned int team_color(int team)
{
	return team == 0 ? 0xC0392BFFu : team == 1 ? 0x2E86DEFFu : 0x7FA035FFu;
}

static void ctext(struct ae_flow_screen *flow, int font, float size_u, float x, float center_y, int align, unsigned int color,
	char const *text)
{
	float size = ae_flow_u(flow, size_u);

	ae_draw_text(font, size, x, ae_flow_cap_top(font, size, center_y), align, color, text);
}

static void tracked(struct ae_flow_screen *flow, float size_u, float x, float center_y, int align, unsigned int color,
	char const *text)
{
	float size = ae_flow_u(flow, size_u);

	ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x, ae_flow_cap_top(AE_FONT_BODY, size, center_y), align, color, text);
}

/* a seat's tags in the order they are drawn: up to three */
static int seat_tags(int seat, int *tags)
{
	struct ae_social_roster const *roster = &lobby.plan.roster;
	int human = seat < roster->humans, count = 0, party_bot = !human && lobby.plan.party_seat[seat];

	if (lobby.pg.on)
	{
		if (seat == 0)
		{
			tags[count++] = AE_TAG_YOU;
			tags[count++] = AE_TAG_LEADER;
		}
		else if (human || party_bot)
			tags[count++] = AE_TAG_PARTY;
		if (lobby.pg.seat[seat].fresh)
			tags[count++] = AE_TAG_NEW;
		if (lobby.pg.seat[seat].staying)
			tags[count++] = AE_TAG_STAYING;
		else if (lobby.pg.online && !human && !party_bot && !lobby.pg.seat[seat].fresh && lobby.pg.seat[seat].ping < 0)
			tags[count++] = AE_TAG_BOT;
		return count;
	}
	if (seat == 0)
		tags[count++] = AE_TAG_YOU;
	return count;
}

static void roster_draw(float x, float y, float width)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct item items[AE_SOCIAL_SEATS + 4];
	int count = items_make(items, AE_SOCIAL_SEATS + 4), shown, at, visible = lobby.pg.on ? 8 : VISIBLE_ROWS;
	float pitch = ae_flow_u(flow, ROW_PITCH_U), height = ae_flow_u(flow, ROW_HEIGHT_U), emblem = ae_flow_u(flow, 32.0f);
	float kd_right = x + width - ae_flow_u(flow, 20.0f) - ae_flow_u(flow, 96.0f), ping_right = kd_right - ae_flow_u(flow, 104.0f);
	struct ae_social_roster const *roster = &lobby.plan.roster;

	shown = count > visible ? visible : count;
	if (lobby.pg.on)
	{
		/* (the column heads over the rows) */
		float mid = y - ae_flow_u(flow, 20.0f);

		if (lobby.pg.online)
			tracked(flow, 14.0f, ping_right, mid, AE_ALIGN_RIGHT, AE_COLOR_MUTED, "PING");
		tracked(flow, 14.0f, kd_right, mid, AE_ALIGN_RIGHT, AE_COLOR_MUTED, "K / D");
		tracked(flow, 14.0f, x + width - ae_flow_u(flow, 20.0f), mid, AE_ALIGN_RIGHT, AE_COLOR_MUTED, "VOTE");
	}
	for (at = 0; at < shown; at++)
	{
		struct item const *it = &items[at];
		float top = y + pitch * (float)at, mid = top + height * 0.5f;

		if (it->kind == ITEM_HEADER)
		{
			ae_draw_rect(x, top + height * 0.25f, ae_flow_u(flow, 6.0f), height * 0.5f, 0.0f, team_color(it->team));
			tracked(flow, 18.0f, x + ae_flow_u(flow, 18.0f), mid, AE_ALIGN_LEFT, AE_COLOR_MUTED, it->team == 0 ? "RED TEAM" : "BLUE TEAM");
			continue;
		}
		if (it->kind == ITEM_EMPTY)
		{
			if (lobby.pg.on)
			{
				ae_draw_rect(x, top, width, height, 0.0f, AE_COLOR_ROW);
				ae_draw_rect(x, top, ae_flow_u(flow, 6.0f), height, 0.0f, team_color(it->team));
				ctext(flow, AE_FONT_BODY, 21.0f, x + ae_flow_u(flow, 64.0f), mid, AE_ALIGN_LEFT, AE_COLOR_MUTED,
					lobby.pg.online ? "Open seat. The queue or a bot fills it." : "Open seat. A bot fills it.");
				continue;
			}
			ae_draw_outline(x, top, width, height, 0.0f, ae_flow_u(flow, 1.0f), AE_COLOR_RULE);
			ctext(flow, AE_FONT_BODY, 21.0f, x + ae_flow_u(flow, 20.0f), mid, AE_ALIGN_LEFT, AE_COLOR_MUTED, "Looking for a player");
			continue;
		}
		{
			int seat = it->seat, human = seat < roster->humans, fresh = !human && ae_social_lobby_seat_new(&lobby.lobby, seat);
			int ballot = vote_view() && seat < lobby.vote.seats ? lobby.vote.ballot[seat] : -1;
			int focused = lobby.pg.on && lobby.pg.roster_focus == seat, tags[3], tag_count, each, party_bot = !human &&
				lobby.plan.party_seat[seat];
			float right_x = x + width - ae_flow_u(flow, 20.0f);
			char name[AE_SOCIAL_LOBBY_NAME + 4], initial[8], right[40];
			unsigned int emblem_color = human ? (it->team == 1 ? 0x2E86DEFFu : 0xC0392BFFu) : 0x3A4651FFu;
			unsigned int ink = focused ? AE_COLOR_SELECTION_TEXT : human ? AE_COLOR_TITLE : AE_COLOR_TEXT;
			float name_x = x + ae_flow_u(flow, 64.0f), name_w, tag_x;

			seat_name(seat, name, sizeof(name));
			if (focused)
			{
				ae_draw_rect(x, top, width, height, 0.0f, AE_COLOR_SELECTION);
				ae_draw_rect(x, top, ae_flow_u(flow, 6.0f), height, 0.0f, AE_COLOR_ACCENT);
			}
			else
			{
				ae_draw_rect(x, top, width, height, 0.0f, AE_COLOR_ROW);
				/* (a local player's row a shade brighter than a bot's) */
				if (human)
					ae_draw_rect(x, top, width, height, 0.0f, AE_COLOR_HOVER);
			}
			if (!focused && (team_edges() && roster->teams > 0))
				ae_draw_rect(x, top, ae_flow_u(flow, 6.0f), height, 0.0f, team_color(it->team));
			/* (during the vote the ice edge is a bot's new vote, not a join) */
			if (vote_view() && !lobby.pg.on)
				fresh = !human && ae_social_vote_fresh(&lobby.vote, seat);
			if (fresh && !lobby.pg.on)
				ae_draw_rect(x, top, ae_flow_u(flow, 6.0f), height, 0.0f, EXP_ICE);
			if (human && roster->teams > 0)
				emblem_color = team_color(roster->seat[seat].team);
			ae_draw_rect(x + ae_flow_u(flow, 20.0f), mid - emblem * 0.5f, emblem, emblem, ae_flow_u(flow, 2.0f), emblem_color);
			ae_party_initial(name, initial, (int)sizeof(initial));
			ctext(flow, AE_FONT_ROW, 20.0f, x + ae_flow_u(flow, 20.0f) + emblem * 0.5f, mid, AE_ALIGN_CENTER, AE_COLOR_TITLE, initial);
			ctext(flow, AE_FONT_ROW, 26.0f, name_x, mid, AE_ALIGN_LEFT, ink, name);
			name_w = ae_draw_text_width(AE_FONT_ROW, ae_flow_u(flow, 26.0f), name);
			tag_x = name_x + name_w + ae_flow_u(flow, 14.0f);
			tag_count = seat_tags(seat, tags);
			for (each = 0; each < tag_count; each++)
				tag_x += ae_party_tag_draw_left(flow, tag_x, mid, tags[each], focused) + ae_flow_u(flow, 8.0f);
			if (fresh && !vote_view() && !lobby.pg.on)
				ctext(flow, AE_FONT_ROW, 16.0f, tag_x, mid, AE_ALIGN_LEFT, EXP_ICE, "JOINED");
			if (focused && !human && !party_bot)
			{
				/* (PARTY UP: the key's square and the word on an ice-blue chip, left of the columns) */
				float chip_w = ae_flow_u(flow, 168.0f), chip_h = ae_flow_u(flow, 36.0f), chip_x = tag_x + ae_flow_u(flow, 8.0f);
				float room = (lobby.pg.online ? ping_right - ae_flow_u(flow, 80.0f) : kd_right - ae_flow_u(flow, 80.0f)) - chip_x;

				/* (never over the name: a narrow row keeps only the key's square) */
				if (room < ae_flow_u(flow, 150.0f))
					chip_w = ae_flow_u(flow, 44.0f);
				ae_draw_rect(chip_x, mid - chip_h * 0.5f, chip_w, chip_h, 0.0f, AE_COLOR_ACCENT);
				ae_draw_rect(chip_x + ae_flow_u(flow, 8.0f), mid - ae_flow_u(flow, 12.0f), ae_flow_u(flow, 24.0f), ae_flow_u(flow, 24.0f), 0.0f,
					AE_COLOR_SELECTION_TEXT);
				ctext(flow, AE_FONT_ROW, 16.0f, chip_x + ae_flow_u(flow, 20.0f), mid, AE_ALIGN_CENTER, AE_COLOR_TITLE, "X");
				if (chip_w >= ae_flow_u(flow, 150.0f))
					tracked(flow, 17.0f, chip_x + ae_flow_u(flow, 44.0f), mid, AE_ALIGN_LEFT, AE_COLOR_SELECTION_TEXT, "PARTY UP");
			}
			if (lobby.pg.on)
			{
				char kd[24];
				int kills = lobby.pg.seat[seat].kills, deaths = lobby.pg.seat[seat].deaths;

				if (kills >= 0 && deaths >= 0)
					snprintf(kd, sizeof(kd), "%d / %d", kills, deaths);
				else
					snprintf(kd, sizeof(kd), "-");
				ctext(flow, AE_FONT_ROW, 24.0f, kd_right, mid, AE_ALIGN_RIGHT, kills >= 0 ? (focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TEXT) :
					AE_COLOR_MUTED, kd);
				if (lobby.pg.online && lobby.pg.seat[seat].ping >= 0)
				{
					snprintf(kd, sizeof(kd), "%d ms", lobby.pg.seat[seat].ping);
					ctext(flow, AE_FONT_BODY, 18.0f, ping_right, mid, AE_ALIGN_RIGHT, focused ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED, kd);
				}
				/* (the vote's mark: the letter of the map this seat voted for) */
				{
					float side = ae_flow_u(flow, 26.0f);

					if (vote_view() && seat < lobby.vote.seats && lobby.vote.ballot[seat] >= 0)
					{
						char letter[2];

						letter[0] = (char)('A' + lobby.vote.ballot[seat]);
						letter[1] = 0;
						ae_draw_rect(right_x - side, mid - side * 0.5f, side, side, 0.0f, AE_COLOR_ACCENT);
						ctext(flow, AE_FONT_ROW, 16.0f, right_x - side * 0.5f, mid, AE_ALIGN_CENTER, AE_COLOR_SELECTION_TEXT, letter);
					}
					else
						ae_draw_rect(right_x - side, mid - side * 0.5f, side, side, 0.0f, AE_COLOR_WELL);
				}
				continue;
			}
			if (human)
				ae_social_view_rank(lobby.rank[seat < AE_SOCIAL_LOCAL ? seat : 0], right, (int)sizeof(right));
			else if (ae_dev_mode_on())
				snprintf(right, sizeof(right), "BOT \xC2\xB7 T%d", roster->seat[seat].tier);
			else
				snprintf(right, sizeof(right), "BOT");
			if (ballot >= 0 && ballot < lobby.vote.count)
			{
				/* (the voted map: its 48 x 27 u picture, its name, a dot, then the seat's tag) */
				float pic_w = ae_flow_u(flow, 48.0f), pic_h = ae_cover_frame_height(pic_w), cap_w;

				if (lobby.tile[ballot].picture)
					ae_draw_image_fill(lobby.tile[ballot].picture, right_x - pic_w, mid - pic_h * 0.5f, pic_w, pic_h, 0xFFFFFFFFu);
				else
					ae_draw_rect(right_x - pic_w, mid - pic_h * 0.5f, pic_w, pic_h, 0.0f, AE_COLOR_ROW);
				ae_draw_outline(right_x - pic_w, mid - pic_h * 0.5f, pic_w, pic_h, 0.0f, ae_flow_u(flow, 1.0f), AE_COLOR_RULE);
				right_x -= pic_w + ae_flow_u(flow, 12.0f);
				tracked(flow, 18.0f, right_x, mid, AE_ALIGN_RIGHT, human ? AE_COLOR_TITLE : AE_COLOR_TEXT, lobby.tile[ballot].caption);
				cap_w = ae_draw_text_tracked_width(AE_FONT_BODY, ae_flow_u(flow, 18.0f), 0.09f, lobby.tile[ballot].caption);
				right_x -= cap_w + ae_flow_u(flow, 16.5f);
				ae_widget_separator(&flow->density, right_x, mid, 5.0f, AE_COLOR_MUTED);
				right_x -= ae_flow_u(flow, 16.5f);
			}
			tracked(flow, 18.0f, right_x, mid, AE_ALIGN_RIGHT, human ? AE_COLOR_TEXT : AE_COLOR_MUTED, right);
		}
	}
	if (count > shown)
	{
		char more[40];

		snprintf(more, sizeof(more), "\xE2\x96\xBC %d more", count - shown);
		tracked(flow, 16.0f, x, y + pitch * (float)shown + ae_flow_u(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, more);
	}
}

/* ---------- the status block */

static float level_position(int shown_level)
{
	float at = (float)(shown_level - 1) / 49.0f;

	return at < 0.0f ? 0.0f : at > 1.0f ? 1.0f : at;
}

static void range_draw(float x, float y, float width)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_social_roster const *roster = &lobby.plan.roster;
	int step = ae_social_lobby_step(&lobby.lobby), span = lobby.lobby.span > 0 ? lobby.lobby.span : 1, low = 1, high = 50;
	float centre = level_position(roster->player_level), a, b, t, h = ae_flow_u(flow, 6.0f);

	ae_social_match_window(lobby.lobby.tier_level, &low, &high);
	t = (float)step / (float)span;
	a = centre + (level_position(low) - centre) * t;
	b = centre + (level_position(high) - centre) * t;
	ae_draw_rect(x, y, width, h, 0.0f, AE_COLOR_RULE);
	ae_draw_rect(x + width * a, y, width * (b - a) + ae_flow_u(flow, 2.0f), h, 0.0f, AE_COLOR_ACCENT);
	ae_draw_rect(x + width * centre - ae_flow_u(flow, 1.0f), y - ae_flow_u(flow, 5.0f), ae_flow_u(flow, 3.0f), h + ae_flow_u(flow, 10.0f),
		0.0f, EXP_ICE);
	tracked(flow, 16.0f, x, y + ae_flow_u(flow, 32.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, "EASIER");
	tracked(flow, 16.0f, x + width * 0.5f, y + ae_flow_u(flow, 32.0f), AE_ALIGN_CENTER, AE_COLOR_MUTED, "SEARCH RANGE");
	tracked(flow, 16.0f, x + width, y + ae_flow_u(flow, 32.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED, "HARDER");
}

/* half votes as text: 5 -> "2.5", 4 -> "2" */
static void votes_text(char *out, size_t size, int half_votes)
{
	if (half_votes & 1)
		snprintf(out, size, "%d.5", half_votes / 2);
	else
		snprintf(out, size, "%d", half_votes / 2);
}

static long lobby_vote_length(void)
{
	return lobby.vote_len > 0 ? lobby.vote_len : AE_SOCIAL_VOTE_MS;
}

static void status_draw(float x, float y, float width)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_social_lobby const *l = &lobby.lobby;
	char head[48], sub[96], clock[16];
	long seconds = l->elapsed / 1000, left;
	float bar_h = ae_flow_u(flow, 6.0f), fraction = 0.0f;
	int joined = ae_social_lobby_joined(l), warn = 0;

	switch (l->stage)
	{
	case AE_SOCIAL_STAGE_SEARCHING:
		snprintf(head, sizeof(head), "FINDING BOTS");
		if (l->fixed)
			snprintf(sub, sizeof(sub), "Bots: %s (fixed)", ae_social_skill_name(lobby.plan.mode));
		else
			snprintf(sub, sizeof(sub), "%s", ae_social_lobby_step(l) * 2 >= (l->span > 0 ? l->span : 1) ? "Widening the search" :
				"Looking near your skill");
		break;
	case AE_SOCIAL_STAGE_FOUND:
		snprintf(head, sizeof(head), "OFFLINE MATCH FOUND");
		snprintf(sub, sizeof(sub), "Waiting for players  %d / %d", joined, l->seats);
		fraction = (float)joined / (float)(l->seats > 0 ? l->seats : 1);
		break;
	case AE_SOCIAL_STAGE_VOTE:
		left = ae_social_vote_left_ms(&lobby.vote);
		if (lobby.pg.on)
		{
			int party = lobby.plan.roster.humans + lobby.plan.party_bots, staying = 0, seat;

			for (seat = 0; seat < l->seats; seat++)
				staying += !lobby.pg.seat[seat].open && (!lobby.pg.online || seat < lobby.plan.roster.humans ||
					lobby.plan.party_seat[seat] || lobby.pg.seat[seat].staying);
			if (ae_still_there_asking(&lobby.pg.still))
			{
				snprintf(head, sizeof(head), "STILL THERE?");
				snprintf(sub, sizeof(sub), "Press A to play the next game.");
				fraction = 1.0f;
				break;
			}
			snprintf(head, sizeof(head), "NEXT GAME IN %ld", (left + 999) / 1000);
			ae_rematch_sub_text(lobby.pg.online, party, staying, l->seats, ae_social_vote_voted(&lobby.vote), l->seats, sub, (int)sizeof(sub));
		}
		else
		{
			snprintf(head, sizeof(head), "VOTE ENDS IN %ld", (left + 999) / 1000);
			snprintf(sub, sizeof(sub), "Votes in  %d / %d", ae_social_vote_voted(&lobby.vote), l->seats);
		}
		/* (reduced motion: the bar steps in whole seconds) */
		fraction = 1.0f - (float)(ae_motion_reduced() ? ((left + 999) / 1000) * 1000 : left) / (float)lobby_vote_length();
		warn = left <= 3000;
		break;
	case AE_SOCIAL_STAGE_WINNER:
	{
		int win = lobby.vote.winner >= 0 ? lobby.vote.winner : 0;
		char votes[12], of[12];

		votes_text(votes, sizeof(votes), ae_social_vote_score(&lobby.vote, win));
		votes_text(of, sizeof(of), ae_social_vote_total_weight(&lobby.vote));

		fraction = 1.0f;
		snprintf(head, sizeof(head), "NEXT MAP: %s", lobby.tile[win].caption);
		if (lobby.vote.tie)
			snprintf(sub, sizeof(sub), "A tie, drawn at random: %s with %s of %s votes.", lobby.tile[win].type, votes, of);
		else
			snprintf(sub, sizeof(sub), "%s with %s of %s votes. Starting in a moment.", lobby.tile[win].type, votes, of);
		break;
	}
	default:
		left = ae_social_lobby_countdown_ms(l);
		snprintf(head, sizeof(head), "STARTING IN %ld", (left + 999) / 1000);
		snprintf(sub, sizeof(sub), "Teams are set. A starts now.");
		fraction = 1.0f - (float)left / (float)AE_SOCIAL_LOBBY_STARTING_MS;
		break;
	}
	snprintf(clock, sizeof(clock), "%ld:%02ld", seconds / 60, seconds % 60);
	/* (a panel under the status words: they stay readable over the backdrop) */
	{
		int ranged = !l->fixed && !vote_begun() && l->stage <= AE_SOCIAL_STAGE_FOUND;

		ae_draw_rect(x, y - ae_flow_u(flow, 18.0f), width, ae_flow_u(flow, ranged ? 186.0f : 128.0f), 0.0f, AE_COLOR_PANEL);
		x += ae_flow_u(flow, 22.0f);
		width -= ae_flow_u(flow, 44.0f);
	}
	ctext(flow, AE_FONT_ROW, 32.0f, x, y + ae_flow_u(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, head);
	/* (the elapsed clock belongs to the search: from FOUND on the stage's own numbers are the ones that matter) */
	if (l->stage == AE_SOCIAL_STAGE_SEARCHING)
		ctext(flow, AE_FONT_BODY, 18.0f, x + width, y + ae_flow_u(flow, 16.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED, clock);
	ctext(flow, AE_FONT_BODY, 21.0f, x, y + ae_flow_u(flow, 56.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT, sub);
	/* (FOUND, VOTE and STARTING: the bar fills from the left as the stage goes on) */
	if (l->stage >= AE_SOCIAL_STAGE_FOUND)
	{
		ae_draw_rect(x, y + ae_flow_u(flow, 85.0f), width, bar_h, 0.0f, AE_COLOR_RULE);
		ae_draw_rect(x, y + ae_flow_u(flow, 85.0f), width * fraction, bar_h, 0.0f, warn ? AE_COLOR_WARNING : AE_COLOR_ACCENT);
	}
	if (!l->fixed && !vote_begun() && l->stage <= AE_SOCIAL_STAGE_FOUND)
		range_draw(x, y + ae_flow_u(flow, 120.0f), width);
}

/* ---------- the game card */

static void card_draw(float x, float y, float width)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_social_plan const *plan = &lobby.plan;
	float pad = ae_flow_u(flow, 23.0f), pic_w = ae_flow_u(flow, 340.0f), pic_h = ae_cover_frame_height(pic_w), height, foot;
	float side_x = x + pad + pic_w + ae_flow_u(flow, 24.0f), side_w = x + width - pad - side_x, row = 36.0f;
	int pack = strstr(plan->map, "@smp") != NULL, lines = 0, at;
	char map[AE_SOCIAL_MAP], line[120], kind[96], fitted[160], extra[3][64];
	struct ae_social_playlist const *list = ae_social_playlist_by_id(plan->playlist);

	/* (one layout for every map: the picture in its 16:9 frame, the words beside it, the bot skill and EXP below) */
	map_caption(plan->map, map, sizeof(map));
	line[0] = 0;
	if (list && list->teams == 2)
		snprintf(line, sizeof(line), "%d V %d", list->size, list->size);
	else if (list)
		snprintf(line, sizeof(line), "%d PLAYERS", list->size);
	snprintf(kind, sizeof(kind), "%s \xC2\xB7 TOURNAMENT", plan->label);
	/* (a line too long for the room beside the picture drops "TOURNAMENT") */
	if (ae_draw_text_tracked_width(AE_FONT_BODY, ae_flow_u(flow, 18.0f), 0.09f, kind) > side_w)
		snprintf(kind, sizeof(kind), "%s", plan->label);
	if (plan->start)
		snprintf(extra[lines++], sizeof(extra[0]), "%s", ae_social_start_label(plan->start));
	if (plan->ae_weapons)
		snprintf(extra[lines++], sizeof(extra[0]), "%s", plan->ae_weapons == 2 ? "MAP WEAPONS" : "AE WEAPONS");
	if (pack && lines < 3)
		snprintf(extra[lines++], sizeof(extra[0]), "SOCIAL MAP PACK");
	foot = 24.0f + pic_h / ae_flow_u(flow, 1.0f) + 28.0f;
	height = ae_flow_u(flow, foot + 90.0f);
	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_PANEL);
	ae_draw_rect(x, y, width, ae_flow_u(flow, 4.0f), 0.0f, AE_COLOR_ACCENT);
	if (lobby.picture)
		ae_draw_image_fill(lobby.picture, x + pad, y + ae_flow_u(flow, 24.0f), pic_w, pic_h, 0xFFFFFFFFu);
	else
		ae_draw_rect(x + pad, y + ae_flow_u(flow, 24.0f), pic_w, pic_h, 0.0f, AE_COLOR_WELL);
	tracked(flow, 18.0f, side_x, y + ae_flow_u(flow, row + 4.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, kind);
	ae_fit_text(AE_FONT_TITLE, ae_flow_u(flow, 40.0f), map, side_w, fitted, (int)sizeof(fitted));
	ae_draw_text(AE_FONT_TITLE, ae_flow_u(flow, 40.0f), side_x, ae_flow_cap_top(AE_FONT_TITLE, ae_flow_u(flow, 40.0f),
		y + ae_flow_u(flow, row + 48.0f)), AE_ALIGN_LEFT, AE_COLOR_TITLE, fitted);
	row += 88.0f;
	tracked(flow, 18.0f, side_x, y + ae_flow_u(flow, row), AE_ALIGN_LEFT, AE_COLOR_MUTED, line);
	for (at = 0; at < lines; at++)
	{
		row += 28.0f;
		tracked(flow, 18.0f, side_x, y + ae_flow_u(flow, row), AE_ALIGN_LEFT, AE_COLOR_MUTED, extra[at]);
	}
	ae_draw_rect(x + pad, y + ae_flow_u(flow, foot - 4.0f), width - 2.0f * pad, ae_flow_u(flow, 1.0f), 0.0f, AE_COLOR_RULE);
	tracked(flow, 18.0f, x + pad, y + ae_flow_u(flow, foot + 26.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, "BOT SKILL");
	ctext(flow, AE_FONT_ROW, 24.0f, x + width - pad, y + ae_flow_u(flow, foot + 26.0f), AE_ALIGN_RIGHT, AE_COLOR_ACCENT,
		plan->mode ? ae_social_skill_name(plan->mode) : "AUTO");
	tracked(flow, 18.0f, x + pad, y + ae_flow_u(flow, foot + 58.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, "A WIN OR A TIE");
	ctext(flow, AE_FONT_ROW, 24.0f, x + width - pad, y + ae_flow_u(flow, foot + 58.0f), AE_ALIGN_RIGHT, AE_COLOR_ACCENT, "+1 EXP");
}

/* ---------- the vote card */

/* an outlined badge ("YOUR VOTE", "NEXT MAP"): the text 13 u, the box 14 u wider than it, 20 u tall, centred on center_y */
static float badge_draw(float x, float center_y, unsigned int color, char const *text)
{
	struct ae_flow_screen *flow = &lobby.flow;
	float size = ae_flow_u(flow, 13.0f), text_w = ae_draw_text_width(AE_FONT_ROW, size, text), box_w = text_w + ae_flow_u(flow, 14.0f),
		box_h = ae_flow_u(flow, 20.0f);

	ae_draw_outline(x, center_y - box_h * 0.5f, box_w, box_h, 0.0f, ae_flow_u(flow, 1.0f), color);
	ae_draw_text(AE_FONT_ROW, size, x + box_w * 0.5f, ae_flow_cap_top(AE_FONT_ROW, size, center_y), AE_ALIGN_CENTER, color, text);
	return box_w;
}

static void tile_draw(int index, float x, float y, float width, float height)
{
	struct ae_flow_screen *flow = &lobby.flow;
	int winner_moment = lobby.lobby.stage == AE_SOCIAL_STAGE_WINNER, humans = lobby.plan.roster.humans, split = humans > 1;
	int dim = winner_moment && index != lobby.vote.winner;
	int bar = winner_moment ? index == lobby.vote.winner : (!split && lobby.focus[0] == index);
	int hover = !winner_moment && !bar && lobby.hover == index;
	float u = ae_flow_u(flow, 1.0f), alpha = dim ? AE_DISABLED_ROW_ALPHA : 1.0f;
	int narrow = width < 620.0f * u;
	float pic_w = (narrow ? 148.0f : 196.0f) * u, pic_h = ae_cover_frame_height(pic_w), pic_y = y + (height - pic_h) * 0.5f;
	float tx = x + ae_flow_u(flow, 16.0f) + pic_w + ae_flow_u(flow, 24.0f), col_w = x + width - ae_flow_u(flow, 16.0f) - tx;
	float title_size = ae_flow_u(flow, narrow ? 32.0f : 40.0f), cap_h = ae_draw_cap_height(AE_FONT_TITLE, title_size);
	unsigned int ink = bar ? AE_COLOR_SELECTION_TEXT : dim ? AE_COLOR_MUTED : AE_COLOR_TITLE;
	unsigned int sub_ink = bar ? AE_COLOR_SELECTION_TEXT : dim ? AE_COLOR_MUTED : AE_COLOR_TEXT;
	unsigned int quiet = bar ? AE_COLOR_SELECTION_MUTED : AE_COLOR_MUTED;
	char fitted[64];
	float name_w, caption_mid, line_top, box_x;
	int voter, seat, mine = !split && lobby.vote.ballot[0] == index && !winner_moment;
	char count_text[16];
	int same;

	if (bar)
	{
		ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_SELECTION);
		ae_draw_rect(x, y, ae_flow_u(flow, 6.0f), height, 0.0f, AE_COLOR_ACCENT);
	}
	else
	{
		ae_draw_rect(x, y, width, height, 0.0f, with_alpha(AE_COLOR_ROW, alpha));
		if (hover)
			ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_HOVER);
	}
	/* (every map's picture in the same 16:9 frame) */
	if (lobby.tile[index].picture)
		ae_draw_image_fill(lobby.tile[index].picture, x + ae_flow_u(flow, 16.0f), pic_y, pic_w, pic_h, with_alpha(0xFFFFFFFFu, alpha));
	else
		ae_draw_rect(x + ae_flow_u(flow, 16.0f), pic_y, pic_w, pic_h, 0.0f, with_alpha(AE_COLOR_WELL, alpha));
	if (lobby.pg.on)
	{
		/* (the choice's letter, the one the roster's vote marks show) */
		char letter[2];
		float badge = ae_flow_u(flow, 26.0f);

		letter[0] = (char)('A' + index);
		letter[1] = 0;
		ae_draw_rect(x + ae_flow_u(flow, 24.0f), pic_y + ae_flow_u(flow, 8.0f), badge, badge, 0.0f, AE_COLOR_POPOVER);
		ae_draw_text(AE_FONT_ROW, ae_flow_u(flow, 16.0f), x + ae_flow_u(flow, 24.0f) + badge * 0.5f, ae_flow_cap_top(AE_FONT_ROW,
			ae_flow_u(flow, 16.0f), pic_y + ae_flow_u(flow, 8.0f) + badge * 0.5f), AE_ALIGN_CENTER, AE_COLOR_TITLE, letter);
	}
	/* (the map's name: cut with "..." only when even the smaller size does not fit) */
	same = lobby.pg.on && index == 0;
	ae_fit_text(AE_FONT_TITLE, title_size, same ? ae_rematch_choice_word(0) : lobby.tile[index].caption, col_w, fitted, (int)sizeof(fitted));
	ae_draw_text(AE_FONT_TITLE, title_size, tx, y + ae_flow_u(flow, 24.0f), AE_ALIGN_LEFT, ink, fitted);
	name_w = ae_draw_text_width(AE_FONT_TITLE, title_size, fitted);
	caption_mid = y + ae_flow_u(flow, 24.0f) + cap_h * 0.5f;
	box_x = tx + name_w + ae_flow_u(flow, 12.0f);
	if (winner_moment && index == lobby.vote.winner)
		badge_draw(box_x, caption_mid, bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT, "NEXT MAP");
	else if (mine && box_x + ae_draw_text_width(AE_FONT_ROW, ae_flow_u(flow, 13.0f), "YOUR VOTE") + ae_flow_u(flow, 14.0f) <= x + width)
		badge_draw(box_x, caption_mid, bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED, "YOUR VOTE");
	ae_fit_text(AE_FONT_ROW, ae_flow_u(flow, 24.0f), same ? lobby.tile[index].caption : lobby.tile[index].type, col_w, fitted, (int)sizeof(fitted));
	ae_draw_text(AE_FONT_ROW, ae_flow_u(flow, 24.0f), tx, y + ae_flow_u(flow, 71.0f), AE_ALIGN_LEFT, sub_ink, fitted);
	line_top = y + ae_flow_u(flow, 102.0f);
	ae_draw_text_tracked(AE_FONT_BODY, ae_flow_u(flow, 16.0f), 0.09f, tx, line_top, AE_ALIGN_LEFT, quiet, lobby.tile[index].rules);
	/* the tally: a square a player's vote in the player's colour (with the number), a half width square a bot's vote */
	{
		float sx = tx, sy = y + ae_flow_u(flow, 136.0f), side = ae_flow_u(flow, 18.0f);

		for (voter = 0; (seat = ae_social_vote_voter(&lobby.vote, index, voter)) >= 0; voter++)
		{
			if (seat < humans)
			{
				char number[4];

				ae_draw_rect(sx, sy, side, side, ae_flow_u(flow, 2.0f), with_alpha(slot_color(seat), alpha));
				snprintf(number, sizeof(number), "%d", seat + 1);
				ae_draw_text(AE_FONT_ROW, ae_flow_u(flow, 13.0f), sx + side * 0.5f, ae_flow_cap_top(AE_FONT_ROW,
					ae_flow_u(flow, 13.0f), sy + side * 0.5f), AE_ALIGN_CENTER, AE_COLOR_TITLE, number);
				sx += ae_flow_u(flow, 24.0f);
			}
			else
			{
				ae_draw_rect(sx, sy, side * 0.5f, side, ae_flow_u(flow, 2.0f),
					bar ? AE_COLOR_SELECTION_MUTED : with_alpha(AE_COLOR_TEXT, alpha));
				sx += ae_flow_u(flow, 15.0f);
			}
		}
	}
	votes_text(count_text, sizeof(count_text), ae_social_vote_score(&lobby.vote, index));
	{
		int half = ae_social_vote_score(&lobby.vote, index);
		size_t used = strlen(count_text);

		snprintf(count_text + used, sizeof(count_text) - used, " VOTE%s", half == 2 ? "" : "S");
	}
	tracked(flow, 18.0f, x + width - ae_flow_u(flow, 20.0f), y + ae_flow_u(flow, 145.0f), AE_ALIGN_RIGHT,
		bar ? AE_COLOR_SELECTION_TEXT : dim ? AE_COLOR_MUTED : AE_COLOR_TEXT, count_text);
	if (split)
	{
		/* (split screen: a frame a player in their colour, nested 5 u apart, and the emblems of those who voted here) */
		int player, nest = 0;
		float mark_x = x + width - ae_flow_u(flow, 20.0f);

		for (player = 0; player < humans && player < AE_SOCIAL_LOCAL && !winner_moment; player++)
			if (lobby.focus[player] == index)
			{
				float o = ae_flow_u(flow, 5.0f) * (float)nest++;

				ae_draw_outline(x + o, y + o, width - 2.0f * o, height - 2.0f * o, ae_flow_u(flow, 2.0f), ae_flow_u(flow, 3.0f),
					slot_color(player));
			}
		for (player = 0; player < humans && player < AE_SOCIAL_LOCAL; player++)
			if (lobby.vote.ballot[player] == index)
			{
				char number[4];
				float side = ae_flow_u(flow, 24.0f);

				mark_x -= side;
				ae_draw_rect(mark_x, y + ae_flow_u(flow, 16.0f), side, side, ae_flow_u(flow, 2.0f), with_alpha(slot_color(player), alpha));
				snprintf(number, sizeof(number), "%d", player + 1);
				ae_draw_text(AE_FONT_ROW, ae_flow_u(flow, 16.0f), mark_x + side * 0.5f, ae_flow_cap_top(AE_FONT_ROW,
					ae_flow_u(flow, 16.0f), y + ae_flow_u(flow, 28.0f)), AE_ALIGN_CENTER, AE_COLOR_TITLE, number);
				mark_x -= ae_flow_u(flow, 6.0f);
			}
	}
	if (!winner_moment)
		ae_hit_add(x, y, width, height, HIT_TILES, AE_PART_ITEM, (short)index);
}

static void vote_card_draw(float x, float y, float width)
{
	struct ae_flow_screen *flow = &lobby.flow;
	int count = lobby.vote.count, at, lines, winner_moment = lobby.lobby.stage == AE_SOCIAL_STAGE_WINNER;
	float pad = ae_flow_u(flow, 23.0f), tile_h = ae_flow_u(flow, 176.0f), gap = ae_flow_u(flow, 8.0f);
	float height = ae_flow_u(flow, 56.0f) + (float)count * tile_h + (float)(count - 1) * gap + pad;
	char kind[64], text[200], wrapped[3][160];

	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_PANEL);
	ae_draw_rect(x, y, width, ae_flow_u(flow, 4.0f), 0.0f, AE_COLOR_ACCENT);
	if (lobby.pg.on)
		snprintf(kind, sizeof(kind), "NEXT GAME  /  %s", lobby.plan.label);
	else
		snprintf(kind, sizeof(kind), "%s \xC2\xB7 TOURNAMENT", lobby.plan.label);
	ae_draw_text_tracked(AE_FONT_BODY, ae_flow_u(flow, 18.0f), 0.09f, x + pad, y + ae_flow_u(flow, 24.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, kind);
	ae_draw_text_tracked(AE_FONT_BODY, ae_flow_u(flow, 18.0f), 0.09f, x + width - pad, y + ae_flow_u(flow, 24.0f), AE_ALIGN_RIGHT,
		AE_COLOR_TEXT, "MAP VOTE");
	for (at = 0; at < count; at++)
		tile_draw(at, x + pad, y + ae_flow_u(flow, 56.0f) + (float)at * (tile_h + gap), width - 2.0f * pad, tile_h);
	if (winner_moment)
		return;
	if (lobby.pg.on)
	{
		/* (the news under the vote: who joined, who left, who is staying, the invite sent) */
		if (lobby.pg.news_count)
			ae_draw_rect(x, y + height + ae_flow_u(flow, 8.0f), width, ae_flow_u(flow, 24.0f + 31.0f * (float)lobby.pg.news_count), 0.0f,
				AE_COLOR_PANEL);
		for (at = 0; at < lobby.pg.news_count; at++)
			ae_draw_text(AE_FONT_BODY, ae_flow_u(flow, 21.0f), x + pad, y + height + ae_flow_u(flow, 36.0f + 31.0f * (float)at + 8.0f), AE_ALIGN_LEFT,
				at == 0 && lobby.pg.news_accent ? AE_COLOR_ACCENT : AE_COLOR_TEXT, lobby.pg.news[at]);
		return;
	}
	snprintf(text, sizeof(text), "%s", lobby.plan.roster.humans > 1 ?
		"Each player votes with their own controller: the frame in your colour is your choice, A votes. A bot counts half." :
		"A votes for the lit map; you can change it until the time is up. The most votes wins, a bot counts half, and a tie is drawn at random.");
	lines = ae_wrap_text(AE_FONT_BODY, ae_flow_u(flow, 21.0f), text, width, wrapped, 3);
	for (at = 0; at < lines; at++)
		ae_draw_text(AE_FONT_BODY, ae_flow_u(flow, 21.0f), x, y + height + ae_flow_u(flow, 24.0f + 31.0f * (float)at), AE_ALIGN_LEFT,
			AE_COLOR_MUTED, wrapped[at]);
}

/* ---------- the screen */

/* the LAST GAME card over the roster: the result, the score and map, the rank now, the EXP the game gave */
static void last_draw(float x, float y, float width)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_social_last const *last = &lobby.pg.last;
	float height = ae_flow_u(flow, 86.0f), pad = ae_flow_u(flow, 22.0f), right = x + width - pad;
	unsigned int edge = strstr(last->title, "RED") ? team_color(0) : strstr(last->title, "BLUE") ? team_color(1) : AE_COLOR_MUTED;
	char map[AE_SOCIAL_MAP], line[96], exp[16];

	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_PANEL);
	ae_draw_rect(x, y, ae_flow_u(flow, 6.0f), height, 0.0f, edge);
	tracked(flow, 15.0f, x + pad, y + ae_flow_u(flow, 24.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, "LAST GAME");
	ctext(flow, AE_FONT_ROW, 30.0f, x + pad, y + ae_flow_u(flow, 56.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, last->title[0] ? last->title : "GAME OVER");
	map_caption(last->map, map, sizeof(map));
	if (last->score[0])
		snprintf(line, sizeof(line), "%s  /  %s", last->score, map);
	else
		snprintf(line, sizeof(line), "%s", map);
	{
		/* (the score and map between the title and the rank: cut with "..." when the room is short) */
		float title_w = ae_draw_text_width(AE_FONT_ROW, ae_flow_u(flow, 30.0f), last->title[0] ? last->title : "GAME OVER");
		float from = x + pad + title_w + ae_flow_u(flow, 28.0f), to = right - ae_flow_u(flow, 120.0f) - ae_draw_text_width(AE_FONT_ROW,
			ae_flow_u(flow, 21.0f), "NOW RANK 99") - ae_flow_u(flow, 20.0f);
		char fitted[96];

		/* (the score alone when the map does not fit beside it) */
		if (last->score[0] && ae_draw_text_width(AE_FONT_BODY, ae_flow_u(flow, 21.0f), line) > to - from)
			snprintf(line, sizeof(line), "%s", last->score);
		if (ae_draw_text_width(AE_FONT_BODY, ae_flow_u(flow, 21.0f), line) <= to - from)
		{
			ae_fit_text(AE_FONT_BODY, ae_flow_u(flow, 21.0f), line, to - from, fitted, (int)sizeof(fitted));
			ctext(flow, AE_FONT_BODY, 21.0f, from, y + ae_flow_u(flow, 57.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, fitted);
		}
	}
	if (last->exp_delta || last->rank_after)
	{
		snprintf(exp, sizeof(exp), "%+d", last->exp_delta);
		tracked(flow, 15.0f, right, y + ae_flow_u(flow, 24.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED, "EXP");
		ctext(flow, AE_FONT_ROW, 30.0f, right, y + ae_flow_u(flow, 56.0f), AE_ALIGN_RIGHT, last->exp_delta > 0 ? AE_COLOR_ACCENT : AE_COLOR_TEXT, exp);
		if (last->rank_after > 0)
		{
			char rank[24];

			snprintf(rank, sizeof(rank), "NOW RANK %d", last->rank_after);
			ctext(flow, AE_FONT_ROW, 21.0f, right - ae_flow_u(flow, 120.0f), y + ae_flow_u(flow, 57.0f), AE_ALIGN_RIGHT, AE_COLOR_TITLE, rank);
		}
	}
}

static void lobby_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &lobby.flow;
	char crumb[48];
	float width, left, top;

	(void)instance;
	ae_flow_full_density(flow);
	snprintf(crumb, sizeof(crumb), "AE TOURNAMENT");
	ae_flow_title(flow, lobby.plan.label, crumb);
	width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH;
	left = ae_flow_left_x(flow);
	top = ae_flow_u(flow, 190.0f);
	status_draw(left, top, width);
	if (lobby.pg.on)
	{
		last_draw(left, top + ae_flow_u(flow, 122.0f), width);
		roster_draw(left, top + ae_flow_u(flow, 246.0f), width);
	}
	else
		roster_draw(left, top + ae_flow_u(flow, 184.0f), width);
	if (vote_view())
		vote_card_draw(flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X, top, flow->frame.rect.x + flow->frame.rect.width -
			flow->frame.margin - (flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X));
	else if (lobby.lobby.stage >= AE_SOCIAL_STAGE_FOUND && !(lobby.vote_on && !vote_begun()))
		card_draw(flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X, top, flow->frame.rect.x + flow->frame.rect.width -
			flow->frame.margin - (flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X));
	flow->prompt_count = 0;
	if (!lobby.vote_on)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Start now", AE_ACTION_ACCEPT);
	else
	{
		if (lobby.lobby.stage == AE_SOCIAL_STAGE_VOTE)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, lobby.pg.on && ae_still_there_asking(&lobby.pg.still) ?
				"I'm here" : "Vote", AE_ACTION_ACCEPT);
		else if (lobby.lobby.stage < AE_SOCIAL_STAGE_VOTE && !vote_begun() && lobby.lobby.stage <= AE_SOCIAL_STAGE_FOUND)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "To the vote", AE_ACTION_ACCEPT);
		if (lobby.pg.on && lobby.pg.roster_focus >= lobby.plan.roster.humans)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, lobby.plan.party_seat[lobby.pg.roster_focus] ? "Leave party" :
				"Party up", AE_ACTION_X);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_START, "Start now", AE_ACTION_START);
	}
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Leave", AE_ACTION_BACK);
	if (lobby.pg.on && lobby.lobby.stage == AE_SOCIAL_STAGE_VOTE)
	{
		flow->prompts[flow->prompt_count] = ae_flow_prompt(AE_BUTTON_DPAD_LEFT, "Roster / votes", AE_ACTION_LEFT);
		flow->prompts[flow->prompt_count++].pair = 1;
	}
	ae_flow_draw_prompts(flow);
}

static void stage_log(void)
{
	int stage = lobby.lobby.stage;

	if (stage == lobby.last_stage)
		return;
	lobby.last_stage = stage;
	{
		static char const *const names[] = { "SEARCHING", "FOUND", "STARTING", "GO", "CANCELLED", "VOTE", "WINNER" };

		platform_log("ae social lobby: stage %s", stage >= 0 && stage <= 6 ? names[stage] : "?");
		ae_dev_event("social_lobby", "\"stage\":\"%s\",\"seats\":%d", stage >= 0 && stage <= 6 ? names[stage] : "?",
			ae_social_lobby_joined(&lobby.lobby));
	}
}

/* ---------- the vote */

static void vote_seat_name(int seat, char *name, size_t size)
{
	if (seat < lobby.plan.roster.humans)
		snprintf(name, size, "P%d", seat + 1);
	else
		seat_name(seat, name, size);
}

static void vote_log_open(void)
{
	struct ae_social_vote const *vote = &lobby.vote;
	char line[256];
	int at;

	line[0] = 0;
	for (at = 0; at < vote->count; at++)
		snprintf(line + strlen(line), sizeof(line) - strlen(line), "%s%s:%s", at ? " " : "", vote->choice[at].map, vote->choice[at].gametype);
	platform_log("ae social vote: choices %s x%d", line, vote->count);
	line[0] = 0;
	for (at = 0; at < vote->count; at++)
		snprintf(line + strlen(line), sizeof(line) - strlen(line), "%s\"%s:%s\"", at ? "," : "", vote->choice[at].map, vote->choice[at].gametype);
	ae_dev_event("social_vote", "\"phase\":\"open\",\"choices\":[%s],\"seconds\":%d", line,
		(int)((lobby_vote_length() - lobby.vote.elapsed + 999) / 1000));
}

/* a seat's vote counted: the log says who and which */
static void vote_log_cast(int seat)
{
	char name[AE_SOCIAL_LOBBY_NAME + 4];
	int choice = lobby.vote.ballot[seat];

	vote_seat_name(seat, name, sizeof(name));
	platform_log("ae social vote: %s -> %d (%s)", name, choice + 1, lobby.vote.choice[choice].map);
	lobby.logged[seat] = 1;
}

/* the vote is decided: the winner becomes the plan's game (map, game type, start loadout, picture), once */
static void vote_apply(void)
{
	struct ae_social_vote const *vote = &lobby.vote;
	int winner = vote->winner, each;

	if (lobby.applied || !vote->closed || winner < 0)
		return;
	lobby.applied = 1;
	for (each = vote->humans; each < vote->seats; each++)
		if (vote->ballot[each] >= 0 && !lobby.logged[each])
			vote_log_cast(each);
	platform_log("ae social vote: winner %s (%d/%d half votes, tie drawn: %s)", vote->choice[winner].map, ae_social_vote_score(vote, winner),
		ae_social_vote_total_weight(vote), vote->tie ? "yes" : "no");
	ae_dev_event("social_vote", "\"phase\":\"winner\",\"votes\":[%d,%d,%d],\"unit\":\"half\",\"winner\":%d,\"tie\":%d",
		ae_social_vote_score(vote, 0), ae_social_vote_score(vote, 1), vote->count > 2 ? ae_social_vote_score(vote, 2) : 0, winner,
		vote->tie ? 1 : 0);
	if (!ae_social_plan_choose(&lobby.plan, vote->choice[winner].entry))
		return;
	lobby.picture = lobby.tile[winner].picture;
	lobby.picture_w = lobby.tile[winner].w;
	lobby.picture_h = lobby.tile[winner].h;
}

/* the vote's state follows the lobby's stage: bots vote as time passes, the vote ends at its length or when everyone has voted */
static void vote_follow(void)
{
	int stage = lobby.lobby.stage, seat;

	if (!lobby.vote_on || lobby.vote.closed)
	{
		if (lobby.vote_on)
			vote_apply();
		return;
	}
	if (stage == AE_SOCIAL_STAGE_VOTE)
	{
		ae_social_vote_advance(&lobby.vote, ae_social_lobby_vote_elapsed(&lobby.lobby));
		for (seat = lobby.vote.humans; seat < lobby.vote.seats; seat++)
			if (lobby.vote.ballot[seat] >= 0 && !lobby.logged[seat])
				vote_log_cast(seat);
		if (lobby.vote.closed)
			ae_social_lobby_vote_finish(&lobby.lobby);
	}
	else if (stage != AE_SOCIAL_STAGE_SEARCHING && stage != AE_SOCIAL_STAGE_FOUND)
	{
		/* (time ran out between two frames, or the lobby went on: the vote ends where it is) */
		ae_social_vote_advance(&lobby.vote, lobby_vote_length());
		ae_social_vote_close(&lobby.vote);
	}
	vote_apply();
}

/* a player votes for a tile */
static void vote_cast(int player, int tile)
{
	if (player < 0 || player >= lobby.plan.roster.humans)
		return;
	if (ae_social_vote_cast(&lobby.vote, player, tile))
	{
		ae_sound_request(AE_SOUND_FORWARD, 0);
		vote_log_cast(player);
	}
}

/* START: the vote ends now (the bots that have not voted vote), the winner is the game, and the lobby goes on to GO */
static void vote_end_now(void)
{
	if (lobby.vote_on && !lobby.vote.closed)
	{
		ae_social_lobby_vote_open(&lobby.lobby);
		ae_social_vote_close(&lobby.vote);
		vote_apply();
	}
}

/* the dev scene: the vote opens at once, its time and the bots' votes set */
static void vote_dev_open(void)
{
	if (!lobby.vote_on)
		return;
	ae_social_lobby_vote_open(&lobby.lobby);
	ae_social_lobby_vote_seek(&lobby.lobby, lobby.dev_left >= 0 && lobby.dev_left * 1000L < lobby_vote_length() ?
		lobby_vote_length() - lobby.dev_left * 1000L : 0);
	/* (the set votes first: the bots whose time has come by then vote for themselves after them) */
	if (lobby.dev_has_votes)
		ae_social_vote_preset(&lobby.vote, lobby.dev_votes, lobby.vote.count);
	ae_social_vote_advance(&lobby.vote, ae_social_lobby_vote_elapsed(&lobby.lobby));
}

/* the Still-there counter takes this lobby: input in it starts the count again, else it counts one more game nobody touched */
static void still_settle(void)
{
	lobby.pg.decided = 1;
	if (!ae_still_there_auto_start(&lobby.pg.still, lobby.pg.input_seen))
		platform_log("ae social lobby: still there? asked after %d games with no input", AE_REMATCH_IDLE_GAMES);
	still_keep = lobby.pg.still;
}

static void lobby_update(struct ae_screen *instance)
{
	unsigned long now = ae_motion_now();
	long passed = lobby.last_ms ? (long)(now - lobby.last_ms) : 0;

	(void)instance;
	lobby.last_ms = now;
	if (passed < 0 || passed > 1000)
		passed = 0;
	if (lobby.pg.hold)
		passed = 0;
	if (lobby.pg.on && !lobby.pg.decided)
	{
		/* ("Still there?": settled once, before the vote ends by itself or as it ends any other way; two games in a row that nobody
		touched and the lobby stops and asks. A frame never takes more than the time to 0.5 s before the end of the vote) */
		if (lobby.lobby.stage == AE_SOCIAL_STAGE_VOTE)
		{
			long left = ae_social_vote_left_ms(&lobby.vote);

			if (left <= 600)
				still_settle();
			else if (passed > left - 500)
				passed = left - 500;
		}
		else if (lobby.lobby.stage >= AE_SOCIAL_STAGE_STARTING)
			still_settle();
	}
	if (lobby.pg.on && ae_still_there_asking(&lobby.pg.still))
		passed = 0;
	ae_social_lobby_advance(&lobby.lobby, passed);
	stage_log();
	vote_follow();
	if (lobby.lobby.stage == AE_SOCIAL_STAGE_GO && !lobby.started)
	{
		lobby.started = 1;
		platform_log("ae social lobby: go");
		if (ae_social_plan_start(&lobby.plan))
		{
			lobby.open = 0;
			ae_ui_remove(&lobby);
			return;
		}
		/* (the game did not open: back to MATCHMAKING with the lobby gone) */
		ae_sound_request(AE_SOUND_FAILURE, 0);
		lobby.open = 0;
		ae_ui_remove(&lobby);
	}
}

static void focus_step(int player, int direction)
{
	short focus;

	if (player < 0 || player >= AE_SOCIAL_LOCAL || player >= lobby.plan.roster.humans)
		return;
	focus = ae_choice_step((short)lobby.vote.count, lobby.focus[player], (short)direction);
	if (focus != lobby.focus[player])
	{
		lobby.focus[player] = focus;
		ae_sound_request(AE_SOUND_CURSOR, 0);
	}
}

/* the focus on the roster steps over the seats that are people or bots (an open seat is skipped) */
static void roster_focus_step(int direction)
{
	int seat = lobby.pg.roster_focus, count = lobby.plan.roster.count;

	for (seat += direction; seat >= 0 && seat < count; seat += direction)
		if (!lobby.pg.seat[seat].open)
		{
			lobby.pg.roster_focus = (short)seat;
			ae_sound_request(AE_SOUND_CURSOR, 0);
			return;
		}
}

/* PARTY UP on the focused seat: offline a bot joins the party (free, at once); online a person is sent an invite */
static void party_up(int seat)
{
	struct ae_social_roster *roster = &lobby.plan.roster;
	char name[AE_SOCIAL_LOBBY_NAME + 4], line[96];
	int bot = seat - roster->humans;

	if (seat < roster->humans)
		return;
	seat_name(seat, name, sizeof(name));
	if (lobby.plan.party_seat[seat])
	{
		/* (X on a party bot takes it out of the party: it plays as any bot) */
		char id[AE_PARTY_ID];

		snprintf(id, sizeof(id), "bot:%s", name);
		if (!lobby.pg.dev)
			ae_glue_party_remove(id);
		lobby.plan.party_seat[seat] = 0;
		lobby.plan.party_bots--;
		ae_rematch_news(AE_NEWS_PARTY_LEFT, name, 0, line, (int)sizeof(line));
		platform_log("ae party: %s left the party from the lobby", name);
		memmove(lobby.pg.news[1], lobby.pg.news[0], sizeof(lobby.pg.news[0]) * (AE_REMATCH_NEWS_LINES - 1));
		snprintf(lobby.pg.news[0], sizeof(lobby.pg.news[0]), "%s", line);
		if (lobby.pg.news_count < AE_REMATCH_NEWS_LINES)
			lobby.pg.news_count++;
		lobby.pg.news_accent = 1;
		ae_sound_request(AE_SOUND_BACK, 0);
		return;
	}
	if (lobby.pg.online && lobby.pg.seat[seat].ping >= 0)
	{
		ae_rematch_news(AE_NEWS_INVITE_SENT, name, 0, line, (int)sizeof(line));
		platform_log("ae party: invite to %s (staged)", name);
	}
	else
	{
		struct ae_social_playlist const *list = ae_social_playlist_by_id(lobby.plan.playlist);
		int limit = list ? ae_social_party_limit(list) : 1;

		/* (one limit: the playlist's party rule; a full party refuses, and says so) */
		if (roster->humans + lobby.plan.party_bots >= limit || (!lobby.pg.dev && !ae_glue_party_up_bot(name, roster->seat[seat].tier)))
		{
			ae_rematch_news(AE_NEWS_PARTY_FULL, "", limit, line, (int)sizeof(line));
			memmove(lobby.pg.news[1], lobby.pg.news[0], sizeof(lobby.pg.news[0]) * (AE_REMATCH_NEWS_LINES - 1));
			snprintf(lobby.pg.news[0], sizeof(lobby.pg.news[0]), "%s", line);
			if (lobby.pg.news_count < AE_REMATCH_NEWS_LINES)
				lobby.pg.news_count++;
			lobby.pg.news_accent = 0;
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
		lobby.plan.party_seat[seat] = 1;
		lobby.plan.party_bots++;
		/* (it plays on the leader's team: the team of a seat that is not the party's swaps with it) */
		if (roster->teams > 0)
		{
			int team[AE_SOCIAL_SEATS], member[AE_SOCIAL_SEATS], each;

			for (each = 0; each < roster->count; each++)
			{
				team[each] = roster->seat[each].team;
				member[each] = each < roster->humans || lobby.plan.party_seat[each];
			}
			ae_party_keep_on_team(team, member, roster->count, roster->seat[0].team);
			for (each = 0; each < roster->count; each++)
				roster->seat[each].team = team[each];
		}
		snprintf(lobby.plan.bot_name[bot], sizeof(lobby.plan.bot_name[bot]), "%s", name);
		ae_rematch_news(AE_NEWS_PARTY_JOINED, name, 0, line, (int)sizeof(line));
		platform_log("ae party: %s (tier %d) partied up from the lobby", name, roster->seat[seat].tier);
	}
	/* (the newest news first) */
	memmove(lobby.pg.news[1], lobby.pg.news[0], sizeof(lobby.pg.news[0]) * (AE_REMATCH_NEWS_LINES - 1));
	snprintf(lobby.pg.news[0], sizeof(lobby.pg.news[0]), "%s", line);
	if (lobby.pg.news_count < AE_REMATCH_NEWS_LINES)
		lobby.pg.news_count++;
	lobby.pg.news_accent = 1;
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_dev_event("party_up", "\"name\":\"%s\",\"online\":%d", name, lobby.pg.online);
}

static int lobby_handle(struct ae_screen *instance, struct ae_event const *event)
{
	int stage = lobby.lobby.stage;

	(void)instance;
	/* ("Still there?" up: A or START answers it, B still leaves, the rest is not an answer) */
	if (lobby.pg.on && ae_still_there_asking(&lobby.pg.still) && event->action != AE_ACTION_BACK)
	{
		if (event->action == AE_ACTION_ACCEPT || event->action == AE_ACTION_START)
		{
			lobby.pg.input_seen = 1;
			ae_still_there_input(&lobby.pg.still);
			still_keep = lobby.pg.still;
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		return 1;
	}
	if (lobby.pg.on && event->action != AE_ACTION_NONE)
	{
		/* (any input counts at once: the count starts again and the next lobby has it) */
		lobby.pg.input_seen = 1;
		ae_still_there_input(&lobby.pg.still);
		still_keep = lobby.pg.still;
	}
	switch (event->action)
	{
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (lobby.pg.on && stage == AE_SOCIAL_STAGE_VOTE)
		{
			short was = lobby.pg.roster_focus;

			if (event->action == AE_ACTION_LEFT && was < 0)
			{
				int seat;

				for (seat = lobby.plan.roster.humans; seat < lobby.plan.roster.count; seat++)
					if (!lobby.pg.seat[seat].open)
						break;
				lobby.pg.roster_focus = (short)(seat < lobby.plan.roster.count ? seat : 0);
			}
			else if (event->action == AE_ACTION_RIGHT)
				lobby.pg.roster_focus = -1;
			if (was != lobby.pg.roster_focus)
				ae_sound_request(AE_SOUND_CURSOR, 0);
		}
		return 1;
	case AE_ACTION_X:
		if (lobby.pg.on && stage == AE_SOCIAL_STAGE_VOTE && lobby.pg.roster_focus >= 0)
			party_up(lobby.pg.roster_focus);
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		if (lobby.pg.on && lobby.pg.roster_focus >= 0 && stage == AE_SOCIAL_STAGE_VOTE)
			roster_focus_step(event->action == AE_ACTION_UP ? -1 : 1);
		else if (lobby.vote_on && stage == AE_SOCIAL_STAGE_VOTE)
			focus_step(event->player < 0 ? 0 : event->player, event->action == AE_ACTION_UP ? -1 : 1);
		return 1;
	case AE_ACTION_ACCEPT:
		if (lobby.pg.on && lobby.pg.roster_focus >= 0)
			return 1;
		if (lobby.vote_on && stage == AE_SOCIAL_STAGE_VOTE)
			vote_cast(event->player < 0 ? 0 : event->player, lobby.focus[event->player < 0 || event->player >= AE_SOCIAL_LOCAL ? 0 : event->player]);
		else if (lobby.vote_on && stage <= AE_SOCIAL_STAGE_FOUND)
		{
			/* (before the vote A fills the lobby and opens it) */
			if (ae_social_lobby_vote_open(&lobby.lobby))
				ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else if (!lobby.vote_on || stage == AE_SOCIAL_STAGE_STARTING)
		{
			if (ae_social_lobby_skip(&lobby.lobby))
				ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		return 1;
	case AE_ACTION_START:
		if (stage == AE_SOCIAL_STAGE_VOTE || stage <= AE_SOCIAL_STAGE_FOUND)
			vote_end_now();
		if (ae_social_lobby_skip(&lobby.lobby))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return 1;
	case AE_ACTION_BACK:
		/* (during the vote only player 1 leaves: another player's B does nothing) */
		if (lobby.vote_on && stage == AE_SOCIAL_STAGE_VOTE && event->player > 0 && event->player < AE_SOCIAL_LOCAL)
			return 1;
		ae_social_lobby_cancel(&lobby.lobby);
		platform_log("ae social lobby: left");
		ae_sound_request(AE_SOUND_BACK, 0);
		lobby.open = 0;
		ae_ui_remove(&lobby);
		return 1;
	default:
		return 1;
	}
}

static void lobby_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;

	if (ae_prompts_pointer(lobby.flow.prompts, lobby.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		lobby_handle(instance, &event);
		return;
	}
	lobby.hover = -1;
	if (!lobby.vote_on || lobby.lobby.stage != AE_SOCIAL_STAGE_VOTE)
		return;
	if (!ae_hit_at(pointer->x, pointer->y, 0, &hit) || hit.id != HIT_TILES || hit.index < 0 || hit.index >= lobby.vote.count)
		return;
	lobby.hover = hit.index;
	/* (a hover moves the focus only after a pointer move, as the lists do) */
	if (pointer->moved && lobby.plan.roster.humans < 2 && lobby.focus[0] != hit.index)
	{
		lobby.focus[0] = hit.index;
		ae_sound_request(AE_SOUND_CURSOR, 0);
	}
	if (pointer->left_clicks)
	{
		int player = pointer->player > 0 && pointer->player < AE_SOCIAL_LOCAL ? pointer->player : 0;

		lobby.focus[player] = hit.index;
		vote_cast(player, hit.index);
	}
}

static struct ae_screen_class const lobby_class =
{
	.name = "social_lobby", .handle = lobby_handle, .draw = lobby_draw, .pointer = lobby_pointer, .update = lobby_update,
};

/* DEV MODE's vote scene: its time and the bots' votes */
struct dev_vote
{
	int left, has_votes;
	int votes[AE_SOCIAL_VOTE_CHOICES];
};

/* the vote of a lobby: the choices from the playlist, their pictures and words; off for a named map, a game that loads at once
   and a playlist with fewer than two maps to choose from */
static void vote_setup(int enabled)
{
	struct ae_social_vote_choice choices[AE_SOCIAL_VOTE_CHOICES];
	int count, at;
	char shape[32];

	lobby.vote_on = 0;
	if (!enabled)
		return;
	if (lobby.pg.on && lobby.plan.entry >= 0 && ae_social_playlist_by_id(lobby.plan.playlist) &&
		lobby.plan.entry < ae_social_playlist_by_id(lobby.plan.playlist)->count)
	{
		/* (a rematch's vote: SAME AGAIN first, then two picks that are not the map just played) */
		struct ae_social_playlist const *list = ae_social_playlist_by_id(lobby.plan.playlist);

		count = ae_social_plan_vote_choices(&lobby.plan, choices + 1, AE_SOCIAL_VOTE_CHOICES - 1);
		if (count < 1)
			return;
		memset(&choices[0], 0, sizeof(choices[0]));
		choices[0].entry = lobby.plan.entry;
		snprintf(choices[0].map, sizeof(choices[0].map), "%s", list->entry[lobby.plan.entry].map);
		snprintf(choices[0].gametype, sizeof(choices[0].gametype), "%s", list->entry[lobby.plan.entry].gametype);
		choices[0].weight = list->entry[lobby.plan.entry].weight;
		count++;
	}
	else
		count = ae_social_plan_vote_choices(&lobby.plan, choices, AE_SOCIAL_VOTE_CHOICES);
	if (count < 2)
		return;
	ae_social_vote_init(&lobby.vote, choices, count, lobby.plan.roster.count, lobby.plan.roster.humans, lobby.plan.seed);
	ae_social_vote_set_length(&lobby.vote, lobby_vote_length());
	ae_social_lobby_vote_enable(&lobby.lobby, lobby_vote_length());
	shape_line(&lobby.plan, shape, sizeof(shape));
	for (at = 0; at < count; at++)
	{
		struct ae_social_plan as_plan = lobby.plan;

		map_caption(choices[at].map, lobby.tile[at].caption, sizeof(lobby.tile[at].caption));
		snprintf(lobby.tile[at].type, sizeof(lobby.tile[at].type), "%s", choices[at].gametype);
		lobby.tile[at].picture = ae_setup_map_picture_named(choices[at].map, &lobby.tile[at].w, &lobby.tile[at].h);
		/* (the start loadout this map would play: the plan's rule for it) */
		ae_social_plan_choose(&as_plan, choices[at].entry);
		snprintf(lobby.tile[at].rules, sizeof(lobby.tile[at].rules), "%s", shape);
		if (as_plan.start)
			snprintf(lobby.tile[at].rules + strlen(lobby.tile[at].rules), sizeof(lobby.tile[at].rules) - strlen(lobby.tile[at].rules),
				" \xC2\xB7 %s", ae_social_start_label(as_plan.start));
	}
	lobby.vote_on = 1;
	lobby.hover = -1;
}

/* the screen for lobby.plan, ready: the names, the profile's, the picture, the vote, the screen pushed. now: the game loads at once; no_vote:
a named map; rematch: the REMATCH's stage (no search: every seat is in and the vote is open); dev: DEV MODE's scene */
static int lobby_start(int now, int no_vote, int rematch, struct dev_vote const *dev)
{
	short local;
	char key[AE_SR_KEY];

	lobby.plan.staged = dev_staged_next || lobby.pg.dev;
	dev_staged_next = 0;
	ae_social_lobby_init(&lobby.lobby, &lobby.plan.roster, lobby.plan.seed);
	ae_social_plan_names(&lobby.plan, lobby.names);
	for (local = 0; local < lobby.plan.humans && local < AE_SOCIAL_LOCAL; local++)
	{
		if (!ae_social_profile_name(local, lobby.you[local], sizeof(lobby.you[local])))
			snprintf(lobby.you[local], sizeof(lobby.you[local]), "PLAYER %d", (int)local + 1);
		lobby.rank[local] = ae_social_screen_key(local, key, sizeof(key)) ? ae_social_profile_rank(key, lobby.plan.playlist) : 1;
	}
	lobby.picture = ae_setup_map_picture_named(lobby.plan.map, &lobby.picture_w, &lobby.picture_h);
	lobby.flow.hit_rows = HIT_LOBBY;
	lobby.flow.hit_prompts = HIT_PROMPTS;
	lobby.last_stage = -1;
	lobby.hover = -1;
	lobby.vote_len = rematch ? AE_REMATCH_VOTE_MS : AE_SOCIAL_VOTE_MS;
	vote_setup(!now && !no_vote);
	if (dev && lobby.vote_on)
	{
		lobby.dev_left = dev->left;
		lobby.dev_has_votes = dev->has_votes;
		memcpy(lobby.dev_votes, dev->votes, sizeof(lobby.dev_votes));
	}
	if (!ae_ui_push(&lobby_class, AE_OWNER_ANY, &lobby))
	{
		platform_log("ae social lobby: the screen could not be pushed: %s not opened", lobby.plan.playlist);
		return 0;
	}
	lobby.open = 1;
	if (now)
		ae_social_lobby_skip(&lobby.lobby);
	if (dev_hold_next == AE_SOCIAL_LOBBY_DEV_HOLD_STARTING)
	{
		/* (DEV MODE: run to the STARTING stage, one second into it, and stop there; 120 s at most) */
		long ran;

		for (ran = 0; ran < 120000 && lobby.lobby.stage != AE_SOCIAL_STAGE_STARTING &&
			lobby.lobby.stage != AE_SOCIAL_STAGE_GO && lobby.lobby.stage != AE_SOCIAL_STAGE_CANCELLED; ran += 100)
			ae_social_lobby_advance(&lobby.lobby, 100);
		if (lobby.lobby.stage == AE_SOCIAL_STAGE_STARTING)
			ae_social_lobby_advance(&lobby.lobby, 1000);
		platform_log("ae social lobby: held at stage %d after %ld ms", (int)lobby.lobby.stage, ran);
		lobby.pg.hold = 1;
		dev_hold_next = -1;
	}
	else if (dev_hold_next >= 0)
	{
		/* (DEV MODE: run to that moment and stop there) */
		ae_social_lobby_advance(&lobby.lobby, dev_hold_next * 1000L);
		lobby.pg.hold = 1;
		dev_hold_next = -1;
	}
	platform_log("ae social lobby: open %s (seed %lu, map %s, %d seats%s%s)", lobby.plan.playlist, lobby.plan.seed, lobby.plan.map,
		lobby.plan.roster.count, lobby.vote_on ? ", map vote" : "", rematch ? ", rematch" : "");
	if (rematch)
	{
		/* (no search: every seat is in, the vote is open at once) */
		if (lobby.vote_on)
			ae_social_lobby_vote_open(&lobby.lobby);
		lobby.pg.roster_focus = -1;
	}
	if (dev && lobby.vote_on)
	{
		vote_dev_open();
		stage_log();
	}
	if (lobby.vote_on)
		vote_log_open();
	return 1;
}

static int lobby_open(char const *playlist, unsigned long seed, char const *map, int now, struct dev_vote const *dev)
{
	if (ae_ui_holds(&lobby))
	{
		platform_log("ae social lobby: already open: %s not opened", playlist ? playlist : "?");
		return 0;
	}
	memset(&lobby, 0, sizeof(lobby));
	ae_still_there_init(&still_keep);
	if (!ae_social_plan_make(playlist, seed, map, NULL, &lobby.plan))
	{
		platform_log("ae social lobby: no plan for %s", playlist ? playlist : "?");
		return 0;
	}
	/* (the vote is off for a named map and a game that loads at once) */
	return lobby_start(now, map && map[0], 0, dev);
}

/* a news line, the newest first */
static void news_add(int kind, char const *name, int tier)
{
	memmove(lobby.pg.news[1], lobby.pg.news[0], sizeof(lobby.pg.news[0]) * (AE_REMATCH_NEWS_LINES - 1));
	ae_rematch_news(kind, name, tier, lobby.pg.news[0], (int)sizeof(lobby.pg.news[0]));
	if (lobby.pg.news_count < AE_REMATCH_NEWS_LINES)
		lobby.pg.news_count++;
}

int ae_screen_social_lobby_open_rematch(void)
{
	struct ae_social_last last;
	struct ae_social_plan plan;
	int game_no, fresh[AE_SOCIAL_SEATS], left_count = 0, seat, at;
	char left[2][24];

	if (ae_ui_holds(&lobby) || !ae_social_game_last(&last) || !ae_social_game_rematch(&plan, &game_no, fresh, left, &left_count))
		return 0;
	memset(&lobby, 0, sizeof(lobby));
	lobby.plan = plan;
	lobby.pg.on = 1;
	lobby.pg.last = last;
	lobby.pg.still = still_keep;
	for (seat = 0; seat < plan.roster.count; seat++)
	{
		lobby.pg.seat[seat].fresh = fresh[seat];
		lobby.pg.seat[seat].ping = -1;
		lobby.pg.seat[seat].kills = lobby.pg.seat[seat].deaths = -1;
		if (seat < plan.roster.humans && seat < AE_SOCIAL_LOCAL)
		{
			lobby.pg.seat[seat].kills = last.kills[seat];
			lobby.pg.seat[seat].deaths = last.deaths[seat];
		}
	}
	if (!lobby_start(0, 0, 1, NULL))
	{
		lobby.pg.on = 0;
		return 0;
	}
	for (at = 0; at < left_count; at++)
		news_add(AE_NEWS_LEFT, left[at], 0);
	for (seat = plan.roster.humans; seat < plan.roster.count; seat++)
		if (fresh[seat])
			news_add(AE_NEWS_JOINED_BOT, lobby.names[seat - plan.roster.humans], plan.roster.seat[seat].tier);
	return 1;
}

int ae_screen_social_lobby_open(char const *playlist, unsigned long seed, char const *map, int now)
{
	return lobby_open(playlist, seed, map, now, NULL);
}

void ae_screen_social_lobby_close(void)
{
	if (lobby.open)
		ae_ui_remove(&lobby);
	lobby.open = 0;
}

static void dev_close_old(void)
{
	/* a lobby left open by an earlier (TEST RUN) scene holds the screen: close it first */
	if (ae_ui_holds(&lobby))
	{
		platform_log("ae social lobby: an earlier lobby was still open: closed for social start");
		ae_ui_remove(&lobby);
		lobby.open = 0;
	}
}

int ae_social_game_dev_lobby(char const *playlist, char const *map, unsigned long seed, int now, char *text, size_t size)
{
	dev_close_old();
	dev_staged_next = 1;
	if (!ae_screen_social_lobby_open(playlist, seed, map, now))
	{
		dev_staged_next = 0;
		snprintf(text, size, "social start: the lobby did not open (%s)", playlist);
		return 0;
	}
	snprintf(text, size, "social start %s: lobby open%s", playlist, now ? ", game loading" : "");
	return 1;
}

int ae_social_game_dev_lobby_vote(char const *playlist, unsigned long seed, int left_seconds, int const *votes, int has_votes,
	char *text, size_t size)
{
	struct dev_vote dev;
	int at;

	memset(&dev, 0, sizeof(dev));
	dev.left = left_seconds < 0 || left_seconds > 20 ? -1 : left_seconds;
	dev.has_votes = has_votes;
	for (at = 0; at < AE_SOCIAL_VOTE_CHOICES && votes && has_votes; at++)
		dev.votes[at] = votes[at];
	dev_close_old();
	dev_staged_next = 1;
	if (!lobby_open(playlist, seed, NULL, 0, &dev))
	{
		dev_staged_next = 0;
		snprintf(text, size, "social start: the lobby did not open (%s)", playlist);
		return 0;
	}
	if (!lobby.vote_on)
	{
		snprintf(text, size, "social start %s vote: this playlist has fewer than two maps here to vote on", playlist);
		return 0;
	}
	snprintf(text, size, "social start %s vote: the map vote is open (%ld s left)", playlist,
		(long)((ae_social_vote_left_ms(&lobby.vote) + 999) / 1000));
	return 1;
}

/* DEV MODE: the post-game lobby on a made-up last game (the REMATCH of the playlist) */
int ae_social_game_dev_lobby_post(char const *playlist, unsigned long seed, int left_seconds, int const *votes, int has_votes,
	int online, int still, int bots, char *text, size_t size)
{
	static char const *const party_names[] = { "TheRhino", "xMirax", "Nebulaxd", "Boulder99", "DuskBlade", "Gutterfox", "Ironquill" };
	static char const *const friend_names[] = { "Kestrel44", "Hollow_Fox", "Nebulaxd", "Boulder99", "DuskBlade", "Gutterfox", "Ironquill" };
	struct dev_vote dev;
	struct ae_social_roster *roster;
	int at, seat, humans, bot_count, first_other, team[AE_SOCIAL_SEATS], member[AE_SOCIAL_SEATS];

	memset(&dev, 0, sizeof(dev));
	dev.left = left_seconds < 0 || left_seconds > 20 ? 15 : left_seconds;
	dev.has_votes = has_votes;
	for (at = 0; at < AE_SOCIAL_VOTE_CHOICES && votes && has_votes; at++)
		dev.votes[at] = votes[at];
	dev_close_old();
	memset(&lobby, 0, sizeof(lobby));
	ae_still_there_init(&still_keep);
	if (!ae_social_plan_make(playlist, seed, NULL, NULL, &lobby.plan))
	{
		snprintf(text, size, "social start: no plan for %s", playlist);
		return 0;
	}
	roster = &lobby.plan.roster;
	humans = roster->humans;
	bot_count = roster->count - humans;
	if (bots > bot_count - 3)
		bots = bot_count - 3;
	if (bots < 0)
		bots = 0;
	lobby.pg.on = 1;
	lobby.pg.dev = 1;
	lobby.pg.online = online;
	lobby.plan.party_bots = bots;
	memset(lobby.plan.party_seat, 0, sizeof(lobby.plan.party_seat));
	for (at = 0; at < bots; at++)
	{
		lobby.plan.party_seat[humans + at] = 1;
		snprintf(lobby.plan.bot_name[at], sizeof(lobby.plan.bot_name[at]), "%s", online ? friend_names[at] : party_names[at]);
	}
	if (roster->teams > 0)
	{
		for (seat = 0; seat < roster->count; seat++)
		{
			team[seat] = roster->seat[seat].team;
			member[seat] = seat < humans || lobby.plan.party_seat[seat];
		}
		ae_party_keep_on_team(team, member, roster->count, roster->seat[0].team);
		for (seat = 0; seat < roster->count; seat++)
			roster->seat[seat].team = team[seat];
	}
	first_other = humans + bots;
	if (online)
	{
		/* (two people besides your party, an open seat: the last, and the rest bots) */
		snprintf(lobby.plan.bot_name[first_other - humans], sizeof(lobby.plan.bot_name[0]), "Dawnbreaker");
		snprintf(lobby.plan.bot_name[first_other + 1 - humans], sizeof(lobby.plan.bot_name[0]), "Mossy7");
		lobby.pg.seat[first_other].staying = 0;
		lobby.pg.seat[first_other + 1].staying = 1;
		lobby.pg.seat[roster->count - 1].open = 1;
	}
	else
	{
		snprintf(lobby.plan.bot_name[roster->count - 1 - humans], sizeof(lobby.plan.bot_name[0]), "Dusk44");
		lobby.pg.seat[roster->count - 1].fresh = 1;
	}
	lobby.pg.last.valid = 2;
	snprintf(lobby.pg.last.title, sizeof(lobby.pg.last.title), "RED TEAM WINS");
	snprintf(lobby.pg.last.score, sizeof(lobby.pg.last.score), "50 - 38");
	snprintf(lobby.pg.last.map, sizeof(lobby.pg.last.map), "%s", lobby.plan.map);
	lobby.pg.last.exp_delta = 60;
	lobby.pg.last.rank_after = 2;
	for (seat = 0; seat < roster->count; seat++)
	{
		lobby.pg.seat[seat].ping = -1;
		lobby.pg.seat[seat].kills = 14 - (seat > 3 ? seat - 3 : 0) - (seat ? 1 : 0);
		lobby.pg.seat[seat].deaths = 6 + seat;
		if (lobby.pg.seat[seat].fresh || lobby.pg.seat[seat].open)
			lobby.pg.seat[seat].kills = lobby.pg.seat[seat].deaths = -1;
	}
	if (online)
	{
		static int const party_pings[] = { 38, 61, 52, 44, 58, 66, 49 };

		for (at = 0; at < bots; at++)
			lobby.pg.seat[humans + at].ping = party_pings[at];
		lobby.pg.seat[first_other].ping = 47;
		lobby.pg.seat[first_other + 1].ping = 72;
	}
	lobby.pg.still = still_keep;
	if (still)
	{
		lobby.pg.still.idle_games = AE_REMATCH_IDLE_GAMES;
		lobby.pg.still.asked = 1;
		lobby.pg.decided = 1;
		dev.left = 1;
	}
	if (!lobby_start(0, 0, 1, &dev))
	{
		lobby.pg.on = 0;
		snprintf(text, size, "social start: the lobby did not open (%s)", playlist);
		return 0;
	}
	/* (the focus on the first bot that is not the party's: its PARTY UP shows) */
	for (seat = humans; seat < roster->count; seat++)
		if (!lobby.plan.party_seat[seat] && !lobby.pg.seat[seat].open && !lobby.pg.seat[seat].fresh)
			break;
	lobby.pg.roster_focus = (short)(seat < roster->count ? seat : -1);
	lobby.pg.news_accent = online;
	if (online)
	{
		news_add(AE_NEWS_LEFT, "RowdyMuffin", 0);
		news_add(AE_NEWS_STAYING, "Mossy7", 0);
		news_add(AE_NEWS_INVITE_SENT, "Dawnbreaker", 0);
	}
	else
	{
		news_add(AE_NEWS_JOINED_BOT, "Dusk44", 9);
		news_add(AE_NEWS_LEFT, "RowdyMuffin", 0);
	}
	snprintf(text, size, "social start %s postgame%s: the rematch lobby is open (%d in your party, vote %ld s left)", playlist,
		online ? " online" : "", bots, (long)((ae_social_vote_left_ms(&lobby.vote) + 999) / 1000));
	return 1;
}
