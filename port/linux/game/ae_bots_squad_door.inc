/*
AE_BOTS_SQUAD_DOOR.INC

Arena Evolved: the campaign AI squad at a door (ae_bots_squad_rules.h, DOOR STACK): it lines up beside the frame on the
player's side, outside the door's reach, and on GO goes in nearest first, each to a corner of its own, a grenade first when an
enemy in the room was seen or heard. Included by ae_bots_squad.inc.
*/

#include "ae_campaign_rows_rules.h"

int config_text(char const *name, char *text, size_t size);

/* a machine definition's kind for a door (device_machines.c) */
#define AE_SQUAD_DOOR_MACHINE_KIND 0
/* a door the squad has just done is not stacked at on its own again for this long (EVERY DOOR) */
#define AE_SQUAD_DOOR_AGAIN_TICKS 600

/* the door the squad works (NONE none), its state, its centre and facing, the squad's side and where the room opens, the
members in entry order, since when the state holds, when the first man came into its reach, when an enemy in the room was
last seen or heard, the grenade's throw, the last enemy seen inside; the order's serial the stack answers, whether the squad
stacked on its own (EVERY DOOR), whether GO came from the player's ATTACK; the player's stillness for EVERY DOOR; halo.log's
counts */
static struct
{
	long door;
	int state;
	real_point3d centre;
	real forward_x;
	real forward_y;
	real radius;
	int side;
	int open_left;
	int order[AE_COMPANION_MOST];
	int count;
	long since;
	long first_in_reach;
	long seen_or_heard;
	long thrown;
	long seen_inside;
	boolean said;
	/* PLACES: the stack point, each member's place (placed: one found; else it holds where it is); the
	room's clear list (its points, their height, visited or not), each member's call of its side said */
	real_point3d stack_point;
	real_point3d place[AE_COMPANION_MOST];
	boolean placed[AE_COMPANION_MOST];
	float clear_points[AE_SQUAD_CLEAR_POINTS][2];
	real clear_z;
	int clear_visited[AE_SQUAD_CLEAR_POINTS];
	int clear_count;
	boolean side_said[AE_COMPANION_MOST];
	long serial;
	boolean on_its_own;
	boolean ordered;
	real_point3d player_was;
	long player_still_since;
	long last_door;
	long ended;
	long stacks;
	long entries;
	long grenades;
	long locked;
} ae_squad_door = { NONE };

/* the door forgotten (game time went back: a revert, a new level) */
static void ae_squad_door_forget(void)
{
	ae_squad_door.state = AE_SQUAD_DOOR_NONE;
	ae_squad_door.door = NONE;
	ae_squad_door.last_door = NONE;
	ae_squad_door.ended = 0;
	ae_squad_door.player_still_since = 0;
}

/* the door's open fraction (0 shut, 1 open) */
static real ae_squad_door_position(long door)
{
	struct machine_datum const *machine = (struct machine_datum const *)object_try_and_get_and_verify_type(door, _object_mask_machine);

	return machine ? machine->device.position : 0.0f;
}

/* whether a machine is a door, and whether the level keeps it from opening by itself (locked) */
static boolean ae_squad_door_is(long object_index, boolean *locked)
{
	struct machine_datum const *machine = (struct machine_datum const *)object_try_and_get_and_verify_type(object_index,
		_object_mask_machine);

	if (!machine || machine_definition_get(machine->definition_index)->machine.type != AE_SQUAD_DOOR_MACHINE_KIND)
		return FALSE;
	if (locked)
		*locked = (boolean)ae_bots_squad_door_shut_for_good(TEST_FLAG(machine->machine.flags, _machine_does_not_operate_automatically_bit),
			(float)machine->device.power);
	return TRUE;
}

/* the nearest door within reach of a point, NONE none; on the point's own floor (its base within 1.5 in height); shut_only:
only a shut door the level has not locked (for EVERY DOOR, a pod's or an opened door beside the player is no door to stack at) */
static long ae_squad_door_near(real_point3d const *point, real reach, boolean shut_only)
{
	struct object_iterator iterator;
	long best = NONE;
	real best_distance = reach;

	object_iterator_new(&iterator, _object_mask_machine, 0);
	while (object_iterator_next(&iterator))
	{
		real distance;
		boolean locked = FALSE;

		if (!ae_squad_door_is(iterator.index, &locked) ||
			(real)fabs(object_get(iterator.index)->object.position.z - point->z) > 1.5f ||
			(shut_only && (locked || ae_squad_door_position(iterator.index) >= 0.05f)))
			continue;
		distance = bots_distance3d(&object_get(iterator.index)->object.bounding_sphere_center, point);
		if (distance <= best_distance)
		{
			best_distance = distance;
			best = iterator.index;
		}
	}
	return best;
}

/* a door's reach: its automatic activation radius, else its bounding sphere's */
static real ae_squad_door_reach_of(long door)
{
	struct object_datum const *object = object_get(door);
	struct machine_definition const *definition = machine_definition_get(object->definition_index);

	return definition->device.automatic_activation_radius > _real_epsilon ? definition->device.automatic_activation_radius :
		object->object.bounding_sphere_radius;
}

/* DOOR KEEP-OUT as a filter (ae_bots_squad_in_any_door_reach): a point within a door's reach + pad on its floor, but door
except; the door in *door_at (NONE none) */
static boolean ae_squad_in_door_reach(real_point3d const *point, real pad, long except, long *door_at)
{
	struct object_iterator iterator;

	if (door_at)
		*door_at = NONE;
	object_iterator_new(&iterator, _object_mask_machine, 0);
	while (object_iterator_next(&iterator))
	{
		struct object_datum const *object;

		if (iterator.index == except || !ae_squad_door_is(iterator.index, NULL))
			continue;
		object = object_get(iterator.index);
		if ((real)fabs(object->object.position.z - point->z) > 1.5f)
			continue;
		if (ae_bots_squad_in_any_door_reach((float)bots_distance2d(&object->object.bounding_sphere_center, point),
				(float)(ae_squad_door_reach_of(iterator.index) + pad - AE_SQUAD_DOOR_KEEP_OUT), 0))
		{
			if (door_at)
				*door_at = iterator.index;
			return TRUE;
		}
	}
	return FALSE;
}

static boolean ae_squad_door_line(real_point3d const *a, real_point3d const *b);

