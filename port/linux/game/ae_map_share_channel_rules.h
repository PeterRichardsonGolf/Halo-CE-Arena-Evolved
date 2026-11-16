/*
AE_MAP_SHARE_CHANNEL_RULES.H

Arena Evolved (0.7.7): map sharing's connection to the host carried over the game's own connection, for a host that
is reached only at its game port (a forwarded port, a tunnel that carries the game port alone). The pure rules: the
frame that carries the stream's bytes, the window that bounds what is in flight, and the inbox that holds what came
until map sharing reads it. Unit-tested (port/linux/tests/ae_map_share_channel_rules_test.c); the glue is
ae_map_share_channel.c, and port/linux/NETCODE.md ("Over the game's connection") says how it goes on the wire.

A stream frame is a body after map sharing's AE envelope (the request message from a guest, the offer message from a
host, both named for no map): type (AE_MAP_SHARE_CHANNEL_FRAME_TYPE), the stream's session (1-255, the guest's
number for it), flags, a zero byte, the peer's bytes taken so far (4 bytes, little-endian, valid with the ACK flag),
then the stream's bytes. The guest opens a session (OPEN on its first frame), either end closes it (CLOSE), and an end
sends no more than AE_MAP_SHARE_CHANNEL_WINDOW bytes the other has not said it took: what an end must hold for the
other is bounded, and the game's own messages never wait behind more than that.
*/

#ifndef __AE_MAP_SHARE_CHANNEL_RULES_H
#define __AE_MAP_SHARE_CHANNEL_RULES_H

#include <stdint.h>

/* ---------- constants */

/* the frame's type: after AE_LAYOUT_SHARE_FRAME_TYPE (3); a build without this code drops a frame of a type it does
not know (it is never sent one: a host sends stream frames only on a session a guest opened) */
#define AE_MAP_SHARE_CHANNEL_FRAME_TYPE 4
#define AE_MAP_SHARE_CHANNEL_FLAG_OPEN 0x01
#define AE_MAP_SHARE_CHANNEL_FLAG_CLOSE 0x02
#define AE_MAP_SHARE_CHANNEL_FLAG_ACK 0x04
#define AE_MAP_SHARE_CHANNEL_FLAGS (AE_MAP_SHARE_CHANNEL_FLAG_OPEN | AE_MAP_SHARE_CHANNEL_FLAG_CLOSE | \
	AE_MAP_SHARE_CHANNEL_FLAG_ACK)

enum
{
	AE_MAP_SHARE_CHANNEL_HEADER_BYTES = 8,
	/* the stream's bytes in one frame: a reliable message is at most 4095 bytes, of which the message header (8)
	and the envelope (72 at most) */
	AE_MAP_SHARE_CHANNEL_DATA_BYTES = 3584,
	/* the bytes in flight one way at most, and what the other end holds for it at most */
	AE_MAP_SHARE_CHANNEL_WINDOW = 0x40000,
	/* the bytes taken that make an end say so (a quarter of the window) */
	AE_MAP_SHARE_CHANNEL_ACK_STEP = AE_MAP_SHARE_CHANNEL_WINDOW / 4,
};

/* ---------- structures */

struct ae_map_share_channel_frame
{
	unsigned char session;
	unsigned char flags;
	/* the peer's bytes this end took, all told (with the ACK flag) */
	uint32_t taken;
	unsigned char const *data;
	long data_bytes;
};

/* one end's count of the stream: the bytes it sent and the peer said it took, and those it took and said it took */
struct ae_map_share_channel_window
{
	uint32_t sent;
	uint32_t peer_took;
	uint32_t took;
	uint32_t said;
};

/* what came and is not read yet (AE_MAP_SHARE_CHANNEL_WINDOW at most), in the caller's buffer */
struct ae_map_share_channel_inbox
{
	unsigned char *bytes;
	long capacity;
	long start;
	long count;
};

/* ---------- prototypes */

/* the frame's bytes (a body of AE_MAP_SHARE_CHANNEL_HEADER_BYTES + its data's), or 0 for one that cannot be:
no session, a flag unknown, data over AE_MAP_SHARE_CHANNEL_DATA_BYTES, a buffer too small */
long ae_map_share_channel_frame_encode(
	struct ae_map_share_channel_frame const *frame,
	unsigned char *bytes,
	long size);
/* a received frame (its data points into the bytes); 0 for one that is not: not the type, no session, a flag
unknown, a byte reserved not zero, data over AE_MAP_SHARE_CHANNEL_DATA_BYTES, OPEN with CLOSE */
int ae_map_share_channel_frame_decode(
	unsigned char const *bytes,
	long size,
	struct ae_map_share_channel_frame *frame);

/* the next session number after `last` (1-255, never 0) */
unsigned char ae_map_share_channel_next_session(
	unsigned char last);

/* the bytes this end may send now (the window less what is in flight) */
long ae_map_share_channel_window_room(
	struct ae_map_share_channel_window const *window);
void ae_map_share_channel_window_sent(
	struct ae_map_share_channel_window *window,
	long bytes);
/* the peer says it took `taken` bytes in all: 0 (the stream to be closed) if that is fewer than it said before or
more than were sent */
int ae_map_share_channel_window_peer_took(
	struct ae_map_share_channel_window *window,
	uint32_t taken);
void ae_map_share_channel_window_took(
	struct ae_map_share_channel_window *window,
	long bytes);
/* whether this end says what it took now: AE_MAP_SHARE_CHANNEL_ACK_STEP more since it last did (the peer, its window
full, waits for it) */
int ae_map_share_channel_window_ack_due(
	struct ae_map_share_channel_window const *window);
/* ... said: its count (the frame's `taken`) */
uint32_t ae_map_share_channel_window_ack_said(
	struct ae_map_share_channel_window *window);

/* the inbox over the caller's buffer of `capacity` bytes (AE_MAP_SHARE_CHANNEL_WINDOW: the peer sends no more) */
void ae_map_share_channel_inbox_init(
	struct ae_map_share_channel_inbox *inbox,
	unsigned char *bytes,
	long capacity);
/* bytes that came: 0 (the stream to be closed) if they do not fit, the peer having sent past the window */
int ae_map_share_channel_inbox_put(
	struct ae_map_share_channel_inbox *inbox,
	unsigned char const *bytes,
	long size);
/* up to `size` bytes read, how many */
long ae_map_share_channel_inbox_take(
	struct ae_map_share_channel_inbox *inbox,
	unsigned char *bytes,
	long size);

#endif
