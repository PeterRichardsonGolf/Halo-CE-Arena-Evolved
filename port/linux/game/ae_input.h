/*
AE_INPUT.H

The AE menus' input bridge (ae_input.c): the game's controllers, keyboard
and mouse turned into ae_events and pointer moves for the screen stack
(ae_ui.h). Kept out of ae_ui.h, which stays free of the game's types for its
unit tests.
*/

#ifndef __AE_INPUT_H
#define __AE_INPUT_H

struct halo_ui_pointer;

/* reads this frame's input, dispatches it to the top screen, and leaves the
game's event queue empty (the menus behind see none of it) */
void ae_input_poll(void);
/* the menus' pointer (port/linux/include/halo_ui_pointer.h), while a screen is open */
void ae_input_pointer(struct halo_ui_pointer const *pointer);
/* after the last screen closes: holds back the inputs held now from the game's menus until each is let go of (at
most 2 s); ae_input_holding is nonzero while it does */
void ae_input_hold_begin(void);
int ae_input_holding(void);

#endif
