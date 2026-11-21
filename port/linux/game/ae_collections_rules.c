/*
AE_COLLECTIONS_RULES.C

Workshop collections' page rules. See ae_collections_rules.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_collections_rules.h"

#define BULLET "\xE2\x80\xA2"

void ae_coll_size_text(unsigned long long bytes, char *text, int size)
{
	if (size <= 0)
		return;
	text[0] = 0;
	if (!bytes)
		return;
	if (bytes >= 1000ull * 1000ull * 1000ull)
	{
		unsigned long long tenths = (bytes + 50ull * 1000ull * 1000ull) / (100ull * 1000ull * 1000ull);

		snprintf(text, (size_t)size, "%llu.%llu GB", tenths / 10ull, tenths % 10ull);
	}
	else
		snprintf(text, (size_t)size, "%llu MB", (bytes + 500000ull) / 1000000ull);
}

static int is_digit(char c)
{
	return c >= '0' && c <= '9';
}

void ae_coll_map_name(char const *title, char *name, int size)
{
	char work[AE_COLL_TITLE + 16];
	char *at = work;
	size_t length;

	if (size <= 0)
		return;
	name[0] = 0;
	if (!title)
		return;
	snprintf(work, sizeof(work), "%s", title);
	if (!strncmp(at, "Halo", 4) || !strncmp(at, "HALO", 4))
	{
		char *dash = strstr(at, " - ");

		if (dash && dash - at < 28)
			at = dash + 3;
	}
	length = strlen(at);
	while (length && (at[length - 1] == ' ' || at[length - 1] == '\r' || at[length - 1] == '\n'))
		at[--length] = 0;
	/* (a trailing pack tag: " - SMP", "-SMP", " SMP") */
	if (length > 3 && !strcmp(at + length - 3, "SMP"))
	{
		length -= 3;
		while (length && (at[length - 1] == ' ' || at[length - 1] == '-'))
			length--;
		at[length] = 0;
	}
	{
		size_t index, written = 0;

		for (index = 0; at[index] && written + 1 < (size_t)size; index++)
		{
			unsigned char c = (unsigned char)at[index];

			if (c < 0x20 || c >= 0x7F)
				c = ' ';
			name[written++] = (char)c;
		}
		name[written] = 0;
	}
}

void ae_coll_display_name(char const *title, char *name, int size)
{
	int index, length, from = 0, written = 0;

	if (size <= 0)
		return;
	name[0] = 0;
	if (!title)
		return;
	length = (int)strlen(title);
	for (index = 0; index <= length && written + 1 < size; index++)
	{
		if (title[index] == ' ' || !title[index])
		{
			int each, digit = 0;

			for (each = from; each < index; each++)
				digit |= is_digit(title[each]);
			for (each = from; each < index && written + 1 < size; each++)
			{
				char c = title[each];
				int upper = !digit && each == from;

				if (c >= 'a' && c <= 'z' && upper)
					c = (char)(c - 32);
				else if (c >= 'A' && c <= 'Z' && !upper)
					c = (char)(c + 32);
				name[written++] = c;
			}
			if (title[index] == ' ' && written + 1 < size)
				name[written++] = ' ';
			from = index + 1;
		}
	}
	name[written] = 0;
}

