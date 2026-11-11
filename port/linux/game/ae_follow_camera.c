/*
AE_FOLLOW_CAMERA.C

(Arena Evolved, 0.8.1, owner 2026-11-11: the follow camera's turning lagged and snapped, and every change of view a hard cut) The follow camera of a dead player (follow_camera.c) and
of network co-op's spectator (coop_spectate.c) sits behind the player it follows and looks where they aim: its yaw
takes their aim's, its pitch a share of their aim's (ae_follow_pitch), each eased by a half-life, so it turns with
them smoothly at any frame rate rather than swinging round behind their body late. The point it looks at follows them
eased too, slower up and down, so jumps, crouches and landings don't bob it. Where the room behind them is tight it
looks down from higher with a wider view (ae_follow_camera_room.c), eased the same way.

When it begins following (from the death camera on the body, or from the player's own view) or changes to another
player, it glides from where the view was to its place behind them in AE_FOLLOW_GLIDE_SECONDS, on a minimum-jerk
curve, instead of cutting there: the dead camera's own command would put it at the new player at once, half a world
unit away, and pull out from there.

The command it writes is whole and forced (every parameter's time 0), so the observer takes it as it is, and the
director's pause on a change of camera adds no second easing; the observer still keeps it out of walls.

Played the same day, and set after play-testing (owner: "Look at the killer, Start at the
helmet, Right stick orbits body"; a switch cuts with a quick fade in):
- THE DEATH CAMERA of a multiplayer game (the dead camera on the player's own body) is AE's too: it starts at the
  player's eyes and eases out in and out over a second to its place behind the body, looking past the body at the killer
  while the killer is in sight (the teabag in view), drifting slowly with no killer; any look input from then on orbits
  it round the body. It lasts until the respawn, or until follow_camera.c moves on (ae_follow_leave_body).
- A SWITCH (from the death camera to following, or to another player) cuts, and the view fades in from black over
  AE_FOLLOW_FADE_SECONDS (the screen flash of that local player's view: ae_follow_camera_screen_flash). It glides only
  from one followed player to another near it and in its sight.
*/

#include "cseries.h"
#include "camera/dead_camera.h"
#include "camera/observer.h"
#include "game/game.h"
#include "game/players.h"
#include "input/input.h"
#include "game/game_engine.h"
#include "objects/objects.h"
#include "physics/collisions.h"
#include "render/render_cameras.h"
#include "units/units.h"

#include <math.h>
#include <string.h>

#include "ae_follow_camera.h"
#include "ae_follow_camera_room.h"
#include "ae_follow_camera_rules.h"
#include "coop_spectate.h"
#include "halo_port_limits.h"

/* ---------- constants */

/* the cameras followed with (one a local player) */
#define FOLLOW_CAMERAS 4

/* what a camera's last command was */
enum
{
	FOLLOW_MODE_NONE = 0,
	FOLLOW_MODE_FOLLOW,
	FOLLOW_MODE_DEATH,
};

/* player_control.c: each local player's look input this frame (the sticks, the mouse, with their settings) */
extern real_euler_angles2d player_control_port_debug_facing[];

void platform_log(char const *format, ...);

/* input_abstraction.c: how many ticks the keyboard's jump key has been held */
byte input_abstraction_port_accept(short controller_index);

/* port_config.c */
char const *config_string(char const *name);

/* ---------- globals */

/* A (or jump) held last frame, per controller */
static boolean follow_a_held[MAXIMUM_GAMEPADS];

