/*
UI_MAP_LIST.C

The menus' list of multiplayer maps, filled by the port rather than fixed in
the game (ui_widget_event_handler_functions.c's multiplayer level list): the
Xbox's thirteen maps first, as they were, then the Xbox v5 community maps
found (custom_maps.c, upstream PR #63: named by their caches' names,
described as a community map), then the Custom Edition maps
(Halo PC's, played as <name>@ce), each named with [CE], then HaloMD's maps
(Halo PC retail's, played as <name>@md), each named with [MD]: each family
in its folders (halo_map_families.h), in the order of their names.

A row past the Xbox's has no string or bitmap frame of its own in ui.map:
its text comes from here, by a string list index of
UI_MAP_LIST_STRING_BASE and up (ui_widget.c asks ui_map_list_text when a
text box has one), and its picture by a bitmap frame of
UI_MAP_LIST_PICTURE_BASE and up (ui_widget.c asks ui_map_list_picture). Its
name is two lines in the map list's narrow boxes (the name, then [CE] or
[MD]), and one in the lobby's.

Those are Halo PC's own, read from its ui.map in maps\ce as Halo PC shows
them: the names of its map list (ui\shell\main_menu\mp_map_list), their
descriptions (...\mp_map_select\map_data) and their pictures
(ui\shell\bitmaps\mp_map_grafix, its pixels in maps\ce\bitmaps.map), each
the map's by Halo PC's order of its maps (ce_maps); a map of another's
making is named by its file, with Halo PC's picture for an unknown level.
Without Halo PC's ui.map, the names are ce_maps' and an Xbox map's picture
stands in. A HaloMD map is named as HaloMD's mod list names it (by its
file's name: halomd_map_names.h, written by tools/halomd_map_names.py), or
by its file's name made readable, with Halo PC's picture for an unknown
level.

The server browser (browser_screen.c) names a listed game's Custom Edition
or HaloMD map by the same (ui_map_list_family_name), on every build: the
builds without such maps (HALO_CUSTOM_EDITION) still name one. On those with
them it has Halo PC's picture of the map (ui_map_list_family_picture) and
asks whether the map is in its family's folders (ui_map_list_family_present).
*/

#include "cseries.h"
#include "cseries_windows.h"
#include "errors.h"

#include <stdio.h>
#include <string.h>

#include "halo_map_families.h"
#include "halo_ui_map_list.h"
#include "halomd_map_names.h"
#include "halo_custom_maps.h"

#include "bitmaps/bitmap_group.h"
#include "tag_files/tag_groups.h"
#include "text/text_group.h"

/* ---------- an Xbox map's picture in the loaded ui.map

The menus' map pictures (ui\shell\bitmaps\mp_map_grafix) and names
(ui\shell\main_menu\mp_map_list) are the loaded ui.map's, and a mod's ui.map
has its own: Halo 1: NHE's has 27 maps' pictures in its own order and its
"Unknown Level" (a "?") at 27, the Xbox's 13 maps in theirs and its "?" at 13.
Frame i of that bitmap is the map named by string i of that list (both
ui.maps), so a map's picture is found by its name there, never by a fixed
frame: a map the list lacks gets the list's "Unknown Level" picture. */

/* the Xbox's thirteen maps (the game's order) and their names */
static char const *const xbox_map_files[] =
{
	"beavercreek", "sidewinder", "damnation", "ratrace", "prisoner", "hangemhigh", "chillout",
	"carousel", "boardingaction", "bloodgulch", "wizard", "putput", "longest",
};
static char const *const xbox_map_captions[] =
{
	"Battle Creek", "Sidewinder", "Damnation", "Rat Race", "Prisoner", "Hang 'Em High", "Chill Out",
	"Derelict", "Boarding Action", "Blood Gulch", "Wizard", "Chiron TL34", "Longest",
};
/* map files named other than their maps (Halo 1: NHE's, whose scenarios are
levels\test\beavercreek\..., ...\outbound\... and ...\damnation\...) */
static struct
{
	char const *file;
	char const *caption;
} const map_file_captions[] =
{
	{ "badcreek", "Battle Creek" },
	{ "outbnd", "Outbound" },
	{ "dammy", "Damnation" },
};

enum
{
	/* the Xbox ui.map's "Unknown Level" frame */
	XBOX_UNKNOWN_LEVEL_FRAME = 13,
	MAP_KEY_LENGTH = 48,
};

/* a name's letters and digits, lower case ("Hang 'Em High ": hangemhigh) */
static void map_key(
	char const *name,
	char *key)
{
	long length = 0;

	for (; *name && length < MAP_KEY_LENGTH - 1; name++)
	{
		char character = *name;

		if (character >= 'A' && character <= 'Z')
			character = (char)(character - 'A' + 'a');
		if ((character >= 'a' && character <= 'z') || (character >= '0' && character <= '9'))
			key[length++] = character;
	}
	key[length] = 0;
}

