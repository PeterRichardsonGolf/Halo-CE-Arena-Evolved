/* The downloaded maps' names (ae_downloaded_maps_rules.h). */
#include <string.h>

#include "ae_downloaded_maps_rules.h"

enum { SLOTS = AE_DOWNLOADED_MAPS_MAXIMUM * 2 };

static char names[AE_DOWNLOADED_MAPS_MAXIMUM][AE_DOWNLOADED_MAPS_NAME];
/* open addressing: the name's index plus one, 0 for an empty slot */
static int slots[SLOTS];
static int count;
static int overflowed;

static int lower(int c)
{
	return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static char const *stripped(char const *name)
{
	char const *last = strrchr(name, '\\');

	return last ? last + 1 : name;
}

static unsigned hash(char const *name)
{
	unsigned value = 2166136261u;

	for (; *name; name++)
		value = (value ^ (unsigned)lower((unsigned char)*name)) * 16777619u;
	return value;
}

static int same(char const *a, char const *b)
{
	while (*a && lower((unsigned char)*a) == lower((unsigned char)*b))
	{
		a++;
		b++;
	}
	return !*a && !*b;
}

/* the slot holding name, or the empty one it would go in */
static int find(char const *name)
{
	unsigned at = hash(name) % SLOTS;

	while (slots[at] && !same(names[slots[at] - 1], name))
		at = (at + 1) % SLOTS;
	return (int)at;
}

void ae_downloaded_maps_reset(void)
{
	memset(slots, 0, sizeof(slots));
	count = 0;
	overflowed = 0;
}

int ae_downloaded_maps_add(char const *name)
{
	char const *base;
	int at;

	if (!name || !*name)
		return 1;
	base = stripped(name);
	if (!*base || strlen(base) >= AE_DOWNLOADED_MAPS_NAME)
		return 1;
	at = find(base);
	if (slots[at])
		return 1;
	if (count >= AE_DOWNLOADED_MAPS_MAXIMUM)
	{
		overflowed = 1;
		return 0;
	}
	strcpy(names[count], base);
	slots[at] = ++count;
	return 1;
}

int ae_downloaded_maps_overflowed(void)
{
	return overflowed;
}

int ae_downloaded_maps_has(char const *map_name)
{
	if (!map_name || !*map_name)
		return 0;
	if (overflowed)
		return 1;
	return slots[find(stripped(map_name))] != 0;
}

int ae_downloaded_maps_count(void)
{
	return count;
}

void ae_downloaded_maps_reader_start(struct ae_downloaded_maps_reader *reader)
{
	reader->length = 0;
	reader->skipping = 0;
}

static void line_done(struct ae_downloaded_maps_reader *reader)
{
	if (reader->length && !reader->skipping)
	{
		reader->line[reader->length] = 0;
		ae_downloaded_maps_add(reader->line);
	}
	reader->length = 0;
	reader->skipping = 0;
}

void ae_downloaded_maps_reader_feed(struct ae_downloaded_maps_reader *reader, char const *text, long size)
{
	long i;

	for (i = 0; i < size; i++)
	{
		if (text[i] == '\n' || text[i] == '\r')
			line_done(reader);
		else if (reader->length >= AE_DOWNLOADED_MAPS_NAME - 1)
			reader->skipping = 1;
		else
			reader->line[reader->length++] = text[i];
	}
}

void ae_downloaded_maps_reader_end(struct ae_downloaded_maps_reader *reader)
{
	line_done(reader);
}

int ae_downloaded_maps_file_name(char const *path, char *out, int size)
{
	char const *base;
	char const *slash;
	size_t length;

	if (!path || !*path || size < 8)
		return 0;
	base = path;
	for (slash = path; *slash; slash++)
	{
		if (*slash == '\\' || *slash == '/')
			base = slash + 1;
	}
	length = strlen(base);
	if (length > 4 && base[length - 4] == '.' && lower((unsigned char)base[length - 3]) == 'm' &&
		lower((unsigned char)base[length - 2]) == 'a' && lower((unsigned char)base[length - 1]) == 'p')
		length -= 4;
	if (!length || length + 4 > (size_t)size || length + 4 > AE_DOWNLOADED_MAPS_NAME)
		return 0;
	memcpy(out, base, length);
	strcpy(out + length, "@ce");
	return 1;
}

int ae_downloaded_maps_has_either(char const *requested, char const *opened_path)
{
	char name[AE_DOWNLOADED_MAPS_NAME];

	if (ae_downloaded_maps_has(requested))
		return 1;
	return ae_downloaded_maps_file_name(opened_path, name, (int)sizeof(name)) && ae_downloaded_maps_has(name);
}

int ae_downloaded_maps_alias_name(char const *file)
{
	return file && strchr(file, '~') != NULL;
}