/* who last killed each player (by absolute index), or NONE (follow_camera_player_killed) */
static long follow_killers[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
/* (owner 11-11) when each was recorded (game ticks), so the death camera takes this death's killer, not the last one's */
static long follow_killed_at[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
static boolean follow_killers_cleared = FALSE;

/* each local player's view: the seconds of its fade in from black still to run */
static real follow_fades[4];

static struct
{
	struct dead_camera *camera;
	/* ae_follow_camera_track called since the last command */
	boolean tracked;
	/* what the last command was (FOLLOW_MODE_...; NONE: not this module's, the next starts afresh) */
	short mode;
	/* the unit followed */
	long unit_index;
	/* where the camera eases to: the point it looks at, its angles (radians), distance and view */
	real_point3d focus;
	real yaw;
	real pitch;
	real distance;
	real field_of_view;
	/* the glide: from these, for the seconds gone so far (done at AE_FOLLOW_GLIDE_SECONDS) */
	real_point3d from_focus;
	real from_yaw;
	real from_pitch;
	real from_distance;
	real from_field_of_view;
	real glide_seconds;
	/* the death camera: the seconds since it began, whether the look input took it over, and the killer (NONE) */
	real death_seconds;
	boolean orbiting;
	long killer;
	long death_started_at;
} follow_states[FOLLOW_CAMERAS];

/* ---------- private code */

static short follow_slot(
	struct dead_camera *camera,
	boolean add)
{
	short index;

	for (index = 0; index < FOLLOW_CAMERAS; index++)
	{
		if (follow_states[index].camera == camera)
			return index;
	}
	if (!add)
		return NONE;
	for (index = 0; index < FOLLOW_CAMERAS; index++)
	{
		if (!follow_states[index].camera)
		{
			follow_states[index].camera = camera;
			follow_states[index].mode = FOLLOW_MODE_NONE;
			return index;
		}
	}
	follow_states[0].camera = camera;
	follow_states[0].mode = FOLLOW_MODE_NONE;
	return 0;
}

/* the followed unit's aim (yaw and pitch), or its facing if it has no aim */
static void follow_aim(
	struct object_datum *unit,
	long unit_index,
	real *yaw,
	real *pitch)
{
	real_vector3d aim = unit_get(unit_index)->unit.aiming_vector;
	real across = (real)sqrt(aim.i * aim.i + aim.j * aim.j);

	if (across < 0.01f)
	{
		*yaw = (real)atan2(unit->object.forward.j, unit->object.forward.i);
		*pitch = 0.0f;
		return;
	}
	*yaw = (real)atan2(aim.j, aim.i);
	*pitch = (real)atan2(aim.k, across);
}

/* the yaw and pitch the view looks along */
static void follow_angles_of(
	real_vector3d const *forward,
	real *yaw,
	real *pitch)
{
	real across = (real)sqrt(forward->i * forward->i + forward->j * forward->j);

	*yaw = (real)atan2(forward->j, forward->i);
	*pitch = (real)atan2(forward->k, across);
}

/* the glide's start: where the local player's view is now, looking at the point it was looking at */
static void follow_glide_from_view(
	short slot,
	short local_player_index)
{
	struct observer_result const *view = observer_get_camera(local_player_index);
	real_point3d const *focus = observer_get_focus_position(local_player_index);
	real dx = focus->x - view->position.x, dy = focus->y - view->position.y, dz = focus->z - view->position.z;

	follow_states[slot].from_focus = *focus;
	follow_angles_of(&view->forward, &follow_states[slot].from_yaw, &follow_states[slot].from_pitch);
	follow_states[slot].from_distance = (real)sqrt(dx * dx + dy * dy + dz * dz);
	follow_states[slot].from_field_of_view = view->field_of_view;
	follow_states[slot].glide_seconds = 0.0f;
}

/* whether there is a clear line from one point to another (the structure only) */
static boolean follow_in_sight(
	real_point3d const *from,
	real_point3d const *to,
	long ignore_object_index)
{
	struct collision_result collision;
	real_vector3d way;

	way.i = to->x - from->x;
	way.j = to->y - from->y;
	way.k = to->z - from->z;
	return !collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
		from, &way, ignore_object_index, &collision);
}

/* the shorter-way blend of two angles */
static real follow_angle_lerp(
	real from,
	real to,
	real t)
{
	real turn = to - from;

	while (turn > _pi)
		turn -= 2.0f * _pi;
	while (turn < -_pi)
		turn += 2.0f * _pi;
	return from + turn * t;
}

/* the command: from the glide's start to where the camera eases, `t` of the way; whole and forced */
static void follow_write_command(
	short slot,
	real t,
	struct observer_command *command)
{
	real_euler_angles2d facing;
	short parameter;

	command->flags = FLAG(_observer_command_valid_bit);
	command->focus_position.x = follow_states[slot].from_focus.x + (follow_states[slot].focus.x - follow_states[slot].from_focus.x) * t;
	command->focus_position.y = follow_states[slot].from_focus.y + (follow_states[slot].focus.y - follow_states[slot].from_focus.y) * t;
	command->focus_position.z = follow_states[slot].from_focus.z + (follow_states[slot].focus.z - follow_states[slot].from_focus.z) * t;
	command->focus_offset = *global_zero_vector3d;
	command->focus_distance = follow_states[slot].from_distance +
		(follow_states[slot].distance - follow_states[slot].from_distance) * t;
	command->field_of_view = follow_states[slot].from_field_of_view +
		(follow_states[slot].field_of_view - follow_states[slot].from_field_of_view) * t;
	facing.yaw = follow_angle_lerp(follow_states[slot].from_yaw, follow_states[slot].yaw, t);
	facing.pitch = follow_states[slot].from_pitch + (follow_states[slot].pitch - follow_states[slot].from_pitch) * t;
	vector3d_from_euler_angles2d(&command->forward, &facing);
	observer_up_from_forward(&command->forward, &command->up);
	command->focus_velocity = *global_zero_vector3d;
	command->timer = 0.0f;
	for (parameter = 0; parameter < NUMBER_OF_OBSERVER_COMMAND_PARAMETERS; parameter++)
	{
		command->parameter_flags[parameter] = FLAG(_observer_time_valid_bit) | FLAG(_observer_time_force_bit);
		command->parameter_timers[parameter] = 0.0f;
	}
}

/* a cut: the glide done at once, and the local player's view fading in from black */
static void follow_cut(
	short slot,
	short local_player_index)
{
	follow_states[slot].from_focus = follow_states[slot].focus;
	follow_states[slot].from_yaw = follow_states[slot].yaw;
	follow_states[slot].from_pitch = follow_states[slot].pitch;
	follow_states[slot].from_distance = follow_states[slot].distance;
	follow_states[slot].from_field_of_view = follow_states[slot].field_of_view;
	follow_states[slot].glide_seconds = AE_FOLLOW_GLIDE_SECONDS;
	if (local_player_index >= 0 && local_player_index < (short)NUMBEROF(follow_fades))
		follow_fades[local_player_index] = AE_FOLLOW_FADE_SECONDS;
}

/* the killer's unit, alive, or NONE */
static long follow_killer_unit(
	long killer)
{
	struct player_datum *player = killer != NONE ? player_try_and_get(killer) : NULL;
	struct object_datum *object;

	if (!player || player->unit_index == NONE)
		return NONE;
	object = object_try_and_get(player->unit_index);
	return object && !TEST_FLAG(object->object.damage_flags, _object_dead_bit) ? player->unit_index : NONE;
}

/* THE DEATH CAMERA (the player's own body, a multiplayer game): see the file's comment */
static void follow_death_command(
	short slot,
	struct dead_camera *camera,
	struct player_datum *player,
	struct object_datum *body,
	real seconds,
	struct observer_command *command)
{
	short local_player_index = player->local_player_index;
	real_point3d target = body->object.bounding_sphere_center;
	long killer_unit;
	real_euler_angles2d look = player_control_port_debug_facing[local_player_index];

	if (follow_states[slot].mode != FOLLOW_MODE_DEATH || follow_states[slot].unit_index != camera->unit_index)
	{
		long absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(camera->player_index);

		/* (from the player's own eyes, where the view was a frame ago) */
		follow_glide_from_view(slot, local_player_index);
		follow_states[slot].unit_index = camera->unit_index;
		follow_states[slot].mode = FOLLOW_MODE_DEATH;
		follow_states[slot].death_seconds = 0.0f;
		follow_states[slot].orbiting = FALSE;
		/* (the killer is read each frame below: the kill can be told a tick after the camera starts) */
		follow_states[slot].killer = NONE;
		follow_states[slot].death_started_at = game_time_get();
		follow_states[slot].focus = target;
		follow_states[slot].yaw = follow_states[slot].from_yaw;
		follow_states[slot].pitch = AE_DEATH_PITCH;
		follow_states[slot].distance = AE_DEATH_DISTANCE;
		follow_states[slot].field_of_view = DEGREES_TO_RADIANS(AE_FOLLOW_FIELD_OF_VIEW_DEGREES);
	}
	/* (owner 11-11: this death's killer only, none for one's own kill; the gaze below eases to it, no snap) */
	if (follow_states[slot].killer == NONE && follow_killers_cleared)
	{
		long absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(camera->player_index);

		if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS &&
			ae_death_killer_is_this_death(follow_killers[absolute] != NONE ? follow_killed_at[absolute] : -1,
				follow_states[slot].death_started_at, TICKS_PER_SECOND))
		{
			follow_states[slot].killer = ae_death_killer_to_watch(follow_killers[absolute], camera->player_index);
		}
	}
	follow_states[slot].death_seconds += seconds;
	follow_states[slot].focus.x = ae_follow_smooth(follow_states[slot].focus.x, target.x, AE_FOLLOW_FOCUS_RISE_HALF_LIFE, seconds);
	follow_states[slot].focus.y = ae_follow_smooth(follow_states[slot].focus.y, target.y, AE_FOLLOW_FOCUS_RISE_HALF_LIFE, seconds);
	follow_states[slot].focus.z = ae_follow_smooth(follow_states[slot].focus.z, target.z, AE_FOLLOW_FOCUS_RISE_HALF_LIFE, seconds);
	/* (the look input, once the ease is under way, takes it over for good) */
	if (!follow_states[slot].orbiting && follow_states[slot].death_seconds > 0.3f &&
		(real)fabs(look.yaw) + (real)fabs(look.pitch) > 0.0005f)
	{
		follow_states[slot].orbiting = TRUE;
	}
	if (follow_states[slot].orbiting)
	{
		follow_states[slot].yaw += look.yaw;
		follow_states[slot].pitch = PIN(follow_states[slot].pitch + look.pitch, AE_DEATH_PITCH_LOWEST, AE_DEATH_PITCH_HIGHEST);
	}
	else if (follow_states[slot].killer != NONE && follow_states[slot].killer != camera->player_index)
	{
		/* (past the body at the killer, while in sight) */
		killer_unit = follow_killer_unit(follow_states[slot].killer);
		if (killer_unit != NONE)
		{
			real_point3d const *at = &object_get(killer_unit)->object.bounding_sphere_center;
			real dx = at->x - target.x, dy = at->y - target.y;

			if (follow_in_sight(&target, at, camera->unit_index))
			{
				follow_states[slot].yaw = ae_follow_smooth_angle(follow_states[slot].yaw, (real)atan2(dy, dx),
					AE_DEATH_GAZE_HALF_LIFE, seconds);
				follow_states[slot].pitch = ae_follow_smooth(follow_states[slot].pitch,
					ae_death_pitch(at->z - target.z, (real)sqrt(dx * dx + dy * dy)), AE_DEATH_GAZE_HALF_LIFE, seconds);
			}
		}
	}
	else
		follow_states[slot].yaw += DEGREES_TO_RADIANS(AE_DEATH_DRIFT_DEGREES) * seconds;
	follow_write_command(slot, ae_follow_ease_in_out(follow_states[slot].death_seconds / AE_DEATH_EASE_SECONDS), command);
}

