/*
AE_EXTRA_BUTTONS_RULES.H

A Steam Controller's EXTRA BUTTONS screen (ae_screen_keys.c's back buttons screen), the words and the arithmetic: its
seven rows in two groups (BACK: L4, R4, L5, R5; FRONT: the right trackpad click, the Steam button, quick access), each
row's help, the row's summary on CONTROLS and the dialogs' texts. Other pads keep BACK PADDLES' words (the screen's own).
Pure (port/linux/tests/ae_extra_buttons_rules_test.c).
*/

#ifndef AE_EXTRA_BUTTONS_RULES_H
#define AE_EXTRA_BUTTONS_RULES_H

#include <stddef.h>

enum { AE_EXTRA_BUTTONS_ROWS = 7, AE_EXTRA_BUTTONS_BACK = 4, AE_EXTRA_BUTTONS_FRONT = 3 };

/* the row's key (the config's own, in the screen's row order), label, and help's first line */
const char *ae_extra_buttons_key(int row);
const char *ae_extra_buttons_label(int row);
const char *ae_extra_buttons_help(int row);
/* the row's group, 0 BACK or 1 FRONT, and the groups' names (the rail items and the list headers) */
int ae_extra_buttons_group(int row);
/* the text on the key cap that stands for the row's button where no picture is drawn ("R PAD" for the right trackpad click;
"" for a row with none) */
const char *ae_extra_buttons_chip(int row);
const char *ae_extra_buttons_group_name(int group);

/* the summary on CONTROLS' row and the screen's crumb: "NONE" for none set, "n SET" else; set[i] nonzero: row i is not NONE */
void ae_extra_buttons_summary(const int set[AE_EXTRA_BUTTONS_ROWS], char *text, size_t size);

/* the dialogs: the keep dialog's body (name: the player's), the restore dialog's body and its already-at-defaults text */
void ae_extra_buttons_keep_body(const char *name, char *text, size_t size);
void ae_extra_buttons_restore_body(const char *name, char *text, size_t size);
#define AE_EXTRA_BUTTONS_TITLE "EXTRA BUTTONS"
#define AE_EXTRA_BUTTONS_RESTORE_TITLE "Restore EXTRA BUTTONS defaults?"
#define AE_EXTRA_BUTTONS_AT_DEFAULTS "EXTRA BUTTONS is already at its defaults."

#endif
