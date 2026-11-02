/*
AE_SCOREBOARD.C

(0.4.0) HUD STYLE AE's in-game scoreboard (ae_scoreboard.h).
*/

#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "render/render.h"
#include "text/unicode.h"

#include "ae_header_lines.h"
#include "ae_hud_digits.h"
#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "halo_port_limits.h"

#include "ae_scoreboard.h"
#include "ae_scoreboard_header.h"
#include "ae_postgame.h"
#include "latency_meter.h"
#include "../src/ae_draw.h"

/* the layout at 1080 (screen pixels, times the view's height / 1080) */
#define SB_WIDTH 900.0f          /* one list's (the stacked teams', FFA's) */
#define SB_SIDE_WIDTH 820.0f     /* each team's, side by side */
#define SB_SIDE_GAP 40.0f
#define SB_TOP 60.0f             /* the title's top, from the view's */
#define SB_FOOT 120.0f           /* the room under the last row (the score strip's) */
#define SB_SIDE_MARGIN 40.0f     /* the least room left and right of the panel */
#define SB_TITLE 30.0f           /* the gametype's size */
#define SB_SUB 17.0f             /* the map and goal line's */
#define SB_HEAD_GAP 66.0f        /* the title's top to the rule */
#define SB_RULE_GAP 12.0f
#define SB_BAR 46.0f             /* a team's bar (36 compact) */
#define SB_BAR_COMPACT 36.0f
#define SB_HEADS 22.0f           /* the column heads' line (18 compact) */
#define SB_ROW 40.0f             /* a row (28 compact) and the gap under it */
#define SB_ROW_COMPACT 28.0f
#define SB_ROW_GAP 4.0f
#define SB_ROW_GAP_COMPACT 2.0f
#define SB_BLOCK_GAP 18.0f       /* between the stacked teams */
#define SB_MORE 24.0f            /* an "N MORE" / "N ABOVE" line */
#define SB_COLUMN 78.0f          /* a number column's width (PING 96) */
#define SB_PING_COLUMN 96.0f
#define SB_SPREE_COLUMN 140.0f  /* BEST SPREE's (the post-game's) */
#define SB_EDGE 18.0f            /* a row's text from its sides */
#define SB_CHIP 60.0f            /* the colour chip's left in a team row, and the name's */
#define SB_NAME 88.0f
#define SB_FFA_CHIP 76.0f        /* FFA's, after its places (1ST, 128TH) */
#define SB_FFA_NAME 108.0f
#define SB_BIG_ROWS 16           /* up to this many players: full rows */
#define SB_STACKED_MAXIMUM 24    /* teams stacked up to this many players, side by side to SB_SIDE_MAXIMUM */
#define SB_SIDE_MAXIMUM 40
#define SB_FFA_PINNED 3          /* FFA's top places kept above a scrolled list */
#define SB_WHEEL_STEP 3
#define SB_SPLIT_FLOOR 0.62f     /* a split view's scale, at least, of the full screen's */

/* (sdl_platform.c's) the screen's size in pixels */
void platform_video_drawable_size(int *width, int *height);

typedef struct { real r, g, b; } sb_rgb;

static const sb_rgb text_rgb = { 205.0f / 255.0f, 214.0f / 255.0f, 221.0f / 255.0f };
static const sb_rgb title_rgb = { 238.0f / 255.0f, 243.0f / 255.0f, 246.0f / 255.0f };
static const sb_rgb muted_rgb = { 132.0f / 255.0f, 147.0f / 255.0f, 159.0f / 255.0f };
static const sb_rgb sel_rgb = { 232.0f / 255.0f, 238.0f / 255.0f, 242.0f / 255.0f };
static const sb_rgb selt_rgb = { 15.0f / 255.0f, 23.0f / 255.0f, 30.0f / 255.0f };
static const sb_rgb accent_rgb = { 95.0f / 255.0f, 200.0f / 255.0f, 236.0f / 255.0f };
static const sb_rgb row_rgb = { 32.0f / 255.0f, 40.0f / 255.0f, 48.0f / 255.0f };
static const sb_rgb rule_rgb = { 200.0f / 255.0f, 214.0f / 255.0f, 224.0f / 255.0f };
static const sb_rgb scrim_rgb = { 4.0f / 255.0f, 7.0f / 255.0f, 10.0f / 255.0f };
static const sb_rgb team_rgb[2] = {
	{ 192.0f / 255.0f, 57.0f / 255.0f, 43.0f / 255.0f },
	{ 46.0f / 255.0f, 134.0f / 255.0f, 222.0f / 255.0f } };
