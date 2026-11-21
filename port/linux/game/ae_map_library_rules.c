/*
AE_MAP_LIBRARY_RULES.C

The installed maps library's rules (ae_map_library_rules.h): map headers, the list's search and order, which maps may
change, the required files, the scan in slices and the DEV MODE command's words and answers.
*/

#include <stdio.h>
#include <string.h>

#include "ae_map_folders_rules.h"
#include "ae_map_library_rules.h"

/* ---------- text */

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

static int compare_folded(char const *a, char const *b)
{
	int difference;

	while (*a && lower((unsigned char)*a) == lower((unsigned char)*b))
	{
		a++;
		b++;
	}
	difference = lower((unsigned char)*a) - lower((unsigned char)*b);
	return difference;
}

/* whether a word (length bytes) is in text, letters' case aside */
static int contains_folded(char const *text, char const *word, size_t length)
{
	size_t index;

	if (!length)
		return 1;
	for (; *text; text++)
	{
		for (index = 0; index < length && text[index] &&
			lower((unsigned char)text[index]) == lower((unsigned char)word[index]); index++)
		{
		}
		if (index == length)
			return 1;
	}
	return 0;
}

static void copy_text(char *out, unsigned int size, char const *text)
{
	if (!size)
		return;
	snprintf(out, size, "%s", text ? text : "");
}

/* ---------- a map file */

static unsigned int little_u32(unsigned char const *p)
{
	return (unsigned int)p[0] | (unsigned int)p[1] << 8 | (unsigned int)p[2] << 16 | (unsigned int)p[3] << 24;
}

int ae_map_library_header_read(unsigned char const *header, unsigned int size, struct ae_map_header_info *info)
{
	unsigned int version;
	unsigned int index;

	if (!header || size < AE_MAP_HEADER_SIZE || !info)
		return 0;
	if (memcmp(header, "daeh", 4) || memcmp(header + 0x7FC, "toof", 4))
		return 0;
	version = little_u32(header + 4);
	if (version != 5 && version != 7 && version != 609 && version != 13)
		return 0;
	/* (the name: 32 bytes ending in a zero, plain characters) */
	if (!memchr(header + 0x20, 0, 32) || !header[0x20])
		return 0;
	for (index = 0; header[0x20 + index]; index++)
	{
		if (header[0x20 + index] < 0x20 || header[0x20 + index] >= 0x7F)
			return 0;
	}
	memset(info, 0, sizeof(*info));
	info->version = (int)version;
	info->type = header[0x60] | header[0x61] << 8;
	memcpy(info->name, header + 0x20, 32);
	return 1;
}

int ae_map_library_serves(int type)
{
	switch (type)
	{
	case 0: return AE_MAP_SERVES_CAMPAIGN;
	case 1: return AE_MAP_SERVES_MULTIPLAYER;
	case 2: return AE_MAP_SERVES_MENU;
	default: return 0;
	}
}

static char const *const stock_names[][2] =
{
	{ "beavercreek", "Battle Creek" }, { "sidewinder", "Sidewinder" }, { "damnation", "Damnation" },
	{ "ratrace", "Rat Race" }, { "prisoner", "Prisoner" }, { "hangemhigh", "Hang 'Em High" },
	{ "chillout", "Chill Out" }, { "carousel", "Derelict" }, { "boardingaction", "Boarding Action" },
	{ "bloodgulch", "Blood Gulch" }, { "wizard", "Wizard" }, { "putput", "Chiron TL-34" },
	{ "longest", "Longest" }, { "icefields", "Ice Fields" }, { "deathisland", "Death Island" },
	{ "dangercanyon", "Danger Canyon" }, { "infinity", "Infinity" }, { "timberland", "Timberland" },
	{ "gephyrophobia", "Gephyrophobia" },
	{ "a10", "Pillar of Autumn" }, { "a30", "Halo" }, { "a50", "Truth and Reconciliation" },
	{ "b30", "Silent Cartographer" }, { "b40", "Assault on the Control Room" }, { "c10", "343 Guilty Spark" },
	{ "c20", "The Library" }, { "c40", "Two Betrayals" }, { "d20", "Keyes" }, { "d40", "The Maw" },
	{ "ui", "Main Menu" }
};

