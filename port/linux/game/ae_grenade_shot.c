/*
AE_GRENADE_SHOT.C

SHOOTABLE GRENADES (ae_grenade_shot_rules.h; the gametype's byte is ae_variant_ext's shootable_grenades). A bullet whose
travel this tick passes a grenade sets it off: a live thrown grenade, in flight or lying (never one stuck to someone), or a
grenade pickup (dropped at a death or placed by the map, known by its equipment's grenade powerup and grenade type, as a map's
own grenade pickup is not the globals' item). The hit size is the object's bounds, clamped 0.15 to 0.35; a wall between the
bullet's start and the grenade blocks it. A pickup goes off as a thrown grenade of its kind would: its projectile is made where
it lies and set off at once. The shooter owns the blast: his kill.

THE NETWORK: every machine has its own copy of a grenade. The host decides: a bullet it simulates sets a copy off at once; a
client's own players' bullets are reported to the host (message 117, reliably: the point, the kind, whether a pickup), which
checks the shooter's machine, the rate, a grenade of that kind near the point and a clear line from the shooter's head,
sets its copy off and tells every client (message 118: the point, the kind, the shooter), and each client sets off its nearest
copy of that kind. A machine without the messages drops them, and its copy goes off at its fuse. No network version changes.

DEV MODE: one line for each blast ("grenade shot: ...").
*/

#include <math.h>
#include <string.h>

#include "cseries.h"
#include "real_math.h"
#include "tag_files/tag_groups.h"
#include "tag_files/tag_files.h"
#include "game/game.h"
#include "game/game_globals.h"
#include "items/equipment_definitions.h"
#include "items/projectile_definitions.h"
#include "items/projectiles.h"
#include "objects/objects.h"
#include "game/players.h"
#include "physics/collisions.h"
#include "scenario/scenario.h"
#include "units/units.h"

#include "network_distributed.h"
#include "ae_variant_ext.h"
#include "ae_grenade_shot_rules.h"
#include "ae_grenade_shot.h"

void platform_log(const char *format, ...);
/* projectiles.c (AE hook): the projectile set to detonate on its next update */
void ae_projectile_detonate(long projectile_index);

enum
{
	AE_GRENADE_SHOT_VERSION = 1,
	AE_GRENADE_SHOT_PLAYERS = 32,
	AE_GRENADE_SHOT_BLASTS = 8,
	/* a client does not report one grenade again within this many ticks */
	AE_GRENADE_SHOT_REPORTED_TICKS = 10,
};

struct ae_grenade_shot_message
{
	struct distributed_message_header header;
	unsigned short version;
	unsigned char kind;
	unsigned char flags;
	unsigned char player;
	unsigned char reserved[3];
	real_point3d point;
};

/* (the host's) the last few bullets it saw of each player: where each travelled this tick, and when; a client's report is
taken only when it matches one (a client cannot set off a grenade with no shot of its own) */
enum { AE_GRENADE_SHOT_SEEN = 8 };
static struct { struct ae_grenade_shot_point from, to; long time; } seen[AE_GRENADE_SHOT_PLAYERS][AE_GRENADE_SHOT_SEEN];
static short seen_next[AE_GRENADE_SHOT_PLAYERS];
static long last_report_time[AE_GRENADE_SHOT_PLAYERS];
static struct { real_point3d point; long time; } blasts[AE_GRENADE_SHOT_BLASTS];
static long blast_next;
static long reported_object = NONE;
static long reported_time;

void ae_grenade_shot_reset(void)
{
	short index;

	for (index = 0; index < AE_GRENADE_SHOT_PLAYERS; index++)
	{
		short slot;

		last_report_time[index] = -1000;
		seen_next[index] = 0;
		for (slot = 0; slot < AE_GRENADE_SHOT_SEEN; slot++)
			seen[index][slot].time = -1000;
	}
	for (index = 0; index < AE_GRENADE_SHOT_BLASTS; index++)
		blasts[index].time = -1000;
	blast_next = 0;
	reported_object = NONE;
}

static boolean rule_on(void)
{
	return ae_variant_ext_game()->shootable_grenades == AE_EXT_SHOOTABLE_GRENADES_ON;
}

static struct ae_grenade_shot_point shot_point(real_point3d const *point)
{
	struct ae_grenade_shot_point result;

	result.x = point->x;
	result.y = point->y;
	result.z = point->z;
	return result;
}

/* the globals' grenade whose projectile this definition is: its index (0 the frag), else NONE */
static short grenade_of(long definition_index)
{
	struct game_globals *globals = scenario_get_game_globals();
	short index;

	if (!globals || definition_index == NONE)
		return NONE;
	for (index = 0; index < globals->grenades.count; index++)
	{
		struct game_globals_grenade *grenade = TAG_BLOCK_GET_ELEMENT(&globals->grenades, index, struct game_globals_grenade);

		if (grenade->projectile.index == definition_index)
			return index;
	}
	return NONE;
}

