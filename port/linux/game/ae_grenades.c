/*
AE_GRENADES.C

Arena Evolved's grenade layer, the game's side (ae_grenades.h): the mask, the grenade pack's types on the loaded map
(the slot map and the grown globals block), the detonation's area query, the effects' state, the stall's controls,
the NEXT / PREVIOUS GRENADE keys and the test aids.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "real_math.h"
#include "game/game.h"
#include "game/game_allegiance.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "items/equipment.h"
#include "items/equipment_definitions.h"
#include "items/items.h"
#include "items/projectiles.h"
#include "networking/network_game_globals.h"
#include "objects/objects.h"
#include "physics/collisions.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "units/units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_grenade_catalogue.h"
#include "ae_grenade_fx.h"
#include "ae_grenades.h"
#include "ae_key_prompts.h"
#include "ae_silent_rules.h"
#include "ae_weapon_cache.h"
#include "ae_weapon_pack.h"
#include "ae_mcc_address.h"
#include "halo_keyboard.h"
#include "ae_seats.h"
#include "mcc_cache.h"
#include "mcc_grenades.h"

long tag_loaded(long group_tag, const char *name);
extern struct game_variant game_variant_global;
char *tag_get_name(long tag_index);
long config_integer(char const *name);
int config_boolean(const char *name);

/* the grenade pack's tags: its prefix, and the groups a row names */
#define GRENADE_PACK_PREFIX "ae\\grenades\\"
#define GROUP_PROJECTILE 0x70726F6AUL /* 'proj' */
#define GROUP_EQUIPMENT 0x65716970UL /* 'eqip' */
#define GROUP_COLLECTION 0x69746D63UL /* 'itmc' */
#define GROUP_HUD_INTERFACE 0x67726869UL /* 'grhi' */
#define GROUP_EFFECT 0x65666665UL /* 'effe' */
enum
{
	_tag_projectile,
	_tag_equipment,
	_tag_collection,
	_tag_hud_interface,
	_tag_effect,
	NUMBER_OF_GRENADE_TAGS
};
static unsigned long const tag_groups[NUMBER_OF_GRENADE_TAGS] =
{
	GROUP_PROJECTILE, GROUP_EQUIPMENT, GROUP_COLLECTION, GROUP_HUD_INTERFACE, GROUP_EFFECT
};

/* a stalled flyer's lift (ae_grenades_stall_lift_scale) */
#define STALL_LIFT_SCALE 0.35f
/* the throws noted, by projectile handle (a catalogue grenade found by the slot it was thrown from) */
#define THROWS_NOTED 32

static struct
{
	/* the map's: its name, the mask it was made with, the slot map */
	char map_name[32];
	unsigned int mask;
	struct ae_grenade_slot_map slots;
	boolean on;
	/* the tags of each placed id; a stand-in's (debug.grenade_stand_in: the map's frag's) */
	long tags[AE_GRENADE_CATALOGUE_IDS][NUMBER_OF_GRENADE_TAGS];
	boolean stand_in;
	/* the grown globals block, and the map's to put back */
	struct game_globals *globals;
	struct game_globals_grenade *grown;
	long grown_count;
	XPTR(void) original_address;
	long original_count;
	/* the catalogue grenades thrown, by projectile handle, and the last detonation run */
	struct { long projectile; short id; } throws[THROWS_NOTED];
	short throw_next;
	long last_detonated;
	/* the test aid's (debug.grenade_test): when it gave, and its EMPs set off (0-3) */
	long test_given_game, test_dropped_game;
	/* debug.grenade_test_me: the value last seen, and the tick its EMP goes off (NONE: none pending) */
	long test_me_seen, test_me_at;
} grenades;

/* the effects' state, a datum's by its absolute index, kept for its full handle */
struct grenade_state
{
	long handle;
	long stall_end;
	long immune_until;
	long scramble_end;
	long scramble_start;
	boolean dirty;
	boolean stall_logged;
};
static struct grenade_state states[MAXIMUM_OBJECTS_PER_MAP];

/* ---------- the mask */

/* THE MASK'S PLACE. Under the AE weapon cache the gametype keeps the mask in game_variant_options' byte 0x2B (bit n:
catalogue id n; ae_weapon_selection.h's `grenades`), and it travels with the variant to every machine before the map
loads; the test override game.grenade_types is the host's alone, written into that byte (ae_grenades_host_options) and
never read on a client. A build without the cache has no byte for it (0x2B is weapon_pack_top there): it stores no
mask, and its only grenade games are the test override, read on EVERY machine of such a build (each machine of the
test sets it: there is no byte to carry it; a host cannot tell a joiner without it from one with it, and refusing
remote machines would refuse a local test's own). The cache's build is found by its header (the T0 interface): no
flag to set */
#if defined(__has_include)
#if __has_include("ae_weapon_selection.h")
#include "ae_weapon_selection.h"
#endif
#endif
#ifdef AE_WEAPON_SELECTION_H
#define GRENADE_MASK_STORED 1
typedef char verify_grenade_byte_offset[AE_WEAPON_SELECTION_GRENADE_BYTE_OFFSET == 0x2B &&
	sizeof(struct game_variant_options) == 0x2C ? 1 : -1];
#else
#define GRENADE_MASK_STORED 0
#endif

