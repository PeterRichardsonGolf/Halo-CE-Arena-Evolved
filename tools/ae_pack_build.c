/*
AE_PACK_BUILD.C

The weapon pack builder (docs/weapon-packs.md, "Building a pack"): a
weapon's tag graph out of a DONOR map (an Xbox map of the game's build)
into a pack, through the game's own objects of the tag validator, the
schema, the pack writer and the builder (port/linux/game/ae_pack_build.c).

	ae_pack_build --donor <map> --weapon <tag name> [--root <group>:<name>]...
		--external-names <file> --prefix <name prefix> --id <pack id>
		--revision <n> --display <name> --weapon-key <key> --labels <label>[,<fallback>...]
		[--behaviour 0..3] [--replaces 0|2..9] [--texts <file>]
		[--icon <bitmap>:<sequence>] [--smooth <bitmap>[:coverage]]...
		[--credits <file>] [--verbose] --out <pack>
	ae_pack_build --list-names [--out <file>] <map>...

The external names are lines "<group>\t<name>" (a group's four
characters, a tab, the tag's name), as --list-names writes them for the
given maps: every tag of each (checked by the tag validator), one line a
group and name (ASCII case aside), sorted. A texts file has lines
"menu_name=...", "weapon_pickup=...", "ammo_pickup=..." (each optional;
blank lines and lines starting with '#' are skipped); the credits file is
UTF-8 text.

Prints the pack's counts (and with --verbose every tag and the validator's
corrections of the donor) and exits 0; a refusal prints "ae_pack_build:
refused: <reason>" and exits 1, with nothing written; bad arguments exit 2.

Built by ninja linux as build/linux/ae_pack_build (32-bit: the donor's
tags must lie at the tag cache's address, as map_validate's), with the
platform layer's POSIX flags against the game's objects, whose functions
take and return the same types in both ABIs.
*/

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "zlib_prefixed.h"

#include "ae_pack_build.h"

/* ---------- constants */

enum
{
	CACHE_FILE_HEADER_SIZE = 0x800,
	CACHE_FILE_HEADER_SIGNATURE = 0x68656164, /* 'head' */
	CACHE_FILE_FOOTER_SIGNATURE = 0x666F6F74, /* 'foot' */
	CACHE_FILE_XBOX_VERSION = 5,
	/* a donor on disk and inflated */
	MAXIMUM_DONOR_SIZE = 0x10000000,
	MAXIMUM_SIDE_FILE_SIZE = 16 * 1024 * 1024,
	MAXIMUM_PREFIX = 200,
	MAXIMUM_LABELS = 3,
};

#define USAGE \
	"usage: ae_pack_build --donor <map> --weapon <tag name> [--root <group>:<name>]...\n" \
	"           --external-names <file> --prefix <name prefix> --id <pack id> --revision <n>\n" \
	"           --display <name> --weapon-key <key> --labels <label>[,<fallback>...]\n" \
	"           [--behaviour 0..3] [--replaces 0|2..9] [--texts <file>] [--icon <bitmap>:<sequence>]\n" \
	"           [--smooth <bitmap>[:coverage]]... [--credits <file>] [--verbose] --out <pack>\n" \
	"       ae_pack_build --donor <map> --root <group>:<name> [--root ...]... --external-names <file>\n" \
	"           --prefix <name prefix> --id <pack id> --revision <n> --display <name>\n" \
	"           [--smooth <bitmap>[:coverage]]... [--credits <file>] [--verbose] --out <pack>   (tags only)\n" \
	"       ae_pack_build --list-names [--out <file>] <map>...\n"

/* ---------- the game's C library names (port/linux/include/stdio.h, cseries.h), which its objects call */

