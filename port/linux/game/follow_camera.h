/*
FOLLOW_CAMERA.H

The multiplayer follow camera: a player with a long wait to respawn watches
a living player from behind instead of their own body (follow_camera.c).
*/

#ifndef __FOLLOW_CAMERA_H
#define __FOLLOW_CAMERA_H

/* the unit a dead local player follows (LB/RB, or Q and E on the keyboard,
switch who), or NONE for the death camera: alive, a short wait to respawn,
the setting off, or nobody to follow */
long follow_camera_unit(
	short local_player_index);

/* called each frame a local player is not on the dead camera (alive, or no death yet): forgets the last death's
state (when they were first seen dead, whether following), so the next death starts the death camera afresh. Only
follow_camera_unit used to reset it, and the director calls that only while dead, so every death after the first one
kept the first one's clock and follow flag */
void follow_camera_alive(
	short local_player_index);

/* game_engine_player_killed: who killed whom, for following the killer in
a game without teams */
void follow_camera_player_killed(
	long killing_player_index,
	long dead_player_index);

/* the HUD line saying who a dead local player is following */
void follow_camera_draw(
	short local_player_index);

#endif
