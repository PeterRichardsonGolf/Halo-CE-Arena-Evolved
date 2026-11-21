/*
AE_MARK_TARGETS.C

MARKS: the named places a press can take, read from the scenario (ae_mark_targets.h). Hills, teleporters and bomb sites are
places, not objects: the aim is tested on their points, as a marker's.
*/

#include "cseries.h"
#include "math/real_math.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmaps.h"
#include "cache/cache_files.h"
#include "cache/texture_cache.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "interface/hud_draw.h"
#include "interface/weapon_hud_interface_definition.h"
#include "items/weapon_definitions.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_groups.h"

#include "ae_mark_calls.h"
#include "ae_mark_targets.h"
#include "ae_waypoint_icons.h"

/* (hud_item_timers.c's: the weapons' HUD interface tag) */
#define target_weapon_hud_interface_get(index) \
	((struct weapon_hud_interface_definition *)tag_get('wphi', (index)))

/* (a game's weapon list is short: the stock one has 12) */
#define TARGET_WEAPON_LIST_MOST 32

/* a teleporter pad's, and a bomb site's (the bomb's own arming radius), and a hill of one point's */
#define TARGET_TELEPORTER_RADIUS 0.5f
#define TARGET_BOMB_SITE_RADIUS 1.0f
#define TARGET_HILL_RADIUS_LEAST 1.0f
#define TARGET_HILLS_MOST 16

/* the hills of the map: a hill's points are the flags of its id (the team index), its middle that of their box */
static short targets_hills(struct scenario const *scenario, struct ae_mark_place *places, short maximum)
{
	short ids[TARGET_HILLS_MOST];
	short count = 0, found = 0, index, other;

	for (index = 0; index < scenario->netgame_flags.count && count < TARGET_HILLS_MOST; index++)
	{
		struct scenario_netgame_flag const *flag = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index, struct scenario_netgame_flag);
		boolean seen = FALSE;

		if (flag->type != _netgame_flag_hill)
			continue;
		for (other = 0; other < count; other++)
			seen = seen || ids[other] == flag->team_index;
		if (!seen)
			ids[count++] = flag->team_index;
	}
	for (other = 0; other < count && found < maximum; other++)
	{
		real_point3d minimum, maximum_point;
		real radius = 0.0f;
		boolean first = TRUE;
		struct ae_mark_place *place = &places[found];

		for (index = 0; index < scenario->netgame_flags.count; index++)
		{
			struct scenario_netgame_flag const *flag = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index, struct scenario_netgame_flag);

			if (flag->type != _netgame_flag_hill || flag->team_index != ids[other])
				continue;
			if (first)
			{
				minimum = maximum_point = flag->position;
				first = FALSE;
				continue;
			}
			minimum.x = MIN(minimum.x, flag->position.x);
			minimum.y = MIN(minimum.y, flag->position.y);
			minimum.z = MIN(minimum.z, flag->position.z);
			maximum_point.x = MAX(maximum_point.x, flag->position.x);
			maximum_point.y = MAX(maximum_point.y, flag->position.y);
			maximum_point.z = MAX(maximum_point.z, flag->position.z);
		}
		if (first)
			continue;
		place->point.x = (minimum.x + maximum_point.x) * 0.5f;
		place->point.y = (minimum.y + maximum_point.y) * 0.5f;
		place->point.z = (minimum.z + maximum_point.z) * 0.5f;
		for (index = 0; index < scenario->netgame_flags.count; index++)
		{
			struct scenario_netgame_flag const *flag = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index, struct scenario_netgame_flag);

			if (flag->type == _netgame_flag_hill && flag->team_index == ids[other])
				radius = MAX(radius, distance3d(&flag->position, &place->point));
		}
		place->radius = MAX(radius, TARGET_HILL_RADIUS_LEAST);
		place->word = MARK_WORD_HILL;
		found++;
	}
	return found;
}

