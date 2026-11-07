/*
AE_HIT_FEEDBACK_RULES.H

COMBAT FEEDBACK (ARENA OPTIONS; "hit feedback" in the code): HALO CE (AE_HIT_FEEDBACK_CLASSIC), shields and hits
look as in Halo CE;
ARENA EVOLVED, a player's shield shows its state:

- a hit's flare is a short pulse, so quick hits read as a flicker;
- the POP, once a shield: a white flash of the whole body that then drains
  from the head to the feet, shown as the shield falls to where one finishing
  hit to the head would kill (or as it truly breaks, if it never got there);
- while the shield is down and not yet recharging, thin bands flicker on the
  body;
- every recharge is a band rising from the feet whose height is the shield's
  real level, with a flash of the whole body as it fills.

Looks only: no damage changes, and nothing new goes over the network (every
machine draws from the shield's level and flags it already has).

The rules here are pure (no engine includes): numbers in, numbers out. The
gametype keeps the option in bits 0-1 of the byte the Xbox game left as its
universal variant's pad5 (the high nibble is SILENT MOVEMENT's and SILENT
WEAPONS', ae_silent_rules.h). Every gametype from before has 0: CLASSIC.
*/

#ifndef __AE_HIT_FEEDBACK_RULES_H
#define __AE_HIT_FEEDBACK_RULES_H

enum
{
	AE_HIT_FEEDBACK_CLASSIC = 0,
	AE_HIT_FEEDBACK_ARENA,
	NUMBER_OF_AE_HIT_FEEDBACK_VALUES
};

/* two bits: 0 HALO CE, 1 ARENA EVOLVED; 2 and 3 are kept for two more looks (not built: a gametype
that has one plays ARENA EVOLVED) */
#define AE_HIT_FEEDBACK_BYTE_MASK 0x03u

/* the option of a variant's byte, and the byte with the option set (its other bits kept) */
int ae_hit_feedback_value(unsigned char byte);
unsigned char ae_hit_feedback_byte_set(unsigned char byte, int value);
char const *ae_hit_feedback_name(int value);

/* ---- the pop */

/* seconds the pop lasts; the part of it the whole body stays white */
#define AE_HIT_FEEDBACK_POP_SECONDS 0.6f
#define AE_HIT_FEEDBACK_POP_FLASH 0.25f

/* whether a shield with this many points left is one finishing hit to the
head from a kill: the finisher's points against a shield, less what is left,
is more than nothing (a hit that only empties the shield kills nobody).
finisher_points 0 (no such weapon on the map): never */
int ae_hit_feedback_pop_due(float shield_points_left, float finisher_points);

/* the pop at t (0..1 of its time): the flare's strength (0..1), and the part
of the body's height (0 feet, 1 head) under which it is drawn (1: all of it);
FALSE when over */
int ae_hit_feedback_pop_shape(float t, float *strength, float *top);

/* ---- the recharge */

/* the band's length as a part of the body's height, and the seconds of the
flash of the whole body as the shield fills */
#define AE_HIT_FEEDBACK_BAND_WIDTH 0.65f
#define AE_HIT_FEEDBACK_FULL_FLASH_SECONDS 0.3f

/* the band of a shield at this level (0 empty, 1 full): its front's height
on the body (0..1) and its tail's (never under 0) */
void ae_hit_feedback_band(float shield_level, float *front, float *tail);
/* the band's colour at this level: white turning pale yellow (rgb, 0..1) */
void ae_hit_feedback_band_color(float shield_level, float *rgb);
/* the flash as the shield fills, seconds after it did: 0..1, 0 when over */
float ae_hit_feedback_full_flash(float seconds_since_full);
/* the first-person arms' swell as the band passes their height: 0..1 */
float ae_hit_feedback_arms_swell(float shield_level);

/* ---- hits, and the shield while down */

/* a hit's flare drawn from the game's (1 at the hit, fading to 0 over two seconds) */
float ae_hit_feedback_hit_pulse(float game_value);

/* the flicker of a shield that is down, for a unit at a time: TRUE when a band
is lit now, its height on the body (0..1) and its strength (0..1) */
int ae_hit_feedback_flicker(unsigned long unit, float seconds, float *height, float *strength);

#endif /* __AE_HIT_FEEDBACK_RULES_H */