void ae_coll_summarize(struct ae_coll const *coll, struct ae_coll_summary *summary)
{
	int index;

	memset(summary, 0, sizeof(*summary));
	summary->sizes_known = 1;
	summary->cut = coll->cut;
	summary->count = coll->count;
	for (index = 0; index < coll->count && index < AE_COLL_ITEMS; index++)
	{
		struct ae_coll_item const *item = &coll->items[index];

		if (!item->size)
			summary->sizes_known = 0;
		if (item->failed)
			summary->failed_get += item->state == AE_COLL_ITEM_ABSENT || item->state == AE_COLL_ITEM_UPDATE;
		summary->total_bytes += item->size;
		switch (item->state)
		{
		case AE_COLL_ITEM_PRESENT:
		case AE_COLL_ITEM_UPDATE:
			summary->present++;
			if (item->state == AE_COLL_ITEM_UPDATE)
			{
				summary->updates++;
				summary->update_bytes += item->size;
				summary->unknown_update += item->size == 0;
			}
			if (item->steam_owned)
				summary->steam_kept++;
			else
			{
				summary->removable++;
				summary->removable_bytes += item->size;
			}
			break;
		case AE_COLL_ITEM_WAITING:
			summary->waiting++;
			summary->left_bytes += item->size;
			break;
		case AE_COLL_ITEM_GETTING:
			summary->getting++;
			summary->left_bytes += item->size;
			break;
		default:
			summary->absent++;
			summary->absent_bytes += item->size;
			summary->unknown_absent += item->size == 0;
			break;
		}
	}
}

void ae_coll_row_value(struct ae_coll_summary const *summary, int room_ok, char *text, int size, int *ink)
{
	char bytes[24];

	*ink = AE_COLL_INK_MUTED;
	if (size <= 0)
		return;
	if (summary->cut)
	{
		snprintf(text, (size_t)size, "TOO MANY MAPS");
		*ink = AE_COLL_INK_WARNING;
	}
	else if (summary->getting)
	{
		snprintf(text, (size_t)size, "GETTING %d OF %d", summary->present, summary->count);
		*ink = AE_COLL_INK_ACCENT;
	}
	else if (summary->waiting)
		snprintf(text, (size_t)size, "WAITING");
	else if (summary->absent == 0)
	{
		if (summary->updates)
		{
			snprintf(text, (size_t)size, "UPDATE");
			*ink = AE_COLL_INK_ACCENT;
		}
		else
			snprintf(text, (size_t)size, "INSTALLED");
	}
	else if (!room_ok)
	{
		snprintf(text, (size_t)size, "NO ROOM");
		*ink = AE_COLL_INK_WARNING;
	}
	else if (summary->present > 0)
		snprintf(text, (size_t)size, "%d OF %d ON THIS PC", summary->present, summary->count);
	else
	{
		ae_coll_size_text(summary->total_bytes, bytes, (int)sizeof(bytes));
		if (summary->sizes_known && bytes[0])
			snprintf(text, (size_t)size, "%d MAPS " BULLET " %s", summary->count, bytes);
		else
			snprintf(text, (size_t)size, "%d MAPS", summary->count);
	}
}

int ae_coll_first_action(struct ae_coll_summary const *summary)
{
	if (summary->cut)
		return AE_COLL_ACT_NONE;
	if (summary->getting)
		return AE_COLL_ACT_STOP;
	if (summary->waiting)
		return AE_COLL_ACT_STOP_WAITING;
	if (summary->absent > 0)
		return summary->present > 0 ? AE_COLL_ACT_GET : AE_COLL_ACT_GET_ALL;
	if (summary->updates > 0)
		return AE_COLL_ACT_UPDATES;
	return AE_COLL_ACT_NONE;
}

unsigned long long ae_coll_get_bytes(struct ae_coll_summary const *summary)
{
	return summary->absent > 0 ? summary->absent_bytes : summary->update_bytes;
}

