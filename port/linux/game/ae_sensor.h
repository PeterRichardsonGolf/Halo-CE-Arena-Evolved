/*
AE_SENSOR.H

HUD STYLE AE's motion sensor (spec 14.15 part 4, G3, G4, G8; plan Task 16):
flat, in CE's place and size (its blips' centre, its sweep's radius), drawn in
the AE panels' language: a backing, the disc, the rim, the inner ring at half
the range, three bearing dividers, the view's wedge from the render camera's
field of view, the player's chevron, CE's own blips (their places, sizes,
colours, history and pulse are CE's: motion_sensor.c computes them and AE
hook lines hand them over instead of to the rasterizer), the sweep ring and
the range label on the shared bottom line. CE's contact logic, its radar
settings and the scripts' show flags are untouched: the sensor shows exactly
when CE's does. CLASSIC draws CE's.
*/

#ifndef __AE_SENSOR_H
#define __AE_SENSOR_H

#include "ae_pure_types.h"

union real_point2d;
union real_rgb_color;
union point2d;

/* whether HUD STYLE AE draws the sensor */
boolean ae_sensor_active(void);
/* (motion_sensor.c's render_blip) a blip CE would draw: TRUE when AE takes it (CE does not draw it) */
boolean ae_sensor_take_blip(const union real_point2d *position, real fade, real size,
	const union real_rgb_color *color, boolean custom, real sensor_scale);
/* (motion_sensor.c) whether CE's blips are AE's (CE's sweep is then not drawn) */
boolean ae_sensor_takes_blips(void);
/* (hud_unit.c, after CE's sensor work) draws the AE sensor round CE's blips' centre (HUD units of the view) */
void ae_sensor_draw(short local_player_index, const union point2d *centre, boolean split);
/* (hud.c, as the map's HUD globals load for a game) the game's SENSOR RANGE into them, the map's own for STOCK or
no game (ae_sensor_range_rules.h); the rules log says which */
void ae_sensor_range_apply(void);
/* the map's own motion sensor range, whatever the gametype set (current when none was set yet): how far enemies'
names show over their heads (hud.c), which the gametype's SENSOR RANGE does not change */
float ae_sensor_range_map_own(float current);
/* the sensor's box in the view px from its last draw, FALSE when none */
boolean ae_sensor_rectangle(short local_player_index, real *x0, real *y0, real *x1, real *y1);

#endif /* __AE_SENSOR_H */