FILE *halo_linux_fopen(char const *path, char const *mode);
int halo_linux_remove(char const *path);
int halo_linux_rename(char const *old_path, char const *new_path);
int halo_linux_vsnprintf(char *buffer, size_t count, char const *format, va_list arguments);
int halo_linux_snprintf(char *buffer, size_t count, char const *format, ...);
void *csmemset(void *buffer, long c, unsigned long size);
void *csmemcpy(void *destination, void const *source, unsigned long size);
unsigned long csstrlen(char const *s);
char *csstrcat(char *s1, char const *s2);
long csstrcmp(char const *s1, char const *s2);
void tag_validate_report(char const *message);

/* the validator's (port/linux/game/tag_validate.c) */
long tag_validate_corrections(void);

FILE *halo_linux_fopen(char const *path, char const *mode)
{
	return fopen(path, mode);
}

int halo_linux_remove(char const *path)
{
	return remove(path);
}

int halo_linux_rename(char const *old_path, char const *new_path)
{
	return rename(old_path, new_path);
}

int halo_linux_vsnprintf(char *buffer, size_t count, char const *format, va_list arguments)
{
	return vsnprintf(buffer, count, format, arguments);
}

int halo_linux_snprintf(char *buffer, size_t count, char const *format, ...)
{
	va_list arguments;
	int result;

	va_start(arguments, format);
	result = vsnprintf(buffer, count, format, arguments);
	va_end(arguments);
	return result;
}

void *csmemset(void *buffer, long c, unsigned long size)
{
	return memset(buffer, (int)c, size);
}

void *csmemcpy(void *destination, void const *source, unsigned long size)
{
	return memcpy(destination, source, size);
}

unsigned long csstrlen(char const *s)
{
	return strlen(s);
}

char *csstrcat(char *s1, char const *s2)
{
	return strcat(s1, s2);
}

long csstrcmp(char const *s1, char const *s2)
{
	return strcmp(s1, s2);
}

/* the validator's corrections of a donor: printed with --verbose */
static int verbose;

void tag_validate_report(char const *message)
{
	if (verbose)
		printf("donor: %s\n", message);
}

/* ---------- refusals */

static void usage_error(char const *format, ...)
{
	va_list arguments;

	fprintf(stderr, "ae_pack_build: ");
	va_start(arguments, format);
	vfprintf(stderr, format, arguments);
	va_end(arguments);
	fprintf(stderr, "\n%s", USAGE);
	exit(2);
}

static void refuse(char const *format, ...)
{
	va_list arguments;

	fprintf(stderr, "ae_pack_build: refused: ");
	va_start(arguments, format);
	vfprintf(stderr, format, arguments);
	va_end(arguments);
	fprintf(stderr, "\n");
	exit(1);
}

/* ---------- arguments */

static int printable(char const *text, size_t minimum, size_t maximum)
{
	size_t length = strlen(text);
	size_t index;

	if (length < minimum || length > maximum)
		return 0;
	for (index = 0; index < length; index++)
	{
		if ((unsigned char)text[index] < 0x20 || (unsigned char)text[index] > 0x7e)
			return 0;
	}
	return 1;
}

/* [a-z0-9_]{1,31}: a pack id or weapon key */
static int key_valid(char const *text)
{
	size_t length = strlen(text);
	size_t index;

	if (length < 1 || length > 31)
		return 0;
	for (index = 0; index < length; index++)
	{
		char c = text[index];

		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
			return 0;
	}
	return 1;
}

/* a whole decimal number in [minimum, maximum], or -1 */
static long long number(char const *text, long long minimum, long long maximum)
{
	long long value = 0;
	char const *c;

	if (!*text || strlen(text) > 10)
		return -1;
	for (c = text; *c; c++)
	{
		if (*c < '0' || *c > '9')
			return -1;
		value = value * 10 + (*c - '0');
	}
	return value >= minimum && value <= maximum ? value : -1;
}

/* a group's four characters as its tag ('weap'), or 0 */
static unsigned int group_of(char const *text, size_t length)
{
	unsigned int group = 0;
	size_t index;

	if (length != 4)
		return 0;
	for (index = 0; index < 4; index++)
	{
		if ((unsigned char)text[index] < 0x20 || (unsigned char)text[index] > 0x7e)
			return 0;
		group = group << 8 | (unsigned char)text[index];
	}
	return group;
}

