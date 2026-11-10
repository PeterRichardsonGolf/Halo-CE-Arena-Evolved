/*
AE_GRENADE_SHOT_RULES.H

SHOOTABLE GRENADES, a gametype rule (GAME OPTIONS, WEAPONS; the gametype's byte is ae_variant_ext's
shootable_grenades, AE_EXT_SHOOTABLE_GRENADES_*): a bullet whose travel passes a grenade sets it off. The grenade is a
live thrown one (in flight or lying, never stuck to someone) or a grenade pickup (dropped at a death or placed by the
map). Halo CE's grenade projectiles have no collision model, so the hit is tested in code, as these pure functions say
(port/linux/tests/ae_grenade_shot_rules_test.c); ae_grenade_shot.c plays them.
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
