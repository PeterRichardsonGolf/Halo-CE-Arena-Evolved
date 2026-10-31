/*
AE_STEADY_CAMERA.C

The view before CE's camera effects, for the waypoints (ae_steady_camera.h).
*/

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "game/players.h"
#include "render/render.h"
#include "render/render_cameras.h"

#include "ae_hud_style.h"
#include "ae_steady_camera.h"

static struct
{
	boolean valid;
	real_point3d position;
	real_vector3d forward;
	real_vector3d up;
	struct render_frustum frustum;
	boolean built;
} steady[MAXIMUM_LOCAL_PLAYERS];

void ae_steady_camera_note(short local_player_index, const real_point3d *position, const real_vector3d *forward,
	const real_vector3d *up)
{
	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return;
	steady[local_player_index].valid = TRUE;
	steady[local_player_index].position = *position;
	steady[local_player_index].forward = *forward;
	steady[local_player_index].up = *up;
	steady[local_player_index].built = FALSE;
}

const real_matrix4x3 *ae_steady_world_to_view(const real_matrix4x3 *shaken)
{
	short index = render.local_player_index;

	if (!ae_hud_style_is_ae() || index < 0 || index >= MAXIMUM_LOCAL_PLAYERS || !steady[index].valid)
		return shaken;
	if (!steady[index].built)
	{
		/* (the view being drawn, its own camera's place and facing only replaced: same field of view, viewport) */
		struct render_camera camera = render.camera;

		camera.position = steady[index].position;
		camera.forward = steady[index].forward;
		camera.up = steady[index].up;
		render_camera_build_frustum(&camera, NULL, &steady[index].frustum, TRUE);
		steady[index].built = TRUE;
	}
	return &steady[index].frustum.world_to_view;
}
