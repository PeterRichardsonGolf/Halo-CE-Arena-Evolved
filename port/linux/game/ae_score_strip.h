/*
AE_SCORE_STRIP.H

The score strip (HUD STYLE AE, notes/specs/2026-10-11-score-strip.md 11):
drawn with the HUD, each view, from the engine reader (ae_strip_engine.c),
the model (ae_strip_model.c) and the layout (ae_strip_layout.c).
*/

#ifndef __AE_SCORE_STRIP_H
#define __AE_SCORE_STRIP_H

#include "ae_pure_types.h"

/* in the HUD's draw of a view (hud.c), after the unit's HUD and the item timers */
void ae_score_strip_draw(short local_player_index);
/* (hud.c) the view's unit HUD was drawn this frame: the strip draws only then */
void ae_score_strip_unit_hud_drawn(short local_player_index);
/* whether the view's strip was drawn last frame (for the corner clock's and the timers list's switches) */
boolean ae_score_strip_visible(short local_player_index);
/* (the debug log, debug.score_strip_log) the last frame's: its alpha, the clock's and the left score's boxes
(x, y, w, h in the view's screen pixels); FALSE when it was not drawn */
boolean ae_score_strip_last_drawn(short local_player_index, real *alpha, real clock[4], real left_score[4]);
/* (the debug log) the last frame's time played line (MATCH CLOCK BOTH): its box; FALSE when none was drawn */
boolean ae_score_strip_last_played(short local_player_index, real played[4]);
/* (the debug log) the shared line the view's strip was last laid out on */
real ae_score_strip_last_line(short local_player_index);
/* the view's scale for AE's HUD: its factor (a half screen 0.85, a quarter 0.75) x the screen's height / 1080 */
real ae_score_strip_view_scale(void);
/* the strip's last drawn extent in the view px: left, top, right, bottom; FALSE when not drawn */
boolean ae_score_strip_extent(short local_player_index, real extent[4]);
/* the view's shared bottom line (spec 11.12), screen pixels from its top: the motion sensor's foot, or where it
would be */
real ae_score_strip_shared_line_y(short local_player_index);

#endif /* __AE_SCORE_STRIP_H */
