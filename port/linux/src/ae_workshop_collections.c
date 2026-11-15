/*
AE_WORKSHOP_COLLECTIONS.C

Steam's Workshop collections, as text. See ae_workshop_collections.h.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_workshop_collections.h"
#include "server_json.h"

/* the ids of the Halo CE collections the game asks Steam about (their pages: steamcommunity.com/sharedfiles/
filedetails/?id=<id>); what is in them and their titles are Steam's */
static const char *const known_ids[] = {
	"3001370390",   /* Pro Edition */
	"3001717490",   /* NHE Map Pack */
	"2899609719",   /* Social Map Pack */
	"3817409420"    /* 2v2 maps */
};
#define KNOWN_COUNT ((int)(sizeof(known_ids) / sizeof(known_ids[0])))

int ae_workshop_collections_id_count(void)
{
	return KNOWN_COUNT;
}

const char *ae_workshop_collections_id(int index)
{
	return index >= 0 && index < KNOWN_COUNT ? known_ids[index] : "";
}

void ae_wsc_table_init(struct ae_wsc_table *table)
{
	memset(table, 0, sizeof(*table));
}

static int is_id(const char *text)
{
	int index;

	for (index = 0; text[index]; index++)
	{
		if (text[index] < '0' || text[index] > '9')
			return 0;
	}
	return index > 0 && index < AE_WSC_ID;
}

static int body(const char *count_name, char *text, int size)
{
	int index, written = 0, asked = 0;

	if (!text || size <= 0)
		return 0;
	text[0] = 0;
	for (index = 0; index < KNOWN_COUNT; index++)
	{
		int more;

		if (!is_id(known_ids[index]))
			continue;
		more = snprintf(text + written, (size_t)(size - written), "%spublishedfileids%%5B%d%%5D=%s", asked ? "&" : "",
			asked, known_ids[index]);
		if (more < 0 || written + more >= size)
			break;
		written += more;
		asked++;
	}
	if (!asked)
	{
		text[0] = 0;
		return 0;
	}
	{
		char head[64];
		int length = snprintf(head, sizeof(head), "%s=%d&", count_name, asked);

		if (written + length >= size)
		{
			text[0] = 0;
			return 0;
		}
		memmove(text + length, text, (size_t)written + 1);
		memcpy(text, head, (size_t)length);
	}
	return asked;
}

int ae_wsc_children_body(char *text, int size)
{
	return body("collectioncount", text, size);
}

int ae_wsc_titles_body(char *text, int size)
{
	return body("itemcount", text, size);
}

static int slot_of(struct ae_wsc_table const *table, const char *id)
{
	int index;

	for (index = 0; index < table->count; index++)
	{
		if (!strcmp(table->collections[index].id, id))
			return index;
	}
	return -1;
}

/* the table's slot for a known id: its place in the table follows the known ids' order, made when it is not there */
static int slot_make(struct ae_wsc_table *table, const char *id)
{
	int slot = slot_of(table, id);

	if (slot >= 0)
		return slot;
	if (table->count >= AE_WSC_COLLECTIONS || !is_id(id))
		return -1;
	memset(&table->collections[table->count], 0, sizeof(table->collections[0]));
	snprintf(table->collections[table->count].id, AE_WSC_ID, "%s", id);
	return table->count++;
}

static int known(const char *id)
{
	int index;

	for (index = 0; index < KNOWN_COUNT; index++)
	{
		if (!strcmp(known_ids[index], id))
			return 1;
	}
	return 0;
}

/* a member of an object as text: a JSON string, or a number as the digits it was written with */
static int member_text(struct server_json_document const *document, int object, const char *key, char *text, int size)
{
	int token = server_json_member(document, object, key);
	double number;

	if (token < 0)
		return 0;
	if (server_json_get_string(document, token, text, size))
		return 1;
	if (server_json_get_number(document, token, &number) && number >= 0 && number < 1e15)
	{
		snprintf(text, (size_t)size, "%.0f", number);
		return 1;
	}
	return 0;
}