static long override_mask(void)
{
	long override = config_integer("game.grenade_types");

	return override > 0 ? (override & ((1L << AE_GRENADE_STORED_IDS) - 1)) : 0;
}

boolean ae_grenades_mask_travels(void)
{
	return GRENADE_MASK_STORED;
}

unsigned int ae_grenades_mask_of(struct game_variant_options const *options)
{
#if GRENADE_MASK_STORED
	unsigned char const *bytes = (unsigned char const *)options;
	unsigned int mask;

	if (!options)
		return 0;
	/* (the selection's rule: the byte, else what a 0.7.6 test build left in the START bytes, read once) */
	mask = bytes[AE_WEAPON_SELECTION_GRENADE_BYTE_OFFSET];
	if (!mask)
		mask = ae_weapon_selection_old_grenade_bits(options->pack_start[0]) |
			(ae_weapon_selection_old_grenade_bits(options->pack_start[1]) << 3);
	return mask;
#else
	(void)options;
	return (unsigned int)override_mask();
#endif
}

void ae_grenades_host_options(struct game_variant_options *options)
{
#if GRENADE_MASK_STORED
	if (!options || !override_mask())
		return;
	((unsigned char *)options)[AE_WEAPON_SELECTION_GRENADE_BYTE_OFFSET] = (unsigned char)override_mask();
	error(_error_silent, "grenades: the host's game takes game.grenade_types: mask 0x%02X in its gametype's grenade "
		"byte", ae_grenades_mask_of(options));
#else
	(void)options;
#endif
}

void ae_grenades_network_test_options(struct game_variant_options *options, unsigned int mask)
{
#if GRENADE_MASK_STORED
	if (options && mask)
		((unsigned char *)options)[AE_WEAPON_SELECTION_GRENADE_BYTE_OFFSET] = (unsigned char)mask;
#else
	if (mask)
		error(_error_silent, "grenades: debug.network_test_grenade_types needs the weapon cache's grenade byte: not "
			"stored by this build");
	(void)options;
#endif
}

unsigned int ae_grenades_mask(void)
{
	return ae_grenades_mask_of(game_variant_options_get());
}

boolean ae_grenades_layer_on(void)
{
	return grenades.on;
}

short ae_grenades_own_count(void)
{
	return grenades.on ? (short)grenades.slots.own_count : 2;
}

short ae_grenades_slot_id(short slot)
{
	if (!grenades.on || slot < 0 || slot >= AE_GRENADE_SLOTS || slot >= grenades.slots.type_count)
		return NONE;
	return grenades.slots.ids[slot] < 0 ? NONE : grenades.slots.ids[slot];
}

short ae_grenades_slot_of(short id)
{
	int slot = grenades.on ? ae_grenade_slot_of(&grenades.slots, id) : -1;

	return slot < 0 ? NONE : (short)slot;
}

boolean ae_grenades_type_on_map(short id)
{
	return ae_grenades_slot_of(id) != NONE;
}

long ae_grenades_tag(short id, unsigned long group)
{
	short index;

	if (!ae_grenades_type_on_map(id))
		return NONE;
	for (index = 0; index < NUMBER_OF_GRENADE_TAGS; index++)
		if (tag_groups[index] == group)
			return grenades.tags[id][index];
	return NONE;
}

unsigned int ae_grenades_join_feature_bits(void)
{
	/* (only with the grenade pack read and taken: without its tags this machine would play frag and plasma alone; and
	the pack's hash, whose tags' indices every machine must agree on) */
	return ae_grenade_join_bits(ae_grenade_layer_version_told(ae_weapon_pack_grenade_pack_loaded()),
		ae_weapon_pack_grenade_pack_hash_byte());
}

/* ---------- the map */

boolean ae_grenades_pack_asked(long scenario_tag_index)
{
	struct game_variant const *variant = &game_variant_global;

	if (!variant->game_engine_index || scenario_tag_index == NONE ||
		scenario_definition_get(scenario_tag_index)->type != _scenario_type_multiplayer)
	{
		return FALSE;
	}
	return ae_grenades_mask() != 0;
}

/* a row's tag of a group, found by name in the grenade pack (NONE: none) */
static long pack_tag(char const *name, unsigned long group)
{
	char full[256];

	if (!name)
		return NONE;
	snprintf(full, sizeof(full), "%s%s", GRENADE_PACK_PREFIX, name);
	return tag_loaded((long)group, full);
}

static void reference_set(struct tag_reference *reference, unsigned long group, long tag_index)
{
	char *name = tag_index != NONE ? tag_get_name(tag_index) : NULL;

	reference->group_tag = group;
	reference->index = tag_index;
	reference->name = name ? XBOX_ADDRESS(name) : XBOX_NULL;
	reference->name_length = name ? (long)strlen(name) : 0;
}

