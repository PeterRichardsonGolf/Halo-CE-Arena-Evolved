/*
AE_WHEEL_DRAW.H

The CALLOUT WHEEL drawn (ae_wheel_draw.c): the ring of eight sectors (three while dead) about the holder's view's middle,
in HUD STYLE AE's inks (the HUD line, the dark fill, the keylines, Overpass 900 outlined), the focused sector filled in
the line with its word under or over the ring, the direction wedge, and in one view the legend of hints. The numbers are
ae_wheel_layout.c's; the open wheel's state is ae_wheel.c's. Other HUD styles draw the same forms in the same HUD line
(the HUD line colour is CE's own).

The ring draws in the holder's view only, centred on the view's middle: the HUD area does not move it, as it follows the
reticle. Include after cseries.h.
*/

#ifndef __AE_WHEEL_DRAW_H
#define __AE_WHEEL_DRAW_H

/* hud.c, after the marks: the wheel of this local player's view, while it is open and for the send's flash and closing */
void ae_wheel_draw(short local_player_index);

/* whether the wheel covers a point of this view now (ae_marks_draw's markers fade to 0.3 behind a sector): x and y in the
HUD's units from the view's top left, as the markers' places are */
boolean ae_wheel_covers(short local_player_index, real x, real y);

/* (the game's text rows, ae_hud_messages.c: an open wheel hides the rows that would touch it) whether this view's wheel is open
(or closing), and where it keeps text off: the ring's centre, the radius of its outer edge with the focused sector's growth (so
it does not change as the focus moves), the focused word's box as x0, y0, x1, y1 (empty with no focus) and the limit to keep, all
in the view's pixels from its top left */
boolean ae_wheel_text_keepout(short local_player_index, real *centre_x, real *centre_y, real *radius, real word_box[4],
	real *limit);

#endif
