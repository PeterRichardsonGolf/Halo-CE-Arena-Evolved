/*
AE_SPAWNER_MSG_RULES.C

The spawners' state message body (ae_spawner_msg_rules.h).
*/

#include <string.h>

#include "ae_spawner_msg_rules.h"
#include "ae_spawner_logic_rules.h"

int ae_spawner_msg_state(int state, int on_spot, long ticks_left, unsigned short *seconds)
{
	long s;

	*seconds = 0;
	if (state == AE_SPAWNER_DORMANT)
		return AE_SPAWNER_MSG_OFF;
	if (ticks_left > 0)
	{
		s = (ticks_left + AE_SPAWNER_TICKS_PER_SECOND - 1) / AE_SPAWNER_TICKS_PER_SECOND;
		*seconds = (unsigned short)(s > AE_SPAWNER_MSG_SECONDS_MAXIMUM ? AE_SPAWNER_MSG_SECONDS_MAXIMUM : s);
	}
	if (state == AE_SPAWNER_RED)
		return AE_SPAWNER_MSG_RED;
	if (state == AE_SPAWNER_READY || on_spot)
		return AE_SPAWNER_MSG_READY;
	return AE_SPAWNER_MSG_WAITING;
}

static void put16(unsigned char *at, unsigned int value)
{
	at[0] = (unsigned char)(value & 0xFF);
	at[1] = (unsigned char)((value >> 8) & 0xFF);
}

static unsigned int get16(unsigned char const *at)
{
	return (unsigned int)at[0] | ((unsigned int)at[1] << 8);
}

int ae_spawner_msg_encode(struct ae_spawner_msg const *msg, unsigned char *bytes, int size)
{
	int k, needed;

	if (msg->count < 0 || msg->count > AE_SPAWNER_MSG_MAXIMUM)
		return 0;
	needed = AE_SPAWNER_MSG_HEAD + msg->count * AE_SPAWNER_MSG_ENTRY;
	if (size < needed)
		return 0;
	bytes[0] = AE_SPAWNER_MSG_VERSION;
	bytes[1] = msg->set;
	put16(bytes + 2, (unsigned int)msg->count);
	for (k = 0; k < msg->count; k++)
	{
		struct ae_spawner_msg_entry const *entry = &msg->entries[k];
		unsigned int state = entry->state <= AE_SPAWNER_MSG_RED ? entry->state : AE_SPAWNER_MSG_OFF;
		unsigned int seconds = entry->seconds > AE_SPAWNER_MSG_SECONDS_MAXIMUM ? AE_SPAWNER_MSG_SECONDS_MAXIMUM :
			entry->seconds;
		unsigned char *at = bytes + AE_SPAWNER_MSG_HEAD + k * AE_SPAWNER_MSG_ENTRY;

		put16(at, entry->item);
		put16(at + 2, state | (seconds << 2));
	}
	return needed;
}

int ae_spawner_msg_decode(unsigned char const *bytes, int size, struct ae_spawner_msg *msg)
{
	struct ae_spawner_msg read;
	unsigned char seen[65536 / 8];
	int k;

	if (size < AE_SPAWNER_MSG_HEAD || bytes[0] != AE_SPAWNER_MSG_VERSION)
		return 0;
	memset(&read, 0, sizeof(read));
	read.set = bytes[1];
	read.count = (int)get16(bytes + 2);
	if (read.count > AE_SPAWNER_MSG_MAXIMUM || size != AE_SPAWNER_MSG_HEAD + read.count * AE_SPAWNER_MSG_ENTRY)
		return 0;
	memset(seen, 0, sizeof(seen));
	for (k = 0; k < read.count; k++)
	{
		unsigned char const *at = bytes + AE_SPAWNER_MSG_HEAD + k * AE_SPAWNER_MSG_ENTRY;
		unsigned int item = get16(at), word = get16(at + 2);

		if (seen[item >> 3] & (1 << (item & 7)))
			return 0;
		seen[item >> 3] |= (unsigned char)(1 << (item & 7));
		read.entries[k].item = (unsigned short)item;
		read.entries[k].state = (unsigned char)(word & 3);
		read.entries[k].seconds = (unsigned short)(word >> 2);
	}
	*msg = read;
	return 1;
}

int ae_spawner_msg_find(struct ae_spawner_msg const *msg, int item)
{
	int k;

	for (k = 0; k < msg->count; k++)
		if (msg->entries[k].item == item)
			return k;
	return -1;
}

int ae_spawner_msg_states_differ(struct ae_spawner_msg const *a, struct ae_spawner_msg const *b)
{
	int k;

	if (a->set != b->set || a->count != b->count)
		return 1;
	for (k = 0; k < a->count; k++)
		if (a->entries[k].item != b->entries[k].item || a->entries[k].state != b->entries[k].state)
			return 1;
	return 0;
}
