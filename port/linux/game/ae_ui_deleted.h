/*
AE_UI_DELETED.H

Arena Evolved: the menus' mouse targets of widgets deleted since the targets
were noted (port/linux/tests/ae_ui_deleted_test.c runs it). Pure.

ui_widget.c notes its mouse targets (each with its widget) as the menus are
drawn, and resolves a pointer's hover or click against them in the next
frame's ui_widgets_process_mouse. A widget deleted in between (a game joined
or started closes the menus after their render) left a target on freed
memory: the hover read its name and the game crashed (2026-10-19, hosting
Blood Gulch from the network test). ui_widget_delete notes each widget it
deletes here; ui_widgets_process_mouse drops their targets before using any.
*/

#ifndef __AE_UI_DELETED_H
#define __AE_UI_DELETED_H

/* the widgets one frame can delete before every target is dropped instead */
#define AE_UI_DELETED_MAXIMUM 256

/* a widget being deleted (ui_widget_delete) */
void ae_ui_deleted_note(void const *widget);
/* forgets the deleted widgets: the targets were noted again, or emptied */
void ae_ui_deleted_clear(void);
/* removes the targets (count of them, stride bytes apart, each's widget pointer
at widget_offset) whose widget was deleted, keeping the others in order;
nonzero when any was removed */
int ae_ui_deleted_forget(void *targets, long *count, unsigned long stride, unsigned long widget_offset);

#endif
