/*
AE_PICKER_FOLDERS.C

The player's own folders of maps and of gametypes. See ae_picker_folders.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_picker_folders.h"

enum { LINE = 256 };

static const char *const kind_words[AE_PF_KINDS] = { "map", "gametype" };
static const char *const kind_headings[AE_PF_KINDS] = { "maps", "gametypes" };

static char lower(char c)
{
	return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
}

/* two names the same, letters' case aside */
static int same_name(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
	{
		if (lower(*a) != lower(*b))
			return 0;
	}
	return *a == *b;
}

/* A-Z, letters' case aside (then by case, so the order is the same each time) */
static int compare_names(const char *a, const char *b)
{
	const char *x = a, *y = b;

	for (; *x && *y; x++, y++)
	{
		if (lower(*x) != lower(*y))
			return (unsigned char)lower(*x) - (unsigned char)lower(*y);
	}
	if (*x || *y)
		return *x ? 1 : -1;
	return strcmp(a, b);
}

static int name_character(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' ||
		c == '_' || c == '\'';
}

/* an id: 1 to 63 printable characters, no space at either end */
static int id_valid(const char *id)
{
	size_t length = id ? strlen(id) : 0, index;

	if (!length || length >= AE_PF_ID || id[0] == ' ' || id[length - 1] == ' ')
		return 0;
	for (index = 0; index < length; index++)
	{
		if (id[index] < 0x20 || id[index] > 0x7E)
			return 0;
	}
	return 1;
}

void ae_pf_init(struct ae_picker_folders *store, int kind)
{
	if (!store)
		return;
	memset(store, 0, sizeof(*store));
	store->kind = kind >= 0 && kind < AE_PF_KINDS ? kind : AE_PF_MAPS;
	store->folder_count = 1;
	snprintf(store->names[AE_PF_FAVOURITES], AE_PF_NAME, "Favourites");
}

const char *ae_pf_name(struct ae_picker_folders const *store, short folder)
{
	return store && folder >= 0 && folder < store->folder_count ? store->names[folder] : "";
}

short ae_pf_find(struct ae_picker_folders const *store, const char *name)
{
	short folder;

	for (folder = 0; store && name && folder < store->folder_count; folder++)
	{
		if (same_name(store->names[folder], name))
			return folder;
	}
	return -1;
}

int ae_pf_name_check(struct ae_picker_folders const *store, const char *name, short except)
{
	size_t length = name ? strlen(name) : 0, index;
	short found;

	if (!length)
		return AE_PF_NAME_EMPTY;
	for (index = 0; index < length; index++)
	{
		if (!name_character(name[index]))
			return AE_PF_NAME_CHARACTERS;
	}
	/* (spaces alone are no name; one at an end is the typing's, not the name's) */
	if (name[0] == ' ' || name[length - 1] == ' ')
	{
		for (index = 0; index < length && name[index] == ' '; index++)
			;
		return index == length ? AE_PF_NAME_EMPTY : AE_PF_NAME_CHARACTERS;
	}
	if (length > AE_PF_NAME_LETTERS)
		return AE_PF_NAME_LONG;
	found = ae_pf_find(store, name);
	if (found >= 0 && found != except)
		return AE_PF_NAME_TAKEN;
	return AE_PF_NAME_OK;
}

void ae_pf_new_name(struct ae_picker_folders const *store, char *name, int size)
{
	int number;

	if (!name || size <= 0)
		return;
	snprintf(name, (size_t)size, "New Folder");
	for (number = 2; ae_pf_find(store, name) >= 0 && number < 1000; number++)
		snprintf(name, (size_t)size, "New Folder %d", number);
}

/* the folders past FAVOURITES put A-Z again, the entries' folders with them; returns where folder went */
static short sort_folders(struct ae_picker_folders *store, short folder)
{
	short order[AE_PF_FOLDERS], place[AE_PF_FOLDERS], index, at;
	char names[AE_PF_FOLDERS][AE_PF_NAME];
	int entry;

	order[0] = 0;
	for (index = 1; index < store->folder_count; index++)
	{
		for (at = index; at > 1 && compare_names(store->names[order[at - 1]], store->names[index]) > 0; at--)
			order[at] = order[at - 1];
		order[at] = index;
	}
	memcpy(names, store->names, sizeof(names));
	for (index = 0; index < store->folder_count; index++)
	{
		place[order[index]] = index;
		memcpy(store->names[index], names[order[index]], AE_PF_NAME);
	}
	for (entry = 0; entry < store->entry_count; entry++)
		store->entries[entry].folder = place[store->entries[entry].folder];
	return folder >= 0 && folder < store->folder_count ? place[folder] : -1;
}

short ae_pf_folder_add(struct ae_picker_folders *store, const char *name)
{
	short folder;

	if (!store || store->folder_count >= AE_PF_FOLDERS || ae_pf_name_check(store, name, -1) != AE_PF_NAME_OK)
		return -1;
	folder = store->folder_count++;
	snprintf(store->names[folder], AE_PF_NAME, "%s", name);
	return sort_folders(store, folder);
}

short ae_pf_folder_rename(struct ae_picker_folders *store, short folder, const char *name)
{
	if (!store || folder <= AE_PF_FAVOURITES || folder >= store->folder_count ||
		ae_pf_name_check(store, name, folder) != AE_PF_NAME_OK)
		return -1;
	snprintf(store->names[folder], AE_PF_NAME, "%s", name);
	return sort_folders(store, folder);
}

