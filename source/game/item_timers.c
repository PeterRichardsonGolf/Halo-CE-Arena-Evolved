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
#include "hs/hs.h"
#include "items/equipment_definitions.h"
#include "items/item_definitions.h"
#include "items/items.h"
#include "items/weapon_definitions.h"
#include "objects/object_definitions.h"
#include "objects/object_types.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"

/* ---------- constants */

#define MAXIMUM_ITEM_TIMERS 64
#define ITEM_TIMER_LABEL_LENGTH 15
/* an entry nearer one team's base than the other's by less than this part
of the distance between the bases is in the middle (of the entry's own
distance, an item far along a map's length from both, as Boarding Action's
snipers at a ship's end, would be) */
#define ITEM_TIMER_MIDDLE_FRACTION 0.15f
/* how far from its spawn point an entry's spawned item is looked for */
#define ITEM_TIMER_SPAWNED_RADIUS 2.0f

/* ---------- macros */

#define item_collection_definition_get(index) \
	((struct item_collection_definition *)tag_get(ITEM_COLLECTION_DEFINITION_TAG, (index)))

/* ---------- prototypes */

void platform_log(char const *format, ...);

/* ---------- globals */

static struct item_timer item_timers[MAXIMUM_ITEM_TIMERS];
static short item_timer_count;
/* each entry's TRAINING waypoint at the last tick (item_timers_update's log) */
static boolean item_timer_waypoints[MAXIMUM_ITEM_TIMERS];
/* the item a mixed entry (OS/CAMO) last spawned, as seen on the map
(item_timers_update): the spawn's tick, its class (NONE: not seen) */
static struct
{
	long spawn_tick;
	short timer_class;
} item_timer_spawned[MAXIMUM_ITEM_TIMERS];

static char const *const item_timer_side_names[NUMBER_OF_ITEM_TIMER_SIDES] =
{
	"MIDDLE",
	"RED",
	"BLUE",
};

/* ---------- private code */