static void map_key_wide(
	wchar_t const *name,
	char *key)
{
	long length = 0;

	for (; name && *name && length < MAP_KEY_LENGTH - 1; name++)
	{
		wchar_t character = *name;

		if (character >= 'A' && character <= 'Z')
			character = (wchar_t)(character - 'A' + 'a');
		if ((character >= 'a' && character <= 'z') || (character >= '0' && character <= '9'))
			key[length++] = (char)character;
	}
	key[length] = 0;
}

/* the frames of the loaded mp_map_grafix the menus draw (its first
sequence's, as bitmap_group_get_bitmap_from_sequence takes them), or 0 */
static short xbox_picture_count(
	void)
{
	long index = tag_loaded('bitm', "ui\\shell\\bitmaps\\mp_map_grafix");
	struct bitmap_group *group = index != NONE ? bitmap_group_get(index) : NULL;

	if (!group)
		return 0;
	if (group->sequences.count > 0)
	{
		struct bitmap_group_sequence *sequence =
			TAG_BLOCK_GET_ELEMENT(&group->sequences, 0, struct bitmap_group_sequence);

		if (sequence->bitmap_count > 0)
			return sequence->bitmap_count;
	}
	return (short)MIN(group->bitmaps.count, 0x7fff);
}

/* the stock (Xbox) map of a name, else NONE */
static short xbox_map_index(
	char const *file)
{
	short index;

	for (index = 0; index < (short)NUMBEROF(xbox_map_files); index++)
	{
		if (!_stricmp(file, xbox_map_files[index]))
			return index;
	}
	return NONE;
}

short ui_map_list_xbox_picture(
	char const *map_name,
	boolean *own)
{
	char const *file = native_map_basename(map_name);
	char const *caption = file;
	short stock = xbox_map_index(file);
	/* (the loaded ui.map's list of names unreadable: as the Xbox's) */
	short fallback = stock != NONE ? stock : XBOX_UNKNOWN_LEVEL_FRAME;
	long list = tag_loaded('ustr', "ui\\shell\\main_menu\\mp_map_list");
	short pictures = xbox_picture_count();
	short unknown = NONE, count, index;
	char key[MAP_KEY_LENGTH], entry[MAP_KEY_LENGTH];

	if (own)
		*own = stock != NONE;
	if (stock != NONE)
		caption = xbox_map_captions[stock];
	for (index = 0; index < (short)NUMBEROF(map_file_captions); index++)
	{
		if (!_stricmp(file, map_file_captions[index].file))
			caption = map_file_captions[index].caption;
	}
	if (list == NONE || pictures <= 0)
		return fallback;
	map_key(caption, key);
	count = (short)MIN(unicode_string_list_definition_get(list)->strings.count, pictures);
	for (index = 0; index < count; index++)
	{
		map_key_wide(unicode_string_list_get_string(list, index), entry);
		if (key[0] && !strcmp(entry, key))
		{
			if (own)
				*own = TRUE;
			return index;
		}
		if (unknown == NONE && !strcmp(entry, "unknownlevel"))
			unknown = index;
	}
	/* (a list read, without this map: its unknown level's; a list of
	other names, another language's, as the Xbox's) */
	if (unknown != NONE)
	{
		if (own)
			*own = FALSE;
		return unknown;
	}
	return fallback;
}

/* Halo PC's multiplayer maps, in the order of its map list (its strings and
pictures), and an Xbox map of the same kind whose picture stands in
without Halo PC's ui.map */
static struct
{
	char const *file;
	wchar_t const *name;
	short xbox_picture_index;
} const ce_maps[] =
{
	{ "beavercreek", L"Battle Creek", 0 },
	{ "sidewinder", L"Sidewinder", 1 },
	{ "damnation", L"Damnation", 2 },
	{ "ratrace", L"Rat Race", 3 },
	{ "prisoner", L"Prisoner", 4 },
	{ "hangemhigh", L"Hang 'Em High", 5 },
	{ "chillout", L"Chill Out", 6 },
	{ "carousel", L"Derelict", 7 },
	{ "boardingaction", L"Boarding Action", 8 },
	{ "bloodgulch", L"Blood Gulch", 9 },
	{ "wizard", L"Wizard", 10 },
	{ "putput", L"Chiron TL34", 11 },
	{ "longest", L"Longest", 12 },
	{ "icefields", L"Ice Fields", 1 },
	{ "deathisland", L"Death Island", 1 },
	{ "dangercanyon", L"Danger Canyon", 9 },
	{ "infinity", L"Infinity", 9 },
	{ "timberland", L"Timberland", 9 },
	{ "gephyrophobia", L"Gephyrophobia", 2 },
};

/* a map file's place in ce_maps (by its name, any case), or NONE */
static long ce_map_index(
	char const *file)
{
	long index;

	for (index = 0; index < (long)NUMBEROF(ce_maps); index++)
	{
		char const *a = file;
		char const *b = ce_maps[index].file;

		while (*a && (*a | 0x20) == *b)
		{
			a++;
			b++;
		}
		if (!*a && !*b)
			return index;
	}
	return NONE;
}