static int stock_index(char const *name)
{
	int index;

	for (index = 0; index < (int)(sizeof(stock_names) / sizeof(stock_names[0])); index++)
	{
		if (equal_folded(name, stock_names[index][0]))
			return index;
	}
	return -1;
}

int ae_map_library_stock_name(char const *name)
{
	return name && stock_index(name) >= 0;
}

void ae_map_library_title(char const *name, char *title, unsigned int size)
{
	unsigned int index;
	int capitalize = 1;
	int stock;

	if (!size)
		return;
	if (!name)
		name = "";
	stock = stock_index(name);
	if (stock >= 0)
	{
		copy_text(title, size, stock_names[stock][1]);
		return;
	}
	for (index = 0; index + 1 < size && name[index]; index++)
	{
		int c = (unsigned char)name[index];

		if (c == '_')
			c = ' ';
		if (capitalize && c >= 'a' && c <= 'z')
			c -= 'a' - 'A';
		title[index] = (char)c;
		capitalize = c == ' ';
	}
	title[index] = 0;
}

int ae_map_library_source_of(int root_source, struct ae_map_header_info const *info)
{
	if (root_source != AE_MAP_SOURCE_STOCK)
		return root_source;
	if (info->version == 5 && ae_map_library_stock_name(info->name))
		return AE_MAP_SOURCE_STOCK;
	if (info->version == 609 || info->version == 7)
		return AE_MAP_SOURCE_CUSTOM_EDITION;
	return AE_MAP_SOURCE_XBOX_CUSTOM;
}

static char const *const source_names[NUMBER_OF_AE_MAP_SOURCES] =
{
	"stock", "ce", "xbox", "downloaded", "pack", "mcc", "workshop"
};

char const *ae_map_library_source_name(int source)
{
	return source >= 0 && source < NUMBER_OF_AE_MAP_SOURCES ? source_names[source] : "?";
}

int ae_map_library_source_by_name(char const *name)
{
	int source;

	if (!name)
		return -1;
	for (source = 0; source < NUMBER_OF_AE_MAP_SOURCES; source++)
	{
		if (equal_folded(name, source_names[source]))
			return source;
	}
	if (equal_folded(name, "custom"))
		return AE_MAP_SOURCE_CUSTOM_EDITION;
	if (equal_folded(name, "packs"))
		return AE_MAP_SOURCE_MAP_PACK;
	if (equal_folded(name, "downloads"))
		return AE_MAP_SOURCE_DOWNLOADED;
	return -1;
}

/* ---------- search, filter, sort */

int ae_map_library_matches(struct ae_map_entry const *entry, struct ae_map_filter const *filter)
{
	char const *word;

	if (!filter)
		return 1;
	if (filter->sources && !(filter->sources & AE_MAP_SOURCE_BIT(entry->source)))
		return 0;
	if (filter->serves && !(filter->serves & entry->serves))
		return 0;
	for (word = filter->text; *word; )
	{
		size_t length;

		while (*word == ' ')
			word++;
		length = strcspn(word, " ");
		if (length && !contains_folded(entry->name, word, length) && !contains_folded(entry->title, word, length) &&
			!contains_folded(entry->pack, word, length))
		{
			return 0;
		}
		word += length;
	}
	return 1;
}

static int compare_entries(struct ae_map_entry const *a, struct ae_map_entry const *b, int sort)
{
	int order = 0;

	switch (sort)
	{
	case AE_MAP_SORT_TITLE:
		order = compare_folded(a->title, b->title);
		break;
	case AE_MAP_SORT_SOURCE:
		order = a->source - b->source;
		break;
	case AE_MAP_SORT_SIZE_LARGEST:
		order = a->size > b->size ? -1 : a->size < b->size;
		break;
	case AE_MAP_SORT_SIZE_SMALLEST:
		order = a->size < b->size ? -1 : a->size > b->size;
		break;
	default:
		break;
	}
	if (!order)
		order = compare_folded(a->name, b->name);
	if (!order)
		order = compare_folded(a->pack, b->pack);
	if (!order)
		order = strcmp(a->path, b->path);
	return order;
}

