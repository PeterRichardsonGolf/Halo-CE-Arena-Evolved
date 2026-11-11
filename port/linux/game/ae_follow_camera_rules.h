#ifndef AE_FOLLOW_CAMERA_RULES_H
#define AE_FOLLOW_CAMERA_RULES_H

/*
AE_FOLLOW_CAMERA_RULES.H

(Arena Evolved, 0.8.1, owner 2026-11-11: "the rotation is not smooth", the turning lagged and snapped; "when you die it instantly cuts to follow cam would be nice if we could see teabags and stuff for a
bit") The rules of the death and follow cameras (ae_follow_camera.c), apart from the engine so the unit tests can run
them:
- the death camera stays on the player's own body until they respawn, or until they press A, LB or RB (after a short
  pause, so a button pressed at death doesn't skip it), or, waiting long, after AE_FOLLOW_HOLD_SECONDS;
- the follow camera sits behind the player it follows and turns with where they aim, smoothed by a half-life (the same
  feel at any frame rate), and changes to it, or between players, glide in AE_FOLLOW_GLIDE_SECONDS on a minimum-jerk
  curve instead of cutting.
*/

/* the camera behind the player followed (world units; 3.0 until the owner's first play, 11-11: "bring in the follow cam a
little bit and not zoom it out so far"), and its view (degrees) */
#define AE_FOLLOW_DISTANCE 2.2f
#define AE_FOLLOW_FIELD_OF_VIEW_DEGREES 70.0f
/* how its look follows the aim: half of the way in this many seconds (yaw and pitch) */
#define AE_FOLLOW_TURN_HALF_LIFE 0.07f
/* how the point it looks at follows the player: across, and up and down (slower: no bob on jumps and crouches) */
#define AE_FOLLOW_FOCUS_HALF_LIFE 0.04f
#define AE_FOLLOW_FOCUS_RISE_HALF_LIFE 0.10f
/* the room near walls (pitch and view, ae_follow_camera_room.c), and the distance coming in or out */
#define AE_FOLLOW_ROOM_HALF_LIFE 0.12f
/* the aim's pitch taken (a share of it, looking a little down on the player), within these (radians, up positive) */
#define AE_FOLLOW_PITCH_SHARE 0.6f
#define AE_FOLLOW_PITCH_DOWN 0.15f
#define AE_FOLLOW_PITCH_LOWEST -0.9f
#define AE_FOLLOW_PITCH_HIGHEST 0.35f
/* a change of camera or of the player followed: a glide this long */
#define AE_FOLLOW_GLIDE_SECONDS 0.5f
/* the death camera: a press counts after this long dead (1 s until the owner's first play, 11-11: "you don't get to see
your death cam very long": a press a second in left the body), so the body is watched at least that long; on a long
wait (a long respawn, a round-based game holding the spawns) it moves on by itself after AE_FOLLOW_HOLD_SECONDS
(15 until the owner's first play: "auto follow for long respawns and round based modes") */
#define AE_FOLLOW_PRESS_AFTER_SECONDS 3.0f
#define AE_FOLLOW_HOLD_SECONDS 5.0f

/* THE DEATH CAMERA (owner 2026-11-11, after play-testing: "Look at the killer, Start at the helmet,
Right stick orbits body"): it starts at the player's own eyes and eases out over AE_DEATH_EASE_SECONDS (in and out) to
its place AE_DEATH_DISTANCE behind the body, looking down AE_DEATH_PITCH; it turns to look past the body at the killer
while the killer is in sight (half the way in AE_DEATH_GAZE_HALF_LIFE; a little of the killer's height taken); with no
killer (a fall, a suicide) it drifts round slowly; the right stick (the look input) orbits it from then on, within its
pitch limits */
#define AE_DEATH_EASE_SECONDS 1.0f
#define AE_DEATH_DISTANCE 2.5f
#define AE_DEATH_PITCH -0.35f
#define AE_DEATH_GAZE_HALF_LIFE 0.25f
#define AE_DEATH_DRIFT_DEGREES 6.0f
#define AE_DEATH_PITCH_LOWEST -1.3f
#define AE_DEATH_PITCH_HIGHEST 0.6f
/* SWITCHES (owner 2026-11-11, after play-testing: a cut with a quick fade in): a change to another player, or from the
death camera to following, cuts, the view fading in from black over AE_FOLLOW_FADE_SECONDS; it glides
(AE_FOLLOW_GLIDE_SECONDS) only from one followed player to another within AE_FOLLOW_GLIDE_REACH of it and in sight */
#define AE_FOLLOW_FADE_SECONDS 0.2f
#define AE_FOLLOW_GLIDE_REACH 3.0f

/* display.follow_camera's values */
enum
{
	AE_FOLLOW_SETTING_LONG_WAITS = 0,
	AE_FOLLOW_SETTING_ALWAYS,
	AE_FOLLOW_SETTING_OFF,
};

/* the share of the way from current to target after seconds, for a half-life (1 for a half-life of 0 or less; 0 for no
time) */
float ae_follow_blend(float half_life, float seconds);
/* a value smoothed toward a target over seconds */
float ae_follow_smooth(float current, float target, float half_life, float seconds);
/* an angle (radians) smoothed toward a target the shorter way round; the result is within -pi..pi of the target */
float ae_follow_smooth_angle(float current, float target, float half_life, float seconds);
/* the minimum-jerk curve of a glide: 0 at 0, 1 at 1 and after, with no speed at either end */
float ae_follow_glide(float fraction);
/* an ease in and out (quadratic): 0 at 0, 1 at 1 and after */
float ae_follow_ease_in_out(float fraction);
/* whether a change from one followed player to another glides (else it cuts and fades in): the distance between them
and whether the new one is in sight of the old */
int ae_follow_switch_glides(float distance, int in_sight);
/* the death camera's pitch looking past the body at the killer: the killer's rise over the body and its distance */
float ae_death_pitch(float rise, float across);
/* the camera's pitch for the followed player's aim pitch (radians, up positive) */
float ae_follow_pitch(float aim_pitch);
/* whether the dead player's camera leaves their body for the follow camera now: the setting, the seconds dead, whether
they pressed A, LB or RB this frame, whether their wait to respawn is a long one, and whether no respawn is coming (out of
lives, odd man out, the next round's). Owner 11-11: on the body till the respawn; a press from 3 s leaves it (ALWAYS: any
death; LONG WAITS: a long wait); on its own after 5 s only when no respawn is coming */
int ae_follow_leave_body(int setting, float dead_seconds, int pressed, int long_wait, int no_respawn);
/* (owner 11-11: a self-kill looked at the last death's killer) whether a kill recorded at killed_at (game ticks, -1: none)
is this death's: at most a second before the death camera began at started_at, or after it (the kill may be told a
tick late) */
int ae_death_killer_is_this_death(long killed_at, long started_at, long ticks_per_second);
/* the killer the death camera looks at: none (-1) for one's own kill or no killer */
long ae_death_killer_to_watch(long killer, long dead);

#endif
