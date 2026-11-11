/*
AE_BOTS_VEHICLE_RULES.H

The bots' vehicles (ae_bots_vehicle.c, notes/specs/2026-10-16-bots-infinite.md
area 4): which seat a bot on foot goes for, when a bot in a seat gets out, and
(the driving spike) the throttle and steering of a bot at the wheel. Pure
numbers only (no engine types), unit-tested in
port/linux/tests/ae_bots_vehicle_rules_test.c.

Seats, as a player takes them (the action button at the seat's entrance):
- GUNNER: a free gunner seat (a Warthog's chain gun, a gun turret) of a vehicle
  near the bot (bots notice vehicles within 10 units) and a few
  units off its way (or anywhere within 10 while it is losing a fight), or of a
  vehicle a teammate drives (within 15); never while it is
  winning a fight (it fights on), never for a bot that holds a place (a CTF
  defender, the hill's holder) or holds the objective in its hands (a trigger
  pull drops the flag or the ball, players.c: a carrier never pulls one).
- PASSENGER: the free passenger seat of a vehicle a teammate drives (within 15); a carrier
  may take it (it rides home without a trigger).
- DRIVER (the spike, debug.bot_vehicles 2): an empty Warthog or Ghost within 18
  when its goal is far (25), the detour to it at most half the way. A Scorpion has a valuation of its own: an
  objective trip of 60 or more over open ground (AE_BOTS_TANK_DRIVE_GOAL), a detour of 0.3 of the way at most; a roaming bot
  (Slayer) out of a fight takes one standing on open ground; never a fighting bot's; at a lower score than a light vehicle.
- Never a seat in a hull nearly gone (ae_bots_vehicle_hull_low).
- Never a vehicle moving faster than 2 units a second (the limit), a
  flipped or wrecked one, one an enemy is in, a seat another bot goes for, nor
  the one it just left. No Banshee (no flying). A Scorpion's driver has its cannon and chain gun; its other seats only ride.

Leaving (the action button again), after 1.5 seconds in the seat at least
(wrecked or flipped at once): wrecked; flipped; a gunner without a driver for
8 seconds and no fight for 3; a passenger without a driver for 2; any rider
of a vehicle standing still (under the 'active' speed) for 15 seconds,
a passenger for 4, a gun turret's 8 seconds after its last fight; near its goal (8 units); going away from a goal still far
(15 units) for 3 seconds; a bot that comes to hold a place.
*/

#ifndef __AE_BOTS_VEHICLE_RULES_H
#define __AE_BOTS_VEHICLE_RULES_H

/* a vehicle's kind, by its tag's name */
enum ae_bots_vehicle_class
{
	AE_BOTS_VEHICLE_OTHER = 0,
	AE_BOTS_VEHICLE_WARTHOG,
	AE_BOTS_VEHICLE_ROCKET_WARTHOG,
	AE_BOTS_VEHICLE_GHOST,
	AE_BOTS_VEHICLE_SCORPION,
	AE_BOTS_VEHICLE_BANSHEE,
	AE_BOTS_VEHICLE_TURRET,
	AE_BOTS_VEHICLE_CLASSES
};

/* a seat's role */
enum ae_bots_seat_kind
{
	AE_BOTS_SEAT_NONE = 0,
	AE_BOTS_SEAT_DRIVER,
	AE_BOTS_SEAT_GUNNER,
	AE_BOTS_SEAT_PASSENGER,
	AE_BOTS_SEAT_KINDS
};

/* who drives a vehicle */
enum ae_bots_vehicle_driver
{
	AE_BOTS_DRIVER_NONE = 0,
	AE_BOTS_DRIVER_TEAMMATE,
	AE_BOTS_DRIVER_ENEMY
};

