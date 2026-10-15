/*
AE_HUD_MESSAGE_FLOW.C

Where a piece of a CE message line goes under HUD STYLE AE
(ae_hud_message_flow.h).
*/

#include "ae_hud_message_flow.h"

void ae_hud_message_flow_start(struct ae_hud_message_flow *flow, real left, real right, real first_baseline,
	real pitch)
{
	flow->left = left;
	flow->right = right;
	flow->first_baseline = first_baseline;
	flow->pitch = pitch;
	flow->x = left;
	flow->baseline = first_baseline;
	flow->lines = 0;
	flow->line_empty = TRUE;
}

void ae_hud_message_flow_line(struct ae_hud_message_flow *flow)
{
	if (flow->lines > 0)
		return;
	flow->lines = 1;
	flow->x = flow->left;
	flow->baseline = flow->first_baseline;
	flow->line_empty = TRUE;
}

void ae_hud_message_flow_new_line(struct ae_hud_message_flow *flow)
{
	if (flow->lines == 0)
	{
		ae_hud_message_flow_line(flow);
		return;
	}
	flow->lines++;
	flow->x = flow->left;
	flow->baseline += flow->pitch;
	flow->line_empty = TRUE;
}

static boolean is_space(wchar_t c) { return (boolean)(c == L' ' || c == L'\t'); }

long ae_hud_message_flow_fit(const struct ae_hud_message_flow *flow, const wchar_t *text, long length,
	ae_hud_message_measure measure, void *context, long *skip_spaces)
{
	real room = flow->right - flow->x;
	long skip = 0, end, best = 0, first_word_end = 0, i;

	*skip_spaces = 0;
	if (!text || length <= 0)
		return 0;
	if (flow->line_empty)
	{
		while (skip < length && is_space(text[skip]))
			skip++;
		*skip_spaces = skip;
		text += skip;
		length -= skip;
	}
	/* (the text up to the line break, if any) */
	for (end = 0; end < length && text[end] != L'\n'; end++)
		;
	for (i = 0; i < end && is_space(text[i]); i++)
		;
	if (i == end)
		return end; /* spaces only: they move the pen */

	/* the longest prefix ending after a whole word that fits */
	for (i = 0; i < end; i++)
	{
		if (is_space(text[i]) || (i + 1 < end && !is_space(text[i + 1])))
			continue;
		/* (text[i] ends a word) */
		if (!first_word_end)
			first_word_end = i + 1;
		if (measure(text, i + 1, context) > room)
			break;
		best = i + 1;
	}
	if (best)
	{
		while (best < end && is_space(text[best]))
			best++;
		return best;
	}
	if (!flow->line_empty)
		return 0;
	/* a word wider than the whole line: cut between characters */
	for (best = 1; best < first_word_end && measure(text, best + 1, context) <= room; best++)
		;
	return best;
}

boolean ae_hud_message_flow_picture(struct ae_hud_message_flow *flow, real width, real gap, real *x)
{
	boolean broke = FALSE;

	ae_hud_message_flow_line(flow);
	if (!flow->line_empty && flow->x + width > flow->right)
	{
		ae_hud_message_flow_new_line(flow);
		broke = TRUE;
	}
	*x = flow->x;
	flow->x += width + gap;
	flow->line_empty = FALSE;
	return broke;
}
