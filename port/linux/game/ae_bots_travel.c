/*
AE_BOTS_TRAVEL.C

The engine's side of the bots' getting around (ae_bots_travel.h): the map's teleporter pads and the layout file's bot
lines read, the health packs lying about, the gametype's HEALTH, the settings, and debug.bot_hints's logging of player
1's jumps and crouches as layout lines. The bots' own use of them is ae_bots_travel.inc's (in bots.c), the choices
ae_bots_travel_rules.c's.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "items/equipment_definitions.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "units/units.h"
#include "units/bipeds.h"
#include "tag_files/tag_groups.h"

#include "ae_layout.h"
#include "ae_layout_rules.h"
#include "ae_bots.h"
#include "ae_bots_travel_rules.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* port/linux/src/port_config.c */
int config_boolean(char const *name);
unsigned long config_changes(void);
void platform_log(char const *format, ...);

/* ---------- the map */

long ae_bots_travel_read_links(struct ae_bots_travel_link *links, long maximum)
{
	struct scenario *scenario = global_scenario_get();
	struct ae_layout const *hints = ae_layout_bot_hints();
	long count = 0, index;

	if (!scenario)
		return 0;
	/* (the engine's game_engine_update_teleporter: a player within half a unit of a sender goes to the first receiver
	of the sender's channel, its team index) */
	for (index = 0; index < scenario->netgame_flags.count && count < maximum; index++)
	{
		struct scenario_netgame_flag *source =
			TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, index, struct scenario_netgame_flag);
		long target;

		if (source->type != _netgame_flag_teleporter_source)
			continue;
		target = find_netgame_flag(NULL, 0.0f, 0.0f, _netgame_flag_teleporter_target, source->team_index);
		if (target == NONE)
		{
			platform_log("bots: travel: teleporter sender %ld (channel %d) at (%.1f %.1f %.1f) sends nowhere", index,
				(int)source->team_index, source->position.x, source->position.y, source->position.z);
			continue;
		}
		links[count].kind = AE_BOTS_TRAVEL_LINK_TELEPORTER;
		links[count].source = index;
		links[count].target = target;
		links[count].entry = source->position;
		links[count].exit = TAG_BLOCK_GET_ELEMENT(&scenario->netgame_flags, target, struct scenario_netgame_flag)->position;
		count++;
	}
	for (index = 0; hints && index < hints->bot_jump_count && count < maximum; index++)
	{
		struct ae_layout_bot_jump const *jump = &hints->bot_jumps[index];

		links[count].kind = AE_BOTS_TRAVEL_LINK_JUMP;
		links[count].source = jump->line;
		links[count].target = jump->line;
		links[count].entry.x = jump->from_x;
		links[count].entry.y = jump->from_y;
		links[count].entry.z = jump->from_z;
		links[count].exit.x = jump->to_x;
		links[count].exit.y = jump->to_y;
		links[count].exit.z = jump->to_z;
		count++;
	}
	return count;
}

long ae_bots_travel_read_avoids(struct ae_bots_travel_avoid *avoids, long maximum)
{
	struct ae_layout const *hints = ae_layout_bot_hints();
	long count = 0, index;

	for (index = 0; hints && index < hints->bot_avoid_count && count < maximum; index++)
	{
		avoids[count].point.x = hints->bot_avoids[index].x;
		avoids[count].point.y = hints->bot_avoids[index].y;
		avoids[count].point.z = hints->bot_avoids[index].z;
		avoids[count].radius = hints->bot_avoids[index].radius;
		avoids[count].line = hints->bot_avoids[index].line;
		count++;
	}
	return count;
}

/* ---------- health */

int ae_bots_travel_health_style(void)
{
	switch (game_engine_health_style())
	{
	case _health_style_classic:
		return AE_BOTS_TRAVEL_HEALTH_CLASSIC;
	case _health_style_reach:
		return AE_BOTS_TRAVEL_HEALTH_REACH;
	default:
		return AE_BOTS_TRAVEL_HEALTH_FULL;
	}
}

static boolean pack_lying(struct object_datum *object)
{
	struct equipment_definition *definition;

	if (object->object.parent_object_index != NONE || object->definition_index == NONE)
		return FALSE;
	definition = equipment_definition_get(object->definition_index);
	return (boolean)(definition && definition->equipment.powerup_type == _equipment_powerup_health);
}

long ae_bots_travel_read_packs(long *objects, real_point3d *positions, long maximum)
{
	struct object_iterator iterator;
	long count = 0;

	object_iterator_new(&iterator, _object_mask_equipment, 0);
	while (object_iterator_next(&iterator) && count < maximum)
	{
		struct object_datum *object = object_get(iterator.index);

		if (!pack_lying(object))
			continue;
		objects[count] = iterator.index;
		positions[count] = object->object.position;
		count++;
	}
	return count;
}

