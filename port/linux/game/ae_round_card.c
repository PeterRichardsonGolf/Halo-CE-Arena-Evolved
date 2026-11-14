/*
AE_ROUND_CARD.C

The round card (ae_round_card.h): the layout B of the approved mockup (centred over the dimmed game), at 1080 tall and
scaled by the view's height, in AE's menus' layer. Colours and faces as the scoreboard's: AE_FONT_TITLE (OpenCE) for the
verdict, Overpass 900 for the numerals and names, Overpass 750 for the tracked labels.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "game/game.h"
#include "game/players.h"
#include "render/render.h"
#include "text/unicode.h"

#include "ae_hud_digits.h"
#include "ae_round.h"
#include "ae_round_card.h"
#include "ae_round_card_rules.h"
#include "ae_round_internal.h"
#include "ae_scoreboard.h"
#include "../src/ae_draw.h"

/* (sdl_platform.c's) the screen's size in pixels */
void platform_video_drawable_size(int *width, int *height);

/* the layout at 1080 (layout units; the mockup's, make.py): everything is a cap top or a baseline in a 1080-tall view,
x from the view's centre */
#define CARD_TITLE_EM 150.0f
#define CARD_TITLE_BASELINE 360.0f
#define CARD_REASON_EM 28.0f
#define CARD_REASON_CAP_TOP 397.0f
#define CARD_NUMERAL_EM 120.0f
#define CARD_NUMERAL_CAP_TOP 472.0f
#define CARD_NUMERAL_GAP 60.0f         /* each numeral's inner edge from the centre */
#define CARD_DIVIDER_HALF_HEIGHT 52.0f
#define CARD_DIVIDER_CENTRE 540.0f
#define CARD_SIDE_EM 26.0f             /* RED / BLUE */
#define CARD_SIDE_CAP_TOP 552.0f
#define CARD_SIDE_FROM_CENTRE 180.0f   /* the label's inner edge */
#define CARD_PANEL_W 760.0f
#define CARD_PANEL_TOP 680.0f
#define CARD_PANEL_H 92.0f
#define CARD_PANEL_PAD 28.0f
#define CARD_MVP_LABEL_EM 20.0f
#define CARD_MVP_LABEL_CAP_TOP 699.0f
#define CARD_MVP_CHIP 26.0f
#define CARD_MVP_CHIP_TOP 730.0f
#define CARD_MVP_NAME_EM 32.0f
#define CARD_MVP_NAME_CAP_TOP 728.0f
#define CARD_MVP_KILLS_EM 30.0f
#define CARD_MVP_KILLS_CAP_TOP 730.0f
#define CARD_FOOT_EM 26.0f
#define CARD_FOOT_CAP_TOP 904.0f
#define CARD_TRACK 0.12f
#define CARD_DIM_ALPHA 0.88f
#define CARD_SPLIT_FLOOR 0.62f         /* a split view's scale, at least, of the full screen's (the scoreboard's) */
#define CARD_SIDE_MARGIN 24.0f         /* the least room left and right of the widest line, at 1080 */

static unsigned int const ink = 0x04070AFFu;
static unsigned int const panel = 0x14191FFFu;
static unsigned int const muted = 0x84939FFFu;
static unsigned int const white = 0xEEF2F6FFu;
static unsigned int const ice = 0x5FC8ECFFu;
static unsigned int const grey = 0xB0BAC4FFu;
static unsigned int const divider = 0x3C4650FFu;
static unsigned int const team_colour[2] = { 0xC0392BFFu, 0x2E86DEFFu };

/* what a view's card says */
struct card
{
	int verdict;                 /* enum ae_round_card_verdict */
	int reason;
	boolean teams;
	int wins_red, wins_blue, wins_me;
	boolean mvp;
	char mvp_name[40];
	int mvp_team;                /* 0 red, 1 blue, -1 none */
	int mvp_kills;
	int next_round, max_rounds;
	float seconds;               /* since the card began */
};

/* DEV MODE's staged card: the local views', 3 s from the tick it began */
static struct { boolean on; long start; int verdict; int reason; } dev;

