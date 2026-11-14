/*
AE_RAGDOLL_RULES.H

Arena Evolved: whose body a ragdoll is, as a pure rule (port/linux/tests/ae_ragdoll_rules_test.c runs it).

The game clears a unit's player_index at its death (units.c's unit_kill and the death path; player_died moves the
player's unit_index to its dead_unit_index), and a ragdoll is made after that. A body's style (game.ragdoll_players
or game.ragdoll_ai) therefore cannot read the unit's player_index: every player's body read as an AI's. A body is a
player's while its unit has a player, or once dead when a player's unit_index or dead_unit_index names it (every
death passes through player_died, whatever killed it: a bullet, a blast, a fall, a kill command, a vehicle).
*/

#ifndef __AE_RAGDOLL_RULES_H
#define __AE_RAGDOLL_RULES_H

#include <math.h>

/* the player lists hold NONE (-1) for no unit; a body never matches that */
static inline int ae_ragdoll_is_player_body(long unit_player_index, long unit_index,
	long const *player_unit_indices, long const *player_dead_unit_indices, long player_count)
{
	long index;

	if (unit_player_index != -1)
		return 1;
	if (unit_index == -1)
		return 0;
	for (index = 0; index < player_count; index++)
	{
		if (player_unit_indices[index] == unit_index || player_dead_unit_indices[index] == unit_index)
			return 1;
	}
	return 0;
}

/* ---------- MODERN's pushes: the start's kick, the push of a bullet or a melee, a hard
blow's tumble */

/* CE's damage categories (a damage effect's category, the tags' order) */
enum
{
	AE_DAMAGE_CATEGORY_NONE,
	AE_DAMAGE_CATEGORY_FALLING,
	AE_DAMAGE_CATEGORY_BULLET,
	AE_DAMAGE_CATEGORY_GRENADE,
	AE_DAMAGE_CATEGORY_HIGH_EXPLOSIVE,
	AE_DAMAGE_CATEGORY_SNIPER,
	AE_DAMAGE_CATEGORY_MELEE,
	AE_DAMAGE_CATEGORY_FLAME,
	AE_DAMAGE_CATEGORY_MOUNTED_WEAPON,
	AE_DAMAGE_CATEGORY_VEHICLE,
	AE_DAMAGE_CATEGORY_PLASMA,
	AE_DAMAGE_CATEGORY_NEEDLE,
	AE_DAMAGE_CATEGORY_SHOTGUN
};

/* the push (instantaneous acceleration, wu/s, before the body's scale: 1.2) MODERN gives a category whose CE tags
push nothing: CE's bullets and melee carry 0. Bullets 0.2, sniper 0.25, melee 0.75, plasma 0.125, needles 0.05,
shotgun pellets 0.1; mounted guns as bullets. Grenades and explosives: 0 here (CE's tags push them) */
static inline float ae_ragdoll_category_push(int category)
{
	switch (category)
	{
	case AE_DAMAGE_CATEGORY_BULLET: return 0.2f;
	case AE_DAMAGE_CATEGORY_SNIPER: return 0.25f;
	case AE_DAMAGE_CATEGORY_MELEE: return 0.75f;
	case AE_DAMAGE_CATEGORY_MOUNTED_WEAPON: return 0.2f;
	case AE_DAMAGE_CATEGORY_PLASMA: return 0.125f;
	case AE_DAMAGE_CATEGORY_NEEDLE: return 0.05f;
	case AE_DAMAGE_CATEGORY_SHOTGUN: return 0.1f;
	default: return 0.f;
	}
}

/* the parts' mass-weighted middle; 0 when they weigh nothing */
static inline int ae_ragdoll_middle(float const (*positions)[3], float const *inverse_masses, int count, float *middle)
{
	float total = 0.f;
	int index;

	middle[0] = middle[1] = middle[2] = 0.f;
	for (index = 0; index < count; index++)
	{
		float mass = inverse_masses[index] > 1e-6f ? 1.f / inverse_masses[index] : 0.f;

		middle[0] += positions[index][0] * mass;
		middle[1] += positions[index][1] * mass;
		middle[2] += positions[index][2] * mass;
		total += mass;
	}
	if (total <= 0.f)
		return 0;
	middle[0] /= total;
	middle[1] /= total;
	middle[2] /= total;
	return 1;
}

