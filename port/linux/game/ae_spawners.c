/*
AE_SPAWNERS.C

AE weapon spawners on the loaded map (ae_spawners.h). The engine half: the conversion of the map's weapon items (on the
pure rules of ae_spawner_rules.h), the paired-spot merge, the sidecar's edits, the set's rows (ae_weaponset_rules.h)
resolved against the cache and the map, the per-spawner state machine (ae_spawner_logic_rules.h) fed with what the
host sees, the pools (ae_spawner_pool_rules.h), the item timers' view, and the dev chips. First built on Sali Creek.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "cseries/cseries.h"
#include "cseries/errors.h"
#include "cache/cache_files.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/item_timers.h"
#include "interface/hud.h"
#include "interface/ui_widget.h"
#include "items/item_definitions.h"
#include "items/items.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "math/real_math.h"
#include "networking/network_game_globals.h"
#include "objects/object_definitions.h"
#include "objects/object_types.h"
#include "objects/objects.h"
#include "render/render.h"
#include "render/render_camera_projection.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_files.h"

#include "ae_bots.h"
#include "ae_dev_events.h"
#include "ae_gametype_ammo.h"
#include "ae_h2_match.h"
#include "ae_hud_digits.h"
#include "ae_hud_text.h"
#include "ae_layout.h"
#include "ae_layout_rules.h"
#include "ae_medals_glue.h"
#include "ae_score_strip.h"
#include "ae_spawner_file_rules.h"
#include "ae_spawner_logic_rules.h"
#include "ae_spawner_msg_rules.h"
#include "ae_spawner_pool_rules.h"
#include "ae_spawner_rules.h"
#include "ae_spawners.h"
#include "ae_spawner_holo.h"
#include "ae_variant_ext.h"
#include "ae_variant_ext_rules.h"
#include "ae_weapon_ids.h"
#include "ae_weapon_registry.h"
#include "ae_weapon_selection.h"
#include "ae_weapon_selection_game.h"
#include "ae_weaponset_rules.h"
#include "mcc_cache.h"
#include "network_distributed.h"

void platform_log(char const *format, ...);
int config_boolean(char const *name);
char const *config_string(char const *name);

#define item_collection_definition_get(index) \
	((struct item_collection_definition *)tag_get(ITEM_COLLECTION_DEFINITION_TAG, index))

#define SPAWNERS_MAXIMUM 256
#define MERGED_MAXIMUM 4
#define EQUIPMENT_AT_REST_BIT 0 /* game_engine.c's _equipment_created_at_rest_bit */

/* the globals' weapon list order is ae_spawner_rules.h's stock order for the first ten (AE_STOCK_SWORD is not in it) */
#define LIST_STOCK_COUNT 10

static char const *const stock_short[LIST_STOCK_COUNT] = { "AR", "FLAME", "FUEL ROD", "NEEDLER", "MAGNUM", "PLASMA PISTOL",
	"PLASMA RIFLE", "ROCKETS", "SHOTGUN", "SNIPER" };

struct spawner
{
	short n; /* the map item (the first of a merged pair) */
	short merged[MERGED_MAXIMUM]; /* the other items merged into this spot */
	short merged_count;
	signed char cls, tier, kind;
	boolean active; /* in this game's game type */
	boolean skipped; /* a sidecar `spawner skip`: a plain map item (the CE loop's) */
	boolean split; /* a sidecar `split`: never merged */
	unsigned long sets; /* the sets it exists under (bits) */
	short own_seconds; /* the entry's own spawn_time */
	long map_definition; /* the map's own weapon (an empty pool's) */
	real_point3d position;
	long pinned; /* a pinned weapon (NONE none): the pool is it alone */
	char channel[AE_WEAPONSET_NAME_SIZE];
	/* the set's row applied */
	struct ae_weaponset_row row;
	boolean has_row;
	long pool_definitions[AE_SPAWNER_POOL_MAXIMUM];
	char pool_word[AE_SPAWNER_POOL_MAXIMUM][AE_WEAPONSET_REF_SIZE];
	struct ae_spawner_pool pool;
	/* the host's state */
	struct ae_spawner_machine machine;
	long object;
	long made_object; /* a static spawner's last weapon, watched for its pickup (NONE none) */
	char weapon[24]; /* the last weapon it made */
	long hold_red_until; /* DEV MODE's `spawner red`: forced RED until this tick (0 none) */
};

static struct
{
	struct scenario *scenario;
	int set; /* AE_WEAPONSET_* in play, -1 off */
	int built_set;
	boolean built;
	long last_tick;
	unsigned long games;
	short count;
	boolean set_forced;
	int forced_set;
	boolean classic; /* the gametype's WEAPON SPAWNS is CLASSIC: plain weapons, every spot static, no holograms */
	struct ae_weaponset weaponset; /* the set in play, resolved */
	boolean sidecar_read;
	boolean sidecar_ok;
	struct ae_spawner_file sidecar;
	struct spawner spawners[SPAWNERS_MAXIMUM];
} spawners;

/* the set's timed-drop channels in play (design 3.4, the host's): each holds the trigger spawners that name it */
struct channel_run
{
	struct ae_weaponset_channel const *definition;
	struct ae_spawner_channel rules;
	short members[AE_SPAWNER_CHANNEL_MEMBERS]; /* indexes into spawners.spawners */
};

static struct
{
	short count;
	struct channel_run runs[AE_WEAPONSET_CHANNELS_MAXIMUM];
} channels;

/* ---------- the set in play */

static int set_of_word(char const *name)
{
	if (!name || !name[0] || !strcmp(name, "off")) return -1;
	if (!strcmp(name, "auto")) return -2;
	return ae_weaponsets_builtin_id(name);
}

/* the gametype's set: its AECW set id when AE WEAPONS is on, else off */
static int set_of_gametype(void)
{
	struct ae_weapon_selection selection;

	ae_weapon_selection_of_game(game_engine_get_variant(), game_variant_options_get(), &selection);
	if (!selection.ae_weapons) return -1;
	if (selection.set_id >= AE_WEAPONSET_BUILTIN_COUNT) return AE_WEAPONSET_CASUAL; /* a larger id reads as casual */
	return (int)selection.set_id;
}

static int set_wanted(void)
{
	int set;

	if (spawners.set_forced)
		return spawners.forced_set;
	set = set_of_word(config_string("debug.weapon_set"));
	if (set == -2 || set < -1)
		return set_of_gametype();
	return set;
}

/* the gametype's WEAPON SPAWNS (the AE block's spawner_look; a guest's is the host's): TRUE for CLASSIC */
static boolean look_classic(void)
{
	struct ae_variant_ext const *ext = ae_variant_ext_game();

	return ext && ext->spawner_look == AE_EXT_SPAWNER_LOOK_CLASSIC;
}

/* whether the map's item n is in play by the layout's comp section under the set (AE_WEAPONSET_*, -1 off) */
static boolean item_in_set(long n, int set)
{
	return ae_layout_item_in_play(ae_layout_item_rule(n), set == AE_WEAPONSET_COMP);
}

int ae_spawners_set_id(void)
{
	return spawners.built ? spawners.set : -1;
}

/* reads a built-in set, with its parent resolved: FALSE when the text does not read (a generated file: never) */
static boolean load_set(int id, struct ae_weaponset *out)
{
	char error[96];
	char const *text = ae_weaponsets_builtin_text(ae_weaponsets_builtin_name(id));
	int line;

	if (!text) return FALSE;
	line = ae_weaponset_read(text, (long)strlen(text), out, error, sizeof(error));
	if (line)
	{
		platform_log("spawner: the %s set does not read (line %d: %s)", ae_weaponsets_builtin_name(id), line, error);
		return FALSE;
	}
	if (out->extends[0])
	{
		struct ae_weaponset parent;
		int parent_id = ae_weaponsets_builtin_id(out->extends);

		if (parent_id >= 0 && parent_id != id && load_set(parent_id, &parent))
			ae_weaponset_extend(out, &parent);
	}
	return TRUE;
}

/* ---------- conversion */

/* the class and tier of a weapon definition: the stock table for the globals' list, the registry row for a pack weapon */
static boolean class_of_definition(long definition_index, int *cls, int *tier)
{
	long list = weapon_definition_index_to_list_index(definition_index);
	struct ae_weapon_registry_row const *row;

	if (list >= 0 && list < LIST_STOCK_COUNT)
	{
		*cls = ae_spawner_stock_class((int)list);
		*tier = ae_spawner_stock_tier((int)list);
		return *cls >= 0;
	}
	row = ae_weapon_id_row_of_definition(definition_index);
	if (row && row->menu_kind)
	{
		char word[16];
		int k;

		for (k = 0; row->menu_kind[k] && k < 15; k++)
			word[k] = row->menu_kind[k] == ' ' ? '_' : row->menu_kind[k];
		word[k] = 0;
		*cls = ae_spawner_class_of_word(word);
		*tier = ae_spawner_tier_of_word(row->tier ? row->tier : "BASE");
		if (*tier < 0) *tier = row->power ? AE_SPAWNER_POWER : AE_SPAWNER_BASE;
		return *cls >= 0;
	}
	return FALSE;
}