static const sb_rgb white_rgb = { 1.0f, 1.0f, 1.0f };

#define SB_ROW_ALPHA (189.0f / 255.0f)
#define SB_SCRIM_ALPHA (165.0f / 255.0f)
#define SB_BAR_ALPHA (235.0f / 255.0f)
#define SB_RULE_ALPHA 0.2f

static real_argb_color argb(const sb_rgb *rgb, real alpha)
{
	real_argb_color color;

	color.alpha = alpha;
	color.red = rgb->r;
	color.green = rgb->g;
	color.blue = rgb->b;
	return color;
}

/* where the board draws: the HUD's pass (screen pixels: the game's scoreboard), or AE's menus' drawing (layout units,
over the whole picture at Present: the post-game's CARNAGE, which the game draws no HUD under) */
static boolean sb_ae_layer = FALSE;

static unsigned int rgba_of(const sb_rgb *rgb, real alpha)
{
	return ((unsigned int)(PIN(rgb->r, 0.0f, 1.0f) * 255.0f + 0.5f) << 24) |
		((unsigned int)(PIN(rgb->g, 0.0f, 1.0f) * 255.0f + 0.5f) << 16) |
		((unsigned int)(PIN(rgb->b, 0.0f, 1.0f) * 255.0f + 0.5f) << 8) |
		(unsigned int)(PIN(alpha, 0.0f, 1.0f) * 255.0f + 0.5f);
}

/* UTF-16 to UTF-8 (AE's drawing's text) */
static const char *utf8(const wchar_t *text, char *out, long size)
{
	long at = 0;

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
	return out;
}

/* AE's faces: the small muted words (heads, "N MORE", DEAD) Overpass 750, the rest 900 (the mockups') */
static int ae_font(real px)
{
	return px < 17.5f ? AE_FONT_BODY : AE_FONT_ROW;
}

static real measure(const wchar_t *text, real px)
{
	char buffer[128];

	if (sb_ae_layer)
		return ae_draw_text_width(ae_font(px), px, utf8(text, buffer, sizeof(buffer)));
	return ae_hud_text_measure(text, px);
}

static void quads_begin(void)
{
	if (!sb_ae_layer)
		ae_hud_quads_begin();
}

static void quads_end(void)
{
	if (!sb_ae_layer)
		ae_hud_quads_end();
}

static void box(real x, real y, real w, real h, const sb_rgb *rgb, real alpha)
{
	real points[4][2];
	real_argb_color color = argb(rgb, alpha);

	if (w <= 0.0f || h <= 0.0f || alpha <= 0.0f)
		return;
	if (sb_ae_layer)
	{
		ae_draw_rect(x, y, w, h, 0.0f, rgba_of(rgb, alpha));
		return;
	}
	points[0][0] = x; points[0][1] = y;
	points[1][0] = x + w; points[1][1] = y;
	points[2][0] = x + w; points[2][1] = y + h;
	points[3][0] = x; points[3][1] = y + h;
	ae_hud_fill_quad_color(points, &color);
}

/* text centred on y (its capitals'), at x by align: 0 left, 1 right, 2 centre */
static void label(real x, real cy, const wchar_t *text, real px, const sb_rgb *rgb, real alpha, short align)
{
	real_argb_color color = argb(rgb, alpha);
	real baseline = cy + 0.5f * AE_HUD_TEXT_CAP * px;

	if (!text || !text[0])
		return;
	if (sb_ae_layer)
	{
		char buffer[128];
		int font = ae_font(px);

		ae_draw_text(font, px, x, cy - 0.5f * ae_draw_cap_height(font, px), align == 1 ? AE_ALIGN_RIGHT : align == 2 ?
			AE_ALIGN_CENTER : AE_ALIGN_LEFT, rgba_of(rgb, alpha), utf8(text, buffer, sizeof(buffer)));
		return;
	}
	if (align == 1)
		x -= ae_hud_text_measure(text, px);
	else if (align == 2)
		x -= 0.5f * ae_hud_text_measure(text, px);
	ae_hud_text_draw_ex(render.local_player_index, x, baseline, text, px, &color, _ae_text_plain);
}

boolean ae_scoreboard_on(void)
{
	return (boolean)(game_engine && ae_hud_style_is_ae());
}

boolean ae_scoreboard_hides_hud(short local_player_index)
{
	/* (game over too: the engine's own messages are drawn after the HUD, by game_engine_post_rasterize) */
	return (boolean)(ae_scoreboard_on() && game_engine_scoreboard_shown(local_player_index) >= 1.0f);
}

