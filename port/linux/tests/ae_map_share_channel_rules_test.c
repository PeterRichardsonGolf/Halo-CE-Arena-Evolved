/* map sharing's connection over the game's own (ae_map_share_channel_rules.h): the frame, the window, the inbox */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_map_share_channel_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void test_frame(void)
{
	unsigned char data[AE_MAP_SHARE_CHANNEL_DATA_BYTES + 1];
	unsigned char bytes[AE_MAP_SHARE_CHANNEL_HEADER_BYTES + AE_MAP_SHARE_CHANNEL_DATA_BYTES + 16];
	struct ae_map_share_channel_frame frame;
	struct ae_map_share_channel_frame back;
	long length;
	long index;

	for (index = 0; index < (long)sizeof(data); index++)
		data[index] = (unsigned char)(index * 7);
	memset(&frame, 0, sizeof(frame));
	frame.session = 9;
	frame.flags = AE_MAP_SHARE_CHANNEL_FLAG_OPEN | AE_MAP_SHARE_CHANNEL_FLAG_ACK;
	frame.taken = 0x12345678UL;
	frame.data = data;
	frame.data_bytes = 100;
	length = ae_map_share_channel_frame_encode(&frame, bytes, sizeof(bytes));
	CHECK(length == AE_MAP_SHARE_CHANNEL_HEADER_BYTES + 100);
	CHECK(bytes[0] == AE_MAP_SHARE_CHANNEL_FRAME_TYPE && bytes[1] == 9 && bytes[3] == 0);
	CHECK(bytes[4] == 0x78 && bytes[7] == 0x12);
	CHECK(ae_map_share_channel_frame_decode(bytes, length, &back));
	CHECK(back.session == 9 && back.flags == frame.flags && back.taken == 0x12345678UL && back.data_bytes == 100);
	CHECK(back.data == bytes + AE_MAP_SHARE_CHANNEL_HEADER_BYTES && !memcmp(back.data, data, 100));

	/* (no ACK: the count is not read, nor written) */
	frame.flags = 0;
	length = ae_map_share_channel_frame_encode(&frame, bytes, sizeof(bytes));
	CHECK(length && !bytes[4] && !bytes[5] && !bytes[6] && !bytes[7]);
	bytes[4] = 5;
	CHECK(ae_map_share_channel_frame_decode(bytes, length, &back) && back.taken == 0);

	/* the largest, and one more */
	frame.data_bytes = AE_MAP_SHARE_CHANNEL_DATA_BYTES;
	CHECK(ae_map_share_channel_frame_encode(&frame, bytes, sizeof(bytes)) ==
		AE_MAP_SHARE_CHANNEL_HEADER_BYTES + AE_MAP_SHARE_CHANNEL_DATA_BYTES);
	frame.data_bytes = AE_MAP_SHARE_CHANNEL_DATA_BYTES + 1;
	CHECK(!ae_map_share_channel_frame_encode(&frame, bytes, sizeof(bytes)));
	frame.data_bytes = 10;
	CHECK(!ae_map_share_channel_frame_encode(&frame, bytes, AE_MAP_SHARE_CHANNEL_HEADER_BYTES + 9));
	frame.session = 0;
	CHECK(!ae_map_share_channel_frame_encode(&frame, bytes, sizeof(bytes)));
	frame.session = 1;
	frame.flags = 0x80;
	CHECK(!ae_map_share_channel_frame_encode(&frame, bytes, sizeof(bytes)));
	frame.flags = 0;
	frame.data = NULL;
	CHECK(!ae_map_share_channel_frame_encode(&frame, bytes, sizeof(bytes)));

	/* what a peer may send that is refused */
	frame.data = data;
	length = ae_map_share_channel_frame_encode(&frame, bytes, sizeof(bytes));
	CHECK(length);
	CHECK(!ae_map_share_channel_frame_decode(bytes, AE_MAP_SHARE_CHANNEL_HEADER_BYTES - 1, &back));
	CHECK(!ae_map_share_channel_frame_decode(NULL, length, &back));
	bytes[0] = 3;
	CHECK(!ae_map_share_channel_frame_decode(bytes, length, &back));
	bytes[0] = AE_MAP_SHARE_CHANNEL_FRAME_TYPE;
	bytes[1] = 0;
	CHECK(!ae_map_share_channel_frame_decode(bytes, length, &back));
	bytes[1] = 1;
	bytes[2] = 0x08;
	CHECK(!ae_map_share_channel_frame_decode(bytes, length, &back));
	bytes[2] = AE_MAP_SHARE_CHANNEL_FLAG_OPEN | AE_MAP_SHARE_CHANNEL_FLAG_CLOSE;
	CHECK(!ae_map_share_channel_frame_decode(bytes, length, &back));
	bytes[2] = 0;
	bytes[3] = 1;
	CHECK(!ae_map_share_channel_frame_decode(bytes, length, &back));
	bytes[3] = 0;
	CHECK(ae_map_share_channel_frame_decode(bytes, length, &back));
	CHECK(!ae_map_share_channel_frame_decode(bytes, AE_MAP_SHARE_CHANNEL_HEADER_BYTES + AE_MAP_SHARE_CHANNEL_DATA_BYTES + 1,
		&back));
	/* (a frame of no data: a close, an ack) */
	CHECK(ae_map_share_channel_frame_decode(bytes, AE_MAP_SHARE_CHANNEL_HEADER_BYTES, &back) && !back.data &&
		!back.data_bytes);
}

