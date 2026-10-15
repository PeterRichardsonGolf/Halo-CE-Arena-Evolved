/*
AE_SCORE_STRIP.H

The score strip (HUD STYLE AE, notes/specs/2026-10-11-score-strip.md 11):
drawn with the HUD, each view, from the engine reader (ae_strip_engine.c),
the model (ae_strip_model.c) and the layout (ae_strip_layout.c).
*/

#ifndef __AE_SCORE_STRIP_H
#define __AE_SCORE_STRIP_H

/* in the HUD's draw of a view (hud.c), after the unit's HUD and the item timers */
void ae_score_strip_draw(short local_player_index);

#endif /* __AE_SCORE_STRIP_H */