/* ---------- public code */

int ae_follow_camera_leave_body(
	short setting,
	long dead_ticks,
	int lbrb,
	short controller_index,
	int long_wait,
	int no_respawn)
{
	/* per controller: the ticks dead at the last call (a smaller one is a new death), whether every button was seen up
	since that death (a button held from life, or one that reads pressed every frame, never counts), and A held */
	static long last_dead[MAXIMUM_GAMEPADS];
	static boolean armed[MAXIMUM_GAMEPADS];
	struct gamepad_state const *gamepad;
	int a_value = 0, accept = 0, a_down, pressed = 0, leave;

	if (controller_index < 0 || controller_index >= MAXIMUM_GAMEPADS)
		return ae_follow_leave_body(setting, (float)dead_ticks / (float)TICKS_PER_SECOND, 0, long_wait != 0, no_respawn != 0);
	gamepad = input_get_gamepad_state(controller_index);
	a_value = gamepad ? gamepad->buttons[FIRST_GAMEPAD_ANALOG_BUTTON + _gamepad_analog_button_a] : 0;
	accept = input_abstraction_port_accept(controller_index);
	a_down = a_value > 0 || accept > 0;
	if (dead_ticks < last_dead[controller_index])
		armed[controller_index] = FALSE;
	last_dead[controller_index] = dead_ticks;
	if (!armed[controller_index])
	{
		if (!a_down && !lbrb)
			armed[controller_index] = TRUE;
	}
	else
		pressed = lbrb != 0 || (a_down && !follow_a_held[controller_index]);
	follow_a_held[controller_index] = (boolean)a_down;
	leave = ae_follow_leave_body(setting, (float)dead_ticks / (float)TICKS_PER_SECOND, pressed, long_wait != 0,
		no_respawn != 0);
	if (leave)
		platform_log("ae follow camera: leaves the body at %.1f s dead (%s: lb/rb %d, a %d, jump key %d, setting %d, long wait %d)",
			(double)dead_ticks / TICKS_PER_SECOND, pressed ? "a press" : "time", lbrb, a_value, accept, (int)setting, long_wait);
	return leave;
}

