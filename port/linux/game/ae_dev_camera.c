/*
AE_DEV_CAMERA.C

DEV MODE's cameras (ae_dev_camera.h).

The third-person camera is an over-the-shoulder view, turned on with F5 and
mirrored with F6 (BACK and the d-pad's down and right on a controller). It is built here from the player's eye: the focus sits beside the head
(shoulder) and a little above it, the camera behind it along the aim, and
CE's observer pulls it in where a wall is (observer_check_penetration) and
eases each change. The camera turns to the point the shot would hit, so the
reticle at the screen's centre stays true although the camera is off to the
side.
*/

#include "cseries.h"
#include "camera/director.h"
#include "camera/flying_camera.h"
#include "camera/observer.h"
#include "camera/static_camera.h"
#include "camera/first_person_camera.h"
#include "camera/following_camera.h"
#include "camera/dead_camera.h"
#include "camera/orbiting_camera.h"
#include "camera/bored_camera.h"
#include "camera/camera_scripting.h"
#include "cutscene/cinematics.h"
#include "game/players.h"
#include "input/input.h"
#include "objects/objects.h"
#include "physics/collisions.h"
#include "units/units.h"

#include <string.h>

#include "halo_keyboard.h"
#include "ae_seats.h"
#include "ae_dev_mode.h"
#include "ae_dev_camera.h"

void platform_log(char const *format, ...);
double config_real(char const *name);
char const *config_string(char const *name);
int config_boolean(char const *name);

/* ---------- constants */

/* (world units: 1 is 10 feet) the focus beside the eye, to the right */
#define AE_DEV_CAMERA_SHOULDER 0.16f
/* and above it */
#define AE_DEV_CAMERA_RISE 0.07f
/* the camera behind the focus at system.dev_camera_distance 100 */
#define AE_DEV_CAMERA_DEPTH 0.85f
/* a shoulder held this far from a wall it would go through */
#define AE_DEV_CAMERA_WALL_GAP 0.03f
/* how far the aim is followed for the point to turn to, and the nearest point turned to (a nearer one would swing
the camera hard round a wall the player faces) */
#define AE_DEV_CAMERA_AIM_RANGE 128.f
#define AE_DEV_CAMERA_AIM_NEAREST 1.5f
/* the free camera starts this far behind the view and above it */
#define AE_DEV_CAMERA_FREE_BACK 0.6f
#define AE_DEV_CAMERA_FREE_RISE 0.15f
/* the free camera's speed (world units a second) at full throttle, and up or down */
#define AE_DEV_CAMERA_FREE_SPEED 3.0f
/* the frames a body is had before the free camera starts from the view (the observer places it after the director) */
#define AE_DEV_CAMERA_BODY_FRAMES 3

/* ---------- structures */

/* the head of director.c's camera_control */
struct ae_dev_camera_action
{
	short local_player_index;
};

/* ---------- globals */

static struct
{
	boolean third;
	boolean left_shoulder;
	boolean free;
	/* the flying camera running is this file's, so turning it off gives the view back */
	boolean flying;
	/* this frame's choice holds the view */
	boolean holds;
	/* the third-person camera gave the view back for a while (a zoom, a vehicle, a death): it comes back at once */
	boolean gave_back;
	/* frames this player has had a body (up to BODY_FRAMES) */
	short body_frames;
	/* debug.dev_camera read */
	boolean started;
	/* the keys and chords held last frame (FLAG(AE_DEV_CAMERA_KEY_*)) */
	long held;
	/* the player's own input while the free camera runs (player_control.c, each tick): the turn not yet taken, the
	throttle, and jump and crouch */
	real_euler_angles2d look;
	real_vector2d throttle;
	boolean rise;
	boolean fall;
} dev_camera[MAXIMUM_LOCAL_PLAYERS];

/* ---------- private code */

/* the keys a player holds: the keyboard's (controls.dev_*) for the keyboard's player, a controller's BACK with the
d-pad's down, right or left (BACK and down the camera, BACK and right the shoulder) */
static long dev_camera_held(short local_player_index)
{
	long held = 0;

	if (local_player_index == ae_seats_keyboard_controller())
	{
		unsigned long actions = halo_keyboard_actions(local_player_index);

		if (actions & (1UL << HALO_KEYBOARD_DEV_CAMERA))
			SET_FLAG(held, AE_DEV_CAMERA_KEY_THIRD, TRUE);
		if (actions & (1UL << HALO_KEYBOARD_DEV_SHOULDER))
			SET_FLAG(held, AE_DEV_CAMERA_KEY_SHOULDER, TRUE);
		if (actions & (1UL << HALO_KEYBOARD_DEV_FREE_CAMERA))
			SET_FLAG(held, AE_DEV_CAMERA_KEY_FREE, TRUE);
	}
	if (input_has_gamepad(local_player_index))
	{
		struct gamepad_state const *pad = input_get_gamepad_state(local_player_index);

		if (pad && pad->buttons[_gamepad_binary_button_back])
		{
			if (pad->buttons[_gamepad_binary_button_dpad_down])
				SET_FLAG(held, AE_DEV_CAMERA_KEY_THIRD, TRUE);
			if (pad->buttons[_gamepad_binary_button_dpad_right])
				SET_FLAG(held, AE_DEV_CAMERA_KEY_SHOULDER, TRUE);
			if (pad->buttons[_gamepad_binary_button_dpad_left])
				SET_FLAG(held, AE_DEV_CAMERA_KEY_FREE, TRUE);
		}
	}
	return held;
}


