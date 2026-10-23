/*
AE_HUD_MESSAGE_FLOW.H

HUD STYLE AE's CE message lines (ae_hud_messages.c): where a piece of a line
goes. A line is built of pieces in order (text, a button or weapon picture,
more text) on a pen that moves right; a piece that does not fit before the
wrap limit goes on the next line, broken between words. Pure (no engine
includes): the text's width comes from a measure the caller hands in.
*/

#ifndef __AE_HUD_MESSAGE_FLOW_H
#define __AE_HUD_MESSAGE_FLOW_H

#include <wchar.h>

#include "ae_pure_types.h"

/* a run of text's advance in pixels: count characters from text */
typedef real (*ae_hud_message_measure)(const wchar_t *text, long count, void *context);

struct ae_hud_message_flow
{
	real left;          /* every line's left x */
	real right;         /* the wrap limit */
	real first_baseline;
	real pitch;         /* baseline to baseline */
	real x;             /* the pen */
	real baseline;      /* the current line's */
	short lines;        /* lines started; 0: none yet */
	boolean line_empty; /* nothing drawn on the current line yet */
};

void ae_hud_message_flow_start(struct ae_hud_message_flow *flow, real left, real right, real first_baseline,
	real pitch);
/* the pen on a line: the first line when none is started yet */
void ae_hud_message_flow_line(struct ae_hud_message_flow *flow);
/* the next line (the first when none is started yet) */
void ae_hud_message_flow_new_line(struct ae_hud_message_flow *flow);

/* how many characters of text (up to length) go on the current line from the pen: a prefix ending after a whole
word (with the spaces after it), or the line break character; 0 when not even the first word fits and the line
already holds something (the caller starts a new line). On an empty line a word wider than the line is cut between
characters (at least one). *skip_spaces: leading spaces to drop first when the line is empty (counted in neither) */
long ae_hud_message_flow_fit(const struct ae_hud_message_flow *flow, const wchar_t *text, long length,
	ae_hud_message_measure measure, void *context, long *skip_spaces);
/* CE's text as the pen lays it out: every line end CE's text drawing breaks a line at (draw_string.c's
parse_unicode_string: the "|n" of the campaign's help text, a carriage return, a "\r\n" pair) and a lone "\n" one
'\n'; a tab a space. Up to maximum - 1 characters copied, then 0; the copy's length */
long ae_hud_message_flow_clean(const wchar_t *source, wchar_t *text, long maximum);
/* a picture width wide (plus gap after it): TRUE when it starts a new line first; then the pen's x is where it
goes, and the pen moves past it */
boolean ae_hud_message_flow_picture(struct ae_hud_message_flow *flow, real width, real gap, real *x);

/* ---------- sizes (campaign review 2026-10-18, F1-F3: the lines' own size, not the score strip's sub-line 14) */

#define AE_HUD_MESSAGE_PX 24.0f            /* the text's em at 1080p in a single view (capitals 0.70 x: 16.8 px, 0.60
                                              of stock's single_player_font) */
#define AE_HUD_MESSAGE_FLOOR_PX 16.0f      /* never under (capitals 11.2 px), whatever the view or the screen */
#define AE_HUD_MESSAGE_PITCH 1.3f          /* baseline to baseline, of the em */
#define AE_HUD_MESSAGE_WRAP_EM 26.0f       /* the widest a line runs, of the em */
#define AE_HUD_MESSAGE_WRAP_MINIMUM_EM 8.0f
#define AE_HUD_MESSAGE_EDGE 16.0f          /* px x scale kept clear of the view's right edge */
#define AE_HUD_MESSAGE_PICTURE 1.9f        /* a button picture's height, of the capitals (the PC menus' text icons) */
#define AE_HUD_MESSAGE_PICTURE_WIDE 2.6f   /* a wide one (a weapon's: twice as wide as tall or more) */
#define AE_HUD_MESSAGE_PICTURE_WIDE_ASPECT 2.0f
/* the HUD's message icons keep CE's proportions to its A button (a 24x30 cell): a stick with its RIGHT / LEFT label
(36x40) is drawn 40/30 as tall as a button, so 2.53 x the capitals and its label readable; never under a button's
1.9, never over a weapon's 2.6. (The review's "taller than wide" cannot tell them apart: CE's buttons are taller than
wide too.) */

/* the text's em in px for a view at scale (drawable height / 1080 x the view's factor) */
real ae_hud_message_px(real scale);
/* baseline to baseline for text of px */
real ae_hud_message_pitch(real px);
/* the wrap limit's x: under an ammo group (group_width > 0) no wider than twice the group, else 26 em; never more than
26 em, never past the view's edge less 16 px x scale, never under 8 em */
real ae_hud_message_wrap_right(real left, real view_width, real group_width, real px, real scale);
/* a picture's height for capitals of cap, from its size in texels and its sheet's A button's height in texels (0: not
the HUD's message icons) */
real ae_hud_message_picture_height(real cap, real texels_wide, real texels_high, real button_texels_high);

#endif /* __AE_HUD_MESSAGE_FLOW_H */