/* a player's row: what it shows */
struct sb_row
{
	long player_index;
	long rank;            /* its place in its list, from 0 */
	sb_rgb chip;
	boolean me;
	wchar_t place[8];
	wchar_t name[32];
	wchar_t score[24];
	wchar_t status[8];    /* DEAD, QUIT, or "" */
	long kills, deaths, assists;
	long ping;            /* NONE: none shown */
	long spree;           /* the best spree (the post-game's column), NONE: not known */
};

/* a list drawn: its rows, the window shown, and its place */
struct sb_list
{
	struct sb_row *rows;
	long count;
	short team;           /* NONE: FFA */
	long score;
	wchar_t score_text[24];  /* the score as the game says it (a time M:SS in Oddball and King) */
	long first;           /* the first row shown of the window */
	long shown;           /* the window's rows */
	long pinned;          /* FFA: the top rows kept above the window */
	real x, y, w;
};

static struct sb_row sb_rows[2][HALO_PORT_MAXIMUM_NETWORK_PLAYERS];

static void place_text(long place, wchar_t *out, long size)
{
	long n = place + 1;
	const wchar_t *suffix = L"TH";

	if (n % 100 < 11 || n % 100 > 13)
	{
		if (n % 10 == 1) suffix = L"ST";
		else if (n % 10 == 2) suffix = L"ND";
		else if (n % 10 == 3) suffix = L"RD";
	}
	usnprintf(out, size, L"%ld%s", n, suffix);
	out[size - 1] = 0;
}

/* the engine's time string leaves the minute blank under one (" :36"): 0:36, as every AE clock */
static void minute_shown(wchar_t *text, long count)
{
	long length, blanks = 0;

	if (!text || count <= 0)
		return;
	while (text[blanks] == L' ')
		blanks++;
	if (text[blanks] != L':')
		return;
	/* (the blanks taken off; the 0 put in) */
	for (length = 0; text[blanks + length]; length++)
		text[length] = text[blanks + length];
	text[length] = 0;
	for (length = 0; text[length]; length++)
		;
	if (length + 1 >= count)
		return;
	for (; length >= 0; length--)
		text[length + 1] = text[length];
	text[0] = L'0';
}

static void fill_row(struct sb_row *row, const struct ae_scoreboard_stat *stat, long viewer, boolean network,
	boolean team_game, const struct ae_scoreboard_options *options)
{
	struct player_datum *player = player_try_and_get(stat->player_index);
	union real_rgb_color armour;
	wchar_t buffer[48];
	const wchar_t *name;

	csmemset(row, 0, sizeof(*row));
	row->player_index = stat->player_index;
	row->me = (boolean)(stat->player_index == viewer);
	row->kills = stat->kills;
	row->deaths = stat->deaths;
	row->assists = stat->assists;
	row->ping = NONE;
	row->spree = options->best_spree ? ae_postgame_best_spree(stat->player_index) : NONE;
	place_text(PIN(stat->place & 0x7F, 0, 127), row->place, NUMBEROF(row->place));
	if (!player)
		return;
	/* (the post-game's: every player of this machine is "you") */
	if (options->all_local_me && player->local_player_index != NONE)
		row->me = TRUE;
	name = ae_game_engine_scoreboard_name(stat->player_index, buffer, NUMBEROF(buffer));
	usnprintf(row->name, NUMBEROF(row->name), L"%s", name ? name : L"");
	row->name[NUMBEROF(row->name) - 1] = 0;
	if (game_engine->format_player_score)
		game_engine->format_player_score(stat->player_index, row->score);
	minute_shown(row->score, NUMBEROF(row->score));
	if (game_engine_player_is_out_of_lives(stat->player_index))
		usnprintf(row->status, NUMBEROF(row->status), L"DEAD");
	else if (player->quit_out_of_game)
		usnprintf(row->status, NUMBEROF(row->status), L"QUIT");
	if (network)
		row->ping = latency_meter_player_ping((short)DATUM_INDEX_TO_ABSOLUTE_INDEX(stat->player_index));
	if (team_game)
		row->chip = team_rgb[PIN(player->team_index, 0, 1)];
	else if (game_engine_player_get_change_color(&armour, stat->player_index))
	{
		row->chip.r = armour.red;
		row->chip.g = armour.green;
		row->chip.b = armour.blue;
	}
	else
		row->chip = muted_rgb;
}

/* the number columns, from the right: PING, BEST SPREE (the post-game's), A, D, K (and SCORE) */
enum { _sb_score, _sb_kills, _sb_deaths, _sb_assists, _sb_spree, _sb_ping, NUMBER_OF_SB_COLUMNS };
/* which of the optional columns are drawn */
enum { _sb_column_score = 1, _sb_column_spree = 2 };

