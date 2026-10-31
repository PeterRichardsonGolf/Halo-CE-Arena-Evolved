/*
AE_TEST_RUN_NOTE.H
TEST RUN's note box (DEV MODE): the owner gives a line of text when a test is FAILed or NOTEd. While the box is open it
has the keyboard (xinput_sdl.c asks ae_test_run_typing) and the pad (ae_test_run_input.c takes every pad button from the
game and gives the d-pad, A and B to this module).

Keyboard: printable keys and pasted text type (UTF-8 is kept whole), Backspace and Delete delete a character, Left and
Right, Home and End move the cursor, Ctrl+V pastes the clipboard's first line, Enter saves (an empty note is fine),
Escape cancels. Pad: the d-pad moves over the quick tags (ae_test_run_note_tag_label), A on a tag saves its text, in capitals as
shown, as the note; NO NOTE saves an empty note; B cancels; TYPE… is the on-screen keyboard's (ae_test_run_note_type_open).
*/
#ifndef AE_TEST_RUN_NOTE_H
#define AE_TEST_RUN_NOTE_H

struct ae_test_run_keyboard;

/* opens the box: what was typed before is dropped */
void ae_test_run_note_open(void);
/* closes the box (the keys go back to the game, the pad's buttons when they are released) */
void ae_test_run_note_close(void);
/* the box has the keyboard and the pad (1 while it is open) */
int ae_test_run_typing(void);
/* reads the keys typed and the pad's presses this frame into note (size bytes, edited in place): 0 still typing, 1 saved
(Enter, or a tag), -1 cancelled (Escape, or B) */
int ae_test_run_note_poll(char *note, int size);

/* the quick tags, in the order the box shows them; the last is NO NOTE (an empty note) */
int ae_test_run_note_tag_count(void);
const char *ae_test_run_note_tag_label(int index);
/* the tag the pad's cursor is on */
int ae_test_run_note_tag_index(void);
/* the keyboard's cursor in the note being typed, a byte offset */
int ae_test_run_note_cursor(void);

/* TYPE…: the pad keyboard (ae_test_run_keyboard.h) takes the pad: A types, X is Backspace, Y a space, LB the case, RB the
symbols, START or DONE saves the note, B goes back to the tags and drops what was typed. The keys keep typing into the same
note. (The AE screen stack's keyboard, ae_keyboard_open, was tried in game and not used: see the commit.) */
void ae_test_run_note_type_open(void);
/* the pad keyboard is open, and its state (for the card to draw) */
int ae_test_run_note_picker_open(void);
const struct ae_test_run_keyboard *ae_test_run_note_picker(void);

/* The verdict chooser: after a NOTE is saved the box stays up as three chips, PASS, FAIL and CONTINUE (which records SKIP). It has
the keyboard and the pad as the box does. Keyboard: F7 PASS, F8 FAIL, F5 CONTINUE, Left and Right then Enter on a chip, Escape
to stay. Pad: the d-pad (or stick) moves, A picks, B stays. */
enum { AE_TEST_RUN_CHOICE_NONE, AE_TEST_RUN_CHOICE_PASS, AE_TEST_RUN_CHOICE_FAIL, AE_TEST_RUN_CHOICE_CONTINUE,
	AE_TEST_RUN_CHOICE_STAY };
void ae_test_run_note_choose_open(void);
/* 0 still choosing, else the AE_TEST_RUN_CHOICE_* picked (STAY: Escape or B) */
int ae_test_run_note_choose_poll(void);
/* the chooser is up, and the chip the cursor is on (0 PASS, 1 FAIL, 2 CONTINUE) */
int ae_test_run_note_choosing(void);
int ae_test_run_note_choice_index(void);

#endif
