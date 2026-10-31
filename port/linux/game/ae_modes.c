/*
AE_MODES.C

Arena Evolved's modes of Slayer on OpenCE's gametypes (ae_modes.h), the
first of SPCLib's (Chalwk's SAPP scripts, MIT; their rules and numbers, no
code), and the built-in gametypes that play them, after OpenCE's in the
OPENCE bank:

- ONE IN THE CHAMBER (FFA, 3 lives, the last player standing wins): a pistol
  with one round, no grenades, nothing on the map, nothing picked up; a
  player's bullet kills at once; a kill gives a round (ae_modes_rules.h).
- FRAG NATION (FFA): fists, two frags and two plasmas, the map's grenades; a
  grenade kill gives grenades back.
- MELEE ONLY (FFA): fists, no grenades, nothing on the map.
- NEEDLES: no mode, only options (a needler with BOTTOMLESS CLIP, nothing on
  the map).
- SNOWBALL FIGHT (FFA; owner 2026-10-28, Frag Nation with plasmas only): fists
  and four plasma grenades, none on the map; a plasma kill gives a plasma back.

The fists are ChupathingyCE's melee-only Bomb's (game_engine_bomb.c): the
multiplayer weapon list's pistol, given unseen and empty, so its pistol whip
is the blow and its first person arms are seen; game_engine_bomb_hides_weapon
asks ae_modes_hides_weapon, so every machine hides it, draws a bare
crosshair, never lets it shoot, and the host deletes one left lying.

Only the host gives weapons, rounds and grenades and decides a kill: a
guest's weapons, rounds and grenades come with its inventory
(network_objects.c), so a stock OpenCE guest plays them as AE's do. Each
mode is Slayer's game engine with some of its functions wrapped, as OpenCE's
Gun Game is (gun_game.c).
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_engine_playlist.h"
#include "game/game_engine_slayer.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "items/weapons.h"
#include "items/weapon_definitions.h"
#include "networking/network_game_globals.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "tag_files/tag_files.h"
#include "text/unicode.h"
#include "units/units.h"
#include "game_modes.h"
#include "ae_modes.h"
#include "ae_modes_rules.h"
#include "ae_weapon_rules.h"

#include <string.h>

/* game_engine.c's values of the variant's (game_modes.c's) */
enum
{
	AE_MODES_WEAPON_SET_NO_GRENADES = 10,
	AE_MODES_VEHICLE_SET_NONE = 1
};

/* One in the Chamber's lives, and Frag Nation's grenades at a spawn */
enum
{
	AE_MODES_CHAMBER_LIVES = 3,
	AE_MODES_FRAG_NATION_FRAGS = 2,
	AE_MODES_FRAG_NATION_PLASMAS = 2,
	AE_MODES_SNOWBALL_PLASMAS = 4
};

enum
{
	_ae_gametype_one_in_the_chamber = 0,
	_ae_gametype_frag_nation,
	_ae_gametype_melee_only,
	_ae_gametype_needles,
	/* (after the others: a built-in's place in the list stays the same) */
	_ae_gametype_snowball_fight,
	NUMBER_OF_AE_GAMETYPES
};

static struct
{
	char const *name;
	wchar_t const *display_name;
	/* (11 characters at most) */
	wchar_t const *variant_name;
} const ae_modes_gametypes[NUMBER_OF_AE_GAMETYPES] =
{
	{ "one_in_the_chamber", L"One in the Chamber", L"One Chamber" },
	{ "frag_nation", L"Frag Nation", L"Frag Nation" },
	{ "melee_only", L"Melee Only", L"Melee Only" },
	{ "needles", L"Needles", L"Needles" },
	{ "snowball_fight", L"Snowball Fight", L"Snowball" },
};

static struct
{
	struct game_engine slayer;
	struct game_engine chamber;
	struct game_engine frag_nation;
	struct game_engine melee_only;
	/* the kill being recorded's damage (damage.c), NONE */
	long kill_damage_definition_index;
	/* the host is giving a weapon itself (its pickup allowed) */
	boolean equipping;
} ae_modes = { { 0 }, { 0 }, { 0 }, { 0 }, NONE, FALSE };

/* ---- the built-in gametypes */

short ae_modes_gametype_count(
	void)
{
	return NUMBER_OF_AE_GAMETYPES;
}

wchar_t const *ae_modes_gametype_name(
	short index)
{
	return VALID_INDEX(index, NUMBER_OF_AE_GAMETYPES) ? ae_modes_gametypes[index].display_name : L"";
}

