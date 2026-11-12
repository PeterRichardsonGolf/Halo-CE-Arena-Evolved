/*
AE_BOTS_VEHICLE_RULES.C

The bots' seats, leaving and driving (ae_bots_vehicle_rules.h): pure numbers,
tuned by box games (the vehicle report's numbers).
*/

#include "ae_bots_vehicle_rules.h"

#include <ctype.h>
#include <string.h>

/* whether the name holds the word (letters' case aside) */
static int name_has(char const *name, char const *word)
{
	size_t length = strlen(word);

	for (; *name; name++)
	{
		size_t index;

		for (index = 0; index < length; index++)
		{
			if (!name[index] || tolower((unsigned char)name[index]) != word[index])
				break;
		}
		if (index == length)
			return 1;
	}
	return 0;
}

long ae_bots_vehicle_sight_ignore_of(long unit_index, long ultimate_parent_index)
{
	return ultimate_parent_index != -1 ? ultimate_parent_index : unit_index;
}

int ae_bots_vehicle_class_of_name(char const *tag_name)
{
	if (!tag_name)
		return AE_BOTS_VEHICLE_OTHER;
	if (name_has(tag_name, "turret"))
		return AE_BOTS_VEHICLE_TURRET;
	if (name_has(tag_name, "banshee"))
		return AE_BOTS_VEHICLE_BANSHEE;
	if (name_has(tag_name, "scorpion"))
		return AE_BOTS_VEHICLE_SCORPION;
	if (name_has(tag_name, "ghost"))
		return AE_BOTS_VEHICLE_GHOST;
	if (name_has(tag_name, "rwarthog") || name_has(tag_name, "rocket"))
		return AE_BOTS_VEHICLE_ROCKET_WARTHOG;
	if (name_has(tag_name, "warthog") || name_has(tag_name, "hog"))
		return AE_BOTS_VEHICLE_WARTHOG;
	return AE_BOTS_VEHICLE_OTHER;
}

int ae_bots_vehicle_seat_kind(int vehicle_class, int driver_flag, int gunner_flag)
{
	switch (vehicle_class)
	{
	case AE_BOTS_VEHICLE_BANSHEE:
		return AE_BOTS_SEAT_NONE;
	case AE_BOTS_VEHICLE_SCORPION:
		/* (its one gun is its driver's; the others ride on it) */
		return driver_flag ? AE_BOTS_SEAT_DRIVER : AE_BOTS_SEAT_PASSENGER;
	case AE_BOTS_VEHICLE_TURRET:
		return AE_BOTS_SEAT_GUNNER;
	default:
		break;
	}
	if (driver_flag)
		return AE_BOTS_SEAT_DRIVER;
	if (gunner_flag)
		return AE_BOTS_SEAT_GUNNER;
	return AE_BOTS_SEAT_PASSENGER;
}

int ae_bots_vehicle_hull_low(float body_vitality)
{
	return body_vitality < AE_BOTS_VEHICLE_HULL_LOW;
}

float ae_bots_vehicle_speed_scale(int vehicle_class, float top_speed_per_tick)
{
	float scale;

	if (vehicle_class != AE_BOTS_VEHICLE_GHOST || !(top_speed_per_tick > AE_BOTS_GHOST_STOCK_SPEED))
		return 1.0f;
	scale = top_speed_per_tick / AE_BOTS_GHOST_STOCK_SPEED;
	return scale > 2.0f ? 2.0f : scale;
}

int ae_bots_vehicle_hail_holds(float hailer_distance, int fighting, float waited)
{
	return !fighting && hailer_distance >= 0.0f && hailer_distance < AE_BOTS_VEHICLE_HAIL_NEAR && waited < AE_BOTS_VEHICLE_HAIL_WAIT_MAX;
}

