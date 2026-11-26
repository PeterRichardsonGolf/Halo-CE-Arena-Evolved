/* The career rank's and the skill rank's drawing (see the header). */
#include <stdio.h>
#include <string.h>

#include "../src/ae_draw.h"
#include "ae_career_draw.h"
#include "ae_career_view_rules.h"
#include "ae_mcc_career.h"
#include "ae_mcc_rank.h"
#include "ae_style.h"
#include "ae_widgets.h"

/* the post-game's "this game's gain" ice blue at .5 over the track: one colour for XP gained or to gain */
#define CAREER_GHOST AE_COLOR_CAREER_GHOST

static unsigned int with_alpha(unsigned int rgba, float alpha)
{
	unsigned int a = (unsigned int)((float)(rgba & 0xFF) * (alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha) + 0.5f);

	return (rgba & 0xFFFFFF00u) | a;
}

float ae_career_cap_top(int font, float size, float center_y)
{
	return center_y - ae_draw_cap_height(font, size) * 0.5f;
}

int ae_career_art_shown(void)
{
	return ae_career_art_on() && ae_mcc_career_available();
}

static void line(float u, int font, float size, float tracking, float x, float cy, int align, unsigned int color, char const *text)
{
	float px = size * u;

	if (tracking > 0.0f)
		ae_draw_text_tracked(font, px, tracking, x, ae_career_cap_top(font, px, cy), align, color, text);
	else
		ae_draw_text(font, px, x, ae_career_cap_top(font, px, cy), align, color, text);
}

float ae_career_dots_draw(float u, int font, float size, float tracking, float x, float cy, int align, unsigned int color,
	char const *const *pieces, int count)
{
	float px = size * u, gap = 16.5f * u, dot = 5.0f * u, width = 0.0f, pen;
	int each;

	for (each = 0; each < count; each++)
		width += (tracking > 0.0f ? ae_draw_text_tracked_width(font, px, tracking, pieces[each]) : ae_draw_text_width(font, px, pieces[each])) +
			(each ? 2.0f * gap + dot : 0.0f);
	pen = align == AE_ALIGN_RIGHT ? x - width : x;
	for (each = 0; each < count; each++)
	{
		float piece = tracking > 0.0f ? ae_draw_text_tracked_width(font, px, tracking, pieces[each]) : ae_draw_text_width(font, px, pieces[each]);

		if (each)
		{
			ae_draw_rect(pen + gap, cy - dot * 0.5f, dot, dot, dot * 0.5f, color);
			pen += 2.0f * gap + dot;
		}
		line(u, font, size, tracking, pen, cy, AE_ALIGN_LEFT, color, pieces[each]);
		pen += piece;
	}
	return width;
}

void ae_career_bar_draw(float u, float x, float y, float width, int percent, int ghost_percent, char const *left, char const *right,
	unsigned int right_color)
{
	float h = 8.0f * u, fill = width * (float)(percent < 0 ? 0 : percent > 100 ? 100 : percent) / 100.0f, ghost = 0.0f;

	ae_draw_rect(x, y, width, h, 0.0f, AE_COLOR_RULE);
	ae_draw_rect(x, y, fill, h, 0.0f, AE_COLOR_ACCENT);
	if (ghost_percent > 0)
	{
		ghost = width * (float)ghost_percent / 100.0f;
		if (fill + ghost > width)
			ghost = width - fill;
		if (ghost > 0.0f)
			ae_draw_rect(x + fill, y, ghost, h, 0.0f, CAREER_GHOST);
	}
	line(u, AE_FONT_BODY, 18.0f, 0.06f, x, y + 28.0f * u, AE_ALIGN_LEFT, AE_COLOR_TEXT, left);
	if (right && right[0])
		line(u, AE_FONT_BODY, 18.0f, 0.06f, x + width, y + 28.0f * u, AE_ALIGN_RIGHT, right_color, right);
}

