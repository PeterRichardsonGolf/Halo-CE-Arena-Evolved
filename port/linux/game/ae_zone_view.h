/*
AE_ZONE_VIEW.H

What a machine knows of the round's danger zone, read from the round state (the host's own, or a joined PC's from message
106): the centre, the plan and where it stands now. A machine without the message has none (and the zone shows nothing; the
host's damage still lands). Everything the zone draws (ae_zone_world.c, ae_zone_hud.c) and the strip's clock cell read it.
*/
#ifndef AE_ZONE_VIEW_H
#define AE_ZONE_VIEW_H

#include "ae_pure_types.h"
#include "ae_round_zone_rules.h"

struct ae_zone_view
{
	real centre_x, centre_y, centre_z;
	struct ae_zone_plan plan;
	boolean running;      /* the zone's clock runs (the round is live and the zone has started) */
	long elapsed;         /* ticks since the zone started (0 before) */
	int phase;            /* enum ae_zone_phase */
	real radius;          /* the ring now */
	real next_radius;     /* the ring it closes to next (the radius itself once closed) */
};

/* TRUE with the zone to show: a round mode with a zone, in its countdown (the full ring) or live; not once the round is
over */
boolean ae_zone_view_get(struct ae_zone_view *out);

#endif