/* an id's tags on the map: its row's in the pack, or (debug.grenade_stand_in) the map's frag's; FALSE when it has
not the projectile and the equipment */
static boolean id_tags_find(short id, struct game_globals_grenade const *own, boolean pack_added)
{
	struct ae_grenade_entry const *entry = ae_grenade_catalogue_entry(id);
	char const *names[NUMBER_OF_GRENADE_TAGS];
	short index;

	for (index = 0; index < NUMBER_OF_GRENADE_TAGS; index++)
		grenades.tags[id][index] = NONE;
	if (!entry)
		return FALSE;
	if (!pack_added && grenades.stand_in)
	{
		grenades.tags[id][_tag_projectile] = own[0].projectile.index;
		grenades.tags[id][_tag_equipment] = own[0].item.index;
		grenades.tags[id][_tag_hud_interface] = own[0].hud_interface.index;
		return grenades.tags[id][_tag_projectile] != NONE && grenades.tags[id][_tag_equipment] != NONE;
	}
	/* (the weapon cache: the type's DIRECTORY entry names its tags by index, no name is looked up) */
	if (ae_weapon_cache_pack())
	{
		for (index = 0; index < NUMBER_OF_GRENADE_TAGS; index++)
			grenades.tags[id][index] = ae_weapon_cache_grenade_tag((unsigned int)id, (enum ae_cache_grenade_tag)index);
		return grenades.tags[id][_tag_projectile] != NONE && grenades.tags[id][_tag_equipment] != NONE;
	}
	names[_tag_projectile] = entry->projectile;
	names[_tag_equipment] = entry->equipment;
	names[_tag_collection] = entry->collection;
	names[_tag_hud_interface] = entry->hud_interface;
	names[_tag_effect] = entry->field_effect;
	for (index = 0; index < NUMBER_OF_GRENADE_TAGS; index++)
		grenades.tags[id][index] = pack_tag(names[index], tag_groups[index]);
	return grenades.tags[id][_tag_projectile] != NONE && grenades.tags[id][_tag_equipment] != NONE;
}

static void slot_entry_make(struct game_globals_grenade *slot_entry, short slot, short id,
	struct game_globals_grenade const *own, long own_count)
{
	struct ae_grenade_entry const *entry = ae_grenade_catalogue_entry(id);
	long equipment = grenades.tags[id][_tag_equipment];

	memset(slot_entry, 0, sizeof(*slot_entry));
	slot_entry->maximum_count = entry->maximum;
	slot_entry->mp_spawn_default = entry->spawn_default;
	/* (the row's own throwing effect, else the map's plasma's: every map has one) */
	slot_entry->throwing_effect = own[own_count > 1 ? 1 : 0].throwing_effect;
	reference_set(&slot_entry->hud_interface, GROUP_HUD_INTERFACE, grenades.tags[id][_tag_hud_interface]);
	reference_set(&slot_entry->item, GROUP_EQUIPMENT, equipment);
	reference_set(&slot_entry->projectile, GROUP_PROJECTILE, grenades.tags[id][_tag_projectile]);
	/* (the pack's equipment picks up into its slot: the slot is the map's, so it is written here; a stand-in's
	equipment is the map's frag's, kept) */
	if (!grenades.stand_in && equipment != NONE)
	{
		struct equipment_definition *definition = equipment_definition_get(equipment);

		definition->equipment.grenade_type = slot;
		definition->equipment.powerup_type = _equipment_powerup_grenade;
	}
}

