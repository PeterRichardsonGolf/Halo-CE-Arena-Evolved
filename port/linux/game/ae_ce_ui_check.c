/*
AE_CE_UI_CHECK.C

The check of a Custom Edition map's pause menu widgets (ae_ce_ui_check.h).
Each widget definition ('DeLa', 0x3EC bytes) is read for its event handlers
(a block of 0x48-byte elements at 0x54, including widgets they open) and its children (a block of
0x50-byte references at 0x3E0, each naming a widget at 0x0C), and each child
is checked in turn; a collection ('Soul') names its screens in a block of
tag references (0x10 bytes, the index at 0x0C) at its start. A tag block is
its count and its address. Nothing is written.
*/

#include "ae_ce_ui_check.h"

#include <stdio.h>
#include <string.h>

/* ---------- constants */

enum
{
	GROUP_WIDGET_DEFINITION = 0x44654C61, /* 'DeLa' */
	GROUP_WIDGET_COLLECTION = 0x536F756C, /* 'Soul' */
	INSTANCE_SIZE = 0x20,
	INSTANCE_GROUP = 0x00,
	INSTANCE_INDEX = 0x0C,
	INSTANCE_BASE_ADDRESS = 0x14,
	DEFINITION_SIZE = 0x3EC,
	DEFINITION_EVENT_HANDLERS = 0x54,
	DEFINITION_CHILDREN = 0x3E0,
	EVENT_HANDLER_SIZE = 0x48,
	EVENT_HANDLER_FLAGS = 0x00,
	EVENT_HANDLER_WIDGET_INDEX = 0x14,
	EVENT_HANDLER_OPEN_WIDGET_BIT = 3,
	CHILD_SIZE = 0x50,
	CHILD_WIDGET_INDEX = 0x0C,
	COLLECTION_SIZE = 0x0C,
	REFERENCE_SIZE = 0x10,
	REFERENCE_INDEX = 0x0C,
};

/* ---------- structures */

struct walk
{
	const struct ae_ce_ui_image *image;
	long visits;
	/* the widgets entered so far, by the low 16 bits of the tag index (the
	table index): screens link to each other (a child's handler may open its
	parent), so a widget is checked once and a second arrival is not a loop */
	unsigned char entered[65536 / 8];
	char *why;
	size_t why_size;
};

/* ---------- private code */

static uint32_t read32(const unsigned char *bytes)
{
	return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}

static int fail(struct walk *walk, const char *what, long value)
{
	if (walk->why && walk->why_size)
		snprintf(walk->why, walk->why_size, "%s (%ld)", what, value);
	return 0;
}

/* the bytes at the address, size of them, if they all lie in the image */
static const unsigned char *in_image(const struct ae_ce_ui_image *image, uint32_t address, uint32_t size)
{
	uint32_t offset;

	if (address < image->base)
		return NULL;
	offset = address - image->base;
	if (offset > image->size || size > image->size - offset)
		return NULL;
	return image->bytes + offset;
}

/* a tag's data, size bytes of it, if the index names a tag of the group whose
data lies in the image */
static const unsigned char *tag_data(struct walk *walk, int32_t tag_index, uint32_t group, uint32_t size)
{
	const struct ae_ce_ui_image *image = walk->image;
	long absolute = (long)((uint32_t)tag_index & 0xFFFF);
	const unsigned char *instance, *data;

	if (absolute >= image->instance_count)
	{
		fail(walk, "a widget's tag index is past the tag table", (long)tag_index);
		return NULL;
	}
	instance = image->instances + absolute * INSTANCE_SIZE;
	if ((int32_t)read32(instance + INSTANCE_INDEX) != tag_index)
	{
		fail(walk, "a widget's tag index is not its tag's", (long)tag_index);
		return NULL;
	}
	if (read32(instance + INSTANCE_GROUP) != group)
	{
		fail(walk, "a widget's tag is of another group", (long)tag_index);
		return NULL;
	}
	data = in_image(image, read32(instance + INSTANCE_BASE_ADDRESS), size);
	if (!data)
		fail(walk, "a widget's tag lies outside the tags", (long)tag_index);
	return data;
}