/* the globals' grenade whose pickup (an equipment) this definition is: its index, else NONE */
static short grenade_of_item(long definition_index)
{
	struct game_globals *globals = scenario_get_game_globals();
	struct equipment_definition *equipment;
	int item_kind = -1;
	short index;

	if (!globals || definition_index == NONE)
		return NONE;
	equipment = equipment_definition_get(definition_index);
	for (index = 0; index < globals->grenades.count; index++)
	{
		struct game_globals_grenade *grenade = TAG_BLOCK_GET_ELEMENT(&globals->grenades, index, struct game_globals_grenade);

		if (grenade->item.index == definition_index)
			item_kind = index;
	}
	return (short)ae_grenade_shot_pickup_kind(equipment && equipment->equipment.powerup_type == _equipment_powerup_grenade,
		equipment ? equipment->equipment.grenade_type : -1, globals->grenades.count, item_kind);
}

/* a copy that is already going off (a projectile with its fuse out or an action set) */
static boolean going_off(struct object_datum *object, boolean pickup)
{
	struct projectile_datum *projectile = (struct projectile_datum *)object;

	/* (an action set, projectiles.c's detonate or disappear, is a grenade already going) */
	return !pickup && (projectile->projectile.detonation_timer >= 1.0f || projectile->projectile.action != 0);
}

/* a grenade copy's kind (projectile or pickup), NONE when it is not a grenade this rule sets off: not one stuck to someone
or held, nor (unless going is TRUE) one already going off */
static short copy_kind(struct object_datum *object, boolean pickup, boolean going)
{
	if (object->object.parent_object_index != NONE)
		return NONE;
	if (!going && going_off(object, pickup))
		return NONE;
	return pickup ? grenade_of_item(object->definition_index) : grenade_of(object->definition_index);
}

/* the shooter's fields on a grenade: his kill */
static void give_owner(long grenade_index, long owner_unit, long owner_player, short owner_team)
{
	struct object_datum *grenade = object_get(grenade_index);

	grenade->object.owner_object_index = owner_unit;
	grenade->object.owner_player_index = owner_player;
	grenade->object.owner_team_index = owner_team;
}

/* sets a copy off now: a pickup is replaced by its kind's projectile where it lies; the shooter owns it. The projectile
(NONE when it could not be made) */
static long set_off(long index, boolean pickup, short kind, real_point3d const *point, long owner_unit, long owner_player,
	short owner_team)
{
	if (pickup)
	{
		struct game_globals *globals = scenario_get_game_globals();
		struct game_globals_grenade *grenade;
		struct object_placement_data placement;
		long made;

		if (!globals || kind < 0 || kind >= globals->grenades.count)
			return NONE;
		grenade = TAG_BLOCK_GET_ELEMENT(&globals->grenades, kind, struct game_globals_grenade);
		if (grenade->projectile.index == NONE)
			return NONE;
		object_placement_data_new(&placement, grenade->projectile.index, owner_unit);
		placement.position = *point;
		placement.forward = *global_down3d;
		placement.up = *global_forward3d;
		/* (the projectile first: when it cannot be made the pickup stays, and no blast is told) */
		made = object_new(&placement);
		if (made == NONE)
			return NONE;
		object_delete(index);
		index = made;
	}
	give_owner(index, owner_unit, owner_player, owner_team);
	ae_projectile_detonate(index);
	return index;
}

/* the nearest copy of the kind (pickups or live grenades) to the point within tolerance: its index, else NONE. The nearest
one already going off is the blast already made: NONE, not the next copy along */
static long nearest_copy(real_point3d const *point, short kind, boolean pickup, boolean (*near)(real distance, boolean pickup),
	real_point3d *center_out)
{
	struct object_iterator iterator;
	struct object_datum *object;
	long best = NONE;
	boolean best_going = FALSE;
	real best_distance = 0.f;

	object_iterator_new(&iterator, pickup ? _object_mask_equipment : _object_mask_projectile, 0);
	while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
	{
		real_point3d center;
		real radius, distance;

		if (copy_kind(object, pickup, TRUE) != kind)
			continue;
		object_get_bounding_sphere(iterator.index, &center, &radius);
		distance = distance3d(&center, point);
		if (near(distance, pickup) && (best == NONE || distance < best_distance))
		{
			best = iterator.index;
			best_distance = distance;
			best_going = going_off(object, pickup);
			*center_out = center;
		}
	}
	return best_going ? NONE : best;
}

static boolean report_near(real distance, boolean pickup)
{
	return ae_grenade_shot_report_near(distance, pickup);
}

static boolean copy_near(real distance, boolean pickup)
{
	(void)pickup;
	return ae_grenade_shot_copy_near(distance);
}

