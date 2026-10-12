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

#endif
