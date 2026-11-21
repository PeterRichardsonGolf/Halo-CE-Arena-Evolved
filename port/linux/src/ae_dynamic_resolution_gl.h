/*
AE_DYNAMIC_RESOLUTION_GL.H

TARGET 60 FPS in the renderer (the rules: port/linux/game/ae_dynamic_resolution_rules.h; the settings and the view
count: ae_dynamic_resolution.c). Each view's 3D passes draw into the part of the view its scale gives, from the
view's top left corner (render.c's render_window, between halo_screen_dynamic_resolution_begin and _end), and the
end scales that part up over the whole view, so the HUD and menus drawn after are at the screen's resolution.
While it draws, d3d8_gl.c maps the game's coordinates for the screen's targets through ae_dynres_coordinate
(viewports, scissors, clears), counts a visibility test's pixels at the scale's area, and reads the screen's targets
as textures from the drawn part (ae_dynres_texture: the shaders' texture_scale with an offset). Other targets (the
secondary target of the mirrors and active camouflage, the shadow maps) are drawn as before.

The scale changes only between frames: at the swap, from the frame's time (the CPU's work between swaps, and the
GPU's own where OpenGL can time it), ae_dynamic_resolution_swap_begin and _end in platform_video_swap.
*/

#ifndef AE_DYNAMIC_RESOLUTION_GL_H
#define AE_DYNAMIC_RESOLUTION_GL_H

/* this frame's scale each way (1 at full resolution), and whether a view's 3D passes are drawing at it now */
extern float ae_dynres_scale;
extern int ae_dynres_drawing;

/* render.c: a view's 3D passes start and end (the view's bounds in the game's units) */
void halo_screen_dynamic_resolution_begin(short x0, short y0, short x1, short y1);
void halo_screen_dynamic_resolution_end(short x0, short y0, short x1, short y1);

/* d3d8_gl.c, while ae_dynres_drawing, for the screen's targets: a coordinate (the game's units, axis 0 across, 1 down)
where it is drawn; the area a pixel counts for; a texture of the screen's read from the drawn part (scale: the
shader's texture_scale, xy the coordinates' scale, zw an offset; width and height: the target's in the game's units) */
float ae_dynres_coordinate(float coordinate, int axis);
float ae_dynres_area(void);
void ae_dynres_texture(int screen, unsigned long width, unsigned long height, float scale[4]);

/* sdl_platform.c's platform_video_swap: before the frame is shown, and once it is (after any wait for the frame rate
limit) */
void ae_dynamic_resolution_swap_begin(void);
/* d3d8_gl.c's D3DDevice_Present: the frame is drawn, before its copy to the window (the GPU's timing ends) */
void ae_dynamic_resolution_frame_drawn(void);
void ae_dynamic_resolution_swap_end(void);

#endif