void ae_follow_camera_killed(
	long killing_player_index,
	long dead_player_index)
{
	long absolute;

	if (!follow_killers_cleared)
	{
		for (absolute = 0; absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; absolute++)
		{
			follow_killers[absolute] = NONE;
			follow_killed_at[absolute] = -1;
		}
		follow_killers_cleared = TRUE;
	}
	if (dead_player_index == NONE)
		return;
	absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(dead_player_index);
	if (absolute >= 0 && absolute < HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
	{
		follow_killers[absolute] = killing_player_index;
		follow_killed_at[absolute] = game_time_get();
	}
}

void ae_follow_camera_screen_flash(
	short local_player_index,
	struct render_screen_flash *flash,
	short darken_type)
{
	real intensity;
	long player_index;
	struct player_datum *player;

	if (local_player_index < 0 || local_player_index >= (short)NUMBEROF(follow_fades) || follow_fades[local_player_index] <= 0.0f)
		return;
	/* (a cut just before the respawn: its fade ends with the death, not a life under black) */
	player_index = local_player_get_player_index(local_player_index);
	player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	if (!player || player->unit_index != NONE)
	{
		follow_fades[local_player_index] = 0.0f;
		return;
	}
	intensity = follow_fades[local_player_index] / AE_FOLLOW_FADE_SECONDS;
	if (flash->type != 0 && flash->intensity >= intensity)
		return;
	flash->type = darken_type;
	flash->color.alpha = 1.0f;
	flash->color.rgb.red = 0.0f;
	flash->color.rgb.green = 0.0f;
	flash->color.rgb.blue = 0.0f;
	flash->intensity = PIN(intensity, 0.0f, 1.0f);
}

/* display.follow_camera "off": the original death camera only (no death camera of AE's, the stock switch as it was) */
static boolean follow_setting_off(
	void)
{
	char const *value = config_string("display.follow_camera");

	return value && !strcmp(value, "off");
}

int ae_follow_camera_holds_body(
	struct dead_camera const *camera)
{
	struct player_datum *player = camera->player_index != NONE ? player_try_and_get(camera->player_index) : NULL;

	/* (the same test as the death camera's in ae_follow_camera_command) */
	return !follow_setting_off() && !game_engine_showing_postgame() && player && player->local_player_index != NONE &&
		camera->unit_index != NONE &&
		camera->unit_index == player->dead_unit_index && object_try_and_get(camera->unit_index) && game_engine_running() &&
		!coop_spectating();
}

void ae_follow_camera_track(
	struct dead_camera *camera)
{
	follow_states[follow_slot(camera, TRUE)].tracked = TRUE;
}

void ae_follow_camera_command(
	struct dead_camera *camera,
	float seconds,
	struct observer_command *command)
{
	short slot = follow_slot(camera, FALSE);
	struct object_datum *unit;
	struct player_datum *player;
	real_point3d target;
	real aim_yaw, aim_pitch, pitch, field_of_view;
	short local_player_index;

	unit = camera->unit_index != NONE ? object_try_and_get(camera->unit_index) : NULL;
	player = camera->player_index != NONE ? player_try_and_get(camera->player_index) : NULL;
	if (player && player->local_player_index >= 0 && player->local_player_index < (short)NUMBEROF(follow_fades))
		follow_fades[player->local_player_index] = MAX(0.0f, follow_fades[player->local_player_index] - (real)seconds);
	/* (the player's own body in a multiplayer game: the death camera) */
	if ((slot == NONE || !follow_states[slot].tracked) && unit && player && player->local_player_index != NONE &&
		camera->unit_index == player->dead_unit_index && game_engine_running() && !coop_spectating() &&
		!follow_setting_off())
	{
		if (slot == NONE)
			slot = follow_slot(camera, TRUE);
		follow_death_command(slot, camera, player, unit, (real)seconds, command);
		return;
	}
	if (slot == NONE)
		return;
	if (!follow_states[slot].tracked)
	{
		follow_states[slot].mode = FOLLOW_MODE_NONE;
		return;
	}
	follow_states[slot].tracked = FALSE;
	if (!unit || !player || player->local_player_index == NONE)
	{
		follow_states[slot].mode = FOLLOW_MODE_NONE;
		return;
	}
	local_player_index = player->local_player_index;
	target = unit->object.bounding_sphere_center;
	follow_aim(unit, camera->unit_index, &aim_yaw, &aim_pitch);
	pitch = ae_follow_pitch(aim_pitch);
	field_of_view = DEGREES_TO_RADIANS(AE_FOLLOW_FIELD_OF_VIEW_DEGREES);
	ae_follow_camera_room(local_player_index, camera->unit_index, aim_yaw, &pitch, &field_of_view);

	if (follow_states[slot].mode != FOLLOW_MODE_FOLLOW || follow_states[slot].unit_index != camera->unit_index)
	{
		/* a new start: a glide from one followed player to another near it and in its sight; else a cut, fading in */
		boolean glides = follow_states[slot].mode == FOLLOW_MODE_FOLLOW &&
			ae_follow_switch_glides((float)distance3d(&follow_states[slot].focus, &target),
				follow_in_sight(&follow_states[slot].focus, &target, camera->unit_index));

		follow_glide_from_view(slot, local_player_index);
		follow_states[slot].unit_index = camera->unit_index;
		follow_states[slot].focus = target;
		follow_states[slot].yaw = aim_yaw;
		follow_states[slot].pitch = pitch;
		follow_states[slot].distance = AE_FOLLOW_DISTANCE;
		follow_states[slot].field_of_view = field_of_view;
		follow_states[slot].mode = FOLLOW_MODE_FOLLOW;
		if (!glides)
			follow_cut(slot, local_player_index);
	}
	else
	{
		follow_states[slot].focus.x = ae_follow_smooth(follow_states[slot].focus.x, target.x, AE_FOLLOW_FOCUS_HALF_LIFE, seconds);
		follow_states[slot].focus.y = ae_follow_smooth(follow_states[slot].focus.y, target.y, AE_FOLLOW_FOCUS_HALF_LIFE, seconds);
		follow_states[slot].focus.z = ae_follow_smooth(follow_states[slot].focus.z, target.z, AE_FOLLOW_FOCUS_RISE_HALF_LIFE,
			seconds);
		follow_states[slot].yaw = ae_follow_smooth_angle(follow_states[slot].yaw, aim_yaw, AE_FOLLOW_TURN_HALF_LIFE, seconds);
		follow_states[slot].pitch = ae_follow_smooth(follow_states[slot].pitch, pitch, AE_FOLLOW_TURN_HALF_LIFE, seconds);
		follow_states[slot].field_of_view = ae_follow_smooth(follow_states[slot].field_of_view, field_of_view,
			AE_FOLLOW_ROOM_HALF_LIFE, seconds);
		follow_states[slot].glide_seconds += seconds;
	}
	/* (the glide: from where the view was to where the camera eases, all of the way at its end) */
	follow_write_command(slot, ae_follow_glide(follow_states[slot].glide_seconds / AE_FOLLOW_GLIDE_SECONDS), command);
}