void ae_grenades_map_tags_added(char const *map_name, long scenario_tag_index, boolean pack_added)
{
	struct game_globals *globals;
	struct game_globals_grenade const *own;
	unsigned int mask, refused = 0;
	long own_count, count;
	short id, slot;
	long globals_index;

	ae_grenades_map_tags_let_go();
	if (config_integer("game.grenade_types") > 0 || ae_grenades_mask() != 0)
		error(_error_silent, "grenades: map %s: mask 0x%02X, grenade pack %s", map_name ? map_name : "?", ae_grenades_mask(),
			pack_added ? "on the map" : "not on the map");
	if (!ae_grenades_pack_asked(scenario_tag_index))
		return;
	/* (the map's globals as scenario_load finds them: its own global is set only after the tags load) */
	globals_index = tag_loaded(GAME_GLOBALS_TAG, "globals\\globals");
	if (globals_index == NONE || !(globals = game_globals_definition_get(globals_index)))
	{
		error(_error_silent, "grenades: none on %s: the map has no globals tag", map_name ? map_name : "?");
		return;
	}
	mask = ae_grenades_mask();
	snprintf(grenades.map_name, sizeof(grenades.map_name), "%s", map_name ? map_name : "?");
	grenades.stand_in = !pack_added && config_boolean("debug.grenade_stand_in");
	own_count = globals->grenades.count;
	own = XBOX_POINTER(struct game_globals_grenade, globals->grenades.address);
	if (own_count < 2 || !own)
	{
		error(_error_silent, "grenades: none on %s: the map's globals have %ld grenade types", grenades.map_name,
			own_count);
		return;
	}
	/* (each id named: its row available, then its tags; the slot map made again without the refused) */
	ae_grenade_slot_map_make(&grenades.slots, (int)own_count, mask);
	for (id = 0; id < AE_GRENADE_CATALOGUE_IDS; id++)
	{
		struct ae_grenade_entry const *entry = ae_grenade_catalogue_entry(id);

		if (!(mask & (1u << id)))
			continue;
		if (grenades.slots.refused & (1u << id))
		{
			error(_error_silent, "grenades: %s unavailable on %s: %s", entry ? entry->display : "a later type",
				grenades.map_name, grenades.slots.no_slot & (1u << id) ? "no free grenade slot" :
				entry ? "not in this build yet" : "this build does not know it");
			continue;
		}
		if (!pack_added && !grenades.stand_in)
		{
			error(_error_silent, "grenades: %s unavailable on %s: the grenade pack is not on the map",
				entry->display, grenades.map_name);
			refused |= 1u << id;
			continue;
		}
		if (!id_tags_find(id, own, pack_added))
		{
			error(_error_silent, "grenades: %s unavailable on %s: its projectile or equipment is not in the grenade "
				"pack", entry->display, grenades.map_name);
			refused |= 1u << id;
		}
	}
	ae_grenade_slot_map_make(&grenades.slots, (int)own_count, mask & ~refused);
	grenades.mask = mask;
	if (!grenades.slots.placed)
	{
		error(_error_silent, "grenades: %s plays frag and plasma only (mask 0x%02X)", grenades.map_name, mask);
		return;
	}
	/* (the grown block: the map's own entries, then one a placed id, in pack memory: the map's put back on let go) */
	count = grenades.slots.type_count;
	grenades.grown = (struct game_globals_grenade *)malloc((size_t)count * sizeof(*grenades.grown));
	if (!grenades.grown)
	{
		error(_error_silent, "grenades: none on %s: no memory for the grenade block", grenades.map_name);
		return;
	}
	memcpy(grenades.grown, own, (size_t)MIN(own_count, count) * sizeof(*grenades.grown));
	for (slot = (short)grenades.slots.own_count; slot < count; slot++)
		slot_entry_make(&grenades.grown[slot], slot, grenades.slots.ids[slot], own, own_count);
	grenades.globals = globals;
	grenades.original_address = globals->grenades.address;
	grenades.original_count = own_count;
	grenades.grown_count = count;
	globals->grenades.address = XBOX_ADDRESS(grenades.grown);
	globals->grenades.count = count;
	grenades.on = TRUE;
	grenades.last_detonated = NONE;
	grenades.test_given_game = NONE;
	grenades.test_dropped_game = 0;
	grenades.test_me_seen = 0;
	grenades.test_me_at = NONE;
	memset(grenades.throws, 0, sizeof(grenades.throws));
	ae_grenades_state_reset();
	mcc_grenades_reset();
	for (slot = (short)grenades.slots.own_count; slot < count; slot++)
	{
		id = grenades.slots.ids[slot];
		error(_error_silent, "grenades: %s in slot %d on %s%s (projectile %ld, equipment %ld, HUD %ld, collection %ld, "
			"field %ld)", ae_grenade_catalogue_entry(id)->display, (int)slot, grenades.map_name,
			grenades.stand_in ? " as a STAND-IN (the map's frag grenade's tags: debug.grenade_stand_in)" : "",
			grenades.tags[id][_tag_projectile], grenades.tags[id][_tag_equipment],
			grenades.tags[id][_tag_hud_interface], grenades.tags[id][_tag_collection], grenades.tags[id][_tag_effect]);
	}
	error(_error_silent, "grenades: layer on for %s: mask 0x%02X, %d grenade types (%d the map's own)",
		grenades.map_name, mask, grenades.slots.type_count, grenades.slots.own_count);
}

void ae_grenades_map_tags_let_go(void)
{
	if (grenades.globals && grenades.grown)
	{
		grenades.globals->grenades.address = grenades.original_address;
		grenades.globals->grenades.count = grenades.original_count;
		error(_error_silent, "grenades: the map's own grenade block put back on %s (%ld types)", grenades.map_name,
			grenades.original_count);
	}
	if (grenades.grown)
		free(grenades.grown);
	grenades.grown = NULL;
	grenades.globals = NULL;
	grenades.grown_count = 0;
	grenades.on = FALSE;
	grenades.stand_in = FALSE;
	memset(&grenades.slots, 0, sizeof(grenades.slots));
	ae_grenades_state_reset();
}

void ae_grenades_new_map(void)
{
	/* (a map started again without its tags loaded again: a reset, a local playlist's next game, DEV MODE's reload, a
	TEST RUN group's next test: the effects, the throws and the test aids of the game before go; the slot map and the
	grown block stay the map's) */
	ae_grenades_state_reset();
	memset(grenades.throws, 0, sizeof(grenades.throws));
	grenades.throw_next = 0;
	grenades.last_detonated = NONE;
	grenades.test_given_game = NONE;
	grenades.test_dropped_game = 0;
	grenades.test_me_seen = 0;
	grenades.test_me_at = NONE;
}

int ae_grenades_memory(unsigned long address, long size)
{
	unsigned long start, end;

	if (!grenades.grown || size < 0)
		return 0;
	start = (unsigned long)XBOX_ADDRESS(grenades.grown);
	end = start + (unsigned long)grenades.grown_count * sizeof(*grenades.grown);
	return address >= start && address < end && (unsigned long)size <= end - address;
}

/* ---------- the effects' state */

static struct grenade_state *state_of(long object_index, boolean create)
{
	long index = DATUM_INDEX_TO_ABSOLUTE_INDEX(object_index);
	struct grenade_state *state;

	if (object_index == NONE || index < 0 || index >= MAXIMUM_OBJECTS_PER_MAP)
		return NULL;
	state = &states[index];
	if (state->handle != object_index)
	{
		if (!create)
			return NULL;
		memset(state, 0, sizeof(*state));
		state->handle = object_index;
	}
	return state;
}

