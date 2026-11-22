/*
AE_CE_ARCHIVE_RULES.C

The community map archive's pure rules (ae_ce_archive_rules.h).

The list's format, its signature's check and its lines' fields are from the Android host's fractum_archive.c
(thelinkin3000), as Halo Content Evolved's port/linux/src/map_archive.c (Watanoge) brought them to the desktop;
both AGPL-3.0-or-later. The bounds, the name rules and the address and answer rules are this port's.
*/

#include "ae_ce_archive_rules.h"

#include "monocypher.h"
#include "monocypher-ed25519.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the list's first line, and what its signature covers before its bytes */
#define LIST_FORMAT "# halo map torrents v1"
#define LIST_CONTEXT "fractum-halo-map-index 1\n"

enum
{
	FIELDS_MAXIMUM = 12,
	HEADER_VERSION_OFFSET = 0x4,
	HEADER_FILE_LENGTH_OFFSET = 0x8,
	HEADER_TAG_DATA_OFFSET = 0x10,
	HEADER_TAG_DATA_SIZE_OFFSET = 0x14,
	HEADER_NAME_OFFSET = 0x20,
	HEADER_NAME_SIZE = 0x20,
	HEADER_FOOTER_OFFSET = 0x7fc,
	HEADER_SIGNATURE = 0x68656164,
	FOOTER_SIGNATURE = 0x666f6f74,
	CUSTOM_EDITION_VERSION = 609
};

static void refuse(char *error, int error_size, char const *format, ...) __attribute__((format(printf, 3, 4)));

static void refuse(char *error, int error_size, char const *format, ...)
{
	va_list arguments;

	if (!error || error_size <= 0)
		return;
	va_start(arguments, format);
	vsnprintf(error, (size_t)error_size, format, arguments);
	va_end(arguments);
}

