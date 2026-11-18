/*
AE_FOLLOW_CAMERA.H

(Arena Evolved, 0.8.1, owner 2026-11-11) The follow camera turns with the aim of the player it follows, smoothed, and
glides into place instead of cutting (ae_follow_camera.c; its rules: ae_follow_camera_rules.h).
*/

#ifndef __AE_FOLLOW_CAMERA_H
#define __AE_FOLLOW_CAMERA_H

struct dead_camera;
struct observer_command;
struct render_screen_flash;

/* (coop_spectate.c's coop_spectate_camera, each frame it watches a living player from behind) this dead camera
follows that player: ae_follow_camera_command writes its command this frame */
void ae_follow_camera_track(struct dead_camera *camera);

/* (dead_camera.c's dead_camera_update, last) for a camera tracked this frame: the command behind the followed
player along their aim, eased with the frame's seconds, gliding from where the view was when it began following or
changed player; for any other, nothing. (The dead camera's command is laid out as the observer's.) */
void ae_follow_camera_command(struct dead_camera *camera, float seconds, struct observer_command *command);
/* (owner 11-11) whether the death camera holds this camera on its player's own body: the original game's switch to the
next player waits then (it cut to an enemy in a game without teams, and back to the old camera); A, LB and RB still
leave the body for the follow camera */
int ae_follow_camera_holds_body(struct dead_camera const *camera);

/* (follow_camera.c, while the dead player's camera is still on their body) whether it leaves the body for the follow
camera now: display.follow_camera's value (follow_camera.c's order: long waits, always, off), the ticks dead, LB or RB
pressed this frame (1 or -1, follow_camera.c's edge), the dead player's controller (A, or jump on the keyboard, read
here), and whether the wait to respawn is a long one. A press counts only once every button has been seen up since the
death (a button held from life, or one that reads as held every frame, never leaves the body). Logged when it leaves. */
int ae_follow_camera_leave_body(short setting, long dead_ticks, int lbrb, short controller_index, int long_wait,
	int no_respawn);

/* (follow_camera.c's follow_camera_player_killed) who killed whom: the death camera looks at the killer */
void ae_follow_camera_killed(long killing_player_index, long dead_player_index);

/* (player_effects.c's player_effect_get_screen_flash, last) a local player's view fading in from black after a cut of the
follow camera: the screen flash, darkened (darken_type: player_effects.c's darken) unless a stronger flash is on */
void ae_follow_camera_screen_flash(short local_player_index, struct render_screen_flash *flash, short darken_type);

#endif
