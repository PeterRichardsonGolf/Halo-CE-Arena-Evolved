/*
AE_TRAITS_RULES.C

The player traits' table and arithmetic (ae_traits_rules.h).
*/

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "ae_traits_rules.h"
#include "ae_variant_ext_rules.h"

/* (game_variant_options.jump_percent's offset, which tools/ae_gametype_layout_check.c pins) */
#define AE_TRAITS_JUMP_OPTIONS_OFFSET 0x27

static const struct ae_trait_rule rules[AE_TRAIT_COUNT] =
{
	/* (the jump's speed, 50 to 200 percent: ae_jump_rules.h) */
	[AE_TRAIT_JUMP_HEIGHT] = { "JUMP HEIGHT", AE_TRAIT_IN_OPTIONS, AE_TRAIT_JUMP_SPEED_PERCENT,
		AE_TRAITS_JUMP_OPTIONS_OFFSET, 50, 200 },
	[AE_TRAIT_GRAVITY] = { "GRAVITY", AE_TRAIT_IN_EXT, AE_TRAIT_PERCENT,
		(unsigned short)offsetof(struct ae_variant_ext, gravity_percent), 10, 250 },
	[AE_TRAIT_SPEED] = { "PLAYER SPEED", AE_TRAIT_IN_EXT, AE_TRAIT_PERCENT,
		(unsigned short)offsetof(struct ae_variant_ext, speed_percent), 10, 250 },
};

const struct ae_trait_rule *ae_trait_rule(int trait)
{
	return trait >= 0 && trait < AE_TRAIT_COUNT ? &rules[trait] : NULL;
}

static int stored_in_range(const struct ae_trait_rule *rule, unsigned char stored)
{
	return stored && stored >= rule->minimum && stored <= rule->maximum;
}

int ae_trait_percent(int trait, unsigned char stored)
{
	const struct ae_trait_rule *rule = ae_trait_rule(trait);

	if (!rule || !stored_in_range(rule, stored))
		return 100;
	if (rule->kind == AE_TRAIT_JUMP_SPEED_PERCENT)
		return (stored * stored + 50) / 100;
	return stored;
}

unsigned char ae_trait_stored(int trait, int percent)
{
	const struct ae_trait_rule *rule = ae_trait_rule(trait);
	long value;

	if (!rule || percent == 100 || percent <= 0)
		return 0;
	value = rule->kind == AE_TRAIT_JUMP_SPEED_PERCENT ? lround(10.0 * sqrt((double)percent)) : percent;
	if (value < rule->minimum)
		value = rule->minimum;
	if (value > rule->maximum)
		value = rule->maximum;
	return value == 100 ? 0 : (unsigned char)value;
}

float ae_trait_multiplier(int trait, unsigned char stored)
{
	const struct ae_trait_rule *rule = ae_trait_rule(trait);

	if (!rule || !stored_in_range(rule, stored))
		return 1.0f;
	return stored / 100.0f;
}

float ae_traits_jump_rise(float jump_velocity, float jump_multiplier, float gravity, float gravity_multiplier)
{
	float speed = jump_velocity * jump_multiplier, pull = gravity * gravity_multiplier;

	if (!(speed > 0.0f) || !(pull > 0.0f))
		return 0.0f;
	return speed * speed / (2.0f * pull);
}

float ae_traits_jump_reach(float jump_multiplier, float gravity_multiplier)
{
	if (!(jump_multiplier > 0.0f) || !(gravity_multiplier > 0.0f))
		return 1.0f;
	return jump_multiplier * jump_multiplier / gravity_multiplier;
}

/* a build name's version: its three numbers and its release candidate (0 for a release, -1 for another
prerelease such as -beta); 0 when it is not Arena Evolved's */
static int build_version(const char *build, int part[3], int *candidate)
{
	static const char prefix[] = "Arena Evolved ";
	int at = 0, digits = 0;

	part[0] = part[1] = part[2] = 0;
	*candidate = 0;
	if (!build || strncmp(build, prefix, sizeof(prefix) - 1))
		return 0;
	build += sizeof(prefix) - 1;
	for (; *build && at < 3; build++)
	{
		if (*build >= '0' && *build <= '9')
		{
			part[at] = part[at] * 10 + (*build - '0');
			if (++digits > 4)
				return 0;
		}
		else if (*build == '.' && digits && at < 2)
		{
			at++;
			digits = 0;
		}
		else
			break;
	}
	if (at != 2 || !digits)
		return 0;
	if (*build == '-')
	{
		int number = 0;

		if (strncmp(build, "-rc", 3) || build[3] < '0' || build[3] > '9')
		{
			*candidate = -1;
			return 1;
		}
		for (build += 3; *build >= '0' && *build <= '9' && number < 1000; build++)
			number = number * 10 + (*build - '0');
		*candidate = number;
	}
	return 1;
}

int ae_traits_build_has_them(const char *build, const char *local_build)
{
	int part[3], candidate, index;
	static const int first[3] = { 1, 0, 0 };

	if (!build)
		return 0;
	if (local_build && *local_build && !strcmp(build, local_build))
		return 1;
	if (!build_version(build, part, &candidate))
		return 0;
	for (index = 0; index < 3; index++)
		if (part[index] != first[index])
			return part[index] > first[index];
	/* (1.0.0: the release, or its fifth release candidate on) */
	return candidate == 0 || candidate >= 5;
}
