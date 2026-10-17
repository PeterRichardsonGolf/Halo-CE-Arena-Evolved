/*
AE_MAP_FOLDERS_RULES.C

The map lists' pages (ae_map_folders_rules.h). Pure C: no game, no files.
*/

#include <stdio.h>
#include <string.h>

#include "ae_map_folders_rules.h"

static int lower(int c)
{
	return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

static int equal_folded(char const *a, char const *b)
{
	while (*a && lower((unsigned char)*a) == lower((unsigned char)*b))
	{
		a++;
		b++;
	}
	return lower((unsigned char)*a) == lower((unsigned char)*b);
}

int ae_map_folders_folder_is_page(char const *folder)
{
	static char const *const skipped[] = { "ce", "previews", "layouts", "_shared" };
	size_t length, index;

	if (!folder || !folder[0] || folder[0] == '.' || folder[0] == '_')
		return 0;
	length = strlen(folder);
	if (length >= AE_MAP_FOLDER_NAME_SIZE || strpbrk(folder, "/\\:|"))
		return 0;
	for (index = 0; index < length; index++)
	{
		if ((unsigned char)folder[index] < 0x20 || folder[index] == 0x7F)
			return 0;
	}
	/* (a folder name with spaces at its ends is shown as another's) */
	if (folder[0] == ' ' || folder[length - 1] == ' ')
		return 0;
	for (index = 0; index < sizeof(skipped) / sizeof(skipped[0]); index++)
	{
		if (equal_folded(folder, skipped[index]))
			return 0;
	}
	return 1;
}

int ae_map_folders_page_name(char const *folder, char *out, unsigned int size)
{
	unsigned int index;

	if (!out || !size)
		return 0;
	out[0] = 0;
	if (!ae_map_folders_folder_is_page(folder))
		return 0;
	for (index = 0; index + 1 < size && folder[index]; index++)
	{
		unsigned char c = (unsigned char)folder[index];

		if (c >= 'a' && c <= 'z')
			c = (unsigned char)(c - 'a' + 'A');
		else if (c > 0x7E)
			c = '?';
		out[index] = (char)c;
	}
	/* (cut at the page's size: no space left at its end) */
	while (index > 0 && out[index - 1] == ' ')
		index--;
	out[index] = 0;
	if (!strcmp(out, AE_MAP_PAGE_STOCK) || !strcmp(out, AE_MAP_PAGE_LOOSE) || !strcmp(out, AE_MAP_PAGE_SINGLEPLAYER))
	{
		out[0] = 0;
		return 0;
	}
	return out[0] != 0;
}

int ae_map_folders_compare(char const *a, char const *b)
{
	char const *x = a, *y = b;

	while (*x && lower((unsigned char)*x) == lower((unsigned char)*y))
	{
		x++;
		y++;
	}
	if (lower((unsigned char)*x) != lower((unsigned char)*y))
		return lower((unsigned char)*x) < lower((unsigned char)*y) ? -1 : 1;
	return strcmp(a, b);
}

void ae_map_folders_sort(char (*folders)[AE_MAP_FOLDER_NAME_SIZE], int count)
{
	int i, j;

	/* (a few dozen folders: an insertion sort) */
	for (i = 1; i < count; i++)
	{
		char held[AE_MAP_FOLDER_NAME_SIZE];

		memcpy(held, folders[i], sizeof(held));
		for (j = i; j > 0 && ae_map_folders_compare(folders[j - 1], held) > 0; j--)
			memcpy(folders[j], folders[j - 1], sizeof(held));
		memcpy(folders[j], held, sizeof(held));
	}
}

void ae_map_folder_index_clear(struct ae_map_folder_index *index)
{
	if (index)
		index->count = 0;
}

char const *ae_map_folder_index_find(struct ae_map_folder_index const *index, char const *name)
{
	int entry;

	if (!index || !name)
		return NULL;
	for (entry = 0; entry < index->count; entry++)
	{
		if (equal_folded(index->names[entry], name))
			return index->folders[entry];
	}
	return NULL;
}

int ae_map_folder_index_add(struct ae_map_folder_index *index, char const *name, char const *folder)
{
	if (!index || !name || !name[0] || !folder || strlen(name) >= AE_MAP_NAME_SIZE ||
		strlen(folder) >= AE_MAP_FOLDER_NAME_SIZE)
	{
		return -1;
	}
	if (ae_map_folder_index_find(index, name))
		return 0;
	if (index->count >= AE_MAP_FOLDER_INDEX_SIZE)
		return -1;
	strcpy(index->names[index->count], name);
	strcpy(index->folders[index->count], folder);
	index->count++;
	return 1;
}

static char const stock_page[] = AE_MAP_PAGE_STOCK;
static char const loose_page[] = AE_MAP_PAGE_LOOSE;

/* the page a map goes on: STOCK, a folder's page (one named as the list's own pages are is none), MULTIPLAYER */
static char const *map_page(int map, unsigned char const *stock, char const *const *folder_pages)
{
	char const *folder = folder_pages ? folder_pages[map] : NULL;

	if (stock && stock[map])
		return stock_page;
	if (folder && folder[0] && strcmp(folder, AE_MAP_PAGE_STOCK) && strcmp(folder, AE_MAP_PAGE_LOOSE) &&
		strcmp(folder, AE_MAP_PAGE_SINGLEPLAYER))
	{
		return folder;
	}
	return loose_page;
}

void ae_map_pages_build(struct ae_map_pages *pages, int map_count, unsigned char const *stock,
	char const *const *folder_pages)
{
	char folder_names[AE_MAP_PAGES_MAXIMUM][AE_MAP_PAGE_NAME_SIZE];
	int folder_count = 0, map, page, filled = 0;

	if (!pages)
		return;
	memset(pages, 0, sizeof(*pages));
	if (map_count < 0)
		map_count = 0;
	/* the folders' page names (each once), A-Z; room kept for STOCK and MULTIPLAYER */
	for (map = 0; map < map_count; map++)
	{
		char const *name = map_page(map, stock, folder_pages);
		int known;

		if (name == stock_page || name == loose_page)
			continue;
		for (known = 0; known < folder_count && strcmp(folder_names[known], name); known++)
			;
		if (known < folder_count || folder_count >= AE_MAP_PAGES_MAXIMUM - 2 || strlen(name) >= AE_MAP_PAGE_NAME_SIZE)
			continue;
		strcpy(folder_names[folder_count++], name);
	}
	{
		int i, j;

		for (i = 1; i < folder_count; i++)
		{
			char held[AE_MAP_PAGE_NAME_SIZE];

			memcpy(held, folder_names[i], sizeof(held));
			for (j = i; j > 0 && strcmp(folder_names[j - 1], held) > 0; j--)
				memcpy(folder_names[j], folder_names[j - 1], sizeof(held));
			memcpy(folder_names[j], held, sizeof(held));
		}
	}
	strcpy(pages->names[pages->page_count++], AE_MAP_PAGE_STOCK);
	for (page = 0; page < folder_count; page++)
		strcpy(pages->names[pages->page_count++], folder_names[page]);
	strcpy(pages->names[pages->page_count++], AE_MAP_PAGE_LOOSE);
	/* each page's maps, in the list's order */
	for (page = 0; page < pages->page_count; page++)
	{
		pages->first[page] = filled;
		for (map = 0; map < map_count && filled < AE_MAP_PAGES_MAPS; map++)
		{
			char const *name = map_page(map, stock, folder_pages);
			int on;

			if (page == 0)
				on = name == stock_page;
			else if (page == pages->page_count - 1)
			{
				int known;

				/* (loose, or a folder whose page found no room) */
				for (known = 1; known < pages->page_count - 1 && strcmp(pages->names[known], name); known++)
					;
				on = name != stock_page && known == pages->page_count - 1;
			}
			else
				on = name != stock_page && name != loose_page && !strcmp(pages->names[page], name);
			if (on)
				pages->maps[filled++] = map;
		}
		pages->count[page] = filled - pages->first[page];
	}
	/* the empty pages left out */
	{
		int kept = 0;

		for (page = 0; page < pages->page_count; page++)
		{
			if (!pages->count[page])
				continue;
			if (kept != page)
			{
				memcpy(pages->names[kept], pages->names[page], sizeof(pages->names[kept]));
				pages->first[kept] = pages->first[page];
				pages->count[kept] = pages->count[page];
			}
			kept++;
		}
		pages->page_count = kept;
	}
}

int ae_map_pages_find(struct ae_map_pages const *pages, char const *name)
{
	int page;

	if (!pages || !name)
		return -1;
	for (page = 0; page < pages->page_count; page++)
	{
		if (!strcmp(pages->names[page], name))
			return page;
	}
	return -1;
}

int ae_map_pages_page_of(struct ae_map_pages const *pages, int map)
{
	int page;

	if (!pages || map < 0)
		return -1;
	for (page = 0; page < pages->page_count; page++)
	{
		if (ae_map_pages_entry_of(pages, page, map) >= 0)
			return page;
	}
	return -1;
}

int ae_map_pages_entry_of(struct ae_map_pages const *pages, int page, int map)
{
	int entry;

	if (!pages || page < 0 || page >= pages->page_count)
		return -1;
	for (entry = 0; entry < pages->count[page]; entry++)
	{
		if (pages->maps[pages->first[page] + entry] == map)
			return entry;
	}
	return -1;
}

int ae_map_pages_map_at(struct ae_map_pages const *pages, int page, int entry)
{
	if (!pages || page < 0 || page >= pages->page_count || entry < 0 || entry >= pages->count[page])
		return -1;
	return pages->maps[pages->first[page] + entry];
}

int ae_map_chooser_count(int page_count, int singleplayer)
{
	int count = (page_count > 0 ? page_count : 0) + (singleplayer ? 1 : 0);

	return count > 0 ? count : 1;
}

int ae_map_chooser_page(int value, int page_count, int singleplayer)
{
	int count = ae_map_chooser_count(page_count, singleplayer);

	if (value < 0)
		value = 0;
	if (value >= count)
		value = count - 1;
	if (singleplayer)
		value--;
	return value < 0 || page_count <= 0 ? -1 : value;
}

int ae_map_chooser_value(int page, int page_count, int singleplayer)
{
	if (page < 0 || page_count <= 0)
		return 0;
	if (page >= page_count)
		page = page_count - 1;
	return page + (singleplayer ? 1 : 0);
}
