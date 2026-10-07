/*
AE_INPUT.H

The AE menus' input bridge: the game's events and the menus' pointer turned
into ae_events for the screen stack (ae_ui.h). Kept out of ae_ui.h, which
stays free of the game's types for its unit tests.
*/

#ifndef __AE_INPUT_H
#define __AE_INPUT_H

struct halo_ui_pointer;

/* reads this frame's input and dispatches it to the top screen */
void ae_input_poll(void);
/* the menus' pointer (port/linux/include/halo_ui_pointer.h), while a screen is open */
void ae_input_pointer(struct halo_ui_pointer const *pointer);

#endif