static void column_rights(real x, real w, real s, unsigned short columns, real *rights)
{
	real right = x + w - SB_EDGE * s;
	short column;

	for (column = _sb_ping; column >= _sb_score; column--)
	{
		if ((column == _sb_score && !(columns & _sb_column_score)) || (column == _sb_spree && !(columns & _sb_column_spree)))
		{
			rights[column] = -1.0f;
			continue;
		}
		rights[column] = right;
		right -= (column == _sb_ping ? SB_PING_COLUMN : column == _sb_spree ? SB_SPREE_COLUMN : SB_COLUMN) * s;
	}
}

static void draw_heads(real x, real y, real w, real s, unsigned short columns, boolean compact, real alpha)
{
	static const wchar_t *heads[NUMBER_OF_SB_COLUMNS] = { L"SCORE", L"K", L"D", L"A", L"BEST SPREE", L"PING" };
	real rights[NUMBER_OF_SB_COLUMNS];
	short column;

	column_rights(x, w, s, columns, rights);
	for (column = 0; column < NUMBER_OF_SB_COLUMNS; column++)
	{
		if (rights[column] > 0.0f)
			label(rights[column], y + (compact ? 9.0f : 11.0f) * s, heads[column], (compact ? 13.0f : 14.0f) * s,
				&muted_rgb, alpha, 1);
	}
}

/* a row's panel (quads pass) */
static void row_box(const struct sb_row *row, real x, real y, real w, real h, real s, boolean ffa, real alpha)
{
	real chip = (h >= SB_ROW * s ? 22.0f : 16.0f) * s;
	real ring = MAX(1.0f, 2.0f * s);
	real chip_x = x + (ffa ? SB_FFA_CHIP : SB_CHIP) * s;
	real chip_y = y + (h - chip) / 2.0f;

	box(x, y, w, h, row->me ? &sel_rgb : &row_rgb, (row->me ? 1.0f : SB_ROW_ALPHA) * alpha);
	if (row->me)
		box(x, y, 5.0f * s, h, &accent_rgb, alpha);
	/* a grey ring keeps black armour on a dark row and white armour on the own row in sight */
	box(chip_x - ring, chip_y - ring, chip + 2.0f * ring, chip + 2.0f * ring, &muted_rgb, alpha);
	box(chip_x, chip_y, chip, chip, &row->chip, alpha);
}

/* a row's text (text pass) */
static void row_text(const struct sb_row *row, real x, real y, real w, real h, real s, unsigned short columns,
	boolean ffa, real alpha)
{
	boolean compact = (boolean)(h < SB_ROW * s);
	real cy = y + h / 2.0f;
	real name_px = (compact ? 17.0f : 22.0f) * s;
	real number_px = (compact ? 16.0f : 20.0f) * s;
	const sb_rgb *text = row->me ? &selt_rgb : &text_rgb;
	real rights[NUMBER_OF_SB_COLUMNS];
	wchar_t number[16];
	wchar_t rank[8];
	short column;

	if (ffa)
		usnprintf(rank, NUMBEROF(rank), L"%s", row->place);
	else
		usnprintf(rank, NUMBEROF(rank), L"%ld", row->rank + 1);
	label(x + 20.0f * s, cy, rank, (compact ? 14.0f : 18.0f) * s, row->me ? &selt_rgb : &muted_rgb, alpha, 0);
	label(x + (ffa ? SB_FFA_NAME : SB_NAME) * s, cy, row->name, name_px, text, alpha, 0);
	if (row->status[0])
	{
		/* DEAD / QUIT after the name, small and muted */
		real after = x + (ffa ? SB_FFA_NAME : SB_NAME) * s + measure(row->name, name_px) + 12.0f * s;

		label(after, cy, row->status, 13.0f * s, row->me ? &selt_rgb : &muted_rgb, alpha * 0.9f, 0);
	}
	column_rights(x, w, s, columns, rights);
	for (column = 0; column < NUMBER_OF_SB_COLUMNS; column++)
	{
		long value;

		if (rights[column] <= 0.0f)
			continue;
		switch (column)
		{
		case _sb_score:
			label(rights[column], cy, row->score, (compact ? 17.0f : 22.0f) * s, text, alpha, 1);
			continue;
		case _sb_kills: value = row->kills; break;
		case _sb_deaths: value = row->deaths; break;
		case _sb_assists: value = row->assists; break;
		case _sb_spree:
			if (row->spree == NONE)
				continue;
			value = row->spree;
			break;
		default:
			if (row->ping == NONE)
				continue;
			value = row->ping;
			break;
		}
		if (column == _sb_ping && value > 999)
			usnprintf(number, NUMBEROF(number), L"999+");
		else
			usnprintf(number, NUMBEROF(number), L"%ld", value);
		label(rights[column], cy, number, number_px, column == _sb_ping && !row->me ? &muted_rgb : text, alpha, 1);
	}
}

