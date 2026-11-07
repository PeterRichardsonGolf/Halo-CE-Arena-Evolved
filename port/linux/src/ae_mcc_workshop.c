/*
AE_MCC_WORKSHOP.C

MCC Workshop items' ModInfo.json and map json, read (ae_mcc_workshop.h).
*/

/* (readdir, stat and snprintf under -std=c99) */
#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#endif

#include "ae_mcc_workshop.h"
#include "server_json.h"

#include <stdio.h>
#include <string.h>

static void workshop_reason(char *reason, size_t size, const char *text)
{
	if (reason && size)
	{
		strncpy(reason, text, size - 1);
		reason[size - 1] = 0;
	}
}

/* parses text under the size and depth limits; 1 with document to free, else 0 with the reason */
static int workshop_parse(struct server_json_document *document, const char *text, size_t length, char *reason,
	size_t reason_size)
{
	if (!text || !length)
	{
		workshop_reason(reason, reason_size, "empty json");
		return 0;
	}
	if (length > AE_MCC_WORKSHOP_MAXIMUM_JSON)
	{
		workshop_reason(reason, reason_size, "json too large");
		return 0;
	}
	if (!server_json_parse(document, text, (int)length, AE_MCC_WORKSHOP_MAXIMUM_DEPTH))
	{
		workshop_reason(reason, reason_size, "not readable json");
		return 0;
	}
	if (document->count < 1 || document->tokens[0].type != _server_json_object)
	{
		server_json_document_free(document);
		workshop_reason(reason, reason_size, "json is not an object");
		return 0;
	}
	return 1;
}

/* a string member's text into out, cut at a whole UTF-8 character if longer than it holds; 1 if the member is a
string (else out is ""). A string beyond the scratch size reads as absent */
static int workshop_title(struct server_json_document const *document, int token, char *out, size_t size)
{
	char scratch[512];
	size_t length;

	out[0] = 0;
	if (token < 0 || !server_json_get_string(document, token, scratch, (int)sizeof(scratch)))
		return 0;
	length = strlen(scratch);
	if (length >= size)
	{
		length = size - 1;
		/* (not inside a multibyte character: back off its continuation bytes) */
		while (length > 0 && (((unsigned char)scratch[length]) & 0xC0) == 0x80)
			length--;
	}
	memcpy(out, scratch, length);
	out[length] = 0;
	return 1;
}

/* an item-relative path: not empty, no control characters, not absolute (a leading / or \, or a drive), no "..". Its
backslashes become '/' */
static int workshop_path_ok(const char *path)
{
	const char *c;

	if (!path[0] || path[0] == '/' || path[0] == '\\' || path[1] == ':')
		return 0;
	for (c = path; *c; c++)
	{
		if ((unsigned char)*c < 0x20 || *c == 0x7F)
			return 0;
		if (c[0] == '.' && c[1] == '.')
			return 0;
	}
	return 1;
}

int ae_mcc_workshop_read_item(const char *text, size_t length, struct ae_mcc_workshop_item *item,
	char *reason, size_t reason_size)
{
	struct server_json_document document;
	char engine[32], path[128];
	int token, maps, index, count;
	double number;

	memset(item, 0, sizeof(*item));
	if (!workshop_parse(&document, text, length, reason, reason_size))
		return 0;

	workshop_title(&document, server_json_member(&document, server_json_member(&document, 0, "Title"), "Neutral"),
		item->title, sizeof(item->title));
	if (server_json_get_string(&document, server_json_member(&document, 0, "Engine"), engine, (int)sizeof(engine)))
		item->halo1 = !strcmp(engine, "Halo1");

	token = server_json_member(&document, server_json_member(&document, server_json_member(&document, 0,
		"ModIdentifier"), "HostedModIds"), "SteamWorkshopId");
	if (token >= 0)
	{
		/* (a whole number, at most 18 digits: it is shown and compared as text) */
		if (!server_json_get_number(&document, token, &number) || number < 0.0 || number >= 1e18 ||
			number != (double)(long long)number)
		{
			server_json_document_free(&document);
			workshop_reason(reason, reason_size, "SteamWorkshopId is not a whole number");
			return 0;
		}
		sprintf(item->id, "%.0f", number);
	}

	maps = server_json_member(&document, server_json_member(&document, 0, "GameModContents"), "MultiplayerMaps");
	if (maps >= 0)
	{
		if (document.tokens[maps].type != _server_json_array)
		{
			server_json_document_free(&document);
			workshop_reason(reason, reason_size, "MultiplayerMaps is not a list");
			return 0;
		}
		count = document.tokens[maps].size;
		if (count > AE_MCC_WORKSHOP_MAXIMUM_ITEM_MAPS)
		{
			server_json_document_free(&document);
			workshop_reason(reason, reason_size, "too many MultiplayerMaps");
			return 0;
		}
		for (index = 0; index < count; index++)
		{
			char *c;

			if (!server_json_get_string(&document, server_json_element(&document, maps, index), path,
				(int)sizeof(path)) || !workshop_path_ok(path))
			{
				server_json_document_free(&document);
				workshop_reason(reason, reason_size, "a MultiplayerMaps path is not a plain relative path");
				return 0;
			}
			for (c = path; *c; c++)
			{
				if (*c == '\\')
					*c = '/';
			}
			strcpy(item->map_json[index], path);
		}
		item->map_count = count;
	}
	server_json_document_free(&document);
	return 1;
}