void ae_grenades_state_reset(void)
{
	long index;

	for (index = 0; index < MAXIMUM_OBJECTS_PER_MAP; index++)
	{
		memset(&states[index], 0, sizeof(states[index]));
		states[index].handle = NONE;
	}
}

long ae_grenades_stall_ticks_left(long vehicle_index)
{
	struct grenade_state const *state = grenades.on ? state_of(vehicle_index, FALSE) : NULL;
	long left = state ? state->stall_end - game_time_get() : 0;

	return left > 0 ? left : 0;
}

boolean ae_grenades_vehicle_stalled(long vehicle_index)
{
	return ae_grenades_stall_ticks_left(vehicle_index) > 0;
}

long ae_grenades_scramble_ticks_left(long unit_index)
{
	struct grenade_state const *state = grenades.on ? state_of(unit_index, FALSE) : NULL;
	long left = state ? state->scramble_end - game_time_get() : 0;

	return left > 0 ? left : 0;
}

long ae_grenades_scramble_age(long unit_index)
{
	struct grenade_state const *state = grenades.on ? state_of(unit_index, FALSE) : NULL;

	if (!state || state->scramble_end <= game_time_get())
		return NONE;
	return game_time_get() - state->scramble_start;
}

void ae_grenades_state_set_stall(long object_index, long ticks)
{
	struct grenade_state *state = state_of(object_index, ticks > 0);
	long now = game_time_get();

	if (!state)
		return;
	if (ticks > 0 && state->stall_end <= now)
		state->stall_logged = FALSE;
	state->stall_end = ticks > 0 ? now + ticks : now;
	state->dirty = TRUE;
}

void ae_grenades_state_set_scramble(long object_index, long ticks)
{
	struct grenade_state *state = state_of(object_index, ticks > 0);
	long now = game_time_get();

	if (!state)
		return;
	if (ticks > 0 && state->scramble_end <= now)
		state->scramble_start = now;
	state->scramble_end = ticks > 0 ? now + ticks : now;
	state->dirty = TRUE;
}

boolean ae_grenades_state_dirty(long object_index)
{
	struct grenade_state const *state = state_of(object_index, FALSE);

	return state && state->dirty;
}

void ae_grenades_state_sent(long object_index)
{
	struct grenade_state *state = state_of(object_index, FALSE);

	if (state)
		state->dirty = FALSE;
}

long ae_grenades_state_next(long *cursor)
{
	while (cursor && *cursor >= 0 && *cursor < MAXIMUM_OBJECTS_PER_MAP)
	{
		struct grenade_state const *state = &states[(*cursor)++];

		if (state->handle != NONE && state->handle != 0 && (state->dirty || state->stall_end > game_time_get() ||
			state->scramble_end > game_time_get()))
		{
			return state->handle;
		}
	}
	return NONE;
}

/* the host: a stall hit of `ticks` on a vehicle (re-hits refresh, never stack; none while immune); the new end */
long ae_grenade_fx_stall_hit(long vehicle_index, long ticks, float immunity_seconds)
{
	struct grenade_state *state = state_of(vehicle_index, TRUE);
	long now = game_time_get(), end;

	if (!state)
		return 0;
	end = ae_grenade_stall_refresh(now, state->stall_end, state->immune_until, ticks);
	if (end != state->stall_end)
	{
		if (state->stall_end <= now)
			state->stall_logged = FALSE;
		state->stall_end = end;
		state->immune_until = ae_grenade_stall_immunity_end(end, immunity_seconds);
		state->dirty = TRUE;
	}
	return end > now ? end - now : 0;
}

/* the host: a scramble hit of `ticks` on a unit (refreshes, never stacks); the ticks left */
long ae_grenade_fx_scramble_hit(long unit_index, long ticks)
{
	struct grenade_state *state = state_of(unit_index, TRUE);
	long now = game_time_get();

	if (!state || ticks <= 0)
		return 0;
	if (state->scramble_end <= now)
		state->scramble_start = now;
	if (now + ticks > state->scramble_end)
	{
		state->scramble_end = now + ticks;
		state->dirty = TRUE;
	}
	return state->scramble_end - now;
}

/* ---------- the stall's controls */

void ae_grenades_stall_controls(long unit_index)
{
	struct unit_datum *unit;

	if (!grenades.on || !ae_grenades_vehicle_stalled(unit_index) ||
		!(unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_vehicle)))
	{
		return;
	}
	unit->unit.throttle = *global_zero_vector3d;
	unit->unit.control_flags &= ~(FLAG(_unit_control_jump_bit) | FLAG(_unit_control_crouch_modifier_bit));
	/* (the driver's own gun: a seat that drives and shoots; a gunner of another seat keeps firing) */
	if (unit->unit.driver_object_index != NONE && unit->unit.gunner_object_index == unit->unit.driver_object_index)
	{
		unit->unit.control_flags &= ~UNIT_CONTROL_GUNNER_MASK;
		unit->unit.primary_trigger = 0.0f;
	}
}

void ae_grenades_stall_brake(long vehicle_index)
{
	struct grenade_state *state;

	if (!grenades.on || !(state = state_of(vehicle_index, FALSE)) || state->stall_end <= game_time_get() ||
		state->stall_logged)
	{
		return;
	}
	/* (said once a stall, the first tick it brakes) */
	state->stall_logged = TRUE;
	error(_error_silent, "grenades: vehicle %08lX stalled: no throttle, braking, its driver's gun locked (%ld ticks left)",
		(unsigned long)vehicle_index, state->stall_end - game_time_get());
}

