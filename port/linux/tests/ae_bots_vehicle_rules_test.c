#include <stdio.h>
#include <string.h>
#include "ae_bots_vehicle_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* a Warthog's seat 4 units off, on the bot's way, standing, no one in it */
static struct ae_bots_seat_offer offer(int seat)
{
	struct ae_bots_seat_offer o;

	memset(&o, 0, sizeof(o));
	o.vehicle_class = AE_BOTS_VEHICLE_WARTHOG;
	o.seat = seat;
	o.distance = 4.0f;
	o.off_route = 1.0f;
	o.detour = 2.0f;
	o.speed = 0.0f;
	o.alive = 1;
	o.upright = 1;
	o.driver = AE_BOTS_DRIVER_NONE;
	return o;
}

/* a bot on foot going 40 units off, no fight */
static struct ae_bots_vehicle_situation situation(void)
{
	struct ae_bots_vehicle_situation s;

	memset(&s, 0, sizeof(s));
	s.goal_distance = 40.0f;
	return s;
}

/* a gunner 5 seconds in a driven Warthog, its goal far, no reason to go */
static struct ae_bots_ride ride(int seat)
{
	struct ae_bots_ride r;

	memset(&r, 0, sizeof(r));
	r.seat = seat;
	r.alive = 1;
	r.upright = 1;
	r.seated = 5.0f;
	r.since_fight = 30.0f;
	r.goal_distance = 40.0f;
	r.patient_gunner = 1;
	return r;
}

static void test_names(void)
{
	CHECK(ae_bots_vehicle_class_of_name("vehicles\\warthog\\mp_warthog") == AE_BOTS_VEHICLE_WARTHOG);
	CHECK(ae_bots_vehicle_class_of_name("vehicles\\rwarthog\\rwarthog") == AE_BOTS_VEHICLE_ROCKET_WARTHOG);
	CHECK(ae_bots_vehicle_class_of_name("vehicles\\ghost\\ghost_mp") == AE_BOTS_VEHICLE_GHOST);
	CHECK(ae_bots_vehicle_class_of_name("vehicles\\scorpion\\scorpion_mp") == AE_BOTS_VEHICLE_SCORPION);
	CHECK(ae_bots_vehicle_class_of_name("vehicles\\banshee\\banshee_mp") == AE_BOTS_VEHICLE_BANSHEE);
	CHECK(ae_bots_vehicle_class_of_name("vehicles\\c gun turret\\c gun turret_mp") == AE_BOTS_VEHICLE_TURRET);
	CHECK(ae_bots_vehicle_class_of_name("Vehicles\\WARTHOG\\Mp_Warthog") == AE_BOTS_VEHICLE_WARTHOG);
	CHECK(ae_bots_vehicle_class_of_name("vehicles\\pelican\\pelican") == AE_BOTS_VEHICLE_OTHER);
	CHECK(ae_bots_vehicle_class_of_name("") == AE_BOTS_VEHICLE_OTHER);
	CHECK(ae_bots_vehicle_class_of_name(NULL) == AE_BOTS_VEHICLE_OTHER);
	CHECK(!strcmp(ae_bots_vehicle_class_name(AE_BOTS_VEHICLE_TURRET), "gun turret"));
	CHECK(!strcmp(ae_bots_vehicle_class_name(99), "?") && !strcmp(ae_bots_vehicle_class_name(-1), "?"));
	CHECK(!strcmp(ae_bots_vehicle_seat_name(AE_BOTS_SEAT_GUNNER), "gunner"));
	CHECK(!strcmp(ae_bots_vehicle_leave_name(AE_BOTS_LEAVE_NO_DRIVER), "no driver"));
	CHECK(!strcmp(ae_bots_vehicle_leave_name(AE_BOTS_LEAVE_REASONS), "?"));
}

