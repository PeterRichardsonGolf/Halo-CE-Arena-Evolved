/*
AE_MELEE_RULES.H

MELEE, a gametype rule (the gametype's byte is ae_variant_ext's melee_rule, AE_EXT_MELEE_*). Pure
(port/linux/tests/ae_melee_rules_test.c).
- CLASSIC: Halo CE's melee damage.
- AE: a melee blow is worth at least AE_MELEE_BODY_SHARE of the target's maximum body health (100 on a 75 body
  health player), scaled by the gametype's damage multiplier as the blow itself is, never less than the weapon's own
  blow, so a blow on a full shield breaks it and finishes the body in one hit.
*/

#ifndef AE_MELEE_RULES_H
#define AE_MELEE_RULES_H

#define AE_MELEE_BODY_SHARE (4.0f / 3.0f)

int ae_melee_rule_on(int rule);

/* the blow's points under the rule: CLASSIC the weapon's own, AE at least AE_MELEE_BODY_SHARE * maximum_body * scale.
scale is the damage multiplier the gametype already put on the blow (health, resistance; 1 when none), so the floor
follows the health the gametype sets */
float ae_melee_blow_points(int rule, float points, float maximum_body, float scale);

/* what is left of a blow after the shield took shield_left of it (never below zero) */
float ae_melee_body_after(float blow, float shield_left);

/* the bleed mode a blow converts by: a melee blow under the AE rule the second game's way (the leftover without the
shield's multiplier) whatever the SHIELD BLEED row says, any other damage the row's */
int ae_melee_bleed_mode(int rule, int is_melee, int row_mode);

/* the shield points a raised blow carries into the body: excess (in the blow's raised points) turned back by the
UNRAISED ratio raw / unraised (the shield's multiplier alone), so the body's own multiplier lands it point for point */
float ae_melee_bleed_excess(float excess, float raw, float unraised);

/* a covered blow's points reaching the body: a leftover that reaches maximum_body (CE kills only below zero) is a
hair over it, so it kills */
float ae_melee_kill_points(float points, float maximum_body);

/* whether the body side raises a blow: only one the shield side did not count, and only one that reaches the body
(an over-charged shield takes all of it and the body gets nothing) */
int ae_melee_body_raise(int counted_by_shield, float body_points);

/* whether a melee from behind kills outright: only under AE, a melee blow, on an enemy, not seated, and unsuspecting
(the target does not see the attacker; units.h unit_unsuspecting) */
int ae_melee_kills_from_behind(int rule, int is_melee, int enemies, int seated, int unsuspecting);

/* a back smack's cone: the cosine of the widest angle from straight behind a blow still counts (60 degrees each way;
a blow from the side does not) */
#define AE_MELEE_BEHIND_COS 0.5f

/* whether a blow comes from behind, on the ground plane: (dir_x, dir_y) from the strike point to the victim's centre,
(look_x, look_y) the victim's look; 1 when the angle between them is within the cone, 0 for a zero vector */
int ae_melee_from_behind(float dir_x, float dir_y, float look_x, float look_y);

/* the finishing hit's points an early shield pop is reckoned by: the finisher's own under both values (AE's own pop,
COMBAT FEEDBACK's, unchanged: under AE a shield it pops is one a melee finishes) */
float ae_melee_pop_points(int rule, float finisher_points);

/* how far forward the reach is measured (the assault rifle's standing head lean) */
float ae_melee_reach_forward(void);

/* the ray's origin under AE: the unit's position (px, py) plus ae_melee_reach_forward() along the aim's horizontal
(normalised in the plane; a straight up or down aim adds nothing), at head_z as the animation posed it */
void ae_melee_origin_from(float px, float py, float head_z, float aim_x, float aim_y, float out[3]);

#endif