/* the entry's class and tier by collection weight; FALSE when it holds no weapon (equipment: the map's) */
static boolean classify_entry(struct scenario_netgame_equipment const *equipment, int *cls, int *tier, long *heaviest)
{
	struct item_collection_definition *collection;
	struct item_permutation_definition const *permutations;
	int weights[AE_SPAWNER_CLASS_COUNT];
	int tiers[AE_SPAWNER_CLASS_COUNT];
	real heaviest_weight = -1.0f;
	long index;
	boolean any = FALSE;

	memset(weights, 0, sizeof(weights));
	memset(tiers, 0, sizeof(tiers));
	*heaviest = NONE;
	if (equipment->item_collection.index == NONE)
		return FALSE;
	collection = item_collection_definition_get(equipment->item_collection.index);
	permutations = (struct item_permutation_definition const *)xbox_pointer(collection->permutations.address);
	for (index = 0; index < collection->permutations.count; index++)
	{
		long definition_index = permutations[index].item.index;
		int c, t;

		if (definition_index == NONE || permutations[index].weight <= 0.0f ||
			object_definition_get(definition_index)->object.type != _object_type_weapon)
			continue;
		if (!class_of_definition(definition_index, &c, &t))
			continue;
		any = TRUE;
		weights[c] += (int)(permutations[index].weight * 1000.0f + 0.5f);
		if (t == AE_SPAWNER_POWER)
			tiers[c] = AE_SPAWNER_POWER;
		if (permutations[index].weight > heaviest_weight)
		{
			heaviest_weight = permutations[index].weight;
			*heaviest = definition_index;
		}
	}
	if (!any)
		return FALSE;
	*cls = ae_spawner_class_of_weights(weights);
	*tier = *cls == AE_SPAWNER_CLASS_ANY ? AE_SPAWNER_TIER_ANY : tiers[*cls];
	*tier = ae_spawner_tier_of(*tier, equipment->spawn_time);
	return TRUE;
}

static void short_name(long definition_index, char *text, int size)
{
	long list = definition_index == NONE ? NONE : weapon_definition_index_to_list_index(definition_index);
	struct ae_weapon_registry_row const *row = definition_index == NONE ? NULL : ae_weapon_id_row_of_definition(definition_index);

	if (row && row->display)
		snprintf(text, size, "%s", row->display);
	else if (list >= 0 && list < LIST_STOCK_COUNT)
		snprintf(text, size, "%s", stock_short[list]);
	else if (definition_index != NONE)
	{
		char const *name = tag_get_name(definition_index), *slash = name ? strrchr(name, '\\') : NULL;

		snprintf(text, size, "%s", slash ? slash + 1 : name ? name : "?");
	}
	else
		snprintf(text, size, "?");
}

/* a set's weapon ref to a definition this map and the cache give; NONE (with the reason) when they cannot */
static long resolve_ref(char const *word, char *reason, int size)
{
	char key[32];
	char const *colon;
	unsigned int id;
	struct ae_weapon_registry_row const *row;
	long definition_index;

	reason[0] = 0;
	if (!strncmp(word, "stock:", 6))
	{
		int stock = ae_spawner_stock_of_word(word + 6);

		if (stock < 0)
		{
			snprintf(reason, size, "unknown stock weapon");
			return NONE;
		}
		if (stock >= LIST_STOCK_COUNT)
		{
			snprintf(reason, size, "the map's %s is not in the globals' list", word + 6);
			return NONE;
		}
		definition_index = list_index_to_weapon_definition_index(stock);
		if (definition_index == NONE)
			snprintf(reason, size, "the map has no %s", word + 6);
		return definition_index;
	}
	colon = strchr(word, ':');
	snprintf(key, sizeof(key), "%.*s", colon ? (int)(colon - word) : (int)strlen(word), word);
	id = ae_weapon_registry_id_of_word(key);
	row = id ? ae_weapon_registry_row(id) : NULL;
	if (!row)
	{
		snprintf(reason, size, "not in the registry");
		return NONE;
	}
	if (!row->active)
	{
		snprintf(reason, size, "held (not in the release cache)");
		return NONE;
	}
	if (!ae_weapon_id_available(id))
	{
		snprintf(reason, size, "unavailable on this map");
		return NONE;
	}
	definition_index = ae_weapon_ids_give_definition(id, "a weapon spawner");
	if (definition_index == NONE)
	{
		snprintf(reason, size, "refused (the no-cache guard)");
		return NONE;
	}
	if (colon)
		platform_log("spawner: %s: config '%s' is not in the cache yet: the weapon plays as it is", word, colon + 1);
	return definition_index;
}

static void apply_set(struct spawner *spawner)
{
	struct ae_weaponset const *set = &spawners.weaponset;
	long seconds, delay = 0;
	int logic, k;
	unsigned long seed = ae_spawner_pool_seed(spawners.games * 2654435761UL, spawner->n);

	spawner->pool.count = 0;
	spawner->has_row = ae_weaponset_cell(set, spawner->cls, spawner->tier, &spawner->row) != 0;
	if (spawner->pinned != NONE)
	{
		/* a pinned weapon: that alone, on the row's logic and times (or the cell's defaults) */
		spawner->pool_definitions[0] = spawner->pinned;
		short_name(spawner->pinned, spawner->pool_word[0], sizeof(spawner->pool_word[0]));
		ae_spawner_pool_begin(&spawner->pool, 1, NULL, seed);
	}
	if (!spawner->has_row || spawner->row.mute)
	{
		if (spawner->has_row && spawner->row.mute)
		{
			spawner->machine.state = AE_SPAWNER_DORMANT;
			platform_log("spawner: %d: the %s set mutes %s %s: nothing spawns here", spawner->n, set->name,
				ae_spawner_class_word(spawner->cls), ae_spawner_tier_word(spawner->tier));
			ae_spawner_logic_begin(&spawner->machine, AE_SPAWNER_STATIC, 30 * TICKS_PER_SECOND, 0);
			spawner->machine.state = AE_SPAWNER_DORMANT;
			return;
		}
		/* no row: the map's own weapon on the cell's default logic and the map's own clock */
		logic = ae_spawner_default_logic(spawner->cls, spawner->tier);
		seconds = spawner->own_seconds ? spawner->own_seconds : 30;
		platform_log("spawner: %d: the %s set has no %s %s row: the map's own weapon, %s every %lds", spawner->n, set->name,
			ae_spawner_class_word(spawner->cls), ae_spawner_tier_word(spawner->tier), ae_spawner_logic_word(logic), seconds);
	}
	else
	{
		struct ae_weaponset_row const *row = &spawner->row;

		logic = row->logic >= 0 ? row->logic : ae_spawner_default_logic(spawner->cls, spawner->tier);
		seconds = row->seconds;
		if (set->map_time && spawner->tier == AE_SPAWNER_POWER && spawner->own_seconds)
			seconds = spawner->own_seconds;
		delay = row->delay;
		if (!spawner->channel[0] && row->channel[0])
			snprintf(spawner->channel, sizeof(spawner->channel), "%s", row->channel);
		if (spawner->pinned == NONE)
		{
			unsigned char weights[AE_SPAWNER_POOL_MAXIMUM];
			int count = 0;

			for (k = 0; k < row->weapon_count && k < AE_SPAWNER_POOL_MAXIMUM; k++)
			{
				char reason[64];
				char const *ref = ae_weaponset_swapped(set, row->weapons[k]);
				long definition_index = resolve_ref(ref, reason, sizeof(reason));

				if (definition_index == NONE)
				{
					platform_log("spawner: %d: %s dropped from the %s pool: %s", spawner->n, ref, set->name, reason);
					continue;
				}
				spawner->pool_definitions[count] = definition_index;
				snprintf(spawner->pool_word[count], sizeof(spawner->pool_word[0]), "%s", ref);
				weights[count] = row->weights[k];
				count++;
			}
			if (!count)
				platform_log("spawner: %d: the %s pool is empty here: the map's own weapon", spawner->n, set->name);
			ae_spawner_pool_begin(&spawner->pool, count, weights, seed);
		}
	}
	if (spawners.classic && spawner->pool.count > 1)
	{
		/* (CLASSIC: the pool's heaviest weapon alone, on every spawn of the spot) */
		int pick = ae_spawner_look_pick(spawner->pool.count, spawner->pool.weights);
		unsigned char weight = spawner->pool.weights[pick];

		spawner->pool_definitions[0] = spawner->pool_definitions[pick];
		memmove(spawner->pool_word[0], spawner->pool_word[pick], sizeof(spawner->pool_word[0]));
		ae_spawner_pool_begin(&spawner->pool, 1, &weight, seed);
	}
	(void)ae_spawner_pool_peek(&spawner->pool, set->draw); /* (the first count's weapon, for its ghost) */
	if (logic == AE_SPAWNER_TRIGGER && !spawner->channel[0])
		logic = AE_SPAWNER_STATIC; /* a trigger without a channel would never spawn */
	if (logic == AE_SPAWNER_TRIGGER)
	{
		/* (the channel's beat is its period: the timers and the ghost count in it) */
		struct ae_weaponset_channel const *channel = ae_weaponset_channel_of(set, spawner->channel);

		if (channel)
			seconds = channel->every_seconds;
		else
		{
			logic = AE_SPAWNER_STATIC;
			if (seconds <= 0)
				seconds = spawner->own_seconds ? spawner->own_seconds : 120;
			platform_log("spawner: %d: the %s set has no channel %s: a fixed clock of %lds instead", spawner->n, set->name,
				spawner->channel, seconds);
		}
	}
	if (spawners.classic && logic != AE_SPAWNER_STATIC)
	{
		/* (CLASSIC: every spot on the game's own clock; a timed drop's beat becomes its fixed period) */
		if (seconds <= 0)
			seconds = spawner->own_seconds ? spawner->own_seconds : 30;
		logic = ae_spawner_look_logic(TRUE, logic);
		spawner->channel[0] = 0;
	}
	ae_spawner_logic_begin(&spawner->machine, logic, seconds * TICKS_PER_SECOND, delay * TICKS_PER_SECOND);
}

