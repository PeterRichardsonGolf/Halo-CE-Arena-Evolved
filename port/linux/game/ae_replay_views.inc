/*
AE_REPLAY_VIEWS.INC

Included in network_replay_playback.c, after its camera helpers: the Theater's BEHIND and CINEMATIC views, Content Evolved's
(network_replay_playback.c there), written as it has them for a replay played locally. Behind is the camera turned round the
player (or the vehicle they ride), flown round them with the right stick and zoomed with the left; cinematic is a tripod that
tracks the player, frames them, widens to show who hurts them, and follows them when they go on (its distance the left stick's).
Nothing here is read from or written to a game's state or the network: it is the view of a replay that plays, from what the
replay's objects are drawn as. The aim of the player is the unit's (Content Evolved takes it from the facings it indexes as a
replay loads, where it has them; this file does not index those: the unit's aim is what its own code falls back to).
*/

/* the camera: how far behind and above what it watches (a unit on foot, or
the vehicle it rides), its pitch, and how much of its turn toward the
watched's facing it makes a frame */
#define PLAYBACK_CAMERA_DISTANCE 3.5f
#define PLAYBACK_CAMERA_VEHICLE_DISTANCE 7.0f
#define PLAYBACK_CAMERA_HEIGHT 0.6f
#define PLAYBACK_CAMERA_VEHICLE_HEIGHT 1.5f
#define PLAYBACK_CAMERA_PITCH -0.3f
#define PLAYBACK_CAMERA_TURN 0.12f
/* (how far from a wall the camera stays) */
#define PLAYBACK_CAMERA_WALL_MARGIN 0.2f
/* behind, flown round the player with the right stick: how fast it turns
and rises (radians a second, the stick all the way), and how high and low
it goes (a little short of straight down on them, a little below level);
and the left stick's zoom (how fast, a second, and how near and far, of
the distance behind) */
#define PLAYBACK_CAMERA_ORBIT_TURN 2.5f
#define PLAYBACK_CAMERA_ORBIT_HIGHEST -1.5f
#define PLAYBACK_CAMERA_ORBIT_LOWEST 0.35f
#define PLAYBACK_CAMERA_ZOOM_RATE 1.2f
#define PLAYBACK_CAMERA_ZOOM_NEAREST 0.4f
#define PLAYBACK_CAMERA_ZOOM_FARTHEST 4.0f
/* the free camera tracking the player, as on a tripod: how long its aim
lags theirs (seconds, the time it takes to make most of a turn toward
them); and, while it has not moved since it was placed, its zoom: the view
narrowed as they go further than this (world units), down to this, so they
stay the size they were, eased over this long (seconds) */
#define PLAYBACK_FREE_TRACK_TIME 0.15f
#define PLAYBACK_FREE_ZOOM_DISTANCE 5.0f
#define PLAYBACK_FREE_NARROWEST DEGREES_TO_RADIANS(8.0f)
#define PLAYBACK_FREE_ZOOM_TIME 0.3f
/* how a view's width is the screen's: the vertical tangent of half of it is
this of the tangent of half the width (main.c's, the observer's width at 4:3,
render_camera_get_adjusted_field_of_view_tangent's 0.85), the horizontal one
that by the window's shape */
#define PLAYBACK_CAMERA_VERTICAL_TANGENT (0.75f * 0.85f)
/* ... the tripod's framing while it tracks: the player on a third-line of the
screen (a third of the way from the middle to the edge) with the room ahead of
where they aim, as much of it as they aim across its view; how long the
framing lags what it wants (seconds) */
#define PLAYBACK_FREE_THIRD (1.0f / 3.0f)
#define PLAYBACK_FREE_FRAMING_TIME 0.4f
/* ... where a tripod is placed: the camera that far from the player kept
clear of what is around it, looked at along the middle of the arm and to this
far each side of it at its end; less of the arm than this, or no sight of the
player from its end, and the next place is tried. A crane (this of the arm's
length across, and this up) and then either side of them */
#define PLAYBACK_FREE_PROBE_RADIUS 0.15f
#define PLAYBACK_FREE_LEAST_ARM 0.5f
#define PLAYBACK_FREE_CRANE_ACROSS 0.45f
#define PLAYBACK_FREE_CRANE_UP 0.95f
/* ... and how high: the ground this far round the player (the arm's length,
in this many directions) looked down on from this far above them, under open
sky. Half of it more than this above their feet (world units) is a valley, and
the camera goes up by what it is more, by at most this */
#define PLAYBACK_FREE_TERRAIN_DIRECTIONS 8
#define PLAYBACK_FREE_TERRAIN_LOOK 3.0f
#define PLAYBACK_FREE_TERRAIN_LEAST 1.0f
#define PLAYBACK_FREE_TERRAIN_MOST 2.0f
/* the cinematic camera: how long the player may be hidden from it (seconds)
before it is placed again, how long they must be in sight for that to start
over, the least a shot lasts (but in the time after they spawn), and how long
after they spawn it is placed at once if they are (seconds); and how far either
side of their middle it looks for them (world units), so that a post is not
what hides them */
#define PLAYBACK_CINEMATIC_HIDDEN_TIME 0.2f
#define PLAYBACK_CINEMATIC_CLEAR_TIME 0.15f
#define PLAYBACK_CINEMATIC_LEAST_SHOT 1.0f
#define PLAYBACK_CINEMATIC_SPAWN_TIME 0.5f
#define PLAYBACK_CINEMATIC_SIGHT_WIDTH 0.2f
/* ... whoever hurt the player, off the screen: brought in for this long after
the last hit (seconds of the game), at this of the way from the middle to the
edge of the screen (the player at that of the other side), the view widened to
this at most; and no more once the player aims this near them (radians) */
#define PLAYBACK_CINEMATIC_THREAT_HOLD 1.25f
#define PLAYBACK_CINEMATIC_THREAT_EDGE 0.8f
#define PLAYBACK_CINEMATIC_THREAT_WIDEST DEGREES_TO_RADIANS(90.0f)
#define PLAYBACK_CINEMATIC_THREAT_FACED DEGREES_TO_RADIANS(30.0f)
/* the cinematic camera moving (playback_cinematic_move). The tether: how far
the player may go before it moves, in the arm's lengths the camera is placed
at; the distance it follows them at, in arms, scaled by the left stick (up
nearer) between NEAREST and FARTHEST of it, as behind's zoom is, and never
nearer than CLOSEST of it as it goes; the spring that makes up the difference
from it, its time; and the most it pulls at once, so that a camera let go of a
corner does not fly. A focus going further and faster than a vehicle goes has
been moved there, and is cut to */
#define PLAYBACK_CINEMATIC_TETHER_ARMS 3.5f
#define PLAYBACK_CINEMATIC_FOLLOW_ARMS (PLAYBACK_CINEMATIC_TETHER_ARMS / 3.0f)
#define PLAYBACK_CINEMATIC_FOLLOW_NEAREST (0.5f / 0.75f)
#define PLAYBACK_CINEMATIC_FOLLOW_FARTHEST (1.0f / 0.75f)
#define PLAYBACK_CINEMATIC_FOLLOW_CLOSEST 0.5f
#define PLAYBACK_CINEMATIC_TETHER_TIME 1.0f
/* ... following: the player's velocity (their focus's going) smoothed on a
spring this slow (seconds), so that a client's recording's corrections do not
shake it and a moment's dash does not move it;
going this fast (units a second) once this far (arms) from where it last stood
still starts it, and slower than this for this long (seconds) stops it, once it
has come to rest (slower than this, units a second) */
#define PLAYBACK_CINEMATIC_VELOCITY_TIME 0.4f
#define PLAYBACK_CINEMATIC_FOLLOW_SPEED 1.0f
#define PLAYBACK_CINEMATIC_FOLLOW_LEAST 0.5f
#define PLAYBACK_CINEMATIC_STILL_SPEED 0.5f
#define PLAYBACK_CINEMATIC_STILL_TIME 0.75f
#define PLAYBACK_CINEMATIC_RESTING 0.1f
/* ... and how long it takes to go as fast as they go, once it starts (seconds) */
#define PLAYBACK_CINEMATIC_FOLLOW_EASE 0.5f
#define PLAYBACK_CINEMATIC_TETHER_MOST_PULL 16.0f
#define PLAYBACK_CINEMATIC_JUMP_LEAST 1.0f
#define PLAYBACK_CINEMATIC_JUMP_SPEED 60.0f
/* ... the arc: going across its view this fast (units a second) or turning
this fast (radians a second) for ORBIT_AFTER seconds starts it, and ORBIT_REST
seconds of neither stops it; it goes to QUARTER either side of behind where they
aim, on its spring's time, not nearer the player than LEAST across */
#define PLAYBACK_CINEMATIC_ORBIT_SPEED 1.0f
#define PLAYBACK_CINEMATIC_ORBIT_TURN 0.5f
#define PLAYBACK_CINEMATIC_ORBIT_AFTER 0.75f
#define PLAYBACK_CINEMATIC_ORBIT_REST 0.5f
#define PLAYBACK_CINEMATIC_ORBIT_QUARTER DEGREES_TO_RADIANS(45.0f)
#define PLAYBACK_CINEMATIC_ORBIT_TIME 1.5f
#define PLAYBACK_CINEMATIC_ORBIT_LEAST 0.5f

