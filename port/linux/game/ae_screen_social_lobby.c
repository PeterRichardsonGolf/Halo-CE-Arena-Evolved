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
#include "ae_glue_setup_maps.h"
#include "ae_input.h"
#include "ae_motion.h"
#include "ae_screen_social_lobby.h"
#include "ae_service_record.h"
#include "ae_setup_flow.h"
#include "ae_social_lobby_rules.h"
#include "ae_social_view_rules.h"
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
} lobby;

static struct ae_screen_class const lobby_class;

/* ---------- the rows */

enum { ITEM_SEAT, ITEM_HEADER, ITEM_EMPTY, ITEM_MORE };
struct item { int kind, seat, team; };

/* the roster as drawn now: before STARTING your party first, then the bots as they joined, empty seats at the end; from STARTING the
seats group under their team's header */
static int items_make(struct item *items, int max)
{
	struct ae_social_roster const *roster = &lobby.plan.roster;
	int count = 0, seat, order, team;

	if (lobby.lobby.stage >= AE_SOCIAL_STAGE_STARTING && roster->teams > 0)
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
			char name[AE_SOCIAL_LOBBY_NAME + 4], initial[4], right[40];
			unsigned int emblem_color = human ? (it->team == 1 ? 0x2E86DEFFu : 0xC0392BFFu) : 0x3A4651FFu;
			float name_x = x + ae_flow_u(flow, 64.0f), name_w;

			seat_name(seat, name, sizeof(name));
			ae_draw_rect(x, top, width, height, 0.0f, AE_COLOR_ROW);
			if (lobby.lobby.stage >= AE_SOCIAL_STAGE_STARTING && roster->teams > 0)
				ae_draw_rect(x, top, ae_flow_u(flow, 6.0f), height, 0.0f, team_color(it->team));
			if (fresh)
				ae_draw_rect(x, top, ae_flow_u(flow, 6.0f), height, 0.0f, EXP_ICE);
			if (human && roster->teams > 0)
				emblem_color = team_color(roster->seat[seat].team);
			ae_draw_rect(x + ae_flow_u(flow, 20.0f), mid - emblem * 0.5f, emblem, emblem, ae_flow_u(flow, 2.0f), emblem_color);
			snprintf(initial, sizeof(initial), "%c", name[0] ? name[0] : '?');
			ctext(flow, AE_FONT_ROW, 20.0f, x + ae_flow_u(flow, 20.0f) + emblem * 0.5f, mid, AE_ALIGN_CENTER, AE_COLOR_TITLE, initial);
			ctext(flow, AE_FONT_ROW, 26.0f, name_x, mid, AE_ALIGN_LEFT, human ? AE_COLOR_TITLE : AE_COLOR_TITLE, name);
			name_w = ae_draw_text_width(AE_FONT_ROW, ae_flow_u(flow, 26.0f), name);
			if (fresh)
				ctext(flow, AE_FONT_ROW, 16.0f, name_x + name_w + ae_flow_u(flow, 14.0f), mid, AE_ALIGN_LEFT, EXP_ICE, "JOINED");
			if (human)
				ae_social_view_rank(lobby.rank[seat < AE_SOCIAL_LOCAL ? seat : 0], right, (int)sizeof(right));
			else if (ae_dev_mode_on())
				snprintf(right, sizeof(right), "BOT \xC2\xB7 T%d", roster->seat[seat].tier);
			else
				snprintf(right, sizeof(right), "BOT");
			tracked(flow, 18.0f, x + width - ae_flow_u(flow, 20.0f), mid, AE_ALIGN_RIGHT, human ? AE_COLOR_TEXT : AE_COLOR_MUTED, right);
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
	int joined = ae_social_lobby_joined(l);

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
	default:
		left = ae_social_lobby_countdown_ms(l);
		snprintf(head, sizeof(head), "STARTING IN %ld", (left + 999) / 1000);
		snprintf(sub, sizeof(sub), "Teams are set. A starts now.");
		fraction = (float)left / (float)AE_SOCIAL_LOBBY_STARTING_MS;
		break;
	}
	snprintf(clock, sizeof(clock), "%ld:%02ld", seconds / 60, seconds % 60);
	ctext(flow, AE_FONT_ROW, 32.0f, x, y + ae_flow_u(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_TITLE, head);
	ctext(flow, AE_FONT_BODY, 18.0f, x + width, y + ae_flow_u(flow, 16.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED, clock);
	ctext(flow, AE_FONT_BODY, 21.0f, x, y + ae_flow_u(flow, 56.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT, sub);
	if (l->stage >= AE_SOCIAL_STAGE_FOUND)
	{
		ae_draw_rect(x, y + ae_flow_u(flow, 85.0f), width, bar_h, 0.0f, AE_COLOR_RULE);
		ae_draw_rect(x, y + ae_flow_u(flow, 85.0f), width * fraction, bar_h, 0.0f, AE_COLOR_ACCENT);
	}
	if (!l->fixed)
		range_draw(x, y + ae_flow_u(flow, 120.0f), width);
}

/* ---------- the game card */

static void card_draw(float x, float y, float width)
{
	struct ae_flow_screen *flow = &lobby.flow;
	struct ae_social_plan const *plan = &lobby.plan;
	float pad = ae_flow_u(flow, 23.0f), height = ae_flow_u(flow, 560.0f), pic_w = ae_flow_u(flow, 300.0f), pic_h = ae_flow_u(flow, 244.0f);
	char map[AE_SOCIAL_MAP], line[120], kind[64];
	int at;

	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_PANEL);
	ae_draw_rect(x, y, width, ae_flow_u(flow, 4.0f), 0.0f, AE_COLOR_ACCENT);
	if (lobby.picture)
	{
		float aspect = lobby.picture_h > 0 ? (float)lobby.picture_w / (float)lobby.picture_h : 1.0f;
		float w = pic_h * aspect;

		ae_draw_image(lobby.picture, x + pad, y + ae_flow_u(flow, 24.0f), w > pic_w ? pic_w : w, pic_h, 0xFFFFFFFFu);
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
	snprintf(map, sizeof(map), "%s", plan->map);
	for (at = 0; map[at]; at++)
	{
		if (map[at] == '@')
		{
			map[at] = 0;
			break;
		}
		map[at] = (char)((map[at] >= 'a' && map[at] <= 'z') ? map[at] - 32 : map[at] == '_' ? ' ' : map[at]);
	}
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
	if (lobby.lobby.stage >= AE_SOCIAL_STAGE_FOUND)
		card_draw(flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X, top, flow->frame.rect.x + flow->frame.rect.width -
			flow->frame.margin - (flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X));
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Start now", AE_ACTION_ACCEPT);
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
		static char const *const names[] = { "SEARCHING", "FOUND", "STARTING", "GO", "CANCELLED" };

		platform_log("ae social lobby: stage %s", stage >= 0 && stage <= 4 ? names[stage] : "?");
		ae_dev_event("social_lobby", "\"stage\":\"%s\",\"seats\":%d", stage >= 0 && stage <= 4 ? names[stage] : "?",
			ae_social_lobby_joined(&lobby.lobby));
	}
}

static void lobby_update(struct ae_screen *instance)
{
	unsigned long now = ae_motion_now();
	long passed = lobby.last_ms ? (long)(now - lobby.last_ms) : 0;

	(void)instance;
	lobby.last_ms = now;
	if (passed < 0 || passed > 1000)
		passed = 0;
	if (ae_motion_reduced() && lobby.lobby.stage == AE_SOCIAL_STAGE_SEARCHING)
		passed += 0;
	ae_social_lobby_advance(&lobby.lobby, passed);
	stage_log();
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

static int lobby_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_ACCEPT:
	case AE_ACTION_START:
		if (ae_social_lobby_skip(&lobby.lobby))
			ae_sound_request(AE_SOUND_FORWARD, 0);
		return 1;
	case AE_ACTION_BACK:
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

	if (ae_prompts_pointer(lobby.flow.prompts, lobby.flow.prompt_count, pointer, HIT_PROMPTS, &event))
		lobby_handle(instance, &event);
}

static struct ae_screen_class const lobby_class =
{
	.name = "social_lobby", .handle = lobby_handle, .draw = lobby_draw, .pointer = lobby_pointer, .update = lobby_update,
};

int ae_screen_social_lobby_open(char const *playlist, unsigned long seed, char const *map, int now)
{
	short local;
	char key[AE_SR_KEY];
	int each;

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
	for (each = 0; each < lobby.plan.roster.humans; each++)
		;
	lobby.picture = ae_setup_map_picture_named(lobby.plan.map, &lobby.picture_w, &lobby.picture_h);
	lobby.flow.hit_rows = HIT_LOBBY;
	lobby.flow.hit_prompts = HIT_PROMPTS;
	lobby.last_stage = -1;
	if (!ae_ui_push(&lobby_class, AE_OWNER_ANY, &lobby))
	{
		platform_log("ae social lobby: the screen could not be pushed: %s not opened", lobby.plan.playlist);
		return 0;
	}
	lobby.open = 1;
	if (now)
		ae_social_lobby_skip(&lobby.lobby);
	platform_log("ae social lobby: open %s (seed %lu, map %s, %d seats)", lobby.plan.playlist, lobby.plan.seed, lobby.plan.map,
		lobby.plan.roster.count);
	return 1;
}

void ae_screen_social_lobby_close(void)
{
	if (lobby.open)
		ae_ui_remove(&lobby);
	lobby.open = 0;
}

int ae_social_game_dev_lobby(char const *playlist, char const *map, unsigned long seed, int now, char *text, size_t size)
{
	/* a lobby left open by an earlier (TEST RUN) scene holds the screen: close it first */
	if (ae_ui_holds(&lobby))
	{
		platform_log("ae social lobby: an earlier lobby was still open: closed for social start");
		ae_ui_remove(&lobby);
		lobby.open = 0;
	}
	if (!ae_screen_social_lobby_open(playlist, seed, map, now))
	{
		snprintf(text, size, "social start: the lobby did not open (%s)", playlist);
		return 0;
	}
	snprintf(text, size, "social start %s: lobby open%s", playlist, now ? ", game loading" : "");
	return 1;
}