static void log_spawner(struct spawner const *spawner, struct scenario_netgame_equipment const *equipment)
{
	char pool[256], merged[64];
	char const *name = tag_get_name(equipment->item_collection.index), *slash = name ? strrchr(name, '\\') : NULL;
	int k, used = 0;

	pool[0] = 0;
	for (k = 0; k < spawner->pool.count && used < (int)sizeof(pool) - 40; k++)
		used += snprintf(pool + used, sizeof(pool) - used, "%s%s", k ? " " : "", spawner->pool_word[k]);
	merged[0] = 0;
	for (k = 0, used = 0; k < spawner->merged_count && used < (int)sizeof(merged) - 12; k++)
		used += snprintf(merged + used, sizeof(merged) - used, "%s%d", k ? " " : " merges item ", (int)spawner->merged[k]);
	platform_log("spawner: %d %s %s %s logic %s respawn %lds delay %lds games %d %d %d %d%s at %.2f %.2f %.2f (map item %s, own %ds)%s pool %s",
		spawner->n, ae_spawner_class_word(spawner->cls), ae_spawner_tier_word(spawner->tier),
		ae_spawner_kind_word(spawner->kind), ae_spawner_logic_word(spawner->machine.logic),
		spawner->machine.period / TICKS_PER_SECOND, spawner->machine.delay / TICKS_PER_SECOND, equipment->game_type[0],
		equipment->game_type[1], equipment->game_type[2], equipment->game_type[3], spawner->active ? "" : " (not this game type)",
		spawner->position.x, spawner->position.y, spawner->position.z, slash ? slash + 1 : name ? name : "?",
		equipment->spawn_time, merged, pool[0] ? pool : "(the map's own)");
}

static struct scenario_netgame_equipment const *equipment_of(long n)
{
	struct scenario *scenario = global_scenario_get();

	return TAG_BLOCK_GET_ELEMENT(&scenario->netgame_equipment, n, struct scenario_netgame_equipment);
}

static struct spawner *spawner_of(long equipment_index)
{
	short k;

	if (!spawners.built)
		return NULL;
	for (k = 0; k < spawners.count; k++)
		if (spawners.spawners[k].n == equipment_index && !spawners.spawners[k].skipped)
			return &spawners.spawners[k];
	return NULL;
}

/* the spawner lines of this map's layout (ae_layout_spawners), read once per map: TRUE when it has some. (They were a
sidecar file beside the layout once: "sidecar" in this file names them still) */
static boolean sidecar(void)
{
	char error[96];
	long length = 0;
	char const *text;
	int line;

	if (spawners.sidecar_read)
		return spawners.sidecar_ok;
	spawners.sidecar_read = TRUE;
	spawners.sidecar_ok = FALSE;
	text = ae_layout_spawners(&length);
	if (!text)
		return FALSE;
	/* (the layout's own map and hash: checked against the map when the layout applied) */
	line = ae_spawner_file_read(text, length, &spawners.sidecar, error, sizeof(error));
	if (line)
	{
		platform_log("spawner: the layout's spawner lines are not applied: line %d: %s", line - 1, error);
		return FALSE;
	}
	spawners.sidecar_ok = TRUE;
	return TRUE;
}

/* the sidecar's edits of item n, before the set applies (retag, skip, split, pin); adds come after the list */
static void sidecar_edit(struct spawner *spawner)
{
	short k;

	if (!spawners.sidecar_ok)
		return;
	for (k = 0; k < spawners.sidecar.edit_count; k++)
	{
		struct ae_spawner_edit const *edit = &spawners.sidecar.edits[k];

		if (edit->n != spawner->n)
			continue;
		switch (edit->edit)
		{
		case AE_SPAWNER_EDIT_SKIP:
			spawner->skipped = TRUE;
			break;
		case AE_SPAWNER_EDIT_SPLIT:
			spawner->split = TRUE;
			break;
		case AE_SPAWNER_EDIT_PIN:
		{
			char reason[64];
			long definition_index = resolve_ref(edit->weapon, reason, sizeof(reason));

			if (definition_index == NONE)
				platform_log("spawner: %d: the sidecar's %s cannot be pinned: %s", spawner->n, edit->weapon, reason);
			else
				spawner->pinned = definition_index;
			break;
		}
		case AE_SPAWNER_EDIT_RETAG:
			if (edit->has & AE_SPAWNER_EDIT_HAS_KIND)
			{
				spawner->kind = edit->kind;
				if (ae_spawner_kind_tier(edit->kind) != AE_SPAWNER_TIER_ANY && !(edit->has & AE_SPAWNER_EDIT_HAS_TIER))
					spawner->tier = (signed char)ae_spawner_kind_tier(edit->kind);
			}
			if (edit->has & AE_SPAWNER_EDIT_HAS_CLASS) spawner->cls = edit->cls;
			if (edit->has & AE_SPAWNER_EDIT_HAS_TIER) spawner->tier = edit->tier;
			if (edit->has & AE_SPAWNER_EDIT_HAS_CHANNEL) snprintf(spawner->channel, sizeof(spawner->channel), "%s", edit->channel);
			if (edit->has & AE_SPAWNER_EDIT_HAS_SETS) spawner->sets = edit->sets;
			/* (logic, respawn, delay, games: applied over the row after the set, see retag_times) */
			break;
		default:
			break;
		}
	}
}

/* the sidecar's logic / respawn / delay over the set's row */
static void retag_times(struct spawner *spawner)
{
	short k;

	if (!spawners.sidecar_ok)
		return;
	for (k = 0; k < spawners.sidecar.edit_count; k++)
	{
		struct ae_spawner_edit const *edit = &spawners.sidecar.edits[k];
		int logic = spawner->machine.logic;
		long period = spawner->machine.period, delay = spawner->machine.delay;

		if (edit->n != spawner->n || edit->edit != AE_SPAWNER_EDIT_RETAG)
			continue;
		if (!(edit->has & (AE_SPAWNER_EDIT_HAS_LOGIC | AE_SPAWNER_EDIT_HAS_RESPAWN | AE_SPAWNER_EDIT_HAS_DELAY)))
			continue;
		if (edit->has & AE_SPAWNER_EDIT_HAS_LOGIC) logic = edit->logic;
		if (edit->has & AE_SPAWNER_EDIT_HAS_RESPAWN) period = edit->respawn * TICKS_PER_SECOND;
		if (edit->has & AE_SPAWNER_EDIT_HAS_DELAY) delay = edit->delay * TICKS_PER_SECOND;
		if (logic == AE_SPAWNER_TRIGGER && !spawner->channel[0]) logic = AE_SPAWNER_STATIC;
		ae_spawner_logic_begin(&spawner->machine, logic, period, delay);
	}
}

/* two converted items lying together are one spot: the second folds into the first */
static void merge_pairs(void)
{
	short a, b;

	for (a = 0; a < spawners.count; a++)
	{
		struct spawner *first = &spawners.spawners[a];

		if (first->skipped || first->split)
			continue;
		for (b = (short)(a + 1); b < spawners.count; b++)
		{
			struct spawner *second = &spawners.spawners[b];
			struct scenario_netgame_equipment const *ea = equipment_of(first->n), *eb = equipment_of(second->n);
			real pa[3], pb[3];

			if (second->skipped || second->split || second->merged_count || first->merged_count == MERGED_MAXIMUM)
				continue;
			if (memcmp(ea->game_type, eb->game_type, sizeof(ea->game_type)) != 0)
				continue; /* (placed in different game types: Hang 'em High's shotgun over its sniper) */
			pa[0] = first->position.x; pa[1] = first->position.y; pa[2] = first->position.z;
			pb[0] = second->position.x; pb[1] = second->position.y; pb[2] = second->position.z;
			if (!ae_spawner_merges(pa, pb, 1, 1))
				continue;
			first->merged[first->merged_count++] = second->n;
			first->tier = (signed char)ae_spawner_merged_tier(first->tier, second->tier);
			if (first->cls != second->cls)
				first->cls = AE_SPAWNER_CLASS_ANY;
			second->skipped = TRUE; /* (its item stays, but the first spot owns the pair: the CE loop skips it) */
			second->merged_count = -1;
		}
	}
}