/* the player's aim, the unit's (the smooth facing Content Evolved takes where its replay has the player's facings is not
indexed here) */
static void playback_aim(
	long unit_index,
	real_vector3d *aim)
{
	*aim = unit_get(unit_index)->unit.aiming_vector;
}

/* the sticks, as the game's input abstraction has them for controller 0 and the keyboard (right: x, forward: y, the look's
turn to the right and up), each 0 in its dead middle, and the mouse's turn since the last frame (radians, to the left and up) */
static void playback_sticks(
	real *move_x,
	real *move_y,
	real *turn_x,
	real *turn_y,
	real *rise,
	real *mouse_yaw,
	real *mouse_pitch)
{
	struct playback_input const *input = (struct playback_input const *)input_abstraction_get_input_state(0);
	float yaw = 0.0f, pitch = 0.0f;

	*move_x = *move_y = *turn_x = *turn_y = *rise = 0.0f;
	halo_linux_mouse_look((short)ae_seats_keyboard_controller(), &yaw, &pitch);
	*mouse_yaw = yaw;
	*mouse_pitch = pitch;
	if (!input)
		return;
	*move_x = -input->strafe;
	*move_y = input->forward;
	*turn_x = -input->yaw;
	*turn_y = input->pitch;
	*rise = (input->buttons[PLAYBACK_INPUT_JUMP] ? 1.0f : 0.0f) - (input->buttons[PLAYBACK_INPUT_CROUCH] ? 1.0f : 0.0f);
}

static real playback_angle_between(real from, real to);
static void playback_focus(long unit_index, real_point3d *focus);

/* the camera at position looking along forward (made unit length), up
square to it */
static void playback_camera_set(
	real_point3d const *position,
	real_vector3d const *forward)
{
	real_vector3d look = *forward;
	real length = (real)sqrt(look.i * look.i + look.j * look.j + look.k * look.k);
	real along;

	if (length < 0.0001f)
		return;
	look.i /= length;
	look.j /= length;
	look.k /= length;
	csmemset(&playback.camera, 0, sizeof(playback.camera));
	playback.camera.position = *position;
	scenario_location_from_point(&playback.camera.location, &playback.camera.position);
	playback.camera.forward = look;
	/* (up: the world's, less what of it is along forward) */
	along = look.k;
	playback.camera.up.i = -look.i * along;
	playback.camera.up.j = -look.j * along;
	playback.camera.up.k = 1.0f - look.k * along;
	length = (real)sqrt(playback.camera.up.i * playback.camera.up.i + playback.camera.up.j * playback.camera.up.j +
		playback.camera.up.k * playback.camera.up.k);
	if (length < 0.0001f)
	{
		playback.camera.up.i = 1.0f;
		playback.camera.up.j = 0.0f;
		playback.camera.up.k = 0.0f;
	}
	else
	{
		playback.camera.up.i /= length;
		playback.camera.up.j /= length;
		playback.camera.up.k /= length;
	}
	playback.camera.field_of_view = PLAYBACK_FIELD_OF_VIEW;
	playback.camera_valid = TRUE;
}

/* the free camera kept off what it has come too near without moving toward
it, past a corner the look along its move only just missed: looked at each way
as far as the margin, and moved out of what is nearer */
static void playback_free_camera_clear(
	void)
{
	static real_vector3d const directions[6] =
	{
		{{ 1.0f, 0.0f, 0.0f }}, {{ -1.0f, 0.0f, 0.0f }},
		{{ 0.0f, 1.0f, 0.0f }}, {{ 0.0f, -1.0f, 0.0f }},
		{{ 0.0f, 0.0f, 1.0f }}, {{ 0.0f, 0.0f, -1.0f }}
	};
	real_vector3d push = {{ 0.0f, 0.0f, 0.0f }};
	short direction;

	for (direction = 0; direction < NUMBEROF(directions); direction++)
	{
		struct collision_result collision;
		real_vector3d probe;

		probe.i = directions[direction].i * PLAYBACK_FREE_WALL_MARGIN;
		probe.j = directions[direction].j * PLAYBACK_FREE_WALL_MARGIN;
		probe.k = directions[direction].k * PLAYBACK_FREE_WALL_MARGIN;
		if (collision_test_vector(PLAYBACK_FREE_COLLISION_FLAGS, &playback.free_position, &probe, NONE, &collision))
		{
			real out = PLAYBACK_FREE_WALL_MARGIN * (1.0f - collision.t);

			push.i += collision.plane.n.i * out;
			push.j += collision.plane.n.j * out;
			push.k += collision.plane.n.k * out;
		}
	}
	if (push.i * push.i + push.j * push.j + push.k * push.k > 0.005f * 0.005f)
		playback_free_camera_move(&push);
}

/* a value moved toward where it is going on a critically damped spring,
time behind it as it goes at a steady speed: starting and stopping gently and
not going past it, the same whatever the frames' lengths */
static void playback_spring(
	real *value,
	real *speed,
	real target,
	real seconds,
	real time)
{
	real rate = 2.0f / time;
	real offset = *value - target;
	real push = (*speed + rate * offset) * seconds;
	real decay = (real)exp(-rate * seconds);

	*speed = (*speed - rate * push) * decay;
	*value = target + (offset + push) * decay;
}

/* the tangents of half of a view's width, across the screen and up it */
static void playback_view_tangents(
	real field_of_view,
	real *across,
	real *up)
{
	rectangle2d window = render.camera.window_bounds;
	long width = window.x1 - window.x0;
	long height = window.y1 - window.y0;

	*up = PLAYBACK_CAMERA_VERTICAL_TANGENT * (real)tan(field_of_view / 2.0f);
	*across = *up * (width > 0 && height > 0 ? (real)width / (real)height : 16.0f / 9.0f);
}

