/*
AE_HUD_MESSAGES.H

HUD STYLE AE's CE message lines in the score strip's text style (owner 10-15:
"full restyle"): the state, help and objective lines with their inline button
and weapon pictures ("Press [X] to enter driver seat of Scorpion", "Hold BACK
for score") and the message list ("Picked up 60 rounds for assault rifle").
CE still runs every line's bookkeeping (hud_messaging.c, and ui_widget.c's
draw_string_and_hack_in_icons for a custom line's %icons); only the drawing is
handed here, by AE hook lines at each of its draws:

- the text in AE's text (ae_hud_text.c: the HUD's Overpass 900 at the strip's
  sub-line size, 14 px at 1080p single view x the view's HUD scale, 14 px at
  least in quarter views) with the strip's dark outline, in white
  (229,240,255) at CE's alpha (an objective's fade, a list line's fade);
- the pictures CE's own bitmaps, a button as tall as the text's capitals x
  1.25 and a wide picture (a weapon's) x 2.0, centred on them, with the
  text's dark halo, in their own colour where CE gives them one (a button's),
  else white;
- the lines from the ammo group's left edge, the first line's capitals 6 px x
  scale under its foot, 18 px x scale apart, broken between words at the
  view's width (mirroring the left margin).

Include after cseries.h (pixel32). CLASSIC never opens a block: CE draws as before.
*/

#ifndef __AE_HUD_MESSAGES_H
#define __AE_HUD_MESSAGES_H

#include <wchar.h>

#include "ae_pure_types.h"

struct bitmap_data;
union real_rectangle2d;

/* (ae_panels.c, at the start of hud_messaging_update's drawing) the view's block: the lines' left x, the first
line's capitals' top, the view's width and the ammo group's (0 without it), all view pixels, and the view's HUD
scale. FALSE when AE's text cannot draw (CE's lines are then drawn smaller, as before) */
boolean ae_hud_messages_begin(short local_player_index, real left, real first_cap_top, real view_width,
	real group_width, real scale);
void ae_hud_messages_end(void);
/* whether a block is open (CE's message lines are being handed here) */
boolean ae_hud_messages_open(void);

/* (hud_messaging.c, ui_widget.c) a piece of a state, help or objective line: TRUE when AE drew it */
boolean ae_hud_message_text(wchar_t const *text);
/* a picture of one: the bitmap and clip CE draws, CE's colour and whether it is the icon's own */
boolean ae_hud_message_icon(struct bitmap_data const *bitmap, union real_rectangle2d const *clip,
	pixel32 color, boolean own_color);
/* (hud_messaging.c) a line of the message list, on a line of its own after what came before */
boolean ae_hud_message_list_line(wchar_t const *text);
/* (hud_messaging.c) CE keeps the first line for the state message: the list starts on the second at least */
void ae_hud_messages_keep_first_line(void);

#endif /* __AE_HUD_MESSAGES_H */