static void convert(int set_index, boolean keep_state)
{
	struct scenario *scenario = global_scenario_get();
	static struct spawner before[SPAWNERS_MAXIMUM];
	short before_count = keep_state ? spawners.count : 0;
	long n;
	short k;

	if (keep_state)
		memcpy(before, spawners.spawners, sizeof(before));
	spawners.count = 0;
	spawners.classic = look_classic();
	if (!load_set(set_index, &spawners.weaponset))
	{
		spawners.built = FALSE;
		return;
	}
	if (!keep_state)
	{
		spawners.sidecar_read = FALSE;
		sidecar();
	}
	if (spawners.sidecar_ok && !spawners.sidecar.auto_convert)
		platform_log("spawner: the sidecar says auto off: only its own lines make spawners");
	for (n = 0; scenario && n < scenario->netgame_equipment.count && spawners.count < SPAWNERS_MAXIMUM; n++)
	{
		struct scenario_netgame_equipment const *equipment = equipment_of(n);
		struct spawner *spawner = &spawners.spawners[spawners.count];
		int cls, tier;
		long heaviest;

		if (!classify_entry(equipment, &cls, &tier, &heaviest))
			continue;
		/* (an item the layout's comp section keeps out of this set: no spawner, and nothing spawns there) */
		if (!item_in_set(n, set_index))
			continue;
		memset(spawner, 0, sizeof(*spawner));
		spawner->n = (short)n;
		spawner->cls = (signed char)cls;
		spawner->tier = (signed char)tier;
		spawner->kind = (signed char)(tier == AE_SPAWNER_POWER ? AE_SPAWNER_PAD : AE_SPAWNER_RACK);
		spawner->sets = AE_SPAWNER_SETS_ALL;
		spawner->own_seconds = equipment->spawn_time;
		spawner->map_definition = heaviest;
		spawner->position = equipment->position;
		spawner->active = game_engine_matches_game_type(equipment->game_type);
		spawner->object = NONE;
		spawner->made_object = NONE;
		spawner->pinned = NONE;
		if (spawners.sidecar_ok && !spawners.sidecar.auto_convert)
			spawner->skipped = TRUE; /* (only a sidecar line that names it brings it back: a retag) */
		sidecar_edit(spawner);
		if (spawners.sidecar_ok && !spawners.sidecar.auto_convert && spawner->skipped)
		{
			short e;

			for (e = 0; e < spawners.sidecar.edit_count; e++)
				if (spawners.sidecar.edits[e].n == spawner->n && spawners.sidecar.edits[e].edit == AE_SPAWNER_EDIT_RETAG)
					spawner->skipped = FALSE;
		}
		/* (a layout's pack weapon on the spot: that weapon alone, as the layout's author asked) */
		if (spawner->pinned == NONE && ae_layout_item_pack_slot(n) != NONE)
		{
			long pinned = ae_weapon_ids_give_definition((unsigned int)ae_layout_item_pack_slot(n), "a layout's spawner");

			if (pinned != NONE)
				spawner->pinned = pinned;
		}
		if (!(spawner->sets & (1ul << set_index)))
			spawner->skipped = TRUE; /* (not under this set: the CE loop spawns the map's own item) */
		spawners.count++;
	}
	merge_pairs();
	for (k = 0; k < spawners.count; k++)
	{
		struct spawner *spawner = &spawners.spawners[k];
		short j;

		if (spawner->skipped)
			continue;
		apply_set(spawner);
		retag_times(spawner);
		if (spawners.classic && spawner->machine.logic != AE_SPAWNER_STATIC && spawner->machine.state != AE_SPAWNER_DORMANT)
		{
			/* (CLASSIC over the layout's own spawner logic too) */
			spawner->channel[0] = 0;
			ae_spawner_logic_begin(&spawner->machine, AE_SPAWNER_STATIC, spawner->machine.period, spawner->machine.delay);
		}
		for (j = 0; j < before_count; j++)
		{
			if (before[j].n == spawner->n && !before[j].skipped)
			{
				/* a set switch: what is on the map stays, a running count keeps its time */
				struct ae_spawner_machine kept = before[j].machine;

				spawner->object = before[j].object;
				memcpy(spawner->weapon, before[j].weapon, sizeof(spawner->weapon));
				ae_spawner_logic_retime(&kept, spawner->machine.logic, spawner->machine.period, spawner->machine.delay,
					game_time_get());
				if (spawner->machine.state != AE_SPAWNER_DORMANT)
					spawner->machine = kept;
			}
		}
		/* (the set's rules: a POWER pad's static beat skipped right after a pickup, under `rules reference`) */
		spawner->machine.rules = (unsigned char)(spawners.weaponset.rules_reference && spawner->tier == AE_SPAWNER_POWER ?
			AE_SPAWNER_RULE_STATIC_SKIP : 0);
		log_spawner(spawner, equipment_of(spawner->n));
	}
	platform_log("spawner: %d spawners on this map under the %s set (%ld map items, rack ammo %s, rules %s, look %s)",
		spawners.count, spawners.weaponset.name, scenario ? (long)scenario->netgame_equipment.count : 0L,
		spawners.weaponset.rack_ammo_fresh ? "fresh" : "topup", spawners.weaponset.rules_reference ? "reference" : "ae",
		spawners.classic ? "classic" : "racks");
	spawners.scenario = scenario;
	spawners.set = set_index;
	spawners.built_set = set_index;
	spawners.built = TRUE;
}

/* the channels of the set in play and their members; the first beat at the members' smallest delay (a game's start),
or a beat from now after a set switch */
static void channels_begin(long now, boolean switched)
{
	struct ae_weaponset const *set = &spawners.weaponset;
	int c;

	memset(&channels, 0, sizeof(channels));
	for (c = 0; c < set->channel_count && channels.count < AE_WEAPONSET_CHANNELS_MAXIMUM; c++)
	{
		struct ae_weaponset_channel const *definition = &set->channels[c];
		struct channel_run *run = &channels.runs[channels.count];
		long every = (long)definition->every_seconds * TICKS_PER_SECOND, first_at = -1;
		int members = 0;
		short k;

		for (k = 0; k < spawners.count && members < AE_SPAWNER_CHANNEL_MEMBERS; k++)
		{
			struct spawner const *spawner = &spawners.spawners[k];

			if (spawner->skipped || spawner->merged_count < 0 || spawner->machine.logic != AE_SPAWNER_TRIGGER ||
				spawner->machine.state == AE_SPAWNER_DORMANT || ae_weaponset_channel_of(set, spawner->channel) != definition)
				continue;
			run->members[members++] = k;
			if (first_at < 0 || spawner->machine.delay < first_at)
				first_at = spawner->machine.delay;
		}
		if (!members)
			continue;
		if (switched)
			first_at = now + every;
		run->definition = definition;
		ae_spawner_channel_begin(&run->rules, members, every, definition->variance_percent, first_at,
			ae_spawner_pool_seed(spawners.games * 2654435761UL, SPAWNERS_MAXIMUM + c));
		platform_log("spawner: channel %s: %d spawners, a drop every %ds (+-%d%%), the first at tick %ld", definition->name,
			members, (int)definition->every_seconds, (int)definition->variance_percent, run->rules.beat_at);
		channels.count++;
	}
}

/* each tick on the host: a channel's beat fires its next free member (the spawner's own step makes the weapon) */
static void channels_tick(long now)
{
	short c;

	for (c = 0; c < channels.count; c++)
	{
		struct channel_run *run = &channels.runs[c];
		unsigned char busy[AE_SPAWNER_CHANNEL_MEMBERS];
		int m;

		for (m = 0; m < run->rules.count; m++)
		{
			struct spawner const *spawner = &spawners.spawners[run->members[m]];

			busy[m] = (unsigned char)(!spawner->active || spawner->machine.state == AE_SPAWNER_DORMANT ||
				spawner->machine.has_object || spawner->machine.next_at >= 0 || spawner->hold_red_until != 0);
		}
		m = ae_spawner_channel_step(&run->rules, now, busy);
		if (m < 0)
			continue;
		ae_spawner_logic_fire(&spawners.spawners[run->members[m]].machine);
		error(_error_silent, "spawner: %d: channel %s drops at tick %ld (the next beat at tick %ld)",
			spawners.spawners[run->members[m]].n, run->definition->name, now, run->rules.beat_at);
	}
}

static void reset_states(void)
{
	short k;

	spawners.games++;
	for (k = 0; k < spawners.count; k++)
	{
		struct spawner *spawner = &spawners.spawners[k];
		boolean dormant = spawner->machine.state == AE_SPAWNER_DORMANT;

		spawner->active = game_engine_matches_game_type(equipment_of(spawner->n)->game_type);
		spawner->object = NONE;
		spawner->made_object = NONE;
		spawner->weapon[0] = 0;
		spawner->hold_red_until = 0;
		ae_spawner_logic_reset(&spawner->machine);
		if (dormant)
			spawner->machine.state = AE_SPAWNER_DORMANT;
		ae_spawner_pool_begin(&spawner->pool, spawner->pool.count, spawner->pool.weights,
			ae_spawner_pool_seed(spawners.games * 2654435761UL, spawner->n));
		(void)ae_spawner_pool_peek(&spawner->pool, spawners.weaponset.draw);
	}
	channels_begin(game_time_get(), FALSE);
}

/* whether the spawners run, and their table is this game's */
static boolean ensure_built(void)
{
	int set_index = set_wanted();

	if (set_index < 0 || !game_engine_running())
	{
		if (spawners.built && set_index < 0)
			platform_log("spawner: weapon set off: the map's items as CE spawns them");
		spawners.built = FALSE;
		spawners.count = 0;
		channels.count = 0;
		return FALSE;
	}
	if (!spawners.built || spawners.scenario != global_scenario_get())
	{
		convert(set_index, FALSE);
		if (spawners.built)
			reset_states();
		return spawners.built;
	}
	if (set_index != spawners.built_set || look_classic() != spawners.classic)
	{
		error(_error_silent, "spawner: weapon set now %s, look %s (from the next spawn)", ae_weaponsets_builtin_name(set_index),
			look_classic() ? "classic" : "racks");
		convert(set_index, TRUE);
		channels_begin(game_time_get(), TRUE);
		item_timers_map_begin();
	}
	return spawners.built;
}

/* ---------- the state machine's engine half */

/* whether the game faces a map item the way its placement does (as game_engine_update_item_spawn's loop): MCC's tags,
Custom Edition's, or a layout's flag; elsewhere the item keeps the placement's default forward */
static boolean item_faces(struct scenario_netgame_equipment const *equipment)
{
	if (mcc_cache_tags_loaded())
		return TRUE;
#ifdef HALO_CUSTOM_EDITION
	if (cache_file_tags_are_ce() || ae_layout_equipment_faces(equipment->flags))
		return TRUE;
#endif
	(void)equipment;
	return FALSE;
}

