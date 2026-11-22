/*
AE_TRAITS.C

GRAVITY and PLAYER SPEED (ae_traits.h): the gametype's bytes in the AE block (the host's, on a client) as a player's
multipliers. Every machine moves the bipeds it simulates, a client its own player, with the same multipliers, and the
host bounds a guest's movement by them. The host gives them only to the players of machines that play them
(ae_traits_build_has_them); an earlier build's players move as the map has it, as their own machines move them.
*/

#include <stdio.h>

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "physics/physics.h"
#include "tag_files/tag_groups.h"
#include "units/biped_definitions.h"
#include "memory/data.h"
#include "game/players.h"
#include "networking/network_game_globals.h"

#include "ae_jump.h"
#include "ae_map_share.h"
#include "ae_traits.h"
#include "ae_traits_rules.h"
#include "ae_variant_ext.h"
#include "ae_variant_ext_rules.h"
#include "../src/delta_peer.h"
#include "bots.h"

#include "halo_product.h"

void platform_log(char const *format, ...);
const char *updater_version(void);

/* whether the player's machine plays the traits: on the host, another machine's player only when its build does */
static boolean player_has_them(long player_index)
{
	struct player_datum *player;
	char build[48], own[48];

	if (player_index == NONE || !global_network_game_server_get())
		return TRUE;
	player = player_get(player_index);
	if (!player || player->local_player_index != NONE)
		return TRUE;
	/* (a bot is the host's own, moved by the host) */
	if (bots_machine_is_bot(player->network_player_data.machine_index))
		return TRUE;
	if (!ae_peer_is_ae(player->network_player_data.machine_index) ||
		delta_peer_game_machine_build((int)player->network_player_data.machine_index, build, (int)sizeof(build)) <= 0)
	{
		return FALSE;
	}
	snprintf(own, sizeof(own), HALO_PRODUCT_SHORT_NAME " %s", updater_version());
	return ae_traits_build_has_them(build, own) != 0;
}

static real trait_multiplier(long player_index, int trait, unsigned char stored, int *logged)
{
	int percent = ae_trait_percent(trait, stored);

	if (player_index == NONE || !stored || !player_has_them(player_index))
		return 1.0f;
	if (percent != *logged)
	{
		*logged = percent;
		platform_log("traits: %s %d%%", ae_trait_rule(trait)->name, percent);
	}
	return ae_trait_multiplier(trait, stored);
}

real ae_traits_gravity_multiplier(long player_index)
{
	static int logged = 100;

	return trait_multiplier(player_index, AE_TRAIT_GRAVITY, ae_variant_ext_game()->gravity_percent, &logged);
}

real ae_traits_speed_multiplier(long player_index)
{
	static int logged = 100;

	return trait_multiplier(player_index, AE_TRAIT_SPEED, ae_variant_ext_game()->speed_percent, &logged);
}

/* Halo CE's player's jump speed and gravity, per tick: the jump the bots' own reaches are measured on */
#define CE_JUMP_VELOCITY 0.07f
#define CE_GRAVITY 0.0035651792f

/* the map's player biped's jump speed (its globals' multiplayer unit), 0 when none */
static real map_jump_velocity(void)
{
	long globals_index = tag_loaded(GAME_GLOBALS_TAG, "globals\\globals");
	struct game_globals *globals = globals_index == NONE ? NULL : game_globals_definition_get(globals_index);
	struct game_globals_multiplayer_information *information = globals && globals->multiplayer_information.count > 0 ?
		TAG_BLOCK_GET_ELEMENT(&globals->multiplayer_information, 0, struct game_globals_multiplayer_information) : NULL;
	long unit_index = information ? information->unit.index : NONE;

	return unit_index == NONE ? 0.0f : ((struct biped_definition *)tag_get(BIPED_DEFINITION_TAG, unit_index))->biped.jump_velocity;
}

/* (ae_traits_game_jump_scales' last answer: the tick and the gametype's bytes it was worked out for; a new game
forgets it, ae_traits_game_jump_scales_forget) */
static struct
{
	boolean valid;
	long tick;
	unsigned char gravity, speed;
	real jump_multiplier;
	real rise, across;
} game_scales;

void ae_traits_game_jump_scales_forget(void)
{
	game_scales.valid = FALSE;
}

void ae_traits_game_jump_scales(real *rise, real *across)
{
	struct ae_variant_ext const *ext = ae_variant_ext_game();
	real jump_multiplier = ae_jump_multiplier(NONE);
	long now = game_time_get();

	if (!game_scales.valid || now != game_scales.tick || ext->gravity_percent != game_scales.gravity ||
		ext->speed_percent != game_scales.speed || jump_multiplier != game_scales.jump_multiplier)
	{
		real velocity = map_jump_velocity(), jump, gravity, speed;

		game_scales.valid = TRUE;
		game_scales.tick = now;
		game_scales.gravity = ext->gravity_percent;
		game_scales.speed = ext->speed_percent;
		game_scales.jump_multiplier = jump_multiplier;
		game_scales.rise = game_scales.across = 1.0f;
		if (velocity > 0.0f && global_gravity > 0.0f)
		{
			jump = velocity * jump_multiplier / CE_JUMP_VELOCITY;
			gravity = global_gravity * ae_trait_multiplier(AE_TRAIT_GRAVITY, ext->gravity_percent) / CE_GRAVITY;
			speed = ae_trait_multiplier(AE_TRAIT_SPEED, ext->speed_percent);
			game_scales.rise = ae_traits_jump_reach(jump, gravity);
			game_scales.across = speed * jump / gravity;
		}
	}
	*rise = game_scales.rise;
	*across = game_scales.across;
}

real ae_traits_game_speed(void)
{
	return ae_trait_multiplier(AE_TRAIT_SPEED, ae_variant_ext_game()->speed_percent);
}