/* whether a wall is between a start and a grenade at the point */
static boolean wall_between(real_point3d const *from, real_point3d const *point, real radius)
{
	real_vector3d ray;
	struct collision_result collision;
	real length;

	vector_from_points3d(from, point, &ray);
	length = magnitude3d(&ray);
	if (length > 0.f)
		scale_vector3d(&ray, ae_grenade_shot_wall_length(length, radius) / length, &ray);
	return collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit), from,
		&ray, NONE, &collision) != 0;
}

static void send_message(unsigned char type, short destination, short kind, boolean pickup, long player, real_point3d const *point)
{
	struct ae_grenade_shot_message message;

	memset(&message, 0, sizeof(message));
	message.version = AE_GRENADE_SHOT_VERSION;
	message.kind = (unsigned char)kind;
	message.flags = pickup ? AE_GRENADE_SHOT_FLAG_PICKUP : 0;
	message.player = distributed_player_to_byte(player);
	message.point = *point;
	distributed_send(&message, type, 1, (word)sizeof(message), destination);
}

/* the host's (or an offline game's) decision to set a copy off: remembers the blast, sets it off and tells the clients */
static void host_blast(long index, boolean pickup, short kind, real_point3d const *point, long owner_unit, long owner_player,
	short owner_team)
{
	if (set_off(index, pickup, kind, point, owner_unit, owner_player, owner_team) == NONE)
		return;
	blasts[blast_next % AE_GRENADE_SHOT_BLASTS].point = *point;
	blasts[blast_next % AE_GRENADE_SHOT_BLASTS].time = game_time_get();
	blast_next++;
	platform_log("grenade shot: player %ld sets off %s of grenade %d at %.2f %.2f %.2f",
		(long)(owner_player != NONE ? DATUM_INDEX_TO_ABSOLUTE_INDEX(owner_player) : -1), pickup ? "a pickup" : "a live one",
		(int)kind, (double)point->x, (double)point->y, (double)point->z);
	if (game_connection() == _game_connection_network_server)
		send_message(_distributed_message_ae_grenade_shot_blast, _distributed_to_clients_reliably, kind, pickup, owner_player,
			point);
}

void ae_grenade_shot(long projectile_index, real_point3d const *from, real_point3d const *to)
{
	struct object_datum *bullet;
	struct object_iterator iterator;
	struct object_datum *object;
	short connection;
	long best = NONE;
	short best_kind = NONE;
	boolean best_pickup = FALSE;
	real best_t = 2.f;
	real best_radius = 0.f;
	real_point3d best_point;
	struct ae_grenade_shot_point shot_from, shot_to;
	short pass;

	if (!rule_on())
		return;
	connection = game_connection();
	bullet = object_get(projectile_index);
	/* (a client reports its own players' bullets; the others' it leaves to the host) */
	if (connection == _game_connection_network_client &&
		(bullet->object.owner_player_index == NONE || !distributed_player_is_local(bullet->object.owner_player_index)))
	{
		return;
	}
	if (grenade_of(bullet->definition_index) != NONE)
		return;
	shot_from = shot_point(from);
	shot_to = shot_point(to);
	if (connection == _game_connection_network_server && bullet->object.owner_player_index != NONE)
	{
		/* (the host's view of each player's shots: what a client's report is matched with) */
		long player = DATUM_INDEX_TO_ABSOLUTE_INDEX(bullet->object.owner_player_index);

		if (player >= 0 && player < AE_GRENADE_SHOT_PLAYERS)
		{
			short slot = seen_next[player]++ % AE_GRENADE_SHOT_SEEN;

			seen[player][slot].from = shot_from;
			seen[player][slot].to = shot_to;
			seen[player][slot].time = game_time_get();
		}
	}
	for (pass = 0; pass < 2; pass++)
	{
		object_iterator_new(&iterator, pass == 0 ? _object_mask_projectile : _object_mask_equipment, 0);
		while ((object = (struct object_datum *)object_iterator_next(&iterator)) != NULL)
		{
			real_point3d center;
			struct ae_grenade_shot_point shot_center;
			real radius;
			float t;
			short kind;

			if (iterator.index == projectile_index)
				continue;
			kind = copy_kind(object, pass == 1, FALSE);
			if (kind == NONE)
				continue;
			object_get_bounding_sphere(iterator.index, &center, &radius);
			radius = ae_grenade_shot_radius(radius);
			shot_center = shot_point(&center);
			if (!ae_grenade_shot_hit(&shot_from, &shot_to, &shot_center, radius, &t) || !ae_grenade_shot_nearer(t, best_t))
				continue;
			best = iterator.index;
			best_kind = kind;
			best_pickup = pass == 1;
			best_point = center;
			best_radius = radius;
			best_t = t;
		}
	}
	if (best == NONE || wall_between(from, &best_point, best_radius))
		return;
	if (connection == _game_connection_network_client)
	{
		if (best == reported_object && game_time_get() - reported_time < AE_GRENADE_SHOT_REPORTED_TICKS)
			return;
		reported_object = best;
		reported_time = game_time_get();
		send_message(_distributed_message_ae_grenade_shot_report, _distributed_to_host_reliably, best_kind, best_pickup,
			bullet->object.owner_player_index, &best_point);
		return;
	}
	host_blast(best, best_pickup, best_kind, &best_point, bullet->object.owner_object_index, bullet->object.owner_player_index,
		bullet->object.owner_team_index);
}