/* the indexes sorted (a merge sort: stable, n log n compares; its scratch is on the stack, AE_MAP_LIBRARY_MAXIMUM ints) */
static void sort_indexes(struct ae_map_entry const *entries, int sort, int *indexes, int count)
{
	int scratch[AE_MAP_LIBRARY_MAXIMUM];
	int *from = indexes;
	int *to = scratch;
	int width;

	if (count > AE_MAP_LIBRARY_MAXIMUM)
		count = AE_MAP_LIBRARY_MAXIMUM;
	for (width = 1; width < count; width *= 2)
	{
		int start;
		int *swap;

		for (start = 0; start < count; start += 2 * width)
		{
			int middle = start + width < count ? start + width : count;
			int end = start + 2 * width < count ? start + 2 * width : count;
			int left = start;
			int right = middle;
			int out = start;

			while (left < middle && right < end)
				to[out++] = compare_entries(&entries[from[right]], &entries[from[left]], sort) < 0 ? from[right++] :
					from[left++];
			while (left < middle)
				to[out++] = from[left++];
			while (right < end)
				to[out++] = from[right++];
		}
		swap = from;
		from = to;
		to = swap;
	}
	if (from != indexes)
		memcpy(indexes, from, sizeof(indexes[0]) * (size_t)count);
}

int ae_map_library_select(struct ae_map_entry const *entries, int count, struct ae_map_filter const *filter, int sort,
	int *indexes, int maximum)
{
	int kept = 0;
	int index;

	if (!entries || !indexes)
		return 0;
	for (index = 0; index < count && kept < maximum; index++)
	{
		if (ae_map_library_matches(&entries[index], filter))
			indexes[kept++] = index;
	}
	sort_indexes(entries, sort, indexes, kept);
	return kept;
}

int ae_map_library_find(struct ae_map_entry const *entries, int count, char const *name, char const *pack)
{
	int found = -1;
	int found_count = 0;
	int plain = -1;
	int plain_count = 0;
	int index;

	if (!entries || !name || !name[0])
		return -1;
	for (index = 0; index < count; index++)
	{
		if (!equal_folded(entries[index].name, name) || (pack && pack[0] && !equal_folded(entries[index].pack, pack)))
			continue;
		found = index;
		found_count++;
		if (!entries[index].pack[0])
		{
			plain = index;
			plain_count++;
		}
	}
	if (found_count <= 1)
		return found;
	/* (no pack named: the one map of the name that has none, as the game's lists name it plainly) */
	if ((!pack || !pack[0]) && plain_count == 1)
		return plain;
	return -2;
}

/* ---------- delete and move */

int ae_map_library_name_in_use(char const *name, char const *const *in_use, int in_use_count)
{
	int index;

	for (index = 0; name && name[0] && in_use && index < in_use_count; index++)
		if (in_use[index] && in_use[index][0] && equal_folded(in_use[index], name))
			return 1;
	return 0;
}

int ae_map_library_may_change(struct ae_map_entry const *entry, char const *const *in_use, int in_use_count)
{
	if (!entry || !entry->name[0])
		return AE_MAP_REFUSED_UNKNOWN_MAP;
	switch (entry->source)
	{
	case AE_MAP_SOURCE_CUSTOM_EDITION:
	case AE_MAP_SOURCE_DOWNLOADED:
		break;
	case AE_MAP_SOURCE_STOCK: return AE_MAP_REFUSED_STOCK;
	case AE_MAP_SOURCE_XBOX_CUSTOM: return AE_MAP_REFUSED_XBOX_CUSTOM;
	case AE_MAP_SOURCE_MAP_PACK: return AE_MAP_REFUSED_MAP_PACK;
	case AE_MAP_SOURCE_MCC: return AE_MAP_REFUSED_MCC;
	case AE_MAP_SOURCE_WORKSHOP: return AE_MAP_REFUSED_WORKSHOP;
	default: return AE_MAP_REFUSED_UNKNOWN_MAP;
	}
	/* (a stock map's name is never changed, wherever its file sits) */
	if (ae_map_library_stock_name(entry->name))
		return AE_MAP_REFUSED_STOCK;
	if (ae_map_library_name_in_use(entry->name, in_use, in_use_count))
		return AE_MAP_REFUSED_IN_USE;
	return AE_MAP_CHANGE_ALLOWED;
}

