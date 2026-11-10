/*
AE_FALL_DAMAGE.C

FALL DAMAGE HALO 2 (ae_fall_damage.h; the numbers and the gametype's byte: ae_fall_damage_rules.h). The map's game
globals have one falling damage block: the distances a landing starts to hurt at and hurts fully at, the distance a fall
kills at, and the three velocities the game works out from them. While the gametype says HALO 2 the block holds the
distances 6, 10 and 14 and their velocities (sqrt(2 * gravity * distance), as the map's own are made); the map's own go
back otherwise, so a CLASSIC gametype played after a HALO 2 one on the same map gets the map's numbers. The damage effects
are the map's. The bipeds read the block (bipeds.c: the landing, the fall that kills, the scream), the bots' drop checks
and the loose bomb's reach too, so they follow. Vehicles keep the map's own fall limit through
ae_fall_damage_vehicle_velocity (a map whose vehicles are meant to survive a long drop, a tank that walks down a cliff).
HALO_AE_FALL_DAMAGE=halo2 / classic forces the rule on / off for tests.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "scenario/scenario.h"
#include "ae_fall_damage.h"
#include "ae_fall_damage_rules.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

extern real global_gravity;

/* game_globals_falling_damage (bipeds.c, 0x98 bytes), the fields changed */
struct ae_fall_damage_block
{
	byte unused0[0x08];
	real falling_distance_lower_bound;
	real falling_distance_upper_bound;
	byte unused1[0x28 - 0x10];
	real maximum_distance;
	byte unused2[0x8C - 0x2C];
	real runtime_maximum_falling_velocity;
	real runtime_minimum_damage_velocity;
	real runtime_maximum_damage_velocity;
};

typedef char ae_fall_damage_block_size_assert[sizeof(struct ae_fall_damage_block) == 0x98 ? 1 : -1];

/* the numbers changed, as the map's globals have them */
struct ae_fall_damage_numbers
{
	real lower, upper, maximum;
	real maximum_falling_velocity, minimum_damage_velocity, maximum_damage_velocity;
};

static struct
{
	boolean have_stock;
	struct ae_fall_damage_numbers stock;
	boolean tuned;
	int force; /* HALO_AE_FALL_DAMAGE: AE_FALL_DAMAGE_HALO2 or _CLASSIC, or -1 unset */
} ae_fall_damage = { FALSE, { 0 }, FALSE, -1 };

static struct ae_fall_damage_block *ae_fall_damage_block(void)
{
	struct game_globals *globals = scenario_get_game_globals();

	if (!globals || globals->falling_damage.count < 1)
		return NULL;
	return TAG_BLOCK_GET_ELEMENT(&globals->falling_damage, 0, struct ae_fall_damage_block);
}

void ae_fall_damage_new_map(void)
{
	char const *force = getenv("HALO_AE_FALL_DAMAGE");

	ae_fall_damage.have_stock = FALSE;
	ae_fall_damage.tuned = FALSE;
	ae_fall_damage.force = -1;
	if (force && !strcmp(force, "halo2"))
		ae_fall_damage.force = AE_FALL_DAMAGE_HALO2;
	else if (force && !strcmp(force, "classic"))
		ae_fall_damage.force = AE_FALL_DAMAGE_CLASSIC;
}

static void ae_fall_damage_put(struct ae_fall_damage_block *block, struct ae_fall_damage_numbers const *numbers)
{
	block->falling_distance_lower_bound = numbers->lower;
	block->falling_distance_upper_bound = numbers->upper;
	block->maximum_distance = numbers->maximum;
	block->runtime_maximum_falling_velocity = numbers->maximum_falling_velocity;
	block->runtime_minimum_damage_velocity = numbers->minimum_damage_velocity;
	block->runtime_maximum_damage_velocity = numbers->maximum_damage_velocity;
}

void ae_fall_damage_update(void)
{
	struct ae_fall_damage_block *block;
	struct game_variant const *variant;
	boolean want;

	/* (the map's tags are loaded when the engine runs; the stock numbers are read before the first change) */
	block = ae_fall_damage_block();
	if (!block)
		return;
	if (!ae_fall_damage.have_stock)
	{
		ae_fall_damage.stock.lower = block->falling_distance_lower_bound;
		ae_fall_damage.stock.upper = block->falling_distance_upper_bound;
		ae_fall_damage.stock.maximum = block->maximum_distance;
		ae_fall_damage.stock.maximum_falling_velocity = block->runtime_maximum_falling_velocity;
		ae_fall_damage.stock.minimum_damage_velocity = block->runtime_minimum_damage_velocity;
		ae_fall_damage.stock.maximum_damage_velocity = block->runtime_maximum_damage_velocity;
		ae_fall_damage.have_stock = TRUE;
	}
	variant = game_engine_running() ? game_engine_get_variant() : NULL;
	want = variant && ae_fall_damage_of(TEST_FLAG(variant->universal_variant.flags, _game_variant_no_falling_damage_bit),
		variant->universal_variant.ball_melee) == AE_FALL_DAMAGE_HALO2;
	if (variant && ae_fall_damage.force >= 0)
		want = ae_fall_damage.force == AE_FALL_DAMAGE_HALO2;
	if (want == ae_fall_damage.tuned)
		return;
	if (want)
	{
		struct ae_fall_damage_numbers tuned = ae_fall_damage.stock;

		tuned.lower = AE_FALL_DAMAGE_LOWER;
		tuned.upper = AE_FALL_DAMAGE_UPPER;
		tuned.maximum = AE_FALL_DAMAGE_MAXIMUM;
		tuned.minimum_damage_velocity = ae_fall_damage_velocity(global_gravity, tuned.lower);
		tuned.maximum_damage_velocity = ae_fall_damage_velocity(global_gravity, tuned.upper);
		tuned.maximum_falling_velocity = ae_fall_damage_velocity(global_gravity, tuned.maximum);
		ae_fall_damage_put(block, &tuned);
		error(_error_silent, "ae fall damage: HALO 2 (hurts from %.1f, fully at %.1f, kills past %.1f world units; "
			"velocities %.4f %.4f %.4f; the map's %.1f %.1f %.1f)", (double)tuned.lower, (double)tuned.upper,
			(double)tuned.maximum, (double)tuned.minimum_damage_velocity, (double)tuned.maximum_damage_velocity,
			(double)tuned.maximum_falling_velocity, (double)ae_fall_damage.stock.lower, (double)ae_fall_damage.stock.upper,
			(double)ae_fall_damage.stock.maximum);
	}
	else
	{
		ae_fall_damage_put(block, &ae_fall_damage.stock);
		error(_error_silent, "ae fall damage: the map's own (hurts from %.1f, fully at %.1f, kills past %.1f world units; "
			"velocities %.4f %.4f %.4f)", (double)ae_fall_damage.stock.lower, (double)ae_fall_damage.stock.upper,
			(double)ae_fall_damage.stock.maximum, (double)ae_fall_damage.stock.minimum_damage_velocity,
			(double)ae_fall_damage.stock.maximum_damage_velocity, (double)ae_fall_damage.stock.maximum_falling_velocity);
	}
	ae_fall_damage.tuned = want;
}

float ae_fall_damage_vehicle_velocity(float globals_velocity)
{
	return ae_fall_damage.tuned && ae_fall_damage.have_stock ? ae_fall_damage.stock.maximum_falling_velocity :
		globals_velocity;
}
