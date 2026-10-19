/*
AE_LAYOUT_SHARE_RULES.C

Layouts travel with maps: the frames, a guest's inbox of its host's layouts, and which layout a guest plays and keeps
(ae_layout_share_rules.h). Pure: no engine includes.
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ae_layout_share_rules.h"

static void put_word(unsigned char *at, unsigned long value)
{
	at[0] = (unsigned char)value;
	at[1] = (unsigned char)(value >> 8);
}

static void put_long(unsigned char *at, unsigned long value)
{
	put_word(at, value);
	put_word(at + 2, value >> 16);
}

static unsigned long get_word(unsigned char const *at)
{
	return (unsigned long)at[0] | (unsigned long)at[1] << 8;
}

static unsigned long get_long(unsigned char const *at)
{
	return get_word(at) | get_word(at + 2) << 16;
}

/* whether a frame's numbers are one a host sends */
static int frame_sound(struct ae_layout_share_frame const *frame)
{
	if (frame->length < 0 || frame->length > AE_LAYOUT_SHARE_CHUNK || frame->total < 0 ||
		frame->total > AE_LAYOUT_MAXIMUM_FILE_SIZE || frame->offset < 0 || frame->serial > 0xFFFFu ||
		!memchr(frame->level, 0, sizeof(frame->level)) || !frame->level[0])
	{
		return 0;
	}
	if (frame->flags & AE_LAYOUT_SHARE_FLAG_NONE)
		return frame->total == 0 && frame->offset == 0 && frame->length == 0;
	/* (a layout has bytes, and every frame some of them, none past its total) */
	return frame->total > 0 && frame->length > 0 && frame->offset < frame->total &&
		frame->length <= frame->total - frame->offset;
}

long ae_layout_share_frame_encode(struct ae_layout_share_frame const *frame, unsigned char *bytes, long size)
{
	long needed;

	if (!frame || !bytes || !frame_sound(frame) || (frame->flags & ~AE_LAYOUT_SHARE_FLAG_NONE))
		return 0;
	needed = AE_LAYOUT_SHARE_FRAME_HEADER_BYTES + frame->length;
	if (size < needed)
		return 0;
	memset(bytes, 0, (size_t)AE_LAYOUT_SHARE_FRAME_HEADER_BYTES);
	bytes[0] = AE_LAYOUT_SHARE_FRAME_TYPE;
	bytes[1] = (unsigned char)frame->flags;
	put_word(bytes + 2, frame->serial);
	put_long(bytes + 4, (unsigned long)frame->total);
	put_long(bytes + 8, (unsigned long)frame->offset);
	put_word(bytes + 12, (unsigned long)frame->length);
	memcpy(bytes + 16, frame->level, AE_LAYOUT_SHARE_LEVEL_BYTES);
	memcpy(bytes + AE_LAYOUT_SHARE_FRAME_HEADER_BYTES, frame->data, (size_t)frame->length);
	return needed;
}

int ae_layout_share_frame_decode(unsigned char const *bytes, long size, struct ae_layout_share_frame *frame)
{
	unsigned long total, offset;

	if (!frame)
		return 0;
	memset(frame, 0, sizeof(*frame));
	if (!bytes || size < AE_LAYOUT_SHARE_FRAME_HEADER_BYTES || size > AE_LAYOUT_SHARE_FRAME_MAXIMUM_BYTES ||
		bytes[0] != AE_LAYOUT_SHARE_FRAME_TYPE || (bytes[1] & ~AE_LAYOUT_SHARE_FLAG_NONE))
	{
		return 0;
	}
	total = get_long(bytes + 4);
	offset = get_long(bytes + 8);
	if (total > AE_LAYOUT_MAXIMUM_FILE_SIZE || offset > AE_LAYOUT_MAXIMUM_FILE_SIZE)
		return 0;
	frame->flags = bytes[1];
	frame->serial = (unsigned int)get_word(bytes + 2);
	frame->total = (long)total;
	frame->offset = (long)offset;
	frame->length = (int)get_word(bytes + 12);
	memcpy(frame->level, bytes + 16, AE_LAYOUT_SHARE_LEVEL_BYTES);
	if (size != AE_LAYOUT_SHARE_FRAME_HEADER_BYTES + (long)frame->length || !frame_sound(frame))
	{
		memset(frame, 0, sizeof(*frame));
		return 0;
	}
	memcpy(frame->data, bytes + AE_LAYOUT_SHARE_FRAME_HEADER_BYTES, (size_t)frame->length);
	return 1;
}

int ae_layout_share_frame_make(char const *level, unsigned int serial, char const *text, long length, long offset,
	struct ae_layout_share_frame *frame)
{
	if (!frame)
		return 0;
	memset(frame, 0, sizeof(*frame));
	if (!level || strlen(level) >= AE_LAYOUT_SHARE_LEVEL_BYTES || !level[0])
		return 0;
	strcpy(frame->level, level);
	frame->serial = serial & 0xFFFFu;
	if (!text)
	{
		frame->flags = AE_LAYOUT_SHARE_FLAG_NONE;
		return offset == 0;
	}
	if (length <= 0 || length > AE_LAYOUT_MAXIMUM_FILE_SIZE || offset < 0 || offset >= length)
		return 0;
	frame->total = length;
	frame->offset = offset;
	frame->length = (int)(length - offset < AE_LAYOUT_SHARE_CHUNK ? length - offset : AE_LAYOUT_SHARE_CHUNK);
	memcpy(frame->data, text + offset, (size_t)frame->length);
	return 1;
}

