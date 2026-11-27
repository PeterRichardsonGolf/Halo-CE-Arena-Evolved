/*
AE_CAREER_DRAW.H

The career rank's and the skill rank's drawing, shared by the CAREER and CHALLENGES screens, the post-game and the lobby (layout
units scaled by u; the PC menus' theme). The plain look has no plates: the rank's name is the value and its numbers are labelled
text, each fact once. With MCC's files the career rank is MCC's insignia with the tour's badge beside it (no plates, no borders)
and the skill rank is MCC's skill picture on a dark disc, labelled by the text next to it. ae_career_art_set(1) turns MCC's
pictures off for the plain look.
*/

#ifndef __AE_CAREER_DRAW_H
#define __AE_CAREER_DRAW_H

#include "ae_glue_career_xp.h"

enum
{
	/* ae_career_block_draw: the CHALLENGES summary at the label's right (CAREER only) */
	AE_CAREER_BLOCK_SUMMARY = 1
};

/* whether MCC's career pictures are drawn now: the install has them and they are not turned off */
int ae_career_art_shown(void);
/* the career block at x, top (the card's inner top) over width, 184 u tall: CAREER RANK label, the rank's name (40, 34 or 30 u: never
   cut short), "RANK 14 OF 30 · TOUR 1 OF 11 · 10,350 XP", the bar and its two captions; with MCC's pictures the insignia (80 u)
   and the tour badge (44 u) before the name. ghost_xp > 0: that reward drawn as the light part after the bar's fill */
void ae_career_block_draw(float u, float x, float top, float width, struct ae_career_view const *view, int flags, long ghost_xp);
/* a progress bar (8 u, the pattern of every panel) at x, y over width: percent filled, then ghost_percent lighter (0: none), the left
   caption under its left end in the text colour, the right caption under its right end in right_color */
void ae_career_bar_draw(float u, float x, float y, float width, int percent, int ghost_percent, char const *left, char const *right,
	unsigned int right_color);
/* text pieces in one line joined by separator dots (5 u, 16.5 u each side); x is the left edge, or the right edge with
   AE_ALIGN_RIGHT; cy the line's centre; returns the width. size and tracking as ae_draw_text_tracked */
float ae_career_dots_draw(float u, int font, float size, float tracking, float x, float cy, int align, unsigned int color,
	char const *const *pieces, int count);
/* a skill rank's MCC picture, size px square, centred on cx, cy over a dark disc of 0.8 of it; 1 when drawn (0: MCC's pictures are not
   shown or have none) */
int ae_skill_picture_draw(int rank, float cx, float cy, float size, float alpha);
/* a skill rank right-aligned at right, centred on cy, at most height tall: MCC's picture on its disc, else "SKILL n" in tracked capitals
   of size px; the left edge of what was drawn */
float ae_skill_rank_draw(int rank, float right, float cy, float height, float size, unsigned int color, float alpha);
/* a text's top for a cap-centred line at center_y */
float ae_career_cap_top(int font, float size, float center_y);

#endif
