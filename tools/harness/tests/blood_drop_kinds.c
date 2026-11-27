#include "harness.h"
/* ae_blood_drops.c's drop kinds, kept by decal tag index with the particle chosen for it: another map's tags (the
tag table changed) start them over even when the game clock went on, so a decal index of the new map is never drawn
with the previous map's particle index */

#define MAX(a, b) ((a) > (b) ? (a) : (b))
typedef struct { real red, green, blue; } real_rgb_color;
typedef struct { real alpha, red, green, blue; } real_argb_color;
static long now_ticks, generation, particle_of_map;
static int particle_lookups;
static long game_time_get(void) { return now_ticks; }
static unsigned long ae_tag_table_generation(void) { return (unsigned long)generation; }
static char const *tag_get_name(long tag_index) { return "decal"; }
static char const *ae_blood_species(char const *name) { return NULL; }
static long ae_blood_drops_particle(char const *species) { particle_lookups++; return particle_of_map; }
static void ae_blood_decal_color(long decal, real_rgb_color *rgb) { rgb->red = rgb->green = rgb->blue = 1.f; }
#include "under_test.inc"

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	short kind;

	CASE("new-map")
	{
		now_ticks = 100;
		particle_of_map = 40;
		kind = ae_blood_drops_kind(7);
		CHECK(ae_blood_drop_kinds[kind].particle == 40, "first map's particle %ld", ae_blood_drop_kinds[kind].particle);
		/* another map: the same decal index, the clock later than the last drop's */
		generation++;
		now_ticks = 900;
		particle_of_map = 55;
		kind = ae_blood_drops_kind(7);
		CHECK(ae_blood_drop_kinds[kind].particle == 55, "the previous map's particle %ld kept",
			ae_blood_drop_kinds[kind].particle);
		/* the same map: kept */
		now_ticks = 901;
		kind = ae_blood_drops_kind(7);
		CHECK(particle_lookups == 2, "%d lookups for 2 maps", particle_lookups);
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
