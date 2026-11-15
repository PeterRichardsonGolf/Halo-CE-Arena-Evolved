/*
AE_SPLATTER.C

FRIENDLY SPLATTER (ae_splatter.h; the rule: ae_splatter_rules.h). Halo CE hurts any biped a vehicle touches above
0.067 world units a tick (2 a second, a crawl) with the full vehicle collision damage, the driver's team ignored: a
warthog nudging a teammate kills him. Under the gametype's FAST ONLY, a teammate of the driver is hurt only from the
minimum speed up, the damage growing to the full at the full speed; enemies and driverless vehicles keep Halo CE's
rule. The host's game decides (the damage is the host's). halo.log has a line per teammate hit, with the speed.
A bot driving hurts no teammate at any speed, under either rule (owner 10-31: "i got ran over twice tonight"; bots do
not steer around their team yet).
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_allegiance.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "math/real_math.h"
#include "objects/objects.h"
#include "units/units.h"
#include "units/vehicles.h"

#include "ae_splatter.h"
#include "ae_splatter_rules.h"
#include "bots.h"

void platform_log(const char *format, ...);

static long ae_splatter_logged_tick = -1000;

real ae_splatter_damage_scale(long vehicle_index, long biped_index)
{
	struct unit_datum *vehicle = vehicle_get(vehicle_index);
	struct object_datum *biped = object_get(biped_index);
	struct object_datum *driver;
	struct game_variant const *variant = game_engine_running() ? game_engine_get_variant() : NULL;
	int fast_only = variant ? ae_splatter_fast_only(variant->universal_variant.pad5) : 0;
	boolean teammate;
	real speed, scale;

	if (vehicle->unit.driver_object_index == NONE)
		return 1.0f;
	driver = object_get(vehicle->unit.driver_object_index);
	teammate = driver->object.owner_team_index != NONE &&
		!game_team_is_enemy(driver->object.owner_team_index, biped->object.owner_team_index);
	/* (the vehicle's own speed: physics.c has already pushed the biped along by then, so the two's difference reads
	low even on a hard hit) */
	speed = magnitude3d(&vehicle->object.translational_velocity);
	scale = ae_splatter_scale(fast_only, teammate, speed);
	/* (a bot at the wheel: its teammates are never hurt) */
	if (teammate && ((struct unit_datum *)driver)->unit.player_index != NONE)
	{
		struct player_datum *player = player_try_and_get(((struct unit_datum *)driver)->unit.player_index);

		if (player && bots_machine_is_bot(player->network_player_data.machine_index))
			scale = 0.0f;
	}
	if (teammate && (game_time_get() < ae_splatter_logged_tick || game_time_get() - ae_splatter_logged_tick >= 5))
	{
		ae_splatter_logged_tick = game_time_get();
		platform_log("friendly splatter: %s, teammate hit at %.3f world units a tick, damage x%.2f%s",
			ae_splatter_rule_name(fast_only), speed, scale, scale == 0.0f && ae_splatter_scale(fast_only, teammate, speed) > 0.0f ?
			" (a bot drives: none)" : "");
	}
	return scale;
}