real ae_grenades_stall_lift_scale(long vehicle_index)
{
	return grenades.on && ae_grenades_vehicle_stalled(vehicle_index) ? STALL_LIFT_SCALE : 1.0f;
}

boolean ae_grenades_prediction_ignored(long vehicle_index)
{
	return grenades.on && ae_grenades_vehicle_stalled(vehicle_index);
}

/* ---------- the detonation */

void ae_grenades_thrown(long projectile_index, short slot)
{
	short id = ae_grenades_slot_id(slot);

	if (id == NONE || projectile_index == NONE)
		return;
	grenades.throws[grenades.throw_next].projectile = projectile_index;
	grenades.throws[grenades.throw_next].id = id;
	grenades.throw_next = (short)((grenades.throw_next + 1) % THROWS_NOTED);
}

/* the catalogue id of a projectile: one thrown from a catalogue slot, else (not a stand-in) by its definition */
static short projectile_id(long projectile_index)
{
	struct object_datum *object = object_get(projectile_index);
	short index, slot;

	for (index = 0; index < THROWS_NOTED; index++)
		if (grenades.throws[index].projectile == projectile_index && projectile_index != 0)
			return grenades.throws[index].id;
	if (grenades.stand_in)
		return NONE;
	for (slot = (short)grenades.slots.own_count; slot < grenades.slots.type_count && slot < AE_GRENADE_SLOTS; slot++)
	{
		short id = grenades.slots.ids[slot];

		if (id >= 0 && grenades.tags[id][_tag_projectile] == object->definition_index)
			return id;
	}
	return NONE;
}

/* whether the blast's point sees the point (the environment only) */
static boolean blast_sees(real_point3d const *from, real_point3d const *to)
{
	struct collision_result collision;
	real_vector3d vector;

	vector_from_points3d(from, to, &vector);
	if (!collision_test_vector(FLAG(_collision_test_front_facing_surfaces_bit) | FLAG(_collision_test_structure_bit) |
		FLAG(_collision_test_ignore_invisible_surfaces_bit), from, &vector, NONE, &collision))
	{
		return TRUE;
	}
	return collision.t >= 0.98f;
}

static void blast_query(struct ae_grenade_blast *blast)
{
	struct object_iterator iterator;
	struct object_datum *object;
	real_point3d from = blast->point;

	/* (a little above the point: a grenade at rest lies on the floor it would otherwise test against) */
	from.z += 0.1f;
	blast->target_count = 0;
	object_iterator_new(&iterator, _object_mask_unit | _object_mask_equipment, 0);
	while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL &&
		blast->target_count < AE_GRENADE_BLAST_TARGETS)
	{
		real_point3d center = object->object.bounding_sphere_center;
		real distance = distance3d(&from, &center) - object->object.bounding_sphere_radius * 0.5f;
		real strength;

		if (distance < 0.0f)
			distance = 0.0f;
		strength = ae_grenade_falloff(&blast->entry->numbers, distance);
		if (!(strength > 0.0f) || !blast_sees(&from, &center))
			continue;
		blast->targets[blast->target_count].object_index = iterator.index;
		blast->targets[blast->target_count].type = object->object.type;
		blast->targets[blast->target_count].strength = strength;
		blast->targets[blast->target_count].distance = distance;
		blast->target_count++;
	}
}

void ae_grenade_detonated(long projectile_index, real_point3d const *point)
{
	struct ae_grenade_blast blast;
	struct object_datum *projectile;
	short id;

	if (!grenades.on || projectile_index == NONE || !point || network_game_distributed_client() ||
		projectile_index == grenades.last_detonated)
	{
		return;
	}
	id = projectile_id(projectile_index);
	if (id == NONE)
		return;
	grenades.last_detonated = projectile_index;
	projectile = object_get(projectile_index);
	memset(&blast, 0, sizeof(blast));
	blast.id = id;
	blast.entry = ae_grenade_catalogue_entry(id);
	blast.point = *point;
	blast.now = game_time_get();
	blast.thrower_unit = projectile->object.owner_object_index;
	blast.thrower_player = projectile->object.owner_player_index;
	blast.thrower_team = projectile->object.owner_team_index;
	blast_query(&blast);
	error(_error_silent, "grenades: %s went off at %.2f %.2f %.2f (projectile %08lX, thrower player %08lX): %d objects "
		"in reach", blast.entry->display, (double)point->x, (double)point->y, (double)point->z,
		(unsigned long)projectile_index, (unsigned long)blast.thrower_player, (int)blast.target_count);
	if (blast.entry->effects & AE_GRENADE_FX_STALL)
		ae_grenade_fx_stall_apply(&blast);
	if (blast.entry->effects & AE_GRENADE_FX_SCRAMBLE)
		ae_grenade_fx_scramble_apply(&blast);
	if (blast.entry->effects & AE_GRENADE_FX_POWERUP_SHORT)
		ae_grenade_fx_powerup_short_apply(&blast);
}

/* ---------- give (DEV MODE's give <grenade>, the test aid) */