/* a map json's Images.<member>: an item-relative path with '/' separators, or "" (absent, not a string, too long,
or not a plain relative path) */
static void workshop_picture(struct server_json_document const *document, const char *member, char *out, size_t size)
{
	char *c;

	out[0] = 0;
	if (!server_json_get_string(document, server_json_member(document, server_json_member(document, 0, "Images"),
		member), out, (int)size) || !workshop_path_ok(out))
	{
		out[0] = 0;
		return;
	}
	for (c = out; *c; c++)
	{
		if (*c == '\\')
			*c = '/';
	}
}

int ae_mcc_workshop_read_map_info(const char *text, size_t length, struct ae_mcc_workshop_map_info *info,
	char *reason, size_t reason_size)
{
	struct server_json_document document;
	char scenario[256];
	const char *name, *c;
	size_t size;

	memset(info, 0, sizeof(*info));
	if (!workshop_parse(&document, text, length, reason, reason_size))
		return 0;
	if (!server_json_get_string(&document, server_json_member(&document, 0, "ScenarioFile"), scenario,
		(int)sizeof(scenario)) || !scenario[0])
	{
		server_json_document_free(&document);
		workshop_reason(reason, reason_size, "no usable ScenarioFile");
		return 0;
	}
	name = scenario;
	for (c = scenario; *c; c++)
	{
		if (*c == '/' || *c == '\\')
			name = c + 1;
	}
	size = strlen(name);
	if (!size || size >= sizeof(info->name))
	{
		server_json_document_free(&document);
		workshop_reason(reason, reason_size, "ScenarioFile's map name is empty or too long");
		return 0;
	}
	for (c = name; *c; c++)
	{
		if ((unsigned char)*c < 0x20 || *c == 0x7F)
		{
			server_json_document_free(&document);
			workshop_reason(reason, reason_size, "ScenarioFile's map name holds a control character");
			return 0;
		}
	}
	strcpy(info->name, name);
	workshop_title(&document, server_json_member(&document, server_json_member(&document, 0, "Title"), "Neutral"),
		info->title, sizeof(info->title));
	workshop_picture(&document, "Thumbnail", info->thumbnail, sizeof(info->thumbnail));
	workshop_picture(&document, "Large", info->large, sizeof(info->large));
	server_json_document_free(&document);
	return 1;
}

/* ---------- files and folders (host paths; Windows' are the ANSI code page's, in win32_ae_mcc.c) */

#include <stdlib.h>

#ifndef _WIN32
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#define WORKSHOP_HEADER_SIZE 0x800
#define WORKSHOP_APP_FOLDER "steamapps/workshop/content/" AE_MCC_STEAM_APP

