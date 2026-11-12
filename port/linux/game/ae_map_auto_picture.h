/*
AE_MAP_AUTO_PICTURE.H

(0.7.9) A picture for each custom map that has none. The first time a local game starts on a map the menus show no picture
for (ae_map_pictures.h: no previews\<file>.png beside its file, no stock art, not a Workshop map), the first life's view
is shot once, clean (no HUD, no crosshair, no first-person weapon, no AE overlay: the frame as the 3D view is before they
are drawn), a moment after the player spawned, and saved as that previews file (16:9, 640 by 360, an 8-bit RGBA PNG,
written whole or not at all, never over a file). Rules in ae_map_auto_picture_rules.h.

Where the game's frame goes: interface.c asks ae_map_auto_picture_hides_hud each frame for each local view (the HUD is
not drawn for the shot's frame), render_objects.c asks ae_map_auto_picture_hides_weapon (before the HUD, in the frame
after the decision), and d3d8_gl.c's present asks ae_map_auto_picture_capturing before the AE layer is drawn and then
reads the back buffer into ae_map_auto_picture_store. A dedicated server draws nothing, so it never shoots; a client of
someone else's game does not.
*/

#ifndef AE_MAP_AUTO_PICTURE_H
#define AE_MAP_AUTO_PICTURE_H

/* (interface.c) the HUD is not drawn in this view: the shot's frame (and each frame of it); also the state machine's
step, once a frame for local view 0 */
int ae_map_auto_picture_hides_hud(short local_player_index);

/* (render_objects.c) the first-person weapon is not drawn: the frame of the shot */
int ae_map_auto_picture_hides_weapon(void);

/* (d3d8_gl.c, at present) this frame is the shot's: read the back buffer (bgra, 4 bytes a pixel, row 0 the top) */
int ae_map_auto_picture_capturing(void);
void ae_map_auto_picture_store(unsigned char const *bgra, int width, int height);

#endif
