/*
AE_MELEE_UNZOOM.H

MELEE UNZOOMS as the gametype's rule (ae_melee_unzoom_rules.h): what the AE hook lines in source/units/bipeds.c,
source/game/player_control.c and game_engine.c call (port/linux/game/ae_hooks.txt lists them). Included after
cseries.h.
*/

#ifndef __AE_MELEE_UNZOOM_H
#define __AE_MELEE_UNZOOM_H

/* bipeds.c's melee gate, for a player's unit whose weapon is zoomed: TRUE under the running game's MELEE UNZOOMS ON
(the host's variant on a network client), after taking the unit's zoom off (unit_unzoom: its own machine's player
control too); FALSE, the zoom kept, with the rule OFF and with no game. Each unzoom is logged
("AE melee unzoom: ...") */
boolean ae_melee_unzoom_begin(long unit_index);

/* player_control.c: the zoom level a local player's action asks for: none while melee is held under ON, else
zoom_level. A melee press with the weapon zoomed is logged, with what the rule does */
short ae_melee_unzoom_local(short local_player_index, short zoom_level, boolean melee_held);

/* the "AE melee unzooms:" line of the game's rules, after its "AE controller crouch:" one (from the variant's flags) */
void ae_melee_unzoom_log_rules(unsigned long flags);

#endif
