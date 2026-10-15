/*
AE_PANELS.H

HUD STYLE AE's ammo + grenade group and shield + health group (spec 14.15
parts 2, 3, 5; plan Task 15), drawn per view in CE's places from the engine's
data (ae_panel_engine.c, ae_panel_model.c, ae_panel_layout.c,
ae_panel_draw.c). CLASSIC draws CE's own panels untouched.

CE's own weapon panel, grenade panel and the player's own shield and health
meters still run under HUD STYLE AE (their sounds, flash timers and placements
are CE's, and the AE panels take their anchors from those placements): only
their drawing is held back, by AE hook lines round their calls
(hud_weapon.c, hud_unit.c) and before the HUD's two screen-quad draws
(hud_draw.c). The crosshair, the vehicle's own unit panels, the flashlight
meter and everything else keep drawing.
*/

#ifndef __AE_PANELS_H
#define __AE_PANELS_H

#include "ae_pure_types.h"
#include "ae_panel_layout.h"

/* (hud_weapon.c, hud_unit.c) round CE's weapon / grenade panel and the player's own meters: TRUE holds their
drawing back while HUD STYLE AE draws its own; FALSE lets the HUD draw again */
void ae_panels_hide_ce(boolean hide);
/* (hud_draw.c) whether the HUD's draw is held back now */
boolean ae_panels_hiding_ce(void);
/* (hud.c, after CE's weapon and unit HUD and the waypoints) the AE panels of this view; TRUE when drawn */
boolean ae_panels_draw(short local_player_index);
/* this view's panel rectangles (view px) from its last draw: the ammo group and the shield group with its rows */
void ae_panels_rectangles(short local_player_index, struct ae_panel_rect out[2], short *count);

union rectangle2d;
/* (hud_item_timers.c) a waypoint or TRAINING label's alpha factor: 0.35 where its box (HUD units of the view) meets
a panel grown by 8 px (the ammo group, the shield group with its rows, the sensor, the timers list, the score
strip: the panels' last drawn), else 1; 1 with CLASSIC */
real ae_panels_label_alpha(short local_player_index, const union rectangle2d *bounds);

/* (hud_messaging.c) the HUD's messages' top line (HUD units of the view) moved under the AE ammo group where it
would start inside it: the AE rounds stand taller than CE's split-screen panel (3p / 4p "Hold BACK for score") */
short ae_panels_message_top(short local_player_index, short line_top);

#endif /* __AE_PANELS_H */
