/*
AE_CROUCH.H

CONTROLLER CROUCH as the gametype's rule (ae_crouch_rules.h): what the AE hook lines in source/game/player_control.c
and game_engine.c call (port/linux/game/ae_hooks.txt lists them). Included after cseries.h.
*/

#ifndef __AE_CROUCH_H
#define __AE_CROUCH_H

/* player_control.c's crouch gate: TRUE when a controller's held crouch (crouch_held) keeps the player crouched at
full stick: the running game's gametype is STAYS DOWN, the host's on a network client. FALSE with no game (the
campaign, the menus) and in every gametype without the rule: Halo's own crouch. Its first read after the game's rules
line (at a grounded player's full stick) is logged ("AE controller crouch gate: ...") */
boolean ae_controller_crouch_holds_down(boolean crouch_held);

/* the "AE controller crouch:" line of the game's rules, after its "AE rules:" one (from the variant's flags); the
gate's next read is logged again */
void ae_controller_crouch_log_rules(unsigned long flags);

#endif