float ae_bots_vehicle_seat_score(struct ae_bots_vehicle_situation const *situation, struct ae_bots_seat_offer const *offer)
{
	float near, notice;

	if (!situation || !offer || offer->seat <= AE_BOTS_SEAT_NONE || offer->seat >= AE_BOTS_SEAT_KINDS ||
		!offer->alive || !offer->upright || offer->enemy_aboard || offer->driver == AE_BOTS_DRIVER_ENEMY ||
		offer->reserved || offer->recently_left || offer->hull_low)
	{
		return 0.0f;
	}
	/* (out of notice, or too fast to board) */
	notice = offer->driver == AE_BOTS_DRIVER_TEAMMATE ? AE_BOTS_VEHICLE_NOTICE_DRIVEN :
		offer->seat == AE_BOTS_SEAT_DRIVER ? AE_BOTS_VEHICLE_NOTICE_DRIVE : AE_BOTS_VEHICLE_NOTICE;
	if (!(offer->distance >= 0.0f && offer->distance <= notice) || !(offer->speed <= AE_BOTS_VEHICLE_ENTER_SPEED))
		return 0.0f;
	/* (a defender, the hill's holder stay; a fight it is winning goes on) */
	if (situation->holds_place || (situation->fighting && situation->winning))
		return 0.0f;
	near = 1.0f - offer->distance / (notice * 2.0f);
	switch (offer->seat)
	{
	case AE_BOTS_SEAT_GUNNER:
	{
		int on_way;

		/* (a trigger pull drops the flag or the ball) */
		if (situation->carrying)
			return 0.0f;
		/* (a teammate's gun, unless its goal is at hand: it would only get out again, 5 of 6 gunners after 1.5 s, Blood Gulch 10-30) */
		if (offer->driver == AE_BOTS_DRIVER_TEAMMATE)
			return !situation->fighting && situation->goal_distance >= 0.0f && situation->goal_distance < 8.0f ? 0.0f : 0.9f * near;
		/* (on its way; or near while it loses a fight: a gun's seat, whatever its way) */
		on_way = situation->fighting || (offer->off_route >= 0.0f ? offer->off_route <= AE_BOTS_VEHICLE_OFF_ROUTE :
			offer->distance <= AE_BOTS_VEHICLE_NEAR_NO_GOAL);
		if (!on_way)
			return 0.0f;
		return (offer->vehicle_class == AE_BOTS_VEHICLE_TURRET ? 0.5f : 0.6f) * near;
	}
	case AE_BOTS_SEAT_PASSENGER:
		/* (a ride with a teammate, to a goal not close by) */
		if (offer->driver != AE_BOTS_DRIVER_TEAMMATE)
			return 0.0f;
		if (situation->goal_distance >= 0.0f && situation->goal_distance < 12.0f)
			return 0.0f;
		return (situation->carrying ? 0.95f : 0.7f) * near;
	case AE_BOTS_SEAT_DRIVER:
		if (situation->carrying || !situation->may_drive || offer->driver != AE_BOTS_DRIVER_NONE)
			return 0.0f;
		/* (a tank: its own valuation: a long objective trip over open ground, not a short or indoor one, below a light vehicle) */
		if (offer->vehicle_class == AE_BOTS_VEHICLE_SCORPION)
		{
			/* (0.7.8, owner: tanks in Slayer too: a roamer out of a fight takes a tank on open ground; the driver picks a far place
			itself, as with a light vehicle, at the tank's lower score) */
			if (situation->roams && !situation->fighting)
				return offer->open_route ? AE_BOTS_TANK_DRIVE_SCORE * near : 0.0f;
			if (situation->roams || situation->fighting || !offer->open_route ||
				!(situation->goal_distance >= AE_BOTS_TANK_DRIVE_GOAL) || !(offer->detour >= 0.0f) ||
				offer->detour > AE_BOTS_TANK_DRIVE_DETOUR * situation->goal_distance + 8.0f)
			{
				return 0.0f;
			}
			return AE_BOTS_TANK_DRIVE_SCORE * near;
		}
		if (offer->vehicle_class != AE_BOTS_VEHICLE_WARTHOG && offer->vehicle_class != AE_BOTS_VEHICLE_ROCKET_WARTHOG &&
			offer->vehicle_class != AE_BOTS_VEHICLE_GHOST)
		{
			return 0.0f;
		}
		/* (roaming, out of a fight: the wheel is its goal; the driver picks a far place itself) */
		if (situation->roams && !situation->fighting)
			return 0.65f * near;
		if (!(situation->goal_distance >= AE_BOTS_VEHICLE_DRIVE_GOAL) || !(offer->detour >= 0.0f) ||
			offer->detour > AE_BOTS_VEHICLE_DRIVE_DETOUR * situation->goal_distance + 8.0f)
		{
			return 0.0f;
		}
		return 0.65f * near;
	default:
		return 0.0f;
	}
}