/* a HaloMD map's name, by its file's name: as HaloMD's mod list names it,
or another version's of the same map (<map>_<version>), or NULL */
static wchar_t const *halomd_map_name(
	char const *file)
{
	size_t stem_length = strlen(file);
	long index;

	for (index = 0; index < (long)NUMBEROF(halomd_map_names); index++)
	{
		if (!_stricmp(file, halomd_map_names[index].file))
			return halomd_map_names[index].name;
	}
	/* (another version's: the name without its _<version>) */
	while (stem_length && file[stem_length - 1] >= '0' && file[stem_length - 1] <= '9')
		stem_length--;
	if (!stem_length || stem_length == strlen(file) || file[stem_length - 1] != '_')
		return NULL;
	for (index = 0; index < (long)NUMBEROF(halomd_map_names); index++)
	{
		char const *other = halomd_map_names[index].file;
		size_t other_length = strlen(other);

		if (other_length > stem_length && !_strnicmp(file, other, stem_length) &&
			strspn(other + stem_length, "0123456789") == other_length - stem_length)
		{
			return halomd_map_names[index].name;
		}
	}
	return NULL;
}

/* a map file's name made readable, as the game list's web pages make it:
its underscores, dots and dashes spaces, each word begun with a capital
(hugeass_v2: Hugeass V2) */
static void ce_map_tidy_name(
	char const *file,
	wchar_t *name,
	long size)
{
	long length = 0;
	boolean space = FALSE;

	for (; *file && length < size - 1; file++)
	{
		unsigned char character = (unsigned char)*file;

		if (character == '_' || character == '.' || character == '-' || character == ' ' || character == '\t')
		{
			space = length > 0;
			continue;
		}
		if (space)
		{
			if (length >= size - 2)
				break;
			name[length++] = ' ';
			space = FALSE;
		}
		if ((length == 0 || name[length - 1] == ' ') && character >= 'a' && character <= 'z')
			character = (unsigned char)(character - 'a' + 'A');
		name[length++] = (wchar_t)character;
	}
	name[length] = 0;
}

#ifdef HALO_CUSTOM_EDITION

#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmaps.h"
#include "rasterizer/rasterizer_swizzle.h"

#include <xtl.h>
#include <stdlib.h>

/* ---------- constants */

enum
{
	XBOX_MAP_COUNT = 13,
	/* (the Xbox's thirteen, then the Custom Edition and HaloMD maps found,
	up to this many rows in all) */
	MAXIMUM_MAP_LIST = 256,
	MAP_NAME_LENGTH = 64,
	DISPLAY_NAME_LENGTH = 48,
	DESCRIPTION_LENGTH = 160,
	/* text boxes' string list indices from here are this list's: the row
	times four, plus its string's kind */
	UI_MAP_LIST_STRING_BASE = 0x4000,
	UI_MAP_LIST_STRINGS_PER_ROW = 4,
	/* bitmap frames from here are Halo PC's map pictures */
	UI_MAP_LIST_PICTURE_BASE = 0x4000,

	/* Halo PC's ui.map: its cache file's version, the address its tags are
	read at, and its map list's entry for an unknown level */
	CE_CACHE_VERSION = 609,
	CE_TAGS_ADDRESS = 0x40440000,
	CE_UNKNOWN_LEVEL = 19,
	MAXIMUM_CE_STRINGS = 24,
	MAXIMUM_CE_PICTURES = 24,
	/* its bitmaps' pixels kept in bitmaps.map */
	CE_BITMAP_EXTERNAL_FLAG = 0x100,
	/* (the bitmaps' type of a 2D texture, as the game's) */
	CE_BITMAP_TYPE_2D = 0,
};

/* ---------- structures */

struct ui_map_entry
{
	char map_name[MAP_NAME_LENGTH];
	wchar_t display_name[DISPLAY_NAME_LENGTH];
	wchar_t lobby_name[DISPLAY_NAME_LENGTH];
	wchar_t description[DESCRIPTION_LENGTH];
	/* its row among the Xbox's (their strings and bitmap frames), or NONE */
	short xbox_index;
	short picture_index;
	/* port: the Xbox map whose picture it shows, found in the loaded ui.map
	when asked (ui_map_list_xbox_picture), or empty for picture_index */
	char picture_map[MAP_NAME_LENGTH];
};

/* ---------- prototypes */

char const *cache_files_map_directory(void);
long bitmap_format_to_d3d_format(short format, word flags);

/* ---------- globals */

static struct ui_map_entry ui_map_list[MAXIMUM_MAP_LIST];
static char *ui_map_list_names_array[MAXIMUM_MAP_LIST];
static long ui_map_list_count_value;

