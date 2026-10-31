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
/* (hud_draw.c) the weapon HUD's zoomed elements are drawn between TRUE and FALSE: not held back */
void ae_panels_zoom_element(boolean begin);
/* (hud_weapon.c) whether a weapon HUD number shown only when zoomed (state_type: its state) is drawn as a zoomed
element: always with CLASSIC; under AE only the range and elevation readouts (ae_panel_zoomed_number_is_readout), a
count of the weapon's is held back with CE's weapon panel */
boolean ae_panels_zoomed_number_kept(short state_type);
/* (hud.c, after CE's weapon and unit HUD and the waypoints) the AE panels of this view; TRUE when drawn */
boolean ae_panels_draw(short local_player_index);
/* debug.ae_panel_state's "script_blink": the scripts' blink bits taken as on (the sensor's too) */
boolean ae_panels_forced_script_blink(void);
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
/* CE's message lines' left x (HUD units) under AE: the ammo group's left edge, so they line up with it (owner 10-15) */
short ae_panels_message_left(short local_player_index, short x);
/* CE's message lines under AE: handed to AE's text in the score strip's style (ae_hud_messages.c), from the ammo
group's left edge and 6 px x scale under its foot; where AE's text cannot draw, CE's own drawn at the panels' scale
about their top-left (x, y: HUD units). end closes the block or undoes the scale */
void ae_panels_message_scale_begin(short local_player_index, short x, short y);
void ae_panels_message_scale_end(void);

#endif /* __AE_PANELS_H */