/* EVERY DOOR's door: the nearest door ahead of the player (ae_bots_squad_door_ahead), open or shut, on the player's floor, not shut
for good; NONE none */
static long ae_squad_door_ahead_of(long player_unit, real_point3d const *player_at)
{
	struct unit_datum const *unit = (struct unit_datum const *)object_try_and_get_and_verify_type(player_unit, _object_mask_unit);
	struct object_iterator iterator;
	real facing, best_distance = AE_SQUAD_DOOR_AUTO_NEAR + 1.0f;
	long best = NONE;

	if (!unit)
		return NONE;
	facing = (real)atan2(unit->unit.aiming_vector.j, unit->unit.aiming_vector.i);
	object_iterator_new(&iterator, _object_mask_machine, 0);
	while (object_iterator_next(&iterator))
	{
		struct object_datum const *object = object_get(iterator.index);
		boolean locked = FALSE;
		real distance, way;

		if (!ae_squad_door_is(iterator.index, &locked) || locked || (real)fabs(object->object.position.z - player_at->z) > 1.5f)
			continue;
		distance = bots_distance2d(&object->object.bounding_sphere_center, player_at);
		way = (real)atan2(object->object.bounding_sphere_center.y - player_at->y, object->object.bounding_sphere_center.x - player_at->x);
		if (ae_bots_squad_door_ahead((float)bots_angle_difference(way, facing), (float)distance) && distance < best_distance)
		{
			real_point3d chest = *player_at, short_of = object->object.bounding_sphere_center;
			real back = distance > 0.6f ? 0.6f / distance : 0.0f;

			/* (a door he can walk to: no wall between him and the door, up to 0.6 short of it) */
			chest.z += 0.6f;
			short_of.x -= (short_of.x - player_at->x) * back;
			short_of.y -= (short_of.y - player_at->y) * back;
			short_of.z = chest.z;
			if (!ae_squad_door_line(&chest, &short_of))
				continue;
			best = iterator.index;
			best_distance = distance;
		}
	}
	return best;
}

/* a level ray's length (to the level's first wall, at most length) */
static real ae_squad_door_ray(real_point3d const *from, real yaw, real length)
{
	struct collision_result collision;
	real_vector3d vector;

	vector.i = (real)cos(yaw) * length;
	vector.j = (real)sin(yaw) * length;
	vector.k = 0.0f;
	if (!collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit), from, &vector,
			NONE, &collision))
		return length;
	return collision.t * length;
}

/* the squad's member brains (slots) on foot and alive, their units */
static boolean ae_squad_door_member_unit(long slot, long *unit)
{
	struct bots_bot const *bot = &bots_globals.bots[slot];
	struct player_datum const *player = bot->player_index != NONE ? player_try_and_get(bot->player_index) : NULL;
	struct object_datum const *object;

	if (!player || player->unit_index == NONE || ae_fireteam_of_brain(slot) == NONE)
		return FALSE;
	object = object_get(player->unit_index);
	if (TEST_FLAG(object->object.damage_flags, _object_dead_bit) || object->object.parent_object_index != NONE)
		return FALSE;
	*unit = player->unit_index;
	return TRUE;
}

/* whether a line between two points, both at a chest's height (0.6) above them, is clear of the level */
static boolean ae_squad_door_line(real_point3d const *a, real_point3d const *b)
{
	struct collision_result collision;
	real_point3d start = *a;
	real_vector3d vector;

	start.z += 0.6f;
	vector.i = b->x - a->x;
	vector.j = b->y - a->y;
	vector.k = b->z + 0.6f - start.z;
	return (boolean)!collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
		FLAG(_collision_test_back_facing_surfaces_bit), &start, &vector, NONE, &collision);
}

/* a walkable place at x, y near height z (ae_bots_squad_grid_walkable without its line): TRUE with the floor point */
static boolean ae_squad_door_floor(real x, real y, real z, real_point3d *floor_at)
{
	struct collision_result collision;
	real_point3d start, above;
	real_vector3d down, up;
	real normal_z, headroom = AE_SQUAD_PLACE_HEADROOM + 0.1f;

	start.x = x;
	start.y = y;
	start.z = z + 0.6f;
	down.i = 0.0f;
	down.j = 0.0f;
	down.k = -(0.6f + AE_SQUAD_PLACE_DROP + 0.05f);
	up.i = 0.0f;
	up.j = 0.0f;
	if (!collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit), &start, &down,
			NONE, &collision))
		return FALSE;
	*floor_at = collision.point;
	normal_z = collision.plane.n.k;
	above = collision.point;
	above.z += 0.1f;
	up.k = headroom;
	if (collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
			FLAG(_collision_test_back_facing_surfaces_bit), &above, &up, NONE, &collision))
		headroom = collision.t * headroom;
	return (boolean)ae_bots_squad_grid_walkable((float)fabs(floor_at->z - z), (float)normal_z, (float)headroom, 1);
}

/* a point on the door's axis, along of its facing on the squad's side (under 0: inside) */
static void ae_squad_door_axis_point(real along, real_point3d *point)
{
	*point = ae_squad_door.centre;
	point->x += ae_squad_door.forward_x * along * (real)ae_squad_door.side;
	point->y += ae_squad_door.forward_y * along * (real)ae_squad_door.side;
}

