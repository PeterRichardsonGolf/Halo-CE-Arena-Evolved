/*
AE_HUD_MESSAGES.H

HUD STYLE AE's CE message lines in the score strip's text style (owner 10-15:
"full restyle"): the state, help and objective lines with their inline button
and weapon pictures ("Press [X] to enter driver seat of Scorpion", "Hold BACK
for score") and the message list ("Picked up 60 rounds for assault rifle").
CE still runs every line's bookkeeping (hud_messaging.c, and ui_widget.c's
draw_string_and_hack_in_icons for a custom line's %icons); only the drawing is
handed here, by AE hook lines at each of its draws:

- the text in AE's text (ae_hud_text.c: the HUD's Overpass 900, 24 px at 1080p
  single view x the view's HUD scale, 16 px at least in any view: campaign
  review 2026-10-18; it was the strip's 14 px sub-line size, 0.36 of stock's
  campaign text) with the strip's dark outline, in white (229,240,255) at CE's
  alpha (an objective's fade, a list line's fade; help and state lines
  opaque, where CE's colours are half transparent); a flashing help line goes
  toward the PC menus' ice-blue (161,206,255) in CE's flash beat;
- the pictures CE's own bitmaps, a button as tall as the text's capitals x
  1.9 (the PC menus' text icons), a stick with its RIGHT / LEFT label x 2.4
  and a wide picture (a weapon's) x 2.6, centred on them, with the text's
  dark halo, in their own colour where CE gives them one (a button's), else
  the text's;
- the lines from the ammo group's left edge, the first line's capitals 6 px x
  scale under its foot, 1.3 em apart, broken between words at twice the
  group's width, or 26 em with no group, never past the view's edge
  (ae_hud_message_flow.h has the numbers).

Include after cseries.h (pixel32). CLASSIC never opens a block: CE draws as before.
*/

#ifndef __AE_HUD_MESSAGES_H
#define __AE_HUD_MESSAGES_H

#include <wchar.h>

#include "ae_pure_types.h"

struct bitmap_data;
struct hud_color_definition;
union real_argb_color;
union real_rectangle2d;

/* (ae_panels.c, at the start of hud_messaging_update's drawing) the view's block: the lines' left x, the first
line's capitals' top, the view's width and the ammo group's (0 without it), all view pixels, and the view's HUD
scale. FALSE when AE's text cannot draw (CE's lines are then drawn smaller, as before) */
boolean ae_hud_messages_begin(short local_player_index, real left, real first_cap_top, real view_width,
	real group_width, real scale);
void ae_hud_messages_end(void);
/* (hud_messaging.c, once the lines' colour is chosen) which line is drawn and how: an objective keeps AE's white at
its fade; a help or state line is opaque white, and while a help line flashes (the scripts' enable_hud_help_flash)
its flash phase is read back from the colour CE drew with the help colour's get_flash_color */
void ae_hud_messages_colour(boolean objective, boolean help, boolean flashing,
	struct hud_color_definition const *help_color, union real_argb_color const *drawn);
/* whether a block is open (CE's message lines are being handed here) */
boolean ae_hud_messages_open(void);

/* (hud_messaging.c, ui_widget.c) a piece of a state, help or objective line: TRUE when AE drew it */
boolean ae_hud_message_text(wchar_t const *text);
/* a picture of one: the bitmap and clip CE draws, CE's colour and whether it is the icon's own */
boolean ae_hud_message_icon(struct bitmap_data const *bitmap, union real_rectangle2d const *clip,
	pixel32 color, boolean own_color);
/* (ae_key_prompts.c) the open block's view, text em and capitals (px) and the line's alpha (1 for a help or state
line, else CE's colour's: an objective's fade); FALSE when no block is open */
boolean ae_hud_message_metrics(short *view, real *px, real *cap, real *alpha);
/* (ae_key_prompts.c) room on the pen for a picture whose body is width x height px, before px after the text before it
(none at a line's start) and after px after it, the body's middle on the capitals' middle: its body's left and top */
void ae_hud_message_place(real width, real height, real before, real after, real *x, real *top);
/* (hud_messaging.c) a line of the message list, on a line of its own after what came before */
boolean ae_hud_message_list_line(wchar_t const *text);
/* (hud_messaging.c) CE keeps the first line for the state message: the list starts on the second at least */
void ae_hud_messages_keep_first_line(void);

#endif /* __AE_HUD_MESSAGES_H */
