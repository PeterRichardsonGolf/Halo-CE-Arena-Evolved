#include <stdio.h>
#include <string.h>

#include "ae_spawner_msg_rules.h"
#include "ae_spawner_logic_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* A round trip keeps every entry; a full message is AE_SPAWNER_MSG_SIZE bytes; the find by map item. */
static void test_round_trip(void)
{
	struct ae_spawner_msg msg, read;
	unsigned char bytes[AE_SPAWNER_MSG_SIZE];
	int k, size;

	memset(&msg, 0, sizeof(msg));
	msg.set = 1;
	msg.count = 3;
	msg.entries[0].item = 7; msg.entries[0].state = AE_SPAWNER_MSG_WAITING; msg.entries[0].seconds = 42;
	msg.entries[1].item = 300; msg.entries[1].state = AE_SPAWNER_MSG_READY; msg.entries[1].seconds = 0;
	msg.entries[2].item = 0; msg.entries[2].state = AE_SPAWNER_MSG_RED; msg.entries[2].seconds = 9;
	size = ae_spawner_msg_encode(&msg, bytes, sizeof(bytes));
	CHECK(size == AE_SPAWNER_MSG_HEAD + 3 * AE_SPAWNER_MSG_ENTRY);
	CHECK(bytes[0] == AE_SPAWNER_MSG_VERSION && bytes[1] == 1 && bytes[2] == 3 && bytes[3] == 0);
	memset(&read, 0xAA, sizeof(read));
	CHECK(ae_spawner_msg_decode(bytes, size, &read));
	CHECK(read.set == 1 && read.count == 3);
	for (k = 0; k < 3; k++)
	{
		CHECK(read.entries[k].item == msg.entries[k].item);
		CHECK(read.entries[k].state == msg.entries[k].state);
		CHECK(read.entries[k].seconds == msg.entries[k].seconds);
	}
	CHECK(ae_spawner_msg_find(&read, 300) == 1);
	CHECK(ae_spawner_msg_find(&read, 0) == 2);
	CHECK(ae_spawner_msg_find(&read, 8) == -1);
	/* the maximum: 256 entries fill the buffer exactly */
	msg.count = AE_SPAWNER_MSG_MAXIMUM;
	for (k = 0; k < AE_SPAWNER_MSG_MAXIMUM; k++)
	{
		msg.entries[k].item = (unsigned short)(k * 3);
		msg.entries[k].state = (unsigned char)(k & 3);
		msg.entries[k].seconds = (unsigned short)(k * 50);
	}
	size = ae_spawner_msg_encode(&msg, bytes, sizeof(bytes));
	CHECK(size == AE_SPAWNER_MSG_SIZE);
	CHECK(ae_spawner_msg_decode(bytes, size, &read));
	CHECK(read.count == AE_SPAWNER_MSG_MAXIMUM);
	CHECK(read.entries[255].item == 765 && read.entries[255].state == 3 && read.entries[255].seconds == 12750);
	CHECK(ae_spawner_msg_find(&read, 765) == 255);
	/* an empty one (the set off) still travels */
	msg.count = 0;
	msg.set = AE_SPAWNER_MSG_SET_OFF;
	size = ae_spawner_msg_encode(&msg, bytes, sizeof(bytes));
	CHECK(size == AE_SPAWNER_MSG_HEAD);
	CHECK(ae_spawner_msg_decode(bytes, size, &read) && read.count == 0 && read.set == AE_SPAWNER_MSG_SET_OFF);
}

