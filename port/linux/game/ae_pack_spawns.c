/*
AE_PACK_SPAWNS.C

Weapon packs' weapons on the map's item spots (ae_pack_spawns.h).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "errors.h"
#include "tag_files/tag_groups.h"
#include "game/game_engine.h"
#include "objects/object_definitions.h"
#include "objects/object_types.h"

#include "ae_layout.h"
#include "ae_pack_spawns.h"
#include "ae_weapon_pack.h"
#include "ae_weapon_slots.h"

/* the replacements a map logs, one line each (the test's proof that the spots spawn the packs' weapons) */
#define LOGGED_MAXIMUM 48

static long logged;

void ae_pack_spawns_new_map(void)
{
	logged = 0;
}

/* the stock weapon (ae_weapon_slots.h) a weapon definition is: the pistol, the assault rifle, or none */
static short stock_of(long definition_index)
{
	if (definition_index == NONE)
		return AE_WEAPON_STOCK_NONE;
	return game_engine_weapon_is_pistol(definition_index) ? AE_WEAPON_STOCK_PISTOL :
		game_engine_weapon_is_assault_rifle(definition_index) ? AE_WEAPON_STOCK_ASSAULT_RIFLE : AE_WEAPON_STOCK_NONE;
}

long ae_pack_spawn_definition(long equipment_index, long definition_index)
{
	unsigned int added = ae_weapon_pack_added_mask();
	short layout_slot, slot = AE_WEAPON_SLOT_NONE, stock;
	long pack_definition;
	char const *why;

	if (!added || definition_index == NONE || object_definition_get(definition_index)->object.type != _object_type_weapon)
		return definition_index;
	/* (what the spot spawns once the weapon set has had its say: the remap draws no random number for a weapon) */
	stock = stock_of(game_engine_remap_object_definition(definition_index));
	if (stock == AE_WEAPON_STOCK_NONE)
		return definition_index;
	layout_slot = ae_layout_item_pack_slot(equipment_index);
	if (layout_slot != AE_WEAPON_SLOT_NONE)
	{
		/* (a layout's pack weapon: its own pack, else the fallback the spot was made from) */
		if ((added & AE_WEAPON_SLOT_BIT(layout_slot)) && stock == ae_weapon_slots_fallback(layout_slot))
			slot = layout_slot;
		why = "layout";
	}
	else
	{
		if (game_engine_pack_map_spawns())
			slot = ae_weapon_slots_spawn(stock, added);
		why = "map spawns";
	}
	if (slot == AE_WEAPON_SLOT_NONE)
		return definition_index;
	pack_definition = ae_weapon_pack_weapon_definition(slot, 0);
	if (pack_definition == NONE)
		return definition_index;
	if (logged < LOGGED_MAXIMUM)
	{
		logged++;
		error(_error_silent, "weapon pack: map item %ld spawns the %s (slot %d) in place of its %s (%s)", equipment_index,
			ae_weapon_slots_name(slot), (int)slot, stock == AE_WEAPON_STOCK_PISTOL ? "pistol" : "assault rifle", why);
	}
	return pack_definition;
}
