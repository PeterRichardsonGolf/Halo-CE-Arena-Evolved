#ifndef __AE_CTF_RETURN_H__
#define __AE_CTF_RETURN_H__

/*
AE_CTF_RETURN.H

FLAG RETURN, the gametype's (ae_variant_ext's ctf_return; the rules: ae_ctf_return_rules.h): Halo CE's own, H2 TIMER
(the time stands still while an enemy is by the flag) or STAND-BY (a defender's touch no longer sends a dropped flag
home; it counts down, each defender standing by it making the count faster). game_engine_ctf.c's AE hooks.
*/

/* whether a defender's touch does nothing (STAND-BY) */
boolean ae_ctf_stand_return(void);
/* (once a tick, each flag; the host) whether it is due home: in CE ce_due (Halo CE's own test), else for a flag taken
from home (handled) the mode's count */
boolean ae_ctf_flag_return_due(long weapon_index, long last_owned_time, boolean handled, boolean ce_due);
/* whether a loose flag away from its stand has an enemy of its team by it (SUDDEN DEATH's contest) */
boolean ae_ctf_flag_contested(long weapon_index);
/* (players.c's player_examine_nearby_item, every machine) FLAG PICKUP HOLD: whether that player takes that weapon (an
enemy's flag in CTF) by the weapon pick-up's hold, not by touch; a bot never (it takes it with a touch) */
boolean ae_ctf_flag_hold(long weapon_index, long player_index);

#endif