static void test_seat_kinds(void)
{
	/* (the Warthog's three seats: driver, gunner, passenger) */
	CHECK(ae_bots_vehicle_seat_kind(AE_BOTS_VEHICLE_WARTHOG, 1, 0) == AE_BOTS_SEAT_DRIVER);
	CHECK(ae_bots_vehicle_seat_kind(AE_BOTS_VEHICLE_WARTHOG, 0, 1) == AE_BOTS_SEAT_GUNNER);
	CHECK(ae_bots_vehicle_seat_kind(AE_BOTS_VEHICLE_WARTHOG, 0, 0) == AE_BOTS_SEAT_PASSENGER);
	/* (the Ghost's one seat drives and shoots: the driver's) */
	CHECK(ae_bots_vehicle_seat_kind(AE_BOTS_VEHICLE_GHOST, 1, 1) == AE_BOTS_SEAT_DRIVER);
	/* (a turret's seat, whatever its flags, is a gunner's) */
	CHECK(ae_bots_vehicle_seat_kind(AE_BOTS_VEHICLE_TURRET, 1, 1) == AE_BOTS_SEAT_GUNNER);
	CHECK(ae_bots_vehicle_seat_kind(AE_BOTS_VEHICLE_TURRET, 0, 0) == AE_BOTS_SEAT_GUNNER);
	/* (no flying, no tank) */
	CHECK(ae_bots_vehicle_seat_kind(AE_BOTS_VEHICLE_BANSHEE, 1, 1) == AE_BOTS_SEAT_NONE);
	CHECK(ae_bots_vehicle_seat_kind(AE_BOTS_VEHICLE_SCORPION, 0, 0) == AE_BOTS_SEAT_NONE);
	CHECK(ae_bots_vehicle_seat_kind(AE_BOTS_VEHICLE_SCORPION, 1, 1) == AE_BOTS_SEAT_NONE);
}

static void test_gunner(void)
{
	struct ae_bots_vehicle_situation s = situation();
	struct ae_bots_seat_offer o = offer(AE_BOTS_SEAT_GUNNER);
	float base = ae_bots_vehicle_seat_score(&s, &o), score;

	/* a free gunner seat near the bot's way: wanted */
	CHECK(base > 0.0f && base <= 1.0f);
	/* off its way: not */
	o.off_route = 4.5f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	/* ... unless it is losing a fight (a gun's seat near) */
	s.fighting = 1;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	s.winning = 1;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	s = situation();
	/* ... but with a teammate at the wheel, yes, and more */
	o.driver = AE_BOTS_DRIVER_TEAMMATE;
	score = ae_bots_vehicle_seat_score(&s, &o);
	CHECK(score > base);
	/* no goal: only close */
	o = offer(AE_BOTS_SEAT_GUNNER);
	o.off_route = -1.0f;
	o.distance = 5.9f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	o.distance = 6.1f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	/* nearer is better */
	o = offer(AE_BOTS_SEAT_GUNNER);
	o.distance = 2.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > base);
	/* out of notice (Modern games: 10 units; a teammate's vehicle 15) */
	o.distance = 10.5f;
	o.off_route = 0.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o.distance = 10.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	o.distance = 15.5f;
	o.driver = AE_BOTS_DRIVER_TEAMMATE;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o.distance = 14.5f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	/* a turret's seat: wanted a little less than a Warthog's */
	o = offer(AE_BOTS_SEAT_GUNNER);
	o.vehicle_class = AE_BOTS_VEHICLE_TURRET;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f && ae_bots_vehicle_seat_score(&s, &o) < base);
	/* a carrier never takes a gunner's seat (a trigger pull drops the flag or ball), even with a teammate driving */
	o = offer(AE_BOTS_SEAT_GUNNER);
	s.carrying = 1;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o.driver = AE_BOTS_DRIVER_TEAMMATE;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
}

