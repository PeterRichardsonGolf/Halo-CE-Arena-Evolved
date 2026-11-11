/*
AE_MOUSE_AIM.H

MOUSE AIM ASSIST as the gametype's rule (ae_mouse_aim_rules.h): what the AE hook lines in network_aim_assist.c and
game_engine.c call (port/linux/game/ae_hooks.txt lists them). Included after cseries.h.
*/

#ifndef __AE_MOUSE_AIM_H
#define __AE_MOUSE_AIM_H

/* the host's (and a local game's) policy: TRUE blocks the mouse's aim assist: host_setting (its
network.block_mouse_aim_assist) or the running game's gametype MOUSE AIM ASSIST OFF. A change is logged ("AE mouse
aim assist: the host's policy: ...") */
boolean ae_mouse_aim_assist_host_blocks(boolean host_setting);

/* a client: the policy its host sent (message 90), logged when it changes ("AE mouse aim assist: the host says: ...") */
void ae_mouse_aim_assist_note_policy(boolean blocked);

/* the "AE mouse aim assist:" line of the game's rules (from the variant's flags) */
void ae_mouse_aim_assist_log_rules(unsigned long flags);

#endif
