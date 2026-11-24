/*
AE_PANEL_ENGINE.H

HUD STYLE AE's panels' engine reader (plan Task 14; spec 14.15 part 8): the
game's state for a local player's view as the plain struct ae_panel_snapshot
(ae_panel_model.h), and the CE panels' real anchors in that view (struct
ae_panel_view, ae_panel_layout.h). Called while the HUD draws that view.

Client rule (Review Focus 8): only state a joined machine has. Per field:
- weapon (magazines, reserve, heat, age, overheated): the weapon object's own,
  replicated by the host's weapon updates (weapon_build_weapon_interface_state,
  weapons.c), as CE's own panel reads it;
- grenades and the armed type: the unit's grenade_counts and
  current_grenade_index, replicated with the unit;
- shield and health: the unit's shield_vitality and body_vitality,
  replicated (damage is the host's);
- camo: the player's powerup_durations[camouflage] ticks, the host's,
  replicated (network_distributed.c);
- overshield: a PROJECTION from the replicated shield vitality (no timer
  exists: damage.c's decay of 0.00074074074 a tick past 1.0; while
  over-charging, + 0.033333335 a tick to 3.0, then the decay), kept from
  rising unless the vitality rose (a host correction never makes it run
  backwards by more than a step);
- the low-ammo flash, the shield's low / down blink: CE's tests on the same
  state; the blink's phase is AE's own (0.5 s), as it has no CE element.
*/

#ifndef __AE_PANEL_ENGINE_H
#define __AE_PANEL_ENGINE_H

#include "ae_pure_types.h"
#include "ae_panel_model.h"
#include "ae_panel_layout.h"

/* FALSE: no live unit for this local player (dead, respawning, no player) */
boolean ae_panel_engine_snapshot(short local_player_index, struct ae_panel_snapshot *out);
/* the CE panels' anchors in this view (the HUD's top-left elements' corner, the shield meter's right edge and top,
from where CE's own placement put them) and its HUD scale; FALSE when a fallback was used for an anchor CE has not
placed yet */
boolean ae_panel_engine_view(short local_player_index, struct ae_panel_view *out);
/* the weapon's short name for the log ("assault rifle"), or "none" */
const char *ae_panel_engine_weapon_name(short local_player_index);
/* debug.ae_panel_log: every 2 s of game time one line per local player (call while the HUD draws each view) */
void ae_panel_engine_debug_log(short local_player_index);
/* debug.ae_panel_log's value, read again only when a setting changed (the HUD's views ask it every frame) */
boolean ae_panel_engine_log_on(void);
/* REDUCE MOTION (display.arena_menus_reduce_motion), read the same way */
boolean ae_panel_engine_reduced_motion(void);
/* (the game's tick, through ae_strip_engine_debug_log's hook) debug.ae_panel_powerups and debug.ae_panel_seat */
void ae_panel_engine_debug_tick(void);

#endif /* __AE_PANEL_ENGINE_H */
