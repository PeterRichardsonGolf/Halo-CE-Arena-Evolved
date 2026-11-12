/*
AE_GRENADE_FX_POWERUP_SHORT.C

The grenade catalogue's POWERUP_SHORT module (ae_grenade_fx.h): a powerup item lying in the blast's reach (overshield,
active camouflage, double speed, full-spectrum vision) is shorted: the host deletes it, and the deletion reaches the
clients as any does. It comes back at its spawn's next respawn, as one picked up does. Halo CE has no powerup a player
carries, so players' camouflage, overshield, shields and health are not touched; nor are grenades on the ground,
weapons, the bomb, flags or projectiles.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "items/equipment.h"
#include "items/equipment_definitions.h"
#include "items/items.h"
#include "objects/objects.h"
#include "objects/object_types.h"
#include "tag_files/tag_files.h"

#include "ae_grenade_fx.h"

char *tag_get_name(long tag_index);

static boolean powerup(short type)
{
	return type == _equipment_powerup_overshield || type == _equipment_powerup_active_camouflage ||
		type == _equipment_powerup_double_speed || type == _equipment_powerup_full_spectrum_vision;
}

void ae_grenade_fx_powerup_short_apply(struct ae_grenade_blast const *blast)
{
	short index;

	for (index = 0; index < blast->target_count; index++)
	{
		long item_index = blast->targets[index].object_index;
		struct object_datum *object;
		struct equipment_definition *definition;

		if (blast->targets[index].type != _object_type_equipment ||
			!(object = object_try_and_get_and_verify_type(item_index, _object_mask_equipment)))
		{
			continue;
		}
		definition = equipment_definition_get(object->definition_index);
		if (!powerup(definition->equipment.powerup_type))
			continue;
		error(_error_silent, "grenades: %s shorts powerup %08lX (%s) at %.2f wu: deleted, back at its next respawn",
			blast->entry->display, (unsigned long)item_index, tag_get_name(object->definition_index),
			(double)blast->targets[index].distance);
		object_delete(item_index);
	}
}