short ae_mark_targets_places(struct ae_mark_place *places, short maximum)
{
	struct scenario const *scenario = global_scenario_try_and_get();
	struct game_variant const *variant = game_engine_running() ? game_engine_get_variant() : NULL;
	short count = 0, index;

	if (!scenario || !variant)
		return 0;
	if (variant->game_engine_index == game_engine_king)
		count += targets_hills(scenario, places, maximum);
	for (index = 0; index < scenario->netgame_flags.count && count < maximum; index++)
	{
		struct scenario_netgame_flag const *flag = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index, struct scenario_netgame_flag);

		if (flag->type != _netgame_flag_teleporter_source && flag->type != _netgame_flag_teleporter_target)
			continue;
		places[count].point = flag->position;
		places[count].radius = TARGET_TELEPORTER_RADIUS;
		places[count].word = MARK_WORD_TELEPORTER;
		count++;
	}
#ifdef HALO_GAME_BROWSER
	if (variant->game_engine_index == game_engine_bomb)
	{
		for (index = 0; index < 2 && count < maximum; index++)
		{
			real_point3d goal;

			if (!game_engine_bomb_goal(index, &goal))
				continue;
			places[count].point = goal;
			places[count].radius = TARGET_BOMB_SITE_RADIUS;
			places[count].word = MARK_WORD_BOMB_SITE;
			count++;
		}
	}
#endif
	return count;
}

/* the map's assault rifle's picture, by the steps the item waypoints take for a power weapon's (hud_item_timers.c's
hud_waypoint_stock_sprite): its HUD interface's messaging icon, a sequence of the HUD globals' messaging icons. Every step is
checked: a Custom Edition map's or a mod's tags may lack any of them */
struct bitmap_data const *ae_mark_rifle_sprite(real_rectangle2d *clip)
{
	struct bitmap_data const *bitmap = NULL;
	real_rectangle2d const *bounds = NULL;
	long group_index, definition_index = NONE, hud_interface_index, list_index;
	struct bitmap_group *group;
	short sequence_index;
	int width, height;

	if (!clip || !hud_globals || !global_scenario_try_and_get())
		return NULL;
	group_index = hud_globals->messaging.messaging_icons.index;
	if (group_index == NONE)
		return NULL;
	for (list_index = 0; list_index < TARGET_WEAPON_LIST_MOST && definition_index == NONE; list_index++)
	{
		long definition = list_index_to_weapon_definition_index(list_index);

		if (definition != NONE && tag_get_group_tag(definition) == WEAPON_DEFINITION_TAG &&
			game_engine_weapon_is_assault_rifle(definition))
		{
			definition_index = definition;
		}
	}
	if (definition_index == NONE)
		return NULL;
	hud_interface_index = weapon_definition_get(definition_index)->weapon.interface_definition.hud_interface.index;
	if (hud_interface_index == NONE || tag_get_group_tag(hud_interface_index) != 'wphi')
		return NULL;
	sequence_index = target_weapon_hud_interface_get(hud_interface_index)->messaging_icon.sequence_index;
	group = bitmap_group_get(group_index);
	if (sequence_index < 0 || sequence_index >= group->sequences.count ||
		TAG_BLOCK_GET_ELEMENT(&group->sequences, sequence_index, struct bitmap_group_sequence)->sprites.count < 1)
	{
		return NULL;
	}
	hud_retrieve_bitmap_and_bounding_rect(group_index, sequence_index, 0, &bitmap, &bounds);
	if (!bitmap || !bounds ||
		!ae_waypoint_sprite_size(bitmap->width, bitmap->height, bounds->x0, bounds->x1, bounds->y0, bounds->y1, &width, &height) ||
		!_texture_cache_bitmap_get_hardware_format((struct bitmap_data *)bitmap, FALSE, TRUE))
	{
		return NULL;
	}
	*clip = *bounds;
	return bitmap;
}