static int separator(char c)
{
#ifdef _WIN32
	return c == '\\' || c == '/';
#else
	return c == '/';
#endif
}

/* the separator a path's joins use: a backslash for a root written with one (the game's d:\ paths), else / */
static char const *joiner(char const *root)
{
	return strchr(root, '\\') ? "\\" : "/";
}

int ae_map_library_path_inside(char const *path, char const *root)
{
	size_t length;

	if (!path || !root || !root[0])
		return 0;
	length = strlen(root);
	/* (a root given with its separator at the end is the same root) */
	while (length > 1 && separator(root[length - 1]))
		length--;
#ifdef _WIN32
	if (_strnicmp(path, root, length))
		return 0;
#else
	if (strncmp(path, root, length))
		return 0;
#endif
	return !path[length] || separator(path[length]) || (length == 1 && separator(root[0]));
}

int ae_map_library_folder_name_ok(char const *folder)
{
	size_t length;

	if (!folder)
		return 0;
	if (!folder[0])
		return 1;
	if (!ae_map_folders_folder_is_page(folder))
		return 0;
	if (strpbrk(folder, "<>\"*?"))
		return 0;
	length = strlen(folder);
	return folder[length - 1] != '.';
}

int ae_map_library_move_root_source(int version)
{
	return version == 5 ? AE_MAP_SOURCE_STOCK : AE_MAP_SOURCE_CUSTOM_EDITION;
}

int ae_map_library_move_target(char const *root, char const *folder, char const *name, char *path, unsigned int size)
{
	char const *join;
	size_t length;
	int written;

	if (!root || !root[0] || !name || !name[0] || !path || !size)
		return 0;
	length = strlen(root);
	join = joiner(root);
	if (folder && folder[0])
		written = snprintf(path, size, "%s%s%s%s%s.map", root, root[length - 1] == join[0] ? "" : join, folder, join,
			name);
	else
		written = snprintf(path, size, "%s%s%s.map", root, root[length - 1] == join[0] ? "" : join, name);
	return written > 0 && (unsigned int)written < size;
}

int ae_map_library_move_answer(char const *real, char const *target, int taken)
{
	if (!real || !target || !real[0] || !target[0])
		return AE_MAP_REFUSED_UNKNOWN_MAP;
#ifdef _WIN32
	if (!_stricmp(real, target))
		return AE_MAP_REFUSED_SAME_FOLDER;
#else
	if (!strcmp(real, target))
		return AE_MAP_REFUSED_SAME_FOLDER;
#endif
	return taken ? AE_MAP_REFUSED_TARGET_TAKEN : AE_MAP_CHANGE_ALLOWED;
}

static char const *const refusal_texts[NUMBER_OF_AE_MAP_CHANGE_ANSWERS] =
{
	"Done.",
	"That map is not in the library.",
	"More than one map has that name: say which pack it is in.",
	"Stock maps can't be deleted or moved.",
	"Xbox maps of your own are kept: only Custom Edition and downloaded maps can be deleted or moved here.",
	"A map pack's maps are its own: remove the pack instead.",
	"MCC's maps belong to its install: Steam manages them.",
	"Workshop maps are Steam's: unsubscribe from the item instead.",
	"That map is in use by the game or the lobby.",
	"That file is outside the maps folders.",
	"That file is a link: only real files are deleted or moved.",
	"That is not a map file.",
	"A folder's name can't have \\ / : * ? \" < > | in it, start with . or _, end with a dot or a space, or be ce, previews, layouts or _shared.",
	"It is in that folder already.",
	"That folder already has a map of that name.",
	"Its path is too long.",
	"The maps are still being looked through: try again in a moment.",
	"The file could not be changed (in use or read-only)."
};

char const *ae_map_library_refusal_text(int answer)
{
	return answer >= 0 && answer < NUMBER_OF_AE_MAP_CHANGE_ANSWERS ? refusal_texts[answer] : "?";
}

/* ---------- required files */

struct required_file_text
{
	char const *file;
	char const *what;
	char const *unlocks;
};