boolean ae_bots_travel_pack_there(long object_index)
{
	struct object_datum *object = object_index == NONE ? NULL :
		(struct object_datum *)object_try_and_get_and_verify_type(object_index, _object_mask_equipment);

	return (boolean)(object && pack_lying(object));
}

/* ---------- settings */

boolean ae_bots_travel_links_on(void)
{
	static unsigned long changes = 0xFFFFFFFFUL;
	static boolean on = TRUE;

	if (config_changes() != changes)
	{
		changes = config_changes();
		on = (boolean)(config_boolean("debug.bot_links") != 0);
	}
	return on;
}

/* ---------- debug.bot_hints: layout lines from player 1's play */

static struct
{
	boolean ready;
	long last_tick;
	boolean airborne;
	real_point3d ground;
	real_point3d takeoff;
	long takeoff_tick;
	long jump_tick;
	boolean jumped;
	long crouch_tick;
	boolean crouch_logged;
} author;

void ae_bots_travel_author_tick(void)
{
	static unsigned long changes = 0xFFFFFFFFUL;
	static boolean on = FALSE;
	long now, player_index;
	struct player_datum *player;
	struct biped_datum *biped;
	real_point3d position;
	boolean airborne;

	if (config_changes() != changes)
	{
		changes = config_changes();
		on = (boolean)(config_boolean("debug.bot_hints") != 0);
	}
	if (!on || !game_engine_running())
		return;
	now = game_time_get();
	/* (a new game: the header again, nothing carried over) */
	if (!author.ready || now < author.last_tick)
	{
		char header[96];

		csmemset(&author, 0, sizeof(author));
		author.ready = TRUE;
		author.jump_tick = -1000;
		if (ae_layout_map_header(header, (long)sizeof(header)))
			platform_log("bots: hint: this map's layout file (maps\\layouts\\<map>.txt) begins: %s", header);
		else
			platform_log("bots: hint: this map has no file a layout can name");
	}
	author.last_tick = now;
	player_index = local_player_get_player_index(0);
	player = player_index != NONE ? player_get(player_index) : NULL;
	biped = player && player->unit_index != NONE ? biped_try_and_get(player->unit_index) : NULL;
	if (!biped || biped->object.parent_object_index != NONE)
	{
		author.airborne = FALSE;
		author.crouch_tick = 0;
		return;
	}
	position = biped->object.position;
	airborne = (boolean)TEST_FLAG(biped->biped.flags, _biped_airborne_bit);
	if (TEST_FLAG(biped->unit.control_flags, _unit_control_jump_bit))
		author.jump_tick = now;
	if (airborne && !author.airborne)
	{
		author.takeoff = author.ground;
		author.takeoff_tick = now;
		author.jumped = (boolean)(now - author.jump_tick <= 3);
	}
	else if (!airborne && author.airborne)
	{
		real dx = position.x - author.takeoff.x, dy = position.y - author.takeoff.y, dz = position.z - author.takeoff.z;
		real distance = (real)sqrt(dx * dx + dy * dy + dz * dz);

		if (now - author.takeoff_tick >= TICKS_PER_SECOND / 3 && distance >= AE_LAYOUT_BOT_JUMP_SHORTEST &&
			distance <= AE_LAYOUT_BOT_JUMP_LONGEST)
		{
			platform_log("bots: hint: bot jump at %.2f %.2f %.2f to %.2f %.2f %.2f   # %s, %.1f s in the air, %.1f up",
				author.takeoff.x, author.takeoff.y, author.takeoff.z, position.x, position.y, position.z,
				author.jumped ? "jumped" : "fell", (double)(now - author.takeoff_tick) / TICKS_PER_SECOND, dz);
		}
	}
	if (!airborne)
		author.ground = position;
	author.airborne = airborne;
	/* (crouched and still a second: a place to keep the bots out of) */
	if (!airborne && TEST_FLAG(biped->unit.control_flags, _unit_control_crouch_modifier_bit) &&
		fabs(biped->object.translational_velocity.i) + fabs(biped->object.translational_velocity.j) < 0.01f)
	{
		if (!author.crouch_tick)
			author.crouch_tick = now;
		if (!author.crouch_logged && now - author.crouch_tick >= TICKS_PER_SECOND)
		{
			platform_log("bots: hint: bot avoid %.2f %.2f %.2f radius 2   # player 1 crouched here", position.x,
				position.y, position.z);
			author.crouch_logged = TRUE;
		}
	}
	else
	{
		author.crouch_tick = 0;
		author.crouch_logged = FALSE;
	}
}
