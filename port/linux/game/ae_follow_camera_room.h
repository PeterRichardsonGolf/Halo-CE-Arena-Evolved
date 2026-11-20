/*
AE_FOLLOW_CAMERA_ROOM.H

(Arena Evolved, SPIKE 10-30) The follow camera in a tight room (ae_follow_camera_room.c).
*/

#ifndef __AE_FOLLOW_CAMERA_ROOM_H
#define __AE_FOLLOW_CAMERA_ROOM_H

/* (ae_follow_camera.c, each frame it follows a unit) where the room behind the unit, looking along yaw, is too tight
for the camera's distance, the camera looks down from higher and its view widens, rather than being pulled in close
behind the unit's back: pitch (in: the camera's own, radians, up positive) becomes the steeper of it and the room's,
and field_of_view (in: the open view) the view the room wants. The caller eases both. */
void ae_follow_camera_room(short local_player_index, long unit_index, real yaw, real *pitch, real *field_of_view);

#endif