/* a block's elements (its count in count), if they all lie in the image */
static int block_whole(struct walk *walk, const unsigned char *block, uint32_t element_size, int32_t *count,
	const unsigned char **elements)
{
	*count = (int32_t)read32(block);
	*elements = NULL;
	if (*count < 0 || *count > AE_CE_UI_MAXIMUM_BLOCK_COUNT)
		return fail(walk, "a widget block's count is out of range", (long)*count);
	if (!*count)
		return 1;
	*elements = in_image(walk->image, read32(block + 4), (uint32_t)*count * element_size);
	if (!*elements)
		return fail(walk, "a widget block lies outside the tags", (long)read32(block + 4));
	return 1;
}

static int widget_whole(struct walk *walk, int32_t tag_index, int depth)
{
	const unsigned char *definition, *handlers, *children;
	int32_t handler_count, child_count, handler, child;

	if (tag_index == AE_CE_UI_NONE)
		return 1;
	{
		uint32_t slot = (uint32_t)tag_index & 0xFFFF;

		/* (a tag index that is not its tag's is refused by tag_data below,
		before anything is read through it; mark only after it passes) */
		if (walk->entered[slot >> 3] & (1u << (slot & 7)))
		{
			const struct ae_ce_ui_image *image = walk->image;

			if (slot < (uint32_t)image->instance_count &&
				(int32_t)read32(image->instances + slot * INSTANCE_SIZE + INSTANCE_INDEX) == tag_index)
			{
				return 1;
			}
		}
	}
	if (++walk->visits > AE_CE_UI_MAXIMUM_VISITS)
		return fail(walk, "too many widgets", walk->visits);
	if (depth > AE_CE_UI_MAXIMUM_DEPTH)
		return fail(walk, "widgets nested too deep", (long)tag_index);
	definition = tag_data(walk, tag_index, GROUP_WIDGET_DEFINITION, DEFINITION_SIZE);
	if (!definition)
		return 0;
	walk->entered[((uint32_t)tag_index & 0xFFFF) >> 3] |= (unsigned char)(1u << (((uint32_t)tag_index & 0xFFFF) & 7));
	if (!block_whole(walk, definition + DEFINITION_EVENT_HANDLERS, EVENT_HANDLER_SIZE, &handler_count, &handlers) ||
		!block_whole(walk, definition + DEFINITION_CHILDREN, CHILD_SIZE, &child_count, &children))
	{
		return 0;
	}
	for (handler = 0; handler < handler_count; handler++)
	{
		const unsigned char *event = handlers + handler * EVENT_HANDLER_SIZE;

		if ((read32(event + EVENT_HANDLER_FLAGS) & (1u << EVENT_HANDLER_OPEN_WIDGET_BIT)) &&
			!widget_whole(walk, (int32_t)read32(event + EVENT_HANDLER_WIDGET_INDEX), depth + 1))
		{
			return 0;
		}
	}
	for (child = 0; child < child_count; child++)
	{
		if (!widget_whole(walk, (int32_t)read32(children + child * CHILD_SIZE + CHILD_WIDGET_INDEX), depth + 1))
			return 0;
	}
	return 1;
}

/* ---------- public code */

int ae_ce_ui_check(const struct ae_ce_ui_image *image, int32_t collection_tag, const int32_t *screen_tags,
	int screen_count, char *why, size_t why_size)
{
	struct walk walk;
	int screen;

	memset(&walk, 0, sizeof(walk));
	walk.image = image;
	walk.why = why;
	walk.why_size = why_size;
	if (why && why_size)
		why[0] = 0;
	if (!image || !image->bytes || !image->instances || image->instance_count < 0)
		return fail(&walk, "no tags", 0);
	if (collection_tag != AE_CE_UI_NONE)
	{
		const unsigned char *collection = tag_data(&walk, collection_tag, GROUP_WIDGET_COLLECTION, COLLECTION_SIZE);
		const unsigned char *references;
		int32_t count, reference;

		if (!collection || !block_whole(&walk, collection, REFERENCE_SIZE, &count, &references))
			return 0;
		for (reference = 0; reference < count; reference++)
		{
			if (!widget_whole(&walk, (int32_t)read32(references + reference * REFERENCE_SIZE + REFERENCE_INDEX), 0))
				return 0;
		}
	}
	for (screen = 0; screen < screen_count; screen++)
	{
		if (!widget_whole(&walk, screen_tags[screen], 0))
			return 0;
	}
	return 1;
}