/* the view's width the tripod's zoom wants, without any threat: while it has
not moved since it was placed and tracks the player, narrowed as they go
further than PLAYBACK_FREE_ZOOM_DISTANCE so they stay the size they were (to
PLAYBACK_FREE_NARROWEST); held while it has not moved but does not track; moved,
the camera's own, and never narrowed again until it is placed anew */
static real playback_free_camera_zoom_wanted(
	void)
{
	real wanted = playback.free_field_of_view;

	if (!playback.free_still)
		wanted = PLAYBACK_FIELD_OF_VIEW;
	else if (playback.free_tracking && playback.free_focus_valid)
	{
		real_vector3d to;
		real distance;

		to.i = playback.free_focus.x - playback.free_position.x;
		to.j = playback.free_focus.y - playback.free_position.y;
		to.k = playback.free_focus.z - playback.free_position.z;
		distance = (real)sqrt(to.i * to.i + to.j * to.j + to.k * to.k);
		wanted = PLAYBACK_FIELD_OF_VIEW;
		if (distance > PLAYBACK_FREE_ZOOM_DISTANCE)
		{
			wanted = 2.0f * (real)atan(tan(PLAYBACK_FIELD_OF_VIEW / 2.0f) * PLAYBACK_FREE_ZOOM_DISTANCE / distance);
			wanted = MAX(wanted, PLAYBACK_FREE_NARROWEST);
		}
	}
	return wanted;
}

/* the free camera's view's width: what its zoom wants, or wider for a threat
(playback_cinematic_threat) */
static void playback_free_camera_zoom(
	real seconds)
{
	real wanted = MAX(playback_free_camera_zoom_wanted(), playback.threat_field_of_view);

	playback_spring(&playback.free_field_of_view, &playback.free_field_of_view_speed, wanted, seconds,
		PLAYBACK_FREE_ZOOM_TIME);
}

/* the host's damage to a unit, as a replay plays it (network_damage.c): whose
it is and where from, for the cinematic camera to show them */
void network_replay_playback_damage(
	long object_index,
	long owner_player_index,
	long owner_object_index,
	real_point3d const *origin)
{
	struct player_datum *player;
	struct object_datum *object;

	if (!playback.playing || playback.view != _playback_view_cinematic || playback_watched() == NONE)
		return;
	player = player_try_and_get(playback_watched());
	if (!player || player->unit_index == NONE)
		return;
	object = object_get(player->unit_index);
	if (object_index != player->unit_index && object_index != object->object.parent_object_index)
		return;
	/* (their own doing, and what is nobody's: a fall) */
	if (owner_player_index == playback_watched() || owner_object_index == player->unit_index ||
		owner_object_index == object->object.parent_object_index ||
		(owner_player_index == NONE && owner_object_index == NONE))
	{
		return;
	}
	playback.threat_tick = game_time_get();
	playback.threat_watched = playback_watched();
	playback.threat_player = owner_player_index;
	playback.threat_object = owner_object_index;
	playback.threat_point = *origin;
}

/* whoever hurt the player, while it is on the cinematic camera to show them:
wanted_yaw and wanted_pitch become where its aim goes from the player's
(focus_yaw, focus_pitch) to have both on the screen, the view widened as it
must be (playback.threat_field_of_view); FALSE if nothing needs to change: no
one, or longer ago than PLAYBACK_CINEMATIC_THREAT_HOLD (in the game's time, so a jump
back in the replay is no hit yet), or they are on the screen as it is
(standard_yaw, the view its zoom wants), or the player has turned to them or
into cover */
static boolean playback_cinematic_threat(
	long unit_index,
	real_vector3d const *aim,
	real focus_yaw,
	real focus_pitch,
	real standard_yaw,
	real *wanted_yaw,
	real *wanted_pitch)
{
	long age = playback.threat_tick != NONE ? game_time_get() - playback.threat_tick : NONE;
	long owner = NONE;
	real_point3d focus;
	real_vector3d to;
	real_vector3d from_player;
	struct collision_result collision;
	real standard = playback_free_camera_zoom_wanted();
	real standard_across, standard_up;
	real across, yaw, pitch, turn, rise, half_turn, half_rise, field_of_view, length;
	real wide_across, wide_up;

	playback.threat_field_of_view = 0.0f;
	if (playback.threat_tick == NONE || playback.threat_watched != playback_watched() || age < 0 ||
		age > (long)(PLAYBACK_CINEMATIC_THREAT_HOLD * TICKS_PER_SECOND))
	{
		playback.threat_tick = NONE;
		return FALSE;
	}
	/* (where they are now, if they are: the hit's origin once they are gone) */
	if (playback.threat_player != NONE)
	{
		struct player_datum *player = player_try_and_get(playback.threat_player);

		if (player && player->unit_index != NONE)
			owner = player->unit_index;
	}
	if (owner == NONE && playback.threat_object != NONE && object_try_and_get(playback.threat_object))
		owner = playback.threat_object;
	if (owner != NONE)
		playback_focus(owner, &playback.threat_point);
	playback_focus(unit_index, &focus);
	from_player.i = playback.threat_point.x - focus.x;
	from_player.j = playback.threat_point.y - focus.y;
	from_player.k = playback.threat_point.z - focus.z;
	length = (real)sqrt(from_player.i * from_player.i + from_player.j * from_player.j + from_player.k * from_player.k);
	/* (the player has turned to them, or is out of their sight) */
	if (length < 0.01f ||
		(from_player.i * aim->i + from_player.j * aim->j + from_player.k * aim->k) / length > (real)cos(PLAYBACK_CINEMATIC_THREAT_FACED) ||
		collision_test_vector(PLAYBACK_FREE_COLLISION_FLAGS, &focus, &from_player, NONE, &collision))
	{
		playback.threat_tick = NONE;
		return FALSE;
	}
	to.i = playback.threat_point.x - playback.free_position.x;
	to.j = playback.threat_point.y - playback.free_position.y;
	to.k = playback.threat_point.z - playback.free_position.z;
	across = (real)sqrt(to.i * to.i + to.j * to.j);
	yaw = (real)atan2(to.j, to.i);
	pitch = (real)atan2(to.k, across);
	/* (on the screen as it would be without them) */
	playback_view_tangents(standard, &standard_across, &standard_up);
	turn = playback_angle_between(standard_yaw, yaw);
	if ((real)fabs(turn) < _pi / 2.0f && (real)fabs(pitch - focus_pitch) < _pi / 2.0f &&
		(real)fabs(tan(turn)) < standard_across * PLAYBACK_CINEMATIC_THREAT_EDGE &&
		(real)fabs(tan(pitch - focus_pitch)) < standard_up * PLAYBACK_CINEMATIC_THREAT_EDGE)
	{
		return FALSE;
	}
	/* (the aim halfway between them, the view as wide as that takes, to its
	widest; wider than that, the player at the far edge of it and the aim as
	near the threat as that lets it) */
	turn = playback_angle_between(focus_yaw, yaw);
	rise = pitch - focus_pitch;
	half_turn = (real)fabs(turn) / 2.0f;
	half_rise = (real)fabs(rise) / 2.0f;
	playback_view_tangents(1.0f, &wide_across, &wide_up);
	wide_across /= (real)tan(0.5f);
	wide_up /= (real)tan(0.5f);
	field_of_view = 2.0f * (real)atan(MAX((real)tan(MIN(half_turn, 1.5f)) / (PLAYBACK_CINEMATIC_THREAT_EDGE * wide_across),
		(real)tan(MIN(half_rise, 1.5f)) / (PLAYBACK_CINEMATIC_THREAT_EDGE * wide_up)));
	field_of_view = PIN(field_of_view, standard, MAX(PLAYBACK_CINEMATIC_THREAT_WIDEST, standard));
	playback.threat_field_of_view = field_of_view;
	playback_view_tangents(field_of_view, &wide_across, &wide_up);
	half_turn = MIN(half_turn, (real)atan(PLAYBACK_CINEMATIC_THREAT_EDGE * wide_across));
	half_rise = MIN(half_rise, (real)atan(PLAYBACK_CINEMATIC_THREAT_EDGE * wide_up));
	*wanted_yaw = turn < 0.0f ? -half_turn : half_turn;
	*wanted_pitch = rise < 0.0f ? -half_rise : half_rise;
	return TRUE;
}

