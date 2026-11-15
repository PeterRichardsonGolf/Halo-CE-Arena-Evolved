/*
AE_BLOOD.H

Dynamic blood (ae_blood.c; SETTINGS > VIDEO > DYNAMIC BLOOD, config.toml's game.dynamic_blood, off by default): a hit
throws blood onto the wall behind, and blood on a wall runs down it. This machine's alone: nothing is sent. Off, every
hook below does nothing and the game's own blood is all there is.

The hooks (port/linux/game/ae_hooks.txt): decals.c, damage.c, render.c and render_particles.c call the functions
marked "hook". The single-quad decals blood's runs are drawn with live in decals.c's own file
(ae_blood_decals.inc, #included at its end), as they reach its private decal list.
*/

#ifndef AE_BLOOD_H
#define AE_BLOOD_H

struct damage_data;

/* hook, decals_initialize_for_new_map: no blood of the last map's */
void ae_blood_new_map(void);
/* hook, decals_update (each frame; runs once a tick): the setting read, drips running, drops falling */
void ae_blood_update(void);
/* hook, render_window, each frame before the decals are drawn: running drips drawn out to where they are between
ticks */
void ae_blood_render_update(void);
/* hook, decal_new_from_collision: before its decal is laid, whether it lands on two-sided surfaces too (a tree's trunk
and leaves, a fence: blood's, with dynamic blood on); read by decal_clip_to_surface */
extern int ae_blood_two_sided;
int ae_blood_decal_two_sided(long decal_definition_index);
/* hook, decal_new_from_collision: a decal landed on the level (blood on a wall starts running) */
void ae_blood_decal_landed(long decal_definition_index, real_point3d const *point, real_vector3d const *normal,
	real radius, short cluster_index);
/* hook, object_damage_aftermath: an object hit (a biped's blood on the wall or slope behind it) */
void ae_blood_object_damaged(long object_index, struct damage_data const *damage, real damage_done,
	real damage_most);
/* hook, render_particles (after the game's particles): blood's drops falling (ae_blood_drops.c) */
void ae_blood_drops_render(void);

/* dynamic blood is on (the setting, cached each frame by ae_blood_update) */
int ae_blood_on(void);
/* ae_blood_drops.c: the drops to draw this frame (falling): where, which way, how big, and the blood's splat */
short ae_blood_drops_for_drawing(real_point3d *positions, real_vector3d *directions, real *radii, long *decals,
	short maximum);

/* ae_blood_decals.inc (in decals.c): decals of one quad blood lays and moves (a drip's run and its bead) */
/* (while set, ae_blood_decal_new_quad maps only this middle share of its picture along and across the quad, at its
definition's fullest intensity: a drip's run, a thin slice down the middle of a splat stretched along it) */
extern real ae_blood_texture_core_along;
extern real ae_blood_texture_core_across;
extern int ae_blood_full_intensity;
long ae_blood_decal_new_quad(long decal_definition_index, real_point3d const *point, real_vector3d const *normal,
	real_vector3d const *axis, real half_length, real half_width, short cluster_index, real_point3d corners[4]);
int ae_blood_decal_move_quad(long decal_index, long decal_definition_index, real_point3d const corners[4],
	short cluster_index);
/* a decal definition's colour, the middle of its range (white without one) */
void ae_blood_decal_color(long decal_definition_index, real_rgb_color *color);
/* how large a decal of the definition is, at most (world units; 0 without one) */
real ae_blood_decal_radius(long decal_definition_index);

#endif
