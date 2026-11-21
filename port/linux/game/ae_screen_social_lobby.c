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
	int dev_left;                   /* DEV MODE: seconds of the vote left when it opens (-1: all) */
	int dev_votes[AE_SOCIAL_VOTE_CHOICES];
	int dev_has_votes;
} lobby;

static struct ae_screen_class const lobby_class;

/* ---------- the vote's words and looks */

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

static void roster_draw(float x, float y, float width)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct item items[AE_SOCIAL_SEATS + 4];
	int count = items_make(items, AE_SOCIAL_SEATS + 4), shown, at;
	float pitch = ae_flow_u(flow, ROW_PITCH_U), height = ae_flow_u(flow, ROW_HEIGHT_U), emblem = ae_flow_u(flow, 32.0f);
	struct ae_social_roster const *roster = &lobby.plan.roster;

	shown = count > VISIBLE_ROWS ? VISIBLE_ROWS : count;
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
			ae_draw_outline(x, top, width, height, 0.0f, ae_flow_u(flow, 1.0f), AE_COLOR_RULE);
			ctext(flow, AE_FONT_BODY, 21.0f, x + ae_flow_u(flow, 20.0f), mid, AE_ALIGN_LEFT, AE_COLOR_MUTED, "Looking for a player");
			continue;
		}
		{
			int seat = it->seat, human = seat < roster->humans, fresh = !human && ae_social_lobby_seat_new(&lobby.lobby, seat);
			int ballot = vote_view() && seat < lobby.vote.seats ? lobby.vote.ballot[seat] : -1;
			float right_x = x + width - ae_flow_u(flow, 20.0f);
			char name[AE_SOCIAL_LOBBY_NAME + 4], initial[4], right[40];
			unsigned int emblem_color = human ? (it->team == 1 ? 0x2E86DEFFu : 0xC0392BFFu) : 0x3A4651FFu;
			float name_x = x + ae_flow_u(flow, 64.0f), name_w;

			seat_name(seat, name, sizeof(name));
			ae_draw_rect(x, top, width, height, 0.0f, AE_COLOR_ROW);
			if (teams_shown() && roster->teams > 0)
				ae_draw_rect(x, top, ae_flow_u(flow, 6.0f), height, 0.0f, team_color(it->team));
			/* (during the vote the ice edge is a bot's new vote, not a join) */
			if (vote_view())
				fresh = !human && ae_social_vote_fresh(&lobby.vote, seat);
			if (fresh)
				ae_draw_rect(x, top, ae_flow_u(flow, 6.0f), height, 0.0f, EXP_ICE);
			if (human && roster->teams > 0)
				emblem_color = team_color(roster->seat[seat].team);
			ae_draw_rect(x + ae_flow_u(flow, 20.0f), mid - emblem * 0.5f, emblem, emblem, ae_flow_u(flow, 2.0f), emblem_color);
			snprintf(initial, sizeof(initial), "%c", name[0] ? name[0] : '?');
			ctext(flow, AE_FONT_ROW, 20.0f, x + ae_flow_u(flow, 20.0f) + emblem * 0.5f, mid, AE_ALIGN_CENTER, AE_COLOR_TITLE, initial);
			ctext(flow, AE_FONT_ROW, 26.0f, name_x, mid, AE_ALIGN_LEFT, human ? AE_COLOR_TITLE : AE_COLOR_TITLE, name);
			name_w = ae_draw_text_width(AE_FONT_ROW, ae_flow_u(flow, 26.0f), name);
			if (fresh && !vote_view())
				ctext(flow, AE_FONT_ROW, 16.0f, name_x + name_w + ae_flow_u(flow, 14.0f), mid, AE_ALIGN_LEFT, EXP_ICE, "JOINED");
			if (human)
				ae_social_view_rank(lobby.rank[seat < AE_SOCIAL_LOCAL ? seat : 0], right, (int)sizeof(right));
			else if (ae_dev_mode_on())
				snprintf(right, sizeof(right), "BOT \xC2\xB7 T%d", roster->seat[seat].tier);
			else
				snprintf(right, sizeof(right), "BOT");
			if (ballot >= 0 && ballot < lobby.vote.count)
			{
				/* (the voted map: its 40 x 32 u picture, its name, a dot, then the seat's tag) */
				float pic_w = ae_flow_u(flow, 40.0f), pic_h = ae_flow_u(flow, 32.0f), cap_w;

				if (lobby.tile[ballot].picture)
					ae_draw_image_whole(lobby.tile[ballot].picture, right_x - pic_w, mid - pic_h * 0.5f, pic_w, pic_h, 0xFFFFFFFFu);
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
		snprintf(head, sizeof(head), "VOTE ENDS IN %ld", (left + 999) / 1000);
		snprintf(sub, sizeof(sub), "Votes in  %d / %d", ae_social_vote_voted(&lobby.vote), l->seats);
		/* (reduced motion: the bar steps in whole seconds) */
		fraction = (float)(ae_motion_reduced() ? ((left + 999) / 1000) * 1000 : left) / (float)AE_SOCIAL_VOTE_MS;
		warn = left <= 3000;
		break;
	case AE_SOCIAL_STAGE_WINNER:
	{
		int win = lobby.vote.winner >= 0 ? lobby.vote.winner : 0, votes = ae_social_vote_count(&lobby.vote, win);

		snprintf(head, sizeof(head), "NEXT MAP: %s", lobby.tile[win].caption);
		if (lobby.vote.tie)
			snprintf(sub, sizeof(sub), "A tie, drawn at random: %s with %d of %d votes.", lobby.tile[win].type, votes, l->seats);
		else
			snprintf(sub, sizeof(sub), "%s with %d of %d votes. Starting in a moment.", lobby.tile[win].type, votes, l->seats);
		break;
	}
	default:
		left = ae_social_lobby_countdown_ms(l);
		snprintf(head, sizeof(head), "STARTING IN %ld", (left + 999) / 1000);
		snprintf(sub, sizeof(sub), "Teams are set. A starts now.");
		fraction = (float)left / (float)AE_SOCIAL_LOBBY_STARTING_MS;
		break;
	}
	snprintf(clock, sizeof(clock), "%ld:%02ld", seconds / 60, seconds % 60);
	ctext(flow, AE_FONT_ROW, 32.0f, x, y + ae_flow_u(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, head);
	/* (the elapsed clock is hidden while the vote counts down: its seconds are the only ones that matter then) */
	if (!vote_view())
		ctext(flow, AE_FONT_BODY, 18.0f, x + width, y + ae_flow_u(flow, 16.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED, clock);
	ctext(flow, AE_FONT_BODY, 21.0f, x, y + ae_flow_u(flow, 56.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT, sub);
	if (l->stage >= AE_SOCIAL_STAGE_FOUND)
	{
		ae_draw_rect(x, y + ae_flow_u(flow, 85.0f), width, bar_h, 0.0f, AE_COLOR_RULE);
		ae_draw_rect(x, y + ae_flow_u(flow, 85.0f), width * fraction, bar_h, 0.0f, warn ? AE_COLOR_WARNING : AE_COLOR_ACCENT);
	}
	if (!l->fixed && !vote_begun())
		range_draw(x, y + ae_flow_u(flow, 120.0f), width);
}

/* ---------- the game card */

static void card_draw(float x, float y, float width)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_social_plan const *plan = &lobby.plan;
	float pad = ae_flow_u(flow, 23.0f), height = ae_flow_u(flow, 560.0f), pic_w = ae_flow_u(flow, 300.0f), pic_h = ae_flow_u(flow, 244.0f);
	char map[AE_SOCIAL_MAP], line[120], kind[64];

	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_PANEL);
	ae_draw_rect(x, y, width, ae_flow_u(flow, 4.0f), 0.0f, AE_COLOR_ACCENT);
	if (lobby.picture)
	{
		/* (the whole picture, its proportions kept; the rest of the box its middle, darkened) */
		ae_draw_image_whole(lobby.picture, x + pad, y + ae_flow_u(flow, 24.0f), pic_w, pic_h, 0xFFFFFFFFu);
	}
	else
		ae_draw_rect(x + pad, y + ae_flow_u(flow, 24.0f), pic_w, pic_h, 0.0f, AE_COLOR_ROW);
	if (strstr(plan->map, "@smp"))
	{
		tracked(flow, 18.0f, x + pad + pic_w + ae_flow_u(flow, 24.0f), y + ae_flow_u(flow, 40.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			"SOCIAL MAP PACK");
		ctext(flow, AE_FONT_BODY, 20.0f, x + pad + pic_w + ae_flow_u(flow, 24.0f), y + ae_flow_u(flow, 82.0f), AE_ALIGN_LEFT,
			AE_COLOR_TEXT, "Converted from your");
		ctext(flow, AE_FONT_BODY, 20.0f, x + pad + pic_w + ae_flow_u(flow, 24.0f), y + ae_flow_u(flow, 110.0f), AE_ALIGN_LEFT,
			AE_COLOR_TEXT, "MCC Workshop maps");
	}
	snprintf(kind, sizeof(kind), "%s \xC2\xB7 TOURNAMENT", plan->label);
	tracked(flow, 18.0f, x + pad, y + ae_flow_u(flow, 300.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, kind);
	map_caption(plan->map, map, sizeof(map));
	ctext(flow, AE_FONT_TITLE, 48.0f, x + pad, y + ae_flow_u(flow, 346.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, map);
	ctext(flow, AE_FONT_ROW, 28.0f, x + pad, y + ae_flow_u(flow, 414.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT, plan->gametype);
	{
		struct ae_social_playlist const *list = ae_social_playlist_by_id(plan->playlist);

		if (list)
			ae_social_view_shape(list, 1, line, (int)sizeof(line));
		if (list && list->teams == 2)
			snprintf(line, sizeof(line), "%d V %d", list->size, list->size);
		else if (list)
			snprintf(line, sizeof(line), "%d PLAYERS", list->size);
		else
			line[0] = 0;
	}
	/* (the start loadout and AE's weapons after the shape: "4 V 4 \xC2\xB7 BATTLE RIFLE \xC2\xB7 AE WEAPONS") */
	if (plan->start)
		snprintf(line + strlen(line), sizeof(line) - strlen(line), " \xC2\xB7 %s", ae_social_start_label(plan->start));
	if (plan->ae_weapons)
		snprintf(line + strlen(line), sizeof(line) - strlen(line), plan->ae_weapons == 2 ? " \xC2\xB7 MAP WEAPONS" :
			" \xC2\xB7 AE WEAPONS");
	tracked(flow, 18.0f, x + pad, y + ae_flow_u(flow, 452.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, line);
	ae_draw_rect(x + pad, y + ae_flow_u(flow, 482.0f), width - 2.0f * pad, ae_flow_u(flow, 1.0f), 0.0f, AE_COLOR_RULE);
	tracked(flow, 18.0f, x + pad, y + ae_flow_u(flow, 512.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, "BOT SKILL");
	ctext(flow, AE_FONT_ROW, 24.0f, x + width - pad, y + ae_flow_u(flow, 512.0f), AE_ALIGN_RIGHT, AE_COLOR_ACCENT,
		plan->mode ? ae_social_skill_name(plan->mode) : "AUTO");
	tracked(flow, 18.0f, x + pad, y + ae_flow_u(flow, 544.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, "A WIN OR A TIE");
	ctext(flow, AE_FONT_ROW, 24.0f, x + width - pad, y + ae_flow_u(flow, 544.0f), AE_ALIGN_RIGHT, AE_COLOR_ACCENT, "+1 EXP");
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
	float pic_w = (narrow ? 148.0f : 196.0f) * u, pic_h = (narrow ? 120.0f : 160.0f) * u;
	float tx = x + ae_flow_u(flow, 16.0f) + pic_w + ae_flow_u(flow, 24.0f), col_w = x + width - ae_flow_u(flow, 16.0f) - tx;
	float title_size = ae_flow_u(flow, narrow ? 32.0f : 40.0f), cap_h = ae_draw_cap_height(AE_FONT_TITLE, title_size);
	unsigned int ink = bar ? AE_COLOR_SELECTION_TEXT : dim ? AE_COLOR_MUTED : AE_COLOR_TITLE;
	unsigned int sub_ink = bar ? AE_COLOR_SELECTION_TEXT : dim ? AE_COLOR_MUTED : AE_COLOR_TEXT;
	unsigned int quiet = bar ? AE_COLOR_SELECTION_MUTED : AE_COLOR_MUTED;
	char fitted[64];
	float name_w, caption_mid, line_top, box_x;
	int voter, seat, mine = !split && lobby.vote.ballot[0] == index && !winner_moment;
	char count_text[16];

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
	if (lobby.tile[index].picture)
		ae_draw_image_whole(lobby.tile[index].picture, x + ae_flow_u(flow, 16.0f), y + ae_flow_u(flow, 8.0f), pic_w, pic_h,
			with_alpha(0xFFFFFFFFu, alpha));
	else
		ae_draw_rect(x + ae_flow_u(flow, 16.0f), y + ae_flow_u(flow, 8.0f), pic_w, pic_h, 0.0f, with_alpha(AE_COLOR_PANEL, alpha));
	/* (the map's name: cut with "..." only when even the smaller size does not fit) */
	ae_fit_text(AE_FONT_TITLE, title_size, lobby.tile[index].caption, col_w, fitted, (int)sizeof(fitted));
	ae_draw_text(AE_FONT_TITLE, title_size, tx, y + ae_flow_u(flow, 24.0f), AE_ALIGN_LEFT, ink, fitted);
	name_w = ae_draw_text_width(AE_FONT_TITLE, title_size, fitted);
	caption_mid = y + ae_flow_u(flow, 24.0f) + cap_h * 0.5f;
	box_x = tx + name_w + ae_flow_u(flow, 12.0f);
	if (winner_moment && index == lobby.vote.winner)
		badge_draw(box_x, caption_mid, bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT, "NEXT MAP");
	else if (mine && box_x + ae_draw_text_width(AE_FONT_ROW, ae_flow_u(flow, 13.0f), "YOUR VOTE") + ae_flow_u(flow, 14.0f) <= x + width)
		badge_draw(box_x, caption_mid, bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_MUTED, "YOUR VOTE");
	ae_fit_text(AE_FONT_ROW, ae_flow_u(flow, 24.0f), lobby.tile[index].type, col_w, fitted, (int)sizeof(fitted));
	ae_draw_text(AE_FONT_ROW, ae_flow_u(flow, 24.0f), tx, y + ae_flow_u(flow, 71.0f), AE_ALIGN_LEFT, sub_ink, fitted);
	line_top = y + ae_flow_u(flow, 102.0f);
	ae_draw_text_tracked(AE_FONT_BODY, ae_flow_u(flow, 16.0f), 0.09f, tx, line_top, AE_ALIGN_LEFT, quiet, lobby.tile[index].rules);
	/* the tally: a square a vote, in the voter's colour (a player's with the number) */
	for (voter = 0; (seat = ae_social_vote_voter(&lobby.vote, index, voter)) >= 0; voter++)
	{
		float sx = tx + ae_flow_u(flow, 24.0f) * (float)voter, sy = y + ae_flow_u(flow, 136.0f), side = ae_flow_u(flow, 18.0f);

		if (seat < humans)
		{
			char number[4];

			ae_draw_rect(sx, sy, side, side, ae_flow_u(flow, 2.0f), with_alpha(slot_color(seat), alpha));
			snprintf(number, sizeof(number), "%d", seat + 1);
			ae_draw_text(AE_FONT_ROW, ae_flow_u(flow, 13.0f), sx + side * 0.5f, ae_flow_cap_top(AE_FONT_ROW, ae_flow_u(flow, 13.0f),
				sy + side * 0.5f), AE_ALIGN_CENTER, AE_COLOR_TITLE, number);
		}
		else
			ae_draw_rect(sx, sy, side, side, ae_flow_u(flow, 2.0f), bar ? AE_COLOR_SELECTION_MUTED : with_alpha(AE_COLOR_TEXT, alpha));
	}
	voter = ae_social_vote_count(&lobby.vote, index);
	snprintf(count_text, sizeof(count_text), "%d VOTE%s", voter, voter == 1 ? "" : "S");
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
	char kind[64], text[160], wrapped[2][160];

	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_PANEL);
	ae_draw_rect(x, y, width, ae_flow_u(flow, 4.0f), 0.0f, AE_COLOR_ACCENT);
	snprintf(kind, sizeof(kind), "%s \xC2\xB7 TOURNAMENT", lobby.plan.label);
	ae_draw_text_tracked(AE_FONT_BODY, ae_flow_u(flow, 18.0f), 0.09f, x + pad, y + ae_flow_u(flow, 24.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, kind);
	ae_draw_text_tracked(AE_FONT_BODY, ae_flow_u(flow, 18.0f), 0.09f, x + width - pad, y + ae_flow_u(flow, 24.0f), AE_ALIGN_RIGHT,
		AE_COLOR_TEXT, "MAP VOTE");
	for (at = 0; at < count; at++)
		tile_draw(at, x + pad, y + ae_flow_u(flow, 56.0f) + (float)at * (tile_h + gap), width - 2.0f * pad, tile_h);
	if (winner_moment)
		return;
	snprintf(text, sizeof(text), "%s", lobby.plan.roster.humans > 1 ?
		"Each player votes with their own controller: the frame in your colour is your choice, A votes." :
		"A votes for the lit map; you can change it until the time is up. The most votes wins, and a tie is drawn at random.");
	lines = ae_wrap_text(AE_FONT_BODY, ae_flow_u(flow, 21.0f), text, width, wrapped, 2);
	for (at = 0; at < lines; at++)
		ae_draw_text(AE_FONT_BODY, ae_flow_u(flow, 21.0f), x, y + height + ae_flow_u(flow, 24.0f + 31.0f * (float)at), AE_ALIGN_LEFT,
			AE_COLOR_MUTED, wrapped[at]);
}

/* ---------- the screen */

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
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Vote", AE_ACTION_ACCEPT);
		else if (lobby.lobby.stage < AE_SOCIAL_STAGE_VOTE && !vote_begun() && lobby.lobby.stage <= AE_SOCIAL_STAGE_FOUND)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "To the vote", AE_ACTION_ACCEPT);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_START, "Start now", AE_ACTION_START);
	}
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Leave", AE_ACTION_BACK);
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
		(int)((AE_SOCIAL_VOTE_MS - lobby.vote.elapsed + 999) / 1000));
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
	platform_log("ae social vote: winner %s (%d/%d, tie drawn: %s)", vote->choice[winner].map, ae_social_vote_count(vote, winner),
		vote->seats, vote->tie ? "yes" : "no");
	ae_dev_event("social_vote", "\"phase\":\"winner\",\"votes\":[%d,%d,%d],\"winner\":%d,\"tie\":%d",
		ae_social_vote_count(vote, 0), ae_social_vote_count(vote, 1), vote->count > 2 ? ae_social_vote_count(vote, 2) : 0, winner,
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
		ae_social_vote_advance(&lobby.vote, AE_SOCIAL_VOTE_MS);
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
	ae_social_lobby_vote_seek(&lobby.lobby, lobby.dev_left >= 0 ? (long)(AE_SOCIAL_VOTE_MS - lobby.dev_left * 1000) : 0);
	/* (the set votes first: the bots whose time has come by then vote for themselves after them) */
	if (lobby.dev_has_votes)
		ae_social_vote_preset(&lobby.vote, lobby.dev_votes, lobby.vote.count);
	ae_social_vote_advance(&lobby.vote, ae_social_lobby_vote_elapsed(&lobby.lobby));
}

static void lobby_update(struct ae_screen *instance)
{
	unsigned long now = ae_motion_now();
	long passed = lobby.last_ms ? (long)(now - lobby.last_ms) : 0;

	(void)instance;
	lobby.last_ms = now;
	if (passed < 0 || passed > 1000)
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

static int lobby_handle(struct ae_screen *instance, struct ae_event const *event)
{
	int stage = lobby.lobby.stage;

	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		if (lobby.vote_on && stage == AE_SOCIAL_STAGE_VOTE)
			focus_step(event->player < 0 ? 0 : event->player, event->action == AE_ACTION_UP ? -1 : 1);
		return 1;
	case AE_ACTION_ACCEPT:
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
	count = ae_social_plan_vote_choices(&lobby.plan, choices, AE_SOCIAL_VOTE_CHOICES);
	if (count < 2)
		return;
	ae_social_vote_init(&lobby.vote, choices, count, lobby.plan.roster.count, lobby.plan.roster.humans, lobby.plan.seed);
	ae_social_lobby_vote_enable(&lobby.lobby, AE_SOCIAL_VOTE_MS);
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

static int lobby_open(char const *playlist, unsigned long seed, char const *map, int now, struct dev_vote const *dev)
{
	short local;
	char key[AE_SR_KEY];

	if (ae_ui_holds(&lobby))
	{
		platform_log("ae social lobby: already open: %s not opened", playlist ? playlist : "?");
		return 0;
	}
	memset(&lobby, 0, sizeof(lobby));
	if (!ae_social_plan_make(playlist, seed, map, NULL, &lobby.plan))
	{
		platform_log("ae social lobby: no plan for %s", playlist ? playlist : "?");
		return 0;
	}
	ae_social_lobby_init(&lobby.lobby, &lobby.plan.roster, lobby.plan.seed);
	ae_social_lobby_names(lobby.plan.seed, lobby.plan.roster.count - lobby.plan.roster.humans, lobby.names);
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
	vote_setup(!now && !(map && map[0]));
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
	platform_log("ae social lobby: open %s (seed %lu, map %s, %d seats%s)", lobby.plan.playlist, lobby.plan.seed, lobby.plan.map,
		lobby.plan.roster.count, lobby.vote_on ? ", map vote" : "");
	if (dev && lobby.vote_on)
	{
		vote_dev_open();
		stage_log();
	}
	if (lobby.vote_on)
		vote_log_open();
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
	if (!ae_screen_social_lobby_open(playlist, seed, map, now))
	{
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
	dev.left = left_seconds < 0 || left_seconds > AE_SOCIAL_VOTE_MS / 1000 ? -1 : left_seconds;
	dev.has_votes = has_votes;
	for (at = 0; at < AE_SOCIAL_VOTE_CHOICES && votes && has_votes; at++)
		dev.votes[at] = votes[at];
	dev_close_old();
	if (!lobby_open(playlist, seed, NULL, 0, &dev))
	{
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