#ifndef _WIN32
int ae_mcc_workshop_list_folder(const char *folder, ae_mcc_workshop_entry_function entry, void *context)
{
	DIR *dir = opendir(folder);
	struct dirent *item;
	char path[AE_MCC_PATH_SIZE];

	if (!dir)
		return 0;
	while ((item = readdir(dir)) != NULL)
	{
		int is_folder = 0, is_regular = 0;

		if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, ".."))
			continue;
		/* (not <sys/stat.h>: the game's build has the MSVC one. A link or an unknown type is told by opening it as a
		folder; one that is not is "regular" here, and ae_mcc_workshop_open then refuses a pipe or a device) */
		if (item->d_type == DT_DIR)
			is_folder = 1;
		else if (item->d_type == DT_REG)
			is_regular = 1;
		else if (item->d_type == DT_LNK || item->d_type == DT_UNKNOWN)
		{
			DIR *probe;

			snprintf(path, sizeof(path), "%s/%s", folder, item->d_name);
			probe = opendir(path);
			if (probe)
			{
				is_folder = 1;
				closedir(probe);
			}
			else
				is_regular = 1;
		}
		if (entry(context, item->d_name, is_folder, is_regular))
			break;
	}
	closedir(dir);
	return 1;
}

FILE *ae_mcc_workshop_open(const char *path)
{
	/* (O_NONBLOCK: a pipe named like a json does not block the open; lseek then refuses it, as it does a terminal) */
	int descriptor = open(path, O_RDONLY | O_NONBLOCK);
	FILE *file;

	if (descriptor < 0)
		return NULL;
	if (lseek(descriptor, 0, SEEK_END) < 0 || lseek(descriptor, 0, SEEK_SET) < 0 || !(file = fdopen(descriptor, "rb")))
	{
		close(descriptor);
		return NULL;
	}
	return file;
}

long long ae_mcc_workshop_file_size(FILE *file)
{
	/* (fseeko/ftello with off_t: 64-bit where the platform has it, as _FILE_OFFSET_BITS or the 64-bit ABI gives) */
	if (fseeko(file, 0, SEEK_END) != 0)
		return -1;
	return (long long)ftello(file);
}
#endif

struct workshop_find_context
{
	const char *name;
	int wanted_folder;
	char *found;
	size_t size;
	int result;
};

static int workshop_same(const char *a, const char *b);

static int workshop_find_entry(void *context, const char *name, int is_folder, int is_regular)
{
	struct workshop_find_context *find = context;

	if (find->wanted_folder ? !is_folder : !is_regular)
		return 0;
	if (!workshop_same(name, find->name) || strlen(name) >= find->size)
		return 0;
	if (!find->result || !strcmp(name, find->name))
	{
		strcpy(find->found, name);
		find->result = 1;
		if (!strcmp(name, find->name))
			return 1;
	}
	return 0;
}