static int same_name(char const *a, char const *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

void ae_layout_share_clear(struct ae_layout_share_inbox *inbox)
{
	int index;

	if (!inbox)
		return;
	inbox->clock = 0;
	for (index = 0; index < AE_LAYOUT_SHARE_SLOTS; index++)
	{
		inbox->slots[index].level[0] = 0;
		inbox->slots[index].state = AE_LAYOUT_SHARE_EMPTY;
		inbox->slots[index].total = inbox->slots[index].received = 0;
		inbox->slots[index].used = 0;
	}
}

int ae_layout_share_find(struct ae_layout_share_inbox const *inbox, char const *level)
{
	int index;

	if (!inbox || !level || !level[0])
		return -1;
	for (index = 0; index < AE_LAYOUT_SHARE_SLOTS; index++)
		if (inbox->slots[index].state != AE_LAYOUT_SHARE_EMPTY && same_name(inbox->slots[index].level, level))
			return index;
	return -1;
}

int ae_layout_share_take(struct ae_layout_share_inbox *inbox, struct ae_layout_share_frame const *frame, int *slot)
{
	struct ae_layout_share_slot *taken;
	int index;

	if (slot)
		*slot = -1;
	if (!inbox || !frame || !frame_sound(frame))
		return -1;
	index = ae_layout_share_find(inbox, frame->level);
	if (frame->offset == 0)
	{
		/* (a level's layout begun anew: in its own slot, else an empty one, else the least lately used) */
		if (index < 0)
		{
			int other;

			index = 0;
			for (other = 0; other < AE_LAYOUT_SHARE_SLOTS; other++)
			{
				if (inbox->slots[other].state == AE_LAYOUT_SHARE_EMPTY)
				{
					index = other;
					break;
				}
				if (inbox->slots[other].used < inbox->slots[index].used)
					index = other;
			}
		}
		taken = &inbox->slots[index];
		strcpy(taken->level, frame->level);
		taken->serial = frame->serial;
		taken->total = frame->total;
		taken->received = 0;
		taken->state = (frame->flags & AE_LAYOUT_SHARE_FLAG_NONE) ? AE_LAYOUT_SHARE_NONE : AE_LAYOUT_SHARE_PARTIAL;
	}
	else
	{
		/* (the next frame of the layout being taken, alone) */
		if (index < 0)
			return -1;
		taken = &inbox->slots[index];
		if (taken->state != AE_LAYOUT_SHARE_PARTIAL || taken->serial != frame->serial || taken->total != frame->total ||
			taken->received != frame->offset)
		{
			return -1;
		}
	}
	taken->used = ++inbox->clock;
	if (slot)
		*slot = index;
	if (taken->state == AE_LAYOUT_SHARE_NONE)
		return 1;
	memcpy(taken->text + taken->received, frame->data, (size_t)frame->length);
	taken->received += frame->length;
	if (taken->received < taken->total)
		return 0;
	taken->state = AE_LAYOUT_SHARE_TEXT;
	return 1;
}

int ae_layout_share_decide(int host, int host_fits, int own_exists, int own_same, int may_write, int *disk)
{
	if (disk)
		*disk = AE_LAYOUT_SHARE_DISK_NOTHING;
	if (host == AE_LAYOUT_SHARE_HOST_NONE)
		return AE_LAYOUT_SHARE_APPLY_NOTHING;
	if (host != AE_LAYOUT_SHARE_HOST_TEXT || !host_fits)
		return AE_LAYOUT_SHARE_APPLY_OWN;
	if (disk && may_write && (!own_exists || !own_same))
		*disk = own_exists ? AE_LAYOUT_SHARE_DISK_BESIDE : AE_LAYOUT_SHARE_DISK_WRITE;
	return AE_LAYOUT_SHARE_APPLY_HOST;
}

int ae_layout_share_beside_path(char const *own_path, char *out, long size)
{
	size_t length;

	if (!own_path || !out || size <= 0)
		return 0;
	out[0] = 0;
	length = strlen(own_path);
	if (length < 5 || own_path[length - 5] == '\\' || own_path[length - 5] == '/' ||
		!same_name(own_path + length - 4, ".txt"))
	{
		return 0;
	}
	if ((long)(length + 5) >= size)
		return 0;
	snprintf(out, (size_t)size, "%.*s.host.txt", (int)(length - 4), own_path);
	return 1;
}

int ae_layout_share_fits(char const *text, long length, char const *map_file, unsigned long map_hash,
	struct ae_layout *scratch)
{
	char error[160];

	if (!text || length <= 0 || length > AE_LAYOUT_MAXIMUM_FILE_SIZE || !scratch)
		return 0;
	return ae_layout_parse(text, length, scratch, error, sizeof(error)) == 0 &&
		ae_layout_for_map(scratch, map_file, map_hash);
}
