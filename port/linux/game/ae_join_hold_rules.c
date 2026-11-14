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