/* PLACES (ae_bots_squad_rules.h): the stack point just outside the door's reach on the squad's side (pulled
back while its line to the door is blocked), the walkable grid points about it ring by ring, each skipped or scored, and the
members' places assigned in entry order */
static void ae_squad_door_places(void)
{
	static float places[AE_SQUAD_PLACES][3];
	static real places_z[AE_SQUAD_PLACES];
	float men[AE_COMPANION_MOST][2];
	int slot_of_man[AE_COMPANION_MOST], count = 0, ring, man, rings = (int)(AE_SQUAD_PLACE_REACH / AE_SQUAD_PLACE_STEP);
	real_point3d inside, floor_point, door_floor = ae_squad_door.centre;
	real lateral_x = -ae_squad_door.forward_y, lateral_y = ae_squad_door.forward_x;
	long foes[32], foe_count = ae_companion_foes(foes, 32), slot;

	door_floor.z -= 0.6f;
	ae_squad_door_axis_point(ae_squad_door.radius + AE_SQUAD_STACK_PAD, &ae_squad_door.stack_point);
	ae_squad_door.stack_point.z = door_floor.z;
	for (ring = 0; ring < 3 && !ae_squad_door_line(&ae_squad_door.stack_point, &door_floor); ring++)
	{
		ae_squad_door.stack_point.x += ae_squad_door.forward_x * 0.5f * (real)ae_squad_door.side;
		ae_squad_door.stack_point.y += ae_squad_door.forward_y * 0.5f * (real)ae_squad_door.side;
	}
	if (ae_squad_door_floor(ae_squad_door.stack_point.x, ae_squad_door.stack_point.y, door_floor.z, &floor_point))
		ae_squad_door.stack_point = floor_point;
	ae_squad_door_axis_point(-1.5f, &inside);
	inside.z = door_floor.z;
	for (ring = 0; ring <= rings && count < AE_SQUAD_PLACES; ring++)
	{
		int ix, iy;

		for (ix = -ring; ix <= ring && count < AE_SQUAD_PLACES; ix++)
			for (iy = -ring; iy <= ring && count < AE_SQUAD_PLACES; iy++)
			{
				real x, y, to_door;
				int wall, foe_near = 0, foe;
				real_point3d side_a, side_b;

				if (ix != -ring && ix != ring && iy != -ring && iy != ring)
					continue;
				x = ae_squad_door.stack_point.x + (real)ix * AE_SQUAD_PLACE_STEP;
				y = ae_squad_door.stack_point.y + (real)iy * AE_SQUAD_PLACE_STEP;
				if (!ae_squad_door_floor(x, y, ae_squad_door.stack_point.z, &floor_point))
					continue;
				to_door = bots_distance2d(&floor_point, &ae_squad_door.centre);
				if (ae_bots_squad_stack_skip((float)bots_distance2d(&floor_point, &ae_squad_door.stack_point),
						ae_bots_squad_door_side((float)ae_squad_door.forward_x, (float)ae_squad_door.forward_y,
							(float)(floor_point.x - ae_squad_door.centre.x), (float)(floor_point.y - ae_squad_door.centre.y)) !=
							ae_squad_door.side, (float)to_door, (float)ae_squad_door.radius))
					continue;
				/* (no place in another door's reach: a body there opens it) */
				if (ae_squad_in_door_reach(&floor_point, AE_SQUAD_STACK_PAD, ae_squad_door.door, NULL))
					continue;
				side_a = floor_point;
				side_a.x += lateral_x * 0.35f;
				side_a.y += lateral_y * 0.35f;
				side_b = floor_point;
				side_b.x -= lateral_x * 0.35f;
				side_b.y -= lateral_y * 0.35f;
				wall = !ae_squad_door_line(&floor_point, &side_a) || !ae_squad_door_line(&floor_point, &side_b);
				for (foe = 0; foe < foe_count; foe++)
				{
					struct object_datum const *object = (struct object_datum const *)object_try_and_get(foes[foe]);

					if (object && !TEST_FLAG(object->object.damage_flags, _object_dead_bit) &&
						bots_distance2d(&object->object.position, &floor_point) < 1.0f)
						foe_near = 1;
				}
				places[count][0] = (float)floor_point.x;
				places[count][1] = (float)floor_point.y;
				places[count][2] = ae_bots_squad_stack_score(wall, ae_squad_door_line(&ae_squad_door.stack_point, &floor_point),
					ae_squad_door_line(&inside, &floor_point), foe_near) +
					(float)ae_bots_squad_stack_sector((float)atan2(ae_squad_door.centre.y - floor_point.y, ae_squad_door.centre.x - floor_point.x),
						(float)atan2(ae_squad_door.stack_point.y - floor_point.y, ae_squad_door.stack_point.x - floor_point.x));
				places_z[count] = floor_point.z;
				count++;
			}
	}
	for (man = 0; man < ae_squad_door.count; man++)
	{
		long unit;
		real_point3d at = ae_squad_door.stack_point;

		if (ae_squad_door_member_unit(ae_squad_door.order[man], &unit))
			object_get_origin(unit, &at);
		men[man][0] = (float)at.x;
		men[man][1] = (float)at.y;
	}
	(void)ae_bots_squad_stack_assign((float const (*)[3])places, count, (float const (*)[2])men, ae_squad_door.count, slot_of_man);
	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
		ae_squad_door.placed[slot] = FALSE;
	for (man = 0; man < ae_squad_door.count; man++)
	{
		int place = slot_of_man[man];

		slot = ae_squad_door.order[man];
		if (place < 0)
			continue;
		ae_squad_door.placed[slot] = TRUE;
		ae_squad_door.place[slot].x = places[place][0];
		ae_squad_door.place[slot].y = places[place][1];
		ae_squad_door.place[slot].z = places_z[place];
		platform_log("ae squad: %ld's stack place %.1f %.1f %.1f (score %.0f, of %d places)", slot + 1, ae_squad_door.place[slot].x,
			ae_squad_door.place[slot].y, ae_squad_door.place[slot].z, places[place][2], count);
	}
}

/* the room's clear list: walkable points inside the room, 0.75 apart within 6 of a point 1 inside the door, that
a line from that point reaches, nearest first */
static void ae_squad_door_clear_list(void)
{
	real_point3d from, floor_point;
	int ring, index;

	ae_squad_door_axis_point(-1.0f, &from);
	from.z -= 0.6f;
	ae_squad_door.clear_count = 0;
	ae_squad_door.clear_z = from.z;
	for (ring = 0; ring <= 8 && ae_squad_door.clear_count < AE_SQUAD_CLEAR_POINTS; ring++)
	{
		int ix, iy;

		for (ix = -ring; ix <= ring && ae_squad_door.clear_count < AE_SQUAD_CLEAR_POINTS; ix++)
			for (iy = -ring; iy <= ring && ae_squad_door.clear_count < AE_SQUAD_CLEAR_POINTS; iy++)
			{
				if ((ix != -ring && ix != ring && iy != -ring && iy != ring) ||
					!ae_squad_door_floor(from.x + (real)ix * 0.75f, from.y + (real)iy * 0.75f, from.z, &floor_point) ||
					ae_bots_squad_door_side((float)ae_squad_door.forward_x, (float)ae_squad_door.forward_y,
						(float)(floor_point.x - ae_squad_door.centre.x), (float)(floor_point.y - ae_squad_door.centre.y)) ==
						ae_squad_door.side || !ae_squad_door_line(&from, &floor_point))
					continue;
				ae_squad_door.clear_points[ae_squad_door.clear_count][0] = (float)floor_point.x;
				ae_squad_door.clear_points[ae_squad_door.clear_count][1] = (float)floor_point.y;
				ae_squad_door.clear_count++;
			}
	}
	for (index = 0; index < ae_squad_door.clear_count; index++)
		ae_squad_door.clear_visited[index] = 0;
	platform_log("ae squad: the room's clear list has %d points", ae_squad_door.clear_count);
}