/* the tripod's framing, how far off the player its aim is going: the room
ahead of where they aim, as much of it as they aim across its view; on the
cinematic camera, whoever hurt them off the screen before that */
static void playback_free_camera_framing(
	long unit_index,
	real focus_yaw,
	real focus_pitch,
	real seconds)
{
	real wanted_yaw = 0.0f;
	real wanted_pitch = 0.0f;

	playback.threat_field_of_view = 0.0f;
	if (unit_index != NONE && playback_watched() != NONE)
	{
		real_vector3d aim;
		real tangent_across, tangent_up;
		real lead;

		playback_view_tangents(playback_free_camera_zoom_wanted(), &tangent_across, &tangent_up);
		playback_aim(unit_index, &aim);
		/* (their aim leftward of the camera's sight of them: the camera turned
		left of them, so that they stand right of the middle) */
		lead = (real)atan(tangent_across * PLAYBACK_FREE_THIRD *
			(real)sin(playback_angle_between(focus_yaw, (real)atan2(aim.j, aim.i))));
		wanted_yaw = lead;
		if (playback.view == _playback_view_cinematic)
		{
			playback_cinematic_threat(unit_index, &aim, focus_yaw, focus_pitch, focus_yaw + lead,
				&wanted_yaw, &wanted_pitch);
		}
	}
	playback_spring(&playback.free_frame_yaw, &playback.free_frame_yaw_speed, wanted_yaw, seconds, PLAYBACK_FREE_FRAMING_TIME);
	playback_spring(&playback.free_frame_pitch, &playback.free_frame_pitch_speed, wanted_pitch, seconds, PLAYBACK_FREE_FRAMING_TIME);
}

/* the free camera's aim going after the player's focus and its framing, as a
tripod's turned by hand: PLAYBACK_FREE_TRACK_TIME behind it */
static void playback_free_camera_track(
	long unit_index,
	real seconds)
{
	real_vector3d to;
	real across;
	real yaw, pitch;

	to.i = playback.free_focus.x - playback.free_position.x;
	to.j = playback.free_focus.y - playback.free_position.y;
	to.k = playback.free_focus.z - playback.free_position.z;
	across = (real)sqrt(to.i * to.i + to.j * to.j);
	if (across < 0.01f && (real)fabs(to.k) < 0.01f)
		return;
	yaw = (real)atan2(to.j, to.i);
	pitch = (real)atan2(to.k, across);
	playback_free_camera_framing(unit_index, yaw, pitch, seconds);
	yaw = playback.free_yaw + playback_angle_between(playback.free_yaw, yaw + playback.free_frame_yaw);
	pitch = PIN(pitch + playback.free_frame_pitch, -PLAYBACK_FREE_PITCH_MOST, PLAYBACK_FREE_PITCH_MOST);
	playback_spring(&playback.free_yaw, &playback.free_yaw_speed, yaw, seconds, PLAYBACK_FREE_TRACK_TIME);
	playback_spring(&playback.free_pitch, &playback.free_pitch_speed, pitch, seconds, PLAYBACK_FREE_TRACK_TIME);
	playback.free_pitch = PIN(playback.free_pitch, -PLAYBACK_FREE_PITCH_MOST, PLAYBACK_FREE_PITCH_MOST);
}

/* whether the player (their focus) can be seen from there past the level, its
scenery and its doors: their middle or either side of it, a post in the way of
one of them not hiding them */
static boolean playback_free_camera_sees(
	real_point3d const *position,
	real_point3d const *focus)
{
	real_vector3d to;
	real across;
	short side;

	to.i = focus->x - position->x;
	to.j = focus->y - position->y;
	to.k = focus->z - position->z;
	across = (real)sqrt(to.i * to.i + to.j * to.j);
	/* (at them: nothing between) */
	if (across < 0.01f && (real)fabs(to.k) < 0.01f)
		return TRUE;
	for (side = 0; side < 3; side++)
	{
		struct collision_result collision;
		real width = side == 0 ? 0.0f : side == 1 ? PLAYBACK_CINEMATIC_SIGHT_WIDTH : -PLAYBACK_CINEMATIC_SIGHT_WIDTH;
		real_vector3d probe = to;

		if (across > 0.01f)
		{
			probe.i -= to.j / across * width;
			probe.j += to.i / across * width;
		}
		if (!collision_test_vector(PLAYBACK_FREE_COLLISION_FLAGS, position, &probe, NONE, &collision))
			return TRUE;
	}
	return FALSE;
}

/* how much of the arm from the player's focus the camera can have, the
wall margin kept: along its middle and to either side and above and below of
its end, so that it is not placed against what it was only just missed by */
static real playback_free_camera_reach(
	real_point3d const *focus,
	real_vector3d const *arm)
{
	real length = (real)sqrt(arm->i * arm->i + arm->j * arm->j + arm->k * arm->k);
	real across = (real)sqrt(arm->i * arm->i + arm->j * arm->j);
	real reach = length;
	real_vector3d side;
	real_vector3d up;
	short probe;

	if (length < 0.01f)
		return 0.0f;
	side.i = across > 0.01f ? -arm->j / across : 1.0f;
	side.j = across > 0.01f ? arm->i / across : 0.0f;
	side.k = 0.0f;
	/* (side, across the arm's direction) */
	up.i = (side.j * arm->k - side.k * arm->j) / length;
	up.j = (side.k * arm->i - side.i * arm->k) / length;
	up.k = (side.i * arm->j - side.j * arm->i) / length;
	for (probe = 0; probe < 5; probe++)
	{
		static real const offsets[5][2] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f } };
		struct collision_result collision;
		real extra = (length + PLAYBACK_CAMERA_WALL_MARGIN) / length;
		real_vector3d vector;

		vector.i = arm->i * extra + (side.i * offsets[probe][0] + up.i * offsets[probe][1]) * PLAYBACK_FREE_PROBE_RADIUS;
		vector.j = arm->j * extra + (side.j * offsets[probe][0] + up.j * offsets[probe][1]) * PLAYBACK_FREE_PROBE_RADIUS;
		vector.k = arm->k * extra + (side.k * offsets[probe][0] + up.k * offsets[probe][1]) * PLAYBACK_FREE_PROBE_RADIUS;
		if (collision_test_vector(PLAYBACK_FREE_COLLISION_FLAGS, focus, &vector, NONE, &collision))
		{
			real along = collision.t * (real)sqrt(vector.i * vector.i + vector.j * vector.j + vector.k * vector.k);

			reach = MIN(reach, along - PLAYBACK_CAMERA_WALL_MARGIN);
		}
	}
	return MAX(reach, 0.0f);
}

/* how far the camera goes up for the ground around the player: none under a
roof; else half of what is around them (so not for a wall on one side of them),
as high as it is more than PLAYBACK_FREE_TERRAIN_LEAST above their feet, a
valley's or a trench's sides otherwise hiding them as they go along it */
static real playback_free_camera_terrain_rise(
	real_point3d const *focus,
	real feet,
	real distance)
{
	struct collision_result collision;
	real_point3d top = *focus;
	real_vector3d look = {{ 0.0f, 0.0f, PLAYBACK_FREE_TERRAIN_LOOK }};
	real heights[PLAYBACK_FREE_TERRAIN_DIRECTIONS];
	short direction;
	short other;

	if (collision_test_vector(PLAYBACK_FREE_COLLISION_FLAGS, focus, &look, NONE, &collision))
		return 0.0f;
	top.z += PLAYBACK_FREE_TERRAIN_LOOK;
	for (direction = 0; direction < PLAYBACK_FREE_TERRAIN_DIRECTIONS; direction++)
	{
		real angle = 2.0f * _pi * direction / PLAYBACK_FREE_TERRAIN_DIRECTIONS;
		real height = feet;
		real_vector3d out;

		out.i = (real)cos(angle) * distance;
		out.j = (real)sin(angle) * distance;
		out.k = 0.0f;
		/* (a wall as high as the look is that high) */
		if (collision_test_vector(PLAYBACK_FREE_COLLISION_FLAGS, &top, &out, NONE, &collision))
			height = top.z;
		else
		{
			real_point3d over = top;
			real_vector3d down = {{ 0.0f, 0.0f, feet - top.z }};

			over.x += out.i;
			over.y += out.j;
			if (collision_test_vector(PLAYBACK_FREE_COLLISION_FLAGS, &over, &down, NONE, &collision))
				height = collision.point.z;
		}
		for (other = direction; other > 0 && heights[other - 1] < height; other--)
			heights[other] = heights[other - 1];
		heights[other] = height;
	}
	return PIN(heights[PLAYBACK_FREE_TERRAIN_DIRECTIONS / 2 - 1] - feet - PLAYBACK_FREE_TERRAIN_LEAST,
		0.0f, PLAYBACK_FREE_TERRAIN_MOST);
}