/* the start's kick, turned about a LEVEL axis only. The kick: along = the part hit less the middle, the
blow's direction made square to it, each part given the direction x strength x (its distance along `along` / the
farthest ahead), x 0.65 behind the middle, x 2 for the part hit, and `lift` straight up on every part. AE takes
`along` as up or down only (the hit's side of the middle's height): a hit on an arm out at the side would otherwise
turn the body about the up axis (a pirouette on the spot), and the direction's level part only. So a body tips the
way it was shot, as a death from the front falls: a chest, belly or head hit throws the top back, a leg hit (the lowest
45 % of the body) sweeps the legs. Velocities are added to (world units a tick); 1 if it turned (0: no level
part to the blow, or no height to the body: the lift only) */
static inline int ae_ragdoll_level_kick(float const (*positions)[3], float const *inverse_masses, int count,
	float const *epicenter, float const *direction, float strength, float lift, float (*velocities)[3])
{
	float middle[3], across[2], length, up, farthest = 0.f;
	int index, hit = -1, turns;
	float nearest = 0.f;

	for (index = 0; index < count; index++)
	{
		float dx = positions[index][0] - epicenter[0], dy = positions[index][1] - epicenter[1],
			dz = positions[index][2] - epicenter[2], distance = dx * dx + dy * dy + dz * dz;

		if (hit < 0 || distance < nearest)
		{
			hit = index;
			nearest = distance;
		}
	}
	turns = hit >= 0 && ae_ragdoll_middle(positions, inverse_masses, count, middle);
	across[0] = direction[0];
	across[1] = direction[1];
	length = across[0] * across[0] + across[1] * across[1];
	if (length < 1e-8f)
		turns = 0;
	else
	{
		length = 1.f / sqrtf(length);
		across[0] *= length;
		across[1] *= length;
	}
	/* (a leg hit: the part hit in the body's lowest 45 %, the thighs down; a hit under the mass middle but above that
	(a belly or a low chest, under a heavy top) still throws the top with the shot, owner 11-11: "shot from the back
	they move forward but fall back ward") */
	{
		float lowest = count > 0 ? positions[0][2] : 0.f, highest = lowest;

		for (index = 1; index < count; index++)
		{
			if (positions[index][2] < lowest)
				lowest = positions[index][2];
			if (positions[index][2] > highest)
				highest = positions[index][2];
		}
		up = turns && positions[hit][2] < middle[2] && positions[hit][2] < lowest + 0.45f * (highest - lowest) ? -1.f :
			1.f;
	}
	for (index = 0; turns && index < count; index++)
	{
		float distance = (positions[index][2] - middle[2]) * up;

		if (distance > farthest)
			farthest = distance;
	}
	if (farthest < 1e-4f)
		turns = 0;
	for (index = 0; index < count; index++)
	{
		if (turns)
		{
			float share = (positions[index][2] - middle[2]) * up / farthest;

			if (share < 0.f)
				share *= 0.65f;
			if (index == hit)
				share *= 2.f;
			velocities[index][0] += across[0] * strength * share;
			velocities[index][1] += across[1] * strength * share;
		}
		velocities[index][2] += lift;
	}
	return turns;
}

/* the tumble of a hard blow (one over 1.4 wu/s): a turn about up x push (a level axis: end over end, the top going
with the push) at `rate` radians a tick (random(0..1) x |push| x pi/2 a second); velocities added to */
static inline void ae_ragdoll_tumble(float const (*positions)[3], float const *inverse_masses, int count,
	float const *push, float rate, float (*velocities)[3])
{
	float middle[3], axis[3], length;
	int index;

	/* (up x push: up = (0, 0, 1)) */
	axis[0] = -push[1];
	axis[1] = push[0];
	axis[2] = 0.f;
	length = axis[0] * axis[0] + axis[1] * axis[1];
	if (length < 1e-8f || !ae_ragdoll_middle(positions, inverse_masses, count, middle))
		return;
	length = rate / sqrtf(length);
	axis[0] *= length;
	axis[1] *= length;
	for (index = 0; index < count; index++)
	{
		float r0 = positions[index][0] - middle[0], r1 = positions[index][1] - middle[1],
			r2 = positions[index][2] - middle[2];

		/* (axis x r, axis's k 0) */
		velocities[index][0] += axis[1] * r2;
		velocities[index][1] += -axis[0] * r2;
		velocities[index][2] += axis[0] * r1 - axis[1] * r0;
	}
}

#endif
