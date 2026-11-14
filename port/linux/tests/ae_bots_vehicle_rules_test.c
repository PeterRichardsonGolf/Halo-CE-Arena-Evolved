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
	/* a Scorpion: its driver has the guns, the others ride */
	CHECK(ae_bots_vehicle_seat_kind(AE_BOTS_VEHICLE_SCORPION, 1, 1) == AE_BOTS_SEAT_DRIVER);
	CHECK(ae_bots_vehicle_seat_kind(AE_BOTS_VEHICLE_SCORPION, 0, 0) == AE_BOTS_SEAT_PASSENGER);
	CHECK(ae_bots_vehicle_seat_kind(AE_BOTS_VEHICLE_BANSHEE, 1, 1) == AE_BOTS_SEAT_NONE);
	{
		int cannon, chain_gun;

		ae_bots_tank_guns(3.0f, &cannon, &chain_gun);
		CHECK(!cannon && chain_gun);
		ae_bots_tank_guns(15.0f, &cannon, &chain_gun);
		CHECK(cannon && chain_gun);
		ae_bots_tank_guns(40.0f, &cannon, &chain_gun);
		CHECK(cannon && !chain_gun);
		ae_bots_tank_guns(90.0f, &cannon, &chain_gun);
		CHECK(!cannon && !chain_gun);
	}
	CHECK(ae_bots_tank_fight_throttle(2.0f, 0.0f) < 0.0f);
	CHECK(ae_bots_tank_fight_throttle(10.0f, 0.0f) == 0.0f);
	CHECK(ae_bots_tank_fight_throttle(40.0f, 0.2f) > 0.0f && ae_bots_tank_fight_throttle(40.0f, -0.2f) < 1.0f);
	CHECK(ae_bots_tank_fight_throttle(40.0f, 2.0f) == 0.0f);
	CHECK(ae_bots_tank_fight_throttle(90.0f, 0.0f) == 1.0f);
	/* (no flying, no tank) */
	CHECK(ae_bots_vehicle_seat_kind(AE_BOTS_VEHICLE_BANSHEE, 1, 1) == AE_BOTS_SEAT_NONE);
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
	/* (a tank has its own valuation: no open route known, no wheel; a 40-unit trip is short anyway) */
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
	s.carrying = 0;
	/* roaming (Slayer): a free wheel near by drives whatever its roaming place; not in a fight, not past 18 */
	s.roams = 1;
	s.goal_distance = 10.0f;
	o.detour = 30.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	s.goal_distance = -1.0f;
	o.detour = -1.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	s.fighting = 1;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	s.fighting = 0;
	o.distance = 18.5f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o.distance = 4.0f;
	o.vehicle_class = AE_BOTS_VEHICLE_SCORPION;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o.vehicle_class = AE_BOTS_VEHICLE_WARTHOG;
	s.may_drive = 0;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
}

/* a Scorpion's own valuation (review 11-05: owner never saw a bot take one) */
static void test_tank_drive(void)
{
	struct ae_bots_vehicle_situation s = situation();
	struct ae_bots_seat_offer o = offer(AE_BOTS_SEAT_DRIVER);
	struct ae_bots_seat_offer w = offer(AE_BOTS_SEAT_DRIVER);
	float tank, light;

	s.may_drive = 1;
	s.goal_distance = 90.0f;
	o.vehicle_class = AE_BOTS_VEHICLE_SCORPION;
	o.open_route = 1;
	o.detour = 5.0f;
	tank = ae_bots_vehicle_seat_score(&s, &o);
	CHECK(tank > 0.0f);
	/* (below a light vehicle at the same place) */
	light = ae_bots_vehicle_seat_score(&s, &w);
	CHECK(light > tank);
	/* (a short trip, a cramped or unknown route, a roamer, a fighter, a long detour, a carrier: no) */
	s.goal_distance = 59.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	s.goal_distance = 60.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	s.goal_distance = 90.0f;
	o.open_route = 0;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o.open_route = 1;
	/* (a roamer (Slayer): a tank on open ground, below a light vehicle; not in a fight, not a cramped one) */
	s.roams = 1;
	s.goal_distance = -1.0f;
	o.detour = -1.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f && ae_bots_vehicle_seat_score(&s, &o) < ae_bots_vehicle_seat_score(&s, &w));
	s.fighting = 1;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	s.fighting = 0;
	o.open_route = 0;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o.open_route = 1;
	s.goal_distance = 90.0f;
	o.detour = 5.0f;
	s.roams = 0;
	s.fighting = 1;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	s.fighting = 0;
	o.detour = 36.1f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o.detour = 35.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	s.carrying = 1;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	s.carrying = 0;
	/* (no goal: no tank trip) */
	s.goal_distance = -1.0f;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	/* (its passenger seat is still a ride with a teammate) */
	s.goal_distance = 90.0f;
	o = offer(AE_BOTS_SEAT_PASSENGER);
	o.vehicle_class = AE_BOTS_VEHICLE_SCORPION;
	o.driver = AE_BOTS_DRIVER_TEAMMATE;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
}