/* the tripod placed for the player: behind them as behind's camera is (turned
as they face), up for the ground around them; or if that leaves too little of
the arm or no sight of them, the first that does of a crane over them and
either side of them; or if none does, whichever comes nearest. Looking at them */
static void playback_free_camera_place(
	long unit_index)
{
	struct object_datum *object = object_get(unit_index);
	long followed = object->object.parent_object_index != NONE ? object->object.parent_object_index : unit_index;
	real distance = followed == unit_index ? PLAYBACK_CAMERA_DISTANCE : PLAYBACK_CAMERA_VEHICLE_DISTANCE;
	real height = followed == unit_index ? PLAYBACK_CAMERA_HEIGHT : PLAYBACK_CAMERA_VEHICLE_HEIGHT;
	real across = distance * (real)cos(PLAYBACK_CAMERA_PITCH);
	real above = -distance * (real)sin(PLAYBACK_CAMERA_PITCH);
	real_point3d focus;
	real_point3d best;
	real best_score = -1.0f;
	real yaw, rise;
	real_vector3d forward;
	short place;
	struct
	{
		real turn;
		real across;
		real above;
	} places[4];

	object = object_get(followed);
	playback_focus(unit_index, &focus);
	yaw = (real)atan2(object->object.forward.j, object->object.forward.i);
	playback.yaw = yaw;
	rise = playback_free_camera_terrain_rise(&focus, focus.z - height, distance);
	places[0].turn = 0.0f;
	places[0].across = across;
	places[0].above = above + rise;
	places[1].turn = 0.0f;
	places[1].across = distance * PLAYBACK_FREE_CRANE_ACROSS;
	places[1].above = distance * PLAYBACK_FREE_CRANE_UP + rise;
	places[2].turn = _pi / 2.0f;
	places[2].across = across;
	places[2].above = above + rise;
	places[3].turn = -_pi / 2.0f;
	places[3].across = across;
	places[3].above = above + rise;
	best = focus;
	for (place = 0; place < NUMBEROF(places); place++)
	{
		real back = yaw + places[place].turn;
		real_vector3d arm;
		real_point3d position;
		real length, reach, score;
		boolean sees;

		arm.i = -(real)cos(back) * places[place].across;
		arm.j = -(real)sin(back) * places[place].across;
		arm.k = places[place].above;
		length = (real)sqrt(arm.i * arm.i + arm.j * arm.j + arm.k * arm.k);
		reach = playback_free_camera_reach(&focus, &arm);
		position.x = focus.x + arm.i * reach / length;
		position.y = focus.y + arm.j * reach / length;
		position.z = focus.z + arm.k * reach / length;
		sees = playback_free_camera_sees(&position, &focus);
		score = (reach / length) * (sees ? 1.0f : 0.25f);
		if (score > best_score)
		{
			best_score = score;
			best = position;
		}
		if (sees && reach / length >= PLAYBACK_FREE_LEAST_ARM)
			break;
	}
	forward.i = focus.x - best.x;
	forward.j = focus.y - best.y;
	forward.k = focus.z - best.z;
	/* (the arm all gone: as behind's, looking as they face) */
	if (forward.i * forward.i + forward.j * forward.j + forward.k * forward.k < 0.0001f)
	{
		forward.i = (real)cos(yaw);
		forward.j = (real)sin(yaw);
		forward.k = 0.0f;
	}
	playback_camera_set(&best, &forward);
}

/* the cinematic camera's sight of the player: hidden from it by the level,
its scenery or its doors for PLAYBACK_CINEMATIC_HIDDEN_TIME (at all, in the
PLAYBACK_CINEMATIC_SPAWN_TIME after they spawn), once the shot has lasted
PLAYBACK_CINEMATIC_LEAST_SHOT, it is placed anew, as L3 places the free camera. That
starts over only when they have been in sight for PLAYBACK_CINEMATIC_CLEAR_TIME, a run
past a row of posts counted as the hiding it is */
static void playback_cinematic_sight(
	long unit_index,
	real seconds)
{
	real_point3d focus;

	playback.cinematic_shot += seconds;
	playback_focus(unit_index, &focus);
	if (!playback_free_camera_sees(&playback.free_position, &focus))
	{
		playback.cinematic_clear = 0.0f;
		playback.cinematic_hidden += seconds;
		if (playback.cinematic_spawned > 0.0f ||
			(playback.cinematic_hidden >= PLAYBACK_CINEMATIC_HIDDEN_TIME && playback.cinematic_shot >= PLAYBACK_CINEMATIC_LEAST_SHOT))
		{
			playback.free_placing = TRUE;
		}
	}
	else
	{
		playback.cinematic_clear += seconds;
		if (playback.cinematic_clear >= PLAYBACK_CINEMATIC_CLEAR_TIME)
			playback.cinematic_hidden = 0.0f;
	}
	playback.cinematic_spawned = MAX(playback.cinematic_spawned - seconds, 0.0f);
}

/* how far round the player the cinematic camera's arc takes it this frame,
only as it moves, what was left of its swing easing off as it stops: once they
have moved across its view or turned for PLAYBACK_CINEMATIC_ORBIT_AFTER, it
swings to the rear quarter of where they aim on the side it is on, until they
have neither moved nor turned for PLAYBACK_CINEMATIC_ORBIT_REST. Not while it frames a
threat. Their velocity is the smoothed one (units a second) */
static real playback_cinematic_orbit(
	long unit_index,
	real_vector3d const *velocity,
	real azimuth,
	real across,
	real seconds)
{
	real_vector3d aim;
	real aim_yaw;
	real lateral;
	real turning = 0.0f;
	real target = azimuth;
	real swung = azimuth;

	playback_aim(unit_index, &aim);
	aim_yaw = (real)atan2(aim.j, aim.i);
	/* (going, any way they move keeps it going: across its view is less of
	their move the further round it gets) */
	lateral = playback.cinematic_orbiting ? (real)sqrt(velocity->i * velocity->i + velocity->j * velocity->j) :
		(real)fabs(velocity->j * (real)cos(azimuth) - velocity->i * (real)sin(azimuth));
	if (playback.cinematic_orbit_aim_valid)
		turning = (real)fabs(playback_angle_between(playback.cinematic_orbit_aim, aim_yaw)) / seconds;
	playback.cinematic_orbit_aim = aim_yaw;
	playback.cinematic_orbit_aim_valid = TRUE;
	if (!playback.cinematic_moving)
	{
		playback.cinematic_orbit_moving = playback.cinematic_orbit_still = 0.0f;
		playback.cinematic_orbiting = FALSE;
		playback.cinematic_orbit_side = 0.0f;
	}
	else if (lateral > PLAYBACK_CINEMATIC_ORBIT_SPEED || turning > PLAYBACK_CINEMATIC_ORBIT_TURN)
	{
		playback.cinematic_orbit_still = 0.0f;
		playback.cinematic_orbit_moving += seconds;
		if (playback.cinematic_orbit_moving >= PLAYBACK_CINEMATIC_ORBIT_AFTER)
			playback.cinematic_orbiting = TRUE;
	}
	else
	{
		playback.cinematic_orbit_still += seconds;
		if (playback.cinematic_orbit_still >= PLAYBACK_CINEMATIC_ORBIT_REST)
		{
			playback.cinematic_orbit_moving = 0.0f;
			playback.cinematic_orbiting = FALSE;
		}
	}
	if (playback.cinematic_orbiting && playback.threat_field_of_view == 0.0f && across >= PLAYBACK_CINEMATIC_ORBIT_LEAST)
	{
		real back = aim_yaw + _pi;
		real off = playback_angle_between(back, azimuth);

		/* (its side kept until it is well round the other, so that it does
		not change sides as they turn a little either way of it) */
		if (playback.cinematic_orbit_side == 0.0f || off * playback.cinematic_orbit_side < -PLAYBACK_CINEMATIC_ORBIT_QUARTER)
			playback.cinematic_orbit_side = off < 0.0f ? -1.0f : 1.0f;
		target = azimuth + playback_angle_between(azimuth, back + playback.cinematic_orbit_side * PLAYBACK_CINEMATIC_ORBIT_QUARTER);
	}
	playback_spring(&swung, &playback.cinematic_orbit_speed, target, seconds, PLAYBACK_CINEMATIC_ORBIT_TIME);
	return swung - azimuth;
}

