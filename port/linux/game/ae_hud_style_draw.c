/*
AE_HUD_STYLE_DRAW.C

HUD STYLE AE for the countdown, the overlay, the names, the voice speakers and
co-op's texts (ae_hud_style_draw.h; spec 12.4, 12.6).
*/

#include <math.h>
#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "memory/data.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "text/draw_string.h"
#include "text/font_group.h"

#include "ae_bomb_assault.h"
#include "ae_header_lines.h"
#include "ae_hud_digits.h"
#include "ae_hud_style.h"
#include "ae_hud_style_draw.h"
#include "ae_hud_text.h"
#include "ae_score_strip.h"
#include "ae_scoreboard_header.h"
#include "network_voice.h"

void platform_video_drawable_size(int *width, int *height);
int halo_capture_notice(char *text, int size);
double config_real(const char *name);

static const real white_rgb[3] = { 229.0f / 255.0f, 240.0f / 255.0f, 1.0f };
static const real grey_rgb[3] = { 188.0f / 255.0f, 198.0f / 255.0f, 212.0f / 255.0f };
static const real hud_blue_rgb[3] = { 117.0f / 255.0f, 186.0f / 255.0f, 1.0f };
static const real red_rgb[3] = { 1.0f, 77.0f / 255.0f, 77.0f / 255.0f };
static const real blue_rgb[3] = { 89.0f / 255.0f, 140.0f / 255.0f, 1.0f };
static const real dark_rgb[3] = { 6.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f };

static void set_color(real const rgb[3], real alpha, real_argb_color *color)
{
	color->alpha = alpha;
	color->red = rgb[0];
	color->green = rgb[1];
	color->blue = rgb[2];
}

/* the screen's scale for screen-level text: its height / 1080 */
static real screen_scale(void)
{
	int width = 0, height = 0;

	platform_video_drawable_size(&width, &height);
	return height > 0 ? (real)height / 1080.0f : 1.0f;
}

static real floor_px(real px)
{
	return px < 12.0f ? 12.0f : px;
}

/* the player of this view, or NULL */
static struct player_datum *view_player(void)
{
	long player_index;