void ae_coll_action_text(int action, struct ae_coll_summary const *summary, int room_ok, char *label, int label_size,
	char *value, int value_size, int *ink)
{
	*ink = AE_COLL_INK_MUTED;
	if (label_size > 0)
		label[0] = 0;
	if (value_size > 0)
		value[0] = 0;
	if (label_size <= 0 || value_size <= 0)
		return;
	switch (action)
	{
	case AE_COLL_ACT_GET:
		snprintf(label, (size_t)label_size, "GET THE %d MAP%s NOT ON THIS PC", summary->absent, summary->absent == 1 ? "" : "S");
		break;
	case AE_COLL_ACT_GET_ALL:
		snprintf(label, (size_t)label_size, "GET ALL %d MAPS", summary->absent);
		break;
	case AE_COLL_ACT_UPDATES:
		snprintf(label, (size_t)label_size, "GET %d UPDATE%s", summary->updates, summary->updates == 1 ? "" : "S");
		break;
	case AE_COLL_ACT_STOP:
		snprintf(label, (size_t)label_size, "STOP GETTING THEM");
		snprintf(value, (size_t)value_size, "%d OF %d", summary->present, summary->count);
		*ink = AE_COLL_INK_ACCENT;
		return;
	case AE_COLL_ACT_STOP_WAITING:
		snprintf(label, (size_t)label_size, "STOP WAITING");
		snprintf(value, (size_t)value_size, "WAITING");
		return;
	default:
		return;
	}
	if (summary->failed_get > 0)
	{
		snprintf(value, (size_t)value_size, "NO ANSWER");
		*ink = AE_COLL_INK_WARNING;
	}
	else if (ae_coll_get_unknown(summary) > 0)
		snprintf(value, (size_t)value_size, "ASKING STEAM");
	else if (!room_ok)
	{
		snprintf(value, (size_t)value_size, "NO ROOM");
		*ink = AE_COLL_INK_WARNING;
	}
	else
		ae_coll_size_text(ae_coll_get_bytes(summary), value, value_size);
}

int ae_coll_get_ids(struct ae_coll const *coll, struct ae_coll_summary const *summary, char (*ids)[AE_COLL_ID], int maximum)
{
	int index, count = 0, want = summary->absent > 0 ? AE_COLL_ITEM_ABSENT : AE_COLL_ITEM_UPDATE;

	for (index = 0; index < coll->count && index < AE_COLL_ITEMS && count < maximum; index++)
		if (coll->items[index].state == want)
			snprintf(ids[count++], AE_COLL_ID, "%s", coll->items[index].id);
	return count;
}

int ae_coll_stop_ids(struct ae_coll const *coll, char (*ids)[AE_COLL_ID], int maximum)
{
	int index, count = 0;

	for (index = 0; index < coll->count && index < AE_COLL_ITEMS && count < maximum; index++)
		if (coll->items[index].state == AE_COLL_ITEM_WAITING || coll->items[index].state == AE_COLL_ITEM_GETTING)
			snprintf(ids[count++], AE_COLL_ID, "%s", coll->items[index].id);
	return count;
}

int ae_coll_remove_ids(struct ae_coll const *coll, char (*ids)[AE_COLL_ID], int maximum)
{
	int index, count = 0;

	for (index = 0; index < coll->count && index < AE_COLL_ITEMS && count < maximum; index++)
	{
		struct ae_coll_item const *item = &coll->items[index];

		if ((item->state == AE_COLL_ITEM_PRESENT || item->state == AE_COLL_ITEM_UPDATE) && !item->steam_owned)
			snprintf(ids[count++], AE_COLL_ID, "%s", item->id);
	}
	return count;
}

int ae_coll_get_asks(unsigned long long bytes)
{
	return bytes >= (unsigned long long)AE_COLL_ASK_MEGABYTES * 1000ull * 1000ull;
}

int ae_coll_room(unsigned long long free_bytes, unsigned long long bytes)
{
	return !free_bytes || bytes <= free_bytes;
}

int ae_coll_get_unknown(struct ae_coll_summary const *summary)
{
	return summary->absent > 0 ? summary->unknown_absent : summary->unknown_update;
}

int ae_coll_get_ready(struct ae_coll_summary const *summary)
{
	return ae_coll_get_unknown(summary) == 0 && summary->failed_get == 0;
}

int ae_coll_room_for(struct ae_coll_summary const *summary, unsigned long long free_bytes)
{
	return ae_coll_get_unknown(summary) > 0 || ae_coll_room(free_bytes, ae_coll_get_bytes(summary));
}