static real dev_camera_distance_scale(void)
{
	real percent = (real)config_real("system.dev_camera_distance");

	return PIN(percent, 25.f, 400.f) / 100.f;
}

/* on foot, alive, not zoomed, no cinematic or scripted camera: where the third-person camera may run */
static boolean dev_camera_third_allowed(short local_player_index)
{
	long unit_index = player_control_get_unit_index(local_player_index);
	struct unit_datum *unit = unit_index == NONE ? NULL : unit_try_and_get(unit_index);

	return unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit) &&
		unit->object.parent_object_index == NONE && player_control_get_zoom_level(local_player_index) == NONE &&
		!cinematic_in_progress() && !(director_camera_scripted && *director_camera_scripted);
}

static void dev_camera_keys(short local_player_index)
{
	long held = dev_camera_held(local_player_index);
	long pressed = held & ~dev_camera[local_player_index].held;

	dev_camera[local_player_index].held = held;
	/* (owner 10-28: holding RB turned a split-screen view's camera on: which input it read, to find out) */
	if (pressed)
	{
		struct gamepad_state const *pad = input_has_gamepad(local_player_index) ?
			input_get_gamepad_state(local_player_index) : NULL;
		unsigned long actions = local_player_index == ae_seats_keyboard_controller() ?
			halo_keyboard_actions(local_player_index) : 0;

		platform_log("dev camera: view %d pressed %lx: keyboard seat %d, keys camera %d shoulder %d free %d; pad %s "
			"back %d d-pad down %d right %d left %d, black %d white %d (the shoulders)", local_player_index, (unsigned long)pressed,
			(int)ae_seats_keyboard_controller(), (int)((actions >> HALO_KEYBOARD_DEV_CAMERA) & 1),
			(int)((actions >> HALO_KEYBOARD_DEV_SHOULDER) & 1), (int)((actions >> HALO_KEYBOARD_DEV_FREE_CAMERA) & 1),
			pad ? "yes" : "none", pad ? (int)pad->buttons[_gamepad_binary_button_back] : 0,
			pad ? (int)pad->buttons[_gamepad_binary_button_dpad_down] : 0,
			pad ? (int)pad->buttons[_gamepad_binary_button_dpad_right] : 0,
			pad ? (int)pad->buttons[_gamepad_binary_button_dpad_left] : 0,
			pad ? (int)pad->buttons[_gamepad_analog_button_black] : 0,
			pad ? (int)pad->buttons[_gamepad_analog_button_white] : 0);
	}
	/* (debug.dev_camera: the first local player's) */
	if (!dev_camera[local_player_index].started && local_player_index == local_player_get_next(NONE))
	{
		char const *start = config_string("debug.dev_camera");

		dev_camera[local_player_index].started = TRUE;
		dev_camera[local_player_index].third = !strncmp(start, "third", 5);
		dev_camera[local_player_index].left_shoulder = !strcmp(start, "third_left");
		dev_camera[local_player_index].free = !strcmp(start, "free");
		if (*start)
			platform_log("dev camera: %s from the start (debug.dev_camera)", start);
	}
	if (TEST_FLAG(pressed, AE_DEV_CAMERA_KEY_THIRD))
	{
		dev_camera[local_player_index].third = !dev_camera[local_player_index].third;
		dev_camera[local_player_index].free = FALSE;
		platform_log("dev camera: third person %s (%s shoulder, distance %.0f%%)",
			dev_camera[local_player_index].third ? "on" : "off",
			dev_camera[local_player_index].left_shoulder ? "left" : "right", dev_camera_distance_scale() * 100.f);
	}
	if (TEST_FLAG(pressed, AE_DEV_CAMERA_KEY_SHOULDER))
	{
		dev_camera[local_player_index].left_shoulder = !dev_camera[local_player_index].left_shoulder;
		platform_log("dev camera: %s shoulder", dev_camera[local_player_index].left_shoulder ? "left" : "right");
	}
	if (TEST_FLAG(pressed, AE_DEV_CAMERA_KEY_FREE))
	{
		dev_camera[local_player_index].free = !dev_camera[local_player_index].free;
		platform_log("dev camera: free camera %s", dev_camera[local_player_index].free ? "on" : "off");
	}
}

