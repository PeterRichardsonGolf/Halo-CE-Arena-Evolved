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
/* a picture width wide (plus gap after it): TRUE when it starts a new line first; then the pen's x is where it
goes, and the pen moves past it */
boolean ae_hud_message_flow_picture(struct ae_hud_message_flow *flow, real width, real gap, real *x);

#endif /* __AE_HUD_MESSAGE_FLOW_H */