static long place(struct spawner *spawner, struct scenario_netgame_equipment const *equipment, long definition_index,
	long life_ticks)
{
	struct object_placement_data placement_data;
	long object_index;

	object_placement_data_new(&placement_data, definition_index, NONE);
	placement_data.position = equipment->position;
	/* (every spawn of a spot faces as the map item did: its ghost's first pose is this one) */
	if (item_faces(equipment))
		vector3d_from_angle(&placement_data.forward, equipment->facing);
	object_index = object_new(&placement_data);
	if (object_index != NONE)
	{
		struct item_datum *item = item_get(object_index);

		object_set_garbage(object_index, FALSE);
		if (TEST_FLAG(equipment->flags, EQUIPMENT_AT_REST_BIT))
			SET_FLAG(item->object.flags, _object_at_rest_bit, TRUE);
		/* (a static spawner's weapon lives one period, as CE's; the others are kept while on their spot) */
		item->item.last_owned_time += life_ticks - 30 * TICKS_PER_SECOND;
		ae_medals_item_spawned(object_index);
		(void)spawner;
	}
	return object_index;
}

static void spawn(struct spawner *spawner, struct scenario_netgame_equipment const *equipment)
{
	long definition_index = NONE;
	long object_index;
	long now = game_time_get();
	char name[24];
	int pick;

	if (game_variant_options_get()->no_map_weapons || ae_gametype_skips_map_item(spawner->map_definition))
	{
		spawner->machine.state = AE_SPAWNER_DORMANT;
		return;
	}
	pick = ae_spawner_pool_draw(&spawner->pool, spawners.weaponset.draw);
	if (pick >= 0)
		definition_index = spawner->pool_definitions[pick];
	else
		definition_index = spawner->map_definition; /* (an empty pool: the map's own weapon, its heaviest) */
	if (definition_index == NONE)
	{
		ae_spawner_logic_made(&spawner->machine, now, FALSE);
		return;
	}
	object_index = place(spawner, equipment, definition_index,
		spawner->machine.logic == AE_SPAWNER_STATIC ? spawner->machine.period : 30 * TICKS_PER_SECOND);
	short_name(definition_index, name, sizeof(name));
	error(_error_silent, "spawner: %d made %s (%s %s %s) at tick %ld", spawner->n, name, ae_spawner_class_word(spawner->cls),
		ae_spawner_tier_word(spawner->tier), ae_spawner_logic_word(spawner->machine.logic), now);
	if (ae_dev_events_on())
	{
		char tag[160];

		ae_dev_event_text(tag, sizeof(tag), tag_get_name(definition_index));
		ae_dev_event("spawner", "\"n\":%d,\"class\":\"%s\",\"tier\":\"%s\",\"logic\":\"%s\",\"weapon\":\"%s\",\"x\":%.2f,\"y\":%.2f,\"z\":%.2f",
			(int)spawner->n, ae_spawner_class_word(spawner->cls), ae_spawner_tier_word(spawner->tier),
			ae_spawner_logic_word(spawner->machine.logic), tag, spawner->position.x, spawner->position.y, spawner->position.z);
	}
	memcpy(spawner->weapon, name, sizeof(spawner->weapon));
	spawner->object = spawner->machine.logic == AE_SPAWNER_STATIC ? NONE : object_index;
	spawner->made_object = spawner->machine.logic == AE_SPAWNER_STATIC ? object_index : NONE;
	ae_spawner_logic_made(&spawner->machine, now, object_index != NONE);
	/* (the next weapon is drawn now, so the ghost of the coming count shows the weapon that will land) */
	(void)ae_spawner_pool_peek(&spawner->pool, spawners.weaponset.draw);
}

/* an expired spawner's weapon out of play: gone, or empty and (the set's `rules ae`) not held */
static boolean out_of_play(long object_index)
{
	struct item_datum *item = item_try_and_get(object_index);
	boolean held;
	int spent = 0;

	if (!item)
		return (boolean)ae_spawner_out_of_play(0, 0, 0, spawners.weaponset.rules_reference);
	held = TEST_FLAG(item->item.flags, _item_attached_to_unit_bit);
	if (object_definition_get(item->definition_index)->object.type == _object_type_weapon)
	{
		spent = ae_bots_weapon_spent(object_index);
		if (spent < 0)
		{
			struct weapon_datum *weapon = weapon_get(object_index);

			spent = weapon->weapon.magazines[0].rounds_total + weapon->weapon.magazines[0].rounds_loaded <= 0;
		}
	}
	return (boolean)ae_spawner_out_of_play(1, held, spent > 0, spawners.weaponset.rules_reference);
}

static void update_logic(struct spawner *spawner, struct scenario_netgame_equipment const *equipment)
{
	long now = game_time_get();
	struct ae_spawner_sight sight;
	struct item_datum *item = spawner->object == NONE ? NULL : item_try_and_get(spawner->object);
	long period_seconds = spawner->machine.period / TICKS_PER_SECOND;

	if (spawner->hold_red_until)
	{
		/* (DEV MODE's `spawner red`: a POWER weapon taken and in play, for the seconds asked; then the count restarts) */
		if (now < spawner->hold_red_until)
		{
			spawner->machine.state = AE_SPAWNER_RED;
			return;
		}
		spawner->hold_red_until = 0;
		spawner->machine.state = AE_SPAWNER_WAITING;
		spawner->machine.has_object = FALSE;
		spawner->machine.taken = FALSE;
		spawner->machine.next_at = now + spawner->machine.period;
	}
	if (spawner->machine.logic == AE_SPAWNER_STATIC && spawner->made_object != NONE)
	{
		/* (a static spawner's weapon picked up: the set's rules may skip the beat right after) */
		struct item_datum *made = item_try_and_get(spawner->made_object);

		if (!made)
			spawner->made_object = NONE;
		else if (TEST_FLAG(made->item.flags, _item_attached_to_unit_bit))
		{
			ae_spawner_logic_static_taken(&spawner->machine, now);
			spawner->made_object = NONE;
		}
	}
	sight.exists = item != NULL;
	sight.held = item && TEST_FLAG(item->item.flags, _item_attached_to_unit_bit);
	sight.out_of_play = spawner->object != NONE && out_of_play(spawner->object);
	switch (ae_spawner_logic_step(&spawner->machine, now, &sight))
	{
	case AE_SPAWNER_DO_SPAWN:
		spawn(spawner, equipment);
		break;
	case AE_SPAWNER_DO_KEEP:
		/* (on its spot: kept there, as the purge would take it 30 s after it spawned) */
		item->item.last_owned_time = now;
		break;
	case AE_SPAWNER_DO_LOG_TAKEN:
		if (spawner->machine.state == AE_SPAWNER_RED)
			error(_error_silent, "spawner: %d taken at tick %ld (%s): red until it is out of play", spawner->n, now, spawner->weapon);
		else
		{
			error(_error_silent, "spawner: %d taken at tick %ld (%s): next in %lds", spawner->n, now, spawner->weapon, period_seconds);
			spawner->object = NONE;
		}
		break;
	case AE_SPAWNER_DO_LOG_LOST:
		error(_error_silent, "spawner: %d lost its weapon untaken at tick %ld: next in %lds", spawner->n, now, period_seconds);
		spawner->object = NONE;
		break;
	case AE_SPAWNER_DO_LOG_SKIPPED:
		error(_error_silent, "spawner: %d skips its beat at tick %ld: its weapon was taken %lds before", spawner->n, now,
			(now - spawner->machine.taken_at) / TICKS_PER_SECOND);
		break;
	case AE_SPAWNER_DO_LOG_EXPIRED:
		error(_error_silent, "spawner: %d expired at tick %ld (%s out of play): next in %lds", spawner->n, now, spawner->weapon,
			period_seconds);
		spawner->object = NONE;
		break;
	default:
		break;
	}
}

/* ---------- the state to guests (distributed message 128, ae_spawner_msg_rules.h) */

enum
{
	/* the host resends its view this often when no state changed (the guests count the seconds down themselves) */
	NET_SEND_EVERY_TICKS = 2 * TICKS_PER_SECOND,
};

struct ae_spawner_message
{
	struct distributed_message_header header;
	unsigned char body[AE_SPAWNER_MSG_SIZE];
};

static struct
{
	struct ae_spawner_msg sent;
	boolean valid;
	long sent_at;
} host_net;

static struct ae_spawner_msg client_msg;
static boolean client_valid;
static long client_at; /* the tick the message arrived */

static void msg_make(struct ae_spawner_msg *msg, long now)
{
	short k;

	memset(msg, 0, sizeof(*msg));
	msg->set = (unsigned char)(spawners.built && spawners.set >= 0 ? spawners.set : AE_SPAWNER_MSG_SET_OFF);
	for (k = 0; spawners.built && k < spawners.count && msg->count < AE_SPAWNER_MSG_MAXIMUM; k++)
	{
		struct spawner const *spawner = &spawners.spawners[k];
		struct ae_spawner_msg_entry *entry;

		if (spawner->skipped)
			continue;
		entry = &msg->entries[msg->count++];
		entry->item = (unsigned short)spawner->n;
		if (!spawner->active)
			entry->state = AE_SPAWNER_MSG_OFF;
		else
			entry->state = (unsigned char)ae_spawner_msg_state(spawner->machine.state,
				ae_spawner_logic_on_spot(&spawner->machine, now), ae_spawner_logic_ticks_left(&spawner->machine, now),
				&entry->seconds);
	}
}

