/*
ITEM_TIMERS.C

port: the netgame equipment's spawns for the gametype's TIMERS and TRAINING
(game_engine.h): each entry's period as game_engine_update_item_spawn has
it, which spawns an entry every period from the game's start whether its
item was taken or not, so every machine knows the next spawn from the game
time alone (a client has the host's: game_time_set_distributed).
*/

/* ---------- headers */

#include "cseries/cseries.h"

#include "cache/cache_files.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/item_timers.h"
#include "items/equipment_definitions.h"
#include "items/item_definitions.h"
#include "items/weapon_definitions.h"
#include "objects/object_definitions.h"
#include "objects/object_types.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"

/* ---------- constants */

#define MAXIMUM_ITEM_TIMERS 64
#define ITEM_TIMER_LABEL_LENGTH 15

/* ---------- macros */

#define item_collection_definition_get(index) \
	((struct item_collection_definition *)tag_get(ITEM_COLLECTION_DEFINITION_TAG, (index)))

/* ---------- prototypes */

void platform_log(char const *format, ...);

/* ---------- globals */

static struct item_timer item_timers[MAXIMUM_ITEM_TIMERS];
static short item_timer_count;

/* ---------- private code */

/* (as game_engine_update_item_spawn: the entry's spawn time, else its item
collection's, else 30 seconds) */
static long item_timer_period(
	struct scenario_netgame_equipment const *equipment)
{
	long period = 30 * TICKS_PER_SECOND;

	if (equipment->spawn_time != 0)
		period = equipment->spawn_time * TICKS_PER_SECOND;
	else if (equipment->item_collection.index != NONE)
	{
		struct item_collection_definition *collection =
			item_collection_definition_get(equipment->item_collection.index);

		if (collection->spawn_time != 0)
			period = collection->spawn_time * TICKS_PER_SECOND;
	}

	return period;
}

/* the class of an item collection: the most powerful item it can spawn
(rockets, sniper, overshield, camo, else other), as the gametype remaps its
items; the heaviest permutation names the entry, and *classes has a bit
for each class it can spawn. FALSE when the gametype spawns none of them
(no shields: no overshield, always invisible: no camo, no weapons on the
map: no weapons) */
static boolean item_timer_classify(
	long collection_index,
	short *timer_class,
	long *heaviest_definition,
	unsigned short *classes)
{
	struct item_collection_definition *collection;
	struct item_permutation_definition const *permutations;
	long permutation_index;
	real heaviest_weight = -1.0f;

	*timer_class = _item_timer_other;
	*heaviest_definition = NONE;
	*classes = 0;
	if (collection_index == NONE)
		return FALSE;

	collection = item_collection_definition_get(collection_index);
	permutations = (struct item_permutation_definition const *)xbox_pointer(collection->permutations.address);
	for (permutation_index = 0;
		permutation_index < collection->permutations.count;
		permutation_index++)
	{
		struct item_permutation_definition const *permutation = &permutations[permutation_index];
		long definition_index = permutation->item.index;
		short permutation_class = _item_timer_other;

		/* (random_item never picks an item of no weight) */
		if (definition_index == NONE || permutation->weight <= 0.0f)
			continue;

		/* (as game_engine_update_item_spawn: the gametype's no weapons on the
		map, tested on the collection's own item) */
		if (game_variant_options_get()->no_map_weapons &&
			object_definition_get(definition_index)->object.type == _object_type_weapon)
		{
			continue;
		}

		definition_index = game_engine_remap_item_definition(definition_index);
		if (definition_index == NONE)
			continue;

		switch (tag_get_group_tag(definition_index))
		{
		case WEAPON_DEFINITION_TAG:
			if (game_engine_weapon_is_rocket_launcher(definition_index))
				permutation_class = _item_timer_rockets;
			else if (game_engine_weapon_is_sniper_rifle(definition_index))
				permutation_class = _item_timer_sniper;
			break;

		case EQUIPMENT_DEFINITION_TAG:
			switch (equipment_definition_get(definition_index)->equipment.powerup_type)
			{
			case _equipment_powerup_overshield:
				permutation_class = _item_timer_overshield;
				break;

			case _equipment_powerup_active_camouflage:
				permutation_class = _item_timer_camo;
				break;
			}
			break;
		}

		SET_FLAG(*classes, permutation_class, TRUE);
		if (permutation_class < *timer_class)
			*timer_class = permutation_class;
		if (permutation->weight > heaviest_weight)
		{
			heaviest_weight = permutation->weight;
			*heaviest_definition = definition_index;
		}
	}

	return *heaviest_definition != NONE;
}

