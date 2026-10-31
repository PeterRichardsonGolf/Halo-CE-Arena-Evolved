/*
AE_MELEE_UNZOOM.C

MELEE UNZOOMS as the gametype's rule (ae_melee_unzoom.h; the bit: ae_melee_unzoom_rules.h). Every machine reads the
running game's variant, the host's, so a network client's players melee by the host's rule.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game_engine.h"
#include "units/units.h"

#include "ae_melee_unzoom.h"
#include "ae_melee_unzoom_rules.h"

/* (game_engine.h names the bit the rules header reads) */
typedef char ae_melee_unzoom_bit_check[_game_variant_melee_unzoom_bit == AE_MELEE_UNZOOM_BIT ? 1 : -1];

enum
{
	/* the local players whose melee press is remembered (player_control.c's) */
	AE_MELEE_UNZOOM_LOCAL_PLAYERS = 4,
};

/* each local player's melee held at its last action (the press is logged once) */
static boolean ae_melee_unzoom_held[AE_MELEE_UNZOOM_LOCAL_PLAYERS];

static int ae_melee_unzoom_rule(void)
{
	struct game_variant const *variant = game_engine_running() ? game_engine_get_variant() : NULL;

	return variant ? ae_melee_unzoom_get(variant->universal_variant.flags) : _ae_melee_unzoom_off;
}

boolean ae_melee_unzoom_begin(long unit_index)
{
	short zoom_level;

	if (!ae_melee_unzoom_allows(ae_melee_unzoom_rule(), TRUE))
		return FALSE;
	zoom_level = unit_get_zoom_level(unit_index);
	unit_unzoom(unit_index);
	error(_error_silent, "AE melee unzoom: unit %08lX zoom %d -> none, melee starts", (unsigned long)unit_index,
		(int)zoom_level);
	return TRUE;
}

short ae_melee_unzoom_local(short local_player_index, short zoom_level, boolean melee_held)
{
	int rule = ae_melee_unzoom_rule();
	short level = ae_melee_unzoom_local_level(rule, melee_held, zoom_level);

	if (local_player_index >= 0 && local_player_index < AE_MELEE_UNZOOM_LOCAL_PLAYERS)
	{
		if (melee_held && !ae_melee_unzoom_held[local_player_index] && zoom_level != NONE)
		{
			error(_error_silent, "AE melee unzoom: local player %d melee pressed at zoom %d: %s",
				(int)local_player_index, (int)zoom_level,
				rule == _ae_melee_unzoom_on ? "zoom -> none" : "rule off, zoom kept (no melee while zoomed)");
		}
		ae_melee_unzoom_held[local_player_index] = melee_held ? TRUE : FALSE;
	}
	return level;
}

void ae_melee_unzoom_log_rules(unsigned long flags)
{
	error(_error_silent, "AE melee unzooms: %s", ae_melee_unzoom_name(ae_melee_unzoom_get(flags)));
}