/* what Halo PC's ui.map has of its map list, read once */
static struct
{
	boolean read;
	long name_count;
	wchar_t names[MAXIMUM_CE_STRINGS][DISPLAY_NAME_LENGTH];
	long description_count;
	wchar_t descriptions[MAXIMUM_CE_STRINGS][DESCRIPTION_LENGTH];
	long picture_count;
	struct bitmap_data pictures[MAXIMUM_CE_PICTURES];
} ce_ui;

/* Halo PC's ui.map's tags, while they are read (ce_ui_read) */
static byte *ce_tags;
static unsigned long ce_tags_size;

/* ---------- private code */

/* (the game's wide characters are 16 bits: not the C library's) */
static void wide_copy(
	wchar_t *destination,
	long size,
	wchar_t const *source)
{
	long index;

	for (index = 0; index < size - 1 && source[index]; index++)
		destination[index] = source[index];
	destination[index] = 0;
}

static void wide_append(
	wchar_t *destination,
	long size,
	wchar_t const *source)
{
	long length = 0;

	while (length < size - 1 && destination[length])
		length++;
	wide_copy(destination + length, size - length, source);
}

static boolean file_read(
	HANDLE file,
	unsigned long offset,
	void *buffer,
	unsigned long size)
{
	unsigned long bytes_read = 0;

	if (SetFilePointer(file, (long)offset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		return FALSE;
	return ReadFile(file, buffer, size, &bytes_read, NULL) && bytes_read == size;
}

/* an address of Halo PC's ui.map's tags made a pointer to size bytes of
them, or NULL outside them */
static void *ce_tags_pointer(
	unsigned long address,
	unsigned long size)
{
	if (address < CE_TAGS_ADDRESS || address - CE_TAGS_ADDRESS > ce_tags_size ||
		size > ce_tags_size - (address - CE_TAGS_ADDRESS))
	{
		return NULL;
	}
	return ce_tags + (address - CE_TAGS_ADDRESS);
}

/* the data of the tag of a group and name, or NULL */
static byte *ce_tag_find(
	unsigned long group_tag,
	char const *name)
{
	unsigned long *header = ce_tags_pointer(CE_TAGS_ADDRESS, 0x10);
	unsigned long *instances;
	unsigned long index;

	if (!header)
		return NULL;
	/* (its instances' address, ..., their count: no more than its tags
hold, so that the size does not overflow) */
	if (header[3] > ce_tags_size / 0x20)
		return NULL;
	instances = ce_tags_pointer(header[0], header[3] * 0x20);
	if (!instances)
		return NULL;
	for (index = 0; index < header[3]; index++)
	{
		/* (an instance: its group, ..., its name, its data) */
		unsigned long *instance = instances + index * 8;
		char const *instance_name = ce_tags_pointer(instance[4], strlen(name) + 1);

		if (instance[0] == group_tag && instance_name && !memcmp(instance_name, name, strlen(name) + 1))
			return ce_tags_pointer(instance[5], 0x6c);
	}
	return NULL;
}

/* a unicode string list's strings, each up to length characters: how many */
static long ce_string_list_read(
	char const *name,
	wchar_t *strings,
	long length,
	long maximum_count)
{
	unsigned long *block = (unsigned long *)ce_tag_find('ustr', name);
	unsigned long *references;
	long count, index;

	if (!block)
		return 0;
	count = MIN((long)block[0], maximum_count);
	references = ce_tags_pointer(block[1], count * 0x14);
	if (!references)
		return 0;
	for (index = 0; index < count; index++)
	{
		/* (a tag data: its size, ..., its address) */
		unsigned long size = references[index * 5];
		unsigned short const *text = ce_tags_pointer(references[index * 5 + 3], size);
		wchar_t *string = strings + index * length;
		long character;

		for (character = 0; text && character < length - 1 && character < (long)(size / 2) && text[character];
			character++)
		{
			string[character] = text[character];
		}
		string[character] = 0;
	}
	return count;
}

/* the map list's pictures: each made a bitmap of its own, its pixels and its
Direct3D texture in contiguous memory, as the texture cache would make it */
static void ce_pictures_read(
	HANDLE ui_file)
{
	byte *group = ce_tag_find('bitm', "ui\\shell\\bitmaps\\mp_map_grafix");
	HANDLE bitmaps_file = INVALID_HANDLE_VALUE;
	unsigned long *block;
	byte *elements;
	long count, index;

	if (!group)
		return;
	/* (its bitmaps block) */
	block = (unsigned long *)(group + 0x60);
	count = MIN((long)block[0], MAXIMUM_CE_PICTURES);
	elements = ce_tags_pointer(block[1], count * 0x30);
	if (!elements)
		return;
	for (index = 0; index < count; index++)
	{
		byte *element = elements + index * 0x30;
		struct bitmap_data *bitmap = &ce_ui.pictures[index];
		unsigned short flags = *(unsigned short *)(element + 0xe);
		unsigned long pixels_offset = *(unsigned long *)(element + 0x18);
		unsigned long pixels_size = *(unsigned long *)(element + 0x1c);
		unsigned long allocated_size;
		HANDLE file = ui_file;
		D3DBaseTexture *texture;
		void *pixels;

		memset(bitmap, 0, sizeof(*bitmap));
		bitmap->signature = *(unsigned long *)element;
		bitmap->width = *(short *)(element + 0x4);
		bitmap->height = *(short *)(element + 0x6);
		bitmap->depth = *(short *)(element + 0x8);
		bitmap->type = *(short *)(element + 0xa);
		bitmap->format = *(short *)(element + 0xc);
		/* (Halo PC's own flags dropped: not cached, its pixels here) */
		bitmap->flags = flags & 0x3f;
		bitmap->mipmap_count = *(short *)(element + 0x14);
		bitmap->pixels_size = pixels_size;
		bitmap->tag_index = NONE;
		bitmap->cache_block_index = NONE;
		/* (a 2D bitmap the game draws, of a format with a Direct3D one:
		bitmap_format_to_d3d_format asserts there is) */
		if (bitmap->type != CE_BITMAP_TYPE_2D || bitmap->width <= 0 || bitmap->height <= 0 ||
			pixels_size == 0 || pixels_size > 0x100000 || !bitmap_verify(bitmap, FALSE) ||
			bitmap->format == 4 || bitmap->format == 5 || bitmap->format == 7 || bitmap->format == 12 ||
			bitmap->format == 13 || (bitmap->flags & 0x10))
		{
			break;
		}
		/* (as much as the renderer reads of its size and format, as the
		texture cache gives it: the pixels the file has, the rest zero) */
		allocated_size = MAX(pixels_size, (unsigned long)rasterizer_xbox_bitmap_get_pixel_data_size(bitmap));
		if (allocated_size > 0x400000)
			break;
		if (flags & CE_BITMAP_EXTERNAL_FLAG)
		{
			if (bitmaps_file == INVALID_HANDLE_VALUE)
			{
				char path[512];

				snprintf(path, sizeof(path), "%sce\\bitmaps.map", cache_files_map_directory());
				bitmaps_file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (bitmaps_file == INVALID_HANDLE_VALUE)
					break;
			}
			file = bitmaps_file;
		}
		pixels = XPhysicalAlloc(allocated_size, -1, 0, PAGE_READWRITE);
		texture = XPhysicalAlloc(sizeof(D3DBaseTexture), -1, 0, PAGE_READWRITE);
		if (pixels)
			memset(pixels, 0, allocated_size);
		if (!pixels || !texture || !file_read(file, pixels_offset, pixels, pixels_size))
		{
			if (pixels)
				XPhysicalFree(pixels);
			if (texture)
				XPhysicalFree(texture);
			break;
		}
		memset(texture, 0, sizeof(*texture));
		/* (texture_cache_initialize_hardware_format's, laid out as Halo PC
		lays out its pixels) */
		texture->Common = D3DCOMMON_TYPE_TEXTURE | 1 | D3DCOMMON_PORT_PC_LAYOUT;
		texture->Format =
			(floor_log2(bitmap->height) << D3DFORMAT_VSIZE_SHIFT) |
			(floor_log2(bitmap->width) << D3DFORMAT_USIZE_SHIFT) |
			(bitmap_format_to_d3d_format(bitmap->format, bitmap->flags) << D3DFORMAT_FORMAT_SHIFT) |
			(2 << D3DFORMAT_DIMENSION_SHIFT) |
			((rasterizer_xbox_bitmap_get_max_mipmap_count(bitmap) + 1) << D3DFORMAT_MIPMAP_SHIFT) |
			D3DFORMAT_BORDERSOURCE_COLOR |
			D3DFORMAT_DMACHANNEL_A;
		IDirect3DBaseTexture8_Register(texture, pixels);
		bitmap->base_address = xbox_address(pixels);
		bitmap->hardware_format = xbox_address(texture);
		ce_ui.picture_count = index + 1;
	}
	if (bitmaps_file != INVALID_HANDLE_VALUE)
		CloseHandle(bitmaps_file);
}

/* what Halo PC's ui.map (maps\ce\ui.map) has of its map list, read the first
time it is asked for */
static void ce_ui_read(
	void)
{
	char path[512];
	unsigned long header[6];
	HANDLE file;

	if (ce_ui.read)
		return;
	ce_ui.read = TRUE;
	snprintf(path, sizeof(path), "%sce\\ui.map", cache_files_map_directory());
	file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return;
	/* ('head', its version, its size, ..., its tags' offset and size) */
	if (file_read(file, 0, header, sizeof(header)) && header[0] == 'head' && header[1] == CE_CACHE_VERSION &&
		header[5] >= 0x10 && header[5] <= 0x4000000)
	{
		ce_tags = malloc(header[5]);
		ce_tags_size = header[5];
		if (ce_tags && file_read(file, header[4], ce_tags, header[5]))
		{
			ce_ui.name_count = ce_string_list_read("ui\\shell\\main_menu\\mp_map_list", ce_ui.names[0],
				DISPLAY_NAME_LENGTH, MAXIMUM_CE_STRINGS);
			ce_ui.description_count = ce_string_list_read(
				"ui\\shell\\main_menu\\multiplayer_type_select\\mp_map_select\\map_data", ce_ui.descriptions[0],
				DESCRIPTION_LENGTH, MAXIMUM_CE_STRINGS);
			ce_pictures_read(file);
		}
		free(ce_tags);
		ce_tags = NULL;
		ce_tags_size = 0;
	}
	CloseHandle(file);
}

static void add_entry(
	char const *map_name,
	wchar_t const *display_name,
	wchar_t const *lobby_name,
	wchar_t const *description,
	short xbox_index,
	short picture_index,
	char const *picture_map)
{
	struct ui_map_entry *entry;

	if (ui_map_list_count_value >= MAXIMUM_MAP_LIST)
		return;
	entry = &ui_map_list[ui_map_list_count_value];
	snprintf(entry->map_name, sizeof(entry->map_name), "%s", map_name);
	wide_copy(entry->display_name, DISPLAY_NAME_LENGTH, display_name);
	wide_copy(entry->lobby_name, DISPLAY_NAME_LENGTH, lobby_name);
	wide_copy(entry->description, DESCRIPTION_LENGTH, description);
	entry->xbox_index = xbox_index;
	entry->picture_index = picture_index;
	snprintf(entry->picture_map, sizeof(entry->picture_map), "%s", picture_map ? picture_map : "");
	ui_map_list_names_array[ui_map_list_count_value] = entry->map_name;
	ui_map_list_count_value++;
}

/* a Custom Edition or HaloMD map's row: its file's name (without its
suffix or .map) */
static void add_pc_entry(
	short family,
	char const *file)
{
	char map_name[MAP_NAME_LENGTH];
	wchar_t name[DISPLAY_NAME_LENGTH];
	wchar_t display_name[DISPLAY_NAME_LENGTH];
	wchar_t lobby_name[DISPLAY_NAME_LENGTH];
	wchar_t const *description = L"A Halo Custom\r\nEdition map";
	wchar_t const *mark = L"CE";
	long ce_index = CE_UNKNOWN_LEVEL;
	short picture_index = 9;
	char const *picture_map = NULL;
	short known;

	name[0] = 0;
	if (family == _map_family_halomd)
	{
		wchar_t const *known_name = halomd_map_name(file);

		description = L"A HaloMD map";
		mark = L"MD";
		if (known_name)
			wide_copy(name, DISPLAY_NAME_LENGTH - 7, known_name);
	}
	for (known = 0; family == _map_family_custom_edition && known < (short)NUMBEROF(ce_maps); known++)
	{
		if (!_stricmp(file, ce_maps[known].file))
		{
			ce_index = known;
			picture_index = ce_maps[known].xbox_picture_index;
			wide_copy(name, DISPLAY_NAME_LENGTH, ce_maps[known].name);
			if (known < ce_ui.name_count && ce_ui.names[known][0])
				wide_copy(name, DISPLAY_NAME_LENGTH, ce_ui.names[known]);
			if (known < ce_ui.description_count)
				description = ce_ui.descriptions[known];
			break;
		}
	}
	if (!name[0])
	{
		/* (a map of another's making: its file's name) */
		long character;

		for (character = 0; file[character] && character < DISPLAY_NAME_LENGTH - 7; character++)
			name[character] = (wchar_t)(unsigned char)file[character];
		name[character] = 0;
	}
	if (ce_index < ce_ui.picture_count)
		picture_index = (short)(UI_MAP_LIST_PICTURE_BASE + ce_index);
	else
		/* (an Xbox map's picture stands in: found by its name in the loaded
		ui.map, a mod's having its own order) */
		picture_map = xbox_map_files[picture_index];
	wide_copy(lobby_name, DISPLAY_NAME_LENGTH, name);
	wide_append(lobby_name, DISPLAY_NAME_LENGTH, L" [");
	wide_append(lobby_name, DISPLAY_NAME_LENGTH, mark);
	wide_append(lobby_name, DISPLAY_NAME_LENGTH, L"]");
	wide_copy(display_name, DISPLAY_NAME_LENGTH, name);
	wide_append(display_name, DISPLAY_NAME_LENGTH, L"\r\n[");
	wide_append(display_name, DISPLAY_NAME_LENGTH, mark);
	wide_append(display_name, DISPLAY_NAME_LENGTH, L"]");
	snprintf(map_name, sizeof(map_name), "%s%s", file, map_family_suffix(family));
	add_entry(map_name, display_name, lobby_name, description, NONE, picture_index, picture_map);
}

/* the files found of a family's maps, while the list is filled */
struct found_files
{
	char names[MAXIMUM_MAP_LIST][MAP_NAME_LENGTH];
	long count;
};

static void file_found(
	char const *file,
	void *context)
{
	struct found_files *found = context;

	if (found->count < MAXIMUM_MAP_LIST && strlen(file) < MAP_NAME_LENGTH - 3)
		snprintf(found->names[found->count++], MAP_NAME_LENGTH, "%s", file);
}

/* ---------- public code */

/* the list anew: the Xbox's maps (the game's thirteen names, in its order),
then the Custom Edition maps found, then HaloMD's */
void ui_map_list_refresh(
	char *const *xbox_maps)
{
	static struct found_files found;
	/* (the counts last logged, to log each change once) */
	static long logged_counts[NUMBER_OF_MAP_FAMILIES] = { -1, -1, -1 };
	long counts[NUMBER_OF_MAP_FAMILIES] = { XBOX_MAP_COUNT, 0, 0 };
	short index, family;

	ui_map_list_count_value = 0;
	for (index = 0; index < XBOX_MAP_COUNT; index++)
	{
		/* (its strings and picture the loaded ui.map's, by its name there:
		ui_map_list_string_index; this name only where that ui.map lacks it) */
		wchar_t name[DISPLAY_NAME_LENGTH];
		char const *caption = xbox_map_captions[index];
		long character;

		for (character = 0; caption[character] && character < DISPLAY_NAME_LENGTH - 1; character++)
			name[character] = (wchar_t)(unsigned char)caption[character];
		name[character] = 0;
		add_entry(xbox_maps[index], name, name, L"", index, index, xbox_maps[index]);
	}
	/* port (upstream PR #63): the Xbox v5 community maps found in the map
	directory and the active mod's maps folder (port/linux/game/custom_maps.c),
	each by its cache's name, after the Xbox's and before Halo PC's */
	{
		short community_count = 0;
		char **community = native_multiplayer_map_list((char **)xbox_maps, XBOX_MAP_COUNT, &community_count);

		for (index = XBOX_MAP_COUNT; index < community_count; index++)
		{
			char caption[HALO_CUSTOM_MAP_NAME_SIZE];
			wchar_t name[DISPLAY_NAME_LENGTH];
			long character;

			native_map_display_name(community[index], caption, sizeof(caption));
			for (character = 0; caption[character] && character < DISPLAY_NAME_LENGTH - 1; character++)
				name[character] = (wchar_t)(unsigned char)caption[character];
			name[character] = 0;
			/* (its picture the loaded ui.map's of its name, else that
			ui.map's unknown level's: ui_map_list_xbox_picture) */
			add_entry(community[index], name, name, L"Community map", NONE, 13, community[index]);
		}
	}
	for (family = _map_family_custom_edition; family < NUMBER_OF_MAP_FAMILIES; family++)
	{
		extern int platform_ce_tag_cache_ready;
		long file;

		/* (none of them would load without their tag cache, which the
		platform layer could not map: port/linux/src/xbox_memory.c) */
		if (!platform_ce_tag_cache_ready)
			break;
		found.count = 0;
		map_family_list(family, file_found, &found);
		if (found.count)
			ce_ui_read();
		/* (in the order of their names, as the list shows them) */
		qsort(found.names, found.count, MAP_NAME_LENGTH, (int (*)(void const *, void const *))_stricmp);
		for (file = 0; file < found.count; file++)
			add_pc_entry(family, found.names[file]);
		counts[family] = found.count;
	}
	if (memcmp(counts, logged_counts, sizeof(counts)))
	{
		memcpy(logged_counts, counts, sizeof(counts));
		error(_error_silent, "the menus' map list: %ld Custom Edition maps, %ld HaloMD maps",
			counts[_map_family_custom_edition], counts[_map_family_halomd]);
	}
}

long ui_map_list_count(
	void)
{
	return ui_map_list_count_value;
}

/* the rows' map names, for the list widget's items */
char **ui_map_list_names(
	void)
{
	return ui_map_list_names_array;
}

/* the row of a map's name (as the list or a game has it), or NONE */
long ui_map_list_find(
	char const *map_name)
{
	long row;

	if (!map_name)
		return NONE;
	for (row = 0; row < ui_map_list_count_value; row++)
	{
		if (!_stricmp(map_name, ui_map_list[row].map_name))
			return row;
	}
	return NONE;
}

/* AE: a map's name on one line, for the post-game report: a stock map's caption
(or its Halo 1: NHE file's alias: badcreek is Battle Creek), else its row's
lobby name, else the file's name made readable. Ask ui_map_list_lookup first so
that the list has the map's row. */
void ui_map_list_caption(
	char const *map_name,
	wchar_t *text,
	long size)
{
	char const *file = native_map_basename(map_name);
	long row, index;
	short stock = xbox_map_index(file);
	char const *caption = stock != NONE ? xbox_map_captions[stock] : NULL;
	char readable[DISPLAY_NAME_LENGTH];

	text[0] = 0;
	if (size <= 0)
		return;
	for (index = 0; index < (long)NUMBEROF(map_file_captions); index++)
	{
		if (!_stricmp(file, map_file_captions[index].file))
			caption = map_file_captions[index].caption;
	}
	if (!caption && (row = ui_map_list_find(map_name)) != NONE && ui_map_list[row].lobby_name[0])
	{
		wide_copy(text, size, ui_map_list[row].lobby_name);
		return;
	}
	if (!caption)
	{
		native_map_display_name(map_name, readable, sizeof(readable));
		caption = readable;
	}
	for (index = 0; index < size - 1 && caption[index]; index++)
		text[index] = (wchar_t)(unsigned char)caption[index];
	text[index] = 0;
}

/* a row's string list index for one of its strings (_ui_map_list_string_*):
the Xbox's own, or this list's */
short ui_map_list_string_index(
	long row,
	short kind)
{
	if (row < 0 || row >= ui_map_list_count_value)
		return 0;
	if (ui_map_list[row].xbox_index != NONE)
	{
		/* (port: a stock map's strings are the loaded ui.map's of its name,
		a mod's in its own order) */
		boolean own;
		short index = ui_map_list_xbox_picture(ui_map_list[row].map_name, &own);

		if (own)
			return index;
	}
	return (short)(UI_MAP_LIST_STRING_BASE + row * UI_MAP_LIST_STRINGS_PER_ROW + kind);
}

/* a row's bitmap frame: an Xbox map's, or Halo PC's picture of it */
short ui_map_list_picture_index(
	long row)
{
	if (row < 0 || row >= ui_map_list_count_value)
		return 0;
	if (ui_map_list[row].picture_map[0])
		return ui_map_list_xbox_picture(ui_map_list[row].picture_map, NULL);
	return ui_map_list[row].picture_index;
}

/* the bitmap of a frame of this list's (UI_MAP_LIST_PICTURE_BASE and up), or
NULL for any other */
struct bitmap_data *ui_map_list_picture(
	short frame_index)
{
	if (frame_index < UI_MAP_LIST_PICTURE_BASE || frame_index - UI_MAP_LIST_PICTURE_BASE >= ce_ui.picture_count)
		return NULL;
	return &ce_ui.pictures[frame_index - UI_MAP_LIST_PICTURE_BASE];
}

/* the text of a string list index of this list's (UI_MAP_LIST_STRING_BASE
and up), or NULL for any other */
wchar_t const *ui_map_list_text(
	short string_list_index)
{
	long row;

	if (string_list_index < UI_MAP_LIST_STRING_BASE)
		return NULL;
	row = (string_list_index - UI_MAP_LIST_STRING_BASE) / UI_MAP_LIST_STRINGS_PER_ROW;
	if (row >= ui_map_list_count_value)
		return NULL;
	switch ((string_list_index - UI_MAP_LIST_STRING_BASE) % UI_MAP_LIST_STRINGS_PER_ROW)
	{
	case _ui_map_list_string_description:
		return ui_map_list[row].description;
	case _ui_map_list_string_lobby_name:
		return ui_map_list[row].lobby_name;
	default:
		return ui_map_list[row].display_name;
	}
}

/* Halo PC's picture of a Custom Edition or HaloMD map, by its file's name:
its own of Halo PC's maps, its unknown level's for another's (and for every
HaloMD map); NULL without Halo PC's ui.map */
struct bitmap_data *ui_map_list_family_picture(
	short family,
	char const *file)
{
	long index = family == _map_family_custom_edition ? ce_map_index(file) : NONE;

	ce_ui_read();
	if (index == NONE)
		index = CE_UNKNOWN_LEVEL;
	return index < ce_ui.picture_count ? &ce_ui.pictures[index] : NULL;
}

/* whether a family's folders have a map of this file's name, of its
family's version (where the game loads one from: cache_files_windows.c) */
boolean ui_map_list_family_present(
	short family,
	char const *file)
{
	char path[512];

	return family != _map_family_xbox && map_family_find(family, file, path, sizeof(path));
}

#endif

/* a Custom Edition or HaloMD map's name, by its file's name: Halo PC's own
for its maps (as its ui.map has it, on the builds with Custom Edition maps),
HaloMD's mod list's for HaloMD's, else the file's name made readable */
void ui_map_list_family_name(
	short family,
	char const *file,
	wchar_t *name,
	long size)
{
	long index = family == _map_family_custom_edition ? ce_map_index(file) : NONE;
	wchar_t const *known = family == _map_family_halomd ? halomd_map_name(file) : NULL;
	long length;

	if (index == NONE && !known)
	{
		ce_map_tidy_name(file, name, size);
		return;
	}
	if (index == NONE)
	{
		for (length = 0; length < size - 1 && known[length]; length++)
			name[length] = known[length];
		name[length] = 0;
		return;
	}
	known = ce_maps[index].name;
#ifdef HALO_CUSTOM_EDITION
	ce_ui_read();
	if (index < ce_ui.name_count && ce_ui.names[index][0])
		known = ce_ui.names[index];
#endif
	for (length = 0; length < size - 1 && known[length]; length++)
		name[length] = known[length];
	name[length] = 0;
}