/* a hull nearly gone takes no new rider; a hail is a short stop (review 11-05) */
static void test_damaged_and_hails(void)
{
	struct ae_bots_vehicle_situation s = situation();
	struct ae_bots_seat_offer o = offer(AE_BOTS_SEAT_PASSENGER);

	s.carrying = 1;
	o.driver = AE_BOTS_DRIVER_TEAMMATE;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	o.hull_low = 1;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	o = offer(AE_BOTS_SEAT_GUNNER);
	o.driver = AE_BOTS_DRIVER_TEAMMATE;
	s.carrying = 0;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) > 0.0f);
	o.hull_low = 1;
	CHECK(ae_bots_vehicle_seat_score(&s, &o) == 0.0f);
	/* (the leave rule's 0.2 plus a margin) */
	CHECK(ae_bots_vehicle_hull_low(0.29f) && ae_bots_vehicle_hull_low(0.0f));
	CHECK(!ae_bots_vehicle_hull_low(0.3f) && !ae_bots_vehicle_hull_low(1.0f));
	CHECK(0.3f > 0.2f);
	/* a hail: a teammate on foot near, no fight of its own, a few seconds in all */
	CHECK(ae_bots_vehicle_hail_holds(5.0f, 0, 0.0f));
	CHECK(ae_bots_vehicle_hail_holds(11.9f, 0, 2.9f));
	CHECK(!ae_bots_vehicle_hail_holds(12.0f, 0, 0.0f));
	CHECK(!ae_bots_vehicle_hail_holds(25.0f, 0, 0.0f));
	CHECK(!ae_bots_vehicle_hail_holds(5.0f, 1, 0.0f));
	CHECK(!ae_bots_vehicle_hail_holds(5.0f, 0, 3.0f));
	CHECK(!ae_bots_vehicle_hail_holds(-1.0f, 0, 0.0f));
	/* a stop that keeps being hailed ends: simulated ticks of 30 a second */
	{
		int tick, held = 0;

		for (tick = 0; tick < 30 * 60; tick++)
		{
			if (!ae_bots_vehicle_hail_holds(4.0f, 0, (float)held / 30.0f))
				break;
			held++;
		}
		CHECK(held == 90);
	}
}

/* speed-aware driving: the AE ghost (0.27 a tick) reads corners and arrivals at stock-ghost marks */
static void test_speed_scale(void)
{
	struct ae_bots_drive_input in;
	struct ae_bots_drive_output out;

	CHECK(ae_bots_vehicle_speed_scale(AE_BOTS_VEHICLE_GHOST, 0.225f) == 1.0f);
	CHECK(ae_bots_vehicle_speed_scale(AE_BOTS_VEHICLE_GHOST, 0.2f) == 1.0f);
	CHECK(ae_bots_vehicle_speed_scale(AE_BOTS_VEHICLE_GHOST, 0.27f) > 1.19f && ae_bots_vehicle_speed_scale(AE_BOTS_VEHICLE_GHOST, 0.27f) < 1.21f);
	CHECK(ae_bots_vehicle_speed_scale(AE_BOTS_VEHICLE_GHOST, 5.0f) == 2.0f);
	CHECK(ae_bots_vehicle_speed_scale(AE_BOTS_VEHICLE_WARTHOG, 0.27f) == 1.0f);
	/* a corner 6 units off at 9 units a second: a stock vehicle brakes (speed > 8); the same speed on a quicker one is stock-slow */
	memset(&in, 0, sizeof(in));
	in.waypoint_distance = 4.0f;
	in.next_turn = 1.5f;
	in.speed = 9.0f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle == -0.3f);
	in.speed_scale = 1.2f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle > -0.3f);
	/* and the quicker one starts easing a scale further out: 5.5 units off is as 4.6 */
	in.speed = 11.0f;
	in.waypoint_distance = 5.5f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle == -0.3f);
	in.speed_scale = 1.0f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle > -0.3f);
}

