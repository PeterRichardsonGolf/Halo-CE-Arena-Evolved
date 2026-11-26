/*
AE_RANDOMIZER_GLUE.INC

Compiled inside randomizer.c (its last line), for the state it keeps: what this build's other files ask of
the randomizer (ae_randomizer_glue.h).
*/

#include "ae_randomizer_glue.h"
#include "ae_randomizer_rules.h"
#include "ae_firefight_doors.h"
#include "ae_tag_loaded.h"
#include "tag_pack.h"

/* the tags the level's load added to its table, and the number a saved game keeps for them */
static int ae_imported;
static unsigned long ae_import_signature;

/* whether this level is one the randomizer plays at all: a campaign level played alone (or in split
screen on one machine), a Firefight game, or the firing range's level. Any multiplayer game's level, a
network game's of any kind and the menus' own are not: nothing is read, copied or logged for them */
static int ae_randomizer_campaign_game(char const *scenario_name)
{
	extern struct game_variant game_variant_global;
	struct network_game *game = network_game_get_game();
	struct scenario *scenario = loaded_scenario();
	int firefight = ae_randomizer_rules_firefight_game(game_variant_global.game_engine_index == game_engine_firefight,
		game && game->variant.game_engine_index == game_engine_firefight);

	return ae_randomizer_rules_plays(firefight, scenario_name && firing_range_level_profile(scenario_name) != NULL,
		game != NULL || network_game_distributed_client(), scenario && scenario->type == _scenario_type_solo);
}

/* the firing range's profile was injected by the randomizer's own range branch (ae_tag_inject.c leaves it) */
static int ae_range_done;

int ae_randomizer_range_injected(void)
{
	return ae_range_done;
}

void ae_randomizer_tags_loaded(char const *scenario_name)
{
	long before_count = 0;
	unsigned long before_hash = 0;
	long after_count = 0;
	unsigned long after_hash = 0;

	ae_range_done = scenario_name && firing_range_level_profile(scenario_name) && !firefight_wanted() &&
		ae_randomizer_campaign_game(scenario_name);
	tag_import_tag_table_fingerprint(&before_count, &before_hash);
	randomizer_tags_loaded(scenario_name);
	tag_import_tag_table_fingerprint(&after_count, &after_hash);
	if (after_count != before_count)
	{
		ae_imported = 1;
		ae_import_signature = (mix(after_hash ^ (unsigned long)after_count) << 1) | 1;
		ae_tag_table_changed();
	}
}

void ae_randomizer_tags_unloaded(void)
{
	randomizer_tags_unloaded();
	ae_range_done = 0;
	ae_imported = 0;
	ae_import_signature = 0;
}

int randomizer_imported(void)
{
	return ae_imported;
}

unsigned long randomizer_save_signature(void)
{
	return ae_imported ? ae_import_signature : 0;
}

int randomizer_actor_weapon(long weapon_definition, long unit_index, long variant_index)
{
	long attempt;

	if (!loaded || !(current.weapons || tag_campaign_level_playing()))
		return 0;
	for (attempt = 0; attempt < 8; attempt++)
	{
		struct object_placement_data placement_data;
		long chosen = randomizer_enemy_weapon(weapon_definition, unit_index, attempt, variant_index);
		long weapon_index;

		if (chosen == NONE)
			break;
		object_placement_data_new(&placement_data, chosen, unit_index);
		weapon_index = object_new(&placement_data);
		if (weapon_index != NONE && unit_add_weapon_to_inventory(unit_index, weapon_index, _unit_add_weapon_replace))
			return 1;
		if (weapon_index != NONE)
			object_delete(weapon_index);
	}

	return 1;
}

void randomizer_pack_state(struct randomizer_pack_state *state)
{
	struct tag_pack_state pack;

	tag_pack_state(RANDOMIZER_PACK, &pack);
	state->writing = pack.writing;
	state->maps_done = pack.maps_done;
	state->maps_wanted = pack.maps_wanted;
	state->done = pack.done;
	state->failed = pack.failed;
	snprintf(state->reason, sizeof(state->reason), "%s", pack.reason);
}

void randomizer_pack_write(void)
{
	tag_pack_request(RANDOMIZER_PACK);
	/* (Firefight's: its scenery, and the skulls' carried skull from Wizard) */
	if (ae_firefight_doors_pack_on() || firefight_wanted())
	{
		tag_pack_request(FIREFIGHT_PACK);
		tag_pack_request(SKULLS_PACK);
	}
	tag_pack_start_now();
}

int randomizer_pack_wait(void)
{
	static int told;
	struct randomizer_pack_state pack;

	if (!ae_firefight_doors_pack_on())
		return 0;
	randomizer_pack_state(&pack);
	if (pack.done || pack.failed)
		return 0;
	if (!told)
	{
		told = 1;
		platform_log("randomizer: a test game waits for the campaign's tag pack to be written");
	}
	return 1;
}

void randomizer_frame(void)
{
	static int requested;

	if (!config_boolean("game.randomizer") && !ae_firefight_doors_pack_on())
	{
		requested = 0;
		if (tag_pack_busy())
			tag_pack_update(FALSE);
		return;
	}
	tag_pack_update(FALSE);
	if (!requested)
	{
		requested = 1;
		randomizer_pack_write();
	}
}

void randomizer_reroll(void)
{
	placement_roll_ready = 0;
}

void randomizer_status(char *text, unsigned int size)
{
	long count = 0;
	unsigned long hash = 0;
	struct randomizer_pack_state pack;

	randomizer_pack_state(&pack);
	tag_import_tag_table_fingerprint(&count, &hash);
	snprintf(text, size,
		"randomizer %s: level %s %s, roll %08lx, %ld enemy %ld ally actors, %ld pickup %ld actor weapons, "
		"loadout %s, copied tags %s, table %ld tags %08lx, pack %s %d of %d maps%s%s",
		config_boolean("game.randomizer") ? "on" : "off", map_name_leaf[0] ? map_name_leaf : "-",
		loaded || loadout_ready ? "randomized" : "not randomized", placement_roll, enemy_choice.count,
		ally_choice.count, pickup_choice.count, actor_weapon_choice.count, loadout_ready ? "given" : "default",
		ae_imported ? "yes" : "no", count, hash, pack.writing ? "writing" : pack.done ? "done" : pack.failed ? "failed" : "absent",
		pack.maps_done, pack.maps_wanted, pack.reason[0] ? ": " : "", pack.reason);
}