int ae_wsc_read_children(struct ae_wsc_table *table, const char *text, int length)
{
	struct server_json_document document;
	int response, details, index, read = 0;

	if (!table || !text || length <= 0 || !server_json_parse(&document, text, length, 16))
		return 0;
	response = server_json_member(&document, 0, "response");
	details = response >= 0 ? server_json_member(&document, response, "collectiondetails") : -1;
	for (index = 0; details >= 0; index++)
	{
		int entry = server_json_element(&document, details, index), children, child, slot;
		char id[AE_WSC_ID], result[16];

		if (entry < 0)
			break;
		/* (result 1 is a collection read; 9 and the others: not found, left alone) */
		if (!member_text(&document, entry, "publishedfileid", id, sizeof(id)) || !known(id) ||
			!member_text(&document, entry, "result", result, sizeof(result)) || strcmp(result, "1"))
			continue;
		slot = slot_make(table, id);
		if (slot < 0)
			continue;
		table->collections[slot].count = 0;
		children = server_json_member(&document, entry, "children");
		for (child = 0; children >= 0; child++)
		{
			int item = server_json_element(&document, children, child);
			char item_id[AE_WSC_ID];

			if (item < 0)
				break;
			if (table->collections[slot].count < AE_WSC_ITEMS && member_text(&document, item, "publishedfileid", item_id,
				sizeof(item_id)) && is_id(item_id))
			{
				snprintf(table->collections[slot].items[table->collections[slot].count++], AE_WSC_ID, "%s", item_id);
			}
		}
		read++;
	}
	server_json_document_free(&document);
	return read;
}

void ae_wsc_clean_title(const char *title, char *name, int size)
{
	char work[256];
	char *at, *dash;
	size_t index, length;

	if (size <= 0)
		return;
	name[0] = 0;
	if (!title)
		return;
	snprintf(work, sizeof(work), "%s", title);
	at = work;
	/* (a leading "Halo CE - ", "Halo: CE - ", "Halo: Combat Evolved - "...: the game's name and a dash) */
	dash = strstr(at, " - ");
	if (dash && dash - at < 28 && (!strncmp(at, "Halo", 4) || !strncmp(at, "HALO", 4)))
		at = dash + 3;
	while (*at == ' ')
		at++;
	length = strlen(at);
	while (length && (at[length - 1] == ' ' || at[length - 1] == '\r' || at[length - 1] == '\n'))
		at[--length] = 0;
	for (index = 0; at[index] && index < (size_t)size - 1; index++)
	{
		unsigned char c = (unsigned char)at[index];

		/* (a control character or a byte past ASCII is not kept: the menus' font has the ASCII) */
		if (c < 0x20 || c >= 0x7F)
			c = ' ';
		name[index] = c >= 'a' && c <= 'z' ? (char)(c - 32) : (char)c;
	}
	name[index] = 0;
}

int ae_wsc_read_titles(struct ae_wsc_table *table, const char *text, int length)
{
	struct server_json_document document;
	int response, details, index, named = 0;

	if (!table || !text || length <= 0 || !server_json_parse(&document, text, length, 16))
		return 0;
	response = server_json_member(&document, 0, "response");
	details = response >= 0 ? server_json_member(&document, response, "publishedfiledetails") : -1;
	for (index = 0; details >= 0; index++)
	{
		int entry = server_json_element(&document, details, index), slot;
		char id[AE_WSC_ID], title[256];

		if (entry < 0)
			break;
		if (!member_text(&document, entry, "publishedfileid", id, sizeof(id)) || !member_text(&document, entry, "title",
			title, sizeof(title)))
			continue;
		slot = slot_of(table, id);
		if (slot < 0)
			continue;
		ae_wsc_clean_title(title, table->collections[slot].name, AE_WSC_NAME);
		named += table->collections[slot].name[0] != 0;
	}
	server_json_document_free(&document);
	return named;
}