int ae_bots_vehicle_leave(struct ae_bots_ride const *ride)
{
	if (!ride)
		return AE_BOTS_LEAVE_STAY;
	if (!ride->alive)
		return AE_BOTS_LEAVE_WRECKED;
	if (!ride->upright)
		return AE_BOTS_LEAVE_FLIPPED;
	/* (a moment in the seat first: no in and out) */
	if (!(ride->seated >= 1.5f))
		return AE_BOTS_LEAVE_STAY;
	if (ride->holds_place)
		return AE_BOTS_LEAVE_HOLD;
	if (ride->seat == AE_BOTS_SEAT_DRIVER && ride->person_aboard)
		return ride->dying && ride->parked >= AE_BOTS_VEHICLE_DYING_PARKED ? AE_BOTS_LEAVE_DYING : AE_BOTS_LEAVE_STAY;
	if (ride->stationary)
		return ride->seated >= 8.0f && ride->since_fight >= 8.0f ? AE_BOTS_LEAVE_PARKED : AE_BOTS_LEAVE_STAY;
	/* (a driver or gunner of a dying hull that stands: out under the guns, not a still target to its death (review 11-05: a
	driver sat in its Warthog until it died); a rider's own rule is below) */
	if (ride->dying && ride->parked >= AE_BOTS_VEHICLE_DYING_PARKED && ride->seat != AE_BOTS_SEAT_PASSENGER)
		return AE_BOTS_LEAVE_DYING;
	switch (ride->seat)
	{
	case AE_BOTS_SEAT_GUNNER:
		/* (no one at the wheel and losing the fight in the seat: out and on foot, not a still target) */
		if (ride->without_driver >= AE_BOTS_VEHICLE_EXPOSED_GUN && ride->under_fire)
			return AE_BOTS_LEAVE_NO_DRIVER;
		/* (owner 10-25, UT spike: "gunners shouldn't ditch so fast if the driver is slightly idle unless it is for a
		flag run all together": 12 s with no driver, and with a driver at the wheel a 45 s stop with no fight for 10 s;
		an objective's goal reached still lets them all out together, below) */
		if (!ride->patient_gunner)
		{
			if (ride->without_driver >= 8.0f && ride->since_fight >= 3.0f)
				return AE_BOTS_LEAVE_NO_DRIVER;
			break;
		}
		if (ride->without_driver >= 12.0f && ride->since_fight >= 3.0f)
			return AE_BOTS_LEAVE_NO_DRIVER;
		if (ride->without_driver < 1.0f)
		{
			if (ride->parked >= 45.0f && ride->since_fight >= 10.0f)
				return AE_BOTS_LEAVE_PARKED;
			if (ride->goal_distance >= 0.0f && ride->goal_distance < 8.0f)
				return AE_BOTS_LEAVE_THERE;
			return AE_BOTS_LEAVE_STAY;
		}
		break;
	case AE_BOTS_SEAT_PASSENGER:
		/* (out with the driver) */
		if (ride->without_driver >= 0.3f)
			return AE_BOTS_LEAVE_NO_DRIVER;
		/* (a rider has no gun: the hull nearly gone, or an enemy close on foot, it hops off and fights on foot) */
		if (ride->dying)
			return AE_BOTS_LEAVE_DYING;
		if (ride->foe_near)
			return AE_BOTS_LEAVE_FOE;
		if (ride->parked >= (ride->tank ? 2.5f : 4.0f))
			return AE_BOTS_LEAVE_PARKED;
		/* (a tank's rider hops off a little before the goal: the tank stops short of it) */
		if (ride->tank && ride->goal_distance >= 0.0f && ride->goal_distance < 14.0f)
			return AE_BOTS_LEAVE_THERE;
		break;
	default:
		break;
	}
	/* ('exit an inactive vehicle': 15 seconds standing) */
	if (ride->parked >= 15.0f && ride->since_fight >= 2.0f)
		return AE_BOTS_LEAVE_PARKED;
	if (ride->goal_distance >= 0.0f)
	{
		if (ride->goal_distance < 8.0f)
			return AE_BOTS_LEAVE_THERE;
		/* (a driver's way bends round rocks and backs off walls: 12 of 15 drivers got out on the 3 s a passenger had, Blood Gulch 10-30) */
		if (ride->goal_distance > 15.0f && ride->going_away >= (ride->seat == AE_BOTS_SEAT_DRIVER ? 12.0f : 6.0f))
			return AE_BOTS_LEAVE_AWAY;
	}
	return AE_BOTS_LEAVE_STAY;
}