void ae_career_block_draw(float u, float x, float top, float width, struct ae_career_view const *v, int flags, long ghost_xp)
{
	char upper[48], rank[24], tour[24], xp[32], left[48], right[64], summary[2][40];
	char const *meta[3];
	int art = ae_career_art_shown(), each, size, ghost = 0;
	float tx = art ? x + 156.0f * u : x, room = x + width - tx;
	long cost;

	line(u, AE_FONT_BODY, 16.0f, 0.09f, x, top + 12.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "CAREER RANK");
	if (flags & AE_CAREER_BLOCK_SUMMARY)
	{
		char const *pieces[2];

		snprintf(summary[0], sizeof(summary[0]), "CHALLENGES  TODAY %d OF 3", v->daily_done);
		snprintf(summary[1], sizeof(summary[1]), "THIS WEEK %d OF 3", v->weekly_done);
		pieces[0] = summary[0];
		pieces[1] = summary[1];
		ae_career_dots_draw(u, AE_FONT_BODY, 16.0f, 0.09f, x + width, top + 12.0f * u, AE_ALIGN_RIGHT, AE_COLOR_MUTED, pieces, 2);
	}
	if (art)
	{
		int insignia = ae_mcc_career_menu_image(0, v->rank), badge = ae_mcc_career_menu_image(1, v->tour);

		if (insignia)
			ae_draw_image(insignia, x, top + 32.0f * u, 80.0f * u, 80.0f * u, 0xFFFFFFFFu);
		if (badge)
			ae_draw_image(badge, x + 92.0f * u, top + 68.0f * u, 44.0f * u, 44.0f * u, 0xFFFFFFFFu);
	}
	for (each = 0; v->name[each] && each < (int)sizeof(upper) - 1; each++)
		upper[each] = v->name[each] >= 'a' && v->name[each] <= 'z' ? (char)(v->name[each] - 32) : v->name[each];
	upper[each] = 0;
	size = ae_cv_fit_size(ae_draw_text_width(AE_FONT_TITLE, 40.0f * u, upper), ae_draw_text_width(AE_FONT_TITLE, 34.0f * u, upper),
		ae_draw_text_width(AE_FONT_TITLE, 30.0f * u, upper), room);
	line(u, AE_FONT_TITLE, (float)size, 0.0f, tx, top + 60.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, upper);
	ae_cv_meta(v->step, v->xp, rank, sizeof(rank), tour, sizeof(tour), xp, sizeof(xp));
	meta[0] = rank;
	meta[1] = tour;
	meta[2] = xp;
	ae_career_dots_draw(u, AE_FONT_BODY, 18.0f, 0.06f, tx, top + 102.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, meta, 3);
	ae_cv_career_captions(v->step, v->to_next, left, sizeof(left), right, sizeof(right));
	cost = v->step >= AE_XP_STEPS - 1 ? 0 : ae_xp_step_start(v->step + 1) - ae_xp_step_start(v->step);
	if (ghost_xp > 0 && cost > 0)
		ghost = (int)((ghost_xp * 100 + cost - 1) / cost);
	ae_career_bar_draw(u, x, top + 126.0f * u, width, v->percent, ghost, left, right, AE_COLOR_MUTED);
}

int ae_skill_picture_draw(int rank, float cx, float cy, float size, float alpha)
{
	int image = ae_career_art_on() ? ae_mcc_rank_menu_image(rank, 0, NULL, NULL) : 0;

	if (!image)
		return 0;
	ae_draw_rect(cx - size * 0.4f, cy - size * 0.4f, size * 0.8f, size * 0.8f, size * 0.4f, with_alpha(AE_COLOR_SELECTION_TEXT, alpha));
	ae_draw_image(image, cx - size * 0.5f, cy - size * 0.5f, size, size, with_alpha(0xFFFFFFFFu, alpha));
	return 1;
}

float ae_skill_rank_draw(int rank, float right, float cy, float height, float size, unsigned int color, float alpha)
{
	char text[24];
	float width;

	if (ae_skill_picture_draw(rank, right - height * 0.5f, cy, height, alpha))
		return right - height;
	snprintf(text, sizeof(text), "SKILL %d", rank);
	width = ae_draw_text_tracked_width(AE_FONT_ROW, size, 0.06f, text);
	ae_draw_text_tracked(AE_FONT_ROW, size, 0.06f, right, ae_career_cap_top(AE_FONT_ROW, size, cy), AE_ALIGN_RIGHT, with_alpha(color, alpha), text);
	return right - width;
}