static struct required_file_text const custom_edition_files[] =
{
	{ "bitmaps.map", "Custom Edition's shared bitmaps", "the textures most Custom Edition maps draw with" },
	{ "sounds.map", "Custom Edition's shared sounds", "the sounds most Custom Edition maps play" },
	{ "loc.map", "Custom Edition's shared strings and fonts", "the fonts and text Custom Edition maps show" }
};

static struct required_file_text const mcc_files[] =
{
	{ "singleplayer/bitmaps.map", "MCC's shared campaign textures", "MCC-format campaign maps" },
	{ "multiplayer/bitmaps.map", "MCC's shared multiplayer textures", "MCC-format multiplayer and Workshop maps" },
	{ "multiplayer/sounds.map", "MCC's shared multiplayer sounds", "MCC-format multiplayer and Workshop maps' sounds" }
};

int ae_map_library_required(int kind, char const *const *folders, int folder_count, ae_map_library_exists exists,
	void *context, struct ae_map_required_file *files, int maximum)
{
	struct required_file_text const *list;
	int list_count;
	int listed = 0;
	int index;

	if (!files || !exists)
		return 0;
	switch (kind)
	{
	case AE_MAP_REQUIRED_CUSTOM_EDITION:
		list = custom_edition_files;
		list_count = (int)(sizeof(custom_edition_files) / sizeof(custom_edition_files[0]));
		break;
	case AE_MAP_REQUIRED_MCC:
		list = mcc_files;
		list_count = (int)(sizeof(mcc_files) / sizeof(mcc_files[0]));
		break;
	default:
		return 0;
	}
	for (index = 0; index < list_count && listed < maximum; index++)
	{
		struct ae_map_required_file *file = &files[listed++];
		int folder;

		file->file = list[index].file;
		file->what = list[index].what;
		file->unlocks = list[index].unlocks;
		file->found = 0;
		file->where[0] = 0;
		for (folder = 0; folders && folder < folder_count && !file->found; folder++)
		{
			char path[AE_MAP_LIBRARY_PATH_SIZE];
			size_t length;
			int written;

			if (!folders[folder] || !folders[folder][0])
				continue;
			length = strlen(folders[folder]);
			written = snprintf(path, sizeof(path), "%s%s%s", folders[folder],
				folders[folder][length - 1] == joiner(folders[folder])[0] ? "" : joiner(folders[folder]),
				list[index].file);
			if (written <= 0 || (unsigned int)written >= sizeof(path))
				continue;
			if (exists(context, path))
			{
				file->found = 1;
				copy_text(file->where, sizeof(file->where), folders[folder]);
			}
		}
		if (!file->found && folders && folder_count > 0 && folders[0])
			copy_text(file->where, sizeof(file->where), folders[0]);
	}
	return listed;
}

/* ---------- the scan, in slices */

void ae_map_scan_begin(struct ae_map_scan *scan, struct ae_map_root const *roots, int root_count,
	struct ae_map_entry *entries, int maximum)
{
	memset(scan, 0, sizeof(*scan));
	if (root_count > AE_MAP_LIBRARY_ROOTS)
		root_count = AE_MAP_LIBRARY_ROOTS;
	if (root_count > 0)
		memcpy(scan->roots, roots, sizeof(roots[0]) * (size_t)root_count);
	scan->root_count = root_count > 0 ? root_count : 0;
	scan->entries = entries;
	scan->maximum = entries ? maximum : 0;
	scan->root = 0;
	scan->done = !scan->root_count;
}

static int name_is_map(char const *name)
{
	size_t length = strlen(name);

	return length > 4 && equal_folded(name + length - 4, ".map");
}

/* the path of the folder being read, with a separator at its end */
static int folder_path(struct ae_map_scan const *scan, char *path, unsigned int size)
{
	struct ae_map_root const *root = &scan->roots[scan->root];
	size_t length = strlen(root->path);
	char const *join;
	int written;

	if (!length)
		return 0;
	join = joiner(root->path);
	written = snprintf(path, size, "%s%s%s%s", root->path, root->path[length - 1] == join[0] ? "" : join,
		scan->folder_name, scan->folder_name[0] ? join : "");
	return written > 0 && (unsigned int)written < size;
}