	if (render.local_player_index < 0 || render.local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return NULL;
	player_index = local_player_get_player_index(render.local_player_index);
	return player_index == NONE ? NULL : player_try_and_get(player_index);
}

static real const *team_rgb(long team_index)
{
	return team_index == 0 ? red_rgb : blue_rgb;
}

/* an ASCII string as the game's wide string */
static void wcscpy_ascii(wchar_t *to, char const *from)
{
	while (*from)
		*to++ = (wchar_t)(unsigned char)*from++;
	*to = 0;
}

/* ---------- the countdown */

/* the header's first line in its colours, from x (draw FALSE: its width only) */
static real header_line1(struct ae_header_lines const *lines, real x, real baseline, real px, boolean draw)
{
	static const wchar_t *const separator = AE_HEADER_SEPARATOR;
	const wchar_t *texts[5];
	real const *colors[5];
	short count = 0, i;
	real width = 0.0f;

	texts[count] = lines->mode; colors[count++] = white_rgb;
	texts[count] = separator; colors[count++] = grey_rgb;
	texts[count] = lines->map; colors[count++] = hud_blue_rgb;
	if (lines->tag[0])
	{
		texts[count] = L" - "; colors[count++] = hud_blue_rgb;
		texts[count] = lines->tag; colors[count++] = hud_blue_rgb;
	}
	for (i = 0; i < count; i++)
	{
		if (draw)
		{
			real_argb_color color;

			set_color(colors[i], 1.0f, &color);
			ae_hud_text_draw(render.local_player_index, x + width, baseline, texts[i], px, &color, TRUE);
		}
		width += ae_hud_text_measure(texts[i], px);
	}
	return width;
}

boolean ae_hud_style_countdown(long ticks_left, short view_width, short view_height)
{
	real ppu = ae_hud_pixels_per_unit();
	real width = (real)view_width * ppu, height = (real)view_height * ppu;
	real s = ae_score_strip_view_scale();
	real cap = 0.24f * height, px = cap / AE_HUD_TEXT_CAP;
	wchar_t digit[2];
	real_argb_color color;
	struct player_datum *viewer = view_player();
	struct ae_header_lines lines;
	real line1_px = floor_px(22.0f * s), line2_px = floor_px(16.0f * s);
	real line1 = height / 2.0f + 0.17f * height, line2 = line1 + line2_px * AE_HUD_TEXT_CAP + 10.0f * s;
	real w;

	if (!ae_hud_style_is_ae())
		return FALSE;
	digit[0] = (wchar_t)(L'0' + (ticks_left + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND);
	digit[1] = 0;
	/* (white without teams; the viewer's team colour in team games) */
	set_color(game_engine_has_teams() && viewer ? team_rgb(viewer->team_index) : white_rgb, 1.0f, &color);
	ae_hud_text_draw(render.local_player_index, width / 2.0f - ae_hud_text_measure(digit, px) / 2.0f,
		height / 2.0f + cap / 2.0f, digit, px, &color, TRUE);
	ae_scoreboard_header_text(&lines);
	w = header_line1(&lines, 0.0f, 0.0f, line1_px, FALSE);
	header_line1(&lines, width / 2.0f - w / 2.0f, line1, line1_px, TRUE);
	set_color(hud_blue_rgb, 0.9f, &color);
	ae_hud_text_draw(render.local_player_index, width / 2.0f - ae_hud_text_measure(lines.goal_time, line2_px) / 2.0f,
		line2, lines.goal_time, line2_px, &color, TRUE);
	/* (0.3.9) Bomb with AE's rules, sides swapping: "ROUND 2 . ATTACK" under them, the job in the viewer's team
	colour */
	{
		long round, attackers;

		if (ae_bomb_round_line(&round, &attackers) && viewer)
		{
			wchar_t text[3][24];
			real const *colors[3];
			real line3 = line2 + line1_px * AE_HUD_TEXT_CAP + 18.0f * s, x;
			short count = 1, i;
			long n = round, digits = 0, j;
			wchar_t reversed[12];

			wcscpy_ascii(text[0], "ROUND ");
			do { reversed[digits++] = (wchar_t)(L'0' + n % 10); n /= 10; } while (n && digits < 11);
			for (j = 0; j < digits; j++)
				text[0][6 + j] = reversed[digits - 1 - j];
			text[0][6 + digits] = 0;
			colors[0] = white_rgb;
			if (attackers != NONE)
			{
				for (j = 0; AE_HEADER_SEPARATOR[j]; j++)
					text[1][j] = AE_HEADER_SEPARATOR[j];
				text[1][j] = 0;
				colors[1] = grey_rgb;
				wcscpy_ascii(text[2], viewer->team_index == attackers ? "ATTACK" : "DEFEND");
				colors[2] = team_rgb(viewer->team_index);
				count = 3;
			}
			for (i = 0, w = 0.0f; i < count; i++)
				w += ae_hud_text_measure(text[i], line1_px);
			for (i = 0, x = width / 2.0f - w / 2.0f; i < count; i++)
			{
				set_color(colors[i], 1.0f, &color);
				ae_hud_text_draw(render.local_player_index, x, line3, text[i], line1_px, &color, TRUE);
				x += ae_hud_text_measure(text[i], line1_px);
			}
		}
	}
	return TRUE;
}

/* ---------- the overlay and the capture notice */

/* (0.7.0, the HUD corners, owner-approved mockup hud-inplace) the performance line's words in the HUD's own colours:
each pair a label (light HUD blue, smaller) then its value (white, or the frame rate's warning colour), pairs a gap
apart; the version at its tail quiet grey. Words: a value starts with a digit or a sign ("1%" is LOW's label), the
version with "v" and a digit. Draws when draw, else measures; the width in screen pixels */
static real overlay_line(const wchar_t *line, real px, real x, real baseline, const union real_argb_color *value_color,
	boolean draw)
{
	real label_px = floor_px(px * 0.8f);
	real pair_gap = px * 0.27f, group_gap = px * 0.9f;
	real start = x;
	const wchar_t *at = line;
	real_argb_color label_color, version_color;

	set_color(hud_blue_rgb, 0.9f * value_color->alpha, &label_color);
	set_color(grey_rgb, 0.55f, &version_color);
	while (*at)
	{
		wchar_t word[48];
		short n = 0, spaces = 0;
		boolean version, value, percent;
		real size;
		short i;

		while (at[spaces] == L' ')
			spaces++;
		if (spaces && x > start)
			x += spaces >= 2 ? group_gap : pair_gap;
		at += spaces;
		while (at[n] && at[n] != L' ' && n < (short)NUMBEROF(word) - 1)
		{
			word[n] = at[n];
			n++;
		}
		word[n] = 0;
		at += n;
		if (!n)
			break;
		version = word[0] == L'v' && word[1] >= L'0' && word[1] <= L'9';
		/* (no wcschr: wchar_t is 2 bytes here, -fshort-wchar) */
		percent = FALSE;
		for (i = 0; word[i]; i++)
			percent = percent || word[i] == L'%';
		value = !version && !percent && ((word[0] >= L'0' && word[0] <= L'9') || word[0] == L'-' ||
			word[0] == L'+' || word[0] == L'.');
		size = value ? px : label_px;
		if (draw)
		{
			ae_hud_text_draw(render.local_player_index, x, baseline, word, size,
				version ? &version_color : value ? value_color : &label_color, TRUE);
		}
		x += ae_hud_text_measure(word, size);
	}
	return x - start;
}

boolean ae_hud_style_overlay(short corner, const union real_argb_color *color, const char *text, boolean *drawn,
	union rectangle2d *extent)
{
	real ppu = ae_hud_pixels_per_unit();
	rectangle2d bounds = render.camera.viewport_bounds;
	real px = floor_px(15.0f * screen_scale());
	wchar_t line[160];
	real_argb_color whole;
	real width = 0.0f, x, baseline, line_step = px * 1.25f;
	const char *at = text;
	short i, lines = 0;

	if (!ae_hud_style_is_ae())
		return FALSE;
	/* (one colour for the whole line: white at 60 FPS and over, else CLASSIC's yellow or red) */
	whole = *color;
	if (hud_globals && fabsf(color->red - hud_globals->messaging.state_color.red) < 0.01f &&
		fabsf(color->green - hud_globals->messaging.state_color.green) < 0.01f &&
		fabsf(color->blue - hud_globals->messaging.state_color.blue) < 0.01f)
	{
		set_color(white_rgb, color->alpha, &whole);
	}
	baseline = (real)(bounds.y0 + 2) * ppu + px * AE_HUD_TEXT_CAP + 2.0f;
	do
	{
		real line_width;

		for (i = 0; i < (short)NUMBEROF(line) - 1 && at && at[i] && at[i] != '\n'; i++)
			line[i] = (wchar_t)(unsigned char)at[i];
		line[i] = 0;
		line_width = overlay_line(line, px, 0.0f, 0.0f, &whole, FALSE);
		if (line_width > width)
			width = line_width;
		x = corner == _text_justification_left ? (real)(bounds.x0 + 4) * ppu :
			(real)(bounds.x1 - 4) * ppu - line_width;
		overlay_line(line, px, x, baseline + line_step * lines, &whole, TRUE);
		lines++;
		at += i;
		if (*at == '\n')
			at++;
	} while (*at);
	x = corner == _text_justification_left ? (real)(bounds.x0 + 4) * ppu : (real)(bounds.x1 - 4) * ppu - width;
	*drawn = (boolean)(width > 0.0f);
	extent->x0 = (short)(x / ppu);
	extent->x1 = (short)((x + width) / ppu + 0.5f);
	extent->y0 = bounds.y0;
	extent->y1 = (short)((baseline + line_step * (lines - 1) + px * 0.3f) / ppu + 0.5f);
	return TRUE;
}

boolean ae_hud_style_capture_notice(boolean statistics_drawn, const union rectangle2d *statistics_extent,
	boolean statistics_right)
{
	real ppu = ae_hud_pixels_per_unit();
	rectangle2d bounds = render.camera.viewport_bounds;
	real px = floor_px(15.0f * screen_scale());
	char text[64];
	wchar_t line[64];
	real_argb_color color;
	real top;
	int kind;
	short i;

	if (!ae_hud_style_is_ae())
		return FALSE;
	kind = halo_capture_notice(text, sizeof(text));
	if (!kind)
		return TRUE;
	for (i = 0; i < (short)NUMBEROF(line) - 1 && text[i]; i++)
		line[i] = (wchar_t)(unsigned char)text[i];
	line[i] = 0;
	top = (real)(bounds.y0 + 2) * ppu;
	if (statistics_drawn && statistics_right && statistics_extent)
		top = (real)(statistics_extent->y1 + 2) * ppu;
	/* (white; a failure red) */
	set_color(kind == 2 ? red_rgb : white_rgb, 0.9f, &color);
	ae_hud_text_draw(render.local_player_index, (real)(bounds.x1 - 4) * ppu - ae_hud_text_measure(line, px),
		top + px * AE_HUD_TEXT_CAP + 2.0f, line, px, &color, TRUE);
	return TRUE;
}

/* ---------- names over players */

boolean ae_hud_style_name_color(boolean ally, union real_argb_color *color)
{
	struct player_datum *viewer = view_player();

	if (!ae_hud_style_is_ae())
		return FALSE;
	/* (ENEMY NAME COLOR RED: allies white, owner 2026-10-20, so a red team's allies never read as enemies) */
	if (ally && game_engine_enemy_name_red())
		set_color(white_rgb, 1.0f, color);
	else if (ally)
		set_color(game_engine_has_teams() && viewer ? team_rgb(viewer->team_index) : white_rgb, 1.0f, color);
	else if (game_engine_enemy_name_red())
		set_color(red_rgb, 1.0f, color);
	else
		set_color(white_rgb, 1.0f, color);
	return TRUE;
}

void ae_hud_style_name_outline(long font_index, const union rectangle2d *bounds, short x, short y,
	const wchar_t *name, const union real_argb_color *color, real scale)
{
	static const real directions[8][2] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f },
		{ 0.7071f, 0.7071f }, { -0.7071f, 0.7071f }, { 0.7071f, -0.7071f }, { -0.7071f, -0.7071f } };
	real step = 1.5f / ae_hud_pixels_per_unit();
	real_argb_color dark;
	short i;

