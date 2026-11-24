/*
AE_BACKPACK_RULES.H

BACKPACK WEAPONS (ae_backpack_weapons.c), where a holstered weapon goes, as
pure rules (port/linux/tests/ae_backpack_rules_test.c runs them):

- ITS KIND, from its tag path, and its definition's bounding radius when no
  word names it: small (a pistol, a magnum, a revolver, an SMG, a needler, a
  plasma rifle) goes on the thigh, very long (a sniper rifle) upright on the
  back, anything else (a rifle, a shotgun, a launcher) slung across the back.
  Words first, as aLTis94's Chimera script guesses an unknown weapon's place
  from its name (an idea only: that script has no licence).
- ITS PLACE, in the order the unit's slots are walked: the first on the back
  takes its kind's place, a second crosses it the other way; the first small
  one takes the right thigh, a second the left; a third of either is not
  drawn.
- ITS FIT, from its tag path: a model that holds its long axis off its up
  axis is turned about its forward axis first, so it lies on the back as one
  that holds it upright does; a bulky one is moved out from the back by a
  distance and leaned toward upright by an angle, so no part of it is inside
  the body; one whose model's node rests turned is set its own way (the
  flamethrower's barrel), and one whose bulk is off its origin is moved up or
  down, to a side or along its barrel so it is centred on the back (the fuel
  rod's tube onto the spine), clear of the helmet and the shoulder pads.
*/

#ifndef __AE_BACKPACK_RULES_H
#define __AE_BACKPACK_RULES_H

enum
{
	AE_BACKPACK_KIND_BACK = 0,
	AE_BACKPACK_KIND_LONG,
	AE_BACKPACK_KIND_THIGH,
};

enum
{
	AE_BACKPACK_PLACE_BACK = 0,
	/* a second weapon on the back (a campaign's third or fourth slot) */
	AE_BACKPACK_PLACE_BACK_SECOND,
	/* a very long weapon, upright */
	AE_BACKPACK_PLACE_LONG,
	AE_BACKPACK_PLACE_THIGH,
	/* a second small one, on the left thigh */
	AE_BACKPACK_PLACE_THIGH_SECOND,
	/* SEARCH AND DESTROY's carried bomb, upright high on the back (ae_bomb_assault.h); never a rule's answer */
	AE_BACKPACK_PLACE_BOMB,
	AE_BACKPACK_PLACE_COUNT,
	AE_BACKPACK_PLACE_NONE = -1
};

/* below this radius (world units) a weapon no word names is small */
#define AE_BACKPACK_SMALL_RADIUS 0.12f

/* a weapon's kind from its tag path (NULL: none known) and its definition's
bounding radius */
int ae_backpack_rules_kind(char const *tag_path, float bounding_radius);

/* the place of the next weapon of a kind, the counts of those already placed
on the back and on the thighs taken and moved on (AE_BACKPACK_PLACE_NONE: not
drawn) */
int ae_backpack_rules_place(int kind, int *on_back, int *on_thighs);

/* how a weapon's model is fitted to its place on the back, from its tag path */
struct ae_backpack_fit
{
	/* degrees about the model's own forward axis, before it is placed */
	float turn_degrees;
	/* world units out from the back, along the body's backward axis */
	float out;
	/* degrees about the body's forward axis, toward upright */
	float lean_degrees;
	/* the directions of its barrel and its top in the body's space at rest (forward, left, up), each in place of its
	place's (all 0: the place's own): a model whose node rests turned is set its own way; a top alone keeps the place's
	barrel (the top at right angles to every place's barrel) */
	float barrel[3];
	float top[3];
	/* world units along the body's up and left axes (a model centred on the back) */
	float up;
	float side;
	/* world units along its barrel, toward the muzzle (a model whose origin is off its middle, centred on the back) */
	float along;
};

/* the fit of a weapon's model (NULL path: none known): all 0 for most */
void ae_backpack_rules_fit(char const *tag_path, struct ae_backpack_fit *fit);

#endif // __AE_BACKPACK_RULES_H
