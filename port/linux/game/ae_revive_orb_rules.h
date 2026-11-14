/*
AE_REVIVE_ORB_RULES.H

The revive orb's numbers, pure (unit tested: port/linux/tests/ae_revive_orb_rules_test.c).
*/
#ifndef AE_REVIVE_ORB_RULES_H
#define AE_REVIVE_ORB_RULES_H

#include "ae_pure_types.h"

/* the orb: its radius (world units; about 0.4 across, waist high), the height of its centre over the spot */
#define AE_REVIVE_ORB_RADIUS 0.2f
#define AE_REVIVE_ORB_LIFT 0.28f
/* when the downed player's own body cannot be named, a body this near the down spot (across, up) is taken for it */
#define AE_REVIVE_BODY_REACH 0.5f
#define AE_REVIVE_BODY_REACH_UP 0.6f
#define AE_REVIVE_PULSE_TICKS 60

/* team index to the orb's colour (red 0, blue 1, others ice) in 0..1 */
void ae_revive_orb_team_rgb(int team, real rgb[3]);
/* the pulse, 0..1 (a sine over AE_REVIVE_PULSE_TICKS) */
real ae_revive_orb_pulse(long ticks);
/* the surface alpha of a facet: more at the rim (facing 0 edge-on .. 1 face-on), a little shimmer by facet and tick */
real ae_revive_orb_alpha(real facing, int facet, long ticks, real pulse);
/* whether a body at (dx, dy, dz) from a down spot is that player's */
int ae_revive_body_is_at_spot(real dx, real dy, real dz);

#endif