static void test_leave(void)
{
	struct ae_bots_ride r = ride(AE_BOTS_SEAT_GUNNER);

	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	CHECK(ae_bots_vehicle_leave(NULL) == AE_BOTS_LEAVE_STAY);
	/* a driver with a person aboard (owner 11-08: in, drive, out at the goal, in again, over and over): it stays at the
	wheel at its goal, parked, or going away; out only wrecked, flipped or on a dying hull that stands */
	{
		struct ae_bots_ride d = ride(AE_BOTS_SEAT_DRIVER);

		d.seated = 20.0f;
		d.goal_distance = 3.0f;
		CHECK(ae_bots_vehicle_leave(&d) == AE_BOTS_LEAVE_THERE);
		d.person_aboard = 1;
		CHECK(ae_bots_vehicle_leave(&d) == AE_BOTS_LEAVE_STAY);
		d.parked = 60.0f;
		d.since_fight = 60.0f;
		CHECK(ae_bots_vehicle_leave(&d) == AE_BOTS_LEAVE_STAY);
		d.goal_distance = 40.0f;
		d.going_away = 30.0f;
		CHECK(ae_bots_vehicle_leave(&d) == AE_BOTS_LEAVE_STAY);
		d.dying = 1;
		CHECK(ae_bots_vehicle_leave(&d) == AE_BOTS_LEAVE_DYING);
		d.dying = 0;
		d.upright = 0;
		CHECK(ae_bots_vehicle_leave(&d) == AE_BOTS_LEAVE_FLIPPED);
	}
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
	/* no driver, hurt with its shield down: out after 3 s, even in a fight; with a driver it stays */
	r = ride(AE_BOTS_SEAT_GUNNER);
	r.under_fire = 1;
	r.since_fight = 0.0f;
	r.without_driver = 2.9f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	r.without_driver = 3.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_NO_DRIVER);
	r.without_driver = 0.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	/* a turret's gunner keeps its gun */
	r.without_driver = 5.0f;
	r.stationary = 1;
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
	/* a tank's rider: an enemy close on foot, the hull nearly gone, the tank standing 2.5 s, its goal near */
	r = ride(AE_BOTS_SEAT_PASSENGER);
	r.tank = 1;
	r.foe_near = 1;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_FOE);
	r = ride(AE_BOTS_SEAT_PASSENGER);
	r.tank = 1;
	r.dying = 1;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_DYING);
	r = ride(AE_BOTS_SEAT_PASSENGER);
	r.tank = 1;
	r.parked = 2.6f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_PARKED);
	r = ride(AE_BOTS_SEAT_PASSENGER);
	r.tank = 1;
	r.goal_distance = 13.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_THERE);
	r = ride(AE_BOTS_SEAT_PASSENGER);
	r.goal_distance = 13.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
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

/* a driver or a gunner of a dying hull that stands gets out; one that rolls stays; a rider keeps its own rule */
static void test_dying_driver(void)
{
	struct ae_bots_ride r = ride(AE_BOTS_SEAT_DRIVER);

	r.dying = 1;
	r.parked = 1.9f;
	r.since_fight = 0.0f;
	r.goal_distance = 40.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	r.parked = 2.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_DYING);
	r.dying = 0;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	r = ride(AE_BOTS_SEAT_GUNNER);
	r.dying = 1;
	r.parked = 3.0f;
	r.since_fight = 0.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_DYING);
	/* (a moment in the seat first) */
	r.seated = 1.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
	/* (a turret keeps its own rule) */
	r = ride(AE_BOTS_SEAT_GUNNER);
	r.stationary = 1;
	r.dying = 1;
	r.parked = 3.0f;
	r.since_fight = 0.0f;
	CHECK(ae_bots_vehicle_leave(&r) == AE_BOTS_LEAVE_STAY);
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
#define NEAR_TO(a, b) ((a) - (b) < 0.001f && (b) - (a) < 0.001f)

