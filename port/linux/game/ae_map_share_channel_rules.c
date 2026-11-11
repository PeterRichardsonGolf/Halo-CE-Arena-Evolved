/*
AE_MAP_SHARE_CHANNEL_RULES.C

Arena Evolved (0.7.7): map sharing's connection over the game's own connection, its pure rules
(ae_map_share_channel_rules.h).
*/

#include "ae_map_share_channel_rules.h"

#include <string.h>

static void put_long(
	unsigned char *bytes,
	uint32_t value)
{
	bytes[0] = (unsigned char)value;
	bytes[1] = (unsigned char)(value >> 8);
	bytes[2] = (unsigned char)(value >> 16);
	bytes[3] = (unsigned char)(value >> 24);
}

static uint32_t get_long(
	unsigned char const *bytes)
{
	return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
		(uint32_t)bytes[3] << 24;
}

long ae_map_share_channel_frame_encode(
	struct ae_map_share_channel_frame const *frame,
	unsigned char *bytes,
	long size)
{
	long needed;

	if (!frame || !bytes || !frame->session || (frame->flags & ~AE_MAP_SHARE_CHANNEL_FLAGS) ||
		frame->data_bytes < 0 || frame->data_bytes > AE_MAP_SHARE_CHANNEL_DATA_BYTES ||
		(frame->data_bytes && !frame->data))
	{
		return 0;
	}
	needed = AE_MAP_SHARE_CHANNEL_HEADER_BYTES + frame->data_bytes;
	if (size < needed)
		return 0;
	bytes[0] = AE_MAP_SHARE_CHANNEL_FRAME_TYPE;
	bytes[1] = frame->session;
	bytes[2] = frame->flags;
	bytes[3] = 0;
	put_long(bytes + 4, (frame->flags & AE_MAP_SHARE_CHANNEL_FLAG_ACK) ? frame->taken : 0);
	if (frame->data_bytes)
		memcpy(bytes + AE_MAP_SHARE_CHANNEL_HEADER_BYTES, frame->data, (size_t)frame->data_bytes);
	return needed;
}

int ae_map_share_channel_frame_decode(
	unsigned char const *bytes,
	long size,
	struct ae_map_share_channel_frame *frame)
{
	if (!frame)
		return 0;
	memset(frame, 0, sizeof(*frame));
	if (!bytes || size < AE_MAP_SHARE_CHANNEL_HEADER_BYTES ||
		size > AE_MAP_SHARE_CHANNEL_HEADER_BYTES + AE_MAP_SHARE_CHANNEL_DATA_BYTES ||
		bytes[0] != AE_MAP_SHARE_CHANNEL_FRAME_TYPE || !bytes[1] || (bytes[2] & ~AE_MAP_SHARE_CHANNEL_FLAGS) ||
		bytes[3] ||
		((bytes[2] & AE_MAP_SHARE_CHANNEL_FLAG_OPEN) && (bytes[2] & AE_MAP_SHARE_CHANNEL_FLAG_CLOSE)))
	{
		return 0;
	}
	frame->session = bytes[1];
	frame->flags = bytes[2];
	frame->taken = (frame->flags & AE_MAP_SHARE_CHANNEL_FLAG_ACK) ? get_long(bytes + 4) : 0;
	frame->data_bytes = size - AE_MAP_SHARE_CHANNEL_HEADER_BYTES;
	frame->data = frame->data_bytes ? bytes + AE_MAP_SHARE_CHANNEL_HEADER_BYTES : NULL;
	return 1;
}

unsigned char ae_map_share_channel_next_session(
	unsigned char last)
{
	return (unsigned char)(last == 255 ? 1 : last + 1);
}

long ae_map_share_channel_window_room(
	struct ae_map_share_channel_window const *window)
{
	uint32_t flight = window->sent - window->peer_took;

	return flight >= AE_MAP_SHARE_CHANNEL_WINDOW ? 0 : (long)(AE_MAP_SHARE_CHANNEL_WINDOW - flight);
}

void ae_map_share_channel_window_sent(
	struct ae_map_share_channel_window *window,
	long bytes)
{
	if (bytes > 0)
		window->sent += (uint32_t)bytes;
}

int ae_map_share_channel_window_peer_took(
	struct ae_map_share_channel_window *window,
	uint32_t taken)
{
	/* (counts that wrap at 4 GiB: the differences are what count) */
	if ((int32_t)(taken - window->peer_took) < 0 || (int32_t)(window->sent - taken) < 0)
		return 0;
	window->peer_took = taken;
	return 1;
}

void ae_map_share_channel_window_took(
	struct ae_map_share_channel_window *window,
	long bytes)
{
	if (bytes > 0)
		window->took += (uint32_t)bytes;
}

int ae_map_share_channel_window_ack_due(
	struct ae_map_share_channel_window const *window)
{
	return window->took - window->said >= AE_MAP_SHARE_CHANNEL_ACK_STEP;
}

uint32_t ae_map_share_channel_window_ack_said(
	struct ae_map_share_channel_window *window)
{
	window->said = window->took;
	return window->said;
}

void ae_map_share_channel_inbox_init(
	struct ae_map_share_channel_inbox *inbox,
	unsigned char *bytes,
	long capacity)
{
	inbox->bytes = bytes;
	inbox->capacity = bytes && capacity > 0 ? capacity : 0;
	inbox->start = 0;
	inbox->count = 0;
}

int ae_map_share_channel_inbox_put(
	struct ae_map_share_channel_inbox *inbox,
	unsigned char const *bytes,
	long size)
{
	long end;
	long first;

	if (size < 0 || (size && !bytes) || size > inbox->capacity - inbox->count)
		return 0;
	if (!size)
		return 1;
	end = (inbox->start + inbox->count) % inbox->capacity;
	first = inbox->capacity - end < size ? inbox->capacity - end : size;
	memcpy(inbox->bytes + end, bytes, (size_t)first);
	if (first < size)
		memcpy(inbox->bytes, bytes + first, (size_t)(size - first));
	inbox->count += size;
	return 1;
}

long ae_map_share_channel_inbox_take(
	struct ae_map_share_channel_inbox *inbox,
	unsigned char *bytes,
	long size)
{
	long taken = size < inbox->count ? size : inbox->count;
	long first;

	if (taken <= 0 || !bytes)
		return 0;
	first = inbox->capacity - inbox->start < taken ? inbox->capacity - inbox->start : taken;
	memcpy(bytes, inbox->bytes + inbox->start, (size_t)first);
	if (first < taken)
		memcpy(bytes + first, inbox->bytes, (size_t)(taken - first));
	inbox->start = (inbox->start + taken) % inbox->capacity;
	inbox->count -= taken;
	if (!inbox->count)
		inbox->start = 0;
	return taken;
}