/* why a rider gets out (0: it stays) */
enum ae_bots_vehicle_leave
{
	AE_BOTS_LEAVE_STAY = 0,
	AE_BOTS_LEAVE_WRECKED,
	AE_BOTS_LEAVE_FLIPPED,
	AE_BOTS_LEAVE_NO_DRIVER,
	AE_BOTS_LEAVE_PARKED,
	AE_BOTS_LEAVE_THERE,
	AE_BOTS_LEAVE_AWAY,
	AE_BOTS_LEAVE_HOLD,
	AE_BOTS_LEAVE_STUCK,
	/* a rider (no gun of its own) with an enemy close on foot; the vehicle about to die */
	AE_BOTS_LEAVE_FOE,
	AE_BOTS_LEAVE_DYING,
	AE_BOTS_LEAVE_REASONS
};

/* (vehicle tuning) vehicles noticed within this many units; entered when moving
under this speed (units a second); 'active' (not parked) above this speed */
#define AE_BOTS_VEHICLE_NOTICE 10.0f
/* (CE's maps park their vehicles off the paths out of a base: box games) a vehicle a teammate drives is noticed
further; the driving spike's driver goes further for an empty Warthog when its goal is far */
#define AE_BOTS_VEHICLE_NOTICE_DRIVEN 15.0f
#define AE_BOTS_VEHICLE_NOTICE_DRIVE 18.0f
#define AE_BOTS_VEHICLE_ENTER_SPEED 2.0f
#define AE_BOTS_VEHICLE_ACTIVE_SPEED 1.3f
/* a gunner seat of a vehicle no one drives: this far off the bot's way at most (or this near without a goal) */
#define AE_BOTS_VEHICLE_OFF_ROUTE 4.0f
#define AE_BOTS_VEHICLE_NEAR_NO_GOAL 6.0f
/* (the driving spike) a goal this far makes a bot drive; the detour to the vehicle at most this part of the way
(and 8 units at least) */
#define AE_BOTS_VEHICLE_DRIVE_GOAL 25.0f
#define AE_BOTS_VEHICLE_DRIVE_DETOUR 0.5f
/* a gunner with no driver this many seconds, hurt with its shield mostly gone, gets out (owner's 0.7.3 list: "leave
driverless gun seats under fire") */
#define AE_BOTS_VEHICLE_EXPOSED_GUN 3.0f

/* a Scorpion is driven (by a bot of its own accord) only on a long objective trip over open ground: 60 units at least */
#define AE_BOTS_TANK_DRIVE_GOAL 60.0f
#define AE_BOTS_TANK_DRIVE_DETOUR 0.3f
#define AE_BOTS_TANK_DRIVE_SCORE 0.55f
/* a hull below this fraction takes no new rider (the leave rule's 0.2 and a margin: a carrier boarded a Warthog at 0.2 and
got out again 1.5 s later) */
#define AE_BOTS_VEHICLE_HULL_LOW 0.3f
/* a driver or gunner leaves a dying hull that has stood this many seconds */
#define AE_BOTS_VEHICLE_DYING_PARKED 2.0f
/* a teammate's round at the driver's vehicle is a hail only from a teammate on foot this near; it stops the driver this many
seconds in all (a stop), then it drives on */
#define AE_BOTS_VEHICLE_HAIL_NEAR 12.0f
#define AE_BOTS_VEHICLE_HAIL_WAIT_MAX 3.0f
int ae_bots_vehicle_hull_low(float body_vitality);
/* SPEED-AWARE DRIVING (owner 11-05: the AE ghost is 20% quicker and bots drove it at stock-ghost marks): the factor by which a
vehicle's own top speed (world units a tick, the vehicle tag's, after any AE tune) outruns the stock one the thresholds were
tuned for: the stock Ghost's 0.225; 1 for any other vehicle or a slower tag; at most 2 */
#define AE_BOTS_GHOST_STOCK_SPEED 0.225f
float ae_bots_vehicle_speed_scale(int vehicle_class, float top_speed_per_tick);
/* whether a hail still holds the driver: the hailer's distance, the driver in a fight of its own, seconds waited in this stop */
int ae_bots_vehicle_hail_holds(float hailer_distance, int fighting, float waited);