static void test_session(void)
{
	CHECK(ae_map_share_channel_next_session(0) == 1);
	CHECK(ae_map_share_channel_next_session(1) == 2);
	CHECK(ae_map_share_channel_next_session(254) == 255);
	CHECK(ae_map_share_channel_next_session(255) == 1);
}

static void test_window(void)
{
	struct ae_map_share_channel_window window;
	struct ae_map_share_channel_window receiver;

	memset(&window, 0, sizeof(window));
	memset(&receiver, 0, sizeof(receiver));
	CHECK(ae_map_share_channel_window_room(&window) == AE_MAP_SHARE_CHANNEL_WINDOW);
	ae_map_share_channel_window_sent(&window, AE_MAP_SHARE_CHANNEL_WINDOW - 10);
	CHECK(ae_map_share_channel_window_room(&window) == 10);
	ae_map_share_channel_window_sent(&window, 10);
	CHECK(ae_map_share_channel_window_room(&window) == 0);
	/* the receiver takes a quarter: it says so, the sender has that room again */
	ae_map_share_channel_window_took(&receiver, AE_MAP_SHARE_CHANNEL_ACK_STEP - 1);
	CHECK(!ae_map_share_channel_window_ack_due(&receiver));
	ae_map_share_channel_window_took(&receiver, 1);
	CHECK(ae_map_share_channel_window_ack_due(&receiver));
	CHECK(ae_map_share_channel_window_peer_took(&window, ae_map_share_channel_window_ack_said(&receiver)));
	CHECK(!ae_map_share_channel_window_ack_due(&receiver));
	CHECK(ae_map_share_channel_window_room(&window) == AE_MAP_SHARE_CHANNEL_ACK_STEP);
	/* a peer that says it took more than was sent, or less than before */
	CHECK(!ae_map_share_channel_window_peer_took(&window, AE_MAP_SHARE_CHANNEL_WINDOW + 1));
	CHECK(!ae_map_share_channel_window_peer_took(&window, AE_MAP_SHARE_CHANNEL_ACK_STEP - 1));
	CHECK(ae_map_share_channel_window_peer_took(&window, AE_MAP_SHARE_CHANNEL_ACK_STEP));
	CHECK(ae_map_share_channel_window_peer_took(&window, AE_MAP_SHARE_CHANNEL_WINDOW));
	CHECK(ae_map_share_channel_window_room(&window) == AE_MAP_SHARE_CHANNEL_WINDOW);
	/* counts that wrap */
	window.sent = 0xFFFFFF00UL;
	window.peer_took = 0xFFFFFF00UL;
	ae_map_share_channel_window_sent(&window, 0x200);
	CHECK(window.sent == 0x100);
	CHECK(ae_map_share_channel_window_room(&window) == AE_MAP_SHARE_CHANNEL_WINDOW - 0x200);
	CHECK(ae_map_share_channel_window_peer_took(&window, 0x80));
	CHECK(ae_map_share_channel_window_room(&window) == AE_MAP_SHARE_CHANNEL_WINDOW - 0x80);
	CHECK(!ae_map_share_channel_window_peer_took(&window, 0x101));
	/* nothing sent backwards */
	ae_map_share_channel_window_sent(&window, -5);
	CHECK(window.sent == 0x100);
}

static void test_inbox(void)
{
	struct ae_map_share_channel_inbox inbox;
	unsigned char *buffer = malloc(16);
	unsigned char in[32];
	unsigned char out[32];
	long index;

	for (index = 0; index < 32; index++)
		in[index] = (unsigned char)(index + 1);
	ae_map_share_channel_inbox_init(&inbox, buffer, 16);
	CHECK(ae_map_share_channel_inbox_take(&inbox, out, 4) == 0);
	CHECK(ae_map_share_channel_inbox_put(&inbox, in, 10));
	CHECK(!ae_map_share_channel_inbox_put(&inbox, in, 7));
	CHECK(inbox.count == 10);
	CHECK(ae_map_share_channel_inbox_take(&inbox, out, 6) == 6 && !memcmp(out, in, 6));
	/* (round the end of the buffer) */
	CHECK(ae_map_share_channel_inbox_put(&inbox, in + 10, 12));
	CHECK(inbox.count == 16);
	CHECK(!ae_map_share_channel_inbox_put(&inbox, in, 1));
	CHECK(ae_map_share_channel_inbox_take(&inbox, out, 32) == 16 && !memcmp(out, in + 6, 16));
	CHECK(inbox.count == 0 && inbox.start == 0);
	CHECK(ae_map_share_channel_inbox_put(&inbox, in, 0));
	CHECK(!ae_map_share_channel_inbox_put(&inbox, NULL, 3));
	CHECK(!ae_map_share_channel_inbox_put(&inbox, in, -1));
	free(buffer);
	/* (no buffer: nothing fits) */
	ae_map_share_channel_inbox_init(&inbox, NULL, 16);
	CHECK(!ae_map_share_channel_inbox_put(&inbox, in, 1));
	CHECK(ae_map_share_channel_inbox_take(&inbox, out, 1) == 0);
}

int main(void)
{
	test_frame();
	test_session();
	test_window();
	test_inbox();
	if (failures)
		return 1;
	printf("ae_map_share_channel_rules: ok\n");
	return 0;
}
