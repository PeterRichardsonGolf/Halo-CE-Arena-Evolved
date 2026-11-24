#include "harness.h"
/* ae_shield_fx.c's early-pop table (32 units whose pop was shown early, freed when the shield recharges): a unit that
dies or goes before it recharges gives its slot up to a new unit, and a new map or game forgets every unit kept */

enum { _object_dead_bit = 3, AE_SHIELD_FX_MODELS = 8 };
struct object_fields { unsigned long damage_flags; };
struct unit_datum { struct object_fields object; };
static struct unit_datum units[64];
static boolean unit_live[64];
static int pops;
static struct { long model_index; real bottom, top; } ae_shield_fx_models[AE_SHIELD_FX_MODELS];
static short ae_shield_fx_next_model;
static struct unit_datum *unit_try_and_get(long index)
{
	return index >= 0 && index < 64 && unit_live[index] ? &units[index] : NULL;
}
static real ae_shield_fx_pop_at(void) { return 0.3f; }
static void ae_shield_fx_pop(long unit_index) { pops++; }
#include "under_test.inc"

int main(int argc, char **argv)
{
	char const *case_name = argv[1];
	long index;

	for (index = 0; index < 64; index++)
		unit_live[index] = TRUE;
	CASE("dead-units-free-slots")
	{
		/* 32 units pop early and die before their shields recharge */
		for (index = 0; index < 32; index++)
		{
			ae_shield_fx_pop_early(index);
			units[index].object.damage_flags |= 1UL << _object_dead_bit;
		}
		ae_shield_fx_pop_early(40);
		CHECK(ae_shield_fx_pop_was_early(40), "the 33rd unit not kept: its next hits pop again");
		ae_shield_fx_pop_early(40);
		CHECK(pops == 33, "%d pops for 33 units", pops);
		return 0;
	}
	CASE("gone-units-free-slots")
	{
		for (index = 0; index < 32; index++)
			ae_shield_fx_pop_early(index);
		unit_live[7] = FALSE;
		ae_shield_fx_pop_early(41);
		CHECK(ae_shield_fx_pop_was_early(41), "a deleted unit's slot not given up");
		CHECK(ae_shield_fx_pop_was_early(8), "a live unit's slot given up");
		return 0;
	}
	CASE("live-units-keep-slots")
	{
		for (index = 0; index < 32; index++)
			ae_shield_fx_pop_early(index);
		ae_shield_fx_pop_early(42);
		for (index = 0; index < 32; index++)
			CHECK(ae_shield_fx_pop_was_early(index), "live unit %ld lost its slot", index);
		return 0;
	}
	CASE("new-map")
	{
		ae_shield_fx_pop_early(5);
		ae_shield_fx_models[2].model_index = 17;
		ae_shield_fx_next_model = 3;
		ae_shield_fx_new_map();
		CHECK(!ae_shield_fx_pop_was_early(5), "the previous map's unit still kept as popped");
		CHECK(ae_shield_fx_models[2].model_index == NONE && ae_shield_fx_next_model == 0,
			"the previous map's model heights kept");
		return 0;
	}
	fprintf(stderr, "unknown case %s\n", case_name);
	return 2;
}
