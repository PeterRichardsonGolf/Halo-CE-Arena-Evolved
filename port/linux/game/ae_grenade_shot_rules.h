/*
AE_GRENADE_SHOT_RULES.H

SHOOTABLE GRENADES, a gametype rule (GAME OPTIONS, WEAPONS; the gametype's byte is ae_variant_ext's
shootable_grenades, AE_EXT_SHOOTABLE_GRENADES_*): a bullet whose travel passes a grenade sets it off. The grenade is a
live thrown one (in flight or lying, never stuck to someone) or a grenade pickup (dropped at a death or placed by the
map). Halo CE's grenade projectiles have no collision model, so the hit is tested in code, as these pure functions say
(port/linux/tests/ae_grenade_shot_rules_test.c); ae_grenade_shot.c plays them.

A live thrown grenade goes off at one hit. A pickup takes a real hit: it is not shootable until it has lain
AE_GRENADE_SHOT_SETTLE_TICKS since it was made (a grenade dropped at a death is a new object; a map's are made at the game's
start), and it goes off when the bullets that passed it have done AE_GRENADE_SHOT_PICKUP_HEALTH damage between them (each
bullet its impact damage, the larger of its bounds), the damage forgotten AE_GRENADE_SHOT_DAMAGE_MEMORY_TICKS after its last
hit. A bullet counts once on a pickup however many machines saw it pass (the host's own simulation and a client's report).
*/

#ifndef AE_GRENADE_SHOT_RULES_H
#define AE_GRENADE_SHOT_RULES_H

struct ae_grenade_shot_point
{
	float x, y, z;
};

enum
{
	/* a grenade's hit size (its model's bounds, clamped), world units */
	AE_GRENADE_SHOT_MINIMUM_RADIUS_100 = 15,
	AE_GRENADE_SHOT_MAXIMUM_RADIUS_100 = 35,
	/* the host's checks of a client's report: a live grenade of the kind near the reported point (a grenade in flight
	has moved since, by a round trip), a pickup lies still; the ticks a player's reports are apart, and the ticks a
	blast's place is remembered against a second report of it */
	AE_GRENADE_SHOT_PROJECTILE_TOLERANCE_100 = 150,
	AE_GRENADE_SHOT_PICKUP_TOLERANCE_100 = 60,
	AE_GRENADE_SHOT_REPORT_TICKS = 3,
	AE_GRENADE_SHOT_BLAST_MEMORY_TICKS = 20,
	/* a report is taken only when the host saw a bullet of that player lately (the ticks, a round trip and the host's own
	delay) whose line passes this near the grenade */
	AE_GRENADE_SHOT_FIRE_TICKS = 20,
	AE_GRENADE_SHOT_RAY_TOLERANCE_100 = 150,
	/* a client sets off the nearest copy within this of the host's point */
	AE_GRENADE_SHOT_COPY_TOLERANCE_100 = 200,
};

enum
{
	/* a pickup lies this long after it was made before it can be shot (1.5 s) */
	AE_GRENADE_SHOT_SETTLE_TICKS = 45,
	/* the damage that sets a pickup off: one battle rifle burst (3 rounds of 6.5), not two of its rounds; two EVO or DMR
	rounds (17.9), not one */
	AE_GRENADE_SHOT_PICKUP_HEALTH = 18,
	/* a pickup's damage is forgotten this long after its last hit (3 s) */
	AE_GRENADE_SHOT_DAMAGE_MEMORY_TICKS = 90,
	/* the pickups whose damage is kept at once, and the bullets remembered as counted */
	AE_GRENADE_SHOT_DAMAGE_PLACES = 32,
	AE_GRENADE_SHOT_COUNTED_PLACES = 128,
};

/* the damage done to pickups: one place a pickup (object -1: free) */
struct ae_grenade_shot_damage
{
	long object[AE_GRENADE_SHOT_DAMAGE_PLACES];
	float damage[AE_GRENADE_SHOT_DAMAGE_PLACES];
	long time[AE_GRENADE_SHOT_DAMAGE_PLACES];
};

