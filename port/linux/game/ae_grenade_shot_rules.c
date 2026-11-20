/*
AE_GRENADE_SHOT_RULES.C

See ae_grenade_shot_rules.h.
*/

#include "ae_grenade_shot_rules.h"

float ae_grenade_shot_radius(float bounds)
{
	float low = (float)AE_GRENADE_SHOT_MINIMUM_RADIUS_100 / 100.0f;
	float high = (float)AE_GRENADE_SHOT_MAXIMUM_RADIUS_100 / 100.0f;

	return !(bounds >= low) ? low : bounds > high ? high : bounds;
}

int ae_grenade_shot_hit(struct ae_grenade_shot_point const *from, struct ae_grenade_shot_point const *to,
	struct ae_grenade_shot_point const *center, float radius, float *t)
{
	float dx = to->x - from->x, dy = to->y - from->y, dz = to->z - from->z;
	float wx = center->x - from->x, wy = center->y - from->y, wz = center->z - from->z;
	float length2 = dx * dx + dy * dy + dz * dz;
	float along = length2 > 0.0f ? (wx * dx + wy * dy + wz * dz) / length2 : 0.0f;
	float nx, ny, nz, distance2;

	along = along < 0.0f ? 0.0f : along > 1.0f ? 1.0f : along;
	nx = from->x + dx * along - center->x;
	ny = from->y + dy * along - center->y;
	nz = from->z + dz * along - center->z;
	distance2 = nx * nx + ny * ny + nz * nz;
	*t = along;
	return distance2 <= radius * radius;
}

float ae_grenade_shot_wall_length(float length, float radius)
{
	return length > radius + 0.1f ? length - radius - 0.1f : length;
}

int ae_grenade_shot_pickup_kind(int is_grenade_powerup, int grenade_type, int kind_count, int item_kind)
{
	if (is_grenade_powerup && grenade_type >= 0 && grenade_type < kind_count)
		return grenade_type;
	if (item_kind >= 0 && item_kind < kind_count)
		return item_kind;
	return -1;
}

int ae_grenade_shot_nearer(float t, float best_t)
{
	return t < best_t;
}

int ae_grenade_shot_report_near(float distance, int pickup)
{
	float tolerance = (float)(pickup ? AE_GRENADE_SHOT_PICKUP_TOLERANCE_100 : AE_GRENADE_SHOT_PROJECTILE_TOLERANCE_100) / 100.0f;

	return distance >= 0.0f && distance <= tolerance;
}

int ae_grenade_shot_report_rate_allows(long now, long last_time)
{
	return now - last_time >= AE_GRENADE_SHOT_REPORT_TICKS || now < last_time;
}

int ae_grenade_shot_is_repeat(float distance, long now, long blast_time)
{
	return now >= blast_time && now - blast_time < AE_GRENADE_SHOT_BLAST_MEMORY_TICKS &&
		distance >= 0.0f && distance <= (float)AE_GRENADE_SHOT_PROJECTILE_TOLERANCE_100 / 100.0f;
}

int ae_grenade_shot_copy_near(float distance)
{
	return distance >= 0.0f && distance <= (float)AE_GRENADE_SHOT_COPY_TOLERANCE_100 / 100.0f;
}

int ae_grenade_shot_message_ok(int kind, int kind_count, int flags)
{
	return kind >= 0 && kind < kind_count && !(flags & ~AE_GRENADE_SHOT_FLAGS);
}

int ae_grenade_shot_fire_recent(long now, long seen_time)
{
	return now >= seen_time && now - seen_time <= AE_GRENADE_SHOT_FIRE_TICKS;
}

int ae_grenade_shot_on_ray(struct ae_grenade_shot_point const *from, struct ae_grenade_shot_point const *to,
	struct ae_grenade_shot_point const *center)
{
	float dx = to->x - from->x, dy = to->y - from->y, dz = to->z - from->z;
	float wx = center->x - from->x, wy = center->y - from->y, wz = center->z - from->z;
	float length2 = dx * dx + dy * dy + dz * dz;
	float along, nx, ny, nz, tolerance = (float)AE_GRENADE_SHOT_RAY_TOLERANCE_100 / 100.0f;

	if (!(length2 > 0.0f))
		return 0;
	along = (wx * dx + wy * dy + wz * dz) / length2;
	/* (a grenade a little behind the start is the bullet's own spot: the tolerance covers it) */
	along = along < 0.0f ? 0.0f : along;
	nx = from->x + dx * along - center->x;
	ny = from->y + dy * along - center->y;
	nz = from->z + dz * along - center->z;
	return nx * nx + ny * ny + nz * nz <= tolerance * tolerance;
}
