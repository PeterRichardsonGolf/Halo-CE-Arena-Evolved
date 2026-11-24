/* ae_join_hold_rules.c: see ae_join_hold_rules.h */
#include "ae_join_hold_rules.h"

int ae_join_hold_transfer_alive(int open, int outgoing, long bytes_out, long moved_ago_ms)
{
	return open && !outgoing && bytes_out > 0 && moved_ago_ms >= 0 && moved_ago_ms <= AE_JOIN_HOLD_MOVED_MS;
}

int ae_join_hold_say_event(unsigned long said_join, unsigned long join_time)
{
	return !said_join || said_join != join_time;
}

int ae_join_hold_download_stalled(int downloading, int downloaded, int still_downloading, int extra)
{
	return downloading && !downloaded && !still_downloading && !extra;
}

long ae_join_hold_since(unsigned long now, unsigned long at)
{
	long since;

	if (!at)
		return -1;
	since = (long)(now - at);
	return since < 0 && since >= -1 ? 0 : since;
}

int ae_join_hold_list_waiting(int hello_sent, int shared, int ready, int listed, int asked, long since_hello_ms,
	long since_ready_ms, long since_listed_ms)
{
	if (!hello_sent || !shared || asked)
		return 0;
	if (listed)
		return since_listed_ms >= 0 && since_listed_ms <= AE_JOIN_HOLD_LIST_GRACE_MS;
	if (!ready)
		return since_hello_ms >= 0 && since_hello_ms <= AE_JOIN_HOLD_LIST_MS;
	return since_ready_ms >= 0 && since_ready_ms <= AE_JOIN_HOLD_LIST_GRACE_MS;
}
