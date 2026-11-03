/*
AE_BOMB_HUD.H

SEARCH AND DESTROY's carried-bomb tile on HUD STYLE AE (2026-10-28, the approved mockups): while the viewer carries the
bomb, a square tile in its team's colour with the bomb's pictogram (three strapped cases, the timer plate with its
three bars across and the beacon, the wire), bottom right of the motion sensor, 8 px clear of it at 1080p; with no
motion sensor drawn, the tile takes the sensor's corner a size up. The score strip says YOU HAVE THE BOMB with it
(ae_strip_model.c). Drawn with the panels' primitives (ae_panel_draw.h), so HUD scale and split views are the HUD's.
*/

#ifndef __AE_BOMB_HUD_H
#define __AE_BOMB_HUD_H

#include "ae_pure_types.h"

union point2d;
struct hud_placement_definition;

/* hud_unit.c, after the motion sensor (AE hook): sensor_corner its blips' centre, HUD units */
void ae_bomb_hud_tile(short local_player_index, const union point2d *sensor_corner, boolean split);
/* hud_unit.c, when no motion sensor is drawn (AE hook): blip_placement the HUD's own, for where it would be */
void ae_bomb_hud_tile_no_sensor(short local_player_index, struct hud_placement_definition const *blip_placement);

#endif // __AE_BOMB_HUD_H
