/*
AE_VIEW_SWAY.H

The first-person weapon's sway (ae_view_sway_rules.h has the springs; AE's default is display.viewmodel_sway 0: the weapon
held still, owner 2026-11-07): at display.viewmodel_sway 100 and
display.viewmodel_sway_speed 1 the stock springs run once a tick as they always did; any other setting runs them every
frame with the frame's own time, chasing a lean scaled by the strength, faster by the speed. Only the drawing changes.
*/

#ifndef __AE_VIEW_SWAY_H
#define __AE_VIEW_SWAY_H

#include "cseries.h"
#include "math/real_math.h"

/* the tick's sway update (first_person_weapons.c): keeps the unit's throttle for the frames; 1 when the stock springs
step now (the stock setting), 0 when the frames step them */
int ae_view_sway_tick(short local_player_index, real throttle_i, real throttle_j);
/* a frame's sway (first_person_weapons.c, where the pose is built): the springs stepped by the frame's time from the
facing's change since the last frame, when the setting is not the stock one */
void ae_view_sway_frame(short local_player_index, union real_vector2d *position, union real_vector2d *position_velocity,
	union real_vector2d *turning, union real_vector2d *turning_velocity, real yaw, real pitch);

#endif
