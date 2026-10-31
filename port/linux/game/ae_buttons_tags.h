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

/* AE's own picture by its table tag (ae_buttons.h's own glyphs: "ae\\key_prompts\\ae"): a placeholder bitmap the HUD's
quads draw it from (coordinates 0 to 1 over the whole picture), made on first use and deleted with the map; NULL when
it cannot be (no such picture, its PNG not decoded) */
struct bitmap_data const *ae_buttons_own_bitmap(char const *tag);
/* the own picture's texture decoded now (a map loading: not at its first draw) */
void ae_buttons_own_preload(char const *tag);

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
/* (ui_widget.c's render_state_bitmap, AE hook) a text icon in the PC menus (the carnage report's, the lobbies'),
drawn as the HUD hints' are: sized by the line's font's capitals and centred on them, the cursor moved past it (TRUE);
FALSE: not in the PC menus (the stock draw). The cursor moves as the stock draw's does: past the icon plus width_offset,
or with absolute_width (the icon's _hud_icon_absolute_width_bit) to width_offset past where it starts; the icon's own
offset is not used, the icon being centred on the capitals instead */
boolean ae_buttons_menu_icon(struct bitmap_data const *bitmap, union real_rectangle2d const *clip,
	union rectangle2d *cursor_bounds, short width_offset, boolean absolute_width, unsigned long color);

#endif