/* the cinematic camera moving, seconds of the game's time. It is a tripod,
turning and zooming and not moving, while the player is still, slow, or has not
gone far from where it stood still. Once they go on at PLAYBACK_CINEMATIC_FOLLOW_SPEED
past PLAYBACK_CINEMATIC_FOLLOW_LEAST of where it stood, or further than the tether, or the
left stick changes the distance it follows at (zooming), it follows them: it goes
with them as fast as they go from it, their speed smoothed
(PLAYBACK_CINEMATIC_VELOCITY_TIME), and the difference from the distance it follows at is
made up on a spring, swinging round them on its arc as it goes
(playback_cinematic_orbit). When they have been slow for PLAYBACK_CINEMATIC_STILL_TIME and
it has come to rest, it is a tripod again. It moves about the player rather than
across the ground. Where it would lose sight of them coming in, it comes only as
far as it can see them from, or if that is under half of the way, the arc stops:
a wall the sight check cuts away from; going out, only as far as there is room.
It moves as a flown free camera does, never through or against a wall, and what
stops it stops its spring */
static void playback_cinematic_move(
	long unit_index,
	boolean zooming,
	real seconds)
{
	struct object_datum *object = object_get(unit_index);
	long followed = object->object.parent_object_index != NONE ? object->object.parent_object_index : unit_index;
	real arm_length = followed == unit_index ? PLAYBACK_CAMERA_DISTANCE : PLAYBACK_CAMERA_VEHICLE_DISTANCE;
	real tether = arm_length * PLAYBACK_CINEMATIC_TETHER_ARMS;
	real ideal = arm_length * PLAYBACK_CINEMATIC_FOLLOW_ARMS * playback.cinematic_distance;
	real_point3d const *focus = &playback.free_focus;
	real_vector3d *velocity = &playback.cinematic_velocity;
	real_vector3d offset;
	real_vector3d direction;
	real_vector3d move;
	real length, across, azimuth, turned, target, moved, speed;
	real with = 0.0f;

	if (seconds <= 0.0f)
		return;
	/* (how fast they go, smoothed: by their focus, so the vehicle's when they
	ride, and as a client's recording's corrections move them, not as their
	velocity says they go) */
	if (!playback.cinematic_velocity_valid)
	{
		velocity->i = velocity->j = velocity->k = 0.0f;
		playback.cinematic_velocity_speed = *velocity;
		playback.cinematic_velocity_valid = TRUE;
		playback.cinematic_anchor = *focus;
	}
	else
	{
		playback_spring(&velocity->i, &playback.cinematic_velocity_speed.i,
			(focus->x - playback.cinematic_last_focus.x) / seconds, seconds, PLAYBACK_CINEMATIC_VELOCITY_TIME);
		playback_spring(&velocity->j, &playback.cinematic_velocity_speed.j,
			(focus->y - playback.cinematic_last_focus.y) / seconds, seconds, PLAYBACK_CINEMATIC_VELOCITY_TIME);
		playback_spring(&velocity->k, &playback.cinematic_velocity_speed.k,
			(focus->z - playback.cinematic_last_focus.z) / seconds, seconds, PLAYBACK_CINEMATIC_VELOCITY_TIME);
	}
	playback.cinematic_last_focus = *focus;
	speed = (real)sqrt(velocity->i * velocity->i + velocity->j * velocity->j);
	playback.cinematic_slow = speed < PLAYBACK_CINEMATIC_STILL_SPEED ? playback.cinematic_slow + seconds : 0.0f;
	offset.i = playback.free_position.x - focus->x;
	offset.j = playback.free_position.y - focus->y;
	offset.k = playback.free_position.z - focus->z;
	length = (real)sqrt(offset.i * offset.i + offset.j * offset.j + offset.k * offset.k);
	across = (real)sqrt(offset.i * offset.i + offset.j * offset.j);
	if (length < 0.01f)
		return;
	if (!playback.cinematic_moving)
	{
		real_vector3d went;
		real least = arm_length * PLAYBACK_CINEMATIC_FOLLOW_LEAST;

		went.i = focus->x - playback.cinematic_anchor.x;
		went.j = focus->y - playback.cinematic_anchor.y;
		went.k = focus->z - playback.cinematic_anchor.z;
		if (length > tether || zooming ||
			(speed >= PLAYBACK_CINEMATIC_FOLLOW_SPEED && went.i * went.i + went.j * went.j + went.k * went.k > least * least))
		{
			playback.cinematic_moving = TRUE;
			playback.cinematic_slow = 0.0f;
			playback.cinematic_with = 0.0f;
		}
	}
	azimuth = (real)atan2(offset.j, offset.i);
	turned = playback_cinematic_orbit(unit_index, velocity, azimuth, across, seconds);
	/* (a tripod: no rays) */
	if (!playback.cinematic_moving && (real)fabs(turned) < 1.0e-5f && (real)fabs(playback.cinematic_tether_speed) < 1.0e-3f)
	{
		playback.cinematic_tether_speed = 0.0f;
		return;
	}
	direction.i = (real)cos(azimuth + turned) * across / length;
	direction.j = (real)sin(azimuth + turned) * across / length;
	direction.k = offset.k / length;
	target = length;
	if (playback.cinematic_moving)
	{
		real_vector3d arm;
		real_point3d goal;

		target = ideal;
		arm.i = direction.i * target;
		arm.j = direction.j * target;
		arm.k = direction.k * target;
		goal.x = focus->x + arm.i;
		goal.y = focus->y + arm.j;
		goal.z = focus->z + arm.k;
		/* (out: as far as there is room for, so that its spring comes to rest) */
		if (target > length)
			target = MAX(length, MIN(target, playback_free_camera_reach(focus, &arm)));
		else if (!playback_free_camera_sees(&goal, focus))
		{
			real reach = playback_free_camera_reach(focus, &arm);

			if (reach >= target * PLAYBACK_FREE_LEAST_ARM)
				target = MIN(target, reach);
			else if (turned != 0.0f)
			{
				turned = 0.0f;
				playback.cinematic_orbit_speed = 0.0f;
				direction.i = offset.i / length;
				direction.j = offset.j / length;
			}
		}
		target = MAX(target, length - PLAYBACK_CINEMATIC_TETHER_MOST_PULL);
		/* (with them as fast as they go from it, so that the spring has only the
		difference to make up, eased up to it as it starts; not backing away from
		them as fast as they come) */
		playback.cinematic_with = MIN(playback.cinematic_with + seconds / PLAYBACK_CINEMATIC_FOLLOW_EASE, 1.0f);
		with = MAX(-(velocity->i * offset.i + velocity->j * offset.j + velocity->k * offset.k) / length, 0.0f) *
			playback.cinematic_with * seconds;
	}
	moved = length;
	playback_spring(&moved, &playback.cinematic_tether_speed, target, seconds, PLAYBACK_CINEMATIC_TETHER_TIME);
	moved = MAX(moved - with, MIN(length, ideal * PLAYBACK_CINEMATIC_FOLLOW_CLOSEST));
	move.i = focus->x + direction.i * moved - playback.free_position.x;
	move.j = focus->y + direction.j * moved - playback.free_position.y;
	move.k = focus->z + direction.k * moved - playback.free_position.z;
	playback_free_camera_move(&move);
	/* (its spring as fast as it went toward or from them: what stops it, stops
	it. Before it is kept off what it is near, which is not its own going) */
	{
		real wanted = moved - length;
		real went;

		offset.i = playback.free_position.x - focus->x;
		offset.j = playback.free_position.y - focus->y;
		offset.k = playback.free_position.z - focus->z;
		went = (real)sqrt(offset.i * offset.i + offset.j * offset.j + offset.k * offset.k) - length;
		if ((real)fabs(wanted) > 1.0e-4f)
			playback.cinematic_tether_speed *= PIN(went / wanted, 0.0f, 1.0f);
	}
	playback_free_camera_clear();
	/* (they have been slow a while and it has come to rest: a tripod again) */
	if (playback.cinematic_moving && !zooming && playback.cinematic_slow >= PLAYBACK_CINEMATIC_STILL_TIME &&
		(real)fabs(playback.cinematic_tether_speed) < PLAYBACK_CINEMATIC_RESTING)
	{
		playback.cinematic_moving = FALSE;
		playback.cinematic_anchor = *focus;
	}
}