/* PLANNED DRIVING (0.9.0): the speed planned ahead of corners and the goal, the throttle chasing it */
static void test_planned_drive(void)
{
	struct ae_bots_drive_input in;
	struct ae_bots_drive_output out;

	/* the aim: full ahead, a share in a turn, the least when the waypoint is well off the nose (never 0) */
	CHECK(ae_bots_drive_wanted_share(0.0f, 30.0f, 0.0f, 0.0f) == 1.0f);
	CHECK(ae_bots_drive_wanted_share(1.0f, 30.0f, 0.0f, 0.0f) == AE_BOTS_DRIVE_AIM_TURN);
	CHECK(ae_bots_drive_wanted_share(-2.0f, 30.0f, 0.0f, 0.0f) == AE_BOTS_DRIVE_AIM_SHARP);
	/* a corner: a right angle 3 units on: (1 - 0.5)^2 x 0.75 + 0.25 = 0.4375 at it, + 0.5625 x (3/12)^2 */
	CHECK(NEAR_TO(ae_bots_drive_wanted_share(0.0f, 3.0f, 1.5707963f, 0.0f), 0.4375f + 0.5625f * 0.0625f));
	/* out of reach, or hardly a turn: no slowing */
	CHECK(ae_bots_drive_wanted_share(0.0f, 12.5f, 1.5707963f, 0.0f) == 1.0f);
	CHECK(ae_bots_drive_wanted_share(0.0f, 3.0f, 0.2f, 0.0f) == 1.0f);
	/* sharper is slower, nearer is slower, a turn about is the slowest */
	CHECK(ae_bots_drive_wanted_share(0.0f, 5.0f, -2.5f, 0.0f) < ae_bots_drive_wanted_share(0.0f, 5.0f, 1.0f, 0.0f));
	CHECK(ae_bots_drive_wanted_share(0.0f, 2.0f, 1.0f, 0.0f) < ae_bots_drive_wanted_share(0.0f, 8.0f, 1.0f, 0.0f));
	CHECK(NEAR_TO(ae_bots_drive_wanted_share(0.0f, 0.0f, 3.1415927f, 0.0f), AE_BOTS_DRIVE_CORNER_SLOWEST));
	/* the goal: from full at 10 down to 0.3 at it; none (0: a fight's target) does nothing */
	CHECK(NEAR_TO(ae_bots_drive_wanted_share(0.0f, 30.0f, 0.0f, 5.0f), 0.65f));
	CHECK(ae_bots_drive_wanted_share(0.0f, 30.0f, 0.0f, 0.0f) == 1.0f);
	/* the least of the three wins */
	CHECK(ae_bots_drive_wanted_share(2.0f, 3.0f, 1.0f, 9.0f) == AE_BOTS_DRIVE_AIM_SHARP);

	memset(&in, 0, sizeof(in));
	in.planned = 1;
	in.waypoint_distance = 30.0f;
	/* the open road from a stop: full throttle */
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle == 1.0f && out.reverse_for == 0.0f);
	/* a right angle 3 units on (about 4.3 a second wanted): from 9 brake, from 6 coast, from 4 the wanted share */
	in.waypoint_distance = 3.0f;
	in.next_turn = 1.5707963f;
	in.speed = 9.0f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle == AE_BOTS_DRIVE_BRAKE);
	in.speed = 6.0f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle == 0.0f);
	in.speed = 4.0f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(NEAR_TO(out.throttle, 0.4375f + 0.5625f * 0.0625f));
	/* a quicker vehicle (scale 1.2) starts slowing a scale further out: 13 units off is as 10.8 */
	in.speed = 0.0f;
	in.waypoint_distance = 13.0f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle == 1.0f);
	in.speed_scale = 1.2f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle < 1.0f);
	/* backing off stays the classic rules' */
	memset(&in, 0, sizeof(in));
	in.planned = 1;
	in.waypoint_distance = 20.0f;
	in.heading_error = 0.4f;
	in.stalled = 1.5f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle < 0.0f && out.reverse_for > 0.0f);
	in.stalled = 0.0f;
	in.reversing = 0.5f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle == -1.0f);
	/* the vehicle's own top speed: the same planned speed takes more throttle in a slower vehicle; never past full */
	memset(&in, 0, sizeof(in));
	in.planned = 1;
	in.waypoint_distance = 3.0f;
	in.next_turn = 1.5707963f;
	in.speed = 2.0f;
	in.top_speed = 7.65f;
	ae_bots_vehicle_drive(&in, &out);
	{
		float warthog = out.throttle;

		in.top_speed = 4.2f;
		ae_bots_vehicle_drive(&in, &out);
		/* (a Scorpion's top speed is under that corner's speed: no slowing at all) */
		CHECK(out.throttle > warthog && out.throttle == 1.0f);
		CHECK(NEAR_TO(warthog, 9.0f * (0.4375f + 0.5625f * 0.0625f) / 7.65f));
	}
	in.waypoint_distance = 30.0f;
	in.next_turn = 0.0f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle == 1.0f);
	/* blocked ahead and on both sides: a crawl, never more; backing off is not capped */
	in.blocked = 1;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle == AE_BOTS_DRIVE_CRAWL);
	in.reversing = 0.5f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle == -1.0f);
	/* the classic driving takes neither */
	memset(&in, 0, sizeof(in));
	in.waypoint_distance = 30.0f;
	in.blocked = 1;
	in.top_speed = 4.2f;
	ae_bots_vehicle_drive(&in, &out);
	CHECK(out.throttle == 1.0f);
	/* aim past the corner: near it and not a sharp turn; far, or sharp (past 115 degrees), aims at the corner */
	CHECK(ae_bots_drive_cut_corner(4.0f, 1.5f) && ae_bots_drive_cut_corner(1.0f, -1.9f));
	CHECK(!ae_bots_drive_cut_corner(7.0f, 1.0f) && !ae_bots_drive_cut_corner(4.0f, 2.1f) && !ae_bots_drive_cut_corner(4.0f, -2.5f));
	/* a new goal from the second wedge in a run */
	CHECK(!ae_bots_drive_new_goal(1) && ae_bots_drive_new_goal(2) && ae_bots_drive_new_goal(3));
	/* the counters: a bump is a 40% fall in a tick from over 3 with the throttle on; stuck is under 0.5 with it at 0.3 */
	CHECK(ae_bots_drive_bump(8.0f, 4.0f, 1.0f));
	CHECK(!ae_bots_drive_bump(8.0f, 5.0f, 1.0f));
	CHECK(ae_bots_drive_bump(8.0f, 4.0f, 0.0f) && !ae_bots_drive_bump(8.0f, 4.0f, -0.3f));
	CHECK(!ae_bots_drive_bump(2.5f, 0.0f, 1.0f));
	CHECK(ae_bots_drive_stuck(0.2f, 0.5f) && ae_bots_drive_stuck(-0.2f, 1.0f));
	CHECK(!ae_bots_drive_stuck(0.2f, 0.05f) && !ae_bots_drive_stuck(1.0f, 1.0f));
	/* a crawl counts as stuck when it does not move */
	CHECK(ae_bots_drive_stuck(0.2f, AE_BOTS_DRIVE_CRAWL));
	CHECK(ae_bots_drive_stuck(0.2f, 0.25f) && ae_bots_drive_stuck(0.2f, -0.5f) && !ae_bots_drive_stuck(0.2f, 0.0f));
}

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
	test_speed_scale();
	test_tank_drive();
	test_damaged_and_hails();
	test_leave();
	test_dying_driver();
	test_never_forever();
	test_drive();
	test_planned_drive();
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