/* the stack begun at a door: the player's side, the room's open side, the members by distance (none on foot: no stack);
on_its_own: EVERY DOOR's, not an order's */
static void ae_squad_door_begin(long door, real_point3d const *player_at, long now, boolean on_its_own)
{
	struct object_datum const *object = object_get(door);
	struct machine_datum const *machine = (struct machine_datum const *)object;
	struct machine_definition const *definition = machine_definition_get(machine->definition_index);
	float distances[AE_COMPANION_MOST];
	real_point3d inside;
	real left;
	long slot;

	ae_squad_door.door = door;
	ae_squad_door.centre = object->object.bounding_sphere_center;
	ae_squad_door.forward_x = object->object.forward.i;
	ae_squad_door.forward_y = object->object.forward.j;
	ae_squad_door.radius = definition->device.automatic_activation_radius > _real_epsilon ?
		definition->device.automatic_activation_radius : object->object.bounding_sphere_radius;
	ae_squad_door.side = ae_bots_squad_door_side((float)ae_squad_door.forward_x, (float)ae_squad_door.forward_y,
		(float)(player_at->x - ae_squad_door.centre.x), (float)(player_at->y - ae_squad_door.centre.y));
	/* (where the room opens: two rays along the frame from 1 unit inside it, at a chest's height) */
	inside = ae_squad_door.centre;
	inside.x -= ae_squad_door.forward_x * (real)ae_squad_door.side;
	inside.y -= ae_squad_door.forward_y * (real)ae_squad_door.side;
	left = (real)atan2(ae_squad_door.forward_x, -ae_squad_door.forward_y);
	ae_squad_door.open_left = ae_bots_squad_door_open_left((float)ae_squad_door_ray(&inside, left, 6.0f),
		(float)ae_squad_door_ray(&inside, left + BOTS_PI, 6.0f));
	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
	{
		long unit;
		real_point3d at;

		distances[slot] = -1.0f;
		if (!ae_squad_door_member_unit(slot, &unit))
			continue;
		object_get_origin(unit, &at);
		distances[slot] = (float)bots_distance2d(&at, &ae_squad_door.centre);
	}
	ae_squad_door.count = ae_bots_squad_entry_order(distances, AE_COMPANION_MOST, ae_squad_door.order);
	if (ae_squad_door.count == 0)
	{
		ae_squad_door.door = NONE;
		return;
	}
	ae_squad_door.serial = ae_fireteam_order_serial_of(ae_fireteam_of_brain(0));
	ae_squad_door.on_its_own = on_its_own;
	ae_squad_door.ordered = FALSE;
	ae_squad_door.state = AE_SQUAD_DOOR_STACKING;
	ae_squad_door.since = now;
	ae_squad_door.first_in_reach = 0;
	ae_squad_door.seen_or_heard = -1;
	ae_squad_door.seen_inside = now;
	ae_squad_door.said = FALSE;
	ae_squad_door.stacks++;
	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
		ae_squad_door.side_said[slot] = FALSE;
	ae_squad_door_places();
	platform_log("ae squad: door at %.1f %.1f %.1f (reach %.1f, facing %.2f %.2f): stack on side %d, the room opens %s, %d members",
		ae_squad_door.centre.x, ae_squad_door.centre.y, ae_squad_door.centre.z, ae_squad_door.radius, ae_squad_door.forward_x,
		ae_squad_door.forward_y, ae_squad_door.side, ae_squad_door.open_left ? "left" : "right", ae_squad_door.count);
}

/* the door given up; follow: the door's own end (clear, locked, gone), the squad follows the player again (a new order of the
player's stands as given) */
static void ae_squad_door_end(char const *why, boolean follow)
{
	long fireteam = ae_fireteam_of_brain(0);

	if (ae_squad_door.state == AE_SQUAD_DOOR_NONE)
		return;
	ae_squad_door.state = AE_SQUAD_DOOR_NONE;
	ae_squad_door.last_door = ae_squad_door.door;
	ae_squad_door.ended = game_time_get();
	ae_squad_door.door = NONE;
	if (follow && fireteam != NONE && ae_fireteam_order_of(fireteam) != _ae_order_follow)
		ae_fireteam_order((int)fireteam, _ae_order_follow, NULL, NONE);
	platform_log("ae squad: door done (%s)", why);
}

/* a line of chat from the first member in entry order */
static void ae_squad_door_say(wchar_t const *words)
{
	int rank;

	for (rank = 0; rank < ae_squad_door.count; rank++)
	{
		struct bots_bot const *bot = &bots_globals.bots[ae_squad_door.order[rank]];
		struct player_datum const *player = bot->player_index != NONE ? player_try_and_get(bot->player_index) : NULL;
		long unit;

		if (player && ae_squad_door_member_unit(ae_squad_door.order[rank], &unit))
		{
			ae_squad_say(player, words);
			return;
		}
	}
}

/* the slot of the nth member on foot in entry order (0 the first: it opens the door, 1 the second: it throws), -1 none */
static int ae_squad_door_living(int nth)
{
	int rank;

	for (rank = 0; rank < ae_squad_door.count; rank++)
	{
		long unit;

		if (ae_squad_door_member_unit(ae_squad_door.order[rank], &unit) && nth-- == 0)
			return ae_squad_door.order[rank];
	}
	return -1;
}

/* the point the grenade goes to: 3 inside the door, on its axis */
static void ae_squad_door_blast(real_point3d *blast)
{
	*blast = ae_squad_door.centre;
	blast->x -= ae_squad_door.forward_x * 3.0f * (real)ae_squad_door.side;
	blast->y -= ae_squad_door.forward_y * 3.0f * (real)ae_squad_door.side;
}

/* GO (ordered: the player's ATTACK, not the player's own walk into or through the door): a grenade first when an enemy in the
room was seen or heard (the second man throws from the door's axis), else in at once */
static void ae_squad_door_go(long now, real_point3d const *player_at, boolean ordered)
{
	real_point3d blast;
	long thrower_unit = NONE;
	int thrower = ae_squad_door_living(1);
	int has = 0;

	ae_squad_door_blast(&blast);
	if (thrower >= 0 && ae_squad_door_member_unit(thrower, &thrower_unit))
	{
		struct unit_datum const *unit = unit_get(thrower_unit);

		has = unit->unit.grenade_counts[0] > 0 || unit->unit.grenade_counts[1] > 0;
	}
	ae_squad_door.since = now;
	ae_squad_door.first_in_reach = 0;
	ae_squad_door.thrown = 0;
	ae_squad_door.seen_inside = now;
	ae_squad_door.entries++;
	ae_squad_door_clear_list();
	/* (the clearing order: nearest the door first, those already on the room's side ahead, a tie to the nearer its
	place) */
	{
		float to_door[AE_COMPANION_MOST], to_place[AE_COMPANION_MOST];
		int room_side[AE_COMPANION_MOST];
		long slot;

		for (slot = 0; slot < AE_COMPANION_MOST; slot++)
		{
			long unit;
			real_point3d at;

			to_door[slot] = -1.0f;
			to_place[slot] = 0.0f;
			room_side[slot] = 0;
			if (!ae_squad_door_member_unit(slot, &unit))
				continue;
			object_get_origin(unit, &at);
			to_door[slot] = (float)bots_distance2d(&at, &ae_squad_door.centre);
			to_place[slot] = ae_squad_door.placed[slot] ? (float)bots_distance2d(&at, &ae_squad_door.place[slot]) : 0.0f;
			room_side[slot] = ae_bots_squad_door_side((float)ae_squad_door.forward_x, (float)ae_squad_door.forward_y,
				(float)(at.x - ae_squad_door.centre.x), (float)(at.y - ae_squad_door.centre.y)) != ae_squad_door.side;
		}
		ae_squad_door.count = ae_bots_squad_clear_order(to_door, room_side, to_place, AE_COMPANION_MOST, ae_squad_door.order);
	}
	ae_squad_door.ordered = ordered;
	ae_squad_door.serial = ae_fireteam_order_serial_of(ae_fireteam_of_brain(0));
	if (ae_bots_squad_entry_grenade(ae_squad_door.seen_or_heard < 0 ? -1 : now - ae_squad_door.seen_or_heard, has,
			(float)bots_distance3d(player_at, &blast), ordered))
	{
		ae_squad_door.state = AE_SQUAD_DOOR_GRENADE;
		platform_log("ae squad: door GO, a grenade first (%d throws)", thrower + 1);
	}
	else
	{
		ae_squad_door.state = AE_SQUAD_DOOR_ENTERING;
		platform_log("ae squad: door GO");
	}
	ae_squad_door_say(L": going in");
}