/* the angle from one to the other, the short way round */
static real playback_angle_between(
	real from,
	real to)
{
	real difference = (real)fmod(to - from, 2.0f * _pi);

	if (difference > _pi)
		difference -= 2.0f * _pi;
	else if (difference < -_pi)
		difference += 2.0f * _pi;
	return difference;
}

/* what the cameras look at: a little above what the unit is (on foot, or
the vehicle it rides), where it is drawn this frame */
static void playback_focus(
	long unit_index,
	real_point3d *focus)
{
	struct object_datum *object = object_get(unit_index);
	long followed = object->object.parent_object_index != NONE ? object->object.parent_object_index : unit_index;

	object_get_origin(followed, focus);
	render_interpolation_object_point(followed, focus);
	focus->z += followed == unit_index ? PLAYBACK_CAMERA_HEIGHT : PLAYBACK_CAMERA_VEHICLE_HEIGHT;
}

/* behind what the unit is (on foot, or the vehicle it rides): turned
with it, or, in the behind view once its right stick is touched, flown
round it (across with the stick's sideways, up to above it and down with
its up and down), at the left stick's zoom (its up nearer, down further).
The free camera is placed where this is */
static void playback_behind_camera(
	long unit_index,
	real seconds)
{
	struct object_datum *object = object_get(unit_index);
	long followed = object->object.parent_object_index != NONE ? object->object.parent_object_index : unit_index;
	real distance = followed == unit_index ? PLAYBACK_CAMERA_DISTANCE : PLAYBACK_CAMERA_VEHICLE_DISTANCE;
	real_point3d focus;
	real_point3d position;
	real_vector3d back;
	real_vector3d forward;
	struct collision_result collision;
	real yaw, turn;
	real pitch = PLAYBACK_CAMERA_PITCH;

	if (playback.view == _playback_view_behind)
	{
		real move_x, move_y, turn_x, turn_y, rise, mouse_yaw, mouse_pitch;

		playback_sticks(&move_x, &move_y, &turn_x, &turn_y, &rise, &mouse_yaw, &mouse_pitch);
		if (!playback.behind_orbiting && playback.camera_valid &&
			(turn_x != 0.0f || turn_y != 0.0f || mouse_yaw != 0.0f || mouse_pitch != 0.0f))
		{
			playback.behind_orbiting = TRUE;
			playback.behind_pitch = PLAYBACK_CAMERA_PITCH;
		}
		/* (across round them, up over them) */
		if (playback.behind_orbiting)
		{
			playback.yaw += mouse_yaw - turn_x * PLAYBACK_CAMERA_ORBIT_TURN * seconds;
			playback.behind_pitch = PIN(playback.behind_pitch - mouse_pitch -
				turn_y * PLAYBACK_CAMERA_ORBIT_TURN * seconds, PLAYBACK_CAMERA_ORBIT_HIGHEST, PLAYBACK_CAMERA_ORBIT_LOWEST);
			pitch = playback.behind_pitch;
		}
		playback.behind_zoom = PIN(playback.behind_zoom * (real)exp(-move_y * PLAYBACK_CAMERA_ZOOM_RATE * seconds),
			PLAYBACK_CAMERA_ZOOM_NEAREST, PLAYBACK_CAMERA_ZOOM_FARTHEST);
		distance *= playback.behind_zoom;
	}

	object = object_get(followed);
	playback_focus(unit_index, &focus);
	/* (eased round behind it, unless flown) */
	yaw = (real)atan2(object->object.forward.j, object->object.forward.i);
	if (!playback.behind_started || !playback.camera_valid)
	{
		playback.yaw = yaw;
		playback.behind_started = TRUE;
	}
	if (!playback.behind_orbiting || playback.view != _playback_view_behind)
	{
		turn = yaw - playback.yaw;
		while (turn > _pi)
			turn -= 2.0f * _pi;
		while (turn < -_pi)
			turn += 2.0f * _pi;
		playback.yaw += turn * PLAYBACK_CAMERA_TURN;
	}
	forward.i = (real)(cos(playback.yaw) * cos(pitch));
	forward.j = (real)(sin(playback.yaw) * cos(pitch));
	forward.k = (real)sin(pitch);
	back.i = -forward.i * distance;
	back.j = -forward.j * distance;
	back.k = -forward.k * distance;
	/* (no further than the first wall behind it) */
	if (collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
		&focus, &back, NONE, &collision))
	{
		real kept = collision.t * distance > PLAYBACK_CAMERA_WALL_MARGIN ?
			(collision.t * distance - PLAYBACK_CAMERA_WALL_MARGIN) / distance : 0.0f;

		back.i *= kept;
		back.j *= kept;
		back.k *= kept;
	}
	position.x = focus.x + back.i;
	position.y = focus.y + back.j;
	position.z = focus.z + back.k;
	playback_camera_set(&position, &forward);
}