/* The refusals: a short buffer, a count over the maximum, a truncated or padded body, another version, a repeated
item; a refused read leaves the message alone; clamps on the way out. */
static void test_refusals(void)
{
	struct ae_spawner_msg msg, read, before;
	unsigned char bytes[AE_SPAWNER_MSG_SIZE + 8];
	int size;

	memset(&msg, 0, sizeof(msg));
	msg.count = 2;
	msg.entries[0].item = 5; msg.entries[0].state = AE_SPAWNER_MSG_WAITING; msg.entries[0].seconds = 60000;
	msg.entries[1].item = 6; msg.entries[1].state = 9; msg.entries[1].seconds = 1;
	CHECK(ae_spawner_msg_encode(&msg, bytes, AE_SPAWNER_MSG_HEAD + 1 * AE_SPAWNER_MSG_ENTRY) == 0);
	size = ae_spawner_msg_encode(&msg, bytes, sizeof(bytes));
	CHECK(size == AE_SPAWNER_MSG_HEAD + 2 * AE_SPAWNER_MSG_ENTRY);
	CHECK(ae_spawner_msg_decode(bytes, size, &read));
	CHECK(read.entries[0].seconds == AE_SPAWNER_MSG_SECONDS_MAXIMUM);	/* clamped */
	CHECK(read.entries[1].state == AE_SPAWNER_MSG_OFF);	/* an unknown state reads as off */
	memset(&before, 0x5A, sizeof(before));
	read = before;
	CHECK(!ae_spawner_msg_decode(bytes, size - 1, &read));	/* truncated */
	CHECK(!ae_spawner_msg_decode(bytes, size + 1, &read));	/* padded */
	CHECK(!ae_spawner_msg_decode(bytes, AE_SPAWNER_MSG_HEAD - 1, &read));
	bytes[0] = 2;
	CHECK(!ae_spawner_msg_decode(bytes, size, &read));	/* another version */
	bytes[0] = AE_SPAWNER_MSG_VERSION;
	bytes[2] = 1; bytes[3] = 1;	/* count 257 */
	CHECK(!ae_spawner_msg_decode(bytes, size, &read));
	bytes[2] = 2; bytes[3] = 0;
	bytes[AE_SPAWNER_MSG_HEAD + AE_SPAWNER_MSG_ENTRY] = 5;	/* the second entry's item = the first's */
	CHECK(!ae_spawner_msg_decode(bytes, size, &read));
	CHECK(memcmp(&read, &before, sizeof(read)) == 0);
	msg.count = AE_SPAWNER_MSG_MAXIMUM + 1;
	CHECK(ae_spawner_msg_encode(&msg, bytes, sizeof(bytes)) == 0);
	msg.count = -1;
	CHECK(ae_spawner_msg_encode(&msg, bytes, sizeof(bytes)) == 0);
}

/* The wire state from a machine's view; which changes make a host resend at once. */
static void test_state_and_differ(void)
{
	struct ae_spawner_msg a, b;
	unsigned short seconds = 99;

	CHECK(ae_spawner_msg_state(AE_SPAWNER_DORMANT, 1, 100, &seconds) == AE_SPAWNER_MSG_OFF && seconds == 0);
	CHECK(ae_spawner_msg_state(AE_SPAWNER_RED, 0, 100, &seconds) == AE_SPAWNER_MSG_RED && seconds == 4);
	CHECK(ae_spawner_msg_state(AE_SPAWNER_READY, 0, 100, &seconds) == AE_SPAWNER_MSG_READY && seconds == 4);
	CHECK(ae_spawner_msg_state(AE_SPAWNER_WAITING, 1, 100, &seconds) == AE_SPAWNER_MSG_READY && seconds == 4);	/* static on spot */
	CHECK(ae_spawner_msg_state(AE_SPAWNER_WAITING, 0, 100, &seconds) == AE_SPAWNER_MSG_WAITING && seconds == 4);
	CHECK(ae_spawner_msg_state(AE_SPAWNER_WAITING, 0, 90, &seconds) == AE_SPAWNER_MSG_WAITING && seconds == 3);
	CHECK(ae_spawner_msg_state(AE_SPAWNER_WAITING, 0, 0, &seconds) == AE_SPAWNER_MSG_WAITING && seconds == 0);
	CHECK(ae_spawner_msg_state(AE_SPAWNER_WAITING, 0, 30L * 100000, &seconds) == AE_SPAWNER_MSG_WAITING &&
		seconds == AE_SPAWNER_MSG_SECONDS_MAXIMUM);
	memset(&a, 0, sizeof(a));
	a.count = 2;
	a.entries[0].item = 1; a.entries[0].state = AE_SPAWNER_MSG_WAITING; a.entries[0].seconds = 10;
	a.entries[1].item = 2; a.entries[1].state = AE_SPAWNER_MSG_READY;
	b = a;
	CHECK(!ae_spawner_msg_states_differ(&a, &b));
	b.entries[0].seconds = 9;
	CHECK(!ae_spawner_msg_states_differ(&a, &b));	/* seconds alone: the slow beat */
	b.entries[0].state = AE_SPAWNER_MSG_READY;
	CHECK(ae_spawner_msg_states_differ(&a, &b));
	b = a;
	b.set = 2;
	CHECK(ae_spawner_msg_states_differ(&a, &b));
	b = a;
	b.count = 1;
	CHECK(ae_spawner_msg_states_differ(&a, &b));
	b = a;
	b.entries[1].item = 3;
	CHECK(ae_spawner_msg_states_differ(&a, &b));
}

int main(void)
{
	test_round_trip();
	test_refusals();
	test_state_and_differ();
	printf("%s: %d checks, %d failures\n", __FILE__, checks, failures);
	return failures ? 1 : 0;
}