static void net_send(long machine_index, struct ae_spawner_msg const *msg, long now)
{
	struct ae_spawner_message message;
	int size;

	memset(&message, 0, sizeof(message));
	size = ae_spawner_msg_encode(msg, message.body, sizeof(message.body));
	if (!size)
		return;
	if (machine_index == NONE)
	{
		distributed_send(&message, _distributed_message_ae_spawners, 1, (word)(sizeof(message.header) + size),
			_distributed_to_clients_reliably);
		host_net.sent = *msg;
		host_net.valid = TRUE;
		host_net.sent_at = now;
	}
	else
		distributed_send_to_machine_reliably(machine_index, &message, _distributed_message_ae_spawners, 1,
			(word)(sizeof(message.header) + size));
}

static boolean net_host(void)
{
	return game_connection() == _game_connection_network_server && !main_menu_is_active();
}

/* the host's view each tick: at once when a state changes, else on the slow beat */
static void net_host_tick(long now)
{
	struct ae_spawner_msg msg;

	if (!net_host())
		return;
	msg_make(&msg, now);
	if (host_net.valid && !ae_spawner_msg_states_differ(&host_net.sent, &msg) && now >= host_net.sent_at &&
		now - host_net.sent_at < NET_SEND_EVERY_TICKS)
	{
		return;
	}
	net_send(NONE, &msg, now);
}

/* the set went off (or the spawners are not this game's): the guests hear it once */
static void net_host_off(long now)
{
	struct ae_spawner_msg msg;

	if (!net_host() || !host_net.valid || host_net.sent.set == AE_SPAWNER_MSG_SET_OFF)
		return;
	msg_make(&msg, now);
	net_send(NONE, &msg, now);
}

void ae_spawners_send(long machine_index)
{
	long now = game_time_get();
	struct ae_spawner_msg msg;

	if (game_connection() != _game_connection_network_server || !spawners.built)
		return;
	msg_make(&msg, now);
	net_send(machine_index, &msg, now);
}

void ae_spawners_net_reset(void)
{
	memset(&client_msg, 0, sizeof(client_msg));
	client_valid = FALSE;
	client_at = 0;
	memset(&host_net, 0, sizeof(host_net));
}

int ae_spawners_handles_message(unsigned short const *message, unsigned short size)
{
	struct distributed_message_header header;

	if (size < sizeof(header))
		return 0;
	memcpy(&header, message, sizeof(header));
	return header.type == _distributed_message_ae_spawners;
}

/* (the host's alone, over its stream, whole: a body the reader takes) */
void ae_spawners_handle_message(long machine_index, unsigned short const *data, unsigned short size)
{
	struct ae_spawner_message message;
	struct ae_spawner_msg read;

	if (machine_index != NONE || game_connection() != _game_connection_network_client ||
		network_connection_last_read_was_unreliable() || size < sizeof(message.header) + AE_SPAWNER_MSG_HEAD ||
		size > sizeof(message))
	{
		return;
	}
	memcpy(&message, data, size);
	if (GET_MESSAGE_TYPE(message.header.header) != 2 || message.header.type != _distributed_message_ae_spawners ||
		message.header.count != 1)
	{
		return;
	}
	if (!ae_spawner_msg_decode(message.body, (int)(size - sizeof(message.header)), &read))
		return;
	if (!client_valid || read.set != client_msg.set)
		error(_error_silent, "spawner: guest state: set %s, %d spawners",
			read.set == AE_SPAWNER_MSG_SET_OFF ? "off" : ae_weaponsets_builtin_name(read.set), read.count);
	client_msg = read;
	client_valid = TRUE;
	client_at = game_time_get();
}

/* a guest's view of one spawner from the last message: its ticks left (counted down here) and whether its weapon is on
its spot; FALSE for an item it does not know or one that is off (the clock as before) */
static boolean guest_timer(long equipment_index, long *ticks_left, boolean *on_spot)
{
	struct ae_spawner_msg_entry const *entry;
	long now = game_time_get(), left;
	int k;

	if (!client_valid || (k = ae_spawner_msg_find(&client_msg, (int)equipment_index)) < 0)
		return FALSE;
	entry = &client_msg.entries[k];
	if (entry->state == AE_SPAWNER_MSG_OFF)
		return FALSE;
	left = (long)entry->seconds * TICKS_PER_SECOND - (now - client_at);
	*ticks_left = left < 1 ? 1 : left;
	*on_spot = entry->state == AE_SPAWNER_MSG_READY;
	/* (a pad's weapon taken inside its window: its waypoint goes off, not 20 s later) */
	if (*on_spot && !ae_spawner_holo_weapon_lies(equipment_index))
		*on_spot = FALSE;
	return TRUE;
}

/* ---------- the engine's tick */

void ae_spawners_tick(void)
{
	long now = game_time_get();

	if (!ensure_built())
	{
		net_host_off(now);
		return;
	}
	if (now < spawners.last_tick)
		reset_states();
	spawners.last_tick = now;
	channels_tick(now);
	net_host_tick(now);
}

boolean ae_spawners_update_entry(long equipment_index, struct scenario_netgame_equipment const *equipment)
{
	struct spawner *spawner;
	short k;

	/* (an item the layout's comp section keeps out of the set in play: nothing spawns there, the CE loop's either) */
	if (!item_in_set(equipment_index, spawners.built ? spawners.set : -1))
		return TRUE;
	if (!spawners.built)
		return FALSE;
	/* (an item merged into another spot: handled by that spot, nothing of its own) */
	for (k = 0; k < spawners.count; k++)
		if (spawners.spawners[k].n == equipment_index && spawners.spawners[k].merged_count < 0)
			return TRUE;
	spawner = spawner_of(equipment_index);
	if (!spawner)
		return FALSE;
	update_logic(spawner, equipment);
	return TRUE;
}

int ae_spawners_listing(long equipment_index, short *timer_class, long definition_index, long *class_definitions,
	unsigned short *classes)
{
	struct spawner *spawner;

	short k;

	if (!ensure_built())
		return item_in_set(equipment_index, -1) ? 0 : -1;
	if (!item_in_set(equipment_index, spawners.set))
		return -1;
	/* (the second weapon of a merged pair is its spawner's: not an entry of its own) */
	for (k = 0; k < spawners.count; k++)
		if (spawners.spawners[k].n == equipment_index && spawners.spawners[k].merged_count < 0)
			return -1;
	if (!(spawner = spawner_of(equipment_index)))
		return 0;
	if (spawner->tier != AE_SPAWNER_POWER)
		return -1;
	if (*timer_class >= NUMBER_OF_ITEM_TIMER_POWER_CLASSES && definition_index != NONE)
	{
		*timer_class = _item_timer_pad;
		class_definitions[_item_timer_pad] = definition_index;
		SET_FLAG(*classes, _item_timer_pad, TRUE);
	}
	return 1;
}

real ae_spawners_waypoint_fade(long equipment_index, real distance)
{
	/* (rc2: a pad's waypoint follows the item timers' rule, shown while its weapon lies on the map: no fade up close) */
	(void)equipment_index;
	(void)distance;
	return 1.0f;
}

boolean ae_spawners_refuses_ammo(long object_index)
{
	short k;

	if (!spawners.built || spawners.classic || !spawners.weaponset.rack_ammo_fresh || object_index == NONE)
		return FALSE;
	for (k = 0; k < spawners.count; k++)
		if (spawners.spawners[k].object == object_index && !spawners.spawners[k].machine.taken)
			return TRUE;
	return FALSE;
}

boolean ae_spawners_fresh_swap(long object_index)
{
	short k;

	/* (a set with `rack_ammo fresh`: the walk-over refill is refused, so the rack's fresh weapon must be takeable in a swap
	for the same weapon in hand: the weapon of a spawner lying on its spot; CLASSIC's plain weapons refill as CE's) */
	if (!spawners.built || spawners.classic || !spawners.weaponset.rack_ammo_fresh || object_index == NONE)
		return FALSE;
	for (k = 0; k < spawners.count; k++)
	{
		struct spawner const *spawner = &spawners.spawners[k];
		struct item_datum *item;
		real dx, dy;

		if (spawner->skipped || spawner->merged_count < 0 || !spawner->active || spawner->tier != AE_SPAWNER_BASE)
			continue;
		if (spawner->object == object_index)
			return TRUE;
		if (spawner->object != NONE || !(item = item_try_and_get(object_index)))
			continue;
		dx = item->object.position.x - spawner->position.x;
		dy = item->object.position.y - spawner->position.y;
		if (dx * dx + dy * dy < 1.0f)
			return TRUE;
	}
	return FALSE;
}

/* ---------- the item timers */

boolean ae_spawners_timer_period(long equipment_index, long *period_ticks)
{
	struct spawner *spawner;

	/* (a guest builds the table too: the same map, set and sidecar give the same periods and tiers; it never spawns) */
	if (!ensure_built())
		return FALSE;
	spawner = spawner_of(equipment_index);
	if (!spawner)
		return FALSE;
	*period_ticks = spawner->machine.period;
	return TRUE;
}

