/*
AE_SCOREBOARD_PAD_RULES.H

(0.7.8) A controller's D-pad on the open scoreboard (pure, unit-tested in port/linux/tests/ae_scoreboard_pad_rules_test.c):
up and down turn the pages as Page Up/Down do, a step when pressed and again as held (ae_repeat_update's pace), and
while the board is open the game gets neither of them. A press still held as the board closes stays taken until it is
let go, so the close does not hand the game a press of it (a marks wheel, a flashlight).
*/

#ifndef __AE_SCOREBOARD_PAD_RULES_H
#define __AE_SCOREBOARD_PAD_RULES_H

#include "ae_ui.h"

/* XInput's D-pad bits */
#define AE_SB_PAD_UP 0x0001
#define AE_SB_PAD_DOWN 0x0002

struct ae_sb_pad
{
	struct ae_repeat up, down;
	unsigned short taken;  /* the D-pad buttons held and not yet let go of */
};

/* one read of a pad's buttons at ms: the D-pad's up and down taken out of *buttons while open (and while one that was
held is), returns the pages to scroll (up negative, down positive; both held: none) */
long ae_sb_pad_take(struct ae_sb_pad *state, int open, unsigned short *buttons, unsigned long ms);

#endif
