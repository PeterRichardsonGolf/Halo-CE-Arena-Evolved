/*
AE_GRENADE_FX_STALL.C

The grenade catalogue's STALL module (ae_grenade_fx.h): every vehicle in the blast's reach is stalled for the row's
stall seconds at its strength (full to the full radius, the edge seconds at the outer edge). Re-hits refresh, never
stack, and a vehicle whose stall ended is immune for the row's immunity seconds. The stall itself is played by every
machine in its own update (ae_grenades_stall_controls, the vehicle's brake), from the host's message.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "objects/objects.h"
#include "objects/object_types.h"

#include "ae_grenade_fx.h"

void ae_grenade_fx_stall_apply(struct ae_grenade_blast const *blast)
{
	struct ae_grenade_numbers const *numbers = &blast->entry->numbers;
	short index;

	for (index = 0; index < blast->target_count; index++)
	{
		long ticks, left;

		if (blast->targets[index].type != _object_type_vehicle)
			continue;
		ticks = ae_grenade_effect_ticks(numbers->stall_seconds, numbers->stall_edge_seconds, numbers->edge_fraction,
			blast->targets[index].strength);
		left = ae_grenade_fx_stall_hit(blast->targets[index].object_index, ticks, numbers->immunity_seconds);
		error(_error_silent, "grenades: %s stalls vehicle %08lX: strength %.2f at %.2f wu, %ld ticks (%ld left%s)",
			blast->entry->display, (unsigned long)blast->targets[index].object_index,
			(double)blast->targets[index].strength, (double)blast->targets[index].distance, ticks, left,
			left < ticks ? ", immune or already longer" : "");
	}
}