short ae_grenades_give(long unit_index, short id, short count)
{
	struct ae_grenade_entry const *entry = ae_grenade_catalogue_entry(id);
	short slot = ae_grenades_slot_of(id);

	if (!entry || slot == NONE || unit_index == NONE || !unit_try_and_get(unit_index))
		return NONE;
	if (count < 0)
		count = entry->maximum;
	mcc_grenades_set(unit_index, slot, (short)MIN(count, entry->maximum));
	return mcc_grenades_get(unit_index, slot);
}

/* ---------- the test aids */

/* the first local player's living unit, or NONE */
static long first_player_unit(void)
{
	long player_index = local_player_get_player_index(0);
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;

	return player && player->unit_index != NONE && unit_try_and_get(player->unit_index) ? player->unit_index : NONE;
}

/* an EMP set off at once at a point, thrown by the unit (the test aid's) */
static long test_set_off(long unit_index, real_point3d const *point)
{
	struct object_placement_data placement;
	short emp_slot = ae_grenades_slot_of(_ae_grenade_emp);
	long projectile_index;

	if (emp_slot == NONE)
		return NONE;
	object_placement_data_new(&placement, grenades.tags[_ae_grenade_emp][_tag_projectile], unit_index);
	placement.position = *point;
	placement.position.z += 0.6f;
	placement.forward = *global_down3d;
	placement.up = *global_forward3d;
	projectile_index = object_new(&placement);
	if (projectile_index != NONE)
	{
		struct object_datum *projectile = object_get(projectile_index);

		projectile->object.owner_player_index = local_player_get_player_index(0);
		ae_grenades_thrown(projectile_index, emp_slot);
		/* (the datum's counting-down bit, 5, and its timer at the end: it goes off at once) */
		SET_FLAG(projectile_get(projectile_index)->projectile.flags, 5, TRUE);
		projectile_get(projectile_index)->projectile.detonation_timer = 1.0f;
	}
	return projectile_index;
}

/* the object of a type nearest a point, other than one (NONE: none); of vehicles, a driven one before any other */
static long nearest_object(unsigned long mask, real_point3d const *origin, long other_than, real *distance_out)
{
	struct object_iterator iterator;
	struct object_datum *object;
	long nearest = NONE;
	real nearest_distance = 0.0f;

	object_iterator_new(&iterator, mask, 0);
	while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
	{
		real distance = distance3d(origin, &object->object.position);

		if (iterator.index == other_than || (mask == _object_mask_biped &&
			((struct unit_datum *)object)->unit.player_index == NONE))
		{
			continue;
		}
		/* (a vehicle someone drives first: its stall shows in its speed) */
		if (mask == _object_mask_vehicle && nearest != NONE &&
			(((struct unit_datum *)object_get(nearest))->unit.driver_object_index != NONE) !=
			(((struct unit_datum *)object)->unit.driver_object_index != NONE))
		{
			if (((struct unit_datum *)object)->unit.driver_object_index == NONE)
				continue;
			nearest = NONE;
		}
		if (nearest == NONE || distance < nearest_distance)
		{
			nearest = iterator.index;
			nearest_distance = distance;
		}
	}
	*distance_out = nearest_distance;
	return nearest;
}

/* debug.grenade_test (seconds): every player given the catalogue's grenades at that second; at the next an EMP set
off at the vehicle nearest player 1; at the one after, one at the other player nearest player 1; at the one after
that, one at the first overshield or camouflage lying on the map (they go off at once) */
static void test_aid(void)
{
	long seconds = config_integer("debug.grenade_test"), now = game_time_get(), unit_index;
	short slot;

	if (seconds <= 0 || !grenades.on || network_game_distributed_client() || (unit_index = first_player_unit()) == NONE)
		return;
	if (grenades.test_given_game == NONE && now >= seconds * TICKS_PER_SECOND)
	{
		struct data_iterator iterator;
		struct player_datum *player;

		grenades.test_given_game = now;
		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			if (player->unit_index == NONE || !unit_try_and_get(player->unit_index))
				continue;
			for (slot = (short)grenades.slots.own_count; slot < grenades.slots.type_count; slot++)
			{
				short id = grenades.slots.ids[slot];

				error(_error_silent, "grenades: test: unit %08lX given %d %s (slot %d)", (unsigned long)player->unit_index,
					(int)ae_grenades_give(player->unit_index, id, -1), ae_grenade_catalogue_entry(id)->display, (int)slot);
			}
		}
	}
	if (grenades.test_dropped_game == 0 && now >= (seconds + 1) * TICKS_PER_SECOND)
	{
		real_point3d origin;
		real distance;
		long nearest, projectile_index;

		grenades.test_dropped_game = 1;
		object_get_origin(unit_index, &origin);
		nearest = nearest_object(_object_mask_vehicle, &origin, NONE, &distance);
		if (nearest == NONE)
		{
			error(_error_silent, "grenades: test: no vehicle on the map to set the EMP off at");
			return;
		}
		projectile_index = test_set_off(unit_index, &object_get(nearest)->object.bounding_sphere_center);
		error(_error_silent, "grenades: test: EMP set off at vehicle %08lX, %.2f wu from player 1 (projectile %08lX)",
			(unsigned long)nearest, (double)distance, (unsigned long)projectile_index);
	}
	if (grenades.test_dropped_game == 1 && now >= (seconds + 2) * TICKS_PER_SECOND)
	{
		real_point3d origin;
		real distance;
		long nearest, projectile_index;

		grenades.test_dropped_game = 2;
		object_get_origin(unit_index, &origin);
		nearest = nearest_object(_object_mask_biped, &origin, unit_index, &distance);
		if (nearest == NONE)
		{
			error(_error_silent, "grenades: test: no other player to set the EMP off at");
			return;
		}
		projectile_index = test_set_off(unit_index, &object_get(nearest)->object.bounding_sphere_center);
		error(_error_silent, "grenades: test: EMP set off at player unit %08lX, %.2f wu from player 1 (projectile %08lX)",
			(unsigned long)nearest, (double)distance, (unsigned long)projectile_index);
	}
	if (grenades.test_dropped_game == 2 && now >= (seconds + 3) * TICKS_PER_SECOND)
	{
		struct object_iterator iterator;
		struct object_datum *object;
		long projectile_index;

		/* (the first powerup item lying on the map: overshield, camouflage...) */
		grenades.test_dropped_game = 3;
		object_iterator_new(&iterator, _object_mask_equipment, 0);
		while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
		{
			short type = equipment_definition_get(object->definition_index)->equipment.powerup_type;

			if (type == _equipment_powerup_overshield || type == _equipment_powerup_active_camouflage)
				break;
		}
		if (!object)
		{
			error(_error_silent, "grenades: test: no powerup lying on the map to set the EMP off at");
			return;
		}
		projectile_index = test_set_off(unit_index, &object->object.position);
		error(_error_silent, "grenades: test: EMP set off at powerup %08lX (projectile %08lX)",
			(unsigned long)iterator.index, (unsigned long)projectile_index);
	}
}

