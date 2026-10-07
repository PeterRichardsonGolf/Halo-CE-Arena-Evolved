/*
AE_HOOKS.H

What the AE hook lines in upstream files call (ae_hooks.txt lists those lines;
notes/ae-hooks.md says where each goes). Included after cseries.h (boolean).
*/

#ifndef __AE_HOOKS_H
#define __AE_HOOKS_H

struct halo_ui_pointer;
union rectangle2d;

/* display.arena_menus (HALO_ARENA_MENUS), read once */
boolean ae_menus_active(void);
/* TRUE: AE's menus handled this frame (process_ui_widgets returns) */
boolean ae_ui_process(void);
/* draws AE's screens over the widgets of one view (render_ui_widgets) */
void ae_ui_render(short local_player_index, union rectangle2d const *window_bounds);
/* TRUE: AE's menus took the pointer (ui_widgets_process_mouse drops its clicks) */
boolean ae_ui_pointer(struct halo_ui_pointer const *pointer);

#endif