int ae_grenade_shot_handles_message(unsigned short const *message, unsigned short size)
{
	struct distributed_message_header header;

	if (size < sizeof(header))
		return 0;
	memcpy(&header, message, sizeof(header));
	return header.type == _distributed_message_ae_grenade_shot_report || header.type == _distributed_message_ae_grenade_shot_blast;
}

void ae_grenade_shot_handle_message(long machine_index, unsigned short const *data, unsigned short size, int from_stream)
{
	struct ae_grenade_shot_message message;
	struct game_globals *globals = scenario_get_game_globals();
	short connection = game_connection();
	boolean pickup;
	long player_index, index, owner_unit;
	struct player_datum *player;
	real_point3d center;

	if (size != sizeof(message) || !from_stream || !globals)
		return;
	memcpy(&message, data, sizeof(message));
	if (GET_MESSAGE_TYPE(message.header.header) != 2 || message.header.count != 1 || message.version != AE_GRENADE_SHOT_VERSION ||
		!ae_grenade_shot_message_ok(message.kind, globals->grenades.count, message.flags) || !rule_on() ||
		!(message.point.x == message.point.x && message.point.y == message.point.y && message.point.z == message.point.z))
	{
		return;
	}
	pickup = (message.flags & AE_GRENADE_SHOT_FLAG_PICKUP) != 0;
	player_index = distributed_player_from_byte(message.player);
	player = player_index != NONE ? distributed_player((short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index)) : NULL;
	owner_unit = player ? distributed_living_unit(player) : NONE;
	if (message.header.type == _distributed_message_ae_grenade_shot_blast)
	{
		/* (the host's, to a client: the nearest copy of that kind goes off, its owner the shooter) */
		if (machine_index != NONE || connection != _game_connection_network_client)
			return;
		index = nearest_copy(&message.point, message.kind, pickup, copy_near, &center);
		if (index != NONE && set_off(index, pickup, message.kind, &center, owner_unit, player_index,
			player ? player->team_index : NONE) != NONE)
		{
			platform_log("grenade shot: the host's blast of %s of grenade %d, player %d", pickup ? "a pickup" : "a live one",
				(int)message.kind, (int)message.player);
		}
		return;
	}
	/* (a client's report, to the host: its machine's player, the rate, a copy of that kind near the point, not a blast made a
	moment ago, the line clear from the shooter's head) */
	if (machine_index == NONE || connection != _game_connection_network_server || !player || owner_unit == NONE ||
		!distributed_machine_has_player(machine_index, (short)message.player) || message.player >= AE_GRENADE_SHOT_PLAYERS ||
		!ae_grenade_shot_report_rate_allows(game_time_get(), last_report_time[message.player]))
	{
		return;
	}
	last_report_time[message.player] = game_time_get();
	for (index = 0; index < AE_GRENADE_SHOT_BLASTS; index++)
	{
		if (ae_grenade_shot_is_repeat(distance3d(&blasts[index].point, &message.point), game_time_get(), blasts[index].time))
			return;
	}
	index = nearest_copy(&message.point, message.kind, pickup, report_near, &center);
	if (index == NONE)
		return;
	/* (a bullet the host saw of that player lately, its line passing the grenade: a report with no shot behind it is a cheat's) */
	{
		struct ae_grenade_shot_point grenade_point = shot_point(&center);
		short slot;
		boolean matched = FALSE;

		for (slot = 0; slot < AE_GRENADE_SHOT_SEEN && !matched; slot++)
		{
			matched = ae_grenade_shot_fire_recent(game_time_get(), seen[message.player][slot].time) &&
				ae_grenade_shot_on_ray(&seen[message.player][slot].from, &seen[message.player][slot].to, &grenade_point);
		}
		if (!matched)
			return;
	}
	{
		real_point3d head;
		real radius;

		unit_get_head_position(owner_unit, &head);
		object_get_bounding_sphere(index, &center, &radius);
		if (wall_between(&head, &center, ae_grenade_shot_radius(radius)))
			return;
	}
	host_blast(index, pickup, message.kind, &center, owner_unit, player_index, player->team_index);
}
