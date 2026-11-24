/*
AE_PACK_SPAWNS.C

Weapon packs' weapons on the map's item spots (ae_pack_spawns.h).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "errors.h"
#include "tag_files/tag_groups.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "objects/object_definitions.h"
#include "objects/object_types.h"

#include "main/main.h"
#include "tag_files/tag_files.h"

#include "ae_glue_social_game.h"
#include "ae_layout.h"
#include "ae_pack_spawns.h"
#include "ae_pickup_rules.h"
#include "ae_weapon_cache.h"
#include "ae_weapon_registry.h"
#include "ae_weapon_selection_game.h"
#include "ae_weapon_ids.h"
#include "ae_weapon_pack.h"
#include "ae_weapon_slots.h"

extern struct game_variant game_variant_global; /* (game.c's) */
extern struct game_variant global_variant; /* (game_engine.c's: the running game's) */

/* the replacements a map logs, one line each (the test's proof that the spots spawn the packs' weapons) */
#define LOGGED_MAXIMUM 48

static long logged;

/* (AE PICKUPS) each assault rifle spot's turn in the map's order (ae_pickup_rules.h), -1 before the spot first spawns
one; the next turn. Spots past the table take their index's turn */
#define PICKUP_SPOTS 512

static short pickup_turn[PICKUP_SPOTS];
static short pickup_next_turn;
static int pickup_turns_set;

void ae_pack_spawns_new_map(void)
{
	logged = 0;
	memset(pickup_turn, 0xff, sizeof(pickup_turn));
	pickup_next_turn = 0;
	pickup_turns_set = 1;
}

/* the loaded map's name: a network game's, else the one loaded */
static char const *pickup_map_name(void)
{
	char const *name = main_get_multiplayer_map_name();

	return name && name[0] ? name : main_get_map_name();
}

/* whether this game lays AE's weapons on the map's spots (ae_pickup_rules.h): the cache's game (its gametype names the
cache, so every machine has it) whose gametype has MAP SPAWNS (CREATE GAME's PACK WEAPONS ON MAP, a server's ae_pickups)
or a tournament game of AE's weapons; never on a Workshop map */
static boolean pickups_on(void)
{
	char const *map = pickup_map_name();

	if (!ae_weapon_cache_pack() || (map && strstr(map, "@ws")))
		return FALSE;
	/* (the bit itself: game_engine_pack_map_spawns also wants a legacy pack named, and a cache game may name none, e.g. a
	server gametype whose start is the protomag) */
	if (!game_variant_pack_map_spawns(global_variant.universal_variant.pack_loadout) && !ae_social_game_pickups())
		return FALSE;
	return ae_weapon_selection_game_uses_cache(&game_variant_global, game_variant_options_get());
}

/* the AE weapon a map's weapon spawns as (AE PICKUPS), NONE for its own */
static long pickup_definition(long equipment_index, long definition_index, short stock)
{
	char const *map = pickup_map_name();
	int halo2 = ae_pickup_map_is_halo2(map);
	enum ae_pickup_kind kind;
	unsigned int turn = 0, id;
	long pickup;

	/* (one of the cache's own weapons already: the BR and needler by the map's references) */
	if (ae_weapon_cache_is_own(definition_index))
		return NONE;
	kind = ae_pickup_kind_of(stock, tag_get_name(definition_index), halo2);
	if (kind == AE_PICKUP_KIND_ASSAULT_RIFLE)
	{
		if (!pickup_turns_set)
			ae_pack_spawns_new_map();
		if (equipment_index >= 0 && equipment_index < PICKUP_SPOTS)
		{
			if (pickup_turn[equipment_index] < 0)
				pickup_turn[equipment_index] = pickup_next_turn++;
			turn = (unsigned int)pickup_turn[equipment_index];
		}
		else
			turn = (unsigned int)(equipment_index < 0 ? 0 : equipment_index);
	}
	id = ae_pickup_weapon(kind, turn);
	if (!id)
		return NONE;
	pickup = ae_weapon_ids_give_definition(id, "an AE pickup");
	if (pickup != NONE && logged < LOGGED_MAXIMUM)
	{
		struct ae_weapon_registry_row const *row = ae_weapon_registry_row(id);

		logged++;
		error(_error_silent, "weapon pack: map item %ld spawns the %s (id %u) in place of its %s (AE pickups%s)",
			equipment_index, row ? row->display : "?", id, tag_get_name(definition_index), halo2 ? ", a Halo 2 map" : "");
	}
	return pickup;
}

/* the stock weapon (ae_weapon_slots.h) a weapon definition is: the pistol, the assault rifle, the shotgun, the sniper
rifle, the rocket launcher (the registry's stock_fallback words), or none. MAP SPAWNS take only the first two
(ae_weapon_slots_spawn); a layout's pack weapon takes the spot of its row's own fallback */
static short stock_of(long definition_index)
{
	if (definition_index == NONE)
		return AE_WEAPON_STOCK_NONE;
	return game_engine_weapon_is_pistol(definition_index) ? AE_WEAPON_STOCK_PISTOL :
		game_engine_weapon_is_assault_rifle(definition_index) ? AE_WEAPON_STOCK_ASSAULT_RIFLE :
		game_engine_weapon_is_shotgun(definition_index) ? AE_WEAPON_STOCK_SHOTGUN :
		game_engine_weapon_is_sniper_rifle(definition_index) ? AE_WEAPON_STOCK_SNIPER_RIFLE :
		game_engine_weapon_is_rocket_launcher(definition_index) ? AE_WEAPON_STOCK_ROCKET_LAUNCHER : AE_WEAPON_STOCK_NONE;
}

