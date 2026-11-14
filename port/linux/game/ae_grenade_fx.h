/*
AE_GRENADE_FX.H

The grenade catalogue's effect modules (ae_grenade_catalogue.h's AE_GRENADE_FX_*): one file each, one entry point
each, run on the host (or a local game) when a catalogue grenade goes off (ae_grenades.c's ae_grenade_detonated),
with the blast's targets: every unit and equipment item within the row's outer radius that the point sees (the
environment only), with its strength (ae_grenade_falloff). A later grenade reuses modules, or adds one file here.

  ae_grenade_fx_stall.c          stall: vehicles lose throttle, brake, their driver's gun locks
  ae_grenade_fx_scramble.c       scramble: a hit player's motion tracker and HUD (the draw is the HUD's)
  ae_grenade_fx_powerup_short.c  powerup_short: powerup items lying in reach are shorted (deleted; they come back
                                 at their spawn's next respawn, as a picked-up one does)
*/

#ifndef __AE_GRENADE_FX_H
#define __AE_GRENADE_FX_H

#include "cseries.h"
#include "math/real_math.h"
#include "ae_grenade_catalogue.h"

#define AE_GRENADE_BLAST_TARGETS 64

struct ae_grenade_blast
{
	short id;
	struct ae_grenade_entry const *entry;
	real_point3d point;
	long now;
	long thrower_unit;
	long thrower_player;
	short thrower_team;
	short target_count;
	struct
	{
		long object_index;
		short type;
		real strength;
		real distance;
	} targets[AE_GRENADE_BLAST_TARGETS];
};

void ae_grenade_fx_stall_apply(struct ae_grenade_blast const *blast);
void ae_grenade_fx_scramble_apply(struct ae_grenade_blast const *blast);
void ae_grenade_fx_powerup_short_apply(struct ae_grenade_blast const *blast);

/* (ae_grenades.c's state) the host's hits: the ticks the effect has left after it */
long ae_grenade_fx_stall_hit(long vehicle_index, long ticks, float immunity_seconds);
long ae_grenade_fx_scramble_hit(long unit_index, long ticks);
#endif
