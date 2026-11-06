/*
AE_UI_DELETED.C

Arena Evolved: the menus' mouse targets of deleted widgets (ae_ui_deleted.h).
*/

#include "ae_ui_deleted.h"

#include <string.h>

static void const *deleted[AE_UI_DELETED_MAXIMUM];
static long deleted_count = 0;
/* more were deleted than the list holds */
static int deleted_overflow = 0;

void ae_ui_deleted_note(
	void const *widget)
{
	if (deleted_count < AE_UI_DELETED_MAXIMUM)
		deleted[deleted_count++] = widget;
	else
		deleted_overflow = 1;
}

void ae_ui_deleted_clear(
	void)
{
	deleted_count = 0;
	deleted_overflow = 0;
}

static int was_deleted(
	void const *widget)
{
	long index;

	for (index = 0; index < deleted_count; index++)
	{
		if (deleted[index] == widget)
			return 1;
	}

	return 0;
}

int ae_ui_deleted_forget(
	void *targets,
	long *count,
	unsigned long stride,
	unsigned long widget_offset)
{
	unsigned char *base = (unsigned char *)targets;
	long index, kept = 0;

	if (!targets || !count || *count <= 0 || (!deleted_count && !deleted_overflow))
		return 0;
	if (deleted_overflow)
	{
		*count = 0;
		return 1;
	}
	for (index = 0; index < *count; index++)
	{
		void const *widget;

		memcpy(&widget, base + index * stride + widget_offset, sizeof(widget));
		if (was_deleted(widget))
			continue;
		if (kept != index)
			memmove(base + kept * stride, base + index * stride, stride);
		kept++;
	}
	if (kept == *count)
		return 0;
	*count = kept;

	return 1;
}