static void test_refusals(void)
{
	struct ae_bots_vehicle_situation s = situation();
	struct ae_bots_seat_offer o;
	int seat;

	for (seat = AE_BOTS_SEAT_DRIVER; seat < AE_BOTS_SEAT_KINDS; seat++)
	{
		struct ae_bots_vehicle_situation d = s;

		d.may_drive = 1;
		o = offer(seat);
		if (seat == AE_BOTS_SEAT_PASSENGER)
			o.driver = AE_BOTS_DRIVER_TEAMMATE;
		CHECK(ae_bots_vehicle_seat_score(&d, &o) > 0.0f);
		o.alive = 0;
		CHECK(ae_bots_vehicle_seat_score(&d, &o) == 0.0f);
		o.alive = 1;
		o.upright = 0;
		CHECK(ae_bots_vehicle_seat_score(&d, &o) == 0.0f);
		o.upright = 1;
		o.enemy_aboard = 1;
		CHECK(ae_bots_vehicle_seat_score(&d, &o) == 0.0f);
		o.enemy_aboard = 0;
		o.reserved = 1;
		CHECK(ae_bots_vehicle_seat_score(&d, &o) == 0.0f);
		o.reserved = 0;
		o.recently_left = 1;
		CHECK(ae_bots_vehicle_seat_score(&d, &o) == 0.0f);
		o.recently_left = 0;
		/* (moving faster than 2 a second) */
		o.speed = 2.1f;
		CHECK(ae_bots_vehicle_seat_score(&d, &o) == 0.0f);
		o.speed = 1.9f;
		CHECK(ae_bots_vehicle_seat_score(&d, &o) > 0.0f);
		o.speed = 0.0f;
		/* (a defender, the hill's holder stay where they are) */
		d.holds_place = 1;
		CHECK(ae_bots_vehicle_seat_score(&d, &o) == 0.0f);
		d.holds_place = 0;
		/* (a fight it is winning goes on; one it is losing may run for a seat) */
		d.fighting = 1;
		d.winning = 1;
		CHECK(ae_bots_vehicle_seat_score(&d, &o) == 0.0f);
		d.winning = 0;
		CHECK(ae_bots_vehicle_seat_score(&d, &o) > 0.0f);
		/* (an enemy's vehicle, never) */
		o.driver = AE_BOTS_DRIVER_ENEMY;
		CHECK(ae_bots_vehicle_seat_score(&d, &o) == 0.0f);
		/* (no seat, a seat out of range) */
		o = offer(AE_BOTS_SEAT_NONE);
		CHECK(ae_bots_vehicle_seat_score(&d, &o) == 0.0f);
		o.seat = AE_BOTS_SEAT_KINDS;
		CHECK(ae_bots_vehicle_seat_score(&d, &o) == 0.0f);
		o = offer(seat);
		o.distance = -1.0f;
		CHECK(ae_bots_vehicle_seat_score(&d, &o) == 0.0f);
	}
	CHECK(ae_bots_vehicle_seat_score(NULL, &o) == 0.0f);
	CHECK(ae_bots_vehicle_seat_score(&s, NULL) == 0.0f);
}

static void test_passenger(void)
{
	struct ae_bots_vehicle_situation s = situation();
	struct ae_bots_seat_offer o = offer(AE_BOTS_SEAT_PASSENGER);

	/* no driver: a passenger's seat goes nowhere */
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o.driver = AE_BOTS_DRIVER_TEAMMATE;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	/* off its way does not matter: the driver goes */
	o.off_route = 9.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	/* a goal close by: walked */
	s.goal_distance = 11.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	s.goal_distance = -1.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	/* a carrier rides with a teammate (home, no trigger), and wants it more than another bot does */
	s = situation();
	{
		float other = ae_bots_vehicle_seat_score(&s, &o);

		s.carrying = 1;
		CHECK(ae_bots_vehicle_seat_score(&s, &o) > other);
	}
	/* a teammate at the wheel: gunner before passenger */
	s = situation();
	{
		struct ae_bots_seat_offer g = offer(AE_BOTS_SEAT_GUNNER);

		g.driver = AE_BOTS_DRIVER_TEAMMATE;
		CHECK(ae_bots_vehicle_seat_score(&s, &g) > ae_bots_vehicle_seat_score(&s, &o));
	}
}

static void test_driver(void)
{
	struct ae_bots_vehicle_situation s = situation();
	struct ae_bots_seat_offer o = offer(AE_BOTS_SEAT_DRIVER);

	/* only with the driving spike on */
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	s.may_drive = 1;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	/* a goal near: walked */
	s.goal_distance = 24.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	s.goal_distance = -1.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	s.goal_distance = 40.0f;
	/* a detour of at most half its way and 8 units (40 off: 28) */
	o.detour = 27.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	o.detour = 29.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o.detour = -1.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o.detour = 2.0f;
	/* (within 18 for the spike's driver) */
	o.distance = 17.5f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	o.distance = 18.5f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o.distance = 4.0f;
	/* a Ghost, yes; a turret, a tank, a Banshee, someone's: no */
	o.vehicle_class = AE_BOTS_VEHICLE_GHOST;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	o.vehicle_class = AE_BOTS_VEHICLE_ROCKET_WARTHOG;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	o.vehicle_class = AE_BOTS_VEHICLE_SCORPION;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o.vehicle_class = AE_BOTS_VEHICLE_OTHER;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o.vehicle_class = AE_BOTS_VEHICLE_WARTHOG;
	o.driver = AE_BOTS_DRIVER_TEAMMATE;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o.driver = AE_BOTS_DRIVER_NONE;
	/* a carrier never drives */
	s.carrying = 1;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
}

