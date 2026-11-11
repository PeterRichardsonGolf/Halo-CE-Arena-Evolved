/*
AE_WEAPON_IDS.C

The weapon ids the consumers ask (ae_weapon_ids.h).
*/

#include "cseries.h"
#include "errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "main/console.h"
#include "objects/objects.h"

#include "ae_weapon_cache.h"
#include "ae_weapon_ids.h"
#include "ae_weapon_pack.h"
#include "ae_weapon_selection_game.h"
#include "ae_weapon_slots.h"

extern struct game_variant game_variant_global;

unsigned int ae_weapon_id_of_definition(
	long definition_index)
{
	/* (the pack API answers by id for a cache: ae_weapon_pack_definition_slot maps a weapon's or a view's definition; the
	ids 1-24 only, the slots of the facade, which slot-indexed arrays rely on) */
	return definition_index == NONE ? 0 : (unsigned int)ae_weapon_pack_definition_slot(definition_index);
}

unsigned int ae_weapon_id_of_definition_any(
	long definition_index)
{
	unsigned int id = 0, config = 0;

	if (definition_index == NONE)
		return 0;
	if (ae_weapon_cache_pack())
		return ae_weapon_cache_of_definition(definition_index, &id, &config) ? id : 0;
	return ae_weapon_id_of_definition(definition_index);
}

struct ae_weapon_registry_row const *ae_weapon_id_row_of_definition(
	long definition_index)
{
	/* (the full id: the registry is searched by id, no array is indexed, so the Protomag (25) has its row too) */
	unsigned int id = ae_weapon_id_of_definition_any(definition_index);

	return id ? ae_weapon_registry_row(id) : NULL;
}

long ae_weapon_id_definition(
	unsigned int id)
{
	if (ae_weapon_cache_pack())
		return ae_weapon_cache_definition(id, 0);
	return id >= 1 && id <= AE_WEAPON_SLOT_COUNT ? ae_weapon_pack_weapon_definition((short)id, 0) : NONE;
}

boolean ae_weapon_id_available(
	unsigned int id)
{
	if (ae_weapon_cache_pack())
		return ae_weapon_cache_available(id);
	return id >= 1 && id <= AE_WEAPON_SLOT_COUNT && ae_weapon_pack_slot_added((short)id);
}

boolean ae_weapon_id_played(
	unsigned int id)
{
	return id >= 1 && id <= AE_WEAPON_REGISTRY_LEGACY_LAST &&
		(ae_weapon_pack_played_mask() & AE_WEAPON_SLOT_BIT(id)) != 0;
}

void ae_weapon_ids_predict_named(
	void)
{
	unsigned int named = game_engine_weapon_pack_mask(), id;

	/* (the legacy bytes name ids 1-16; the cache's other weapons come with the AE WEAPONS sets, which fill the map's spots) */
	for (id = 1; id <= AE_WEAPON_REGISTRY_LEGACY_LAST; id++)
		if (named & AE_WEAPON_SLOT_BIT(id))
			object_definition_predict(ae_weapon_id_definition(id));
}

void ae_weapon_ids_unavailable_lines(
	void)
{
	char unavailable[256];
	unsigned int id;

	for (id = 1; id <= AE_WEAPON_REGISTRY_LEGACY_LAST; id++)
	{
		if (ae_weapon_pack_unavailable_line((short)id, unavailable, sizeof(unavailable)))
		{
			error(_error_silent, "weapon pack: %s", unavailable);
			console_printf(FALSE, "%s", unavailable);
		}
	}
}

long ae_weapon_ids_give_definition(
	unsigned int id,
	char const *what)
{
	long definition_index = id ? ae_weapon_id_definition(id) : NONE;

	if (definition_index != NONE && ae_weapon_selection_guard(&game_variant_global, game_variant_options_get(), what))
		return NONE;
	return definition_index;
}

boolean ae_weapon_ids_is_listed(
	long definition_index)
{
	unsigned int id, config;

	if (!ae_weapon_cache_pack() || !ae_weapon_cache_is_own(definition_index))
		return TRUE;
	return ae_weapon_cache_of_definition(definition_index, &id, &config) && !config;
}