/* the vehicle's kind from its tag's name (the stock multiplayer vehicles: vehicles\warthog\mp_warthog,
vehicles\rwarthog\rwarthog, vehicles\ghost\ghost_mp, vehicles\scorpion\scorpion_mp, vehicles\banshee\banshee_mp,
vehicles\c gun turret\c gun turret_mp; community maps' copies keep those words) */
int ae_bots_vehicle_class_of_name(char const *tag_name);
/* a seat's role from its flags: the driver's, a gunner's, else a passenger's; a turret's one seat is a gunner's;
NONE for a seat of a vehicle the bots leave alone (a Banshee's) */
int ae_bots_vehicle_seat_kind(int vehicle_class, int driver_flag, int gunner_flag);
/* the object a rider's line of sight passes through: the vehicle it rides (its outermost one, given as
ultimate_parent_index; -1, or the unit itself, for a unit on foot), so that the ray starts outside it; else the unit.
(The gunner's head sits inside the Warthog's collision: in box games on Death Island, 23-46% of a RECRUIT gunner's
rays at enemies ended on its own rocket Warthog, 41% on its own Warthog, so a seated bot lost its target again and
again, and each new sighting began its reaction time again: the rocket Warthog's gunners hardly fired. Collision
skips an ignored object's riders too: the bot itself.) */
long ae_bots_vehicle_sight_ignore_of(long unit_index, long ultimate_parent_index);

/* a seat a bot on foot might go for */
struct ae_bots_seat_offer
{
	int vehicle_class;
	int seat;
	/* from the bot to the seat's entrance; the entrance's distance from the bot's way to its goal (negative: no
	goal) */
	float distance;
	float off_route;
	/* how much longer its way is by the seat (to the entrance, then on to the goal, less straight to the goal;
	negative: no goal) */
	float detour;
	/* the vehicle's speed (units a second) */
	float speed;
	int alive;
	int upright;
	int driver;
	/* an enemy rides it; another bot goes for this seat; the bot left this vehicle a moment ago */
	int enemy_aboard;
	int reserved;
	int recently_left;
	/* the hull is nearly gone (ae_bots_vehicle_hull_low): a seat in it is a seat in a wreck-to-be (0: healthy) */
	int hull_low;
	/* a Scorpion's trip: the goal's ground is open and wide enough for a tank (0: not known or cramped) */
	int open_route;
};

/* the bot on foot */
struct ae_bots_vehicle_situation
{
	/* an enemy in sight; it is winning that fight (its confidence high) */
	int fighting;
	int winning;
	/* it holds the objective in its hands (the flag, the ball) */
	int carrying;
	/* it holds a place (defends its flag, holds the hill) */
	int holds_place;
	/* the driving spike is on */
	int may_drive;
	/* how far its goal is (negative: none) */
	float goal_distance;
	/* it roams with no objective to play (Slayer, or no role): a free wheel near by is a reason of its own to drive,
	whatever its roaming place (owner 11-02, Slayer: "sometimes they drive sometimes they dont": only a far roaming
	place past a vehicle made a driver) */
	int roams;
};

/* how much the bot wants the seat, 0 (not at all) to 1; the best seat of those over 0 is the one it goes for */
float ae_bots_vehicle_seat_score(struct ae_bots_vehicle_situation const *situation, struct ae_bots_seat_offer const *offer);

/* a bot in a seat */
struct ae_bots_ride
{
	int seat;
	int alive;
	int upright;
	/* seconds: in the seat; without a driver (0 with one); the vehicle under the active speed; since its last
	fight; going away from its goal */
	float seated;
	float without_driver;
	float parked;
	float since_fight;
	float going_away;
	/* how far its goal is (negative: none: Slayer's gunner fights wherever it is driven) */
	float goal_distance;
	int holds_place;
	/* a gun turret: it never moves; its gunner stays while it fights, and gets out 8 seconds after its last fight */
	int stationary;
	/* the UT spike's gunner patience (HALO_BOTS_UT, on by default): 0 keeps the gunner's times before it */
	int patient_gunner;
	/* hurt in the last moment with its shield mostly gone: a gun with no one at the wheel is a sitting target */
	int under_fire;
	/* a tank's rider seat: it hops off sooner and from further from its goal */
	int tank;
	/* an enemy on foot within a rider's reach to fight it; the vehicle's hull nearly gone */
	int foe_near;
	int dying;
	/* a driver with a person (not a bot) in another seat: it keeps driving them, never out for being there, parked or
	away from its goal (owner 11-08: a called driver with him on the gun drove, got out at its goal, got in again, over
	and over); out only as anyone would be (wrecked, flipped, dying, stuck) */
	int person_aboard;
};

