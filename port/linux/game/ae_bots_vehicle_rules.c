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
	case AE_BOTS_VEHICLE_SCORPION:
		return AE_BOTS_SEAT_NONE;
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

float ae_bots_vehicle_seat_score(struct ae_bots_vehicle_situation const *situation, struct ae_bots_seat_offer const *offer)
{
	float near, notice;

	if (!situation || !offer || offer->seat <= AE_BOTS_SEAT_NONE || offer->seat >= AE_BOTS_SEAT_KINDS ||
		!offer->alive || !offer->upright || offer->enemy_aboard || offer->driver == AE_BOTS_DRIVER_ENEMY ||
		offer->reserved || offer->recently_left)
	{
		return 0.0f;
	}
	/* (out of notice, or too fast to board: as Infinite's bots) */
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
		if (offer->driver == AE_BOTS_DRIVER_TEAMMATE)
			return 0.9f * near;
		on_way = offer->off_route >= 0.0f ? offer->off_route <= AE_BOTS_VEHICLE_OFF_ROUTE :
			offer->distance <= AE_BOTS_VEHICLE_NEAR_NO_GOAL;
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
		if (offer->vehicle_class != AE_BOTS_VEHICLE_WARTHOG && offer->vehicle_class != AE_BOTS_VEHICLE_ROCKET_WARTHOG &&
			offer->vehicle_class != AE_BOTS_VEHICLE_GHOST)
		{
			return 0.0f;
		}
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
	if (ride->stationary)
		return ride->seated >= 8.0f && ride->since_fight >= 8.0f ? AE_BOTS_LEAVE_PARKED : AE_BOTS_LEAVE_STAY;
	switch (ride->seat)
	{
	case AE_BOTS_SEAT_GUNNER:
		if (ride->without_driver >= 8.0f && ride->since_fight >= 3.0f)
			return AE_BOTS_LEAVE_NO_DRIVER;
		break;
	case AE_BOTS_SEAT_PASSENGER:
		if (ride->without_driver >= 2.0f)
			return AE_BOTS_LEAVE_NO_DRIVER;
		if (ride->parked >= 4.0f)
			return AE_BOTS_LEAVE_PARKED;
		break;
	default:
		break;
	}
	/* (Infinite's 'exit an inactive vehicle': 15 seconds standing) */
	if (ride->parked >= 15.0f && ride->since_fight >= 2.0f)
		return AE_BOTS_LEAVE_PARKED;
	if (ride->goal_distance >= 0.0f)
	{
		if (ride->goal_distance < 8.0f)
			return AE_BOTS_LEAVE_THERE;
		if (ride->goal_distance > 15.0f && ride->going_away >= 3.0f)
			return AE_BOTS_LEAVE_AWAY;
	}
	return AE_BOTS_LEAVE_STAY;
}

char const *ae_bots_vehicle_leave_name(int reason)
{
	static char const *const names[AE_BOTS_LEAVE_REASONS] =
	{
		"stays", "wrecked", "flipped", "no driver", "parked", "there", "away from its goal", "holds a place", "stuck",
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

void ae_bots_vehicle_drive(struct ae_bots_drive_input const *input, struct ae_bots_drive_output *output)
{
	float error;

	output->throttle = 0.0f;
	output->steer = 0.0f;
	output->reverse_for = 0.0f;
	if (!input)
		return;
	error = input->heading_error < 0.0f ? -input->heading_error : input->heading_error;
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
	if (input->waypoint_distance < 5.0f && (input->next_turn > 1.0f || input->next_turn < -1.0f))
	{
		if (input->speed > 8.0f)
			output->throttle = -0.3f;
		else if (input->speed > 5.0f && output->throttle > 0.2f)
			output->throttle = 0.2f;
	}
}