static void utf8_of(wchar_t const *text, char *out, int size)
{
	int at = 0;

	for (; text && *text && at < size - 4; text++)
	{
		unsigned long c = (unsigned long)(unsigned short)*text;

		if (c < 0x80)
			out[at++] = (char)c;
		else if (c < 0x800)
		{
			out[at++] = (char)(0xC0 | (c >> 6));
			out[at++] = (char)(0x80 | (c & 0x3F));
		}
		else
		{
			out[at++] = (char)(0xE0 | (c >> 12));
			out[at++] = (char)(0x80 | ((c >> 6) & 0x3F));
			out[at++] = (char)(0x80 | (c & 0x3F));
		}
	}
	out[at] = 0;
}

static struct player_datum *player_of_absolute(long absolute)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		if (DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) == absolute)
			return player;
	return NULL;
}

/* the card this view shows now, FALSE when none: the dev stage's, else the round state's POST with its END tail */
static boolean card_of(short local_player_index, struct card *card)
{
	struct ae_round_wire const *view;
	long viewer_index = local_player_exists(local_player_index) ? local_player_get_player_index(local_player_index) : NONE;
	struct player_datum *viewer = viewer_index != NONE ? player_try_and_get(viewer_index) : NULL;

	memset(card, 0, sizeof(*card));
	card->mvp_team = -1;
	if (dev.on)
	{
		card->seconds = (float)(game_time_get() - dev.start) / (float)TICKS_PER_SECOND;
		if (card->seconds < 0.0f || card->seconds >= (float)AE_ROUND_CARD_SECONDS)
		{
			dev.on = FALSE;
			return FALSE;
		}
		card->verdict = dev.verdict;
		card->reason = dev.reason;
		card->teams = TRUE;
		card->wins_red = 2;
		card->wins_blue = 1;
		card->mvp = TRUE;
		snprintf(card->mvp_name, sizeof(card->mvp_name), "Kestrel");
		card->mvp_team = 0;
		card->mvp_kills = 3;
		card->next_round = 4;
		card->max_rounds = 7;
		return TRUE;
	}
	view = ae_round_card_view();
	if (!view || !viewer || view->post_end <= 0)
		return FALSE;
	card->seconds = (float)(game_time_get() - (view->post_end - AE_ROUND_POST_TICKS)) / (float)TICKS_PER_SECOND;
	if (card->seconds < 0.0f || card->seconds >= (float)AE_ROUND_CARD_SECONDS)
		return FALSE;
	card->teams = (view->flags & AE_ROUND_WIRE_TEAMS) != 0;
	card->verdict = ae_round_card_verdict(view->last_result, card->teams, viewer->team_index,
		(int)DATUM_INDEX_TO_ABSOLUTE_INDEX(viewer_index));
	if (card->verdict == AE_ROUND_CARD_NONE)
		return FALSE;
	card->reason = view->reason;
	card->wins_red = view->wins_red;
	card->wins_blue = view->wins_blue;
	{
		int index;

		for (index = 0; index < view->count; index++)
			if (view->players[index].player == DATUM_INDEX_TO_ABSOLUTE_INDEX(viewer_index))
				card->wins_me = view->players[index].wins;
	}
	if (view->mvp_player != AE_ROUND_WIRE_NO_PLAYER)
	{
		struct player_datum *mvp = player_of_absolute(view->mvp_player);
		wchar_t buffer[48];

		if (mvp)
		{
			utf8_of(ae_game_engine_scoreboard_name(view->mvp_player, buffer, NUMBEROF(buffer)), card->mvp_name,
				sizeof(card->mvp_name));
			card->mvp = card->mvp_name[0] != 0;
			card->mvp_team = card->teams && mvp->team_index >= 0 && mvp->team_index < 2 ? mvp->team_index : -1;
			card->mvp_kills = view->mvp_kills;
		}
	}
	card->next_round = view->round + 1;
	card->max_rounds = view->max_rounds;
	return TRUE;
}

