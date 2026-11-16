/*
AE_DEAD_ALLY_MARKERS.H

DEAD ALLY MARKERS (HUD > NAMES, display.dead_ally_markers): the cross
where a teammate fell (ae_dead_ally_markers.c). hud.c's AE hook draws it.
*/

#ifndef __AE_DEAD_ALLY_MARKERS_H
#define __AE_DEAD_ALLY_MARKERS_H

/* this view's markers (render.local_player_index's, in render.camera), in the HUD's pass */
void ae_dead_ally_markers_draw(short local_player_index);

#endif /* __AE_DEAD_ALLY_MARKERS_H */
