/*
AE_GRENADE_FX_SCRAMBLE.C

The grenade catalogue's SCRAMBLE module (ae_grenade_fx.h): a player's unit in the blast's reach has its motion tracker
and HUD scrambled for the row's scramble seconds at its strength. Enemies of the thrower; teammates only with
friendly fire on; never the thrower. A player in a vehicle too (the player's own unit is in reach). The host keeps
the end and sends it; only the machine whose local player owns the unit draws it.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "objects/objects.h"
#include "objects/object_types.h"
#include "units/units.h"

#include "ae_grenade_fx.h"

/* whether the thrower's blast scrambles a player's unit */
static boolean scrambles(struct ae_grenade_blast const *blast, long unit_index, struct unit_datum const *unit)
{
	struct game_variant_options const *options;

	if (unit->unit.player_index == NONE || unit_index == blast->thrower_unit ||
		(blast->thrower_player != NONE && unit->unit.player_index == blast->thrower_player))
	{
		return FALSE;
	}
	if (!game_engine_has_teams() || blast->thrower_team == NONE || unit->object.owner_team_index != blast->thrower_team)
		return TRUE;
	options = game_variant_options_get();
	return options && options->friendly_fire != _friendly_fire_off;
}

void ae_grenade_fx_scramble_apply(struct ae_grenade_blast const *blast)
{
	struct ae_grenade_numbers const *numbers = &blast->entry->numbers;
	short index;

	for (index = 0; index < blast->target_count; index++)
	{
		long unit_index = blast->targets[index].object_index, ticks;
		struct unit_datum *unit;

		if (blast->targets[index].type != _object_type_biped ||
			!(unit = (struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit)) ||
			!scrambles(blast, unit_index, unit))
		{
			continue;
		}
		ticks = ae_grenade_effect_ticks(numbers->scramble_seconds, numbers->scramble_edge_seconds,
			numbers->edge_fraction, blast->targets[index].strength);
		error(_error_silent, "grenades: %s scrambles player %08lX's unit %08lX: strength %.2f, %ld ticks (%ld left)",
			blast->entry->display, (unsigned long)unit->unit.player_index, (unsigned long)unit_index,
			(double)blast->targets[index].strength, ticks, ae_grenade_fx_scramble_hit(unit_index, ticks));
	}
}