/* the rows a list shows in its room (rows, not counting a pinned own row or "more" lines) and where its window starts:
FFA keeps its top places and opens at your place; a team list scrolls together with the other */
static void list_window(struct sb_list *list, long room, long scroll)
{
	long me = NONE;
	long i;

	for (i = 0; i < list->count; i++)
		if (list->rows[i].me)
			me = i;
	list->pinned = 0;
	if (list->count <= room)
	{
		list->first = 0;
		list->shown = list->count;
		return;
	}
	if (list->team == NONE)
	{
		list->pinned = MIN(SB_FFA_PINNED, room / 3);
		/* (and a scrolled-out own row pinned at the foot, as a team's) */
		room -= list->pinned + 1;
		if (scroll == NONE)
			scroll = me == NONE ? list->pinned : MAX(me - room / 2, list->pinned);
		list->first = PIN(scroll, list->pinned, list->count - room);
	}
	else
	{
		/* (a scrolled-out own row is pinned at the foot: one row less in the window) */
		room -= 1;
		list->first = PIN(scroll == NONE ? 0 : scroll, 0, MAX(list->count - room, 0));
	}
	list->shown = MIN(room, list->count - list->first);
}

static boolean me_hidden(const struct sb_list *list, long *index)
{
	long i;

	for (i = 0; i < list->count; i++)
	{
		if (list->rows[i].me)
		{
			*index = i;
			return (boolean)(list->count > list->shown + list->pinned && i >= list->pinned &&
				(i < list->first || i >= list->first + list->shown));
		}
	}
	return FALSE;
}

/* a list's height as laid out */
static real list_height(const struct sb_list *list, real s, boolean compact)
{
	real row = (compact ? SB_ROW_COMPACT + SB_ROW_GAP_COMPACT : SB_ROW + SB_ROW_GAP) * s;
	real h = (compact ? 18.0f : SB_HEADS) * s;
	long hidden;

	if (list->team != NONE)
		h += (compact ? SB_BAR_COMPACT : SB_BAR) * s + 4.0f * s;
	h += (list->pinned + list->shown) * row;
	if (list->pinned && list->first > list->pinned)
		h += SB_MORE * s;
	if (list->first + list->shown < list->count)
		h += SB_MORE * s;
	if (me_hidden(list, &hidden))
		h += row + 8.0f * s;
	return h;
}

static void more_line(real x, real y, real w, real s, long n, boolean above, real alpha)
{
	wchar_t text[24];

	usnprintf(text, NUMBEROF(text), above ? L"%ld ABOVE" : L"%ld MORE", n);
	label(x + w / 2.0f, y + 11.0f * s, text, 14.0f * s, &muted_rgb, alpha, 2);
}