/* the next folder to read opened (the root's top, then its folders, then the next root's): 0 when none is left */
static int open_next(struct ae_map_scan *scan, struct ae_map_scan_io const *io)
{
	char path[AE_MAP_LIBRARY_PATH_SIZE];

	while (scan->root < scan->root_count)
	{
		if (!scan->root_started)
		{
			scan->root_started = 1;
			scan->folder_name[0] = 0;
			scan->pending_count = 0;
		}
		else if (scan->pending_count > 0)
			copy_text(scan->folder_name, sizeof(scan->folder_name), scan->pending[--scan->pending_count]);
		else
		{
			scan->root++;
			scan->root_started = 0;
			continue;
		}
		if (folder_path(scan, path, sizeof(path)))
		{
			scan->folder = io->open_folder(io->context, path);
			if (scan->folder)
				return 1;
		}
		scan->unopened++;
	}
	return 0;
}

/* a map file of the folder being read added (its header read and checked) */
static void map_add(struct ae_map_scan *scan, struct ae_map_scan_io const *io, char const *file_name)
{
	struct ae_map_root const *root = &scan->roots[scan->root];
	unsigned char header[AE_MAP_HEADER_SIZE];
	struct ae_map_header_info info;
	struct ae_map_entry *entry;
	unsigned long long file_size = 0;
	char path[AE_MAP_LIBRARY_PATH_SIZE];
	size_t length;
	int read;

	if (!folder_path(scan, path, sizeof(path)))
		return;
	length = strlen(path);
	if (length + strlen(file_name) >= sizeof(path))
		return;
	memcpy(path + length, file_name, strlen(file_name) + 1);
	scan->headers++;
	read = io->read_header(io->context, path, header, sizeof(header), &file_size);
	if (read < (int)sizeof(header) || !ae_map_library_header_read(header, sizeof(header), &info))
	{
		scan->not_maps++;
		return;
	}
	if (scan->count >= scan->maximum)
	{
		scan->dropped++;
		return;
	}
	entry = &scan->entries[scan->count++];
	memset(entry, 0, sizeof(*entry));
	/* (the file's name, not the header's: a map is played by its file's name) */
	length = strlen(file_name) - 4;
	if (length >= sizeof(entry->name))
		length = sizeof(entry->name) - 1;
	memcpy(entry->name, file_name, length);
	entry->name[length] = 0;
	ae_map_library_title(entry->name, entry->title, sizeof(entry->title));
	if (root->pack_folders && scan->folder_name[0])
		copy_text(entry->pack, sizeof(entry->pack), scan->folder_name);
	else
	{
		copy_text(entry->pack, sizeof(entry->pack), root->pack);
		copy_text(entry->folder, sizeof(entry->folder), scan->folder_name);
	}
	copy_text(entry->path, sizeof(entry->path), path);
	entry->size = file_size;
	entry->source = ae_map_library_source_of(root->source, &info);
	entry->serves = ae_map_library_serves(info.type);
	entry->version = info.version;
	entry->root = scan->root;
	entry->picture = -1;
}

int ae_map_scan_step(struct ae_map_scan *scan, struct ae_map_scan_io const *io, int headers, unsigned long long budget_us)
{
	unsigned long long start;
	unsigned long long spent;
	int read = 0;

	if (scan->done)
		return 0;
	start = io->now_us(io->context);
	scan->steps++;
	for (;;)
	{
		char name[AE_MAP_LIBRARY_NAME_SIZE + 8];
		int is_folder = 0;

		if (!scan->folder && !open_next(scan, io))
		{
			scan->done = 1;
			break;
		}
		if (!io->next_entry(io->context, scan->folder, name, sizeof(name), &is_folder))
		{
			io->close_folder(io->context, scan->folder);
			scan->folder = NULL;
			continue;
		}
		if (is_folder)
		{
			/* (one level: a root's folders, each a page or a pack) */
			if (!scan->folder_name[0] && scan->roots[scan->root].folders &&
				scan->pending_count < (int)(sizeof(scan->pending) / sizeof(scan->pending[0])) &&
				strlen(name) < sizeof(scan->pending[0]) &&
				(scan->roots[scan->root].pack_folders ? name[0] != '.' : ae_map_folders_folder_is_page(name)))
			{
				copy_text(scan->pending[scan->pending_count++], sizeof(scan->pending[0]), name);
			}
		}
		else if (name_is_map(name) && strlen(name) - 4 < AE_MAP_LIBRARY_NAME_SIZE)
		{
			map_add(scan, io, name);
			read++;
		}
		if (read >= headers || io->now_us(io->context) - start >= budget_us)
			break;
	}
	spent = io->now_us(io->context) - start;
	scan->total_us += spent;
	if (spent > scan->longest_step_us)
		scan->longest_step_us = spent;
	return !scan->done;
}