	if (!ae_hud_style_is_ae() || !bounds || !name || !color)
		return;
	if (fabsf(1.0f - scale) < 0.01f)
		scale = 0.99f;
	set_color(dark_rgb, 0.88f * color->alpha, &dark);
	draw_string_set_draw_mode(font_index, NONE, 2, 0, &dark);
	for (i = 0; i < 8; i++)
	{
		/* (moving the scale's origin by d moves the text by d x (1 - scale)) */
		rasterizer_text_set_scale(scale, (real)x + directions[i][0] * step / (1.0f - scale),
			(real)y + directions[i][1] * step / (1.0f - scale));
		rasterizer_draw_unicode_string(bounds, NULL, NULL, 0, name);
	}
	rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);
}

/* ---------- the voice speakers */

boolean ae_hud_style_voice_speakers(void)
{
	real ppu = ae_hud_pixels_per_unit();
	rectangle2d window = render.camera.window_bounds;
	real s = ae_score_strip_view_scale();
	real px = floor_px(14.0f * s), pitch = 20.0f * s;
	real left, top;
	long machines[128];
	short listed = 0, row = 0;
	struct data_iterator iterator;
	struct player_datum *player;
	struct player_datum *viewer;

	if (!ae_hud_style_is_ae())
		return FALSE;
	if (!network_voice_available())
		return TRUE;
	offset_rectangle2d(&window, -render.camera.viewport_bounds.x0, -render.camera.viewport_bounds.y0);
	left = (real)(window.x0 + 16) * ppu;
	top = ((real)window.y0 + (real)(window.y1 - window.y0) * 0.55f) * ppu;
	viewer = local_player_get_next(NONE) != NONE ?
		player_try_and_get(local_player_get_player_index(local_player_get_next(NONE))) : NULL;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL && row < 8)
	{
		long machine = player->network_player_data.machine_index;
		real_argb_color color;
		wchar_t name[NUMBEROF(player->name) + 1];
		rectangle2d icon;
		real baseline = top + pitch * (real)row + px * AE_HUD_TEXT_CAP;
		real size = px * 0.9f;
		short i;

		if (player->quit_out_of_game)
			continue;
		for (i = 0; i < listed && machines[i] != machine; i++)
			;
		if (i < listed || listed >= (short)NUMBEROF(machines))
			continue;
		machines[listed++] = machine;
		if (!network_voice_machine_speaking(machine))
			continue;
		if (player->local_player_index != NONE)
			set_color(white_rgb, 1.0f, &color);
		else
			ae_hud_style_name_color((boolean)(!game_engine || (viewer && player->team_index == viewer->team_index)),
				&color);
		for (i = 0; i < (short)NUMBEROF(player->name); i++)
			name[i] = player->name[i];
		name[NUMBEROF(player->name)] = 0;
		/* (the speaker, dimmed to read grey, centred on the capitals) */
		icon.x0 = (short)(left / ppu);
		icon.x1 = (short)((left + size) / ppu + 0.5f);
		icon.y0 = (short)((baseline - px * AE_HUD_TEXT_CAP / 2.0f - size / 2.0f) / ppu);
		icon.y1 = (short)(icon.y0 + (icon.x1 - icon.x0));
		network_voice_draw_icon(&icon, FALSE, 0.6f);
		ae_hud_text_draw(render.local_player_index, left + size + 4.0f * s, baseline, name, px, &color, TRUE);
		row++;
	}
	return TRUE;
}