static char const *group_text(unsigned int group, char *text)
{
	int index;

	for (index = 0; index < 4; index++)
		text[index] = (char)(group >> (24 - 8 * index));
	text[4] = 0;
	return text;
}

/* ---------- files */

/* the file whole, terminated: NULL if it cannot be read or is larger than maximum */
static char *file_read(char const *path, size_t maximum, size_t *size)
{
	FILE *file = fopen(path, "rb");
	char *bytes = NULL;
	long length;

	if (!file)
		return NULL;
	if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 || (size_t)length > maximum ||
		fseek(file, 0, SEEK_SET))
	{
		fclose(file);
		return NULL;
	}
	bytes = malloc((size_t)length + 1);
	if (bytes && length && fread(bytes, (size_t)length, 1, file) != 1)
	{
		free(bytes);
		bytes = NULL;
	}
	fclose(file);
	if (bytes)
	{
		bytes[length] = 0;
		*size = (size_t)length;
	}
	return bytes;
}

/* the next line of text (its end made a terminator), or NULL at the end */
static char *line_next(char **cursor)
{
	char *line = *cursor;
	char *end;

	if (!line || !*line)
		return NULL;
	end = strchr(line, '\n');
	if (end)
	{
		*end = 0;
		*cursor = end + 1;
	}
	else
	{
		*cursor = line + strlen(line);
	}
	if (end && end > line && end[-1] == '\r')
		end[-1] = 0;
	return line;
}

static struct ae_pack_builder_name *external_names_read(char const *path, unsigned int *count)
{
	size_t size;
	char *text = file_read(path, MAXIMUM_SIDE_FILE_SIZE, &size);
	char *cursor = text;
	char *line;
	struct ae_pack_builder_name *names = NULL;
	unsigned int capacity = 0;
	unsigned int line_number = 0;

	*count = 0;
	if (!text)
		refuse("external names %s cannot be read (or are larger than 16 MiB)", path);
	if (memchr(text, 0, size))
		refuse("external names %s hold a NUL", path);
	while ((line = line_next(&cursor)) != NULL)
	{
		char *tab = strchr(line, '\t');
		unsigned int group;

		line_number++;
		if (!*line)
			continue;
		group = tab ? group_of(line, (size_t)(tab - line)) : 0;
		if (!group || !printable(tab + 1, 1, AE_PACK_MAXIMUM_NAME))
			refuse("external names %s line %u: not \"<group>\\t<name>\"", path, line_number);
		if (*count == capacity)
		{
			capacity = capacity ? capacity * 2 : 1024;
			names = realloc(names, capacity * sizeof(*names));
			if (!names)
				refuse("out of memory");
		}
		names[*count].group = group;
		names[*count].name = tab + 1;
		(*count)++;
	}
	/* (the text stays: the names point into it) */
	return names;
}

static void texts_read(char const *path, struct ae_pack_builder_options *options)
{
	static char const *const keys[] = { "menu_name", "weapon_pickup", "ammo_pickup" };
	char const **values[3];
	int seen[3] = { 0, 0, 0 };
	size_t size;
	char *text = file_read(path, MAXIMUM_SIDE_FILE_SIZE, &size);
	char *cursor = text;
	char *line;
	unsigned int line_number = 0;

	values[0] = &options->menu_name;
	values[1] = &options->weapon_pickup_text;
	values[2] = &options->ammo_pickup_text;
	if (!text)
		refuse("texts %s cannot be read", path);
	if (memchr(text, 0, size))
		refuse("texts %s hold a NUL", path);
	while ((line = line_next(&cursor)) != NULL)
	{
		char *equals = strchr(line, '=');
		int index;

		line_number++;
		if (!*line || *line == '#')
			continue;
		if (!equals)
			refuse("texts %s line %u: not \"<key>=<text>\"", path, line_number);
		*equals = 0;
		for (index = 0; index < 3 && strcmp(line, keys[index]); index++)
			;
		if (index == 3)
			refuse("texts %s line %u: unknown key %s (menu_name, weapon_pickup, ammo_pickup)", path, line_number,
				line);
		if (seen[index]++)
			refuse("texts %s line %u: %s given twice", path, line_number, line);
		if (!equals[1])
			refuse("texts %s line %u: %s is empty", path, line_number, line);
		*values[index] = equals + 1;
	}
}