static void test_leave(void)
{
	struct ae_bots_ride r = ride(AE_BOTS_SEAT_GUNNER);

	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	CHECK(ae_bots_vehicle_leave(NULL) == AE_BOTS_LEAVE_STAY);
	/* wrecked, flipped: at once */
	r.seated = 0.1f;
	r.alive = 0;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_WRECKED);
	r.alive = 1;
	r.upright = 0;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_FLIPPED);
	r.upright = 1;
	/* anything else waits 1.5 seconds in the seat */
	r.holds_place = 1;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	r.seated = 1.6f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_HOLD);
	r = ride(AE_BOTS_SEAT_GUNNER);
	/* a gunner with no driver: 12 seconds, no fight in 3 (owner 10-25: "shouldn't ditch so fast") */
	r.without_driver = 11.9f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	r.without_driver = 12.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_NO_DRIVER);
	r.since_fight = 1.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	/* the spike off (HALO_BOTS_UT=0): the gunner's times before it: 8 s with no driver, 15 s standing */
	r = ride(AE_BOTS_SEAT_GUNNER);
	r.patient_gunner = 0;
	r.without_driver = 8.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_NO_DRIVER);
	r.without_driver = 0.0f;
	r.parked = 15.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_PARKED);
	/* a gunner with a driver at the wheel: a 45 s stop with no fight for 10 (not 15 s) */
	r = ride(AE_BOTS_SEAT_GUNNER);
	r.parked = 15.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	r.parked = 45.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_PARKED);
	r.since_fight = 9.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	/* ... out at the objective's goal with the rest */
	r = ride(AE_BOTS_SEAT_GUNNER);
	r.goal_distance = 7.9f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_THERE);
	/* a passenger: a vehicle standing 15 seconds (an inactive vehicle), the fight over */
	r = ride(AE_BOTS_SEAT_PASSENGER);
	r.parked = 15.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_PARKED);
	/* a passenger: no driver 2 seconds, standing 4 */
	r = ride(AE_BOTS_SEAT_PASSENGER);
	r.without_driver = 2.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_NO_DRIVER);
	r.without_driver = 0.0f;
	r.parked = 4.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_PARKED);
	/* near its goal; going away from it, far */
	r = ride(AE_BOTS_SEAT_PASSENGER);
	r.goal_distance = 7.9f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_THERE);
	r.goal_distance = 16.0f;
	r.going_away = 5.9f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	r.going_away = 6.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_AWAY);
	r.goal_distance = 14.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	/* a driver's way bends: longer before it gives up */
	r = ride(AE_BOTS_SEAT_DRIVER);
	r.goal_distance = 16.0f;
	r.going_away = 11.9f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	r.going_away = 12.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_AWAY);
	/* Slayer's gunner (no goal) stays wherever it is driven */
	r = ride(AE_BOTS_SEAT_GUNNER);
	r.goal_distance = -1.0f;
	r.going_away = 60.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	/* a turret: while it fights; 8 seconds after its last fight; never 'no driver' */
	r = ride(AE_BOTS_SEAT_GUNNER);
	r.stationary = 1;
	r.goal_distance = -1.0f;
	r.without_driver = 60.0f;
	r.parked = 60.0f;
	r.seated = 60.0f;
	r.since_fight = 7.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	r.since_fight = 8.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_PARKED);
	r.seated = 7.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	r.seated = 60.0f;
	r.alive = 0;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_WRECKED);
}

/* no bot stays forever in a parked vehicle: every seat, every goal, a long enough stand gets it out */
static void test_never_forever(void)
{
	int seat, flags;

	for (seat = AE_BOTS_SEAT_DRIVER; seat < AE_BOTS_SEAT_KINDS; seat++)
	{
		for (flags = 0; flags < 8; flags++)
		{
			struct ae_bots_ride r = ride(seat);

			r.stationary = flags & 1;
			r.goal_distance = flags & 2 ? -1.0f : 10.0f;
			r.without_driver = flags & 4 ? 0.0f : 600.0f;
			r.parked = 600.0f;
			r.seated = 600.0f;
			r.since_fight = 600.0f;
			CHECK(ae_bots_vehicle_leave(&r) != AE_BOTS_LEAVE_STAY);
		}
	}
}

