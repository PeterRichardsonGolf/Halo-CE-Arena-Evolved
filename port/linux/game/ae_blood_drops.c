/*
AE_BLOOD_DROPS.C

Dynamic blood's falling drops drawn (ae_blood.h): drops hanging from edges and undersides, then falling, as ae_blood.c
runs them. Drawn as sprites with the map's own blood particle of the same species as the run's splat (an Elite's, a
Grunt's, a human's...), tinted the colour of that splat, in the particles' pass (render_particles.c's AE hook) after
the game's particles. Nothing here is simulated: only drawn.
*/

#include "cseries.h"
#include "math/real_math.h"
#include "effects/particles.h"
#include "effects/decals.h"
#include "render/render_sprite.h"
#include "render/render_particles.h"
#include "tag_files/tag_files.h"
#include "cache/cache_files.h"
#include "game/game.h"

#include "ae_blood.h"
#include "ae_blood_rules.h"

enum
{
	AE_BLOOD_DROPS_MAXIMUM = 96,
	AE_BLOOD_DROPS_KINDS = 8,
};

/* each splat drawn so far this map: the particle its drops are drawn with, and their tint */
static struct
{
	long decal;
	long particle;
	real_argb_color color;
} ae_blood_drop_kinds[AE_BLOOD_DROPS_KINDS];
static short ae_blood_drop_kind_next = 0;
static long ae_blood_drops_time = NONE;

/* the map's blood particle of a species (NULL: a human's), the most solid of them: a burst's, else an impact's, else
any */
static long ae_blood_drops_particle(char const *species)
{
	struct tag_iterator iterator;
	long tag_index;
	long best = NONE;
	int best_score = -1;

	tag_iterator_new(&iterator, PARTICLE_TAG);
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		int score = ae_blood_particle_score(tag_get_name(tag_index), species);
		struct particle_definition *definition;

		if (score <= best_score)
			continue;
		definition = particle_definition_get(tag_index);
		if (!definition || definition->bitmap.index == NONE || definition->bitmap.index == 0)
			continue;
		best = tag_index;
		best_score = score;
	}
	return best;
}

static short ae_blood_drops_kind(long decal)
{
	long now = game_time_get();
	short index;

	if (ae_blood_drops_time == NONE || now < ae_blood_drops_time)
	{
		for (index = 0; index < AE_BLOOD_DROPS_KINDS; index++)
			ae_blood_drop_kinds[index].decal = NONE;
	}
	ae_blood_drops_time = now;
	for (index = 0; index < AE_BLOOD_DROPS_KINDS; index++)
	{
		if (ae_blood_drop_kinds[index].decal == decal)
			return index;
	}
	index = ae_blood_drop_kind_next;
	ae_blood_drop_kind_next = (short)((ae_blood_drop_kind_next + 1) % AE_BLOOD_DROPS_KINDS);
	{
		char const *species = ae_blood_species(tag_get_name(decal));
		real_rgb_color rgb;
		real brightest;

		ae_blood_drop_kinds[index].decal = decal;
		ae_blood_drop_kinds[index].particle = ae_blood_drops_particle(species);
		if (ae_blood_drop_kinds[index].particle == NONE && species)
			ae_blood_drop_kinds[index].particle = ae_blood_drops_particle(NULL);
		/* (the splat's colour: a tinted splat's tint; an untinted one's, white, leaves the particle's own colours;
		either darkened, wet blood, not lit spray: the particles' shaders add light) */
		ae_blood_decal_color(decal, &rgb);
		brightest = MAX(rgb.red, MAX(rgb.green, rgb.blue));
		if (brightest <= 0.f)
			rgb.red = rgb.green = rgb.blue = 1.f;
		ae_blood_drop_kinds[index].color.alpha = 1.f;
		ae_blood_drop_kinds[index].color.red = rgb.red * 0.28f;
		ae_blood_drop_kinds[index].color.green = rgb.green * 0.28f;
		ae_blood_drop_kinds[index].color.blue = rgb.blue * 0.28f;
	}
	return index;
}

void ae_blood_drops_render(void)
{
	static real_point3d positions[AE_BLOOD_DROPS_MAXIMUM];
	static real_vector3d directions[AE_BLOOD_DROPS_MAXIMUM];
	static real radii[AE_BLOOD_DROPS_MAXIMUM];
	static long decals[AE_BLOOD_DROPS_MAXIMUM];
	static short kinds[AE_BLOOD_DROPS_MAXIMUM];
	short count;
	short index;
	short kind;

	if (!render_particles_enabled)
		return;
	count = ae_blood_drops_for_drawing(positions, directions, radii, decals, AE_BLOOD_DROPS_MAXIMUM);
	if (count <= 0)
		return;
	for (index = 0; index < count; index++)
		kinds[index] = ae_blood_drops_kind(decals[index]);

	/* (each kind's drops together: one particle's picture and shader) */
	for (kind = 0; kind < AE_BLOOD_DROPS_KINDS; kind++)
	{
		struct particle_definition *definition;
		struct build_sprite_data sprite_data;
		short kind_count = 0;
		real total_radius = 0.f;

		for (index = 0; index < count; index++)
		{
			if (kinds[index] == kind)
				kind_count++;
		}
		if (!kind_count || ae_blood_drop_kinds[kind].particle == NONE)
			continue;
		definition = particle_definition_get(ae_blood_drop_kinds[kind].particle);
		if (!definition || definition->bitmap.index == NONE || definition->bitmap.index == 0)
			continue;
		build_sprites_begin(&sprite_data, kind_count, definition->bitmap.index, &definition->shader, 0);
		for (index = 0; index < count; index++)
		{
			real speed_squared;
			boolean falling;

			if (kinds[index] != kind)
				continue;
			speed_squared = directions[index].i * directions[index].i + directions[index].j * directions[index].j +
				directions[index].k * directions[index].k;
			/* (falling fast: along its way; else round, facing you) */
			falling = directions[index].k < -0.004f && speed_squared > 1e-5f;
			build_sprite(&sprite_data, falling ? _build_sprite_parallel : _build_sprite_normal,
				definition->first_sequence_index > 0 ? definition->first_sequence_index : 0, 0,
				&positions[index], &directions[index], 0.f, radii[index] * 2.f * definition->runtime_oo_width,
				&ae_blood_drop_kinds[kind].color, 1.f, 0);
			total_radius += radii[index];
		}
		((struct shader_effect_definition *)sprite_data.shader)->secondary_map_radius = total_radius / kind_count;
		build_sprites_end(&sprite_data);
	}
}
