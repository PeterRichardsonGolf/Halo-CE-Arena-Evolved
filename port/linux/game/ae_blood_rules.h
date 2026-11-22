/*
AE_BLOOD_RULES.H

Dynamic blood's pure rules (ae_blood.c runs them on the game's blood): which surfaces blood runs down, the way down a
surface, a run's speed, thinning and stop, and which of the game's tags are blood, by their names. No game headers:
port/linux/tests/ae_blood_rules_test.c builds it alone. Units: world units (1 = 3.048 m) and ticks (1/30 s).
*/

#ifndef AE_BLOOD_RULES_H
#define AE_BLOOD_RULES_H

/* a surface this steep (its normal's height under this, either way) runs */
#define AE_BLOOD_STEEP 0.9f
/* a run's surface this level (its normal's height this or more, either way) ends it */
#define AE_BLOOD_LEVEL (AE_BLOOD_STEEP + 0.15f)
/* (metres in a world unit) */
#define AE_BLOOD_METRES 3.048f

/* blood landing on a surface of this normal height runs down it */
int ae_blood_surface_runs(float normal_k);
/* a run's surface turned level: what is left pools (1), or, on an underside, drops (2); 0 it runs on */
int ae_blood_surface_ends(float normal_k);
/* down a surface: gravity along it, unit length; 0 (down left alone) on a level one */
int ae_blood_rule_downhill(const float normal[3], float down[3]);

/* a run of blood down a surface */
struct ae_blood_run
{
	float speed;     /* world units a tick */
	float slowing;   /* its speed kept each tick */
	float remaining; /* world units left to run */
	float length;    /* world units it runs in all */
	float width;     /* world units across */
	float start_width;
	short age;       /* ticks */
};

/* a run from a splat of this radius: its length and width the splat's times these, its speed in metres a second,
thickness a kind of blood's (ae_blood_viscosity), length_setting how long runs are (1 as they are, 0..5) */
void ae_blood_run_start(struct ae_blood_run *run, float radius, float length_scale, float width_scale,
	float metres_a_second, float thickness, float length_setting);
/* a tick of a run: slowing, its length running out; 0 once it has stopped (run out, slowed to nothing, or 6 s) */
int ae_blood_run_step(struct ae_blood_run *run);
/* how much of it is left, 0..1 */
float ae_blood_run_share(const struct ae_blood_run *run);
/* its half width now: thinning as it runs out, to under half at its end */
float ae_blood_run_half_width(const struct ae_blood_run *run);
/* the radius of what pools where its surface turns level; 0: too little left to pool */
float ae_blood_run_pool_radius(const struct ae_blood_run *run);

/* a tag's name is blood's ("effects\decals\blood splats\blood splat", "effects\blood aoe elite") */
int ae_blood_name_is_blood(const char *name);
/* blood lands on two-sided surfaces (a tree, a fence) when dynamic blood is on and the decal is blood's; the game's
other decals never do */
int ae_blood_two_sided_takes(const char *decal_name, int dynamic_blood_on);
/* a character's splats by its collision model's area damage effect's name ("effects\blood aoe elite"): the three
decal names under effects\decals\blood splats\ (large and small may fall back to the splat); 0 when the effect is no
blood (a sentinel, a robot) */
int ae_blood_splat_names(const char *effect_name, char *splat, char *large, char *small, unsigned long size);
/* how a kind of blood runs, by its decal's name: an Elite's and a Jackal's half again as fast, a Grunt's at two thirds
the speed and half again as thick */
void ae_blood_viscosity(const char *decal_name, float *speed_scale, float *thickness_scale);
/* the species a blood tag's name says (elite, grunt, jackal, hunter, engineer, flood), NULL: a human's or none */
const char *ae_blood_species(const char *name);
/* how well a particle tag of this name draws a falling drop of a species' blood (NULL: a human's): -1 not at all, else
higher is better (a burst's over an impact's over any) */
int ae_blood_particle_score(const char *particle_name, const char *species);

#endif