static int lower(int c)
{
	return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static int hex_value(int c)
{
	return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 :
		-1;
}

int ae_ce_archive_hex(char const *text, size_t length, uint8_t *bytes, size_t count)
{
	size_t i;

	if (length != count * 2)
		return 0;
	for (i = 0; i < count; i++)
	{
		int high = hex_value((unsigned char)text[2 * i]);
		int low = hex_value((unsigned char)text[2 * i + 1]);

		if (high < 0 || low < 0)
			return 0;
		bytes[i] = (uint8_t)(high * 16 + low);
	}
	return 1;
}

/* ---------- names */

static int reserved_stem(char const *stem, size_t length)
{
	static char const *const plain[] = { "con", "prn", "aux", "nul" };
	size_t i;

	if (length == 3)
	{
		for (i = 0; i < sizeof(plain) / sizeof(plain[0]); i++)
		{
			if (lower((unsigned char)stem[0]) == plain[i][0] && lower((unsigned char)stem[1]) == plain[i][1] &&
				lower((unsigned char)stem[2]) == plain[i][2])
				return 1;
		}
	}
	if (length == 4 && stem[3] >= '0' && stem[3] <= '9')
	{
		if ((lower((unsigned char)stem[0]) == 'c' && lower((unsigned char)stem[1]) == 'o' &&
			lower((unsigned char)stem[2]) == 'm') ||
			(lower((unsigned char)stem[0]) == 'l' && lower((unsigned char)stem[1]) == 'p' &&
			lower((unsigned char)stem[2]) == 't'))
			return 1;
	}
	return 0;
}

int ae_ce_archive_file_name_ok(char const *file, char *installed)
{
	size_t length, stem, i;

	if (!file)
		return 0;
	length = strlen(file);
	if (length < 5 || length > AE_CE_ARCHIVE_STEM_MAXIMUM + 4)
		return 0;
	stem = length - 4;
	if (file[stem] != '.' || lower((unsigned char)file[stem + 1]) != 'm' || lower((unsigned char)file[stem + 2]) != 'a' ||
		lower((unsigned char)file[stem + 3]) != 'p')
		return 0;
	for (i = 0; i < stem; i++)
	{
		unsigned char c = (unsigned char)file[i];

		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
			return 0;
	}
	if (reserved_stem(file, stem))
		return 0;
	/* (<base>_<8 hex digits> is the form a map sharing's name for a version takes: a map of that name could be given back its
	base's name later, which no mark holds) */
	if (stem >= 10 && file[stem - 9] == '_')
	{
		size_t digit;
		int hex = 1;

		for (digit = stem - 8; digit < stem; digit++)
			hex = hex && hex_value((unsigned char)file[digit]) >= 0;
		if (hex)
			return 0;
	}
	if (installed)
	{
		for (i = 0; i < stem; i++)
			installed[i] = (char)lower((unsigned char)file[i]);
		memcpy(installed + stem, ".map", 5);
	}
	return 1;
}

/* ---------- the signature */

int ae_ce_archive_list_signed(unsigned char const *list, size_t list_size, char const *signature_text,
	size_t signature_size, uint8_t const (*keys)[32], int key_count)
{
	uint8_t signature[64];
	size_t start = 0, end = signature_size, context = strlen(LIST_CONTEXT);
	uint8_t *message;
	int i, good = 0;

	if (!list || !signature_text || !keys || key_count <= 0 || list_size > AE_CE_ARCHIVE_LIST_MAXIMUM ||
		signature_size > AE_CE_ARCHIVE_SIGNATURE_MAXIMUM)
		return 0;
	while (start < end && (signature_text[start] == ' ' || signature_text[start] == '\n' ||
		signature_text[start] == '\r' || signature_text[start] == '\t'))
		start++;
	while (end > start && (signature_text[end - 1] == ' ' || signature_text[end - 1] == '\n' ||
		signature_text[end - 1] == '\r' || signature_text[end - 1] == '\t'))
		end--;
	if (!ae_ce_archive_hex(signature_text + start, end - start, signature, sizeof(signature)))
		return 0;
	message = (uint8_t *)malloc(context + list_size);
	if (!message)
		return 0;
	memcpy(message, LIST_CONTEXT, context);
	if (list_size)
		memcpy(message + context, list, list_size);
	for (i = 0; i < key_count && !good; i++)
		good = crypto_ed25519_check(signature, keys[i], message, context + list_size) == 0;
	free(message);
	return good;
}

/* ---------- the list */

/* a line's tab-separated fields, up to count: how many */
static int line_fields(unsigned char const *text, size_t start, size_t end, char const **fields, size_t *lengths,
	int count)
{
	int field = 0;

	while (field < count && start <= end)
	{
		size_t stop = start;

		while (stop < end && text[stop] != '\t')
			stop++;
		fields[field] = (char const *)text + start;
		lengths[field] = stop - start;
		field++;
		start = stop + 1;
	}
	return field;
}

/* the list's "# serial" number into *serial (0 when there is none): 1, or 0 when it is not a number of at most
AE_CE_ARCHIVE_SERIAL_DIGITS digits within AE_CE_ARCHIVE_SERIAL_MAXIMUM */
static int list_serial(unsigned char const *list, size_t size, unsigned long long *serial)
{
	static char const key[] = "# serial ";
	size_t at = 0;

	*serial = 0;
	while (at < size)
	{
		size_t end = at;

		while (end < size && list[end] != '\n')
			end++;
		if (list[at] != '#')
			break;
		if (end - at > sizeof(key) - 1 && !memcmp(list + at, key, sizeof(key) - 1))
		{
			unsigned long long value = 0;
			size_t i = at + sizeof(key) - 1;
			int digits = 0;

			for (; i < end && list[i] >= '0' && list[i] <= '9'; i++, digits++)
			{
				if (digits >= AE_CE_ARCHIVE_SERIAL_DIGITS)
					return 0;
				value = value * 10 + (unsigned long long)(list[i] - '0');
			}
			if (!digits || value > AE_CE_ARCHIVE_SERIAL_MAXIMUM)
				return 0;
			*serial = value;
			return 1;
		}
		at = end + 1;
	}
	return 1;
}

/* the names taken so far, by hash (open addressing: an entry's index plus one) */
static unsigned name_hash(char const *name)
{
	unsigned value = 2166136261u;

	for (; *name; name++)
		value = (value ^ (unsigned char)*name) * 16777619u;
	return value;
}

enum { NAME_SLOTS = 65536 };

static int has_name(struct ae_ce_archive_entry const *entries, int const *slots, char const *file)
{
	unsigned at = name_hash(file) % NAME_SLOTS;

	while (slots[at])
	{
		if (!strcmp(entries[slots[at] - 1].file, file))
			return 1;
		at = (at + 1) % NAME_SLOTS;
	}
	return 0;
}

static void add_name(int *slots, char const *file, int index)
{
	unsigned at = name_hash(file) % NAME_SLOTS;

	while (slots[at])
		at = (at + 1) % NAME_SLOTS;
	slots[at] = index + 1;
}

/* a decimal of at most 18 digits */
static int decimal(char const *text, size_t length, unsigned long long *value)
{
	size_t i;

	if (!length || length > 18)
		return 0;
	*value = 0;
	for (i = 0; i < length; i++)
	{
		if (text[i] < '0' || text[i] > '9')
			return 0;
		*value = *value * 10 + (unsigned long long)(text[i] - '0');
	}
	return 1;
}

static int list_parse_with(unsigned char const *list, size_t size, struct ae_ce_archive_entry *out, int out_size,
	int *count, int *skipped, unsigned long long *serial, char *error, int error_size, int *slots)
{
	size_t at = 0;
	int lines = 0, kept = 0, left_out = 0;
	unsigned long long number = 0;

	if (count)
		*count = 0;
	if (skipped)
		*skipped = 0;
	if (serial)
		*serial = 0;
	if (!list || !out || out_size <= 0)
		return 0;
	if (size > AE_CE_ARCHIVE_LIST_MAXIMUM)
	{
		refuse(error, error_size, "the list is larger than %d bytes", AE_CE_ARCHIVE_LIST_MAXIMUM);
		return 0;
	}
	if (size < sizeof(LIST_FORMAT) - 1 || memcmp(list, LIST_FORMAT, sizeof(LIST_FORMAT) - 1) ||
		(size > sizeof(LIST_FORMAT) - 1 && list[sizeof(LIST_FORMAT) - 1] != '\n' && list[sizeof(LIST_FORMAT) - 1] != '\r'))
	{
		refuse(error, error_size, "it is not a list of maps");
		return 0;
	}
	if (memchr(list, 0, size))
	{
		refuse(error, error_size, "the list holds a NUL byte");
		return 0;
	}
	if (!list_serial(list, size, &number))
	{
		refuse(error, error_size, "the list's serial is not a number up to %llu", (unsigned long long)AE_CE_ARCHIVE_SERIAL_MAXIMUM);
		return 0;
	}
	while (at < size)
	{
		char const *fields[FIELDS_MAXIMUM];
		size_t lengths[FIELDS_MAXIMUM];
		size_t end = at, start = at;
		struct ae_ce_archive_entry entry;
		unsigned long long value;
		int field_count, shared;

		while (end < size && list[end] != '\n')
			end++;
		if (end - at > AE_CE_ARCHIVE_LINE_MAXIMUM)
		{
			refuse(error, error_size, "a line of the list is longer than %d bytes", AE_CE_ARCHIVE_LINE_MAXIMUM);
			return 0;
		}
		at = end + 1;
		if (end > start && list[end - 1] == '\r')
			end--;
		if (++lines > AE_CE_ARCHIVE_LINES_MAXIMUM)
		{
			refuse(error, error_size, "the list has more than %d lines", AE_CE_ARCHIVE_LINES_MAXIMUM);
			return 0;
		}
		if (end == start || list[start] == '#')
			continue;
		if (kept >= AE_CE_ARCHIVE_ENTRIES_MAXIMUM || kept >= out_size)
		{
			refuse(error, error_size, "the list names more than %d maps", kept);
			return 0;
		}
		field_count = line_fields(list, start, end, fields, lengths, FIELDS_MAXIMUM);
		if (field_count < 6 || lengths[0] == 0 || lengths[0] >= AE_CE_ARCHIVE_FILE_SIZE || lengths[1] != 8 ||
			lengths[5] != 8 + 64 || memcmp(fields[5], "blake2b:", 8))
		{
			left_out++;
			continue;
		}
		memset(&entry, 0, sizeof(entry));
		memcpy(entry.listed, fields[0], lengths[0]);
		if (!ae_ce_archive_file_name_ok(entry.listed, entry.file))
		{
			left_out++;
			continue;
		}
		if (!decimal(fields[2], lengths[2], &value) || !value || value > AE_CE_ARCHIVE_MAP_MAXIMUM)
		{
			left_out++;
			continue;
		}
		entry.size = value;
		{
			int digit, valid = 1;

			for (digit = 0; digit < 8; digit++)
				valid = valid && hex_value((unsigned char)fields[1][digit]) >= 0;
			if (!valid)
			{
				left_out++;
				continue;
			}
			entry.checksum = (unsigned int)strtoul(fields[1], NULL, 16);
		}
		if (!ae_ce_archive_hex(fields[5] + 8, 64, entry.blake2b, 32))
		{
			left_out++;
			continue;
		}
		for (shared = 6; shared < field_count; shared++)
		{
			if (lengths[shared] == 6 + 64 && !memcmp(fields[shared], "share:", 6))
			{
				if (!ae_ce_archive_hex(fields[shared] + 6, 64, entry.share, 32))
					memset(entry.share, 0, sizeof(entry.share));
				break;
			}
		}
		/* (the resource maps' header checksum is 0: there is no map in them to play) */
		if (!entry.checksum || has_name(out, slots, entry.file))
		{
			left_out++;
			continue;
		}
		out[kept] = entry;
		add_name(slots, entry.file, kept);
		kept++;
	}
	if (kept <= 0)
	{
		refuse(error, error_size, "the list names no map");
		return 0;
	}
	if (count)
		*count = kept;
	if (skipped)
		*skipped = left_out;
	if (serial)
		*serial = number;
	return 1;
}

int ae_ce_archive_list_parse(unsigned char const *list, size_t size, struct ae_ce_archive_entry *out, int out_size,
	int *count, int *skipped, unsigned long long *serial, char *error, int error_size)
{
	int *slots = (int *)calloc(NAME_SLOTS, sizeof(int));
	int result;

	if (count)
		*count = 0;
	if (skipped)
		*skipped = 0;
	if (serial)
		*serial = 0;
	if (!slots)
	{
		refuse(error, error_size, "out of memory");
		return 0;
	}
	result = list_parse_with(list, size, out, out_size, count, skipped, serial, error, error_size, slots);
	free(slots);
	return result;
}

/* ---------- addresses and answers */

int ae_ce_archive_host_is_loopback(char const *host)
{
	return host && (!strcmp(host, "127.0.0.1") || !strcmp(host, "localhost"));
}

int ae_ce_archive_url_parse(char const *url, struct ae_ce_archive_url *out)
{
	char const *start, *end, *colon, *at;
	size_t host_length, i;

	if (!url || !out)
		return 0;
	memset(out, 0, sizeof(*out));
	if (!strncmp(url, "https://", 8))
	{
		out->secure = 1;
		start = url + 8;
	}
	else if (!strncmp(url, "http://", 7))
	{
		start = url + 7;
	}
	else
		return 0;
	end = start + strcspn(start, "/?#");
	colon = (char const *)memchr(start, ':', (size_t)(end - start));
	host_length = (size_t)((colon ? colon : end) - start);
	if (!host_length || host_length >= sizeof(out->host))
		return 0;
	for (i = 0; i < host_length; i++)
	{
		unsigned char c = (unsigned char)start[i];

		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-'))
			return 0;
	}
	memcpy(out->host, start, host_length);
	out->port = out->secure ? 443 : 80;
	if (colon)
	{
		long port = 0;
		size_t digits = (size_t)(end - colon - 1);

		if (!digits || digits > 5)
			return 0;
		for (i = 0; i < digits; i++)
		{
			if (colon[1 + i] < '0' || colon[1 + i] > '9')
				return 0;
			port = port * 10 + (colon[1 + i] - '0');
		}
		if (port < 1 || port > 65535)
			return 0;
		out->port = (int)port;
	}
	if (!*end)
	{
		strcpy(out->path, "/");
		return 1;
	}
	if (*end != '/')
		return 0;
	if (strlen(end) >= sizeof(out->path))
		return 0;
	for (at = end; *at; at++)
	{
		if ((unsigned char)*at <= 0x20 || (unsigned char)*at >= 0x7f || *at == '#')
			return 0;
	}
	memcpy(out->path, end, strlen(end) + 1);
	return 1;
}

int ae_ce_archive_map_path(char const *base_path, char const *name, char *path, size_t path_size)
{
	size_t used = strlen(base_path);
	char const *at;

	if (used + 1 >= path_size)
		return 0;
	memcpy(path, base_path, used);
	for (at = name; *at; at++)
	{
		unsigned char c = (unsigned char)*at;

		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
			c == '.' || c == '~')
		{
			if (used + 2 > path_size)
				return 0;
			path[used++] = (char)c;
		}
		else
		{
			if (used + 4 > path_size)
				return 0;
			used += (size_t)snprintf(path + used, path_size - used, "%%%02X", c);
		}
	}
	path[used] = 0;
	return 1;
}