short ae_modes_gametype_of_stored_name(
	wchar_t const *stored)
{
	short index;

	for (index = 0; stored && index < NUMBER_OF_AE_GAMETYPES; index++)
	{
		wchar_t const *name = ae_modes_gametypes[index].variant_name;
		short at = 0;

		/* (the whole of the stored name, 11 characters at most; wide strings are 16-bit: no wcsncmp) */
		while (at < 11 && stored[at] && stored[at] == name[at])
			at++;
		if (at == 11 || stored[at] == name[at])
			return index;
	}
	return NONE;
}

boolean ae_modes_gametype(
	short index,
	struct game_variant *variant,
	struct game_variant_options *options)
{
	struct game_variant temporary;

	if (!VALID_INDEX(index, NUMBER_OF_AE_GAMETYPES))
		return FALSE;
	*variant = *build_game_variant_slayer(&temporary);
	game_variant_options_default(variant, options);
	variant->universal_variant.vehicle_set = AE_MODES_VEHICLE_SET_NONE;
	options->loadout = _loadout_custom;
	options->primary_weapon = _loadout_weapon_none;
	options->secondary_weapon = _loadout_weapon_none;
	options->no_map_weapons = TRUE;
	switch (index)
	{
	case _ae_gametype_one_in_the_chamber:
		/* (the pistol is the mode's to give: ae_modes_chamber_equip) */
		options->mode = AE_GAME_MODE_ONE_IN_THE_CHAMBER;
		variant->universal_variant.weapon_set = AE_MODES_WEAPON_SET_NO_GRENADES;
		variant->universal_variant.lives = AE_MODES_CHAMBER_LIVES;
		break;
	case _ae_gametype_frag_nation:
		/* (its grenades the mode's, the map's on: ae_modes_grenades_postspawn) */
		options->mode = AE_GAME_MODE_FRAG_NATION;
		break;
	case _ae_gametype_melee_only:
		options->mode = AE_GAME_MODE_MELEE_ONLY;
		variant->universal_variant.weapon_set = AE_MODES_WEAPON_SET_NO_GRENADES;
		break;
	case _ae_gametype_needles:
		variant->universal_variant.weapon_set = AE_MODES_WEAPON_SET_NO_GRENADES;
		options->primary_weapon = _loadout_weapon_needler;
		variant->universal_variant.pack_loadout = ae_weapon_ammo_byte_set(variant->universal_variant.pack_loadout,
			TRUE, TRUE);
		break;
	case _ae_gametype_snowball_fight:
		/* (its plasmas the mode's, none on the map: ae_modes_grenades_postspawn) */
		options->mode = AE_GAME_MODE_SNOWBALL_FIGHT;
		variant->universal_variant.weapon_set = AE_MODES_WEAPON_SET_NO_GRENADES;
		break;
	}
	options->vehicle_set[0] = options->vehicle_set[1] = (byte)variant->universal_variant.vehicle_set;
	game_modes_options_sanitize(options);
	csmemset(variant->human_readable_game_description, 0, sizeof(variant->human_readable_game_description));
	ustrncpy(variant->human_readable_game_description, ae_modes_gametypes[index].variant_name,
		NUMBEROF(variant->human_readable_game_description) - 1);
	return TRUE;
}

boolean ae_modes_gametype_by_name(
	char const *name,
	struct game_variant *variant,
	struct game_variant_options *options)
{
	short index;

	for (index = 0; index < NUMBER_OF_AE_GAMETYPES; index++)
	{
		if (!strcmp(name, ae_modes_gametypes[index].name))
			return ae_modes_gametype(index, variant, options);
	}
	return FALSE;
}

/* ---- a gametype's options */

boolean ae_modes_known(
	short mode)
{
	return mode >= AE_GAME_MODE_FIRST && mode < AE_GAME_MODE_END;
}

short ae_modes_variant_mode(
	struct game_variant const *variant,
	struct game_variant_options const *options)
{
	if (!variant || !options || variant->game_engine_index != game_engine_slayer || !ae_modes_known(options->mode))
		return _game_mode_none;
	return options->mode;
}

wchar_t const *ae_modes_kind_name(
	short mode,
	boolean hardcore,
	boolean abbreviated)
{
	static wchar_t const *const names[AE_GAME_MODE_END - AE_GAME_MODE_FIRST][2][2] =
	{
		{ { L"ONE IN THE CHAMBER", L"1 CHAMBER" }, { L"HARDCORE ONE IN THE CHAMBER", L"HC 1 CHAMBER" } },
		{ { L"FRAG NATION", L"FRAG NATION" }, { L"HARDCORE FRAG NATION", L"HC FRAG NATION" } },
		{ { L"MELEE ONLY", L"MELEE ONLY" }, { L"HARDCORE MELEE ONLY", L"HC MELEE ONLY" } },
		{ { L"SNOWBALL FIGHT", L"SNOWBALL" }, { L"HARDCORE SNOWBALL FIGHT", L"HC SNOWBALL" } },
	};

	if (!ae_modes_known(mode))
		return NULL;
	return names[mode - AE_GAME_MODE_FIRST][hardcore != FALSE][abbreviated != FALSE];
}

