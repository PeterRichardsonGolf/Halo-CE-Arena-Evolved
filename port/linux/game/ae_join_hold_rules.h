/*
AE_JOIN_HOLD_RULES.H

Whether the lobby keeps a machine that has no player yet because its map transfer is still moving: the host's map
sharing connection to it is open and sent it map data lately (map_share_machine_transfer_alive, map_share.c). A
machine that stopped getting data, or crashed, is not kept, so the lobby's playerless timeout still frees its slot.
Pure, no engine includes (unit test: port/linux/tests/ae_join_hold_rules_test.c).
*/

#ifndef __AE_JOIN_HOLD_RULES_H
#define __AE_JOIN_HOLD_RULES_H

enum
{
	AE_JOIN_HOLD_MOVED_MS = 20000 /* data sent this long ago at most */
};

/* moved_ago_ms: milliseconds since the last block went out, or a negative number for none yet */
int ae_join_hold_transfer_alive(int open, int outgoing, long bytes_out, long moved_ago_ms);

/* whether the machine's "kept while downloading" event was not said for this join yet (said_join is the join time
it was said for, 0 for none; join_time is this join's): the event is said once per machine per lobby join */
int ae_join_hold_say_event(unsigned long said_join, unsigned long join_time);

/* (0.9.0, a host's report 11-12) whether a machine's map download stalled: it was downloading, it has not finished,
and it has gone quiet past map sharing's timeout (still_downloading 0), not the host's extra files (extra). Such a
machine no longer "needs the map", but it does not have it: its join ends instead of its players being added */
int ae_join_hold_download_stalled(int downloading, int downloaded, int still_downloading, int extra);

#endif
