/*
AE_FOLLOW_CAMERA_ROOM.H

(Arena Evolved, SPIKE 10-30) The follow camera in a tight room (ae_follow_camera_room.c).
*/

#ifndef __AE_FOLLOW_CAMERA_ROOM_H
#define __AE_FOLLOW_CAMERA_ROOM_H

struct dead_camera;

/* (coop_spectate.c, after it sets the camera behind the watched unit) where the room behind the unit is too
tight for the camera's distance, the camera looks down from higher and its view widens, rather than being
pulled in close behind the unit's back */
void ae_follow_camera_room(struct dead_camera *camera);

#endif