/* (once a tick, the squad's own) the order and the setting start, release and end the door; an enemy in the room seen by a member
or heard (its shot in flight) is kept */
static void ae_squad_door_tick(long now)
{
	long fireteam = ae_fireteam_of_brain(0), leader, leader_unit = NONE;
	struct player_datum const *leader_player;
	real_point3d player_at, point;
	short kind;
	char value[24];
	boolean every;

	if (!ae_fireteam_campaign_on() || fireteam == NONE)
	{
		ae_squad_door.state = AE_SQUAD_DOOR_NONE;
		return;
	}
	leader = ae_fireteam_leader_player(fireteam);
	leader_player = leader != NONE ? player_try_and_get(leader) : NULL;
	if (leader_player)
		leader_unit = leader_player->unit_index;
	if (leader_unit == NONE || TEST_FLAG(object_get(leader_unit)->object.damage_flags, _object_dead_bit))
	{
		ae_squad_door_end("no player", FALSE);
		return;
	}
	object_get_origin(leader_unit, &player_at);
	kind = ae_fireteam_order_of(fireteam);
	every = (boolean)(config_text("game.companions_door_stack", value, sizeof(value)) && ae_campaign_rows_door_is_every(value));
	/* (the player's stillness, for EVERY DOOR) */
	if (bots_distance3d(&player_at, &ae_squad_door.player_was) > 0.3f || now < ae_squad_door.player_still_since)
	{
		ae_squad_door.player_was = player_at;
		ae_squad_door.player_still_since = now;
	}
	if (ae_squad_door.state == AE_SQUAD_DOOR_NONE)
	{
		long door = NONE;
		boolean locked = FALSE;

		/* (DEFEND, or a SMART tap made one, at a door: the stack) */
		if (kind == _ae_order_defend && ae_fireteam_order_point_of(fireteam, &point))
			door = ae_squad_door_near(&point, AE_SQUAD_DOOR_NEAR, FALSE);
		else if (kind == _ae_order_follow || kind == _ae_order_none)
		{
			long near = ae_squad_door_ahead_of(leader_unit, &player_at);

			if (near != NONE && ae_squad_door_is(near, &locked) &&
				(near != ae_squad_door.last_door || now - ae_squad_door.ended > AE_SQUAD_DOOR_AGAIN_TICKS || now < ae_squad_door.ended) &&
				ae_bots_squad_door_auto(every, now - ae_squad_door.player_still_since,
					(float)bots_distance3d(&object_get(near)->object.bounding_sphere_center, &player_at),
					TRUE, locked))
				door = near;
		}
		/* (bot events on: what EVERY DOOR saw, every 2 s: the doors within 10 units, open or not, locked or not) */
		if (ae_bots_events_on() && every && now % (TICKS_PER_SECOND * 2) == 0)
		{
			struct object_iterator iterator;

			platform_log("ae squad: door check: order %d, still %ld ticks", (int)kind, now - ae_squad_door.player_still_since);
			object_iterator_new(&iterator, _object_mask_machine, 0);
			while (object_iterator_next(&iterator))
			{
				struct object_datum const *object = object_get(iterator.index);
				boolean any_locked = FALSE;
				real away = bots_distance3d(&object->object.bounding_sphere_center, &player_at);

				if (away <= 10.0f && ae_squad_door_is(iterator.index, &any_locked))
					platform_log("ae squad: door check: a door %.1f away at %.1f %.1f %.1f (open %.2f%s)", away, object->object.position.x,
						object->object.position.y, object->object.position.z, ae_squad_door_position(iterator.index),
						any_locked ? ", locked" : "");
			}
		}
		if (door != NONE)
			ae_squad_door_begin(door, &player_at, now, (boolean)(kind != _ae_order_defend));
		return;
	}
	if (!object_try_and_get(ae_squad_door.door))
	{
		ae_squad_door_end("the door is gone", TRUE);
		return;
	}
	/* (a new order of the player's, even of the same kind: ATTACK while stacking or stacked is GO, any other drops the door and
	stands as given) */
	/* (ATTACK again once GO began: the entry goes on) */
	if (ae_bots_squad_door_new_order(ae_squad_door.serial, ae_fireteam_order_serial_of(fireteam)) && kind == _ae_order_attack &&
		ae_squad_door.state != AE_SQUAD_DOOR_STACKING && ae_squad_door.state != AE_SQUAD_DOOR_STACKED)
		ae_squad_door.serial = ae_fireteam_order_serial_of(fireteam);
	if (ae_bots_squad_door_new_order(ae_squad_door.serial, ae_fireteam_order_serial_of(fireteam)))
	{
		if (kind == _ae_order_attack &&
			(ae_squad_door.state == AE_SQUAD_DOOR_STACKING || ae_squad_door.state == AE_SQUAD_DOOR_STACKED))
		{
			ae_squad_door_go(now, &player_at, TRUE);
			return;
		}
		ae_squad_door_end("another order", FALSE);
		return;
	}
	/* (a stack the squad took on its own is let go when the player walks off on its side) */
	if (ae_squad_door.on_its_own &&
		(ae_squad_door.state == AE_SQUAD_DOOR_STACKING || ae_squad_door.state == AE_SQUAD_DOOR_STACKED) &&
		ae_bots_squad_door_left((float)bots_distance3d(&player_at, &ae_squad_door.centre),
			ae_bots_squad_door_side((float)ae_squad_door.forward_x, (float)ae_squad_door.forward_y,
				(float)(player_at.x - ae_squad_door.centre.x), (float)(player_at.y - ae_squad_door.centre.y)) == ae_squad_door.side))
	{
		ae_squad_door_end("the player walked on", FALSE);
		return;
	}
	/* (an enemy in the room: seen by a member, or heard, its shot in flight; within 12 units of the door, on its far side) */
	{
		long foes[64], count = ae_companion_foes(foes, 64), index, slot;
		struct object_iterator iterator;

		for (index = 0; index < count; index++)
		{
			struct object_datum const *foe = (struct object_datum const *)object_try_and_get(foes[index]);
			boolean seen = FALSE;

			if (!foe || TEST_FLAG(foe->object.damage_flags, _object_dead_bit) ||
				bots_distance3d(&foe->object.position, &ae_squad_door.centre) > 12.0f ||
				ae_bots_squad_door_side((float)ae_squad_door.forward_x, (float)ae_squad_door.forward_y,
					(float)(foe->object.position.x - ae_squad_door.centre.x),
					(float)(foe->object.position.y - ae_squad_door.centre.y)) == ae_squad_door.side)
				continue;
			for (slot = 0; slot < AE_COMPANION_MOST; slot++)
			{
				struct bots_bot const *bot = &bots_globals.bots[slot];

				if (bot->target_player_index == BOTS_FOE_TOKEN && bot->foe_unit_index == foes[index] &&
					now - bot->target_seen_tick <= TICKS_PER_SECOND)
					seen = TRUE;
			}
			if (seen)
				ae_squad_door.seen_or_heard = now;
		}
		object_iterator_new(&iterator, _object_mask_projectile, 0);
		while (object_iterator_next(&iterator))
		{
			long owner = object_get(iterator.index)->object.owner_object_index;

			/* (a weapon's shot: its holder's; a gone owner is no one to hear) */
			while (owner != NONE)
			{
				struct object_datum const *object = (struct object_datum const *)object_try_and_get(owner);

				if (!object)
				{
					owner = NONE;
					break;
				}
				if (object->object.parent_object_index == NONE)
					break;
				owner = object->object.parent_object_index;
			}
			for (index = 0; owner != NONE && index < count; index++)
			{
				struct object_datum const *foe;

				if (foes[index] != owner)
					continue;
				foe = object_get(owner);
				if (bots_distance3d(&foe->object.position, &ae_squad_door.centre) <= 12.0f &&
					ae_bots_squad_door_side((float)ae_squad_door.forward_x, (float)ae_squad_door.forward_y,
						(float)(foe->object.position.x - ae_squad_door.centre.x),
						(float)(foe->object.position.y - ae_squad_door.centre.y)) != ae_squad_door.side)
					ae_squad_door.seen_or_heard = now;
			}
		}
	}
	/* (GO by the player's own walk: into the door's reach or through it) */
	if (ae_squad_door.state == AE_SQUAD_DOOR_STACKING || ae_squad_door.state == AE_SQUAD_DOOR_STACKED)
	{
		boolean through = (boolean)(ae_bots_squad_door_side((float)ae_squad_door.forward_x, (float)ae_squad_door.forward_y,
			(float)(player_at.x - ae_squad_door.centre.x), (float)(player_at.y - ae_squad_door.centre.y)) != ae_squad_door.side);

		if (through || bots_distance3d(&player_at, &ae_squad_door.centre) < ae_squad_door.radius)
			ae_squad_door_go(now, &player_at, FALSE);
		return;
	}
	if (ae_squad_door.state == AE_SQUAD_DOOR_GRENADE && ((ae_squad_door.thrown && now - ae_squad_door.thrown >= AE_SQUAD_GRENADE_WAIT_TICKS) ||
		now - ae_squad_door.since > AE_SQUAD_STACK_TIMEOUT_TICKS))
	{
		ae_squad_door.state = AE_SQUAD_DOOR_ENTERING;
		ae_squad_door.since = now;
	}
	/* (locked: still shut a second after the first man came into its reach) */
	if ((ae_squad_door.state == AE_SQUAD_DOOR_GRENADE || ae_squad_door.state == AE_SQUAD_DOOR_ENTERING) &&
		ae_bots_squad_door_locked(ae_squad_door.first_in_reach ? now - ae_squad_door.first_in_reach : 0,
			(float)ae_squad_door_position(ae_squad_door.door)))
	{
		ae_squad_door.locked++;
		ae_squad_door_say(L": door's locked");
		ae_squad_door_end("locked", TRUE);
		return;
	}
	/* (ENTERING: all on foot through the door, or 10 s: IN; IN: every point of the clear list visited, or 25 s: "clear") */
	if (ae_squad_door.state == AE_SQUAD_DOOR_ENTERING)
	{
		int index, through = 0, members = 0;

		for (index = 0; index < ae_squad_door.count; index++)
		{
			long unit;
			real_point3d at;

			if (!ae_squad_door_member_unit(ae_squad_door.order[index], &unit))
				continue;
			members++;
			object_get_origin(unit, &at);
			if (ae_bots_squad_door_side((float)ae_squad_door.forward_x, (float)ae_squad_door.forward_y,
					(float)(at.x - ae_squad_door.centre.x), (float)(at.y - ae_squad_door.centre.y)) != ae_squad_door.side)
				through++;
		}
		if ((members > 0 && through >= members) || now - ae_squad_door.since > AE_SQUAD_STACK_TIMEOUT_TICKS)
		{
			ae_squad_door.state = AE_SQUAD_DOOR_IN;
			ae_squad_door.since = now;
			platform_log("ae squad: in through the door (%d of %d)", through, members);
		}
	}
	if (ae_squad_door.state == AE_SQUAD_DOOR_IN &&
		(ae_bots_squad_clear_next((float const (*)[2])ae_squad_door.clear_points, ae_squad_door.clear_visited,
			ae_squad_door.clear_count, 0.0f, 0.0f) < 0 || now - ae_squad_door.since > AE_SQUAD_STACK_TIMEOUT_TICKS * 5 / 2))
	{
		ae_squad_door_say(L": clear");
		ae_squad_door_end("clear", TRUE);
	}
}