/* debug.grenade_test_me (seconds): set (to a value it has not), an EMP goes off at player 1 that many seconds later,
thrown by nobody, so player 1 is scrambled (the HUD's test: a player is never scrambled by their own) */
static void test_me(void)
{
	long seconds = config_integer("debug.grenade_test_me"), now = game_time_get(), unit_index;

	if (network_game_distributed_client())
		return;
	if (seconds != grenades.test_me_seen)
	{
		grenades.test_me_seen = seconds;
		grenades.test_me_at = seconds > 0 ? now + seconds * TICKS_PER_SECOND : NONE;
	}
	if (grenades.test_me_at == NONE || now < grenades.test_me_at || (unit_index = first_player_unit()) == NONE)
		return;
	grenades.test_me_at = NONE;
	{
		long projectile_index = test_set_off(NONE, &object_get(unit_index)->object.bounding_sphere_center);

		if (projectile_index != NONE)
			object_get(projectile_index)->object.owner_player_index = NONE;
		error(_error_silent, "grenades: test: EMP set off at player 1's unit %08lX, thrown by nobody (projectile %08lX)",
			(unsigned long)unit_index, (unsigned long)projectile_index);
	}
}

void ae_grenades_update(void)
{
	long cursor = 0, handle, now = game_time_get();

	if (!grenades.on)
		return;
	test_aid();
	test_me();
	/* (each effect's end said once, the tick it ends) */
	while ((handle = ae_grenades_state_next(&cursor)) != NONE)
	{
		struct grenade_state const *state = state_of(handle, FALSE);

		if (state && state->stall_end == now + 1)
			error(_error_silent, "grenades: vehicle %08lX's stall ends", (unsigned long)handle);
		/* (a stalled vehicle's speed, each second: it seeks 0) */
		if (state && state->stall_end > now && (state->stall_end - now) % TICKS_PER_SECOND == 0 &&
			object_try_and_get_and_verify_type(handle, _object_mask_vehicle))
		{
			struct object_datum const *vehicle = object_get(handle);

			error(_error_silent, "grenades: vehicle %08lX stalled: speed %.2f wu/s, %ld ticks left", (unsigned long)handle,
				(double)(magnitude3d(&vehicle->object.translational_velocity) * TICKS_PER_SECOND),
				state->stall_end - now);
		}
		if (state && state->scramble_end == now + 1)
			error(_error_silent, "grenades: unit %08lX's scramble ends", (unsigned long)handle);
	}
}

/* ---------- the keys */

short ae_grenades_cycle_input(short local_player_index, long unit_index, short desired_grenade_index)
{
	static unsigned char held[MAXIMUM_LOCAL_PLAYERS][2];
	unsigned long long keys = 0;
	int pressed[2], which;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return desired_grenade_index;
	if (local_player_index == ae_key_prompts_keyboard_player())
		keys = halo_keyboard_actions((short)ae_seats_keyboard_controller());
	pressed[0] = (keys & (1ULL << HALO_KEYBOARD_GRENADE_NEXT)) != 0;
	pressed[1] = (keys & (1ULL << HALO_KEYBOARD_GRENADE_PREVIOUS)) != 0;
	for (which = 0; which < 2; which++)
	{
		if (pressed[which] && !held[local_player_index][which] && unit_index != NONE)
		{
			short next = unit_inventory_next_grenade(unit_index, desired_grenade_index, (short)(which ? -1 : 1));

			if (next != NONE)
				desired_grenade_index = next;
		}
		held[local_player_index][which] = (unsigned char)pressed[which];
	}
	return desired_grenade_index;
}