/* ---------- public code */

void ae_dev_camera_note_input(short local_player_index, float forward, float left, float yaw, float pitch, int rise,
	int fall)
{
	if (!ae_dev_camera_free(local_player_index))
		return;
	dev_camera[local_player_index].look.yaw += yaw;
	dev_camera[local_player_index].look.pitch += pitch;
	set_real_vector2d(&dev_camera[local_player_index].throttle, forward, left);
	dev_camera[local_player_index].rise = rise != 0;
	dev_camera[local_player_index].fall = fall != 0;
}

int ae_dev_camera_free_controls(short local_player_index, float *yaw, float *pitch, real_vector3d *translation,
	float seconds)
{
	real step = AE_DEV_CAMERA_FREE_SPEED * seconds;

	if (!ae_dev_camera_free(local_player_index))
		return FALSE;
	*yaw = dev_camera[local_player_index].look.yaw;
	*pitch = dev_camera[local_player_index].look.pitch;
	dev_camera[local_player_index].look.yaw = 0.f;
	dev_camera[local_player_index].look.pitch = 0.f;
	set_real_vector3d(translation, dev_camera[local_player_index].throttle.i * step,
		dev_camera[local_player_index].throttle.j * step,
		((real)dev_camera[local_player_index].rise - (real)dev_camera[local_player_index].fall) * step);
	return TRUE;
}

int ae_dev_camera_choose(short local_player_index, void *camera_proc, void *camera_data)
{
	boolean ours_third = camera_proc == (void *)ae_dev_camera_update;
	boolean ours_flying = camera_proc == (void *)flying_camera_update && dev_camera[local_player_index].flying;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return AE_DEV_CAMERA_KEEP;
	/* (owner 10-28: holding RB put a split-screen view in a camera, with no dev camera key read: each view's camera
	in the log when it changes, to name the one) */
	{
		static void *logged[MAXIMUM_LOCAL_PLAYERS];

		if (camera_proc != logged[local_player_index])
		{
			logged[local_player_index] = camera_proc;
			platform_log("dev camera: view %d's camera is now %s", local_player_index,
				camera_proc == (void *)first_person_camera_update ? "first person" :
				camera_proc == (void *)following_camera_update ? "following (third person, a seat's)" :
				camera_proc == (void *)dead_camera_update ? "dead" :
				camera_proc == (void *)orbiting_camera_update ? "orbiting" :
				camera_proc == (void *)bored_camera_update ? "bored" :
				camera_proc == (void *)flying_camera_update ? "flying" :
				camera_proc == (void *)static_camera_update ? "static" :
				camera_proc == (void *)scripted_camera_update ? "scripted" :
				camera_proc == (void *)ae_dev_camera_update ? "the dev camera" : "another");
		}
	}
	dev_camera[local_player_index].holds = FALSE;
	if (!ae_dev_mode_on())
	{
		dev_camera[local_player_index].third = FALSE;
		dev_camera[local_player_index].free = FALSE;
	}
	else
	{
		dev_camera_keys(local_player_index);
	}

	/* (started only from a view in the map: once the observer has placed the view at a body, a few frames after it
	is first had, not the pregame's) */
	if (player_control_get_unit_index(local_player_index) == NONE)
		dev_camera[local_player_index].body_frames = 0;
	else if (dev_camera[local_player_index].body_frames < AE_DEV_CAMERA_BODY_FRAMES)
		dev_camera[local_player_index].body_frames++;
	if (dev_camera[local_player_index].free && !cinematic_in_progress() &&
		(ours_flying || dev_camera[local_player_index].body_frames >= AE_DEV_CAMERA_BODY_FRAMES))
	{
		dev_camera[local_player_index].holds = TRUE;
		if (ours_flying)
			return AE_DEV_CAMERA_KEEP;
		{
			/* from where the view is now, stepped back and up out of the head, so the body is seen */
			struct observer_result const *view = observer_get_camera(local_player_index);
			real_point3d start;

			point_from_line3d(&view->position, &view->forward, -AE_DEV_CAMERA_FREE_BACK, &start);
			start.z += AE_DEV_CAMERA_FREE_RISE;
			flying_camera_new_from_point_and_vector((struct flying_camera *)camera_data, &start, &view->forward);
			platform_log("dev camera: the free camera starts at %.2f %.2f %.2f", start.x, start.y, start.z);
		}
		dev_camera[local_player_index].flying = TRUE;
		return AE_DEV_CAMERA_FREE;
	}
	if (dev_camera[local_player_index].third && dev_camera_third_allowed(local_player_index))
	{
		dev_camera[local_player_index].holds = TRUE;
		dev_camera[local_player_index].flying = FALSE;
		if (ours_third)
			return AE_DEV_CAMERA_KEEP;
		platform_log("dev camera: view %d takes the third-person camera%s", local_player_index,
			dev_camera[local_player_index].gave_back ? " again" : "");
		if (dev_camera[local_player_index].gave_back)
		{
			dev_camera[local_player_index].gave_back = FALSE;
			return AE_DEV_CAMERA_THIRD_SNAP;
		}
		return AE_DEV_CAMERA_THIRD;
	}
	dev_camera[local_player_index].flying = FALSE;
	if (!ours_third && !ours_flying)
		return AE_DEV_CAMERA_KEEP;
	/* (the shoulder camera still wanted: back once it may be) */
	dev_camera[local_player_index].gave_back = dev_camera[local_player_index].third;
	platform_log("dev camera: view %d gives the game its camera back (zoom %d)", local_player_index,
		player_control_get_zoom_level(local_player_index));
	return AE_DEV_CAMERA_GIVE_BACK;
}

