/*
AE_BOTS_LAN.INC

Included at the end of bots.c (one AE hook line), so that Arena Evolved's LAN
bots (ae_lan_bots.c) drive their players with bots.c's own brain, its static
functions and state unchanged: a LAN bot is the host's player, no bot machine
of a local game, and its tick comes from the host's update
(ae_lan_bots_action) instead of bots_update_actions. What bots_update_actions
does once a tick for a local game's bots is bots_ae_lan_tick's, what it does
for each bot bots_ae_lan_drive's. It also brings in, at bots.c's end too, the
bots' fight log (ae_bots_events.inc), the fight by level's glue
(ae_bots_combat.inc), the carriers' hiding (ae_bots_cover.inc, before the
objective play's, which calls it), the objective play's (ae_bots_objective.inc), the
vehicles' (ae_bots_vehicle.inc, after the objective's: it reads its state), the
getting around's (ae_bots_travel.inc) and the deaths by the bots' own hand
(ae_bots_falls.inc, last: it reads the fight's and the getting around's state).
*/

#include "ae_lan_bots.h"
#include "main/main.h"
#include "ae_horn.h"
#include "ae_bots_events.inc"
#include "ae_bots_combat.inc"
#include "ae_bots_power.inc"
#include "ae_bots_team.inc"
#include "ae_bots_revenge.inc"
#include "ae_bots_cover.inc"
#include "ae_bots_squads.inc"
#include "ae_bots_objective.inc"
#include "ae_bots_vehicle.inc"
#include "ae_bots_travel.inc"
#include "ae_bots_falls.inc"
#include "ae_bots_ut.inc"
#include "ae_bots_fear.inc"

void bots_ae_lan_tick(void)
{
	/* (halo.log's minute line, of the ticks before this one) */
	if (bots_globals.ticks > 0)
		bots_report();
	if (!bots_globals.map_ready)
		bots_prepare_map();
	bots_globals.skill = bots_skill();
	bots_globals.tick_searches = 0;
	bots_paths_scan();
	bots_globals.ticks++;
}

void bots_ae_lan_drive(long bot_index, long player_index, struct player_action *action)
{
	struct bots_bot *bot;

	if (bot_index < 0 || bot_index >= BOTS_BRAINS)
		return;
	if (!bots_globals.map_ready)
		bots_ae_lan_tick();
	bot = &bots_globals.bots[bot_index];
	if (bot->player_index != player_index)
		bots_bot_reset(bot, player_index, bot_index);
	bots_bot_update(bot, bot_index, action);
}