void ae_map_scan_end(struct ae_map_scan *scan, struct ae_map_scan_io const *io)
{
	if (scan->folder)
		io->close_folder(io->context, scan->folder);
	scan->folder = NULL;
	scan->done = 1;
}

/* ---------- DEV MODE "maps" */

/* the next word of words copied (at most size - 1 bytes; 0 when it does not fit), words moved past it */
static int next_word(char const **words, char *word, unsigned int size)
{
	char const *start = *words;
	size_t length;

	while (*start == ' ' || *start == '\t')
		start++;
	length = strcspn(start, " \t");
	*words = start + length;
	if (length >= size)
		return 0;
	memcpy(word, start, length);
	word[length] = 0;
	return 1;
}

/* <map>[@<pack>] into the command */
static int map_word(char const *word, struct ae_map_command *command)
{
	char const *at = strchr(word, '@');
	size_t length = at ? (size_t)(at - word) : strlen(word);

	if (!length || length >= sizeof(command->text))
		return 0;
	memcpy(command->text, word, length);
	command->text[length] = 0;
	if (at)
	{
		if (!at[1] || strlen(at + 1) >= sizeof(command->pack))
			return 0;
		copy_text(command->pack, sizeof(command->pack), at + 1);
	}
	return 1;
}

int ae_map_library_command_parse(char const *words, struct ae_map_command *command)
{
	char verb[16];
	char word[AE_MAP_LIBRARY_PATH_SIZE];
	char const *rest;

	if (!words || !command)
		return 0;
	memset(command, 0, sizeof(*command));
	command->source = -1;
	if (!next_word(&words, verb, sizeof(verb)) || !verb[0])
		return 0;
	if (equal_folded(verb, "list"))
	{
		command->verb = AE_MAP_COMMAND_LIST;
		while (*words == ' ' || *words == '\t')
			words++;
		rest = words;
		/* (a first word "<source>:" keeps that source; the rest is the search text) */
		if (next_word(&words, word, sizeof(word)) && word[0] && word[strlen(word) - 1] == ':')
		{
			word[strlen(word) - 1] = 0;
			command->source = ae_map_library_source_by_name(word);
			if (command->source < 0)
				return 0;
			while (*words == ' ' || *words == '\t')
				words++;
			rest = words;
		}
		if (strlen(rest) >= sizeof(command->text))
			return 0;
		copy_text(command->text, sizeof(command->text), rest);
		/* (no spaces at its end) */
		while (command->text[0] && command->text[strlen(command->text) - 1] == ' ')
			command->text[strlen(command->text) - 1] = 0;
		return 1;
	}
	if (equal_folded(verb, "missing") || equal_folded(verb, "scan"))
	{
		command->verb = equal_folded(verb, "missing") ? AE_MAP_COMMAND_MISSING : AE_MAP_COMMAND_SCAN;
		return next_word(&words, word, sizeof(word)) && !word[0];
	}
	if (equal_folded(verb, "delete"))
	{
		command->verb = AE_MAP_COMMAND_DELETE;
		if (!next_word(&words, word, sizeof(word)) || !map_word(word, command))
			return 0;
		return next_word(&words, word, sizeof(word)) && !word[0];
	}
	if (equal_folded(verb, "move"))
	{
		command->verb = AE_MAP_COMMAND_MOVE;
		if (!next_word(&words, word, sizeof(word)) || !map_word(word, command))
			return 0;
		/* (the folder is the rest, spaces and all: page folders have them) */
		while (*words == ' ' || *words == '\t')
			words++;
		if (!words[0] || strlen(words) >= sizeof(command->folder))
			return 0;
		copy_text(command->folder, sizeof(command->folder), words);
		while (command->folder[strlen(command->folder) - 1] == ' ')
			command->folder[strlen(command->folder) - 1] = 0;
		/* (a folder in quotes: the quotes taken off) */
		if (strlen(command->folder) >= 2 && command->folder[0] == '"' &&
			command->folder[strlen(command->folder) - 1] == '"')
		{
			size_t length = strlen(command->folder) - 2;

			memmove(command->folder, command->folder + 1, length);
			command->folder[length] = 0;
		}
		if (!strcmp(command->folder, "."))
			command->folder[0] = 0;
		return 1;
	}
	return 0;
}