static int header_is(char const *line, size_t length, char const *name)
{
	size_t n = strlen(name), i;

	if (length < n)
		return 0;
	for (i = 0; i < n; i++)
	{
		if (lower((unsigned char)line[i]) != name[i])
			return 0;
	}
	return 1;
}

int ae_ce_archive_head_parse(char const *head, size_t size, struct ae_ce_archive_head *out)
{
	size_t at = 0, line_end;
	int first = 1;

	memset(out, 0, sizeof(*out));
	out->content_length = -1;
	while (at < size)
	{
		char const *line = head + at;
		size_t length;

		line_end = at;
		while (line_end < size && head[line_end] != '\n')
			line_end++;
		length = line_end - at;
		if (length && line[length - 1] == '\r')
			length--;
		at = line_end + 1;
		if (first)
		{
			int major, minor, status;

			first = 0;
			if (length < 12 || length > 255)
				return 0;
			{
				char text[256];

				memcpy(text, line, length);
				text[length] = 0;
				if (sscanf(text, "HTTP/%d.%d %d", &major, &minor, &status) != 3 || major != 1 || status < 100 ||
					status > 599)
					return 0;
			}
			out->status = status;
			continue;
		}
		if (header_is(line, length, "content-length:"))
		{
			size_t i = 15;
			long long value = 0;
			int digits = 0;

			while (i < length && (line[i] == ' ' || line[i] == '\t'))
				i++;
			for (; i < length && line[i] >= '0' && line[i] <= '9' && digits < 18; i++, digits++)
				value = value * 10 + (line[i] - '0');
			while (i < length && (line[i] == ' ' || line[i] == '\t'))
				i++;
			if (!digits || i != length)
				out->content_length = -2;
			else if (out->content_length == -1)
				out->content_length = value;
			else if (out->content_length != value)
				out->content_length = -2;
		}
		else if (header_is(line, length, "transfer-encoding:"))
			out->chunked = 1;
		else if (header_is(line, length, "content-encoding:"))
		{
			size_t i = 17;

			while (i < length && (line[i] == ' ' || line[i] == '\t'))
				i++;
			if (i < length && !(length - i == 8 && header_is(line + i, length - i, "identity")))
				out->encoded = 1;
		}
		else if (header_is(line, length, "location:"))
			out->has_location = 1;
	}
	return !first;
}

