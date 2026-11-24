/*
AE_SHIELD_BLEED.C

SHIELD BLEED (ae_shield_bleed.h): the leftover of a shield's pop on a player's unit, as the gametype asks. The host's
damage decides, as all damage; the row's value travels in the AE settings block.
*/

#include "cseries.h"
#include "game/players.h"
#include "objects/objects.h"

#include "ae_burst_rifle.h"
#include "ae_melee.h"
#include "ae_melee_rules.h"
#include "ae_rifle_profiles.h"
#include "ae_shield_bleed.h"
#include "ae_shield_bleed_rules.h"
#include "ae_variant_ext.h"

real ae_shield_bleed_excess(long object_index, long damage_definition_index, real excess, real raw, real shielded)
{
	int row_mode = ae_variant_ext_game()->shield_bleed;
	int mode;
	real unraised;
	struct ae_rifle_profile const *profile = damage_definition_index == NONE ? NULL :
		ae_burst_rifle_damage_profile(damage_definition_index);
	int no_spill = profile && profile->no_spill;

	/* (a no-spill rifle's round: nothing past the shield, on any target and in any mode. It holds for ANY round of the
	rifle that breaks a shield, not only one from a full shield: a shield already at 50 after a magnum shot (25), then
	EVO RIFLE rounds: the round that breaks it carries 0 into health, and the next head hit kills) */
	if (no_spill)
		return (real)ae_shield_bleed_excess_rule(row_mode, (float)excess, (float)raw, (float)shielded, no_spill);

	if (!object_try_and_get_and_verify_type(object_index, _object_mask_unit) ||
		player_index_from_unit_index(object_index) == NONE)
	{
		return excess;
	}
	/* a covered melee blow (MELEE rule) converts the second game's way, by its unraised shield points (its shield
	points were raised) */
	unraised = ae_melee_shield_unraised(object_index);
	mode = ae_melee_bleed_mode(ae_variant_ext_game()->melee_rule, unraised > 0.f, row_mode);
	if (mode == AE_EXT_SHIELD_BLEED_CLASSIC)
		return excess;
	if (unraised > 0.f)
		return (real)ae_melee_bleed_excess((float)excess, (float)raw, (float)unraised);
	return (real)ae_shield_bleed_excess_rule(mode, (float)excess, (float)raw, (float)shielded, 0);
}