int ae_pf_folder_delete(struct ae_picker_folders *store, short folder)
{
	int entry, kept = 0;
	short index;

	if (!store || folder <= AE_PF_FAVOURITES || folder >= store->folder_count)
		return 0;
	for (entry = 0; entry < store->entry_count; entry++)
	{
		if (store->entries[entry].folder == folder)
			continue;
		store->entries[kept] = store->entries[entry];
		if (store->entries[kept].folder > folder)
			store->entries[kept].folder--;
		kept++;
	}
	store->entry_count = kept;
	for (index = folder; index < store->folder_count - 1; index++)
		memcpy(store->names[index], store->names[index + 1], AE_PF_NAME);
	store->folder_count--;
	return 1;
}

/* ---------- entries */

int ae_pf_has(struct ae_picker_folders const *store, short folder, const char *id)
{
	int entry;

	for (entry = 0; store && id && entry < store->entry_count; entry++)
	{
		if (store->entries[entry].folder == folder && !strcmp(store->entries[entry].id, id))
			return 1;
	}
	return 0;
}

short ae_pf_count(struct ae_picker_folders const *store, short folder)
{
	int entry;
	short count = 0;

	for (entry = 0; store && entry < store->entry_count; entry++)
		count = (short)(count + (store->entries[entry].folder == folder));
	return count;
}

int ae_pf_put(struct ae_picker_folders *store, short folder, const char *id)
{
	struct ae_pf_entry *entry;

	if (!store || folder < 0 || folder >= store->folder_count || !id_valid(id))
		return -1;
	if (ae_pf_has(store, folder, id))
		return 0;
	if (store->entry_count >= AE_PF_ENTRIES || ae_pf_count(store, folder) >= AE_PF_FOLDER_ENTRIES)
		return -1;
	entry = &store->entries[store->entry_count++];
	entry->folder = folder;
	snprintf(entry->id, AE_PF_ID, "%s", id);
	return 1;
}

int ae_pf_take(struct ae_picker_folders *store, short folder, const char *id)
{
	int entry;

	for (entry = 0; store && id && entry < store->entry_count; entry++)
	{
		if (store->entries[entry].folder != folder || strcmp(store->entries[entry].id, id))
			continue;
		memmove(&store->entries[entry], &store->entries[entry + 1],
			sizeof(store->entries[0]) * (size_t)(store->entry_count - entry - 1));
		store->entry_count--;
		return 1;
	}
	return 0;
}

/* ---------- the file */

/* the next line (LF or CRLF) trimmed of spaces and tabs at both ends into line; NULL at the text's end */
static const char *next_line(const char *text, char *line, int *too_long)
{
	size_t length = 0, start = 0;

	if (!*text)
		return NULL;
	*too_long = 0;
	while (*text && *text != '\n')
	{
		if (length < LINE - 1)
			line[length++] = *text;
		else
			*too_long = 1;
		text++;
	}
	if (*text == '\n')
		text++;
	while (length && (line[length - 1] == '\r' || line[length - 1] == ' ' || line[length - 1] == '\t'))
		length--;
	line[length] = 0;
	while (line[start] == ' ' || line[start] == '\t')
		start++;
	if (start)
		memmove(line, line + start, length - start + 1);
	return text;
}

/* "key = value": the key and the value (trimmed), 0 when the line is not one */
static int key_value(char *line, char **key, char **value)
{
	char *equals = strchr(line, '='), *end;

	if (!equals)
		return 0;
	*equals = 0;
	for (end = equals; end > line && (end[-1] == ' ' || end[-1] == '\t'); end--)
		end[-1] = 0;
	*key = line;
	*value = equals + 1;
	while (**value == ' ' || **value == '\t')
		(*value)++;
	return **key != 0;
}

int ae_pf_read(struct ae_picker_folders *store, int kind, const char *text)
{
	char line[LINE];
	short folder = -1;
	int too_long, dropped = 0;

	ae_pf_init(store, kind);
	if (!store || !text)
		return 0;
	/* (a UTF-8 byte-order mark: not the file's) */
	if (!strncmp(text, "\xEF\xBB\xBF", 3))
		text += 3;
	while ((text = next_line(text, line, &too_long)) != NULL)
	{
		char *key, *value;

		if (!line[0] || line[0] == '#')
			continue;
		if (!key_value(line, &key, &value))
			goto refused;
		if (!strcmp(key, "folder"))
		{
			folder = ae_pf_find(store, value);
			if (folder < 0)
				folder = ae_pf_folder_add(store, value);
			/* (a bad name or the 64: its entries dropped with it, -2) */
			if (folder < 0)
			{
				folder = -2;
				dropped++;
			}
			continue;
		}
		if (strcmp(key, kind_words[store->kind]) || folder == -1)
			goto refused;
		/* (the folder read last keeps its index until the next folder line: adding sorts the entries with it) */
		if (folder < 0 || too_long || ae_pf_put(store, folder, value) != 1)
			dropped++;
	}
	store->dropped = dropped;
	return 1;
refused:
	ae_pf_init(store, kind);
	return 0;
}

long ae_pf_write(struct ae_picker_folders const *store, char *buffer, long size)
{
	long length;
	short folder;
	int entry, written;

	if (!store || !buffer || size <= 0)
		return -1;
	length = snprintf(buffer, (size_t)size, "# AE folders (%s)\n", kind_headings[store->kind]);
	if (length < 0 || length >= size)
		return -1;
	for (folder = 0; folder < store->folder_count; folder++)
	{
		written = snprintf(buffer + length, (size_t)(size - length), "folder = %s\n", store->names[folder]);
		if (written < 0 || written >= size - length)
			return -1;
		length += written;
		for (entry = 0; entry < store->entry_count; entry++)
		{
			if (store->entries[entry].folder != folder)
				continue;
			written = snprintf(buffer + length, (size_t)(size - length), "%s = %s\n", kind_words[store->kind],
				store->entries[entry].id);
			if (written < 0 || written >= size - length)
				return -1;
			length += written;
		}
	}
	return length;
}
