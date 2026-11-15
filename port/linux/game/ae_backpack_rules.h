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

#endif // __AE_BACKPACK_RULES_H