void ae_coll_item_value(struct ae_coll_item const *item, char *text, int size, int *ink)
{
	*ink = AE_COLL_INK_MUTED;
	if (size <= 0)
		return;
	text[0] = 0;
	switch (item->state)
	{
	case AE_COLL_ITEM_PRESENT:
		snprintf(text, (size_t)size, "%s", item->steam_owned == 1 ? "IN STEAM" : item->steam_owned ? "ON THIS PC" : "INSTALLED");
		break;
	case AE_COLL_ITEM_UPDATE:
		snprintf(text, (size_t)size, "UPDATE");
		*ink = AE_COLL_INK_ACCENT;
		break;
	case AE_COLL_ITEM_WAITING:
		snprintf(text, (size_t)size, "WAITING");
		break;
	case AE_COLL_ITEM_GETTING:
		snprintf(text, (size_t)size, "%d %%", item->percent < 0 ? 0 : item->percent > 100 ? 100 : item->percent);
		*ink = AE_COLL_INK_ACCENT;
		break;
	default:
		if (item->failed)
		{
			snprintf(text, (size_t)size, "NO ANSWER");
			*ink = AE_COLL_INK_WARNING;
		}
		else
			ae_coll_size_text(item->size, text, size);
		break;
	}
}

void ae_coll_get_body(char const *name, struct ae_coll_summary const *summary, unsigned long long free_bytes, char *text, int size)
{
	char bytes[24], room[24];
	int maps = summary->absent > 0 ? summary->absent : summary->updates;

	(void)name;
	ae_coll_size_text(ae_coll_get_bytes(summary), bytes, (int)sizeof(bytes));
	ae_coll_size_text(free_bytes, room, (int)sizeof(room));
	if (size <= 0)
		return;
	snprintf(text, (size_t)size, "%d map%s%s%s, from Steam. They install one after another, and keep coming while you play.%s%s%s",
		maps, maps == 1 ? "" : "s", bytes[0] ? ", " : "", bytes, room[0] ? " This PC's maps drive has " : "", room,
		room[0] ? " free." : " The free space on this PC's maps drive is not known here; each map is checked before it starts.");
}

void ae_coll_remove_body(char const *name, struct ae_coll const *coll, struct ae_coll_summary const *summary, char *text, int size)
{
	char bytes[24], kept[160];
	int index, shown = 0;

	(void)name;
	if (size <= 0)
		return;
	ae_coll_size_text(summary->removable_bytes, bytes, (int)sizeof(bytes));
	kept[0] = 0;
	if (summary->steam_kept == 1 || summary->steam_kept == 2)
	{
		for (index = 0; index < coll->count && index < AE_COLL_ITEMS; index++)
		{
			struct ae_coll_item const *item = &coll->items[index];
			char map[AE_COLL_TITLE];
			size_t used = strlen(kept);

			if (!(item->state == AE_COLL_ITEM_PRESENT || item->state == AE_COLL_ITEM_UPDATE) || !item->steam_owned)
				continue;
			ae_coll_map_name(item->title, map, (int)sizeof(map));
			snprintf(kept + used, sizeof(kept) - used, "%s%s", shown ? " and " : "", map);
			shown++;
		}
		snprintf(kept + strlen(kept), sizeof(kept) - strlen(kept), " %s", shown == 1 ? "stays" : "stay");
	}
	else if (summary->steam_kept > 2)
		snprintf(kept, sizeof(kept), "%d maps stay", summary->steam_kept);
	snprintf(text, (size_t)size, "%d map%s%s%s, %s deleted from this PC.%s%s%s The offline playlists skip maps this PC doesn't have.",
		summary->removable, summary->removable == 1 ? "" : "s", bytes[0] ? ", " : "", bytes,
		summary->removable == 1 ? "is" : "are", kept[0] ? " " : "", kept,
		kept[0] ? ": this game did not install them (Steam did, or they were put there by hand)." : "");
}