/* the map at path, inflated, in memory (its header's length): refused if it is not an Xbox map of the game's */
static unsigned char *map_read(char const *path, char const *what, size_t *size)
{
	size_t disk_size;
	unsigned char *disk = (unsigned char *)file_read(path, MAXIMUM_DONOR_SIZE, &disk_size);
	unsigned char *whole;
	unsigned int file_length;
	unsigned int tag_data_offset, tag_data_size;
	unsigned int value;

	if (!disk)
		refuse("the %s cannot be read (or is larger than 256 MiB): %s", what, path);
	if (disk_size < CACHE_FILE_HEADER_SIZE)
		refuse("the %s is not an Xbox map of the game's: %s", what, path);
	memcpy(&value, disk, 4);
	memcpy(&file_length, disk + 8, 4);
	{
		unsigned int version, footer;

		memcpy(&version, disk + 4, 4);
		memcpy(&footer, disk + CACHE_FILE_HEADER_SIZE - 4, 4);
		if (value != CACHE_FILE_HEADER_SIGNATURE || footer != CACHE_FILE_FOOTER_SIGNATURE ||
			version != CACHE_FILE_XBOX_VERSION || file_length < CACHE_FILE_HEADER_SIZE ||
			file_length > MAXIMUM_DONOR_SIZE || !memchr(disk + 0x40, 0, 0x20))
		{
			refuse("the %s is not an Xbox map of the game's: %s", what, path);
		}
	}
	if (disk_size == file_length)
	{
		whole = disk;
	}
	else
	{
		/* (a map as the disc has it: its header, then the rest deflated, all of it and no more than its header
		says) */
		z_stream stream;
		int result;

		whole = malloc(file_length);
		if (!whole)
			refuse("out of memory");
		memcpy(whole, disk, CACHE_FILE_HEADER_SIZE);
		memset(&stream, 0, sizeof(stream));
		stream.next_in = disk + CACHE_FILE_HEADER_SIZE;
		stream.avail_in = (uInt)(disk_size - CACHE_FILE_HEADER_SIZE);
		stream.next_out = whole + CACHE_FILE_HEADER_SIZE;
		stream.avail_out = (uInt)(file_length - CACHE_FILE_HEADER_SIZE);
		if (inflateInit(&stream) != Z_OK)
			refuse("the %s could not be inflated: %s", what, path);
		result = inflate(&stream, Z_FINISH);
		inflateEnd(&stream);
		if (result != Z_STREAM_END || stream.avail_out != 0)
			refuse("the %s could not be inflated: %s", what, path);
		free(disk);
	}
	memcpy(&tag_data_offset, whole + 0x10, 4);
	memcpy(&tag_data_size, whole + 0x14, 4);
	if (tag_data_size < 0x24 || tag_data_size > AE_PACK_BUILDER_TAG_CACHE_SIZE || tag_data_offset > file_length ||
		tag_data_size > file_length - tag_data_offset)
	{
		refuse("the %s's tags are not in its file, or do not fit the tag cache: %s", what, path);
	}
	*size = file_length;
	return whole;
}

/* the map's tags put where the game reads them (the tag cache's address) */
static void *tags_place(unsigned char *tag_cache, unsigned char const *map)
{
	unsigned int tag_data_offset, tag_data_size;

	memcpy(&tag_data_offset, map + 0x10, 4);
	memcpy(&tag_data_size, map + 0x14, 4);
	memset(tag_cache, 0xCD, AE_PACK_BUILDER_TAG_CACHE_SIZE);
	memcpy(tag_cache, map + tag_data_offset, tag_data_size);
	return tag_cache;
}