/* ---- the game being played */

/* a player's living unit, the host's to change, NULL */
static struct unit_datum *ae_modes_host_unit(
	long player_index)
{
	struct player_datum *player;
	struct unit_datum *unit;

	if (network_game_distributed_client() || player_index == NONE)
		return NULL;
	player = player_try_and_get(player_index);
	if (!player || player->quit_out_of_game || player->unit_index == NONE)
		return NULL;
	unit = unit_try_and_get(player->unit_index);
	if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		return NULL;
	return unit;
}

/* nothing picked up but what the host gives (its own weapons a unit holds:
readying one asks too) */
static boolean ae_modes_allow_pick_up(
	long unit_index,
	long weapon_index)
{
	struct unit_datum *unit = unit_try_and_get(unit_index);
	short slot;

	if (ae_modes.equipping)
		return TRUE;
	for (slot = 0; unit && slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		if (unit->unit.weapon_object_indices[slot] == weapon_index)
			return TRUE;
	}
	return FALSE;
}

/* the host gives a spawned player a weapon, the loadout's or a definition's
(its pickup the mode's own), NULL */
static struct weapon_datum *ae_modes_give_weapon(
	long player_index,
	short loadout_weapon,
	long definition_index)
{
	struct unit_datum *unit = ae_modes_host_unit(player_index);
	long weapon_index;
	struct object_placement_data placement;

	if (loadout_weapon != NONE)
		definition_index = game_engine_loadout_weapon_definition((byte)loadout_weapon);
	if (!unit || definition_index == NONE)
		return NULL;
	object_placement_data_new(&placement, definition_index, NONE);
	weapon_index = object_new(&placement);
	if (weapon_index == NONE)
		return NULL;
	ae_modes.equipping = TRUE;
	if (!unit_add_weapon_to_inventory(player_get(player_index)->unit_index, weapon_index, 2))
	{
		object_delete(weapon_index);
		weapon_index = NONE;
	}
	ae_modes.equipping = FALSE;
	return weapon_index != NONE ? weapon_try_and_get(weapon_index) : NULL;
}

/* whether the game's mode is played with fists */
static boolean ae_modes_fists_mode(
	void)
{
	short mode = game_modes_mode();

	return mode == AE_GAME_MODE_FRAG_NATION || mode == AE_GAME_MODE_MELEE_ONLY || mode == AE_GAME_MODE_SNOWBALL_FIGHT;
}

/* the fists' weapon: the multiplayer weapon list's pistol (game_engine.c's
_weapon_list_pistol, as Chupa's Bomb takes it), if it has first person
arms; NONE */
static long ae_modes_fist_definition(
	void)
{
	long definition_index = list_index_to_weapon_definition_index(4);

	if (definition_index == NONE ||
		weapon_definition_get(definition_index)->weapon.interface_definition.first_person_animations.index == NONE)
	{
		return NONE;
	}
	return definition_index;
}

boolean ae_modes_hides_weapon(
	long definition_index)
{
	return definition_index != NONE && ae_modes_fists_mode() && definition_index == ae_modes_fist_definition();
}

/* the host gives a spawned player the fists, empty */
static void ae_modes_give_fists(
	long player_index)
{
	long definition_index = ae_modes_fist_definition();
	struct weapon_datum *weapon;
	short magazine;

	if (definition_index == NONE || !ae_modes_host_unit(player_index))
		return;
	weapon = ae_modes_give_weapon(player_index, NONE, definition_index);
	for (magazine = 0; weapon && magazine < (short)NUMBEROF(weapon->weapon.magazines); magazine++)
	{
		weapon->weapon.magazines[magazine].rounds_total = 0;
		weapon->weapon.magazines[magazine].rounds_loaded = 0;
	}
}

