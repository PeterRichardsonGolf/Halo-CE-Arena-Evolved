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
/* nonzero while a controller (or the keyboard driving the first) holds a button or a stick */
int ae_input_held(void);

#endif