boolean ae_round_card_hides_hud(short local_player_index)
{
	struct card card;

	return (boolean)(card_of(local_player_index, &card) && ae_round_card_alpha(card.seconds) > 0.0f);
}

/* a view's drawing scale and centre */
struct frame
{
	float s, cx, width;
};

static float px_x(struct frame const *f, float x) { return f->cx + (x - 960.0f) * f->s; }
static float px_y(float y, struct frame const *f) { return 540.0f + (y - 540.0f) * f->s; }

/* a label: tracked caps, its cap top at 1080's y */
static void text_at(struct frame const *f, int font, float em, float tracking, float x, float cap_top, int align,
	unsigned int colour, char const *text)
{
	ae_draw_text_tracked(font, em * f->s, tracking, px_x(f, x), px_y(cap_top, f), align, colour, text);
}

static void draw_card(struct card const *card, struct frame const *f, boolean with_mvp)
{
	unsigned int verdict_colour = card->verdict == AE_ROUND_CARD_WON ? ice : card->verdict == AE_ROUND_CARD_LOST ? grey : white;
	char const *verdict_text = card->verdict == AE_ROUND_CARD_WON ? "ROUND WON" : card->verdict == AE_ROUND_CARD_LOST ?
		"ROUND LOST" : "ROUND TIED";
	char const *reason = ae_round_card_reason_text(card->reason);
	char text[64];
	float numerals_top = CARD_NUMERAL_CAP_TOP;

	/* (the verdict: the title face, its baseline on the mockup's line) */
	ae_draw_text(AE_FONT_TITLE, CARD_TITLE_EM * f->s, px_x(f, 960.0f),
		px_y(CARD_TITLE_BASELINE, f) - ae_draw_cap_height(AE_FONT_TITLE, CARD_TITLE_EM * f->s), AE_ALIGN_CENTER,
		verdict_colour, verdict_text);
	if (reason)
		text_at(f, AE_FONT_BODY, CARD_REASON_EM, CARD_TRACK, 960.0f, CARD_REASON_CAP_TOP, AE_ALIGN_CENTER, muted, reason);
	/* (the round score: red and blue numerals either side of a rule, their labels outside; without teams the viewer's wins) */
	if (card->teams)
	{
		snprintf(text, sizeof(text), "%d", card->wins_red);
		text_at(f, AE_FONT_ROW, CARD_NUMERAL_EM, 0.0f, 960.0f - CARD_NUMERAL_GAP, numerals_top, AE_ALIGN_RIGHT, team_colour[0], text);
		snprintf(text, sizeof(text), "%d", card->wins_blue);
		text_at(f, AE_FONT_ROW, CARD_NUMERAL_EM, 0.0f, 960.0f + CARD_NUMERAL_GAP, numerals_top, AE_ALIGN_LEFT, team_colour[1], text);
		ae_draw_rect(px_x(f, 958.0f), px_y(CARD_DIVIDER_CENTRE - CARD_DIVIDER_HALF_HEIGHT, f), 4.0f * f->s,
			2.0f * CARD_DIVIDER_HALF_HEIGHT * f->s, 0.0f, divider);
		text_at(f, AE_FONT_BODY, CARD_SIDE_EM, CARD_TRACK, 960.0f - CARD_NUMERAL_GAP - 120.0f, CARD_SIDE_CAP_TOP, AE_ALIGN_RIGHT, muted, "RED");
		text_at(f, AE_FONT_BODY, CARD_SIDE_EM, CARD_TRACK, 960.0f + CARD_NUMERAL_GAP + 120.0f, CARD_SIDE_CAP_TOP, AE_ALIGN_LEFT, muted, "BLUE");
	}
	else
	{
		snprintf(text, sizeof(text), "%d", card->wins_me);
		text_at(f, AE_FONT_ROW, CARD_NUMERAL_EM, 0.0f, 960.0f, numerals_top, AE_ALIGN_CENTER, white, text);
		text_at(f, AE_FONT_BODY, CARD_SIDE_EM, CARD_TRACK, 960.0f, CARD_NUMERAL_CAP_TOP + 134.0f, AE_ALIGN_CENTER, muted, "WINS");
	}
	/* (the round's MVP) */
	if (with_mvp && card->mvp)
	{
		float left = 960.0f - CARD_PANEL_W / 2.0f, name_room, name_width;
		char name[40], kills[24];

		ae_draw_rect(px_x(f, left), px_y(CARD_PANEL_TOP, f), CARD_PANEL_W * f->s, CARD_PANEL_H * f->s, 0.0f, panel);
		text_at(f, AE_FONT_BODY, CARD_MVP_LABEL_EM, CARD_TRACK, left + CARD_PANEL_PAD, CARD_MVP_LABEL_CAP_TOP, AE_ALIGN_LEFT, muted,
			"ROUND MVP");
		ae_draw_rect(px_x(f, left + CARD_PANEL_PAD), px_y(CARD_MVP_CHIP_TOP, f), CARD_MVP_CHIP * f->s, CARD_MVP_CHIP * f->s, 0.0f,
			card->mvp_team >= 0 ? team_colour[card->mvp_team] : white);
		ae_round_card_kills_text(card->mvp_kills, kills, sizeof(kills));
		text_at(f, AE_FONT_ROW, CARD_MVP_KILLS_EM, 0.0f, left + CARD_PANEL_W - CARD_PANEL_PAD, CARD_MVP_KILLS_CAP_TOP, AE_ALIGN_RIGHT, white,
			kills);
		/* (a long name is cut to the room between the chip and the kills) */
		strncpy(name, card->mvp_name, sizeof(name) - 1);
		name[sizeof(name) - 1] = 0;
		name_room = CARD_PANEL_W - 68.0f - CARD_PANEL_PAD - ae_draw_text_width(AE_FONT_ROW, CARD_MVP_KILLS_EM, kills) - 24.0f;
		name_width = ae_draw_text_width(AE_FONT_ROW, CARD_MVP_NAME_EM, name);
		while (name_width > name_room && strlen(name) > 1)
		{
			size_t length = strlen(name);

			/* (back over a UTF-8 sequence's continuation bytes) */
			do length--; while (length > 1 && (name[length] & 0xC0) == 0x80);
			name[length] = 0;
			name_width = ae_draw_text_width(AE_FONT_ROW, CARD_MVP_NAME_EM, name);
		}
		text_at(f, AE_FONT_ROW, CARD_MVP_NAME_EM, 0.0f, left + 68.0f, CARD_MVP_NAME_CAP_TOP, AE_ALIGN_LEFT, white, name);
	}
	/* (the footer: the next round and its count) */
	ae_round_card_footer(card->next_round, card->max_rounds, (int)ceilf((float)AE_ROUND_CARD_SECONDS - card->seconds),
		text, sizeof(text));
	text_at(f, AE_FONT_BODY, CARD_FOOT_EM, CARD_TRACK, 960.0f, CARD_FOOT_CAP_TOP, AE_ALIGN_CENTER, white, text);
}