int ae_ce_archive_head_allows(struct ae_ce_archive_head const *head, unsigned long long size, char *error,
	int error_size)
{
	if (head->status >= 300 && head->status < 400)
	{
		refuse(error, error_size, "the archive sent the request on to another address (not followed)");
		return 0;
	}
	if (head->status == 404)
	{
		refuse(error, error_size, "the archive does not have it");
		return 0;
	}
	if (head->status != 200)
	{
		refuse(error, error_size, "the archive answered %d", head->status);
		return 0;
	}
	if (head->chunked || head->encoded)
	{
		refuse(error, error_size, "the archive's answer is not the file's own bytes");
		return 0;
	}
	if (head->content_length < 0)
	{
		refuse(error, error_size, "the archive did not say how large the file is");
		return 0;
	}
	if ((unsigned long long)head->content_length != size)
	{
		refuse(error, error_size, "the archive's file is not the list's size");
		return 0;
	}
	return 1;
}

/* ---------- disk and header */

int ae_ce_archive_disk_allows(unsigned long long free_bytes, unsigned long long size)
{
	return free_bytes >= AE_CE_ARCHIVE_DISK_MARGIN && size <= free_bytes - AE_CE_ARCHIVE_DISK_MARGIN;
}

static unsigned int read_32(unsigned char const *bytes)
{
	return (unsigned int)bytes[0] | (unsigned int)bytes[1] << 8 | (unsigned int)bytes[2] << 16 |
		(unsigned int)bytes[3] << 24;
}