long ae_pack_spawn_definition(long equipment_index, long definition_index)
{
	/* (the gametype's packs: DEV MODE's others are on the map for give and the wheel only) */
	unsigned int added = ae_weapon_pack_played_mask();
	short layout_slot, slot = AE_WEAPON_SLOT_NONE, stock;
	boolean own_fallback;
	long pack_definition, remapped;
	boolean pickups;
	char const *why;

	if (definition_index == NONE || object_definition_get(definition_index)->object.type != _object_type_weapon)
		return definition_index;
	pickups = pickups_on();
	if (!added && !pickups)
		return definition_index;
	/* (what the spot spawns once the weapon set has had its say: the remap draws no random number for a weapon) */
	remapped = game_engine_remap_object_definition(definition_index);
	stock = stock_of(remapped);
	layout_slot = ae_layout_item_pack_slot(equipment_index);
	/* (0.9.3 AE PICKUPS: the cache's table, where no layout line put a pack weapon on the spot; a Halo 2 map's own magnum
	and brute shot are no stock weapon, so they are asked before the stock test) */
	if (pickups && layout_slot == AE_WEAPON_SLOT_NONE)
	{
		short own_stock = stock_of(definition_index);

		/* (the spot's own weapon is classified, and swapped only where the weapon set left it what it was: a SNIPING set's
		snipers stay the set's) */
		if (!ae_pickup_set_keeps(own_stock, stock, remapped == definition_index))
			return definition_index;
		pack_definition = pickup_definition(equipment_index, definition_index, own_stock);
		return pack_definition != NONE ? pack_definition : definition_index;
	}
	/* (AE PICKUPS: a layout line's pack weapon spawns whether or not the gametype names it, as the table's do: a demake's
	layout puts back Halo 2's brute shots and battle rifles on the spots it gave a CE stand-in) */
	/* (a line naming its own fallback, item brute_shot like <n>: its stand-in is another kind, the pack's weapon spawns
	there where the weapon set left the stand-in as it was) */
	own_fallback = ae_layout_item_pack_own_fallback(equipment_index);
	if (pickups && (own_fallback ? remapped == definition_index : stock == ae_weapon_slots_fallback(layout_slot)))
	{
		pack_definition = ae_weapon_ids_give_definition((unsigned int)layout_slot, "a layout weapon");
		if (pack_definition != NONE)
		{
			if (logged < LOGGED_MAXIMUM)
			{
				logged++;
				error(_error_silent, "weapon pack: map item %ld spawns the %s (slot %d) in place of its %s (layout, AE pickups)",
					equipment_index, ae_weapon_slots_name(layout_slot), (int)layout_slot,
					own_fallback ? tag_get_name(definition_index) : ae_weapon_slots_fallback_word(layout_slot));
			}
			return pack_definition;
		}
	}
	if (!added || stock == AE_WEAPON_STOCK_NONE)
		return definition_index;
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
	/* (by the weapon's cache id; the no-cache guard refuses a cache weapon in a network game that does not use the cache) */
	pack_definition = ae_weapon_ids_give_definition((unsigned int)slot, layout_slot != AE_WEAPON_SLOT_NONE ? "a layout weapon" : "a map spawn");
	if (pack_definition == NONE)
		return definition_index;
	if (logged < LOGGED_MAXIMUM)
	{
		logged++;
		error(_error_silent, "weapon pack: map item %ld spawns the %s (slot %d) in place of its %s (%s)", equipment_index,
			ae_weapon_slots_name(slot), (int)slot, stock == AE_WEAPON_STOCK_PISTOL ? "pistol" : stock == AE_WEAPON_STOCK_ASSAULT_RIFLE ? "assault rifle" :
			ae_weapon_slots_fallback_word(slot), why);
	}
	return pack_definition;
}

int ae_pack_spawn_timer(long equipment_index, long definition_index, char *label, long size)
{
	short slot = ae_layout_item_pack_slot(equipment_index);
	struct ae_weapon_registry_row const *row;

	/* (only a layout line's pack weapon: AE PICKUPS' own swaps keep the spot's kind, and asking them would draw from
	their rotation) */
	if (slot == AE_WEAPON_SLOT_NONE || definition_index == NONE ||
		ae_pack_spawn_definition(equipment_index, definition_index) == definition_index)
	{
		return 0;
	}
	row = ae_weapon_registry_row_of_slot(slot);
	if (!row)
		return 0;
	if (!row->power)
		return -1;
	if (label && size > 0)
		snprintf(label, (size_t)size, "%s", row->feed_word);
	return 1;
}
