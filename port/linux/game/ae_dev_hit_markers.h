/*
AE_DEV_HIT_MARKERS.H

DEV MODE's hit markers (the look and timing: ae_dev_hit_markers_rules.h):
corner brackets round the reticle when damage this PC's player dealt lands,
red on a kill. Any weapon, stock or a pack's, grenades and melee too: they
come from the damage itself (damage.c's object_damage_aftermath), to
anything that can be damaged. This PC only (ae_dev_mode_on), so the host's
or a game without network: a client deals no damage of its own.
*/

#ifndef AE_DEV_HIT_MARKERS_H
#define AE_DEV_HIT_MARKERS_H

union real_rectangle2d;
union real_vector2d;

/* damage.c's object_damage_aftermath: damage the owner dealt to an object (shield and body), and whether it killed it */
void ae_dev_hit_markers_note(long owner_player_index, long object_index, float damage, int kill);
/* hud_draw.c: a centred reticle piece drawn at point with its bounds and scale (HUD units), so the brackets fit round
the weapon's own reticle */
void ae_dev_hit_markers_reticle(short x, short y, const union real_rectangle2d *bounds,
	const union real_vector2d *xy_scale, float reticle_scale);
/* hud.c, after the weapon's HUD: the view's brackets */
void ae_dev_hit_markers_draw(short local_player_index);

#endif