void ae_round_card_draw(short local_player_index)
{
	struct card card;
	float alpha;
	int screen_w = 0, screen_h = 0;
	boolean split = local_player_count() > 1;
	struct frame f;
	struct ae_layout layout;
	float view_x, view_y, view_w, view_h;

	if (!ae_draw_available() || !card_of(local_player_index, &card))
		return;
	alpha = ae_round_card_alpha(card.seconds);
	if (alpha <= 0.0f)
		return;
	platform_video_drawable_size(&screen_w, &screen_h);
	ae_draw_current_layout(&layout);
	if (screen_h <= 0 || layout.height <= 0.0f)
		return;
	if (split)
	{
		/* (this view's rectangle, from the game's pixels to layout units) */
		float ppu = ae_hud_pixels_per_unit(), per_pixel = layout.scale > 0.0f ? 1.0f / layout.scale : 1.0f;

		view_x = ((float)render.camera.viewport_bounds.x0 * ppu - layout.origin_x) * per_pixel;
		view_y = ((float)render.camera.viewport_bounds.y0 * ppu - layout.origin_y) * per_pixel;
		view_w = (float)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) * ppu * per_pixel;
		view_h = (float)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * ppu * per_pixel;
		if (view_w <= 0.0f || view_h <= 0.0f)
			return;
		ae_draw_view(view_x, view_y, view_w, view_h);
		f.width = ae_draw_view_width();
	}
	else
	{
		ae_draw_view_full();
		view_w = layout.width;
		view_h = layout.height;
		view_x = view_y = 0.0f;
		f.width = ae_draw_view_width();
	}
	/* (the view's own units are 1080 tall; a split view's card no smaller than CARD_SPLIT_FLOOR of the full screen's, and
	no wider than the view) */
	f.s = 1.0f;
	if (split)
		f.s = MAX(1.0f, CARD_SPLIT_FLOOR * layout.height / view_h);
	f.s = MIN(f.s, (f.width - 2.0f * CARD_SIDE_MARGIN) / 800.0f);
	f.s = MIN(f.s, 1.35f);
	if (f.s <= 0.1f)
		return;
	f.cx = f.width * 0.5f;
	ae_draw_set_alpha(alpha);
	ae_draw_rect(0.0f, 0.0f, f.width, 1080.0f, 0.0f, (ink & 0xFFFFFF00u) | (unsigned int)(CARD_DIM_ALPHA * 255.0f + 0.5f));
	/* (four views or more drop the MVP panel: it does not fit under the score) */
	draw_card(&card, &f, local_player_count() <= 2);
	ae_draw_set_alpha(1.0f);
	ae_draw_view_full();
	(void)view_x;
	(void)view_y;
	(void)view_w;
}