int ae_wsc_prune(struct ae_wsc_table *table)
{
	int from, to = 0, index;
	struct ae_wsc_table *copy = malloc(sizeof(*copy));

	if (!copy)
		return table->count;
	/* (the known ids' order, not the answer's) */
	memset(copy, 0, sizeof(*copy));
	copy->fetched = table->fetched;
	for (index = 0; index < KNOWN_COUNT; index++)
	{
		from = slot_of(table, known_ids[index]);
		if (from >= 0 && table->collections[from].name[0] && table->collections[from].count > 0)
			copy->collections[to++] = table->collections[from];
	}
	copy->count = to;
	memcpy(table, copy, sizeof(*table));
	free(copy);
	return table->count;
}

long ae_wsc_cache_write(struct ae_wsc_table const *table, char *buffer, long size)
{
	long length;
	int index, item;

	if (!table || !buffer || size <= 0)
		return -1;
	length = snprintf(buffer, (size_t)size, "# AE Workshop collections, as Steam last answered\nfetched\t%lld\n",
		table->fetched);
	for (index = 0; index < table->count && length >= 0 && length < size; index++)
	{
		struct ae_wsc_collection const *collection = &table->collections[index];

		length += snprintf(buffer + length, (size_t)(size - length), "collection\t%s\t%s\t", collection->id,
			collection->name);
		for (item = 0; item < collection->count && length >= 0 && length < size; item++)
			length += snprintf(buffer + length, (size_t)(size - length), "%s%s", item ? "," : "", collection->items[item]);
		if (length >= 0 && length < size)
			length += snprintf(buffer + length, (size_t)(size - length), "\n");
	}
	return length < 0 || length >= size ? -1 : length;
}

int ae_wsc_cache_read(struct ae_wsc_table *table, const char *text)
{
	const char *line = text;

	if (!table || !text)
		return 0;
	ae_wsc_table_init(table);
	while (*line)
	{
		const char *end = strchr(line, '\n');
		size_t length = end ? (size_t)(end - line) : strlen(line);
		char *copy = malloc(length + 1), *fields[4], *cursor;
		int count = 0;

		if (!copy)
			return 0;
		memcpy(copy, line, length);
		copy[length] = 0;
		if (length && copy[length - 1] == '\r')
			copy[length - 1] = 0;
		for (cursor = copy; count < 4 && cursor; count++)
		{
			fields[count] = cursor;
			cursor = strchr(cursor, '\t');
			if (cursor)
				*cursor++ = 0;
		}
		if (count == 2 && !strcmp(fields[0], "fetched"))
			table->fetched = atoll(fields[1]);
		else if (count == 4 && !strcmp(fields[0], "collection") && known(fields[1]) && fields[2][0])
		{
			int slot = slot_make(table, fields[1]);

			if (slot >= 0)
			{
				char *item = fields[3], *comma;

				snprintf(table->collections[slot].name, AE_WSC_NAME, "%s", fields[2]);
				table->collections[slot].count = 0;
				while (item && *item && table->collections[slot].count < AE_WSC_ITEMS)
				{
					comma = strchr(item, ',');
					if (comma)
						*comma = 0;
					if (is_id(item))
						snprintf(table->collections[slot].items[table->collections[slot].count++], AE_WSC_ID, "%s", item);
					item = comma ? comma + 1 : NULL;
				}
			}
		}
		free(copy);
		line = end ? end + 1 : line + length;
	}
	ae_wsc_prune(table);
	return 1;
}

int ae_wsc_stale(struct ae_wsc_table const *table, long long now)
{
	return !table || table->count <= 0 || table->fetched <= 0 || now < table->fetched ||
		now - table->fetched >= AE_WSC_REFRESH_SECONDS;
}

int ae_wsc_of(struct ae_wsc_table const *table, const char *item_id, int *indices, int maximum)
{
	int collection, item, written = 0;

	if (!table || !item_id || !indices)
		return 0;
	for (collection = 0; collection < table->count && written < maximum; collection++)
	{
		for (item = 0; item < table->collections[collection].count; item++)
		{
			if (!strcmp(table->collections[collection].items[item], item_id))
			{
				indices[written++] = collection;
				break;
			}
		}
	}
	return written;
}
