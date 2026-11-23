#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ae_blood_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabs((double)(a) - (double)(b)) < 1e-4)

/* a run started as ae_blood.c starts one, stepped to its stop: the ticks it ran, the distance it ran */
static int run_to_stop(struct ae_blood_run *run, float *distance)
{
	int ticks = 0;

	*distance = 0.f;
	do
	{
		*distance += run->speed;
		ticks++;
	} while (ae_blood_run_step(run) && ticks < 1000);
	return ticks;
}

int main(void)
{
	/* which surfaces run: walls and steep slopes, either way up; not floors, not ceilings */
	{
		CHECK(ae_blood_surface_runs(0.f));     /* a wall */
		CHECK(ae_blood_surface_runs(0.5f));    /* a steep slope */
		CHECK(ae_blood_surface_runs(-0.5f));   /* an overhang */
		CHECK(ae_blood_surface_runs(0.89f));
		CHECK(!ae_blood_surface_runs(0.9f));   /* a gentle slope: blood stays as it lands */
		CHECK(!ae_blood_surface_runs(1.f));    /* a floor */
		CHECK(!ae_blood_surface_runs(-1.f));   /* a ceiling */
		/* (a run on its way: it goes on over any surface its downhill has a way down; a level one ends it, which
		ae_blood.c finds by the downhill's failing: pooling on a floor, a drop from an underside) */
		CHECK(!ae_blood_surface_ends(0.f) && !ae_blood_surface_ends(0.95f) && !ae_blood_surface_ends(-0.95f));
		CHECK(AE_BLOOD_LEVEL > AE_BLOOD_STEEP);
	}

	/* the way down a surface: gravity along it, unit length, at right angles to the normal, always down */
	{
		static const float normals[][3] = { { 1.f, 0.f, 0.f }, { 0.f, -1.f, 0.f }, { 0.6f, 0.f, 0.8f },
			{ 0.f, 0.6f, -0.8f }, { 0.48f, 0.64f, 0.6f }, { 0.995f, 0.f, 0.0998749f } };
		unsigned int index;

		for (index = 0; index < sizeof(normals) / sizeof(normals[0]); index++)
		{
			float down[3];
			const float *n = normals[index];

			CHECK(ae_blood_rule_downhill(n, down));
			CHECK(NEAR(down[0] * down[0] + down[1] * down[1] + down[2] * down[2], 1.f));
			CHECK(NEAR(down[0] * n[0] + down[1] * n[1] + down[2] * n[2], 0.f));
			CHECK(down[2] < 0.f);
		}
		/* (a wall's: straight down) */
		{
			float down[3];

			CHECK(ae_blood_rule_downhill(normals[0], down) && NEAR(down[2], -1.f));
		}
		/* (a floor and a ceiling: no way down, the drip pools or drops) */
		{
			static const float floor[3] = { 0.f, 0.f, 1.f }, ceiling[3] = { 0.f, 0.f, -1.f };
			float down[3] = { 7.f, 7.f, 7.f };

			CHECK(!ae_blood_rule_downhill(floor, down) && down[0] == 7.f);
			CHECK(!ae_blood_rule_downhill(ceiling, down));
		}
	}

	/* a run: its width and length from its splat, its speed in world units a tick, slowing to a stop about its length
	down, thinning as it goes, never running for ever */
	{
		struct ae_blood_run run;
		float distance;
		int ticks;

		/* (a 0.1-unit splat, twice as long, a tenth as wide, 0.5 m/s, human blood) */
		ae_blood_run_start(&run, 0.1f, 2.f, 0.1f, 0.5f, 1.f, 1.f);
		CHECK(NEAR(run.width, 0.01f) && NEAR(run.start_width, 0.01f));
		CHECK(NEAR(run.length, 0.2f) && NEAR(run.remaining, 0.2f));
		CHECK(NEAR(run.speed, 0.5f / (3.048f * 30.f)));
		CHECK(run.slowing >= 0.85f && run.slowing <= 0.99f);
		CHECK(NEAR(ae_blood_run_share(&run), 1.f));
		CHECK(NEAR(ae_blood_run_half_width(&run), 0.005f));
		ticks = run_to_stop(&run, &distance);
		CHECK(ticks > 5 && ticks <= 181);
		/* (it stops short of its whole length, with a little left: under 0.12 of it, or slowed to nothing) */
		CHECK(distance > 0.05f && distance <= 0.2f + 1e-4f);
		CHECK(run.remaining == 0.f);
		/* (thinned: under half its width at its end) */
		CHECK(NEAR(ae_blood_run_half_width(&run), 0.01f * 0.45f * 0.5f));
		CHECK(ae_blood_run_pool_radius(&run) == 0.f);

		/* (the speed it keeps: each tick slower) */
		ae_blood_run_start(&run, 0.1f, 2.f, 0.1f, 0.5f, 1.f, 1.f);
		{
			float before = run.speed;

			ae_blood_run_step(&run);
			CHECK(run.speed < before && NEAR(run.speed, before * run.slowing));
			CHECK(run.age == 1);
			/* (half run: what pools is bigger than the run is wide) */
			run.remaining = run.length * 0.5f;
			CHECK(NEAR(ae_blood_run_pool_radius(&run), 0.01f * (1.2f + 2.5f * 0.5f)));
		}

		/* (a fast run slows hard, a slow one hardly: slowing pinned to 0.85..0.99) */
		ae_blood_run_start(&run, 0.01f, 1.f, 0.1f, 5.f, 1.f, 1.f);
		CHECK(NEAR(run.slowing, 0.85f));
		ae_blood_run_start(&run, 0.3f, 2.f, 0.1f, 0.05f, 1.f, 1.f);
		CHECK(NEAR(run.slowing, 0.99f));
		/* (6 s at most, however slow it goes) */
		run.slowing = 1.f;
		run.speed = 0.0005f;
		run.length = run.remaining = 100.f;
		ticks = run_to_stop(&run, &distance);
		CHECK(ticks == 181);

		/* (widths pinned: a hair at least, a Grunt's thick blood thicker) */
		ae_blood_run_start(&run, 0.001f, 1.f, 0.1f, 0.5f, 1.f, 1.f);
		CHECK(NEAR(run.width, 0.003f) && NEAR(run.length, 0.02f));
		ae_blood_run_start(&run, 1.f, 1.f, 1.f, 0.5f, 1.f, 1.f);
		CHECK(NEAR(run.width, 0.035f) && NEAR(run.length, 0.6f));
		ae_blood_run_start(&run, 1.f, 1.f, 1.f, 0.5f, 1.5f, 1.f);
		CHECK(NEAR(run.width, 0.035f * 1.5f));
		/* (the length setting: 0 none, at most five times) */
		ae_blood_run_start(&run, 0.1f, 2.f, 0.1f, 0.5f, 1.f, 9.f);
		CHECK(NEAR(run.length, 1.f));
		ae_blood_run_start(&run, 0.1f, 2.f, 0.1f, 0.5f, 1.f, 0.f);
		CHECK(run.length == 0.f && !ae_blood_run_step(&run));
	}

	/* the tags by name: blood or not, two-sided surfaces only for blood with dynamic blood on */
	{
		CHECK(ae_blood_name_is_blood("effects\\decals\\blood splats\\blood splat"));
		CHECK(ae_blood_name_is_blood("effects\\blood aoe elite"));
		CHECK(!ae_blood_name_is_blood("effects\\decals\\bullet holes\\metal thin"));
		CHECK(!ae_blood_name_is_blood("effects\\decals\\scorch marks\\plasma scorch"));
		CHECK(!ae_blood_name_is_blood(NULL));
		CHECK(ae_blood_two_sided_takes("effects\\decals\\blood splats\\blood elite splat", 1));
		CHECK(!ae_blood_two_sided_takes("effects\\decals\\blood splats\\blood elite splat", 0));
		CHECK(!ae_blood_two_sided_takes("effects\\decals\\bullet holes\\wood", 1));
		CHECK(!ae_blood_two_sided_takes(NULL, 1));
	}

	/* a character's splats by its area damage effect's name */
	{
		char splat[128], large[128], small[128];

		CHECK(ae_blood_splat_names("effects\\blood aoe human", splat, large, small, sizeof(splat)));
		CHECK(!strcmp(splat, "effects\\decals\\blood splats\\blood splat"));
		CHECK(!strcmp(large, "effects\\decals\\blood splats\\blood splat large"));
		CHECK(!strcmp(small, "effects\\decals\\blood splats\\blood splat small"));
		/* (no species word: a human's) */
		CHECK(ae_blood_splat_names("effects\\blood", splat, large, small, sizeof(splat)) &&
			!strcmp(splat, "effects\\decals\\blood splats\\blood splat"));
		CHECK(ae_blood_splat_names("effects\\blood aoe elite", splat, large, small, sizeof(splat)));
		CHECK(!strcmp(splat, "effects\\decals\\blood splats\\blood elite splat"));
		CHECK(large[0] == '\0');
		CHECK(!strcmp(small, "effects\\decals\\blood splats\\blood splat elite small"));
		CHECK(ae_blood_splat_names("effects\\blood aoe grunt", splat, large, small, sizeof(splat)) &&
			!strcmp(splat, "effects\\decals\\blood splats\\blood grunt splat"));
		/* (no blood: a sentinel's, a robot's) */
		CHECK(!ae_blood_splat_names("effects\\sparks aoe sentinel", splat, large, small, sizeof(splat)) &&
			!splat[0] && !large[0] && !small[0]);
		CHECK(!ae_blood_splat_names(NULL, splat, large, small, sizeof(splat)));
		/* (a short buffer: cut, never overrun) */
		{
			char tiny[8], tiny_large[8], tiny_small[8];

			CHECK(ae_blood_splat_names("effects\\blood aoe elite", tiny, tiny_large, tiny_small, sizeof(tiny)) &&
				strlen(tiny) == 7);
		}
	}

	/* how each kind of blood runs */
	{
		float speed, thickness;

		ae_blood_viscosity("effects\\decals\\blood splats\\blood splat", &speed, &thickness);
		CHECK(speed == 1.f && thickness == 1.f);
		ae_blood_viscosity("effects\\decals\\blood splats\\blood elite splat", &speed, &thickness);
		CHECK(speed == 1.5f && thickness == 1.f);
		ae_blood_viscosity("effects\\decals\\blood splats\\blood jackal splat", &speed, &thickness);
		CHECK(speed == 1.5f && thickness == 1.f);
		ae_blood_viscosity("effects\\decals\\blood splats\\blood grunt splat", &speed, &thickness);
		CHECK(speed == 0.65f && thickness == 1.5f);
		ae_blood_viscosity(NULL, &speed, &thickness);
		CHECK(speed == 1.f && thickness == 1.f);
	}

	/* the drops' particles: a species' own, the most solid of them; a human's never another species' */
	{
		CHECK(ae_blood_species("effects\\decals\\blood splats\\blood elite splat") != NULL &&
			!strcmp(ae_blood_species("effects\\decals\\blood splats\\blood elite splat"), "elite"));
		CHECK(ae_blood_species("effects\\decals\\blood splats\\blood splat") == NULL);
		CHECK(ae_blood_species(NULL) == NULL);
		CHECK(ae_blood_particle_score("effects\\particles\\blood\\elite blood burst", "elite") == 3);
		CHECK(ae_blood_particle_score("effects\\particles\\blood\\elite blood impact", "elite") == 2);
		CHECK(ae_blood_particle_score("effects\\particles\\blood\\elite blood drip", "elite") == 1);
		CHECK(ae_blood_particle_score("effects\\particles\\blood\\grunt blood burst", "elite") == -1);
		CHECK(ae_blood_particle_score("effects\\particles\\blood\\elite blood mist", "elite") == -1);
		CHECK(ae_blood_particle_score("effects\\particles\\blood\\elite blood glow", "elite") == -1);
		CHECK(ae_blood_particle_score("effects\\particles\\sparks\\metal burst", NULL) == -1);
		CHECK(ae_blood_particle_score("effects\\particles\\blood\\elite blood burst", NULL) == -1);
		CHECK(ae_blood_particle_score("effects\\particles\\blood\\human blood burst", NULL) == 5);
		CHECK(ae_blood_particle_score("effects\\particles\\blood\\blood burst", NULL) == 3);
		CHECK(ae_blood_particle_score(NULL, NULL) == -1);
	}

	if (failures)
		printf("%d failures\n", failures);
	else
		printf("ae_blood_rules: all passed\n");
	return failures ? 1 : 0;
}
