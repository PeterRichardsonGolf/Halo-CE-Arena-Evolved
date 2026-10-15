/*
AE_STEADY_CAMERA.H

The view before CE's camera effects (firing shake, impulses), for the
waypoints: under HUD STYLE AE they are placed with it, so they hold still
while the world kicks (owner 2026-10-15: they "were jiggling and spazzing
out" on the Scorpion's machine gun).
*/

#ifndef AE_STEADY_CAMERA_H
#define AE_STEADY_CAMERA_H

/* the observer's camera for a local player's view, before the effect matrix (main.c, each frame) */
void ae_steady_camera_note(short local_player_index, const union real_point3d *position,
	const union real_vector3d *forward, const union real_vector3d *up);
/* the world-to-view matrix to place waypoints with in the view being drawn: the steady camera's under HUD STYLE AE,
else the one given */
const struct real_matrix4x3 *ae_steady_world_to_view(const struct real_matrix4x3 *shaken);

#endif