/* a member's walk to a point; TRUE when it is there (within 0.4), facing toward */
static boolean ae_squad_door_walk(struct bots_bot *bot, struct bots_tick *tick, long slot, real_point3d const *to,
	real_point3d const *face)
{
	real turn = 12.0f * BOTS_DEGREES, direction = 0.0f;
	int walk = ae_squad_walk_to(bot, slot, tick->player->unit_index, &tick->position, to, &direction);

	/* (the squad's own walk to the place: arrived within AE_SQUAD_ARRIVE, a failed path holds where it is) */
	if (walk == AE_SQUAD_WALK_GO)
	{
		ae_squad_walk(bot, tick, slot, direction);
		return FALSE;
	}
	tick->moving = FALSE;
	if (walk == AE_SQUAD_WALK_FAILED)
		return FALSE;
	bot->yaw += PIN(bots_angle_difference((real)atan2(face->y - tick->position.y, face->x - tick->position.x), bot->yaw), -turn, turn);
	return TRUE;
}

/* whether the grenade may go now: a clear line from the thrower's eye to the blast point (no wall, no body: the player or a mate
in the doorway), the player and the other members AE_SQUAD_GRENADE_SAFE or more from the blast */
static boolean ae_squad_door_throw_clear(struct bots_tick const *tick, real_point3d const *blast)
{
	struct collision_result collision;
	real_point3d eye = tick->position;
	real_vector3d line;
	long slot;

	eye.z += 0.6f;
	line.i = blast->x - eye.x;
	line.j = blast->y - eye.y;
	line.k = blast->z - eye.z;
	if (collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
			FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_bipeds_bit), &eye, &line, tick->player->unit_index,
			&collision) && collision.t < 0.95f)
		return FALSE;
	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
	{
		long unit;
		real_point3d at;

		if (!ae_squad_door_member_unit(slot, &unit) || unit == tick->player->unit_index)
			continue;
		object_get_origin(unit, &at);
		if (bots_distance3d(&at, blast) < AE_SQUAD_GRENADE_SAFE)
			return FALSE;
	}
	{
		long fireteam = ae_fireteam_of_brain(0), leader = fireteam != NONE ? ae_fireteam_leader_player(fireteam) : NONE;
		struct player_datum const *player = leader != NONE ? player_try_and_get(leader) : NULL;
		real_point3d at;

		if (!player || player->unit_index == NONE)
			return FALSE;
		object_get_origin(player->unit_index, &at);
		return (boolean)ae_bots_squad_entry_grenade(0, 1, (float)bots_distance3d(&at, blast), 1);
	}
}

