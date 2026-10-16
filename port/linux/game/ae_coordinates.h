/*
AE_COORDINATES.H

SHOW COORDINATES (display.coordinates, Video Setup > GRAPHICS), for map
makers: the performance line (display.performance; alone in its corner when
that is off) ends with player 1's position in world units and the way they
face: "X 97.65 Y -169.62 Z 0.26  FACING 21" (degrees about the up axis, 0
along +x, counterclockwise, as Sapien's facing). The position is player 1's
body's origin (at the feet), or the camera's while they have no body.
*/

#ifndef AE_COORDINATES_H
#define AE_COORDINATES_H

/* (main.c main_framerate_render, an AE hook line) appends the coordinates to the line, if shown */
void ae_coordinates_append(char *line, long size);
/* (main.c main_framerate_shown) whether the line is drawn for them (the power list keeps clear of it) */
int ae_coordinates_shown(void);
/* the text for a position and facing (pure) */
void ae_coordinates_format(char *text, long size, float x, float y, float z, float forward_i, float forward_j);

#endif