static void test_drive(void)
{
	struct ae_bots_drive_input in;
	struct ae_bots_drive_output out;

	memset(&in, 0, sizeof(in));
	in.waypoint_distance = 20.0f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle == 1.0f && out.steer == 0.0f && out.reverse_for == 0.0f);
	/* a turn: slower */
	in.heading_error = 0.8f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle > 0.0f && out.throttle < 1.0f);
	in.heading_error = -2.5f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle > 0.0f && out.throttle <= 0.6f);
	/* a sharp corner close ahead, fast: brake; a little less fast: ease off */
	in.heading_error = 0.0f;
	in.waypoint_distance = 3.0f;
	in.next_turn = 1.5f;
	in.speed = 9.0f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle < 0.0f);
	in.speed = 6.0f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle > 0.0f && out.throttle <= 0.2f);
	in.next_turn = -1.5f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle > 0.0f && out.throttle <= 0.2f);
	in.speed = 2.0f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle == 1.0f);
	/* stalled against a wall: back off straight, keeping the heading */
	memset(&in, 0, sizeof(in));
	in.waypoint_distance = 20.0f;
	in.heading_error = 0.4f;
	in.stalled = 1.5f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle < 0.0f && out.reverse_for > 0.0f && out.steer == -0.4f);
	in.stalled = 0.0f;
	in.reversing = 0.7f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle < 0.0f && out.reverse_for == 0.0f);
	ae_bots_vehicle_drive(NULL, &out);
	CHECK(out.throttle == 0.0f);
}

/* every score 0 to 1, whatever the input */
static void test_range(void)
{
	static float const distances[] = { -5.0f, 0.0f, 3.0f, 9.9f, 10.0f, 50.0f, 1e9f };
	static float const speeds[] = { -3.0f, 0.0f, 1.0f, 2.0f, 30.0f };
	int seat, d, v, flags, cls;

	for (cls = 0; cls < AE_BOTS_VEHICLE_CLASSES; cls++)
	for (seat = 0; seat <= AE_BOTS_SEAT_KINDS; seat++)
	for (d = 0; d < (int)(sizeof(distances) / sizeof(distances[0])); d++)
	for (v = 0; v < (int)(sizeof(speeds) / sizeof(speeds[0])); v++)
	for (flags = 0; flags < 128; flags++)
	{
		struct ae_bots_vehicle_situation s = situation();
		struct ae_bots_seat_offer o = offer(seat);
		float score;

		o.vehicle_class = cls;
		o.distance = distances[d];
		o.off_route = distances[(d + flags) % 7];
		o.detour = distances[(d + flags / 2) % 7];
		o.speed = speeds[v];
		o.driver = flags % 3;
		s.fighting = (flags >> 2) & 1;
		s.winning = (flags >> 3) & 1;
		s.carrying = (flags >> 4) & 1;
		s.may_drive = (flags >> 5) & 1;
		s.goal_distance = distances[(d + flags / 3) % 7];
		score = ae_bots_vehicle_seat_score(&s, &o);
		CHECK(score >= 0.0f && score <= 1.0f);
	}
}

/* a rider's line of sight starts outside the vehicle it rides; a unit on foot ignores only itself */
static void test_sight_ignore(void)
{
	CHECK(ae_bots_vehicle_sight_ignore_of(0x10002L, -1) == 0x10002L);
	CHECK(ae_bots_vehicle_sight_ignore_of(0x10002L, 0x20005L) == 0x20005L);
	CHECK(ae_bots_vehicle_sight_ignore_of(-1, -1) == -1);
}

int main(void)
{
	test_names();
	test_seat_kinds();
	test_gunner();
	test_refusals();
	test_passenger();
	test_driver();
	test_leave();
	test_never_forever();
	test_drive();
	test_range();
	test_sight_ignore();
	/* the nearest answer a honk */
	CHECK(ae_bots_vehicle_called_in_turn(0, 1));
	CHECK(ae_bots_vehicle_called_in_turn(1, 2));
	CHECK(!ae_bots_vehicle_called_in_turn(2, 2));
	CHECK(!ae_bots_vehicle_called_in_turn(0, 0));

	/* driving by the setting and the map */
	CHECK(ae_bots_vehicle_may_drive(1, "levels\\test\\bloodgulch\\bloodgulch"));
	CHECK(ae_bots_vehicle_may_drive(1, "icefields"));
	CHECK(!ae_bots_vehicle_may_drive(1, "gephyrophobia"));
	CHECK(!ae_bots_vehicle_may_drive(1, NULL));
	CHECK(ae_bots_vehicle_may_drive(2, "gephyrophobia"));
	CHECK(!ae_bots_vehicle_may_drive(0, "bloodgulch"));

	printf("ae_bots_vehicle_rules_test: %d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