int ae_ce_archive_header_ok(unsigned char const *header, unsigned long long file_size, char *error, int error_size)
{
	unsigned int file_length, tag_offset, tag_size;

	if (file_size < AE_CE_ARCHIVE_HEADER_BYTES || file_size > AE_CE_ARCHIVE_MAP_MAXIMUM)
	{
		refuse(error, error_size, "its size is not a map's");
		return 0;
	}
	if (read_32(header) != HEADER_SIGNATURE || read_32(header + HEADER_FOOTER_OFFSET) != FOOTER_SIGNATURE)
	{
		refuse(error, error_size, "it is not a cache file");
		return 0;
	}
	if (read_32(header + HEADER_VERSION_OFFSET) != CUSTOM_EDITION_VERSION)
	{
		refuse(error, error_size, "it is a cache file of version %u, not Custom Edition's %d",
			read_32(header + HEADER_VERSION_OFFSET), CUSTOM_EDITION_VERSION);
		return 0;
	}
	file_length = read_32(header + HEADER_FILE_LENGTH_OFFSET);
	tag_offset = read_32(header + HEADER_TAG_DATA_OFFSET);
	tag_size = read_32(header + HEADER_TAG_DATA_SIZE_OFFSET);
	/* (0 from some maps' tools: the loader takes the file's own) */
	if (file_length && (file_length < AE_CE_ARCHIVE_HEADER_BYTES || file_length > file_size))
	{
		refuse(error, error_size, "its header says %u bytes, the file has %llu", file_length, file_size);
		return 0;
	}
	if (tag_offset < AE_CE_ARCHIVE_HEADER_BYTES || tag_offset > file_size || tag_size > file_size - tag_offset)
	{
		refuse(error, error_size, "its tag data lies outside the file");
		return 0;
	}
	if (!memchr(header + HEADER_NAME_OFFSET, 0, HEADER_NAME_SIZE))
	{
		refuse(error, error_size, "its name does not end within its header");
		return 0;
	}
	return 1;
}

