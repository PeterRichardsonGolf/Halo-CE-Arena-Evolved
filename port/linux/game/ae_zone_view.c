/* The round's danger zone, as a machine knows it (ae_zone_view.h) */
#include "cseries.h"
#include "game/game.h"

#include "ae_round.h"
#include "ae_round_internal.h"
#include "ae_round_wire.h"
#include "ae_zone_view.h"

#include <string.h>

boolean ae_zone_view_get(struct ae_zone_view *out)
{
	struct ae_round_wire const *view;
	long now = game_time_get();

	if (!out || !ae_round_on() || !(view = ae_round_view()) || !(view->flags & AE_ROUND_WIRE_ZONE))
		return FALSE;
	if (view->phase != AE_ROUND_PHASE_PRE && view->phase != AE_ROUND_PHASE_LIVE)
		return FALSE;
	if (!(view->zone_radius > 0))
		return FALSE;
	memset(out, 0, sizeof(*out));
	out->centre_x = ae_round_wire_real(view->zone_centre[0]);
	out->centre_y = ae_round_wire_real(view->zone_centre[1]);
	out->centre_z = ae_round_wire_real(view->zone_centre[2]);
	out->plan.start_radius = (real)view->zone_radius / 16.0f;
	out->plan.mid_radius = out->plan.start_radius * AE_ZONE_MID_FRACTION;
	out->plan.wait_seconds = view->zone_wait_seconds;
	out->plan.wait_2_seconds = AE_ZONE_WAIT_2_SECONDS;
	out->plan.closing_seconds = view->zone_closing_seconds;
	out->plan.closing_2_seconds = view->zone_closing_2_seconds;
	out->running = (boolean)(view->phase == AE_ROUND_PHASE_LIVE && view->zone_start != NONE && now >= view->zone_start);
	out->elapsed = out->running ? now - view->zone_start : 0;
	out->phase = ae_zone_phase_at(&out->plan, out->elapsed);
	out->radius = ae_zone_radius_at(&out->plan, out->elapsed);
	out->next_radius = ae_zone_next_radius(&out->plan, out->elapsed);
	if (out->phase == AE_ZONE_PHASE_CLOSED)
		out->next_radius = out->radius;
	return TRUE;
}
