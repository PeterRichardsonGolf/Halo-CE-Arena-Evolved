/*
AE_FOLLOW_CAMERA_ROOM.C

(Arena Evolved, SPIKE 10-30) The follow camera (follow_camera.c, the co-op spectator's camera) in a tight room.
The owner: 4.5 wu back was too far in the open, and the real trouble is the camera "too zoomed in" in a tight
interior, where the observer pulls it in against the wall behind the unit. So the distance stays the spectator's
3.5, and where the room behind the unit is shorter than that, the camera rises to look down from higher (of a few
pitches, the one with the most room) and widens its view, both eased so it doesn't jump. (0.8.1: the follow camera
looks along the followed player's aim, ae_follow_camera.c, and eases what this asks for at any frame rate.)
*/

#include <math.h>

#include "cseries.h"
#include "objects/objects.h"
#include "physics/collisions.h"

#include "ae_follow_camera_room.h"
#include "ae_follow_camera_rules.h"

void platform_log(char const *format, ...);


/* ---------- constants */

/* the room the camera wants behind the unit (the follow camera's distance, ae_follow_camera_rules.h: 2.2 since 11-11),
and below this much it counts as tight: fully tight at TIGHT_FULL (the shares of 3.0's 2.6 and 1.2) */
#define ROOM_WANTED AE_FOLLOW_DISTANCE
#define ROOM_TIGHT (AE_FOLLOW_DISTANCE * 0.87f)
#define ROOM_TIGHT_FULL (AE_FOLLOW_DISTANCE * 0.4f)
/* the pitches tried (radians, down): the spectator's own, then steeper */
static real const room_pitches[] = { -0.3f, -0.55f, -0.8f };
#define ROOM_PITCH_COUNT (sizeof(room_pitches) / sizeof(room_pitches[0]))
/* the view: the spectator's 70 degrees, up to this when fully tight */
#define ROOM_FIELD_OF_VIEW_OPEN DEGREES_TO_RADIANS(70.0f)
#define ROOM_FIELD_OF_VIEW_TIGHT DEGREES_TO_RADIANS(90.0f)
/* the local players (whose log line says when their room turns tight or open) */
#define ROOM_PLAYERS 4

/* ---------- globals */

static boolean room_tight[ROOM_PLAYERS];

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
	short local_player_index,
	long unit_index,
	real yaw,
	real *pitch,
	real *field_of_view)
{
	struct object_datum *unit = unit_index != NONE ? object_try_and_get(unit_index) : NULL;
	real best_room = -1.0f, best_pitch = room_pitches[0], room_open, tightness;
	short index;

	if (!unit)
		return;
	room_open = room_behind(&unit->object.bounding_sphere_center, yaw, room_pitches[0], unit_index);
	for (index = 0; index < (short)ROOM_PITCH_COUNT; index++)
	{
		real room = index == 0 ? room_open :
			room_behind(&unit->object.bounding_sphere_center, yaw, room_pitches[index], unit_index);

		/* a steeper pitch only for clearly more room */
		if (room > best_room + 0.25f)
		{
			best_room = room;
			best_pitch = room_pitches[index];
		}
	}
	tightness = room_open >= ROOM_TIGHT ? 0.0f :
		PIN((ROOM_TIGHT - best_room) / (ROOM_TIGHT - ROOM_TIGHT_FULL), 0.0f, 1.0f);
	if (room_open < ROOM_TIGHT && best_pitch < *pitch)
		*pitch = best_pitch;
	*field_of_view = ROOM_FIELD_OF_VIEW_OPEN + (ROOM_FIELD_OF_VIEW_TIGHT - ROOM_FIELD_OF_VIEW_OPEN) * tightness;

	if (local_player_index >= 0 && local_player_index < ROOM_PLAYERS && (room_open < ROOM_TIGHT) != room_tight[local_player_index])
	{
		room_tight[local_player_index] = room_open < ROOM_TIGHT;
		platform_log("ae follow camera: %s (room behind %.1f wu; now pitch %.2f, %.0f deg view, %.1f wu room)",
			room_tight[local_player_index] ? "tight room: looking down from higher" : "open again", (double)room_open,
			(double)*pitch, (double)(*field_of_view * 57.29578f), (double)best_room);
	}
}