/* a power item's class (rockets, sniper, overshield, camo), else other */
static short item_timer_definition_class(
	long definition_index)
{
	switch (tag_get_group_tag(definition_index))
	{
	case WEAPON_DEFINITION_TAG:
		if (game_engine_weapon_is_rocket_launcher(definition_index))
			return _item_timer_rockets;
		if (game_engine_weapon_is_sniper_rifle(definition_index))
			return _item_timer_sniper;
		break;

	case EQUIPMENT_DEFINITION_TAG:
		switch (equipment_definition_get(definition_index)->equipment.powerup_type)
		{
		case _equipment_powerup_overshield:
			return _item_timer_overshield;

		case _equipment_powerup_active_camouflage:
			return _item_timer_camo;
		}
		break;
	}

	return _item_timer_other;
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
		short permutation_class;

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

		permutation_class = item_timer_definition_class(definition_index);
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

/* a team's base (0 red, 1 blue): the middle of its CTF flags, else of
its player starting locations (whatever their game types: a free for all
map's still have teams); FALSE for none */
static boolean item_timers_team_base(
	struct scenario *scenario,
	short team_index,
	real_point3d *base)
{
	long count = 0;
	short index;

	base->x = base->y = base->z = 0.0f;
	for (index = 0; index < scenario->netgame_flags.count; index++)
	{
		struct scenario_netgame_flag const *flag = TAG_BLOCK_GET_ELEMENT(
			&scenario->netgame_flags,
			index,
			struct scenario_netgame_flag);

		if (flag->type == _netgame_flag_ctf_flag && flag->team_index == team_index)
		{
			base->x += flag->position.x;
			base->y += flag->position.y;
			base->z += flag->position.z;
			count++;
		}
	}
	if (!count)
	{
		for (index = 0; index < scenario->players.count; index++)
		{
			struct player_starting_location const *location = TAG_BLOCK_GET_ELEMENT(
				&scenario->players,
				index,
				struct player_starting_location);

			if (location->team_index == team_index)
			{
				base->x += location->position.x;
				base->y += location->position.y;
				base->z += location->position.z;
				count++;
			}
		}
	}
	if (!count)
		return FALSE;

	base->x /= (real)count;
	base->y /= (real)count;
	base->z /= (real)count;
	return TRUE;
}

/* each power entry's side: the nearer team's base, else the middle (as
near to both, within ITEM_TIMER_MIDDLE_FRACTION of the bases' distance
apart, or a map without both teams' bases); and RED / BLUE before the name of an entry whose item (its
class and label) is also at the other team's base */
static void item_timers_find_sides(
	char labels[][ITEM_TIMER_LABEL_LENGTH + 1])
{
	struct scenario *scenario = global_scenario_get();
	real_point3d bases[2];
	boolean found = item_timers_team_base(scenario, 0, &bases[0]) && item_timers_team_base(scenario, 1, &bases[1]);
	real apart = found ? distance3d(&bases[0], &bases[1]) : 0.0f;
	short index, other;

	if (found)
	{
		platform_log("item timers: red base (%.1f %.1f %.1f), blue base (%.1f %.1f %.1f)",
			bases[0].x, bases[0].y, bases[0].z, bases[1].x, bases[1].y, bases[1].z);
	}
	else
	{
		platform_log("item timers: no red and blue bases; every item is in the middle");
	}
	for (index = 0; index < item_timer_count; index++)
	{
		struct item_timer *timer = &item_timers[index];

		timer->side = _item_timer_side_middle;
		timer->side_prefix = FALSE;
		if (found && timer->timer_class < NUMBER_OF_ITEM_TIMER_POWER_CLASSES)
		{
			real red = distance3d(&timer->position, &bases[0]);
			real blue = distance3d(&timer->position, &bases[1]);

			if (fabs(red - blue) > ITEM_TIMER_MIDDLE_FRACTION * apart)
				timer->side = red < blue ? _item_timer_side_red : _item_timer_side_blue;
		}
	}
	for (index = 0; index < item_timer_count; index++)
	{
		struct item_timer *timer = &item_timers[index];

		for (other = 0; other < item_timer_count; other++)
		{
			struct item_timer *opposite = &item_timers[other];

			if (timer->side != _item_timer_side_middle && opposite->side != _item_timer_side_middle &&
				timer->side != opposite->side && timer->timer_class == opposite->timer_class &&
				!csstrcmp(labels[index], labels[other]))
			{
				timer->side_prefix = TRUE;
				break;
			}
		}
	}
}

/* the item on the map a mixed entry (OS/CAMO) spawned: the nearest of its
classes by its spawn point, not held, the latest placed of any there; its
class, else NONE */
static short item_timer_find_spawned(
	struct item_timer const *timer)
{
	struct object_iterator iterator;
	short found_class = NONE;
	long found_time = 0;

	object_iterator_new(&iterator, _object_mask_weapon | _object_mask_equipment, 0);
	while (object_iterator_next(&iterator))
	{
		struct object_datum *object = object_get(iterator.index);
		short object_class;
		long placed;

		if (object->object.parent_object_index != NONE ||
			!TEST_FLAG(object->object.flags, _object_connected_to_map_bit) ||
			distance3d(&timer->position, &object->object.position) > ITEM_TIMER_SPAWNED_RADIUS)
		{
			continue;
		}
		object_class = item_timer_definition_class(object->definition_index);
		if (object_class >= NUMBER_OF_ITEM_TIMER_POWER_CLASSES || !TEST_FLAG(timer->classes, object_class))
			continue;
		placed = item_get(iterator.index)->item.last_owned_time;
		if (found_class == NONE || placed > found_time)
		{
			found_class = object_class;
			found_time = placed;
		}
	}

	return found_class;
}

/* an entry that can spawn more than one power class (OS/CAMO) */
static boolean item_timer_mixed(
	struct item_timer const *timer)
{
	short power_classes = 0;
	short timer_class;

	for (timer_class = 0; timer_class < NUMBER_OF_ITEM_TIMER_POWER_CLASSES; timer_class++)
	{
		if (TEST_FLAG(timer->classes, timer_class))
			power_classes++;
	}

	return power_classes > 1;
}

/* ---------- public code */

void item_timers_map_begin(
	void)
{
	struct scenario *scenario;
	short equipment_index;
	char labels[MAXIMUM_ITEM_TIMERS][ITEM_TIMER_LABEL_LENGTH + 1];
	short index;

	item_timer_count = 0;
	csmemset(item_timer_waypoints, 0, sizeof(item_timer_waypoints));
	for (index = 0; index < MAXIMUM_ITEM_TIMERS; index++)
	{
		item_timer_spawned[index].spawn_tick = NONE;
		item_timer_spawned[index].timer_class = NONE;
	}
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
		item_timer_label(timer_class, classes, definition_index, labels[item_timer_count]);
		timer->position = equipment->position;
		timer->period_ticks = game_engine_item_respawn_period(equipment);
		timer->timer_class = timer_class;
		timer->classes = classes;
		item_timer_count++;
	}

	/* (the power list's labels: RED / BLUE before an item at both bases) */
	item_timers_find_sides(labels);
	for (index = 0; index < item_timer_count; index++)
	{
		struct item_timer *timer = &item_timers[index];
		char label[ITEM_TIMER_LABEL_LENGTH + 1];
		short character_index;

		if (timer->side_prefix)
			snprintf(label, sizeof(label), "%s %s", item_timer_side_names[timer->side], labels[index]);
		else
			snprintf(label, sizeof(label), "%s", labels[index]);
		for (character_index = 0; character_index <= ITEM_TIMER_LABEL_LENGTH; character_index++)
		{
			timer->label[character_index] = (wchar_t)(unsigned char)label[character_index];
			if (!label[character_index])
				break;
		}

		platform_log("item timers: %d %s every %lds at (%.1f %.1f %.1f), %s", index, label,
			timer->period_ticks / TICKS_PER_SECOND, timer->position.x, timer->position.y, timer->position.z,
			timer->timer_class < NUMBER_OF_ITEM_TIMER_POWER_CLASSES ? item_timer_side_names[timer->side] : "not a power item");
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

/* whether TRAINING's markers (the waypoints, render_spawn_markers.c's spawn
markers) may show now: a TRAINING game on a map not Halo 1: NHE's (their
scripts draw their own), past the PRE-GAME COUNTDOWN and not over */
boolean item_timers_training_shown(
	void)
{
	return game_engine_training() && !hs_scenario_is_nhe() && !game_engine_game_over() &&
		game_engine_pregame_countdown_ticks_left() <= 0;
}

/* TRAINING's waypoint over a power entry (hud_item_timers.c), as Halo 1:
NHE's Training mode's (activate_*_waypoint with its call, about 10 s before
the item's spawn; deactivate_powerup_waypoints at :20 of the spawn's
minute), from the entry's own spawns rather than NHE's minutes: from
ITEM_TIMER_WAYPOINT_BEFORE_TICKS before each spawn after the game's start
(not the items placed as it starts, as NHE) until
ITEM_TIMER_WAYPOINT_AFTER_TICKS after it. None on Halo 1: NHE's maps (their
scripts draw their own), while the PRE-GAME COUNTDOWN counts or once the
game is over. Game time only, so every machine shows the same */
boolean item_timer_waypoint_shown(
	struct item_timer const *timer)
{
	long now;

	if (!timer || timer->timer_class < 0 || timer->timer_class >= NUMBER_OF_ITEM_TIMER_POWER_CLASSES ||
		timer->period_ticks <= 0)
	{
		return FALSE;
	}
	if (!item_timers_training_shown())
		return FALSE;

	now = game_time_get();
	if (item_timer_ticks_left(timer) <= ITEM_TIMER_WAYPOINT_BEFORE_TICKS)
		return TRUE;

	return now >= timer->period_ticks && now % timer->period_ticks < ITEM_TIMER_WAYPOINT_AFTER_TICKS;
}

/* whether the entry's last spawn after the game's start was under
ITEM_TIMER_WAYPOINT_AFTER_TICKS ago: its item is on the map, as far as
its waypoint goes (the game spawns one each period, taken or not) */
boolean item_timer_on_map(
	struct item_timer const *timer)
{
	long now = game_time_get();

	if (!timer || timer->period_ticks <= 0)
		return FALSE;

	return now >= timer->period_ticks && now % timer->period_ticks < ITEM_TIMER_WAYPOINT_AFTER_TICKS;
}

/* the class of the item the entry spawned last, while item_timer_on_map:
a mixed entry's (OS/CAMO) as seen on the map, else NONE (not seen yet);
any other's own class */
short item_timer_spawned_class(
	struct item_timer const *timer)
{
	short index = (short)(timer - item_timers);

	if (!item_timer_on_map(timer))
		return NONE;
	if (!item_timer_mixed(timer))
		return timer->timer_class;
	if (index < 0 || index >= item_timer_count ||
		item_timer_spawned[index].spawn_tick != game_time_get() - game_time_get() % timer->period_ticks)
	{
		return NONE;
	}

	return item_timer_spawned[index].timer_class;
}

/* the name over TRAINING's waypoint: RED / BLUE as the power list has it,
and the item's whole name (ROCKETS, SNIPER, OVERSHIELD, CAMO); a mixed
entry's OS/CAMO until the item it spawned is seen on the map */
void item_timer_waypoint_name(
	struct item_timer const *timer,
	wchar_t *name,
	short size)
{
	static char const *const power_names[NUMBER_OF_ITEM_TIMER_POWER_CLASSES] =
	{
		"ROCKETS",
		"SNIPER",
		"OVERSHIELD",
		"CAMO",
	};
	char text[32];
	char const *item = "OS/CAMO";
	short timer_class = item_timer_mixed(timer) ? item_timer_spawned_class(timer) : timer->timer_class;
	short index;

	if (size <= 0)
		return;
	if (timer_class >= 0 && timer_class < NUMBER_OF_ITEM_TIMER_POWER_CLASSES)
		item = power_names[timer_class];
	else if (!item_timer_mixed(timer))
	{
		/* (not a power item: the power list's label) */
		for (index = 0; index < size - 1 && timer->label[index]; index++)
			name[index] = timer->label[index];
		name[index] = 0;
		return;
	}
	if (timer->side_prefix)
		snprintf(text, sizeof(text), "%s %s", item_timer_side_names[timer->side], item);
	else
		snprintf(text, sizeof(text), "%s", item);
	for (index = 0; index < size - 1 && text[index]; index++)
		name[index] = (wchar_t)(unsigned char)text[index];
	name[index] = 0;
}

void item_timers_update(
	void)
{
	short count = item_timers_count();
	short index;

	/* (the item a mixed entry spawned, once it is on the map: a client
	has it when the host's update reaches it) */
	for (index = 0; index < count; index++)
	{
		struct item_timer const *timer = &item_timers[index];
		long spawn;

		if (!item_timer_mixed(timer) || !item_timer_on_map(timer))
			continue;
		spawn = game_time_get() - game_time_get() % timer->period_ticks;
		if (item_timer_spawned[index].spawn_tick != spawn)
		{
			item_timer_spawned[index].spawn_tick = spawn;
			item_timer_spawned[index].timer_class = NONE;
		}
		if (item_timer_spawned[index].timer_class == NONE)
		{
			item_timer_spawned[index].timer_class = item_timer_find_spawned(timer);
			if (item_timer_spawned[index].timer_class != NONE)
			{
				wchar_t name[32];
				char text[32];
				short character_index;

				item_timer_waypoint_name(timer, name, NUMBEROF(name));
				for (character_index = 0; character_index < (short)sizeof(text) - 1 && name[character_index]; character_index++)
					text[character_index] = (char)name[character_index];
				text[character_index] = 0;
				platform_log("item timers: %d spawned %s at tick %ld (seen at tick %ld)", index, text, spawn,
					game_time_get());
			}
		}
	}

	/* (TRAINING's waypoints going on and off, with the spawn they are
	for: the next one's going on, the last one's going off) */
	for (index = 0; index < count; index++)
	{
		struct item_timer const *timer = &item_timers[index];
		boolean shown = item_timer_waypoint_shown(timer);

		if (shown != item_timer_waypoints[index])
		{
			long now = game_time_get();
			long ticks_left = item_timer_ticks_left(timer);
			long spawn = ticks_left <= ITEM_TIMER_WAYPOINT_BEFORE_TICKS ? now + ticks_left : now - now % timer->period_ticks;
			char label[ITEM_TIMER_LABEL_LENGTH + 1];
			short character_index;

			for (character_index = 0; character_index < ITEM_TIMER_LABEL_LENGTH && timer->label[character_index]; character_index++)
				label[character_index] = (char)timer->label[character_index];
			label[character_index] = 0;
			platform_log("item timers: waypoint %d %s %s at tick %ld (spawn at tick %ld)", index, label,
				shown ? "on" : "off", now, spawn);
			item_timer_waypoints[index] = shown;
		}
	}
}