/* the bullets that have counted on a pickup: (bullet, pickup, when), the oldest written over */
struct ae_grenade_shot_counted
{
	long bullet[AE_GRENADE_SHOT_COUNTED_PLACES];
	long object[AE_GRENADE_SHOT_COUNTED_PLACES];
	long time[AE_GRENADE_SHOT_COUNTED_PLACES];
	int next;
};

/* a pickup made at made can be shot at now (a clock gone back: it can) */
int ae_grenade_shot_settled(long now, long made);
/* a bullet's damage on a pickup: the larger of its impact damage's bounds, never below 0 */
float ae_grenade_shot_bullet_damage(float lower, float upper);
void ae_grenade_shot_damage_clear(struct ae_grenade_shot_damage *table);
/* adds a bullet's damage to a pickup at now, its damage first forgotten when its last hit is older than the memory; the
pickup's damage now. A pickup not kept takes a free place, else an expired one, else the one hit longest ago */
float ae_grenade_shot_damage_add(struct ae_grenade_shot_damage *table, long object, long now, float amount);
/* a pickup gone (set off): its place freed */
void ae_grenade_shot_damage_forget(struct ae_grenade_shot_damage *table, long object);
/* the damage is enough to set a pickup off */
int ae_grenade_shot_damage_enough(float damage);
void ae_grenade_shot_counted_clear(struct ae_grenade_shot_counted *counted);
/* 1 when the bullet has not counted on the pickup within the memory, and now it has; 0 when it already had */
int ae_grenade_shot_counted_take(struct ae_grenade_shot_counted *counted, long bullet, long object, long now);

/* the hit size from an object's bounding radius: 0.15 to 0.35 */
float ae_grenade_shot_radius(float bounds);
/* whether the bullet's segment from..to passes within radius of the centre; t (0..1, along the segment) is where it comes
nearest */
int ae_grenade_shot_hit(struct ae_grenade_shot_point const *from, struct ae_grenade_shot_point const *to,
	struct ae_grenade_shot_point const *center, float radius, float *t);
/* the length of the wall test from the bullet's start toward the grenade: stops short of the grenade (a pickup on the floor
has the floor just under its centre) */
float ae_grenade_shot_wall_length(float length, float radius);
/* the globals' grenade kind a pickup is: by its equipment's grenade powerup and grenade type (a map's own grenade pickup
is not the globals' item), else the globals' item that it is (item_kind, -1 for none), else -1 */
int ae_grenade_shot_pickup_kind(int is_grenade_powerup, int grenade_type, int kind_count, int item_kind);
/* the nearest of two ways a bullet can pass: 1 when a candidate at t is nearer along the segment than the best so far */
int ae_grenade_shot_nearer(float t, float best_t);
/* the host's check of a report: a candidate's distance from the reported point, whether it is a pickup */
int ae_grenade_shot_report_near(float distance, int pickup);
/* a player's reports: one at most every AE_GRENADE_SHOT_REPORT_TICKS */
int ae_grenade_shot_report_rate_allows(long now, long last_time);
/* a report at (distance from) a blast made lately: it is that blast's, not a new one */
int ae_grenade_shot_is_repeat(float distance, long now, long blast_time);
/* a client's copy within the tolerance of the host's blast point */
int ae_grenade_shot_copy_near(float distance);
/* the host saw a bullet of the shooter at seen_time (never: a time before the game): lately enough for a report at now */
int ae_grenade_shot_fire_recent(long now, long seen_time);
/* whether the grenade's centre is near the line of a bullet that travelled from..to, on its way on (not behind its start) */
int ae_grenade_shot_on_ray(struct ae_grenade_shot_point const *from, struct ae_grenade_shot_point const *to,
	struct ae_grenade_shot_point const *center);
/* a message's bytes: the kind in range, flags known */
int ae_grenade_shot_message_ok(int kind, int kind_count, int flags);

enum
{
	AE_GRENADE_SHOT_FLAG_PICKUP = 1,
	AE_GRENADE_SHOT_FLAGS = 1,
};

#endif
