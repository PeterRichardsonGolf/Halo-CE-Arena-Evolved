/*
AE_HAMMER_RULES.H

The gravity hammer's blast push, as pure rules (port/linux/tests/
ae_hammer_rules_test.c runs them; ae_hammer.c applies them to the game):

- THE SWING: a swing of the gravity hammer pack's weapon (its tag path,
  whatever its case) lands the hammer's blow (its melee damage) on what it
  hits, as the melee button's blow does, with rays 3.125 times as long (2.5
  units): a body whose centre is within 2.8 units, not one at 3. The swing's
  blast comes too; the blow's recoil on the swinger (the weapon's melee
  response) does not. One swing, one cooldown: the melee button does not
  swing again until the trigger could (its rate of fire).
- THE BLAST is the gravity hammer pack's blast damage tag (its tag path,
  whatever its case). Every swing makes one; its damage and its reach are the
  tag's. Its push follows the rules below instead of the tag's.
- WHO IS PUSHED: every object the blast reaches whose surface (its bounding
  sphere) is within 1.5 units of the blast. The swinger never is: the tag does
  not hurt its owner, so the game leaves him out of the blast altogether.
- THE PUSH is a change of velocity of 3.5 units a second times the object's
  acceleration scale, the same near and far. A body (a biped) is taken to
  have a scale of 1.2 whatever its tag says (the stock cyborg's is 0.7), so a
  player gets 4.2 units a second; a vehicle or an item keeps its own. It
  points away from the blast, at the object's centre raised by a twentieth of
  its radius (straight up when the blast is at the centre). A push of 0.05 or
  less on an object is none (0.3 or less on a body, which 4.2 never is).
*/

#ifndef __AE_HAMMER_RULES_H
#define __AE_HAMMER_RULES_H

/* whether the damage tag (its path, NULL for none) is the gravity hammer's blast */
int ae_hammer_rules_blast(char const *damage_tag_path);

/* whether the weapon tag (its path, NULL for none) is the gravity hammer */
int ae_hammer_rules_weapon(char const *weapon_tag_path);

/* how much longer than the game's melee blow (its rays) the swing's blow is */
float ae_hammer_rules_swing_reach_scale(void);

/* whether the melee button is refused: ticks_since_swing since the hammer's trigger last swung (negative for never),
the trigger's swings a second, the game's ticks a second */
int ae_hammer_rules_melee_blocked(long ticks_since_swing, float swings_a_second, float ticks_a_second);

/* the blast's push on an object: center and radius its bounding sphere's, acceleration_scale its definition's (not
used for a body), body nonzero for a biped; 1 and its velocity change (units a second) when it is pushed, 0 and none
when it is not */
int ae_hammer_rules_push(float const blast[3], float const center[3], float radius, float acceleration_scale,
	int body, float velocity[3]);

#endif