/* a list in two passes: pass 0 the panels (one batch of quads), pass 1 the text */
static void draw_list(const struct sb_list *list, real s, boolean compact, unsigned short columns, short pass,
	real alpha)
{
	real row_h = (compact ? SB_ROW_COMPACT : SB_ROW) * s;
	real step = row_h + (compact ? SB_ROW_GAP_COMPACT : SB_ROW_GAP) * s;
	real x = list->x, y = list->y, w = list->w;
	long i, hidden = NONE;
	boolean ffa = (boolean)(list->team == NONE);

	if (!ffa)
	{
		real bar = (compact ? SB_BAR_COMPACT : SB_BAR) * s;

		if (pass == 0)
			box(x, y, w, bar, &team_rgb[list->team], SB_BAR_ALPHA * alpha);
		else
		{
			wchar_t text[24];
			real name_px = (compact ? 22.0f : 24.0f) * s;
			const wchar_t *name = list->team ? L"BLUE TEAM" : L"RED TEAM";

			label(x + SB_EDGE * s, y + bar / 2.0f, name, name_px, &white_rgb, alpha, 0);
			if (list->count > SB_BIG_ROWS)
			{
				usnprintf(text, NUMBEROF(text), L"%ld PLAYERS", list->count);
				label(x + SB_EDGE * s + measure(name, name_px) + 16.0f * s, y + bar / 2.0f, text,
					15.0f * s, &white_rgb, alpha, 0);
			}
			label(x + w - SB_EDGE * s, y + bar / 2.0f, list->score_text, (compact ? 28.0f : 30.0f) * s, &white_rgb, alpha, 1);
		}
		y += bar + 4.0f * s;
	}
	if (pass == 1)
		draw_heads(x, y, w, s, columns, compact, alpha);
	y += (compact ? 18.0f : SB_HEADS) * s;
	for (i = 0; i < list->pinned; i++, y += step)
	{
		if (pass == 0) row_box(&list->rows[i], x, y, w, row_h, s, ffa, alpha);
		else row_text(&list->rows[i], x, y, w, row_h, s, columns, ffa, alpha);
	}
	if (list->pinned && list->first > list->pinned)
	{
		if (pass == 1)
			more_line(x, y, w, s, list->first - list->pinned, TRUE, alpha);
		y += SB_MORE * s;
	}
	for (i = list->first; i < list->first + list->shown; i++, y += step)
	{
		if (pass == 0) row_box(&list->rows[i], x, y, w, row_h, s, ffa, alpha);
		else row_text(&list->rows[i], x, y, w, row_h, s, columns, ffa, alpha);
	}
	if (list->first + list->shown < list->count)
	{
		if (pass == 1)
			more_line(x, y, w, s, list->count - list->first - list->shown, FALSE, alpha);
		y += SB_MORE * s;
	}
	if (me_hidden(list, &hidden))
	{
		y += 8.0f * s;
		if (pass == 0)
		{
			box(x + 40.0f * s, y - 5.0f * s, w - 80.0f * s, MAX(1.0f, s), &rule_rgb, SB_RULE_ALPHA * alpha);
			row_box(&list->rows[hidden], x, y, w, row_h, s, ffa, alpha);
		}
		else
			row_text(&list->rows[hidden], x, y, w, row_h, s, columns, ffa, alpha);
	}
}

long ae_scoreboard_draw_split(long player_index, real alpha, const struct ae_scoreboard_stat *ranked, long count)
{
	return ae_scoreboard_draw(player_index, alpha, ranked, count, NONE, 0, 0);
}

long ae_scoreboard_draw(long player_index, real alpha, const struct ae_scoreboard_stat *ranked, long count,
	long scroll, long notches, long pages)
{
	struct ae_scoreboard_options options;

	csmemset(&options, 0, sizeof(options));
	return ae_scoreboard_draw_with(player_index, alpha, ranked, count, scroll, notches, pages, &options);
}

/* the lists' room above and below (the post-game's: its banner over them, its prompts under them) */
static real lists_top(const struct ae_scoreboard_options *options, boolean split)
{
	if (options->no_title)
		return options->lists_top;
	return (split ? 24.0f : SB_TOP) + SB_HEAD_GAP + SB_RULE_GAP;
}

/* rows that fit in each list: a team game's by team, stacked (both in room_h) or side by side */
static long rows_room(real room_h, real s, boolean teams, boolean side, boolean compact)
{
	real row_step = (compact ? SB_ROW_COMPACT + SB_ROW_GAP_COMPACT : SB_ROW + SB_ROW_GAP) * s;
	real fixed = (compact ? 18.0f : SB_HEADS) * s + 2.0f * SB_MORE * s + row_step + 8.0f * s;
	long room;

	if (teams)
		fixed += (compact ? SB_BAR_COMPACT : SB_BAR) * s + 4.0f * s;
	if (teams && !side)
		room = (long)((room_h - SB_BLOCK_GAP * s) / 2.0f - fixed) / (long)MAX(row_step, 1.0f);
	else
		room = (long)((room_h - fixed) / MAX(row_step, 1.0f));
	return MAX(room, 3);
}

/* (the post-game, options->fit_all) the rows as big as lets every player in: full rows, else compact, else (teams)
compact side by side; a list longer still is windowed as in the game */
static void fit_all(real room_h, real s, boolean teams, long largest, long total, boolean *side, boolean *compact)
{
	/* (without the "more" lines and the pinned row, which a list that fits never draws) */
	real slack = 2.0f * SB_MORE * s + 8.0f * s;

	*side = FALSE;
	*compact = FALSE;
	if (rows_room(room_h + slack + (SB_ROW + SB_ROW_GAP) * s, s, teams, FALSE, FALSE) >= largest)
		return;
	*compact = TRUE;
	if (rows_room(room_h + slack + (SB_ROW_COMPACT + SB_ROW_GAP_COMPACT) * s, s, teams, FALSE, TRUE) >= largest ||
		!teams || total > SB_SIDE_MAXIMUM)
	{
		return;
	}
	*side = TRUE;
}