/* ---------- co-op's texts */

static boolean begins(const wchar_t *text, const wchar_t *prefix)
{
	while (*prefix && *text == *prefix)
	{
		text++;
		prefix++;
	}
	return (boolean)(*prefix == 0);
}

/* one line's parts: label words grey, a name or a count white */
static void bottom_line(const wchar_t *line, long length, real centre, real baseline, real px)
{
	wchar_t parts[4][96];
	real const *colors[4];
	short count = 0, i;
	real width = 0.0f, x;
	long n = length < 95 ? length : 95;

	if (n >= 11 && begins(line, L"SPECTATING "))
	{
		/* "SPECTATING <name>   (A: NEXT)" -> "SPECTATING" grey, the name white, "A NEXT" grey */
		long end = 11;

		while (end < n && !(line[end] == L' ' && end + 2 < n && line[end + 1] == L' ' && line[end + 2] == L' '))
			end++;
		memcpy(parts[0], L"SPECTATING ", sizeof(wchar_t) * 12);
		colors[0] = grey_rgb;
		memcpy(parts[1], line + 11, sizeof(wchar_t) * (size_t)(end - 11));
		parts[1][end - 11] = 0;
		colors[1] = white_rgb;
		memcpy(parts[2], L"   A NEXT", sizeof(wchar_t) * 10);
		colors[2] = grey_rgb;
		count = 3;
	}
	else
	{
		/* digits white, the rest grey ("VOTED TO SKIP   1 OF 2") */
		long start = 0;

		while (start < n && count < 4)
		{
			boolean digit = (boolean)(line[start] >= L'0' && line[start] <= L'9');
			long end = start;

			while (end < n && (boolean)(line[end] >= L'0' && line[end] <= L'9') == digit && end - start < 94)
				end++;
			if (count == 3)
				end = n < start + 94 ? n : start + 94;
			memcpy(parts[count], line + start, sizeof(wchar_t) * (size_t)(end - start));
			parts[count][end - start] = 0;
			colors[count++] = digit ? white_rgb : grey_rgb;
			start = end;
		}
	}
	for (i = 0; i < count; i++)
		width += ae_hud_text_measure(parts[i], px);
	x = centre - width / 2.0f;
	for (i = 0; i < count; i++)
	{
		real_argb_color color;

		set_color(colors[i], 1.0f, &color);
		ae_hud_text_draw(render.local_player_index, x, baseline, parts[i], px, &color, TRUE);
		x += ae_hud_text_measure(parts[i], px);
	}
}