/* (a member's tick) its part of the door: TRUE when the door moved it this tick */
static boolean ae_squad_door_member(struct bots_bot *bot, struct bots_tick *tick, long slot)
{
	int rank = -1, index, in_place = 0, members = 0;
	int opener = ae_squad_door_living(0), thrower = ae_squad_door_living(1);
	real_point3d spot, inward;

	if (ae_squad_door.state == AE_SQUAD_DOOR_NONE || tick->fighting)
		return FALSE;
	for (index = 0; index < ae_squad_door.count; index++)
		if (ae_squad_door.order[index] == slot)
			rank = index;
	/* (one on foot again after the stack began: last in the order, holding where it is) */
	if (rank < 0)
	{
		if (ae_squad_door.count >= AE_COMPANION_MOST)
			return FALSE;
		rank = ae_squad_door.count;
		ae_squad_door.order[ae_squad_door.count++] = (int)slot;
		ae_squad_door.placed[slot] = FALSE;
	}
	ae_squad_door_axis_point(-3.0f, &inward);
	switch (ae_squad_door.state)
	{
	case AE_SQUAD_DOOR_STACKING:
	case AE_SQUAD_DOOR_STACKED:
	{
		long unit;
		real farthest = 0.0f;

		/* (its place, else it holds where it is, facing the door) */
		spot = ae_squad_door.placed[slot] ? ae_squad_door.place[slot] : tick->position;
		(void)ae_squad_door_walk(bot, tick, slot, &spot, &ae_squad_door.centre);
		if (ae_squad_door.state == AE_SQUAD_DOOR_STACKING)
		{
			for (index = 0; index < ae_squad_door.count; index++)
			{
				int other = ae_squad_door.order[index];
				real_point3d at;

				if (!ae_squad_door_member_unit(other, &unit))
					continue;
				members++;
				object_get_origin(unit, &at);
				/* (in place: at a place found, AE_SQUAD_STACK_IN_PLACE or nearer; a member with no place is not) */
				if (ae_squad_door.placed[other])
				{
					farthest = MAX(farthest, bots_distance2d(&ae_squad_door.place[other], &ae_squad_door.stack_point));
					if (bots_distance2d(&at, &ae_squad_door.place[other]) <= AE_SQUAD_STACK_IN_PLACE)
						in_place++;
				}
			}
			if (!ae_bots_squad_stacked(in_place, members, (float)farthest) &&
				tick->now - ae_squad_door.since > AE_SQUAD_STACK_TIMEOUT_TICKS)
			{
				platform_log("ae squad: could not stack: %d of %d in place, farthest place %.1f from the stack point", in_place, members,
					(double)farthest);
				ae_squad_door_end("could not stack", TRUE);
				return FALSE;
			}
			if (ae_bots_squad_stacked(in_place, members, (float)farthest))
			{
				ae_squad_door.state = AE_SQUAD_DOOR_STACKED;
				platform_log("ae squad: stacked at the door (%d of %d in place, farthest place %.1f from the stack point)", in_place,
					members, (double)farthest);
				if (!ae_squad_door.said)
				{
					ae_squad_door.said = TRUE;
					ae_squad_door_say(L": stacked up");
				}
			}
		}
		return TRUE;
	}
	case AE_SQUAD_DOOR_GRENADE:
		/* (the first man on foot steps into the door's reach, aside of its axis, to open it; the second goes to the axis outside
		and throws once it is open and the line is clear, else the squad goes in without; the rest hold their places) */
		if (slot == opener)
		{
			real_point3d step;
			real side_x = -ae_squad_door.forward_y, side_y = ae_squad_door.forward_x;

			ae_squad_door_axis_point(ae_squad_door.radius * 0.5f, &step);
			step.x += side_x * 0.6f;
			step.y += side_y * 0.6f;
			if (bots_distance2d(&tick->position, &ae_squad_door.centre) < ae_squad_door.radius + AE_SQUAD_STACK_PAD &&
				!ae_squad_door.first_in_reach)
				ae_squad_door.first_in_reach = tick->now;
			(void)ae_squad_door_walk(bot, tick, slot, &step, &inward);
			return TRUE;
		}
		if (slot == thrower && !ae_squad_door.thrown)
		{
			real_point3d from, blast;

			ae_squad_door_axis_point(ae_squad_door.radius + 0.8f, &from);
			if (!ae_squad_door_walk(bot, tick, slot, &from, &inward) || ae_squad_door_position(ae_squad_door.door) <= 0.9f)
				return TRUE;
			ae_squad_door_blast(&blast);
			if (!ae_squad_door_throw_clear(tick, &blast))
			{
				ae_squad_door.state = AE_SQUAD_DOOR_ENTERING;
				ae_squad_door.since = tick->now;
				ae_squad_door_clear_list();
				platform_log("ae squad: no clear throw through the door: in without a grenade");
				return TRUE;
			}
			tick->moving = FALSE;
			bot->yaw = (real)atan2(blast.y - tick->position.y, blast.x - tick->position.x);
			bot->pitch = 0.1f;
			tick->flags |= FLAG(_unit_control_throw_grenade_bit);
			if (tick->unit->unit.grenade_counts[(long)tick->unit->unit.current_grenade_index] <= 0)
				tick->action->desired_grenade_index = tick->unit->unit.grenade_counts[0] > 0 ? 0 : 1;
			ae_squad_door.thrown = tick->now;
			ae_squad_door.grenades++;
			platform_log("ae squad: %ld throws a grenade through the door", slot + 1);
			return TRUE;
		}
		spot = ae_squad_door.placed[slot] ? ae_squad_door.place[slot] : tick->position;
		(void)ae_squad_door_walk(bot, tick, slot, &spot, &ae_squad_door.centre);
		return TRUE;
	case AE_SQUAD_DOOR_ENTERING:
	case AE_SQUAD_DOOR_IN:
	{
		real_point3d approach, through;
		int next;

		/* (on the squad's side: in turn, to the door's axis just outside, then through, calling the side it goes in from) */
		if (ae_bots_squad_door_side((float)ae_squad_door.forward_x, (float)ae_squad_door.forward_y,
				(float)(tick->position.x - ae_squad_door.centre.x), (float)(tick->position.y - ae_squad_door.centre.y)) ==
				ae_squad_door.side)
		{
			real lateral = (tick->position.x - ae_squad_door.centre.x) * -ae_squad_door.forward_y +
				(tick->position.y - ae_squad_door.centre.y) * ae_squad_door.forward_x;

			/* (the one before it in the order not yet through: it waits at its place) */
			if (rank > 0)
			{
				long before_unit;
				real_point3d before_at;

				if (ae_squad_door_member_unit(ae_squad_door.order[rank - 1], &before_unit))
				{
					object_get_origin(before_unit, &before_at);
					if (ae_bots_squad_door_side((float)ae_squad_door.forward_x, (float)ae_squad_door.forward_y,
							(float)(before_at.x - ae_squad_door.centre.x), (float)(before_at.y - ae_squad_door.centre.y)) ==
							ae_squad_door.side && bots_distance2d(&before_at, &ae_squad_door.centre) > 0.4f)
					{
						spot = ae_squad_door.placed[slot] ? ae_squad_door.place[slot] : tick->position;
						(void)ae_squad_door_walk(bot, tick, slot, &spot, &ae_squad_door.centre);
						return TRUE;
					}
				}
			}
			if (!ae_squad_door.side_said[slot])
			{
				struct player_datum const *player = player_try_and_get(bot->player_index);

				ae_squad_door.side_said[slot] = TRUE;
				if (player)
					ae_squad_say(player, lateral > 0.3f ? L": going in left" : lateral < -0.3f ? L": going in right" : L": going in centre");
			}
			if (slot == opener && !ae_squad_door.first_in_reach &&
				bots_distance2d(&tick->position, &ae_squad_door.centre) < ae_squad_door.radius + AE_SQUAD_STACK_PAD)
				ae_squad_door.first_in_reach = tick->now;
			ae_squad_door_axis_point(0.3f, &approach);
			ae_squad_door_axis_point(-0.8f, &through);
			approach.z = through.z = tick->position.z;
			if (bots_distance2d(&tick->position, &approach) > 0.4f && (real)fabs(lateral) > 0.25f)
				(void)ae_squad_door_walk(bot, tick, slot, &approach, &inward);
			else
				(void)ae_squad_door_walk(bot, tick, slot, &through, &inward);
			return TRUE;
		}
		/* (inside: the room's clear list, the nearest point not visited; a mate close ahead: it holds a moment, the stop point) */
		(void)ae_bots_squad_clear_visit((float const (*)[2])ae_squad_door.clear_points, ae_squad_door.clear_visited,
			ae_squad_door.clear_count, (float)tick->position.x, (float)tick->position.y);
		next = ae_bots_squad_clear_next((float const (*)[2])ae_squad_door.clear_points, ae_squad_door.clear_visited,
			ae_squad_door.clear_count, (float)tick->position.x, (float)tick->position.y);
		if (next < 0)
		{
			tick->moving = FALSE;
			return TRUE;
		}
		spot.x = ae_squad_door.clear_points[next][0];
		spot.y = ae_squad_door.clear_points[next][1];
		spot.z = tick->position.z;
		for (index = 0; index < ae_squad_door.count; index++)
		{
			long unit;
			real_point3d at;
			real way = (real)atan2(spot.y - tick->position.y, spot.x - tick->position.x);

			if (ae_squad_door.order[index] == slot || !ae_squad_door_member_unit(ae_squad_door.order[index], &unit))
				continue;
			object_get_origin(unit, &at);
			if (bots_distance2d(&at, &tick->position) < 0.5f &&
				(real)fabs(bots_angle_difference((real)atan2(at.y - tick->position.y, at.x - tick->position.x), way)) < 0.8f)
			{
				tick->moving = FALSE;
				return TRUE;
			}
		}
		(void)ae_squad_door_walk(bot, tick, slot, &spot, &spot);
		return TRUE;
	}
	default:
		return FALSE;
	}
}

