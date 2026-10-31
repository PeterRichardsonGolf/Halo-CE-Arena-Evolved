/*
AE_FOLLOW_CAMERA_ROOM.C

(Arena Evolved, SPIKE 10-30) The follow camera (follow_camera.c, the co-op spectator's camera) in a tight room.
The owner: 4.5 wu back was too far in the open, and the real trouble is the camera "too zoomed in" in a tight
interior, where the observer pulls it in against the wall behind the unit. So the distance stays the spectator's
3.5, and where the room behind the unit is shorter than that, the camera rises to look down from higher (of a few
pitches, the one with the most room) and widens its view, both eased so it doesn't jump.
*/

#include <math.h>

#include "cseries.h"
#include "camera/dead_camera.h"
#include "objects/objects.h"
#include "physics/collisions.h"

#include "ae_follow_camera_room.h"

void platform_log(char const *format, ...);


/* ---------- constants */

/* the room the camera wants behind the unit (the spectator's distance), and below this much it counts as tight:
fully tight at TIGHT_FULL */
#define ROOM_WANTED 3.5f
#define ROOM_TIGHT 2.6f
#define ROOM_TIGHT_FULL 1.2f
/* the pitches tried (radians, down): the spectator's own, then steeper */
static real const room_pitches[] = { -0.3f, -0.55f, -0.8f };
#define ROOM_PITCH_COUNT (sizeof(room_pitches) / sizeof(room_pitches[0]))
/* the view: the spectator's 70 degrees, up to this when fully tight */
#define ROOM_FIELD_OF_VIEW_OPEN DEGREES_TO_RADIANS(70.0f)
#define ROOM_FIELD_OF_VIEW_TIGHT DEGREES_TO_RADIANS(90.0f)
/* how much of the way to the wanted pitch and view each frame */
#define ROOM_EASE 0.12f
/* the cameras eased (one a local player) */
#define ROOM_CAMERAS 4

/* ---------- globals */

static struct
{
	struct dead_camera *camera;
	real pitch;
	real field_of_view;
	boolean tight;
} room_states[ROOM_CAMERAS];

/* ---------- private code */

/* the room behind the point, looking along yaw and pitch, up to ROOM_WANTED */
static real room_behind(
	real_point3d const *target,
	real yaw,
	real pitch,
	long ignore_object_index)
{
	struct collision_result collision;
	real_vector3d back;

	back.i = -(real)(cos(pitch) * cos(yaw)) * ROOM_WANTED;
	back.j = -(real)(cos(pitch) * sin(yaw)) * ROOM_WANTED;
	back.k = -(real)sin(pitch) * ROOM_WANTED;
	if (collision_test_vector(_collision_test_for_projectiles_flags, target, &back, ignore_object_index, &collision))
		return collision.t * ROOM_WANTED;
	return ROOM_WANTED;
}

/* ---------- public code */

void ae_follow_camera_room(
	struct dead_camera *camera)
{
	struct object_datum *unit = camera->unit_index != NONE ? object_try_and_get(camera->unit_index) : NULL;
	real best_room = -1.0f, best_pitch = room_pitches[0], room_open, tightness, wanted_fov;
	short index, slot = NONE;

	if (!unit)
		return;
	for (index = 0; index < ROOM_CAMERAS; index++)
	{
		if (room_states[index].camera == camera)
			slot = index;
	}
	if (slot == NONE)
	{
		for (index = 0; index < ROOM_CAMERAS && slot == NONE; index++)
		{
			if (!room_states[index].camera)
				slot = index;
		}
		if (slot == NONE)
			slot = 0;
		room_states[slot].camera = camera;
		room_states[slot].pitch = camera->facing.pitch;
		room_states[slot].field_of_view = camera->field_of_view;
		room_states[slot].tight = FALSE;
	}

	room_open = room_behind(&unit->object.bounding_sphere_center, camera->facing.yaw, room_pitches[0], camera->unit_index);
	for (index = 0; index < (short)ROOM_PITCH_COUNT; index++)
	{
		real room = index == 0 ? room_open :
			room_behind(&unit->object.bounding_sphere_center, camera->facing.yaw, room_pitches[index], camera->unit_index);

		/* a steeper pitch only for clearly more room */
		if (room > best_room + 0.25f)
		{
			best_room = room;
			best_pitch = room_pitches[index];
		}
	}
	tightness = room_open >= ROOM_TIGHT ? 0.0f :
		PIN((ROOM_TIGHT - best_room) / (ROOM_TIGHT - ROOM_TIGHT_FULL), 0.0f, 1.0f);
	if (room_open >= ROOM_TIGHT)
		best_pitch = room_pitches[0];
	wanted_fov = ROOM_FIELD_OF_VIEW_OPEN + (ROOM_FIELD_OF_VIEW_TIGHT - ROOM_FIELD_OF_VIEW_OPEN) * tightness;

	room_states[slot].pitch += (best_pitch - room_states[slot].pitch) * ROOM_EASE;
	room_states[slot].field_of_view += (wanted_fov - room_states[slot].field_of_view) * ROOM_EASE;
	camera->facing.pitch = room_states[slot].pitch;
	camera->field_of_view = room_states[slot].field_of_view;

	if ((room_open < ROOM_TIGHT) != room_states[slot].tight)
	{
		room_states[slot].tight = room_open < ROOM_TIGHT;
		platform_log("ae follow camera: %s (room behind %.1f wu; now pitch %.2f, %.0f deg view, %.1f wu room)",
			room_states[slot].tight ? "tight room: looking down from higher" : "open again", (double)room_open,
			(double)best_pitch, (double)(wanted_fov * 57.29578f), (double)best_room);
	}
}