static unsigned char *tag_cache_map(void)
{
	unsigned char *tag_cache = mmap((void *)AE_PACK_BUILDER_TAG_CACHE_ADDRESS, AE_PACK_BUILDER_TAG_CACHE_SIZE,
		PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);

	if (tag_cache != (unsigned char *)AE_PACK_BUILDER_TAG_CACHE_ADDRESS)
	{
		fprintf(stderr, "ae_pack_build: the tag cache's address (%08x) is taken\n", AE_PACK_BUILDER_TAG_CACHE_ADDRESS);
		exit(2);
	}
	return tag_cache;
}

/* ---------- --list-names */

struct listed_name
{
	unsigned int group;
	char *name;
};

static struct listed_name *listed;
static size_t listed_count;
static size_t listed_capacity;

static void name_listed(void *context, unsigned int group, char const *name)
{
	(void)context;
	if (listed_count == listed_capacity)
	{
		listed_capacity = listed_capacity ? listed_capacity * 2 : 4096;
		listed = realloc(listed, listed_capacity * sizeof(*listed));
		if (!listed)
			refuse("out of memory");
	}
	listed[listed_count].group = group;
	listed[listed_count].name = strdup(name);
	if (!listed[listed_count].name)
		refuse("out of memory");
	listed_count++;
}

static int lower(int c)
{
	return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c;
}

/* by group, then name (ASCII case aside) */
static int listed_compare(void const *a, void const *b)
{
	struct listed_name const *first = a;
	struct listed_name const *second = b;
	unsigned char const *x = (unsigned char const *)first->name;
	unsigned char const *y = (unsigned char const *)second->name;

	if (first->group != second->group)
		return first->group < second->group ? -1 : 1;
	while (*x && lower(*x) == lower(*y))
	{
		x++;
		y++;
	}
	return lower(*x) - lower(*y);
}

static int list_names(int argc, char **argv)
{
	char const *out = NULL;
	unsigned char *tag_cache;
	FILE *output;
	size_t index, kept;
	int argument;
	int maps = 0;

	for (argument = 2; argument < argc; argument++)
	{
		if (!strcmp(argv[argument], "--out") && argument + 1 < argc)
			out = argv[++argument];
		else if (argv[argument][0] == '-' && argv[argument][1] == '-')
			usage_error("%s is not an option of --list-names", argv[argument]);
		else
			maps++;
	}
	if (!maps)
		usage_error("--list-names needs maps");
	tag_cache = tag_cache_map();
	for (argument = 2; argument < argc; argument++)
	{
		struct ae_pack_problem problem;
		unsigned char *map;
		size_t size;
		unsigned int tag_data_size;

		if (!strcmp(argv[argument], "--out"))
		{
			argument++;
			continue;
		}
		map = map_read(argv[argument], "map", &size);
		memcpy(&tag_data_size, map + 0x14, 4);
		if (!ae_pack_builder_names(tags_place(tag_cache, map), tag_data_size, (unsigned int)size, argv[argument],
			name_listed, NULL, &problem))
		{
			refuse("%s", problem.text);
		}
		free(map);
	}
	qsort(listed, listed_count, sizeof(*listed), listed_compare);
	output = out ? fopen(out, "w") : stdout;
	if (!output)
		refuse("%s cannot be written", out);
	for (index = 0, kept = 0; index < listed_count; index++)
	{
		char text[5];

		if (index && !listed_compare(&listed[index - 1], &listed[index]))
			continue;
		if (!group_of(group_text(listed[index].group, text), 4))
			refuse("a tag of group %08x, which a names file cannot say", listed[index].group);
		fprintf(output, "%s\t%s\n", text, listed[index].name);
		kept++;
	}
	if (out && fclose(output))
		refuse("%s cannot be written", out);
	if (out)
		printf("%s: %zu names of %d maps\n", out, kept, maps);
	return 0;
}

