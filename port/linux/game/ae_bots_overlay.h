/*
AE_BOTS_OVERLAY.H

Arena Evolved: what the DEV MODE bot navigation overlay (ae_bot_nav_overlay.h) reads from the bots' translation unit
(bots.c, ae_bots_overlay.inc): the walkable surfaces closed to the bots, and each driving bot's place and path. Read only;
the obstacles, the nav file, the drive troubles and the learnt wedges come from their own headers.
*/

#ifndef __AE_BOTS_OVERLAY_H
#define __AE_BOTS_OVERLAY_H

#include "cseries.h"
#include "math/real_math.h"

/* the most corners of a closed surface (bots.c's BOTS_SURFACE_CORNERS) and path steps of a driver */
#define AE_BOTS_OVERLAY_CORNERS 16
#define AE_BOTS_OVERLAY_STEPS 4

struct ae_bots_overlay_driver
{
	long bot_index;
	/* ae_bots_vehicle_rules.h's class of its vehicle */
	int vehicle_class;
	real_point3d position;
	/* the next waypoint (the route's point on a route, else the path's next step) */
	int have_next;
	real_point3d next;
	/* the path's steps still to go */
	int step_count;
	real_point3d steps[AE_BOTS_OVERLAY_STEPS];
};

/* how many walkable surfaces the map opens that the bots' searches have closed (0 with no map) */
long ae_bots_overlay_closed_count(void);
/* the closed surfaces with a corner within range (x, y) of centre, up to max: their corners into corners (each surface
AE_BOTS_OVERLAY_CORNERS apart), their numbers of corners into corner_counts; how many. The list of closed surfaces is
kept and read again once a few seconds */
long ae_bots_overlay_closed_near(real_point3d const *centre, real range, real_point3d *corners, long *corner_counts, long max);
/* the bots at a vehicle's wheel, up to max: how many */
long ae_bots_overlay_drivers(struct ae_bots_overlay_driver *out, long max);

#endif