boolean ae_spawners_timer(long equipment_index, long *ticks_left, boolean *on_spot)
{
	struct spawner *spawner;
	long now = game_time_get();

	if (network_game_distributed_client())
		return guest_timer(equipment_index, ticks_left, on_spot);
	spawner = spawner_of(equipment_index);
	if (!spawner)
		return FALSE;
	*ticks_left = ae_spawner_logic_ticks_left(&spawner->machine, now);
	*on_spot = ae_spawner_logic_on_spot(&spawner->machine, now);
	/* (a static pad's window is its clock's 20 s: when its weapon is taken in it the waypoint goes off then, not 20 s
	later; ae_spawner_holo_weapon_lies keeps the 10 ticks after a spawn) */
	if (*on_spot && spawner->machine.logic == AE_SPAWNER_STATIC && !ae_spawner_holo_weapon_lies(equipment_index))
		*on_spot = FALSE;
	return TRUE;
}

boolean ae_spawners_timer_red(long equipment_index)
{
	struct spawner *spawner;
	int k;

	/* (the set's `rules reference`: a POWER pad never shows red; a guest's table and set are the host's) */
	spawner = spawner_of(equipment_index);
	if (network_game_distributed_client())
	{
		return client_valid && (k = ae_spawner_msg_find(&client_msg, (int)equipment_index)) >= 0 &&
			ae_spawner_shown_red(client_msg.entries[k].state == AE_SPAWNER_MSG_RED,
				spawner && spawner->tier == AE_SPAWNER_POWER, spawners.weaponset.rules_reference);
	}
	return spawner && ae_spawner_shown_red(spawner->machine.state == AE_SPAWNER_RED, spawner->tier == AE_SPAWNER_POWER,
		spawners.weaponset.rules_reference);
}

/* the weapon a spawner makes next: the one its pool drew ahead, else the pool's first, else the map's own */
static long next_definition(struct spawner const *spawner)
{
	if (spawner->pool.count > 0 && spawner->pool.ahead >= 0 && spawner->pool.ahead < spawner->pool.count &&
		spawner->pool_definitions[spawner->pool.ahead] != NONE)
		return spawner->pool_definitions[spawner->pool.ahead];
	if (spawner->pool.count > 0 && spawner->pool_definitions[0] != NONE)
		return spawner->pool_definitions[0];
	return spawner->map_definition;
}

/* ---------- DEV MODE's `spawner` */

boolean ae_spawners_dev_stage(char const *what, long seconds, real_point3d const *from, real_point3d *spot, char *text,
	int size)
{
	long now = game_time_get();
	struct spawner *spawner = NULL;
	real nearest = 8.0f * 8.0f;
	short k;
	struct object_iterator iterator;
	char name[24];

	if (!ensure_built())
	{
		snprintf(text, (size_t)size, "no weapon set is running: the spawners are off");
		return FALSE;
	}
	for (k = 0; k < spawners.count; k++)
	{
		struct spawner *candidate = &spawners.spawners[k];
		real dx = candidate->position.x - from->x, dy = candidate->position.y - from->y, dz = candidate->position.z - from->z;
		real d = dx * dx + dy * dy + dz * dz;

		if (candidate->skipped || candidate->merged_count < 0 || !candidate->active ||
			candidate->machine.state == AE_SPAWNER_DORMANT || d >= nearest)
			continue;
		nearest = d;
		spawner = candidate;
	}
	if (!spawner)
	{
		snprintf(text, (size_t)size, "no weapon spawner within 8 world units");
		return FALSE;
	}
	if (network_game_distributed_client())
	{
		snprintf(text, (size_t)size, "the host's spawners are not staged from a guest");
		return FALSE;
	}
	if (spot)
		*spot = spawner->position;
	if (!strcmp(what, "ready"))
	{
		/* (its next weapon on the next tick) */
		spawner->hold_red_until = 0;
		if (spawner->machine.logic == AE_SPAWNER_STATIC)
			spawner->machine.delay = now + 1;
		else if (!spawner->machine.has_object)
			spawner->machine.next_at = now;
		snprintf(text, (size_t)size, "spawner %d: its next weapon now", (int)spawner->n);
		return TRUE;
	}
	/* (the weapons lying on its spot go) */
	object_iterator_new(&iterator, _object_mask_weapon, 0);
	while (object_iterator_next(&iterator))
	{
		struct item_datum *item = item_try_and_get(iterator.index);
		real dx, dy;

		if (!item || item->object.parent_object_index != NONE || TEST_FLAG(item->item.flags, _item_attached_to_unit_bit))
			continue;
		dx = item->object.position.x - spawner->position.x;
		dy = item->object.position.y - spawner->position.y;
		if (dx * dx + dy * dy < 1.0f)
			object_delete(iterator.index);
	}
	spawner->object = NONE;
	spawner->machine.has_object = FALSE;
	spawner->machine.taken = FALSE;
	if (!strcmp(what, "red"))
	{
		spawner->hold_red_until = now + seconds * TICKS_PER_SECOND;
		spawner->machine.state = AE_SPAWNER_RED;
		snprintf(text, (size_t)size, "spawner %d: RED for %ld s (a weapon of it taken and in play)", (int)spawner->n, seconds);
		return TRUE;
	}
	spawner->hold_red_until = 0;
	spawner->machine.state = AE_SPAWNER_WAITING;
	if (spawner->machine.logic == AE_SPAWNER_STATIC)
		spawner->machine.delay = now + seconds * TICKS_PER_SECOND;
	else
		spawner->machine.next_at = now + seconds * TICKS_PER_SECOND;
	short_name(next_definition(spawner), name, sizeof(name));
	snprintf(text, (size_t)size, "spawner %d (%s %s): %s returns in %ld s", (int)spawner->n, ae_spawner_class_word(spawner->cls),
		ae_spawner_tier_word(spawner->tier), name, seconds);
	return TRUE;
}

/* ---------- the holograms' view (ae_spawner_holo.c) */

int ae_spawners_holo_count(void)
{
	/* (CLASSIC: plain weapons, no holograms) */
	if (!ensure_built() || spawners.classic)
		return 0;
	return spawners.count;
}

boolean ae_spawners_holo_get(int k, struct ae_spawners_holo *out)
{
	struct spawner const *spawner;
	struct scenario_netgame_equipment const *equipment;
	long now = game_time_get(), ticks_left;
	int state, j;
	unsigned short seconds;

	if (!spawners.built || k < 0 || k >= spawners.count)
		return FALSE;
	spawner = &spawners.spawners[k];
	/* (an item merged into another spot is that spot's; a sidecar `skip` is a plain map item) */
	if (spawner->skipped || spawner->merged_count < 0 || !spawner->active)
		return FALSE;
	equipment = equipment_of(spawner->n);
	if (network_game_distributed_client())
	{
		if (!client_valid || (j = ae_spawner_msg_find(&client_msg, spawner->n)) < 0)
			return FALSE;
		state = client_msg.entries[j].state;
		ticks_left = (long)client_msg.entries[j].seconds * TICKS_PER_SECOND - (now - client_at);
		if (ticks_left < 1)
			ticks_left = 1;
	}
	else
	{
		ticks_left = ae_spawner_logic_ticks_left(&spawner->machine, now);
		state = ae_spawner_msg_state(spawner->machine.state, ae_spawner_logic_on_spot(&spawner->machine, now), ticks_left,
			&seconds);
	}
	if (state == AE_SPAWNER_MSG_OFF)
		return FALSE;
	/* (the set's `rules reference`: a taken POWER pad shows its ghost waiting, never red) */
	if (state == AE_SPAWNER_MSG_RED && !ae_spawner_shown_red(1, spawner->tier == AE_SPAWNER_POWER, spawners.weaponset.rules_reference))
		state = AE_SPAWNER_MSG_WAITING;
	out->equipment_index = spawner->n;
	out->position = equipment->position;
	out->facing = equipment->facing;
	out->faces_item = item_faces(equipment);
	out->state = state;
	out->power = spawner->tier == AE_SPAWNER_POWER;
	out->ticks_left = ticks_left;
	out->period = spawner->machine.period;
	/* (the host knows the weapon drawn for the coming count; a guest has no pool running: the pool's first) */
	out->definition = next_definition(spawner);
	out->definition_next = !network_game_distributed_client() && out->definition != NONE;
	out->object_index = network_game_distributed_client() ? NONE : spawner->object;
	return TRUE;
}

boolean ae_spawners_holo_accepts(int k, long definition)
{
	struct spawner const *spawner;
	int j;

	if (!spawners.built || k < 0 || k >= spawners.count || definition == NONE)
		return FALSE;
	spawner = &spawners.spawners[k];
	if (definition == spawner->map_definition)
		return TRUE;
	for (j = 0; j < spawner->pool.count && j < AE_SPAWNER_POOL_MAXIMUM; j++)
		if (spawner->pool_definitions[j] == definition)
			return TRUE;
	return FALSE;
}

boolean ae_spawners_set_choose(char const *name, char *text, int size)
{
	int index = set_of_word(name);

	if (index < -2 || (index == -1 && strcmp(name, "off") != 0 && name[0]))
	{
		snprintf(text, size, "no weapon set '%s' (casual, comp, btb, off, auto)", name);
		return FALSE;
	}
	if (index == -2)
	{
		spawners.set_forced = FALSE;
		snprintf(text, size, "weapon set follows the gametype from the next spawn");
		return TRUE;
	}
	spawners.set_forced = TRUE;
	spawners.forced_set = index;
	snprintf(text, size, "weapon set %s from the next spawn", index < 0 ? "off" : ae_weaponsets_builtin_name(index));
	return TRUE;
}

/* ---------- the dev view (debug.spawners) */

