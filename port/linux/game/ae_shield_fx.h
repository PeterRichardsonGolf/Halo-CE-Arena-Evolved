/*
AE_SHIELD_FX.H

HIT FEEDBACK: ARENA EVOLVED's effects (ae_shield_fx.c): what the AE hook lines
call (port/linux/game/ae_hooks.txt lists them). Included after cseries.h.
Each does nothing unless the game's gametype has HIT FEEDBACK: ARENA EVOLVED.
*/

#ifndef __AE_SHIELD_FX_H
#define __AE_SHIELD_FX_H

union real_rgb_color;
union real_vector3d;

/* ---- drawing (render_objects.c, first_person_weapons.c, rasterizer_xbox_plasma_energy.c) */

/* the values a unit's shield flare is drawn with, given the game's (body, or the first-person arms) */
real const *ae_shield_fx_values(long unit_index, real const *values, boolean arms);
/* the color for values from ae_shield_fx_values (a pop's white, a recharge's tint); FALSE: the shield's own */
boolean ae_shield_fx_color(real const *values, union real_rgb_color *color);
/* around the flare's draw: the fade by height for values from ae_shield_fx_values (offset and scale: the noise
coordinate's); TRUE when one was set, to be ended */
boolean ae_shield_fx_draw_begin(real const *values, real offset, real scale);
void ae_shield_fx_draw_end(void);

/* ---- the shield's light (shield_glow.c) */

real ae_shield_fx_glow_value(long unit_index, real value);
void ae_shield_fx_glow_color(long unit_index, union real_rgb_color *color);
real ae_shield_fx_glow_reach(long unit_index);

/* ---- damage (damage.c) */

/* a shield falls: TRUE when its burst was shown already, with the pop */
boolean ae_shield_fx_no_burst(long object_index);
/* a shield fell: its pop, unless shown */
void ae_shield_fx_depleted(long object_index);
/* a shield took damage and stands: the pop when due (with the map's burst), and a shield at nothing broken */
void ae_shield_fx_shield_damaged(long object_index, long burst_effect_index, unsigned long *being_damaged_flags);
/* TRUE once after a pop was shown by the damage just taken (the hit's own flare gives way to it) */
boolean ae_shield_fx_early_pop_taken(void);
/* a shield recharges, at this level now: the pop armed again */
void ae_shield_fx_recharging(long object_index, real shield);
/* whether a hit that reached a body shows the body's hit effect though the game would not (the blood pop)
(0: no; else how many bursts of it) */
short ae_shield_fx_bloodshot(long object_index, unsigned long damage_flags, long owner_object_index,
	long hit_effect_index, real body_damage, unsigned long being_damaged_flags);
/* the direction of the bloodshot's burst number (1: the hit's own; the others thrown a little apart) */
union real_vector3d const *ae_shield_fx_blood_direction(union real_vector3d const *direction, short burst);
/* the effect a bloodshot shows, given the body's own */
long ae_shield_fx_blood_effect(long hit_effect_index);
/* the shield damage of a round, given what the game reckoned: the weapon pack's battle rifle against a shield whose
pop was shown takes at least a third of the finishing hit's points, so one burst of three to the head finishes a
popped player, whatever popped them (COMBAT FEEDBACK: ARENA EVOLVED only; the host's reckoning) */
float ae_shield_fx_rifle_finish(long object_index, long damage_definition_index, float shield_damage);
/* a machine that is not the host: a hit's result as the host sent it (before its replay) */
void ae_shield_fx_guest_hit(unsigned long being_damaged_flags, float body_damage);
/* the body's own hit effect (NONE: none) */
long ae_shield_fx_hit_effect(long object_index);
/* the blood pop's bursts for the hit just handed over, replayed on this object (0: none) */
struct damage_data;
short ae_shield_fx_guest_blood_pop(long object_index, struct damage_data const *damage);

#endif /* __AE_SHIELD_FX_H */