/* the free camera, a tripod, and the cinematic camera, which is the same with its sticks not heard, for the player
(unit_index, NONE while they are dead or not yet in the game): placed behind them (playback_free_camera_place) when it is
chosen; tracking them (the right stick's click toggles it, the right stick or the mouse takes the aim), moved by the left
stick and the jump and crouch buttons, eased and slid along walls, still tracking. The cinematic one is placed anew after
a jump of theirs and when the level hides them, frames them and zooms, following them when they go on
(playback_cinematic_move), its sticks heard only for the distance it follows at (the left stick's up and down). Content
Evolved's playback_free_camera; FALSE, the camera left alone, while it cannot be placed (nobody to place it behind, and no
camera before) */
static boolean playback_cinematic_camera(
	long unit_index,
	real seconds)
{
	real move_x, move_y, turn_x, turn_y, rise, mouse_yaw, mouse_pitch;
	real_vector3d forward;
	real_vector3d move;
	/* (the camera's own moving goes with the game: still while it is paused) */
	real game_seconds = seconds * game_time_get_speed();
	boolean zooming = FALSE;
	boolean cinematic = playback.view == _playback_view_cinematic;

	playback_sticks(&move_x, &move_y, &turn_x, &turn_y, &rise, &mouse_yaw, &mouse_pitch);
	if (cinematic)
	{
		/* (the left stick's up and down: the distance it follows at, as behind's zoom, up nearer; its other sticks not
		heard) */
		zooming = move_y != 0.0f;
		playback.cinematic_distance = PIN(playback.cinematic_distance * (real)exp(-move_y * PLAYBACK_CAMERA_ZOOM_RATE * seconds),
			PLAYBACK_CINEMATIC_FOLLOW_NEAREST, PLAYBACK_CINEMATIC_FOLLOW_FARTHEST);
		move_x = move_y = turn_x = turn_y = rise = mouse_yaw = mouse_pitch = 0.0f;
		/* (a player who died: hidden as they spawn, followed at once) */
		if (unit_index == NONE)
			playback.cinematic_died = TRUE;
		else if (playback.cinematic_died)
		{
			playback.cinematic_died = FALSE;
			playback.cinematic_spawned = PLAYBACK_CINEMATIC_SPAWN_TIME;
		}
	}
	if (cinematic && !playback.free_placing && unit_index != NONE)
	{
		/* (moved there: a teleporter, a spawn, a jump in the replay) */
		if (playback.free_focus_valid)
		{
			real_point3d focus;
			real jumped;

			playback_focus(unit_index, &focus);
			jumped = (real)sqrt((focus.x - playback.free_focus.x) * (focus.x - playback.free_focus.x) +
				(focus.y - playback.free_focus.y) * (focus.y - playback.free_focus.y) +
				(focus.z - playback.free_focus.z) * (focus.z - playback.free_focus.z));
			if (jumped > PLAYBACK_CINEMATIC_JUMP_LEAST && jumped > PLAYBACK_CINEMATIC_JUMP_SPEED * game_seconds)
				playback.free_placing = TRUE;
		}
		playback_cinematic_sight(unit_index, seconds);
	}
	if (playback.free_placing)
	{
		if (unit_index != NONE)
			playback_free_camera_place(unit_index);
		if (!playback.camera_valid)
			return FALSE;
		playback.free_placing = FALSE;
		playback.free_tracking = TRUE;
		playback.free_still = TRUE;
		playback.cinematic_hidden = 0.0f;
		playback.cinematic_clear = 0.0f;
		playback.cinematic_shot = 0.0f;
		playback.cinematic_died = FALSE;
		playback.cinematic_spawned = 0.0f;
		playback.cinematic_moving = FALSE;
		playback.cinematic_tether_speed = 0.0f;
		playback.cinematic_velocity_valid = FALSE;
		playback.cinematic_slow = 0.0f;
		playback.cinematic_orbit_speed = playback.cinematic_orbit_moving = playback.cinematic_orbit_still = 0.0f;
		playback.cinematic_orbiting = FALSE;
		playback.cinematic_orbit_side = 0.0f;
		playback.cinematic_orbit_aim_valid = FALSE;
		playback.threat_field_of_view = 0.0f;
		playback.free_yaw_speed = playback.free_pitch_speed = playback.free_field_of_view_speed = 0.0f;
		playback.free_frame_yaw = playback.free_frame_yaw_speed = 0.0f;
		playback.free_frame_pitch = playback.free_frame_pitch_speed = 0.0f;
		playback.free_field_of_view = PLAYBACK_FIELD_OF_VIEW;
		playback.free_focus_valid = FALSE;
		playback.free_move_x = playback.free_move_y = playback.free_rise = 0.0f;
		playback.free_position = playback.camera.position;
		playback.free_yaw = (real)atan2(playback.camera.forward.j, playback.camera.forward.i);
		playback.free_pitch = (real)asin(PIN(playback.camera.forward.k, -1.0f, 1.0f));
	}
	/* (the player's focus; where they died, while they are dead) */
	if (unit_index != NONE)
	{
		playback_focus(unit_index, &playback.free_focus);
		playback.free_focus_valid = TRUE;
		/* (first, so that its aim is from where it now is) */
		if (cinematic)
			playback_cinematic_move(unit_index, zooming, game_seconds);
	}
	/* (the right stick or the mouse takes the aim) */
	if (turn_x != 0.0f || turn_y != 0.0f || mouse_yaw != 0.0f || mouse_pitch != 0.0f)
		playback.free_tracking = FALSE;
	if (playback.free_tracking && playback.free_focus_valid)
		playback_free_camera_track(unit_index, seconds);
	else
	{
		playback.threat_field_of_view = 0.0f;
		playback.free_yaw_speed = playback.free_pitch_speed = 0.0f;
		playback.free_yaw += mouse_yaw - turn_x * PLAYBACK_FREE_TURN * seconds;
		playback.free_pitch = PIN(playback.free_pitch + mouse_pitch + turn_y * PLAYBACK_FREE_TURN * seconds,
			-PLAYBACK_FREE_PITCH_MOST, PLAYBACK_FREE_PITCH_MOST);
	}
	forward.i = (real)(cos(playback.free_yaw) * cos(playback.free_pitch));
	forward.j = (real)(sin(playback.free_yaw) * cos(playback.free_pitch));
	forward.k = (real)sin(playback.free_pitch);
	/* (as the game's debug flying camera: the stick moves it level, toward where it looks across the ground whatever its
	pitch, and only the jump and crouch buttons change its height; both eased and slid along walls) */
	if (move_x != 0.0f || move_y != 0.0f || rise != 0.0f)
		playback.free_still = FALSE;
	playback_free_camera_speed(move_x, move_y, seconds);
	rise = playback_free_camera_rise(rise, seconds);
	move.i = ((real)cos(playback.free_yaw) * playback.free_move_y + (real)sin(playback.free_yaw) * playback.free_move_x) *
		PLAYBACK_FREE_SPEED * seconds;
	move.j = ((real)sin(playback.free_yaw) * playback.free_move_y - (real)cos(playback.free_yaw) * playback.free_move_x) *
		PLAYBACK_FREE_SPEED * seconds;
	move.k = rise * PLAYBACK_FREE_SPEED * seconds;
	playback_free_camera_move(&move);
	if (playback.free_move_x != 0.0f || playback.free_move_y != 0.0f || rise != 0.0f)
		playback_free_camera_clear();
	playback_free_camera_zoom(seconds);
	playback_camera_set(&playback.free_position, &forward);
	playback.camera.field_of_view = playback.free_field_of_view;
	return TRUE;
}

/* a view of the behind or cinematic kind, made for this frame (the player's unit, NONE with none): FALSE when there is
nothing to make it from (no unit, and no camera yet, or the cinematic one not placeable) */
static boolean playback_views_camera(
	long unit_index)
{
	real seconds = PIN(main_get_seconds_elapsed(), 0.0f, 0.25f);

	if (playback.view == _playback_view_cinematic || playback.view == _playback_view_free)
		return playback_cinematic_camera(unit_index, seconds);
	/* (behind: held where it was while the player has no unit) */
	if (unit_index == NONE)
		return playback.camera_valid;
	playback_behind_camera(unit_index, seconds);
	return TRUE;
}

/* a view chosen with Y: the free and cinematic ones placed anew behind the player (the free one tracks them till it is
flown), the cinematic at the distance it starts at, behind's with no flying round and the distance it starts at */
static void playback_views_begin(
	void)
{
	playback.free_placing = playback.view == _playback_view_cinematic || playback.view == _playback_view_free;
	playback.cinematic_distance = 1.0f;
	playback.behind_orbiting = FALSE;
	playback.behind_started = FALSE;
	playback.behind_zoom = 1.0f;
}
