/*
AE_JOINER_NOTICE.H

(0.7.1, owner 2026-10-31) A host's word on a machine that joins without
Arena Evolved: one chat line to the whole game naming the build the machine
plays on (OpenCE's, or the build a Delta machine names in its HELLO), that
some of the game's rules and extras may not show for it, and where AE is.
Once a machine, when its first player is in the game and its build is known
(or 10 s after it joined).
*/

#ifndef AE_JOINER_NOTICE_H
#define AE_JOINER_NOTICE_H

#include "cseries.h"

/* (ae_silent_host_machine, each frame for each client machine of the host's server) remote_joined: the machine
is joined to the game and not the host's own */
void ae_joiner_notice_host_machine(long machine_index, boolean remote_joined);
/* (0.7.4, owner 2026-11-02) the host refused a machine whose game lacks the weapon packs the gametype uses: one chat
line for the players in the game, at most once a minute (a refused player tends to try again and again) */
void ae_joiner_notice_pack_refusal(void);

/* (0.7.6) the same for a machine refused by the weapon cache's gate: cache_case is ae_silent_rules.h's
ae_cache_join_case (no block: the game needs the newest Arena Evolved; another cache: its case's line) */
void ae_joiner_notice_cache_refusal(int cache_case);

/* (0.7.5) the same for a machine refused because the game is TERRITORIES, which only Arena Evolved plays */
void ae_joiner_notice_territories_refusal(void);
/* (0.7.6) a machine without Arena Evolved refused from a round mode's game (ELIMINATION): a chat line, once a minute */
void ae_joiner_notice_rounds_refusal(void);
void ae_joiner_notice_rounds_full(void);

#endif