/* text appended to line (used bytes so far in *used); 0 once it no longer fits (line then ends in "...") */
static int append(char *line, unsigned int size, unsigned int *used, char const *text)
{
	size_t length = strlen(text);

	if (*used + length + 4 > size)
	{
		if (*used + 4 <= size)
		{
			memcpy(line + *used, "...", 4);
			*used += 3;
		}
		return 0;
	}
	memcpy(line + *used, text, length + 1);
	*used += (unsigned int)length;
	return 1;
}

void ae_map_library_list_line(struct ae_map_entry const *entries, int count, int const *indexes, int shown, char *line,
	unsigned int size)
{
	int per_source[NUMBER_OF_AE_MAP_SOURCES] = { 0 };
	char text[AE_MAP_LIBRARY_NAME_SIZE + AE_MAP_LIBRARY_PACK_SIZE + 32];
	unsigned int used = 0;
	int source;
	int first = 1;
	int index;

	if (line && size)
		line[0] = 0;
	if (!line || size < 8)
		return;
	for (index = 0; index < shown; index++)
		per_source[entries[indexes[index]].source]++;
	snprintf(text, sizeof(text), "maps: %d of %d (", shown, count);
	if (!append(line, size, &used, text))
		return;
	for (source = 0; source < NUMBER_OF_AE_MAP_SOURCES; source++)
	{
		if (!per_source[source])
			continue;
		snprintf(text, sizeof(text), "%s%s %d", first ? "" : ", ", source_names[source], per_source[source]);
		first = 0;
		if (!append(line, size, &used, text))
			return;
	}
	if (!append(line, size, &used, shown ? "):" : ")"))
		return;
	for (index = 0; index < shown; index++)
	{
		struct ae_map_entry const *entry = &entries[indexes[index]];

		snprintf(text, sizeof(text), "%s%s%s%s", index ? ", " : " ", entry->name, entry->pack[0] ? "@" : "",
			entry->pack);
		if (!append(line, size, &used, text))
			return;
	}
}

void ae_map_library_missing_line(int kind, struct ae_map_required_file const *files, int file_count, char *line,
	unsigned int size)
{
	char text[AE_MAP_LIBRARY_PATH_SIZE + 64];
	unsigned int used = 0;
	int listed = 0;
	int pass;

	if (line && size)
		line[0] = 0;
	if (!line || size < 8)
		return;
	snprintf(text, sizeof(text), "required %s:", kind == AE_MAP_REQUIRED_MCC ? "mcc" : "ce");
	if (!append(line, size, &used, text))
		return;
	if (!file_count)
	{
		append(line, size, &used, " no folder to look in");
		return;
	}
	/* (the files found, then the missing ones and where they go: the first folder, the same for each) */
	for (pass = 0; pass < 2; pass++)
	{
		char const *where = "";
		int first = 1;
		int index;

		for (index = 0; index < file_count; index++)
		{
			if (files[index].found != !pass)
				continue;
			snprintf(text, sizeof(text), "%s%s", first ? (pass ? (listed ? "; MISSING " : " MISSING ") : " ok ") : ", ",
				files[index].file);
			first = 0;
			listed = 1;
			if (!where[0])
				where = files[index].where;
			if (!append(line, size, &used, text))
				return;
		}
		if (pass && !first && where[0])
		{
			snprintf(text, sizeof(text), " (put in %s)", where);
			if (!append(line, size, &used, text))
				return;
		}
	}
}