int ae_dev_camera_holds(short local_player_index)
{
	return local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS &&
		dev_camera[local_player_index].holds;
}

int ae_dev_camera_free(short local_player_index)
{
	return ae_dev_camera_holds(local_player_index) && dev_camera[local_player_index].free;
}

int ae_dev_camera_black_switch(void)
{
	return config_boolean("debug.camera_black_switch");
}

int ae_dev_camera_is_third(void *camera_proc)
{
	return camera_proc == (void *)ae_dev_camera_update;
}

void ae_dev_camera_update(void *camera, void *action, void *result)
{
	short local_player_index = ((struct ae_dev_camera_action const *)action)->local_player_index;
	struct camera_command *command = (struct camera_command *)result;
	long unit_index = player_control_get_unit_index(local_player_index);
	real_point3d eye, shoulder, target, view;
	real_vector3d facing, right, vector;
	struct collision_result collision;
	real side, depth, reach, lift;

	(void)camera;
	command->flags = 0;
	if (unit_index == NONE)
		return;
	unit_get_camera_position(unit_index, &eye);
	player_control_get_facing_direction(local_player_index, &facing);
	depth = AE_DEV_CAMERA_DEPTH * dev_camera_distance_scale();
	/* the shoulder's side and rise grow with a distance past 100 (a nearer camera keeps them): the body keeps the screen place
	it has at 100, to the side and low, off the reticle at the centre (it was on it at 300 and 400, the angle from the aim
	to the shoulder shrinking as 1 / depth) */
	lift = MAX(dev_camera_distance_scale(), 1.f);

	/* the shoulder: beside the eye across the aim's heading (observer.c's offset frame: j to the right), pulled in
	before a wall it would go through */
	side = (dev_camera[local_player_index].left_shoulder ? -AE_DEV_CAMERA_SHOULDER : AE_DEV_CAMERA_SHOULDER) * lift;
	set_real_vector3d(&right, facing.j, -facing.i, 0.f);
	if (normalize3d(&right) == 0.f)
		set_real_vector3d(&right, 1.f, 0.f, 0.f);
	set_real_vector3d(&vector, right.i * side, right.j * side, AE_DEV_CAMERA_RISE * lift);
	if (collision_test_vector(_collision_test_for_projectiles_flags, &eye, &vector, unit_index, &collision))
	{
		real t = MAX(0.f, collision.t - AE_DEV_CAMERA_WALL_GAP / magnitude3d(&vector));

		side *= t;
		scale_vector3d(&vector, t, &vector);
	}
	point_from_line3d(&eye, &vector, 1.f, &shoulder);

	/* the point the shot would hit (no nearer than AIM_NEAREST), and the camera turned to it from about where it sits */
	scale_vector3d(&facing, AE_DEV_CAMERA_AIM_RANGE, &vector);
	reach = AE_DEV_CAMERA_AIM_RANGE;
	if (collision_test_vector(_collision_test_for_projectiles_flags, &eye, &vector, unit_index, &collision))
		reach = collision.t * AE_DEV_CAMERA_AIM_RANGE;
	point_from_line3d(&eye, &facing, MAX(reach, AE_DEV_CAMERA_AIM_NEAREST), &target);
	point_from_line3d(&shoulder, &facing, -depth, &view);
	vector_from_points3d(&view, &target, &command->forward);
	if (normalize3d(&command->forward) == 0.f)
		command->forward = facing;

	command->position = eye;
	set_real_vector3d(&command->offset, 0.f, side, shoulder.z - eye.z);
	command->depth = depth;
	command->field_of_view = player_control_get_field_of_view(local_player_index);
	observer_up_from_forward(&command->forward, &command->up);
	object_get_velocities(unit_index, &command->velocity, NULL);
	command->timer = 0.f;
	command->flags = FLAG(0);
}
