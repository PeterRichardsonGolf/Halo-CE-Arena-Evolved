#ifndef __AE_SPLATTER_H__
#define __AE_SPLATTER_H__

/*
AE_SPLATTER.H

FRIENDLY SPLATTER (the gametype's rule, ae_splatter_rules.h): how much of the vehicle collision damage a biped takes
when a vehicle runs into it (physics.c's AE hook).
*/

/* 1 = Halo CE's damage, 0 = none (the biped is only pushed aside) */
real ae_splatter_damage_scale(long vehicle_index, long biped_index);

#endif