/* OG theme: the class colours (no amber), the HUD's white and dark, the menus' ice */
static real const class_rgb[AE_SPAWNER_CLASS_COUNT][3] =
{
	{ 60 / 255.0f, 205 / 255.0f, 205 / 255.0f }, { 165 / 255.0f, 140 / 255.0f, 255 / 255.0f },
	{ 110 / 255.0f, 222 / 255.0f, 140 / 255.0f }, { 255 / 255.0f, 143 / 255.0f, 190 / 255.0f },
	{ 232 / 255.0f, 238 / 255.0f, 247 / 255.0f }, { 255 / 255.0f, 100 / 255.0f, 100 / 255.0f },
	{ 214 / 255.0f, 107 / 255.0f, 255 / 255.0f }, { 150 / 255.0f, 170 / 255.0f, 204 / 255.0f },
};
static char const class_letters[AE_SPAWNER_CLASS_COUNT] = { 'R', 'A', 'P', 'C', 'L', 'H', 'M', '?' };
static real const white_rgb[3] = { 240 / 255.0f, 246 / 255.0f, 255 / 255.0f };
static real const ice_rgb[3] = { 184 / 255.0f, 214 / 255.0f, 255 / 255.0f };
static real const muted_rgb[3] = { 150 / 255.0f, 170 / 255.0f, 204 / 255.0f };
static real const dark_rgb[3] = { 6 / 255.0f, 12 / 255.0f, 28 / 255.0f };
static real const red_rgb[3] = { 255 / 255.0f, 84 / 255.0f, 96 / 255.0f };

#define PI_F 3.14159265f
#define CHIP_BASE 14.0f
#define CHIP_POWER 18.0f
#define CHIP_LIFT 0.45f
#define CHIP_RANGE 70.0f
#define CHIP_DETAIL_NEAR 18.0f
#define CHIP_DETAIL_FAR 32.0f
#define TEXT_PX 15.0f
#define TEXT_FLOOR 12.0f

static union real_argb_color colour(real const rgb[3], real alpha)
{
	union real_argb_color c;

	c.alpha = alpha;
	c.red = rgb[0];
	c.green = rgb[1];
	c.blue = rgb[2];
	return c;
}

static void disc(real cx, real cy, real radius, union real_argb_color const *c)
{
	int step;

	for (step = 0; step < 24; step++)
	{
		real a0 = (real)step * 2.0f * PI_F / 24.0f, a1 = (real)(step + 1) * 2.0f * PI_F / 24.0f;
		real points[4][2];

		points[0][0] = cx; points[0][1] = cy;
		points[1][0] = cx + radius * (real)sin(a0); points[1][1] = cy - radius * (real)cos(a0);
		points[2][0] = cx + radius * (real)sin(a1); points[2][1] = cy - radius * (real)cos(a1);
		points[3][0] = cx; points[3][1] = cy;
		ae_hud_fill_quad_color(points, c);
	}
}

static void ring(real cx, real cy, real radius, real width, union real_argb_color const *c)
{
	int step;
	real inner = radius - width / 2.0f, outer = radius + width / 2.0f;

	for (step = 0; step < 32; step++)
	{
		real a0 = (real)step * 2.0f * PI_F / 32.0f, a1 = (real)(step + 1) * 2.0f * PI_F / 32.0f;
		real points[4][2];

		points[0][0] = cx + inner * (real)sin(a0); points[0][1] = cy - inner * (real)cos(a0);
		points[1][0] = cx + outer * (real)sin(a0); points[1][1] = cy - outer * (real)cos(a0);
		points[2][0] = cx + outer * (real)sin(a1); points[2][1] = cy - outer * (real)cos(a1);
		points[3][0] = cx + inner * (real)sin(a1); points[3][1] = cy - inner * (real)cos(a1);
		ae_hud_fill_quad_color(points, c);
	}
}

static void text_at(short view, char const *text, real x, real baseline, real px, real const rgb[3], real alpha)
{
	wchar_t wide[48];
	union real_argb_color c = colour(rgb, alpha);
	int k;
	real width;

	for (k = 0; k < 47 && text[k]; k++)
		wide[k] = (wchar_t)(unsigned char)text[k];
	wide[k] = 0;
	width = ae_hud_text_measure(wide, px);
	ae_hud_text_draw_ex(view, (real)floor(x - width / 2.0f + 0.5f), (real)floor(baseline + 0.5f), wide, px, &c, _ae_text_outline);
}

static void status_of(struct spawner const *spawner, long now, char *text, int size, real const **rgb)
{
	long ticks;
	boolean on_spot;

	*rgb = white_rgb;
	if (spawner->machine.state == AE_SPAWNER_DORMANT)
	{
		snprintf(text, size, "OFF IN THIS GAME");
		*rgb = muted_rgb;
		return;
	}
	if (spawner->machine.logic != AE_SPAWNER_STATIC && spawner->machine.state == AE_SPAWNER_READY)
	{
		snprintf(text, size, "READY");
		*rgb = ice_rgb;
		return;
	}
	if (spawner->machine.state == AE_SPAWNER_RED)
	{
		snprintf(text, size, "RED");
		*rgb = red_rgb;
		return;
	}
	if (spawner->machine.logic == AE_SPAWNER_TRIGGER && spawner->machine.next_at < 0)
	{
		snprintf(text, size, "ON %s", spawner->channel);
		*rgb = muted_rgb;
		return;
	}
	if (!ae_spawners_timer(spawner->n, &ticks, &on_spot))
	{
		snprintf(text, size, "WAITING");
		*rgb = muted_rgb;
		return;
	}
	(void)now;
	snprintf(text, size, "%ld:%02ld", (ticks + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND / 60,
		((ticks + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND) % 60);
}

void ae_spawners_draw(short local_player_index)
{
	real ppu = ae_hud_pixels_per_unit(), s = ae_score_strip_view_scale();
	long now = game_time_get();
	short k;

	if (local_player_index == NONE || !spawners.built || network_game_distributed_client() ||
		!config_boolean("debug.spawners") || ppu <= 0.0f || s <= 0.0f)
		return;
	for (k = 0; k < spawners.count; k++)
	{
		struct spawner const *spawner = &spawners.spawners[k];
		real_point3d spot, view_position;
		real_point2d screen;
		real x, y, radius, dx, dy, dz, distance, px, below;
		union real_argb_color c;
		char letter[2], status[24], facts[48];
		real const *status_rgb;
		boolean power = spawner->tier == AE_SPAWNER_POWER;

		if (!spawner->active || spawner->skipped)
			continue;
		spot = spawner->position;
		spot.z += CHIP_LIFT;
		dx = spot.x - render.camera.position.x;
		dy = spot.y - render.camera.position.y;
		dz = spot.z - render.camera.position.z;
		distance = (real)sqrt(dx * dx + dy * dy + dz * dz);
		if (distance > CHIP_RANGE)
			continue;
		matrix4x3_transform_point(&render.frustum.world_to_view, &spot, &view_position);
		if (!render_camera_view_to_screen(&render.camera, &render.frustum, &view_position, &screen))
			continue;
		x = (screen.x - (real)render.camera.viewport_bounds.x0) * ppu;
		y = (screen.y - (real)render.camera.viewport_bounds.y0) * ppu;
		radius = (power ? CHIP_POWER : CHIP_BASE) * s;
		ae_hud_quads_begin();
		/* (dark keyline, the class's fill; a POWER pad's white ring; a red rack's red ring outside it) */
		c = colour(dark_rgb, 0.85f);
		disc(x, y, radius + 1.6f * s, &c);
		c = colour(class_rgb[(int)spawner->cls], spawner->machine.state == AE_SPAWNER_READY ||
			spawner->machine.logic == AE_SPAWNER_STATIC ? 0.95f : 0.55f);
		disc(x, y, radius, &c);
		if (power)
		{
			c = colour(white_rgb, 0.95f);
			ring(x, y, radius + 4.0f * s, 2.2f * s, &c);
		}
		if (spawner->machine.state == AE_SPAWNER_RED)
		{
			c = colour(red_rgb, 1.0f);
			ring(x, y, radius + (power ? 8.0f : 4.0f) * s, 3.0f * s, &c);
		}
		ae_hud_quads_end();
		px = TEXT_PX * s < TEXT_FLOOR ? TEXT_FLOOR : TEXT_PX * s;
		letter[0] = class_letters[(int)spawner->cls];
		letter[1] = 0;
		text_at(local_player_index, letter, x, y + px * 0.36f, px * 1.05f, dark_rgb, 1.0f);
		/* (over the chip: the weapon it made last; under it: the state, then the row. Far chips keep the state alone,
		past CHIP_DETAIL_FAR the chip alone: a base's dozen spots stay readable from across the map) */
		if (spawner->weapon[0] && distance < CHIP_DETAIL_NEAR)
			text_at(local_player_index, spawner->weapon, x, y - radius - 7.0f * s, px, white_rgb, 0.95f);
		if (distance >= CHIP_DETAIL_FAR)
			continue;
		status_of(spawner, now, status, sizeof(status), &status_rgb);
		below = y + radius + (spawner->machine.state == AE_SPAWNER_RED ? 10.0f : 6.0f) * s + px;
		text_at(local_player_index, status, x, below, px, status_rgb, 1.0f);
		if (distance >= CHIP_DETAIL_NEAR)
			continue;
		snprintf(facts, sizeof(facts), "#%d%s %s %s %lds", (int)spawner->n, spawner->merged_count > 0 ? "+" : "",
			ae_spawner_tier_word(spawner->tier), ae_spawner_logic_word(spawner->machine.logic),
			spawner->machine.period / TICKS_PER_SECOND);
		text_at(local_player_index, facts, x, below + px * 1.15f, px * 0.82f, muted_rgb, 0.95f);
	}
}