/* each tick: the fists of the dead (or any dropped) gone, the host's word
deleting a client's (as Chupa's Bomb does) */
static void ae_modes_fists_update(
	void)
{
	long definition_index = ae_modes_fist_definition();
	struct object_iterator objects;

	if (definition_index == NONE || network_game_distributed_client())
		return;
	object_iterator_new(&objects, _object_mask_weapon, 0);
	while (object_iterator_next(&objects))
	{
		struct weapon_datum *weapon = weapon_get(objects.index);

		if (weapon->definition_index == definition_index &&
			weapon->object.parent_object_index == NONE &&
			TEST_FLAG(weapon->object.flags, _object_connected_to_map_bit) &&
			!TEST_FLAG(weapon->item.flags, _item_attached_to_unit_bit))
		{
			object_delete(objects.index);
		}
	}
}

static void ae_modes_fists_engine_update(
	void)
{
	if (ae_modes.slayer.update)
		ae_modes.slayer.update();
	ae_modes_fists_update();
}

/* One in the Chamber: the pistol, with one round and none carried */
static void ae_modes_chamber_equip(
	long player_index)
{
	struct weapon_datum *weapon = ae_modes_give_weapon(player_index, _loadout_weapon_pistol, NONE);

	if (weapon)
	{
		weapon->weapon.magazines[0].rounds_loaded = 1;
		weapon->weapon.magazines[0].rounds_total = 0;
	}
}

