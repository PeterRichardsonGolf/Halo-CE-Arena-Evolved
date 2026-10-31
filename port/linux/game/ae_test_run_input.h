/*
AE_TEST_RUN_INPUT.H

TEST RUN's verdict controls (DEV MODE): the owner's seven keys, read from the keyboard
(controls.test_*) and from every controller (BACK held with a button):

	pass F7 / BACK+A      fail F8 / BACK+B      note F4 / BACK+X      skip F5 / BACK+Y
	(a Nintendo pad's A, B, X and Y by their labels, as AE menus read them)
	reload F1 / BACK+d-pad up      back Home / BACK+LB      hide the card, while held: End / BACK+RB

The runner (ae_test_run.c) turns the reader on while a run is active and reads it once a frame.
While it is off the reader sees nothing and the pad is left to the game.
*/

#ifndef AE_TEST_RUN_INPUT_H
#define AE_TEST_RUN_INPUT_H

enum ae_test_run_key { AE_TEST_RUN_KEY_NONE, AE_TEST_RUN_KEY_PASS, AE_TEST_RUN_KEY_FAIL, AE_TEST_RUN_KEY_NOTE, AE_TEST_RUN_KEY_SKIP, AE_TEST_RUN_KEY_RELOAD, AE_TEST_RUN_KEY_BACK };

/* on while a run is active (off at start). Turning it on counts the keys and buttons already held as held, not as a press. */
void ae_test_run_input_enable(int on);
/* edge-triggered: one value per press, over the keyboard and every pad; a held key gives one press. Two newly pressed in
the same frame give the first in the enum's order and drop the other. NONE while the reader is off. Call once a frame. */
enum ae_test_run_key ae_test_run_input_pressed(void);
/* whether the hide-the-card key (End, or BACK+RB) is held; 0 while the reader is off */
int ae_test_run_input_hide_held(void);

/* The NOTE box's pad (ae_test_run_note.h). While the box is open the pad is the box's: every button of every pad is taken
from the game (the hook, ae_test_run_input_pad) and the presses of the d-pad, the left stick (as the d-pad's four), A, B, X, Y,
LB, RB and START are kept for the note module. A held direction repeats as AE's menus do (400 ms, then every 80 ms: ae_repeat_update). When
the box closes each button held at that moment stays taken until it is released, so the closing A or B never reaches the
game. This adds nothing to the reader's per-frame count: only ae_test_run_input_pressed() advances it. */
enum { AE_TEST_RUN_PAD_UP = 1, AE_TEST_RUN_PAD_DOWN = 2, AE_TEST_RUN_PAD_LEFT = 4, AE_TEST_RUN_PAD_RIGHT = 8,
	AE_TEST_RUN_PAD_A = 16, AE_TEST_RUN_PAD_B = 32, AE_TEST_RUN_PAD_X = 64, AE_TEST_RUN_PAD_Y = 128,
	AE_TEST_RUN_PAD_LB = 256, AE_TEST_RUN_PAD_RB = 512, AE_TEST_RUN_PAD_START = 1024 };
/* the AE_TEST_RUN_PAD_* pressed since the last call (a held button gives one press; one that was held when the box
opened gives none) */
unsigned ae_test_run_input_note_edges(void);
/* the device the hints are for, AE's own "last used" (ae_platform_keyboard_last_used: keys, mouse buttons and motion after
any pad; else the pad's family, ae_platform_input_scheme): an ae_device (ae_ui.h), AE_DEVICE_KEYBOARD_MOUSE while the
reader is off. Read it every frame: it follows the owner from pad to keyboard and back. */
int ae_test_run_input_device(void);

#endif