boolean ae_round_card_dev_stage(char const *verdict, char const *reason, char *answer, int size)
{
	int which, why = AE_ROUND_REASON_NONE;
	static struct { char const *word; int reason; } const words[] =
	{
		{ "red", AE_ROUND_REASON_RED_ELIMINATED }, { "blue", AE_ROUND_REASON_BLUE_ELIMINATED },
		{ "detonated", AE_ROUND_REASON_BOMB_DETONATED }, { "bomb", AE_ROUND_REASON_BOMB_DETONATED },
		{ "defused", AE_ROUND_REASON_BOMB_DEFUSED }, { "time", AE_ROUND_REASON_TIME_RAN_OUT },
		{ "exterminat", AE_ROUND_REASON_EXTERMINATION }, { "standing", AE_ROUND_REASON_LAST_STANDING },
		{ "last", AE_ROUND_REASON_LAST_STANDING }, { "level", AE_ROUND_REASON_LEVEL_ON_KILLS },
		{ "eliminated", AE_ROUND_REASON_BLUE_ELIMINATED }
	};

	which = !strcmp(verdict, "won") ? AE_ROUND_CARD_WON : !strcmp(verdict, "lost") ? AE_ROUND_CARD_LOST :
		!strcmp(verdict, "tied") ? AE_ROUND_CARD_TIED : -1;
	if (which < 0)
	{
		snprintf(answer, (size_t)size, "roundcard won|lost|tied [reason]");
		return FALSE;
	}
	if (reason && reason[0])
	{
		size_t index;

		why = -1;
		for (index = 0; index < sizeof(words) / sizeof(words[0]); index++)
			if (strstr(reason, words[index].word))
			{
				why = words[index].reason;
				break;
			}
		if (why < 0)
		{
			snprintf(answer, (size_t)size, "the reason is one of: red, blue, bomb, defused, time, exterminat, standing, level");
			return FALSE;
		}
	}
	else if (which == AE_ROUND_CARD_WON)
		why = AE_ROUND_REASON_BLUE_ELIMINATED;
	else if (which == AE_ROUND_CARD_LOST)
		why = AE_ROUND_REASON_RED_ELIMINATED;
	dev.on = TRUE;
	dev.start = game_time_get();
	dev.verdict = which;
	dev.reason = why;
	snprintf(answer, (size_t)size, "the round card (%s) shows for %d s", verdict, AE_ROUND_CARD_SECONDS);
	return TRUE;
}