char const *ae_bots_vehicle_leave_name(int reason)
{
	static char const *const names[AE_BOTS_LEAVE_REASONS] =
	{
		"stays", "wrecked", "flipped", "no driver", "parked", "there", "away from its goal", "holds a place", "stuck", "enemy close", "about to die",
	};

	return reason >= 0 && reason < AE_BOTS_LEAVE_REASONS ? names[reason] : "?";
}

char const *ae_bots_vehicle_seat_name(int seat)
{
	static char const *const names[AE_BOTS_SEAT_KINDS] = { "no", "driver", "gunner", "passenger" };

	return seat >= 0 && seat < AE_BOTS_SEAT_KINDS ? names[seat] : "?";
}

char const *ae_bots_vehicle_class_name(int vehicle_class)
{
	static char const *const names[AE_BOTS_VEHICLE_CLASSES] =
	{
		"vehicle", "warthog", "rocket warthog", "ghost", "scorpion", "banshee", "gun turret",
	};

	return vehicle_class >= 0 && vehicle_class < AE_BOTS_VEHICLE_CLASSES ? names[vehicle_class] : "?";
}

void ae_bots_tank_guns(float distance, int *cannon, int *chain_gun)
{
	*cannon = distance >= AE_BOTS_TANK_SHELL_LEAST && distance <= AE_BOTS_TANK_SHELL_MOST;
	*chain_gun = distance <= AE_BOTS_TANK_CHAIN_MOST;
}

float ae_bots_tank_fight_throttle(float distance, float off_hull)
{
	float off = off_hull < 0.0f ? -off_hull : off_hull;

	/* (too near for the shell: back away, the hull still turning to the aim) */
	if (distance < AE_BOTS_TANK_BACK_OFF)
		return -AE_BOTS_TANK_CREEP;
	/* (past the shell's reach: on towards it; turned away: stand, the hull only turns on the move, the turret does not need it) */
	if (off > AE_BOTS_TANK_CREEP_OFF_HULL)
		return 0.0f;
	if (distance > AE_BOTS_TANK_SHELL_MOST)
		return 1.0f;
	return distance > AE_BOTS_TANK_HOLD ? AE_BOTS_TANK_CREEP : 0.0f;
}

void ae_bots_vehicle_drive(struct ae_bots_drive_input const *input, struct ae_bots_drive_output *output)
{
	float error, scale, speed, waypoint_distance;

	output->throttle = 0.0f;
	output->steer = 0.0f;
	output->reverse_for = 0.0f;
	if (!input)
		return;
	error = input->heading_error < 0.0f ? -input->heading_error : input->heading_error;
	scale = input->speed_scale >= 1.0f ? input->speed_scale : 1.0f;
	speed = input->speed / scale;
	waypoint_distance = input->waypoint_distance / scale;
	/* (backing out: straight back, the heading kept, then on again) */
	if (input->reversing > 0.0f)
	{
		output->throttle = -1.0f;
		output->steer = -input->heading_error;
		return;
	}
	/* (stopped against something with the throttle open: back off a moment) */
	if (input->stalled >= 1.5f)
	{
		output->throttle = -1.0f;
		output->steer = -input->heading_error;
		output->reverse_for = 1.5f;
		return;
	}
	/* (a turn: a little slower the more it must turn; a Warthog turns only while it rolls) */
	output->throttle = error < 0.5f ? 1.0f : error < 1.2f ? 0.8f : 0.6f;
	if (waypoint_distance < 5.0f && (input->next_turn > 1.0f || input->next_turn < -1.0f))
	{
		if (speed > 8.0f)
			output->throttle = -0.3f;
		else if (speed > 5.0f && output->throttle > 0.2f)
			output->throttle = 0.2f;
	}
}

int ae_bots_vehicle_map_is_open(char const *map_name)
{
	static char const *const open_maps[] = { "bloodgulch", "sidewinder", "icefields", "infinity", "deathisland" };
	size_t index;

	if (!map_name)
		return 0;
	for (index = 0; index < sizeof(open_maps) / sizeof(open_maps[0]); index++)
	{
		if (strstr(map_name, open_maps[index]))
			return 1;
	}
	return 0;
}

int ae_bots_vehicle_may_drive(long setting, char const *map_name)
{
	return setting >= 2 || (setting == 1 && ae_bots_vehicle_map_is_open(map_name));
}

int ae_bots_vehicle_called_in_turn(int nearer_free_bots, int open_seats)
{
	return nearer_free_bots < open_seats;
}