/* ---------- building */

static char const *flags_text(unsigned int flags)
{
	switch (flags & AE_PACK_TAG_KNOWN_FLAGS)
	{
	case AE_PACK_TAG_FLAG_SMOOTH:
		return "smooth";
	case AE_PACK_TAG_FLAG_METER_COVERAGE:
		return "coverage";
	case AE_PACK_TAG_FLAG_SMOOTH | AE_PACK_TAG_FLAG_METER_COVERAGE:
		return "smooth,coverage";
	default:
		return "-";
	}
}

int main(int argc, char **argv)
{
	static struct ae_pack_builder_name roots[AE_PACK_BUILDER_MAXIMUM_ROOTS];
	static struct ae_pack_builder_flags flags[AE_PACK_BUILDER_MAXIMUM_SMOOTH];
	static char *labels[MAXIMUM_LABELS];
	struct ae_pack_builder_options options;
	struct ae_pack_builder_summary summary;
	struct ae_pack_problem problem;
	char const *donor = NULL, *externals = NULL, *texts = NULL, *credits = NULL, *out = NULL;
	char const *revision = NULL, *label_list = NULL;
	unsigned char *map;
	unsigned char *tag_cache;
	size_t map_size;
	unsigned int label_count = 0;
	unsigned int index;
	int argument;
	int written;

	if (argc < 2)
	{
		fprintf(stderr, "%s", USAGE);
		return 2;
	}
	if (!strcmp(argv[1], "--list-names"))
		return list_names(argc, argv);

	memset(&options, 0, sizeof(options));
	options.icon_sequence = 0;
	for (argument = 1; argument < argc; argument++)
	{
		char const *option = argv[argument];
		char const *value;

		if (!strcmp(option, "--verbose"))
		{
			verbose = 1;
			continue;
		}
		if (strncmp(option, "--", 2) || argument + 1 >= argc)
			usage_error("%s: not an option, or its value is missing", option);
		value = argv[++argument];
		if (!strcmp(option, "--donor"))
			donor = value;
		else if (!strcmp(option, "--weapon"))
			options.weapon_name = value;
		else if (!strcmp(option, "--external-names"))
			externals = value;
		else if (!strcmp(option, "--prefix"))
			options.prefix = value;
		else if (!strcmp(option, "--id"))
			options.pack_id = value;
		else if (!strcmp(option, "--revision"))
			revision = value;
		else if (!strcmp(option, "--display"))
			options.display_name = value;
		else if (!strcmp(option, "--weapon-key"))
			options.weapon_key = value;
		else if (!strcmp(option, "--labels"))
			label_list = value;
		else if (!strcmp(option, "--texts"))
			texts = value;
		else if (!strcmp(option, "--credits"))
			credits = value;
		else if (!strcmp(option, "--out"))
			out = value;
		else if (!strcmp(option, "--behaviour"))
		{
			long long behaviour = number(value, AE_PACK_BEHAVIOUR_TAGS, AE_PACK_BEHAVIOUR_LAST);

			if (behaviour < 0)
				usage_error("--behaviour %s: 0 (as its tags say), 1 (the AE battle rifle), 2 (the EVO RIFLE) or 3 (the DMR)", value);
			options.behaviour = (unsigned int)behaviour;
		}
		else if (!strcmp(option, "--replaces"))
		{
			long long replaces = number(value, 0, AE_PACK_REPLACES_LAST);

			if (replaces < 0 || replaces == 1)
				usage_error("--replaces %s: 0 (none) or a loadout weapon 2..9", value);
			options.replaces = (unsigned int)replaces;
		}
		else if (!strcmp(option, "--root"))
		{
			char const *colon = strchr(value, ':');

			if (options.root_count == AE_PACK_BUILDER_MAXIMUM_ROOTS)
				usage_error("--root: at most %d", AE_PACK_BUILDER_MAXIMUM_ROOTS);
			if (!colon || !group_of(value, (size_t)(colon - value)) || !printable(colon + 1, 1, AE_PACK_MAXIMUM_NAME))
				usage_error("--root %s: not <group>:<name>", value);
			roots[options.root_count].group = group_of(value, 4);
			roots[options.root_count].name = colon + 1;
			options.root_count++;
		}
		else if (!strcmp(option, "--icon"))
		{
			char const *colon = strrchr(value, ':');
			long long sequence = colon ? number(colon + 1, 0, AE_PACK_MAXIMUM_ICON_SEQUENCE) : -1;
			char *name;

			if (sequence < 0 || colon == value)
				usage_error("--icon %s: not <bitmap>:<sequence>", value);
			name = strndup(value, (size_t)(colon - value));
			if (!name || !printable(name, 1, AE_PACK_MAXIMUM_NAME))
				usage_error("--icon %s: not <bitmap>:<sequence>", value);
			options.icon_name = name;
			options.icon_sequence = (unsigned int)sequence;
		}
		else if (!strcmp(option, "--smooth"))
		{
			char const *colon = strrchr(value, ':');
			char *name;

			if (options.flag_count == AE_PACK_BUILDER_MAXIMUM_SMOOTH)
				usage_error("--smooth: at most %d", AE_PACK_BUILDER_MAXIMUM_SMOOTH);
			if (colon && strcmp(colon + 1, "coverage"))
				usage_error("--smooth %s: not <bitmap> or <bitmap>:coverage", value);
			name = colon ? strndup(value, (size_t)(colon - value)) : strdup(value);
			if (!name || !printable(name, 1, AE_PACK_MAXIMUM_NAME))
				usage_error("--smooth %s: not <bitmap> or <bitmap>:coverage", value);
			flags[options.flag_count].name = name;
			flags[options.flag_count].flags = AE_PACK_TAG_FLAG_SMOOTH | (colon ? AE_PACK_TAG_FLAG_METER_COVERAGE : 0);
			options.flag_count++;
		}
		else
		{
			usage_error("%s is not an option", option);
		}
	}

	/* every value checked before anything is read */
	if (!donor)
		usage_error("--donor is needed");
	/* (no --weapon: a pack of tags only, its roots' tags, which the game finds by name: no weapon entry and none
	of the weapon's options) */
	if (options.weapon_name && !printable(options.weapon_name, 1, AE_PACK_MAXIMUM_NAME))
		usage_error("--weapon: a tag name");
	if (!options.weapon_name && (!options.root_count || options.weapon_key || label_list || texts ||
		options.icon_name || options.behaviour || options.replaces))
	{
		usage_error("--weapon is needed (a tag name), or for a pack of tags only --root and none of --weapon-key, "
			"--labels, --texts, --icon, --behaviour, --replaces");
	}
	if (!externals)
		usage_error("--external-names is needed (ae_pack_build --list-names writes them)");
	if (!options.prefix || !printable(options.prefix, 1, MAXIMUM_PREFIX) ||
		options.prefix[strlen(options.prefix) - 1] != '\\')
	{
		usage_error("--prefix is needed: a name ending in a backslash, up to %d characters", MAXIMUM_PREFIX);
	}
	if (!options.pack_id || !key_valid(options.pack_id))
		usage_error("--id is needed: [a-z0-9_], 1 to 31 characters");
	if (!revision || number(revision, 1, 0xFFFFFFFFLL) < 1)
		usage_error("--revision is needed: 1, 2, ...");
	options.pack_revision = (unsigned int)number(revision, 1, 0xFFFFFFFFLL);
	if (!options.display_name || !printable(options.display_name, 1, AE_PACK_DISPLAY_NAME_SIZE - 1))
		usage_error("--display is needed: printable ASCII, 1 to 31 characters");
	if (options.weapon_name && (!options.weapon_key || !key_valid(options.weapon_key)))
		usage_error("--weapon-key is needed: [a-z0-9_], 1 to 31 characters");
	if (options.weapon_name && !label_list)
		usage_error("--labels is needed: <label>[,<fallback>[,<fallback>]]");
	if (label_list)
	{
		char *copy = strdup(label_list);
		char *cursor = copy;

		while (copy)
		{
			char *comma = strchr(cursor, ',');

			if (comma)
				*comma = 0;
			if (label_count == MAXIMUM_LABELS || !printable(cursor, 1, AE_PACK_MAXIMUM_LABEL))
				usage_error("--labels %s: one to three labels, each 1 to 31 printable characters", label_list);
			labels[label_count++] = cursor;
			if (!comma)
				break;
			cursor = comma + 1;
		}
	}
	if (!out)
		usage_error("--out is needed");
	options.label = label_count ? labels[0] : NULL;
	options.fallback_labels[0] = label_count > 1 ? labels[1] : NULL;
	options.fallback_labels[1] = label_count > 2 ? labels[2] : NULL;
	options.menu_name = options.display_name;
	options.roots = roots;
	options.flags = flags;
	options.power = 0;

	/* the side files, then the donor */
	options.external_names = external_names_read(externals, &options.external_name_count);
	if (texts)
		texts_read(texts, &options);
	if (credits)
	{
		size_t size;
		char *text = file_read(credits, AE_PACK_MAXIMUM_CREDITS, &size);

		if (!text)
			refuse("credits %s cannot be read (or are larger than 64 KiB)", credits);
		if (memchr(text, 0, size))
			refuse("credits %s hold a NUL", credits);
		options.credits = text;
	}
	map = map_read(donor, "donor", &map_size);
	tag_cache = tag_cache_map();
	options.tag_data = tags_place(tag_cache, map);
	memcpy(&options.tag_data_size, map + 0x14, 4);
	options.file = map;
	options.file_size = (unsigned int)map_size;
	options.donor_name = donor;

	written = ae_pack_builder_build(&options, out, &summary, &problem);
	if (verbose)
	{
		for (index = 0; index < summary.tag_count; index++)
		{
			struct ae_pack_builder_tag const *tag = &summary.tags[index];
			char text[5];

			printf("tag %u %s %s required %u flags %s bytes %u fixups %u parts %u resources %u (%u bytes) "
				"name \"%s\" source \"%s\"\n", index, group_text(tag->group, text),
				tag->kind == AE_PACK_TAG_OWN ? "own" : "external", tag->required, flags_text(tag->flags), tag->bytes,
				tag->fixups, tag->parts, tag->resources, tag->resource_bytes, tag->name, tag->source_path);
		}
	}
	if (!written)
	{
		ae_pack_builder_summary_free(&summary);
		refuse("%s", problem.text);
	}
	{
		unsigned char header[AE_PACK_HEADER_SIZE];
		FILE *file = fopen(out, "rb");
		long size = -1;

		if (!file || fread(header, sizeof(header), 1, file) != 1 || fseek(file, 0, SEEK_END) ||
			(size = ftell(file)) < 0)
		{
			refuse("%s cannot be read back", out);
		}
		fclose(file);
		printf("donor %s: %s %s, %u corrections\n", donor, options.weapon_name ? "weapon" : "tags only, roots",
			options.weapon_name ? options.weapon_name : roots[0].name, summary.corrections);
		printf("pack %s: %u tags (%u own, %u external: %u required), %u fixups, %u model parts, %u resources; "
			"tag bytes %u, geometry %u, resources %u; %ld bytes, sha256 ", out, summary.tag_count, summary.own_count,
			summary.external_count, summary.required_count, summary.fixup_count, summary.part_count,
			summary.resource_count, summary.tag_bytes, summary.geometry_bytes, summary.resource_bytes, size);
		for (index = 0; index < AE_PACK_SHA256_SIZE; index++)
			printf("%02x", header[AE_PACK_SHA256_OFFSET + index]);
		printf("\n");
	}
	ae_pack_builder_summary_free(&summary);
	return 0;
}