/* One in the Chamber: a kill's round, into the killer's pistol (the first of
their weapons with a magazine) */
static void ae_modes_chamber_round(
	long killing_player_index)
{
	struct unit_datum *unit = ae_modes_host_unit(killing_player_index);
	short slot;

	for (slot = 0; unit && slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		struct weapon_datum *weapon = unit->unit.weapon_object_indices[slot] != NONE ?
			weapon_try_and_get(unit->unit.weapon_object_indices[slot]) : NULL;
		struct weapon_definition *definition;

		if (!weapon)
			continue;
		definition = weapon_definition_get(weapon->definition_index);
		if (definition->weapon.magazines.count <= 0)
			continue;
		ae_modes_rules_round_for_kill(&weapon->weapon.magazines[0].rounds_loaded,
			&weapon->weapon.magazines[0].rounds_total,
			TAG_BLOCK_GET_ELEMENT(&definition->weapon.magazines, 0, struct weapon_magazine_definition)->rounds_loaded_maximum);
		error(_error_silent, "ae modes: one in the chamber: player %ld's kill gives a round (%d loaded, %d carried)",
			(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(killing_player_index), (int)weapon->weapon.magazines[0].rounds_loaded,
			(int)weapon->weapon.magazines[0].rounds_total);
		return;
	}
}

static void ae_modes_chamber_player_killed_player(
	long killing_player_index,
	long killing_object_index,
	long dead_player_index,
	boolean friendly_fire)
{
	ae_modes.slayer.player_killed_player(killing_player_index, killing_object_index, dead_player_index,
		friendly_fire);
	if (!friendly_fire && killing_player_index != NONE && killing_player_index != dead_player_index)
		ae_modes_chamber_round(killing_player_index);
}

/* the most grenades of a kind a unit carries (the globals'), 0 for none */
static short ae_modes_grenade_maximum(
	short grenade_type)
{
	struct game_globals *globals = scenario_get_game_globals();

	if (!globals || !VALID_INDEX(grenade_type, globals->grenades.count))
		return 0;
	return TAG_BLOCK_GET_ELEMENT(&globals->grenades, grenade_type, struct game_globals_grenade)->maximum_count;
}

/* a unit's grenades of a kind, given more, up to the most it carries */
static void ae_modes_give_grenades(
	struct unit_datum *unit,
	short grenade_type,
	short count)
{
	if (count > 0 && VALID_INDEX(grenade_type, NUMBER_OF_UNIT_GRENADE_TYPES))
	{
		unit->unit.grenade_counts[grenade_type] = (char)ae_modes_rules_add_capped(
			unit->unit.grenade_counts[grenade_type], count, ae_modes_grenade_maximum(grenade_type));
	}
}

/* Frag Nation's and Snowball Fight's spawn: fists and the mode's grenades, only those */
static void ae_modes_grenades_postspawn(
	long player_index,
	short frags,
	short plasmas,
	char const *mode_name)
{
	struct unit_datum *unit = ae_modes_host_unit(player_index);

	if (!unit)
		return;
	ae_modes_give_fists(player_index);
	unit->unit.grenade_counts[_unit_grenade_human_fragmentation] = 0;
	unit->unit.grenade_counts[_unit_grenade_covenant_plasma] = 0;
	ae_modes_give_grenades(unit, _unit_grenade_human_fragmentation, frags);
	ae_modes_give_grenades(unit, _unit_grenade_covenant_plasma, plasmas);
	error(_error_silent, "ae modes: %s: player %ld spawns with %d frag, %d plasma", mode_name,
		(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), (int)unit->unit.grenade_counts[_unit_grenade_human_fragmentation],
		(int)unit->unit.grenade_counts[_unit_grenade_covenant_plasma]);
}

static void ae_modes_frag_nation_player_killed_player(
	long killing_player_index,
	long killing_object_index,
	long dead_player_index,
	boolean friendly_fire)
{
	struct unit_datum *unit;
	short frags;
	short plasmas;

	ae_modes.slayer.player_killed_player(killing_player_index, killing_object_index, dead_player_index,
		friendly_fire);
	if (friendly_fire || killing_player_index == NONE || killing_player_index == dead_player_index ||
		ae_modes.kill_damage_definition_index == NONE)
	{
		return;
	}
	unit = ae_modes_host_unit(killing_player_index);
	if (!unit)
		return;
	ae_modes_rules_grenades_for_kill(tag_get_name(ae_modes.kill_damage_definition_index), &frags, &plasmas);
	ae_modes_give_grenades(unit, _unit_grenade_human_fragmentation, frags);
	ae_modes_give_grenades(unit, _unit_grenade_covenant_plasma, plasmas);
	if (frags || plasmas)
	{
		error(_error_silent, "ae modes: player %ld's grenade kill gives %d frag, %d plasma",
			(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(killing_player_index), (int)frags, (int)plasmas);
	}
}

struct game_engine *ae_modes_engine(
	struct game_engine *engine)
{
	switch (game_modes_mode())
	{
	case AE_GAME_MODE_ONE_IN_THE_CHAMBER:
		ae_modes.slayer = *engine;
		ae_modes.chamber = *engine;
		ae_modes.chamber.allow_pick_up = ae_modes_allow_pick_up;
		ae_modes.chamber.player_killed_player = ae_modes_chamber_player_killed_player;
		return &ae_modes.chamber;
	case AE_GAME_MODE_FRAG_NATION:
	case AE_GAME_MODE_SNOWBALL_FIGHT:
		/* (Snowball Fight's kills give grenades as Frag Nation's: a plasma's a plasma, and it has no frags) */
		ae_modes.slayer = *engine;
		ae_modes.frag_nation = *engine;
		ae_modes.frag_nation.allow_pick_up = ae_modes_allow_pick_up;
		ae_modes.frag_nation.player_killed_player = ae_modes_frag_nation_player_killed_player;
		ae_modes.frag_nation.update = ae_modes_fists_engine_update;
		return &ae_modes.frag_nation;
	case AE_GAME_MODE_MELEE_ONLY:
		ae_modes.slayer = *engine;
		ae_modes.melee_only = *engine;
		ae_modes.melee_only.allow_pick_up = ae_modes_allow_pick_up;
		ae_modes.melee_only.update = ae_modes_fists_engine_update;
		return &ae_modes.melee_only;
	}
	return engine;
}

void ae_modes_postspawn(
	long player_index)
{
	switch (game_modes_mode())
	{
	case AE_GAME_MODE_ONE_IN_THE_CHAMBER:
		ae_modes_chamber_equip(player_index);
		break;
	case AE_GAME_MODE_FRAG_NATION:
		ae_modes_grenades_postspawn(player_index, AE_MODES_FRAG_NATION_FRAGS, AE_MODES_FRAG_NATION_PLASMAS, "frag nation");
		break;
	case AE_GAME_MODE_SNOWBALL_FIGHT:
		ae_modes_grenades_postspawn(player_index, 0, AE_MODES_SNOWBALL_PLASMAS, "snowball fight");
		break;
	case AE_GAME_MODE_MELEE_ONLY:
		ae_modes_give_fists(player_index);
		break;
	}
}

boolean ae_modes_force_kill(
	long owner_player_index,
	long object_index,
	boolean bullet)
{
	struct unit_datum *unit;

	/* (the host's: a guest replays the kill the host dealt) */
	if (game_modes_mode() != AE_GAME_MODE_ONE_IN_THE_CHAMBER || network_game_distributed_client() ||
		owner_player_index == NONE || !bullet)
	{
		return FALSE;
	}
	unit = (struct unit_datum *)object_try_and_get_and_verify_type(object_index, _object_mask_biped);
	if (!unit || unit->unit.player_index == NONE || unit->unit.player_index == owner_player_index)
		return FALSE;
	error(_error_silent, "ae modes: one in the chamber: player %ld's bullet kills player %ld",
		(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(owner_player_index), (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(unit->unit.player_index));
	return TRUE;
}

void ae_modes_kill_damage(
	long damage_definition_index)
{
	ae_modes.kill_damage_definition_index = damage_definition_index;
}