/* a power class's fixed label (OS/CAMO for a collection that spawns either
powerup, as Blood Gulch's shield-invisibility), else the item's tag name's
last part, upper-cased, underscores as spaces ("weapons\\frag grenade\\frag
grenade": FRAG GRENADE) */
static void item_timer_label(
	short timer_class,
	unsigned short classes,
	long definition_index,
	char *label)
{
	static char const *const power_labels[NUMBER_OF_ITEM_TIMER_POWER_CLASSES] =
	{
		"ROCKETS",
		"SNIPER",
		"OS",
		"CAMO",
	};
	char const *name = power_labels[0];
	short length = 0;

	if (timer_class == _item_timer_overshield && TEST_FLAG(classes, _item_timer_camo))
		name = "OS/CAMO";
	else if (timer_class >= 0 && timer_class < NUMBER_OF_ITEM_TIMER_POWER_CLASSES)
		name = power_labels[timer_class];
	else
	{
		char const *part;

		name = tag_get_name(definition_index);
		if (!name)
			name = "ITEM";
		for (part = name; *part; part++)
		{
			if (*part == '\\' || *part == '/')
				name = part + 1;
		}
	}

	while (name[length] && length < ITEM_TIMER_LABEL_LENGTH)
	{
		char character = name[length];

		if (character == '_')
			character = ' ';
		else if (character >= 'a' && character <= 'z')
			character = (char)(character - 'a' + 'A');
		label[length] = character;
		length++;
	}
	label[length] = 0;
}

/* ---------- public code */

void item_timers_map_begin(
	void)
{
	struct scenario *scenario;
	short equipment_index;

	item_timer_count = 0;
	if (!game_engine_running())
		return;

	scenario = global_scenario_get();
	for (equipment_index = 0;
		equipment_index < scenario->netgame_equipment.count;
		equipment_index++)
	{
		struct scenario_netgame_equipment const *equipment = TAG_BLOCK_GET_ELEMENT(
			&scenario->netgame_equipment,
			equipment_index,
			struct scenario_netgame_equipment);
		struct item_timer *timer;
		short timer_class;
		unsigned short classes;
		long definition_index;
		char label[ITEM_TIMER_LABEL_LENGTH + 1];
		short character_index;

		if (!game_engine_matches_game_type(equipment->game_type) ||
			!item_timer_classify(equipment->item_collection.index, &timer_class, &definition_index, &classes))
		{
			continue;
		}
		if (item_timer_count >= MAXIMUM_ITEM_TIMERS)
		{
			platform_log("item timers: more than %d entries; the rest are not timed", MAXIMUM_ITEM_TIMERS);
			break;
		}

		timer = &item_timers[item_timer_count];
		item_timer_label(timer_class, classes, definition_index, label);
		timer->position = equipment->position;
		timer->period_ticks = item_timer_period(equipment);
		timer->timer_class = timer_class;
		for (character_index = 0; character_index <= ITEM_TIMER_LABEL_LENGTH; character_index++)
		{
			timer->label[character_index] = (wchar_t)(unsigned char)label[character_index];
			if (!label[character_index])
				break;
		}

		platform_log("item timers: %d %s every %lds at (%.1f %.1f %.1f)", item_timer_count, label,
			timer->period_ticks / TICKS_PER_SECOND, timer->position.x, timer->position.y, timer->position.z);
		item_timer_count++;
	}
}

/* (none once the game is over: the table is of the last map's game until
the next one's item_timers_map_begin) */
short item_timers_count(
	void)
{
	if (!game_engine_running())
		return 0;

	return item_timer_count;
}

struct item_timer const *item_timers_get(
	short index)
{
	if (index < 0 || index >= item_timers_count())
		return NULL;

	return &item_timers[index];
}

/* the ticks until the entry next spawns, 1..period (period: it spawned this
tick, as game_engine_update_item_spawn spawns when the game time is a
multiple of the period) */
long item_timer_ticks_left(
	struct item_timer const *timer)
{
	return timer->period_ticks - game_time_get() % timer->period_ticks;
}

void item_timers_update(
	void)
{
	/* (the timers' voice goes here) */
}