/* the panel's width and scale (screen pixels) in a view of view_w x view_h: the narrow screens' shrink */
static real panel_scale(real view_w, real view_h, real width, real s)
{
	return MIN(s, (view_w - 2.0f * SB_SIDE_MARGIN * view_h / 1080.0f) / width);
}

static real panel_width(const struct ae_scoreboard_options *options, boolean side)
{
	if (side)
		return 2.0f * SB_SIDE_WIDTH + SB_SIDE_GAP;
	return options->width > 0.0f ? options->width : SB_WIDTH;
}

void ae_scoreboard_postgame_place(real view_w, real view_h, boolean side, real width, real *x, real *w)
{
	struct ae_scoreboard_options options;
	real s;

	csmemset(&options, 0, sizeof(options));
	options.width = width;
	s = panel_scale(view_w, view_h, panel_width(&options, side), view_h / 1080.0f);
	*w = panel_width(&options, side) * s;
	*x = (view_w - *w) / 2.0f;
}

long ae_scoreboard_draw_with(long player_index, real alpha, const struct ae_scoreboard_stat *ranked, long count,
	long scroll, long notches, long pages, struct ae_scoreboard_options *options)
{
	int screen_w = 0, screen_h = 0;
	real view_x, view_w, view_h, s, top, room_h;
	boolean network = (boolean)(game_connection() == _game_connection_network_client ||
		game_connection() == _game_connection_network_server);
	boolean teams = game_engine_has_teams();
	struct game_variant *variant = game_engine_get_variant();
	boolean score = (boolean)(!variant || variant->game_engine_index != game_engine_slayer);
	unsigned short columns = (unsigned short)((score ? _sb_column_score : 0) |
		(options->best_spree ? _sb_column_spree : 0));
	boolean split = (boolean)(local_player_count() > 1 && !options->full_screen);
	boolean compact = (boolean)(count > SB_BIG_ROWS || split);
	boolean side = (boolean)(teams && count > SB_STACKED_MAXIMUM && count <= SB_SIDE_MAXIMUM);
	struct sb_list lists[2];
	short list_count = teams ? 2 : 1;
	struct ae_header_lines lines;
	wchar_t sub[2 * AE_HEADER_TEXT_LENGTH + 8];
	real x0, y;
	long i, room, page;
	short l, pass;

	options->bottom = 0.0f;
	if (alpha <= 0.0f || !ranked)
		return scroll;
	/* (one view: the whole screen, in its pixels; the window bounds are the HUD's safe area. AE's layer: its layout's
	units, 1080 tall) */
	if (options->layer_height > 0.0f)
	{
		screen_w = (int)options->layer_width;
		screen_h = (int)options->layer_height;
		split = FALSE;
	}
	else
		platform_video_drawable_size(&screen_w, &screen_h);
	sb_ae_layer = (boolean)(options->layer_height > 0.0f);
	view_x = 0.0f;
	view_w = (real)screen_w;
	view_h = (real)screen_h;
	if (split)
	{
		/* (a split view: its own pixels, the HUD's units times their size) */
		real ppu = ae_hud_pixels_per_unit();

		view_w = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) * ppu;
		view_h = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * ppu;
	}
	s = view_h / 1080.0f;
	/* (a split view's board no smaller than SB_SPLIT_FLOOR of the full screen's, as the classic split board) */
	if (split)
		s = MAX(s, SB_SPLIT_FLOOR * (real)screen_h / 1080.0f);
	if (s <= 0.0f)
		return scroll;

	/* the lists: a team game's by team (red first), FFA's one */
	csmemset(lists, 0, sizeof(lists));
	for (l = 0; l < 2; l++)
	{
		lists[l].rows = sb_rows[l];
		lists[l].team = teams ? l : NONE;
	}
	for (i = 0; i < count && i < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; i++)
	{
		struct player_datum *player = player_try_and_get(ranked[i].player_index);
		short team = teams && player ? (short)PIN(player->team_index, 0, 1) : 0;
		struct sb_list *list = &lists[team];

		if (list->count >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
			continue;
		fill_row(&list->rows[list->count], &ranked[i], player_index, network, teams, options);
		list->rows[list->count].rank = list->count;
		list->count++;
	}
	if (teams)
	{
		for (l = 0; l < 2; l++)
		{
			lists[l].score = game_engine_get_team_score(l);
			/* (the game's own team score text: Oddball's and King's are times, not their ticks) */
			lists[l].score_text[0] = 0;
			if (game_engine && game_engine->format_team_name)
				game_engine->format_team_name(l, lists[l].score_text);
			if (!lists[l].score_text[0])
				usnprintf(lists[l].score_text, NUMBEROF(lists[l].score_text), L"%ld", lists[l].score);
			lists[l].score_text[NUMBEROF(lists[l].score_text) - 1] = 0;
			minute_shown(lists[l].score_text, NUMBEROF(lists[l].score_text));
		}
	}
	/* (the post-game: the rows as big as lets everyone in) */
	if (options->fit_all)
	{
		real room_at_1080 = view_h / s - lists_top(options, split) - SB_FOOT;

		fit_all(room_at_1080 * s, s, teams, MAX(lists[0].count, lists[1].count), count, &side, &compact);
	}
	/* (narrow screens, 4:3: the panel shrinks to the width) */
	s = panel_scale(view_w, view_h, panel_width(options, side), s);

	/* the room: rows that fit under the title, for both stacked lists or each side-by-side one */
	top = (split ? 24.0f : SB_TOP) * s;
	room_h = view_h - lists_top(options, split) * s - (split ? 70.0f : SB_FOOT) * s;
	room = rows_room(room_h, s, teams, side, compact);
	/* (fit_all: a list that fits whole draws no "more" lines and no pinned row: their room is the rows') */
	if (options->fit_all)
	{
		real step = (compact ? SB_ROW_COMPACT + SB_ROW_GAP_COMPACT : SB_ROW + SB_ROW_GAP) * s;
		long all = rows_room(room_h + 2.0f * SB_MORE * s + 8.0f * s + step, s, teams, side, compact);

		if (all >= MAX(lists[0].count, lists[1].count))
			room = all;
	}
	/* (the scroll: opened, at your place; the wheel's notches and Page Up/Down) */
	page = MAX(room - 1, 1);
	if (split)
		scroll = NONE;
	if (scroll != NONE)
		scroll += notches * SB_WHEEL_STEP + pages * page;
	for (l = 0; l < list_count; l++)
	{
		long own = scroll;

		if (scroll == NONE && teams)
			own = 0;
		list_window(&lists[l], room, own);
	}
	/* (kept as the window drawn: wheeling past either end does not pile up notches to undo) */
	scroll = 0;
	for (l = 0; l < list_count; l++)
		scroll = MAX(scroll, lists[l].first);

	/* the layout */
	{
		real w = panel_width(options, side);

		x0 = view_x + (view_w - w * s) / 2.0f;
		y = lists_top(options, split) * s;
		for (l = 0; l < list_count; l++)
		{
			if (side)
			{
				lists[l].x = x0 + l * (SB_SIDE_WIDTH + SB_SIDE_GAP) * s;
				lists[l].y = y;
				lists[l].w = SB_SIDE_WIDTH * s;
				options->bottom = MAX(options->bottom, (y + list_height(&lists[l], s, compact)) / s);
			}
			else
			{
				lists[l].x = x0;
				lists[l].y = y;
				lists[l].w = w * s;
				y += list_height(&lists[l], s, compact);
				options->bottom = y / s;
				y += SB_BLOCK_GAP * s;
			}
		}
		ae_scoreboard_header_text(&lines);
		/* (a gametype without a display name: the variant's own) */
		if (!lines.mode[0] && variant)
		{
			csmemcpy(lines.mode, variant->human_readable_game_description,
				MIN(sizeof(lines.mode), sizeof(variant->human_readable_game_description)));
			lines.mode[NUMBEROF(lines.mode) - 1] = 0;
		}
		usnprintf(sub, NUMBEROF(sub), L"%s%s%s", lines.map, AE_HEADER_SEPARATOR, lines.goal_time);
		sub[NUMBEROF(sub) - 1] = 0;

		/* pass 0: the scrim, the rule and every panel, one batch; pass 1: the text */
		for (pass = 0; pass < 2; pass++)
		{
			if (pass == 0)
			{
				quads_begin();
				box(view_x, 0.0f, view_w, view_h, &scrim_rgb,
					(options->scrim_alpha > 0.0f ? options->scrim_alpha : SB_SCRIM_ALPHA) * alpha);
				if (!options->no_title)
					box(x0, top + SB_HEAD_GAP * s, w * s, MAX(1.0f, s), &rule_rgb, SB_RULE_ALPHA * alpha);
			}
			else if (!options->no_title)
			{
				label(x0, top + 14.0f * s, lines.mode, SB_TITLE * s, &title_rgb, alpha, 0);
				label(x0, top + 44.0f * s, sub, SB_SUB * s, &muted_rgb, alpha, 0);
			}
			for (l = 0; l < list_count; l++)
				draw_list(&lists[l], s, compact, columns, pass, alpha);
			if (pass == 0)
				quads_end();
		}
	}
	options->side = side;
	sb_ae_layer = FALSE;
	return scroll;
}
