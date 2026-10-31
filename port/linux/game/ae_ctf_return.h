#ifndef __AE_CTF_RETURN_H__
#define __AE_CTF_RETURN_H__

/*
AE_CTF_RETURN.H

(spike 10-26) FLAG RETURN the later Halos' way (HALO_FLAG_RETURN=stand; off, or anything else: Halo CE's): a defender's
touch no longer sends a dropped flag home; it counts down (HALO_FLAG_RETURN_TIME seconds, 30) and goes home at 0, each
defender standing by it making the count faster; anyone picking it up starts it again. game_engine_ctf.c's AE hooks.
*/

/* whether the spike plays (a defender's touch does nothing then) */
boolean ae_ctf_stand_return(void);
/* (once a tick, each flag) whether it is due home: Halo CE's 17 s untouched, or for a flag taken from home (handled)
the spike's count */
boolean ae_ctf_flag_return_due(long weapon_index, long last_owned_time, boolean handled, boolean ce_due);

#endif