/* whether the rider gets out now, and why (AE_BOTS_LEAVE_STAY: it stays) */
int ae_bots_vehicle_leave(struct ae_bots_ride const *ride);
/* the reason's name for halo.log */
char const *ae_bots_vehicle_leave_name(int reason);
char const *ae_bots_vehicle_seat_name(int seat);
char const *ae_bots_vehicle_class_name(int vehicle_class);

/* ---------- the driving spike */

struct ae_bots_drive_input
{
	/* radians from the vehicle's heading to its next waypoint (-pi..pi); how far that waypoint is; the turn at it
	to the one after (radians, 0 straight on) */
	float heading_error;
	float waypoint_distance;
	float next_turn;
	/* the vehicle's speed forward (units a second; negative backwards) */
	float speed;
	/* seconds it has not moved while the throttle was open; seconds of reversing left (0 none) */
	float stalled;
	float reversing;
	/* ae_bots_vehicle_speed_scale (0: 1): the speed and the waypoint's distance are read in stock-vehicle terms, so the corner and
	arrival slow-downs start sooner by the factor */
	float speed_scale;
};

struct ae_bots_drive_output
{
	/* -1 (full reverse) to 1 (full ahead) */
	float throttle;
	/* the heading to steer for, from the waypoint's (radians): 0 straight at it */
	float steer;
	/* start reversing for this many seconds (0: no) */
	float reverse_for;
};

/* the controls for the next tick */
void ae_bots_vehicle_drive(struct ae_bots_drive_input const *input, struct ae_bots_drive_output *output);

/* a Scorpion's driver in a fight, in world units. Its hull turns to the aim only while it moves, so it stands to shoot
at what is off to its side. The shell's blast reaches 3.25: not fired nearer than AE_BOTS_TANK_SHELL_LEAST */
#define AE_BOTS_TANK_SHELL_LEAST 5.0f
#define AE_BOTS_TANK_SHELL_MOST 70.0f
#define AE_BOTS_TANK_CHAIN_MOST 25.0f
#define AE_BOTS_TANK_BACK_OFF 4.0f
#define AE_BOTS_TANK_HOLD 20.0f
#define AE_BOTS_TANK_CREEP 0.5f
#define AE_BOTS_TANK_CREEP_OFF_HULL 1.0f
/* the ticks between presses of the cannon's trigger: a press a shot, the weapon's own reload deciding when it fires */
#define AE_BOTS_TANK_SHELL_PRESS_TICKS 10
/* which guns reach this far */
void ae_bots_tank_guns(float distance, int *cannon, int *chain_gun);
/* the throttle in a fight, the target this far and this far off the hull's heading (radians) */
float ae_bots_tank_fight_throttle(float distance, float off_hull);

/* debug.bot_vehicles 1 lets bots drive on the open maps only, until the maps with narrow ways are tuned; a map's name holds one
of the open names (a port's "bloodgulch_ce" too). 2: everywhere; 0: never. */
int ae_bots_vehicle_map_is_open(char const *map_name);
/* LOADING UP (owner 2026-10-30: it needs to be fast): a bot answers a honk when fewer free teammates stand nearer than there are open
seats, so the nearest fill the vehicle; a walk across the map is no answer */
int ae_bots_vehicle_called_in_turn(int nearer_free_bots, int open_seats);
int ae_bots_vehicle_may_drive(long setting, char const *map_name);

#endif