/* ---------- search, sort, paging */

static int contains(char const *text, char const *part)
{
	size_t length = strlen(part);

	if (!length)
		return 1;
	for (; *text; text++)
	{
		size_t i = 0;

		while (i < length && text[i] && lower((unsigned char)text[i]) == lower((unsigned char)part[i]))
			i++;
		if (i == length)
			return 1;
	}
	return 0;
}

/* (the sort's own, qsort having no context: a view is built under the library's lock) */
static struct ae_ce_archive_entry const *sort_entries;
static int sort_by;
static int sort_down;

static int compare_names(char const *a, char const *b)
{
	while (*a && lower((unsigned char)*a) == lower((unsigned char)*b))
	{
		a++;
		b++;
	}
	return lower((unsigned char)*a) - lower((unsigned char)*b);
}

static int compare_view(void const *a, void const *b)
{
	struct ae_ce_archive_entry const *x = sort_entries + *(int const *)a;
	struct ae_ce_archive_entry const *y = sort_entries + *(int const *)b;
	int order = 0;

	if (sort_by == AE_CE_ARCHIVE_SORT_SIZE && x->size != y->size)
		order = x->size < y->size ? -1 : 1;
	if (!order)
		order = compare_names(x->file, y->file);
	if (!order)
		order = *(int const *)a - *(int const *)b;
	return sort_down ? -order : order;
}

