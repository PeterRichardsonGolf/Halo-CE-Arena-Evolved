/*
AE_BUTTONS_TAGS.H

Arena Evolved: PlayStation button icons, the game's side (ae_buttons_tags.c;
the design is port/linux/src/ae_buttons.h's).
*/

#ifndef __AE_BUTTONS_TAGS_H
#define __AE_BUTTONS_TAGS_H

struct bitmap_data;

/* hud_hires_tags.c's hud_hires_sprite_bitmap (and so the menus' text icons
and, through hud_messaging.c's AE hook, the in-game messages'): a sheet's
sprite drawn from AE's placeholder while a PlayStation controller is the
last used, or NULL (the sheet's own, or upstream's high-res placeholder) */
struct bitmap_data const *ae_buttons_sprite_bitmap(struct bitmap_data const *bitmap, short sequence_index);

void ae_buttons_tags_loaded(void);
void ae_buttons_tags_unloaded(void);

/* (ce_hud.c's AE hook) a Custom Edition map's message icons sheet given the Xbox's icons: the group (its tag data),
the first of the Xbox's sequences and bitmaps in it */
void ae_buttons_ce_sheet(void const *group, long first_sequence, long first_bitmap);
/* (ui_widget.c's render_state_bitmap, AE hooks) the text icons' sheet: on a Custom Edition map whose message icons
have the Xbox's, that sheet (the globals' hud_msg_icons_sm is Halo PC's, without them), else bitmap_group_index; and
the scale its icons are drawn at there (two thirds: the small sheet's size), else 1 */
long ae_buttons_ce_menu_icons(long bitmap_group_index);
float ae_buttons_ce_menu_icon_scale(long bitmap_group_index);

#endif