/* DOOR KEEP-OUT (ae_bots_squad_door_keep_out): a member walking with the player, out of a door's work, steps out of a door's reach
+ AE_SQUAD_DOOR_KEEP_OUT unless the player went through that door (a body there holds it open, and stands in the player's way); the
nearest door within 3 units is looked up every half second */
static struct
{
	long door[AE_COMPANION_MOST];
	real reach[AE_COMPANION_MOST];
	long next[AE_COMPANION_MOST];
	long steps;
} ae_squad_keep_out;

static boolean ae_squad_door_keep_out(struct bots_tick *tick, long slot, real_point3d const *leader_at)
{
	struct object_datum const *object;
	real to_door, away;
	int member_side, player_side;

	if (ae_squad_door.state != AE_SQUAD_DOOR_NONE || tick->fighting)
		return FALSE;
	if (tick->now >= ae_squad_keep_out.next[slot] || tick->now < ae_squad_keep_out.next[slot] - TICKS_PER_SECOND)
	{
		long door = ae_squad_door_near(&tick->position, 3.0f, FALSE);

		ae_squad_keep_out.next[slot] = tick->now + TICKS_PER_SECOND / 2;
		ae_squad_keep_out.door[slot] = door;
		if (door != NONE)
		{
			struct machine_datum const *machine = (struct machine_datum const *)object_get(door);
			struct machine_definition const *definition = machine_definition_get(machine->definition_index);

			ae_squad_keep_out.reach[slot] = definition->device.automatic_activation_radius > _real_epsilon ?
				definition->device.automatic_activation_radius : machine->object.bounding_sphere_radius;
		}
	}
	if (ae_squad_keep_out.door[slot] == NONE || !(object = (struct object_datum const *)object_try_and_get(ae_squad_keep_out.door[slot])))
		return FALSE;
	to_door = bots_distance2d(&tick->position, &object->object.bounding_sphere_center);
	member_side = ae_bots_squad_door_side(object->object.forward.i, object->object.forward.j,
		(float)(tick->position.x - object->object.bounding_sphere_center.x), (float)(tick->position.y - object->object.bounding_sphere_center.y));
	player_side = ae_bots_squad_door_side(object->object.forward.i, object->object.forward.j,
		(float)(leader_at->x - object->object.bounding_sphere_center.x), (float)(leader_at->y - object->object.bounding_sphere_center.y));
	if (!ae_bots_squad_door_keep_out((float)to_door, (float)ae_squad_keep_out.reach[slot], member_side != player_side))
		return FALSE;
	/* (out along the door's facing on its own side, the way back from the doorway) */
	away = (real)atan2(object->object.forward.j * (real)member_side, object->object.forward.i * (real)member_side);
	tick->move_direction = away;
	tick->moving = TRUE;
	tick->throttle_scale = 1.0f;
	ae_squad_keep_out.steps++;
	return TRUE;
}

