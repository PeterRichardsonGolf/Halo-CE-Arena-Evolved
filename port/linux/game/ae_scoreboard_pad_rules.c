/* AE_SCOREBOARD_PAD_RULES.C: see the header */

#include "ae_scoreboard_pad_rules.h"

long ae_sb_pad_take(struct ae_sb_pad *state, int open, unsigned short *buttons, unsigned long ms)
{
	const unsigned short mask = AE_SB_PAD_UP | AE_SB_PAD_DOWN;
	int up, down;
	long pages = 0;

	if (!open)
	{
		/* (closed: what was taken stays taken till let go) */
		state->taken = (unsigned short)(state->taken & *buttons);
		*buttons = (unsigned short)(*buttons & ~state->taken);
		ae_repeat_update(&state->up, 0, ms);
		ae_repeat_update(&state->down, 0, ms);
		return 0;
	}
	state->taken = (unsigned short)(*buttons & mask);
	up = (*buttons & AE_SB_PAD_UP) != 0;
	down = (*buttons & AE_SB_PAD_DOWN) != 0;
	*buttons = (unsigned short)(*buttons & ~mask);
	/* (both at once is a slip, not a direction) */
	if (ae_repeat_update(&state->up, up && !down, ms))
		pages--;
	if (ae_repeat_update(&state->down, down && !up, ms))
		pages++;
	return pages;
}
