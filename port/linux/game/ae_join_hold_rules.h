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

enum
{
	AE_JOIN_HOLD_LIST_MS = 120000 /* a guest waits this long for the host's list of maps at most */
};

enum
{
	AE_JOIN_HOLD_LIST_GRACE_MS = 15000, /* the time a guest has to ask for its map once the list is ready or sent */
	AE_JOIN_HOLD_HASH_MS = 3 /* the host's hashing for a waiting guest takes this long a frame at most */
};

/* a host's map list not ready yet: after a restart the first guest starts its scan and hashing, and a guest lacking the
game's map waits for the list before it asks for the map, so it adds no player meanwhile. Whether a machine that was told
this host runs this game (hello_sent), is shared with, and has not asked for a map or started a download (asked 0) is
kept, though it has no player: while the list is not ready (ready 0) for AE_JOIN_HOLD_LIST_MS from the hello; once it is
ready, or sent to the machine (listed), for the grace from then. The times are the milliseconds since the hello, since
the list was ready (or the hello, if later) and since it was sent, a negative number for none */
int ae_join_hold_list_waiting(int hello_sent, int shared, int ready, int listed, int asked, long since_hello_ms,
	long since_ready_ms, long since_listed_ms);

/* the milliseconds since a time of system_milliseconds() (at: 0 for none, a time kept as "now | 1" so that it is never
0), for ae_join_hold_list_waiting: -1 for none, and 0 for a time up to a millisecond after now (an odd time set in the
millisecond before it, as the frame that finished the host's hashing reads it) */
long ae_join_hold_since(unsigned long now, unsigned long at);

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
