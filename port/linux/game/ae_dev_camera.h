/*
AE_DEV_CAMERA.H

DEV MODE's cameras, for checking what first person hides (backpack
weapons, armour, effects): a third-person camera over the shoulder
style (controls.dev_camera, F2; a controller's BACK and d-pad down); the
other shoulder (controls.dev_shoulder, F3; BACK and right); a free camera
(CE's flying camera, moved and turned by the player's own input, jump and
crouch up and down: controls.dev_free_camera, F6; BACK and left). This PC
only (ae_dev_mode_on); the shoulder camera on foot only: a vehicle, a zoom,
a death or a cinematic gives the game its own camera back until it ends.
*/

#ifndef AE_DEV_CAMERA_H
#define AE_DEV_CAMERA_H

enum
{
	AE_DEV_CAMERA_KEEP = 0,
	/* director.c sets ae_dev_camera_update, gliding there from the view (a key turned it on) */
	AE_DEV_CAMERA_THIRD,
	/* the same at once (back from a zoom, a vehicle, a death: no glide, which a quick zoom would catch half done) */
	AE_DEV_CAMERA_THIRD_SNAP,
	/* director.c sets flying_camera_update (the camera data already set up) */
	AE_DEV_CAMERA_FREE,
	/* director.c gives the game's camera back at once (director_choose_game_perspective, forced, its glide stopped:
	while one runs the game still counts the view as third person, a zoom's scope and first person weapon unseen) */
	AE_DEV_CAMERA_GIVE_BACK
};

/* the keys (controls.dev_camera, controls.dev_shoulder, controls.dev_free_camera; a controller's BACK with the d-pad's
down, right, left), as bits FLAG(key) */
enum
{
	AE_DEV_CAMERA_KEY_THIRD = 0,
	AE_DEV_CAMERA_KEY_SHOULDER,
	AE_DEV_CAMERA_KEY_FREE
};

union real_vector3d;

/* player_control.c, each tick before the director's hold empties it: the player's throttle (forward, left), turn and
jump and crouch, which move the free camera while it runs */
void ae_dev_camera_note_input(short local_player_index, float forward, float left, float yaw, float pitch, int rise,
	int fall);
/* director.c's director_update_controls: the free camera's turn and move this frame (from the player's input), and
TRUE while it runs (the player held still) */
int ae_dev_camera_free_controls(short local_player_index, float *yaw, float *pitch, union real_vector3d *translation,
	float seconds);
/* director.c's director_choose_camera_game, each frame for a living or dead local player: which camera to switch to,
given the one running (camera_proc) and its data (0x40 bytes) */
int ae_dev_camera_choose(short local_player_index, void *camera_proc, void *camera_data);
/* whether a dev camera holds the view, so the game's own choice (first person, a vehicle's, the death camera) waits */
int ae_dev_camera_holds(short local_player_index);
/* whether the free camera runs, so the director's flying controls take the keyboard and mouse */
int ae_dev_camera_free(short local_player_index);
/* director_get_perspective: whether a camera is the third-person dev camera (a third-person view: the body drawn,
first-person weapons hidden) */
int ae_dev_camera_is_third(void *camera_proc);
/* director.c's director_choose_camera_game: whether holding the gamepad's black button (RB) a second turns the game
camera (first person, following, flying), the Xbox debug control upstream keeps for a host: off unless
debug.camera_black_switch (owner 10-28: RB held in split screen put player 2 in the flying camera; DEV MODE's cameras
have their own keys and chords) */
int ae_dev_camera_black_switch(void);
/* the third-person camera itself (a director_camera_update_proc) */
void ae_dev_camera_update(void *camera, void *action, void *result);

#endif