int ae_ce_archive_view_build(struct ae_ce_archive_entry const *entries, int count, struct ae_ce_archive_query const *query,
	int *view)
{
	int i, shown = 0;

	for (i = 0; i < count; i++)
	{
		if (query->show == AE_CE_ARCHIVE_SHOW_INSTALLED && !entries[i].installed)
			continue;
		if (query->show == AE_CE_ARCHIVE_SHOW_MISSING && entries[i].installed)
			continue;
		if (!contains(entries[i].file, query->text))
			continue;
		view[shown++] = i;
	}
	sort_entries = entries;
	sort_by = query->sort;
	sort_down = query->descending;
	qsort(view, (size_t)shown, sizeof(int), compare_view);
	return shown;
}

int ae_ce_archive_page_first(int view_count, int per_page, int *page, int *pages)
{
	int total = per_page > 0 && view_count > 0 ? (view_count + per_page - 1) / per_page : 1;

	if (*page >= total)
		*page = total - 1;
	if (*page < 0)
		*page = 0;
	if (pages)
		*pages = total;
	return per_page > 0 ? *page * per_page : 0;
}

/* ---------- the highest serial seen */

int ae_ce_archive_address_public(unsigned char const *octets)
{
	unsigned a = octets[0], b = octets[1];

	if (a == 0 || a == 10 || a == 127 || a >= 224)
		return 0;
	if (a == 169 && b == 254)
		return 0;
	if (a == 172 && b >= 16 && b <= 31)
		return 0;
	if (a == 192 && b == 168)
		return 0;
	if (a == 100 && b >= 64 && b <= 127)
		return 0;
	return 1;
}

int ae_ce_archive_serial_in_time(unsigned long long serial, unsigned long long now)
{
	/* (a clock before 2020 is not set: nothing is in time then, a list with no serial excepted) */
	if (!serial)
		return 1;
	return now >= 1577836800ULL && serial <= now + (unsigned long long)AE_CE_ARCHIVE_SERIAL_DAYS_AHEAD * 86400ULL;
}

int ae_ce_archive_record_parse(char const *text, unsigned long long *serial, uint8_t *hash)
{
	unsigned long long value = 0;
	int digits = 0;
	char const *at = text;

	if (strncmp(at, "serial ", 7))
		return 0;
	at += 7;
	for (; *at >= '0' && *at <= '9'; at++, digits++)
	{
		if (digits >= AE_CE_ARCHIVE_SERIAL_DIGITS)
			return 0;
		value = value * 10 + (unsigned long long)(*at - '0');
	}
	if (!digits || value > AE_CE_ARCHIVE_SERIAL_MAXIMUM || *at != ' ')
		return 0;
	at++;
	/* (the text must be 64 characters long before the hex reads them) */
	{
		int length = 0;

		while (length < 65 && at[length])
			length++;
		if (length < 64)
			return 0;
	}
	if (!ae_ce_archive_hex(at, 64, hash, 32) || (at[64] != 0 && at[64] != '\n' && at[64] != '\r'))
		return 0;
	*serial = value;
	return 1;
}

void ae_ce_archive_record_format(unsigned long long serial, uint8_t const *hash, char *text, size_t size)
{
	int i;
	size_t used = (size_t)snprintf(text, size, "serial %llu ", serial);

	for (i = 0; i < 32 && used + 3 < size; i++)
		used += (size_t)snprintf(text + used, size - used, "%02x", hash[i]);
	if (used + 2 <= size)
		snprintf(text + used, size - used, "\n");
}

int ae_ce_archive_serial_allows(int have_record, unsigned long long record_serial, uint8_t const *record_hash,
	unsigned long long serial, uint8_t const *hash, char *error, int error_size)
{
	if (!have_record)
		return 1;
	if (serial < record_serial)
	{
		refuse(error, error_size, "the list is older than one seen before (serial %llu, not %llu)", serial, record_serial);
		return 0;
	}
	if (serial == record_serial && memcmp(hash, record_hash, 32))
	{
		refuse(error, error_size, "the list has the serial of one seen before but other bytes");
		return 0;
	}
	return 1;
}
