/*
AE_RETICLE_MOTION.H

The unified reticle motion (ae_reticle_motion.c): what moves a local player's crosshair, read from the weapon's state
the same on every machine. Included after cseries.h.
*/

#ifndef __AE_RETICLE_MOTION_H
#define __AE_RETICLE_MOTION_H

#include "ae_crosshair_rules.h"

struct ae_reticle_motion
{
	/* the firing spread, in 1080p pixels: the trigger's own error angle above its minimum, at the view's field of view */
	real spread;
	/* the kick of a shot, as a fraction of its peak (0 to 1.x): rifles' by their profile, any other weapon without a
	spread by a modest default; a weapon with a spread has none (its spread is the motion) */
	real kick;
	/* ALL WEAPONS' stock reticle (ae_xh_motion_class_of): the held weapon's class from its tag, and the trigger's real
	error now, 0 to 1 */
	struct ae_xh_motion_class cls;
	real error;
	/* the movement cue, in 1080p pixels (a feel cue only: Halo CE weapons have no movement inaccuracy) */
	real move;
};

void ae_reticle_motion_of(short local_player_index, long weapon_index, struct ae_reticle_motion *motion);

#endif // __AE_RETICLE_MOTION_H