static int workshop_lower(int c)
{
	return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

static int workshop_same(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
	{
		if (workshop_lower((unsigned char)*a) != workshop_lower((unsigned char)*b))
			return 0;
	}
	return *a == *b;
}

/* the entry in folder named like name, case-blind (an exact spelling wins); its real spelling into found. 1 if
found. wanted_folder: 1 a folder, 0 a regular file */
static int workshop_find(const char *folder, const char *name, int wanted_folder, char *found, size_t size)
{
	struct workshop_find_context find;

	find.name = name;
	find.wanted_folder = wanted_folder;
	find.found = found;
	find.size = size;
	find.result = 0;
	if (!ae_mcc_workshop_list_folder(folder, workshop_find_entry, &find))
		return 0;
	return find.result;
}

static int workshop_join(char *path, size_t size, const char *folder, const char *name)
{
	size_t length = strlen(folder);
	int separator = length && (folder[length - 1] == '/' || folder[length - 1] == '\\');
	int written = snprintf(path, size, "%s%s%s", folder, separator ? "" : "/", name);

	return written > 0 && (size_t)written < size;
}

/* a whole small file, terminated (NULL when unreadable, empty or over the json limit) */
static char *workshop_file_read(const char *path, size_t *length)
{
	FILE *file = ae_mcc_workshop_open(path);
	char *text = NULL;
	size_t got;

	*length = 0;
	if (!file)
		return NULL;
	text = malloc(AE_MCC_WORKSHOP_MAXIMUM_JSON + 2);
	if (text)
	{
		got = fread(text, 1, AE_MCC_WORKSHOP_MAXIMUM_JSON + 1, file);
		/* (one byte past the limit reads as too large: the reader refuses it) */
		if (!got)
		{
			free(text);
			text = NULL;
		}
		else
		{
			text[got] = 0;
			*length = got;
		}
	}
	fclose(file);
	return text;
}

/* ---------- the map file */

int ae_mcc_workshop_map_file_valid(const char *path, char *reason, size_t reason_size)
{
	unsigned char header[WORKSHOP_HEADER_SIZE];
	FILE *file = ae_mcc_workshop_open(path);
	unsigned long version, size;
	long long length, total;
	size_t got;

	if (!file)
	{
		workshop_reason(reason, reason_size, "map file cannot be opened");
		return 0;
	}
	got = fread(header, 1, sizeof(header), file);
	length = (long long)got;
	/* (a 64-bit size: a map of 2 GB or more is not "being updated") */
	if (got == sizeof(header) && (total = ae_mcc_workshop_file_size(file)) >= 0)
		length = total;
	fclose(file);
	if (got < 12 || memcmp(header, "daeh", 4))
	{
		workshop_reason(reason, reason_size, "not an MCC Halo CE map");
		return 0;
	}
	version = (unsigned long)header[4] | (unsigned long)header[5] << 8 | (unsigned long)header[6] << 16 |
		(unsigned long)header[7] << 24;
	size = (unsigned long)header[8] | (unsigned long)header[9] << 8 | (unsigned long)header[10] << 16 |
		(unsigned long)header[11] << 24;
	if (version != 13)
	{
		workshop_reason(reason, reason_size, "not an MCC Halo CE map");
		return 0;
	}
	if (length < (long long)size)
	{
		workshop_reason(reason, reason_size, "being updated by Steam");
		return 0;
	}
	if (length != (long long)size || got < sizeof(header) || memcmp(header + 0x7FC, "toof", 4))
	{
		workshop_reason(reason, reason_size, "not an MCC Halo CE map");
		return 0;
	}
	return 1;
}

/* ---------- the scan */

static int workshop_name_safe(const char *name)
{
	const char *c;

	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return 0;
	for (c = name; *c; c++)
	{
		if (*c == ':' || *c == '/' || *c == '\\')
			return 0;
	}
	return 1;
}

static void workshop_hidden(struct ae_mcc_workshop_map *map, const char *item_id, const char *item_title,
	const char *name, const char *title, const char *reason)
{
	memset(map, 0, sizeof(*map));
	snprintf(map->item_id, sizeof(map->item_id), "%s", item_id);
	snprintf(map->item_title, sizeof(map->item_title), "%s", item_title);
	snprintf(map->name, sizeof(map->name), "%s", name);
	snprintf(map->title, sizeof(map->title), "%s", title);
	map->status = AE_MCC_WORKSHOP_HIDDEN;
	snprintf(map->reason, sizeof(map->reason), "%s", reason);
}

/* an item-relative path ('/' separators) into host path, every part found case-blind in its folder (a folder for all
but the last, a regular file for the last). 1 if all were found */
static int workshop_resolve(const char *item_folder, const char *relative, char *path, size_t size)
{
	char part[128], found[256], next[AE_MCC_PATH_SIZE];
	const char *c = relative;

	if (strlen(item_folder) >= size)
		return 0;
	strcpy(path, item_folder);
	while (*c)
	{
		size_t length = 0;

		while (*c == '/')
			c++;
		if (!*c)
			break;
		while (c[length] && c[length] != '/')
			length++;
		if (length >= sizeof(part))
			return 0;
		memcpy(part, c, length);
		part[length] = 0;
		c += length;
		while (*c == '/')
			c++;
		if (!workshop_find(path, part, *c ? 1 : 0, found, sizeof(found)) ||
			!workshop_join(next, sizeof(next), path, found))
			return 0;
		strcpy(path, next);
	}
	return strcmp(path, item_folder) != 0;
}

/* one map of an item: its entry in *map (always one) */
static void workshop_scan_map(const char *item_folder, const char *folder_id, const struct ae_mcc_workshop_item *item,
	const char *json_path, struct ae_mcc_workshop_map *map)
{
	struct ae_mcc_workshop_map_info info;
	char path[AE_MCC_PATH_SIZE], folder[AE_MCC_PATH_SIZE], found[256], file_name[96], reason[128];
	char *text;
	size_t length;

	if (!workshop_resolve(item_folder, json_path, path, sizeof(path)) || !(text = workshop_file_read(path, &length)))
	{
		workshop_hidden(map, folder_id, item->title, "", "", "unreadable map json");
		return;
	}
	if (!ae_mcc_workshop_read_map_info(text, length, &info, reason, sizeof(reason)))
	{
		free(text);
		workshop_hidden(map, folder_id, item->title, "", "", reason);
		return;
	}
	free(text);
	if (!workshop_name_safe(info.name))
	{
		workshop_hidden(map, folder_id, item->title, "", info.title, "unsafe map name");
		return;
	}
	snprintf(file_name, sizeof(file_name), "%s.map", info.name);
	if (!workshop_find(item_folder, "maps", 1, found, sizeof(found)) ||
		!workshop_join(folder, sizeof(folder), item_folder, found) ||
		!workshop_find(folder, file_name, 0, found, sizeof(found)) ||
		!workshop_join(path, sizeof(path), folder, found))
	{
		workshop_hidden(map, folder_id, item->title, info.name, info.title, "map file missing");
		return;
	}
	if (!ae_mcc_workshop_map_file_valid(path, reason, sizeof(reason)))
	{
		workshop_hidden(map, folder_id, item->title, info.name, info.title[0] ? info.title : info.name, reason);
		/* (one Steam is writing keeps its file, to be checked again: ae_mcc_workshop_recheck) */
		if (!strcmp(reason, "being updated by Steam"))
			snprintf(map->path, sizeof(map->path), "%s", path);
		return;
	}
	workshop_hidden(map, folder_id, item->title, info.name, info.title[0] ? info.title : info.name, "");
	map->status = AE_MCC_WORKSHOP_PLAYABLE;
	snprintf(map->path, sizeof(map->path), "%s", path);
	if (info.thumbnail[0] && !workshop_resolve(item_folder, info.thumbnail, map->thumbnail, sizeof(map->thumbnail)))
		map->thumbnail[0] = 0;
	if (info.large[0] && !workshop_resolve(item_folder, info.large, map->large, sizeof(map->large)))
		map->large[0] = 0;
}

struct workshop_scan_context
{
	const char *content;
	struct ae_mcc_workshop_map *maps;
	int maximum;
	int count;
	int *hidden;
};

static void workshop_count_hidden(struct workshop_scan_context *scan, int index)
{
	if (scan->maps[index].status == AE_MCC_WORKSHOP_HIDDEN && scan->hidden)
		(*scan->hidden)++;
}

/* one item folder of the content folder */
static int workshop_scan_item(void *context, const char *entry, int is_folder, int is_regular)
{
	static struct ae_mcc_workshop_item item;
	struct workshop_scan_context *scan = context;
	char item_folder[AE_MCC_PATH_SIZE], path[AE_MCC_PATH_SIZE], found[256], reason[128], why[96], *text;
	size_t length = 0;
	int index;

	(void)is_regular;
	if (scan->count >= scan->maximum)
		return 1;
	if (!is_folder || !workshop_join(item_folder, sizeof(item_folder), scan->content, entry))
		return 0;
	/* an item already scanned is the same item seen through another library path (on Linux, ~/.steam/steam and
	~/.steam/root are links to the same Steam folder): each item once */
	for (index = 0; index < scan->count; index++)
	{
		if (!strcmp(scan->maps[index].item_id, entry))
			return 0;
	}
	text = NULL;
	snprintf(why, sizeof(why), "no ModInfo.json");
	if (workshop_find(item_folder, "ModInfo.json", 0, found, sizeof(found)) &&
		workshop_join(path, sizeof(path), item_folder, found))
	{
		text = workshop_file_read(path, &length);
		if (!text)
			snprintf(why, sizeof(why), "cannot be read");
	}
	if (!text || !ae_mcc_workshop_read_item(text, length, &item, why, sizeof(why)))
	{
		free(text);
		snprintf(reason, sizeof(reason), "unreadable ModInfo.json: %s", why);
		workshop_hidden(&scan->maps[scan->count], entry, "", "", "", reason);
		workshop_count_hidden(scan, scan->count);
		scan->count++;
		return 0;
	}
	free(text);
	if (!item.halo1)
		return 0;
	for (index = 0; index < item.map_count && scan->count < scan->maximum; index++)
	{
		workshop_scan_map(item_folder, entry, &item, item.map_json[index], &scan->maps[scan->count]);
		workshop_count_hidden(scan, scan->count);
		scan->count++;
	}
	return scan->count >= scan->maximum;
}

int ae_mcc_workshop_scan(const char *const *library_paths, int library_count,
	struct ae_mcc_workshop_map *maps, int maximum, int *hidden)
{
	struct workshop_scan_context scan;
	int library;

	if (hidden)
		*hidden = 0;
	scan.maps = maps;
	scan.maximum = maximum;
	scan.count = 0;
	scan.hidden = hidden;
	for (library = 0; library < library_count && scan.count < maximum; library++)
	{
		char content[AE_MCC_PATH_SIZE];

		if (!library_paths[library] || !workshop_join(content, sizeof(content), library_paths[library],
			WORKSHOP_APP_FOLDER))
			continue;
		scan.content = content;
		ae_mcc_workshop_list_folder(content, workshop_scan_item, &scan);
	}
	return scan.count;
}

/* ---------- a rescan's parts */

struct workshop_ids_context
{
	char (*ids)[24];
	int maximum;
	int count;
};

static int workshop_id_entry(void *context, const char *name, int is_folder, int is_regular)
{
	struct workshop_ids_context *list = context;
	int index;

	(void)is_regular;
	if (!is_folder || strlen(name) >= sizeof(list->ids[0]))
		return 0;
	for (index = 0; index < list->count; index++)
	{
		if (!strcmp(list->ids[index], name))
			return 0;
	}
	if (list->count >= list->maximum)
		return 1;
	strcpy(list->ids[list->count++], name);
	return 0;
}

static int workshop_id_compare(const void *a, const void *b)
{
	return strcmp((const char *)a, (const char *)b);
}

int ae_mcc_workshop_item_ids(const char *const *library_paths, int library_count, char (*ids)[24], int maximum)
{
	struct workshop_ids_context list;
	char content[AE_MCC_PATH_SIZE];
	int library;

	list.ids = ids;
	list.maximum = maximum;
	list.count = 0;
	for (library = 0; ids && library < library_count && list.count < maximum; library++)
	{
		if (library_paths[library] && workshop_join(content, sizeof(content), library_paths[library], WORKSHOP_APP_FOLDER))
			ae_mcc_workshop_list_folder(content, workshop_id_entry, &list);
	}
	if (list.count > 1)
		qsort(ids, (size_t)list.count, sizeof(ids[0]), workshop_id_compare);
	return list.count;
}

int ae_mcc_workshop_same_items(char (*a)[24], int a_count, char (*b)[24], int b_count)
{
	int index;

	if (a_count != b_count)
		return 0;
	for (index = 0; index < a_count; index++)
	{
		if (strcmp(a[index], b[index]))
			return 0;
	}
	return 1;
}

int ae_mcc_workshop_recheck(struct ae_mcc_workshop_map *maps, int count)
{
	char reason[128];
	int index, ready = 0;

	for (index = 0; maps && index < count; index++)
	{
		struct ae_mcc_workshop_map *map = &maps[index];

		if (map->status != AE_MCC_WORKSHOP_HIDDEN || !map->path[0] || strcmp(map->reason, "being updated by Steam"))
			continue;
		if (ae_mcc_workshop_map_file_valid(map->path, reason, sizeof(reason)))
		{
			map->status = AE_MCC_WORKSHOP_PLAYABLE;
			map->reason[0] = 0;
			ready++;
		}
		else if (strcmp(reason, "being updated by Steam"))
		{
			snprintf(map->reason, sizeof(map->reason), "%s", reason);
			map->path[0] = 0;
		}
	}
	return ready;
}

/* ---------- MCC's stock maps */

int ae_mcc_stock_map_state(const char *install_root, const char *name, char *reason, size_t reason_size)
{
	char folder[AE_MCC_PATH_SIZE], file[96], path[AE_MCC_PATH_SIZE], found[256];
	int index;

	if (reason && reason_size)
		reason[0] = 0;
	if (!install_root || !install_root[0] || !name || !ae_mcc_stock_map_name(name) ||
		!workshop_join(folder, sizeof(folder), install_root, "halo1/maps"))
	{
		return AE_MCC_STOCK_MISSING;
	}
	/* (MCC names its files in lower case, as the list does) */
	for (index = 0; name[index] && index < 80; index++)
		file[index] = (char)workshop_lower((unsigned char)name[index]);
	file[index] = 0;
	strcat(file, ".map");
	if (!workshop_find(folder, file, 0, found, sizeof(found)) || !workshop_join(path, sizeof(path), folder, found))
		return AE_MCC_STOCK_MISSING;
	return ae_mcc_workshop_map_file_valid(path, reason, reason_size) ? AE_MCC_STOCK_PLAYABLE : AE_MCC_STOCK_HIDDEN;
}

