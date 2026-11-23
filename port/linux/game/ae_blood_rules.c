/*
AE_BLOOD_RULES.C

Dynamic blood's pure rules (ae_blood_rules.h).
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ae_blood_rules.h"

static float pin(float value, float low, float high)
{
	return value < low ? low : value > high ? high : value;
}

int ae_blood_surface_runs(float normal_k)
{
	return (float)fabs(normal_k) < AE_BLOOD_STEEP;
}

int ae_blood_surface_ends(float normal_k)
{
	if ((float)fabs(normal_k) < AE_BLOOD_LEVEL)
		return 0;
	/* (an underside: it drops; a floor: it pools) */
	return normal_k < -0.3f ? 2 : 1;
}

int ae_blood_rule_downhill(const float normal[3], float down[3])
{
	float vector[3];
	float length;

	vector[0] = normal[0] * normal[2];
	vector[1] = normal[1] * normal[2];
	vector[2] = -1.f + normal[2] * normal[2];
	length = (float)sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
	if (!(length > 1e-6f) || !(length < 1e30f))
		return 0;
	down[0] = vector[0] / length;
	down[1] = vector[1] / length;
	down[2] = vector[2] / length;
	return 1;
}

void ae_blood_run_start(struct ae_blood_run *run, float radius, float length_scale, float width_scale,
	float metres_a_second, float thickness, float length_setting)
{
	run->width = pin(radius * width_scale, 0.003f, 0.035f * thickness);
	run->start_width = run->width;
	run->length = pin(radius * length_scale, 0.02f, 0.6f) * pin(length_setting, 0.f, 5.f);
	run->remaining = run->length;
	run->age = 0;
	run->speed = metres_a_second / (AE_BLOOD_METRES * 30.f);
	/* (slowing so it comes to rest about its length down: a run of speed / (1 - slowing)) */
	run->slowing = pin(1.f - run->speed / (run->length > 1e-4f ? run->length : 1e-4f), 0.85f, 0.99f);
}

int ae_blood_run_step(struct ae_blood_run *run)
{
	run->remaining -= run->speed;
	run->speed *= run->slowing;
	run->age++;
	/* (all but run out: it stops, in a few seconds at most) */
	if (run->remaining < run->length * 0.12f || run->speed < 0.0004f || run->age > 6 * 30)
		run->remaining = 0.f;
	return run->remaining > 0.f;
}

float ae_blood_run_share(const struct ae_blood_run *run)
{
	return pin(run->remaining / (run->length > 1e-4f ? run->length : 1e-4f), 0.f, 1.f);
}

float ae_blood_run_half_width(const struct ae_blood_run *run)
{
	return run->width * (0.45f + 0.55f * ae_blood_run_share(run)) * 0.5f;
}

float ae_blood_run_pool_radius(const struct ae_blood_run *run)
{
	float share = ae_blood_run_share(run);

	return share < 0.1f ? 0.f : run->start_width * (1.2f + 2.5f * share);
}

int ae_blood_name_is_blood(const char *name)
{
	return name && strstr(name, "blood") != NULL;
}

int ae_blood_two_sided_takes(const char *decal_name, int dynamic_blood_on)
{
	return dynamic_blood_on && ae_blood_name_is_blood(decal_name);
}

int ae_blood_splat_names(const char *effect_name, char *splat, char *large, char *small, unsigned long size)
{
	const char *word;
	char species[64] = "";

	splat[0] = large[0] = small[0] = '\0';
	if (!ae_blood_name_is_blood(effect_name) || size == 0)
		return 0;
	word = strstr(effect_name, "aoe ");
	if (word)
		snprintf(species, sizeof(species), "%s", word + 4);
	if (!species[0] || !strcmp(species, "human"))
	{
		snprintf(splat, size, "effects\\decals\\blood splats\\blood splat");
		snprintf(large, size, "effects\\decals\\blood splats\\blood splat large");
		snprintf(small, size, "effects\\decals\\blood splats\\blood splat small");
	}
	else
	{
		/* (no large splat of a species': the splat itself) */
		snprintf(splat, size, "effects\\decals\\blood splats\\blood %s splat", species);
		snprintf(small, size, "effects\\decals\\blood splats\\blood splat %s small", species);
	}
	return 1;
}

void ae_blood_viscosity(const char *decal_name, float *speed_scale, float *thickness_scale)
{
	*speed_scale = 1.f;
	*thickness_scale = 1.f;
	if (!decal_name)
		return;
	if (strstr(decal_name, "elite") || strstr(decal_name, "jackal"))
		*speed_scale = 1.5f;
	else if (strstr(decal_name, "grunt"))
	{
		*speed_scale = 0.65f;
		*thickness_scale = 1.5f;
	}
}

const char *ae_blood_species(const char *name)
{
	static const char *const species[] = { "elite", "grunt", "jackal", "hunter", "engineer", "flood" };
	unsigned int index;

	for (index = 0; name && index < sizeof(species) / sizeof(species[0]); index++)
	{
		if (strstr(name, species[index]))
			return species[index];
	}
	return NULL;
}

int ae_blood_particle_score(const char *particle_name, const char *species)
{
	const char *its;
	int score;

	if (!ae_blood_name_is_blood(particle_name) || strstr(particle_name, "mist") || strstr(particle_name, "glow"))
		return -1;
	its = ae_blood_species(particle_name);
	if (species ? (!its || strcmp(its, species)) : (its && !strstr(particle_name, "blood h ")))
		return -1;
	score = strstr(particle_name, "burst") ? 3 : strstr(particle_name, "impact") ? 2 : 1;
	if (!species && (strstr(particle_name, "human") || strstr(particle_name, "blood h ")))
		score += 2;
	return score;
}