#define SPECTATE_FIRST_PX 24.0f   /* "SPECTATING Player (A: NEXT)", px at 1080p, single view */
#define SPECTATE_HINT_PX 20.0f    /* the hint under it */
#define SPECTATE_PITCH 30.0f      /* baseline to baseline, px x s */
#define SPECTATE_FLOOR_PX 16.0f   /* as the message lines' floor (ae_hud_message_flow.h) */

static real spectate_px(real px)
{
	return px < SPECTATE_FLOOR_PX ? SPECTATE_FLOOR_PX : px;
}

boolean ae_hud_style_bottom_text(const wchar_t *text)
{
	real ppu = ae_hud_pixels_per_unit();
	rectangle2d bounds = render.camera.window_bounds;
	real s = ae_score_strip_view_scale();
	/* (co-op's spectate lines at the message lines' size: campaign review 2026-10-18 F4; they were 16 / 14 x s,
	floor 12, 0.40 of stock's) */
	real first_px = spectate_px(SPECTATE_FIRST_PX * s), hint_px = spectate_px(SPECTATE_HINT_PX * s);
	real pitch = SPECTATE_PITCH * s < 1.25f * first_px ? 1.25f * first_px : SPECTATE_PITCH * s; /* (floored lines stay
	apart) */
	real centre = ((real)bounds.x0 + (real)bounds.x1) / 2.0f * ppu;
	real bottom = (real)(bounds.y1 - 24) * ppu;
	long starts[4], lengths[4];
	short lines = 0, i;
	long position = 0;

	if (!ae_hud_style_is_ae())
		return FALSE;
	if (!text)
		return TRUE;
	while (lines < 4)
	{
		long end = position;

		while (text[end] && text[end] != L'\r' && text[end] != L'\n')
			end++;
		starts[lines] = position;
		lengths[lines++] = end - position;
		if (!text[end])
			break;
		position = end;
		while (text[position] == L'\r' || text[position] == L'\n')
			position++;
	}
	for (i = 0; i < lines; i++)
		bottom_line(text + starts[i], lengths[i], centre, bottom - pitch * (real)(lines - 1 - i),
			i == 0 ? first_px : hint_px);
	return TRUE;
}
