/*
AE_SMOKE_RULES.H

THE AE SMOKE GRENADE's volume, pure (port/linux/tests/ae_smoke_rules_test.c). ae_smoke_volume.c and ae_smoke_grenade.c
read it.

A smoke grenade (the pack's cloud, packs\ae_smoke.aepack: tools/weapon_pack/smoke/) is TWO things. The cloud the
player sees is sprites each machine draws with its own random numbers (the engine's effects use a local seed), so
no two machines draw the same cloud. What blocks sight is a VOLUME instead, a sphere (centre, radius, start tick)
made from the grenade's detonation, the same numbers on every machine that sees the detonation, and nothing about
it is random. The bots' sight (bots.c bots_line_of_sight) asks the volume; the engine never blocks sight with
sprites.

TIMES (owner, 2026-11-04): 10 seconds fully there, then a fade of about 3 seconds, gone at 13 s. The sprites'
lifespan in the pack is the same 13 s with a 3 s fade out. The volume blocks at its full radius through the hold and
shrinks linearly to AE_SMOKE_FADE_RADIUS_SCALE of it through the fade (the sprites thin out as it does), then ends.

SIGHT: a line from an eye to a point is blocked when the length of it inside the sphere is at least
AE_SMOKE_MIN_CHORD (a line that only grazes the edge sees through; an eye or a target inside the cloud, close to
its edge, still sees out). The radius is a little under the sprites' dense core so that the cloud never looks
thinner than what blocks.
*/

#ifndef __AE_SMOKE_RULES_H
#define __AE_SMOKE_RULES_H

#include <math.h>

#define AE_SMOKE_TICKS_PER_SECOND 30
#define AE_SMOKE_HOLD_TICKS (10 * AE_SMOKE_TICKS_PER_SECOND)
#define AE_SMOKE_FADE_TICKS (3 * AE_SMOKE_TICKS_PER_SECOND)
#define AE_SMOKE_LIFE_TICKS (AE_SMOKE_HOLD_TICKS + AE_SMOKE_FADE_TICKS)
#define AE_SMOKE_RADIUS 2.8f
#define AE_SMOKE_FADE_RADIUS_SCALE 0.6f
/* the cloud's centre above the detonation (the pack's sprites spawn this high) */
#define AE_SMOKE_LIFT 1.0f
#define AE_SMOKE_MIN_CHORD 1.2f
#define AE_SMOKE_MAXIMUM 16

/* whether a volume of this age (ticks since the detonation) still stands */
static inline int ae_smoke_alive(long age)
{
	return age >= 0 && age < AE_SMOKE_LIFE_TICKS;
}

/* a volume's radius at this age (0 once it has ended) */
static inline float ae_smoke_radius_at(long age)
{
	float fade;

	if (!ae_smoke_alive(age))
		return 0.0f;
	if (age < AE_SMOKE_HOLD_TICKS)
		return AE_SMOKE_RADIUS;
	fade = (float)(age - AE_SMOKE_HOLD_TICKS) / (float)AE_SMOKE_FADE_TICKS;
	return AE_SMOKE_RADIUS * (1.0f - fade * (1.0f - AE_SMOKE_FADE_RADIUS_SCALE));
}

/* the length of the segment a to b inside the sphere (centre c, radius r) */
static inline float ae_smoke_chord(float const *a, float const *b, float const *c, float r)
{
	float d[3], f[3], length, qa, qb, qc, discriminant, root, t0, t1;

	d[0] = b[0] - a[0];
	d[1] = b[1] - a[1];
	d[2] = b[2] - a[2];
	f[0] = a[0] - c[0];
	f[1] = a[1] - c[1];
	f[2] = a[2] - c[2];
	qa = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
	if (qa <= 0.0f || r <= 0.0f)
		return 0.0f;
	qb = 2.0f * (f[0] * d[0] + f[1] * d[1] + f[2] * d[2]);
	qc = f[0] * f[0] + f[1] * f[1] + f[2] * f[2] - r * r;
	discriminant = qb * qb - 4.0f * qa * qc;
	if (discriminant <= 0.0f)
		return 0.0f;
	root = sqrtf(discriminant);
	t0 = (-qb - root) / (2.0f * qa);
	t1 = (-qb + root) / (2.0f * qa);
	if (t0 < 0.0f)
		t0 = 0.0f;
	if (t1 > 1.0f)
		t1 = 1.0f;
	if (t1 <= t0)
		return 0.0f;
	length = sqrtf(qa);
	return (t1 - t0) * length;
}

/* whether the volume of this age at this centre blocks the sight from a to b */
static inline int ae_smoke_blocks_at(long age, float const *centre, float const *a, float const *b)
{
	return ae_smoke_chord(a, b, centre, ae_smoke_radius_at(age)) >= AE_SMOKE_MIN_CHORD;
}

/* the smoke's own projectile: the name of the grenade the AE smoke takes the place of in the map (the plasma
grenade's: SEARCH AND DESTROY's classes give that slot) */
#define AE_SMOKE_PROJECTILE_TAG "weapons\\plasma grenade\\plasma grenade"
#define AE_SMOKE_MODEL_TAG "weapons\\frag grenade\\frag grenade"
#define AE_SMOKE_PACK_ID "ae_smoke"
#define AE_SMOKE_PACK_PREFIX "ae\\packs\\smoke\\"
#define AE_SMOKE_EFFECT_NAME AE_SMOKE_PACK_PREFIX "cloud"

#endif
