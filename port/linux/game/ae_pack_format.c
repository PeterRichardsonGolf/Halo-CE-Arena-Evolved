/*
AE_PACK_FORMAT.C

Weapon pack files, format v1 (ae_pack_format.h; the format, its layout and
every check: docs/weapon-packs.md). tools/weapon_pack/aepack.py is the same
reader and writer in Python, which tools/test_weapon_pack_format.py holds
this one to: on every pack the two must agree.

The reader trusts nothing it reads. In order:
1. the header and the section table (each field's allowed values);
2. the file's size (at most 64 MiB) and its SHA-256, by reading it once
   through our own streaming SHA-256 (R1: FIPS 180-4, no library call; the
   32 bytes that hold it count as zeros);
3. the sections against the file: inside it, 16-aligned, not overlapping,
   within their kind's limit, whole records;
4. the resident sections read (a zlib one inflated to exactly its inflated
   size, the stream ended, nothing after it);
5. what they hold: every string reference inside STRINGS (which ends with a
   terminator), every tag's bytes inside TAG_BYTES (own tags' in tag order,
   none overlapping another's) and its fixups, parts and resources inside
   their sections, in tag order without overlapping and all of them owned;
   every fixup inside its tag's bytes, by offset, none overlapping another,
   naming a target in them or a pack tag; parts' sizes and indices against
   their vertices, none overlapping another; resources' sizes against the
   limits and RESOURCE_DATA; weapons' tags, texts (the pickup texts' formats)
   and values.
That is the containers only: the structure inside a tag's bytes is the tag
validator's to check once the pack's tags are relocated (ae_pack_format.h).
Every size is checked before anything is allocated or inflated for it, and
every sum against its limit as it grows, so no count in a file makes the
reader allocate or loop past the limits.

The writer (ae_pack_write) lays a pack out the one way the format allows a
writer to (docs/weapon-packs.md "Layout"), every section and resource
stored (R2: the port's zlib inflates only, and other zlibs' deflate output
differs between machines), then checks the image as the reader does before
writing it: it never writes a pack a reader would refuse.

Built into the game and into tools/ae_pack_check.c (ninja linux), and with
AE_PACK_SYSTEM_ZLIB under the sanitizers by the tests.
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AE_PACK_SYSTEM_ZLIB
/* (the tests' sanitized build: the system's zlib) */
#include <zlib.h>
#else
/* the port's zlib (its names z_-prefixed: cache_files_decompress_windows.c) */
#include "../../port/third_party/zlib/zlib_prefixed.h"
#endif

#include "ae_pack_format.h"

/* ---------- constants */

enum
{
	HASH_CHUNK = 64 * 1024,
	SECTION_ENTRY_SIZE = 20,
	/* the sections ae_pack_write writes: every kind, in kind order */
	WRITTEN_SECTIONS = AE_PACK_SECTION_KIND_LIMIT - 1,
};

static unsigned int const sha256_constants[64] =
{
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

/* tag layout 1's vertex sizes by type (rasterizer_geometry.c rasterizer_vertex_type_sizes) */
static unsigned char const vertex_sizes[AE_PACK_VERTEX_TYPES] = { 56, 32, 20, 8, 68, 32, 24, 36, 20, 16, 16, 8 };

static char const *const section_names[AE_PACK_SECTION_KIND_LIMIT] =
{
	"?", "STRINGS", "TAGS", "TAG_BYTES", "FIXUPS", "PARTS", "GEOMETRY", "RESOURCES", "RESOURCE_DATA", "WEAPONS",
	"CREDITS",
};

/* ---------- structures */

/* where a pack is read from: its file, or the image ae_pack_write checks */
struct pack_source
{
	FILE *file;
	unsigned char const *image;
	unsigned int image_size;
};

/* the strings ae_pack_write interns, in the order it meets them */
struct string_table
{
	char *data;
	unsigned int size;
	unsigned int capacity;
	char const **texts;
	unsigned int *offsets;
	unsigned int count;
	unsigned int texts_capacity;
	int failed;
};

/* a growing buffer of ae_pack_write's */
struct buffer
{
	unsigned char *data;
	unsigned int size;
	unsigned int capacity;
	int failed;
};

/* ---------- SHA-256 */

#define ROTATE(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_block(
	unsigned int *state,
	unsigned char const *block)
{
	unsigned int words[64];
	unsigned int a, b, c, d, e, f, g, h;
	int index;

	for (index = 0; index < 16; index++)
	{
		words[index] = ((unsigned int)block[index * 4] << 24) | ((unsigned int)block[index * 4 + 1] << 16) |
			((unsigned int)block[index * 4 + 2] << 8) | (unsigned int)block[index * 4 + 3];
	}
	for (index = 16; index < 64; index++)
	{
		unsigned int s0 = ROTATE(words[index - 15], 7) ^ ROTATE(words[index - 15], 18) ^ (words[index - 15] >> 3);
		unsigned int s1 = ROTATE(words[index - 2], 17) ^ ROTATE(words[index - 2], 19) ^ (words[index - 2] >> 10);

		words[index] = words[index - 16] + s0 + words[index - 7] + s1;
	}
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];
	f = state[5];
	g = state[6];
	h = state[7];
	for (index = 0; index < 64; index++)
	{
		unsigned int t1 = h + (ROTATE(e, 6) ^ ROTATE(e, 11) ^ ROTATE(e, 25)) + ((e & f) ^ (~e & g)) +
			sha256_constants[index] + words[index];
		unsigned int t2 = (ROTATE(a, 2) ^ ROTATE(a, 13) ^ ROTATE(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));

		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
	state[5] += f;
	state[6] += g;
	state[7] += h;
}

void ae_pack_sha256_begin(
	struct ae_pack_sha256 *context)
{
	context->state[0] = 0x6a09e667;
	context->state[1] = 0xbb67ae85;
	context->state[2] = 0x3c6ef372;
	context->state[3] = 0xa54ff53a;
	context->state[4] = 0x510e527f;
	context->state[5] = 0x9b05688c;
	context->state[6] = 0x1f83d9ab;
	context->state[7] = 0x5be0cd19;
	context->length_low = 0;
	context->length_high = 0;
	context->used = 0;
}

void ae_pack_sha256_add(
	struct ae_pack_sha256 *context,
	void const *data,
	unsigned int size)
{
	unsigned char const *bytes = (unsigned char const *)data;

	context->length_low += size;
	if (context->length_low < size)
		context->length_high++;
	while (size)
	{
		unsigned int take;

		if (!context->used && size >= 64)
		{
			sha256_block(context->state, bytes);
			bytes += 64;
			size -= 64;
			continue;
		}
		take = 64 - context->used;
		if (take > size)
			take = size;
		memcpy(context->block + context->used, bytes, take);
		context->used += take;
		bytes += take;
		size -= take;
		if (context->used == 64)
		{
			sha256_block(context->state, context->block);
			context->used = 0;
		}
	}
}

void ae_pack_sha256_end(
	struct ae_pack_sha256 *context,
	unsigned char *digest)
{
	unsigned int bits_high = (context->length_high << 3) | (context->length_low >> 29);
	unsigned int bits_low = context->length_low << 3;
	int index;

	context->block[context->used++] = 0x80;
	if (context->used > 56)
	{
		memset(context->block + context->used, 0, 64 - context->used);
		sha256_block(context->state, context->block);
		context->used = 0;
	}
	memset(context->block + context->used, 0, 56 - context->used);
	for (index = 0; index < 4; index++)
	{
		context->block[56 + index] = (unsigned char)(bits_high >> (24 - index * 8));
		context->block[60 + index] = (unsigned char)(bits_low >> (24 - index * 8));
	}
	sha256_block(context->state, context->block);
	for (index = 0; index < 8; index++)
	{
		digest[index * 4] = (unsigned char)(context->state[index] >> 24);
		digest[index * 4 + 1] = (unsigned char)(context->state[index] >> 16);
		digest[index * 4 + 2] = (unsigned char)(context->state[index] >> 8);
		digest[index * 4 + 3] = (unsigned char)context->state[index];
	}
	context->used = 0;
}

void ae_pack_sha256(
	void const *data,
	unsigned int size,
	unsigned char *digest)
{
	struct ae_pack_sha256 context;

	ae_pack_sha256_begin(&context);
	ae_pack_sha256_add(&context, data, size);
	ae_pack_sha256_end(&context, digest);
}

/* ---------- small things */

#ifdef __GNUC__
static int refuse(struct ae_pack_problem *problem, char const *format, ...) __attribute__((format(printf, 2, 3)));
#endif

/* the reason, in problem (if any): zero */
static int refuse(
	struct ae_pack_problem *problem,
	char const *format,
	...)
{
	if (problem)
	{
		va_list arguments;

		va_start(arguments, format);
		vsnprintf(problem->text, sizeof(problem->text), format, arguments);
		va_end(arguments);
	}
	return 0;
}

char const *ae_pack_section_name(
	unsigned int kind)
{
	return kind < AE_PACK_SECTION_KIND_LIMIT ? section_names[kind] : section_names[0];
}

static unsigned int align(
	unsigned int value)
{
	return (value + AE_PACK_ALIGNMENT - 1) & ~(unsigned int)(AE_PACK_ALIGNMENT - 1);
}

static int printable(
	unsigned int c)
{
	return c >= 0x20 && c <= 0x7E;
}

static int key_character(
	unsigned int c)
{
	return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

/* well-formed UTF-8 as a strict decoder takes it (no overlong forms, surrogates or values past U+10FFFF) */
static int utf8_valid(
	unsigned char const *text,
	unsigned int length)
{
	unsigned int index = 0;

	while (index < length)
	{
		unsigned int c = text[index];
		unsigned int need, low = 0x80, high = 0xBF, k;

		if (c < 0x80)
		{
			index++;
			continue;
		}
		if (c >= 0xC2 && c <= 0xDF)
			need = 1;
		else if (c >= 0xE0 && c <= 0xEF)
			need = 2;
		else if (c >= 0xF0 && c <= 0xF4)
			need = 3;
		else
			return 0;
		if (length - index - 1 < need)
			return 0;
		if (c == 0xE0)
			low = 0xA0;
		else if (c == 0xED)
			high = 0x9F;
		else if (c == 0xF0)
			low = 0x90;
		else if (c == 0xF4)
			high = 0x8F;
		if (text[index + 1] < low || text[index + 1] > high)
			return 0;
		for (k = 2; k <= need; k++)
		{
			if ((text[index + k] & 0xC0) != 0x80)
				return 0;
		}
		index += need + 1;
	}
	return 1;
}

/* a NUL-padded header field: 1..maximum characters (keys': [a-z0-9_]; else printable ASCII), zeros after */
static int padded_field_valid(
	char const *field,
	unsigned int size,
	unsigned int maximum,
	int key)
{
	unsigned int length = 0, index;

	while (length < size && field[length])
		length++;
	if (length < 1 || length > maximum || length >= size)
		return 0;
	for (index = length; index < size; index++)
	{
		if (field[index])
			return 0;
	}
	for (index = 0; index < length; index++)
	{
		unsigned int c = (unsigned char)field[index];

		if (key ? !key_character(c) : !printable(c))
			return 0;
	}
	return 1;
}

/* a zlib stream inflated: exactly output_size bytes, the stream ended, nothing after it */
static int inflate_exact(
	unsigned char const *input,
	unsigned int input_size,
	unsigned char *output,
	unsigned int output_size)
{
	z_stream stream;
	unsigned char spare;
	int status;
	int result;

	memset(&stream, 0, sizeof(stream));
	if (inflateInit(&stream) != Z_OK)
		return 0;
	stream.next_in = (unsigned char *)input;
	stream.avail_in = input_size;
	stream.next_out = output_size ? output : &spare;
	stream.avail_out = output_size;
	status = inflate(&stream, Z_FINISH);
	result = status == Z_STREAM_END && stream.avail_out == 0 && stream.avail_in == 0;
	inflateEnd(&stream);
	return result;
}

static int source_read(
	struct pack_source const *source,
	unsigned int offset,
	void *buffer,
	unsigned int size)
{
	if (!size)
		return 1;
	if (source->image)
	{
		if (offset > source->image_size || size > source->image_size - offset)
			return 0;
		memcpy(buffer, source->image + offset, size);
		return 1;
	}
	if (fseek(source->file, offset, SEEK_SET) != 0)
		return 0;
	return fread(buffer, 1, size, source->file) == size;
}

/* a string of STRINGS at offset (terminated: STRINGS ends with a terminator), or NULL past it */
static char const *string_at(
	struct ae_pack const *pack,
	unsigned int offset)
{
	return offset < pack->strings_size ? pack->strings + offset : NULL;
}

char const *ae_pack_string(
	struct ae_pack const *pack,
	unsigned int offset)
{
	return offset == AE_PACK_NONE ? NULL : string_at(pack, offset);
}

/* a string's length, if it is a valid string reference no longer than maximum (else past maximum) */
static unsigned int string_length(
	struct ae_pack const *pack,
	unsigned int offset,
	unsigned int maximum)
{
	char const *text = string_at(pack, offset);
	unsigned int length = 0;

	if (!text)
		return maximum + 1;
	while (text[length] && length <= maximum)
		length++;
	return length;
}

static int text_valid(
	struct ae_pack const *pack,
	unsigned int offset,
	unsigned int minimum,
	unsigned int maximum,
	int ascii)
{
	unsigned int length = string_length(pack, offset, maximum);
	unsigned char const *text = (unsigned char const *)string_at(pack, offset);
	unsigned int index;

	if (!text || length < minimum || length > maximum)
		return 0;
	if (!ascii)
		return utf8_valid(text, length);
	for (index = 0; index < length; index++)
	{
		if (!printable(text[index]))
			return 0;
	}
	return 1;
}

/* the largest a section of the kind may be, inflated, and its record's size */
static void section_limit(
	unsigned int kind,
	unsigned int *maximum,
	unsigned int *record)
{
	*record = 1;
	switch (kind)
	{
	case AE_PACK_SECTION_STRINGS:
		*maximum = AE_PACK_MAXIMUM_STRINGS;
		break;
	case AE_PACK_SECTION_TAGS:
		*record = sizeof(struct ae_pack_tag_record);
		*maximum = AE_PACK_MAXIMUM_TAGS * *record;
		break;
	case AE_PACK_SECTION_TAG_BYTES:
		*maximum = AE_PACK_MAXIMUM_TAG_BYTES;
		break;
	case AE_PACK_SECTION_FIXUPS:
		*record = sizeof(struct ae_pack_fixup_record);
		*maximum = AE_PACK_MAXIMUM_FIXUPS * *record;
		break;
	case AE_PACK_SECTION_PARTS:
		*record = sizeof(struct ae_pack_part_record);
		*maximum = AE_PACK_MAXIMUM_PARTS * *record;
		break;
	case AE_PACK_SECTION_GEOMETRY:
		*maximum = AE_PACK_MAXIMUM_GEOMETRY;
		break;
	case AE_PACK_SECTION_RESOURCES:
		*record = sizeof(struct ae_pack_resource_record);
		*maximum = AE_PACK_MAXIMUM_RESOURCES * *record;
		break;
	case AE_PACK_SECTION_WEAPONS:
		*record = sizeof(struct ae_pack_weapon_record);
		*maximum = AE_PACK_MAXIMUM_WEAPONS * *record;
		break;
	case AE_PACK_SECTION_CREDITS:
		*maximum = AE_PACK_MAXIMUM_CREDITS;
		break;
	default:
		*maximum = AE_PACK_MAXIMUM_FILE_SIZE;
		break;
	}
}

/* ---------- the reader */

static int header_check(
	struct ae_pack_header const *header,
	struct ae_pack_problem *problem)
{
	unsigned int index;

	if (memcmp(header->magic, AE_PACK_MAGIC, sizeof(header->magic)) != 0)
		return refuse(problem, "not a weapon pack");
	if (header->format_version != AE_PACK_FORMAT_VERSION)
		return refuse(problem, "format version %u (this build reads %u)", header->format_version,
			(unsigned int)AE_PACK_FORMAT_VERSION);
	if (header->header_size != AE_PACK_HEADER_SIZE)
		return refuse(problem, "bad header size %u", header->header_size);
	if (header->flags & ~AE_PACK_KNOWN_HEADER_FLAGS)
		return refuse(problem, "unknown header flags 0x%x", header->flags);
	if (!padded_field_valid(header->pack_id, AE_PACK_ID_SIZE, AE_PACK_MAXIMUM_KEY, 1))
		return refuse(problem, "bad pack id");
	if (header->pack_revision < 1)
		return refuse(problem, "bad revision 0");
	if (!padded_field_valid(header->display_name, AE_PACK_DISPLAY_NAME_SIZE, AE_PACK_DISPLAY_NAME_SIZE - 1, 0))
		return refuse(problem, "bad display name");
	if (header->tag_layout != AE_PACK_TAG_LAYOUT_XBOX)
		return refuse(problem, "unknown tag layout %u", header->tag_layout);
	if (header->section_count < 1 || header->section_count > AE_PACK_MAXIMUM_SECTIONS)
		return refuse(problem, "bad section count %u", header->section_count);
	for (index = 0; index < sizeof(header->reserved); index++)
	{
		if (header->reserved[index])
			return refuse(problem, "reserved header bytes set");
	}
	return 1;
}

/* the file's size and SHA-256 (its own 32 bytes zeroed), read through once */
static int source_hash(
	struct pack_source *source,
	unsigned int *file_size,
	unsigned char *digest,
	struct ae_pack_problem *problem)
{
	struct ae_pack_sha256 context;
	unsigned char *chunk;
	unsigned int position = 0;
	int result = 1;

	ae_pack_sha256_begin(&context);
	if (source->image)
	{
		if (source->image_size > AE_PACK_MAXIMUM_FILE_SIZE)
			return refuse(problem, "file larger than 64 MiB");
		chunk = (unsigned char *)malloc(HASH_CHUNK);
		if (!chunk)
			return refuse(problem, "out of memory");
		while (position < source->image_size)
		{
			unsigned int size = source->image_size - position;
			unsigned int index;

			if (size > HASH_CHUNK)
				size = HASH_CHUNK;
			memcpy(chunk, source->image + position, size);
			for (index = 0; index < size; index++)
			{
				if (position + index >= AE_PACK_SHA256_OFFSET &&
					position + index < AE_PACK_SHA256_OFFSET + AE_PACK_SHA256_SIZE)
				{
					chunk[index] = 0;
				}
			}
			ae_pack_sha256_add(&context, chunk, size);
			position += size;
		}
	}
	else
	{
		if (fseek(source->file, 0, SEEK_SET) != 0)
			return refuse(problem, "cannot read the file");
		chunk = (unsigned char *)malloc(HASH_CHUNK);
		if (!chunk)
			return refuse(problem, "out of memory");
		for (;;)
		{
			unsigned int size = (unsigned int)fread(chunk, 1, HASH_CHUNK, source->file);
			unsigned int index;

			if (!size)
			{
				if (ferror(source->file))
					result = refuse(problem, "cannot read the file");
				break;
			}
			if (size > AE_PACK_MAXIMUM_FILE_SIZE - position)
			{
				result = refuse(problem, "file larger than 64 MiB");
				break;
			}
			for (index = 0; index < size && position + index < AE_PACK_SHA256_OFFSET + AE_PACK_SHA256_SIZE; index++)
			{
				if (position + index >= AE_PACK_SHA256_OFFSET)
					chunk[index] = 0;
			}
			ae_pack_sha256_add(&context, chunk, size);
			position += size;
		}
	}
	free(chunk);
	if (!result)
		return 0;
	ae_pack_sha256_end(&context, digest);
	*file_size = position;
	return 1;
}

static int sections_check(
	struct ae_pack *pack,
	struct ae_pack_problem *problem)
{
	unsigned int count = pack->header.section_count;
	unsigned int table_end = AE_PACK_HEADER_SIZE + count * SECTION_ENTRY_SIZE;
	unsigned int seen = 0;
	unsigned int index, other;

	for (index = 0; index < count; index++)
	{
		struct ae_pack_section const *section = &pack->sections[index];
		unsigned int maximum, record;
		char const *name = ae_pack_section_name(section->kind);

		if (section->kind < 1 || section->kind >= AE_PACK_SECTION_KIND_LIMIT || (seen & (1u << section->kind)))
			return refuse(problem, "unknown or repeated section %u", section->kind);
		seen |= 1u << section->kind;
		if ((section->compression != AE_PACK_STORED && section->compression != AE_PACK_ZLIB) ||
			(section->compression == AE_PACK_ZLIB && section->kind == AE_PACK_SECTION_RESOURCE_DATA))
		{
			return refuse(problem, "%s: bad compression %u", name, section->compression);
		}
		if (section->compression == AE_PACK_STORED && section->inflated_size != section->stored_size)
			return refuse(problem, "%s: stored section sizes differ", name);
		if (section->compression == AE_PACK_ZLIB && section->stored_size < 1)
			return refuse(problem, "%s: empty zlib section", name);
		if (section->offset % AE_PACK_ALIGNMENT || section->offset < table_end)
			return refuse(problem, "%s: misplaced section", name);
		if (section->offset > pack->file_size || section->stored_size > pack->file_size - section->offset)
			return refuse(problem, "%s: section past the end of the file", name);
		section_limit(section->kind, &maximum, &record);
		if (section->inflated_size > maximum)
			return refuse(problem, "%s too large (%u bytes)", name, section->inflated_size);
		if (section->inflated_size % record)
			return refuse(problem, "%s not whole records", name);
	}
	for (index = 0; index < count; index++)
	{
		struct ae_pack_section const *first = &pack->sections[index];

		for (other = index + 1; other < count; other++)
		{
			struct ae_pack_section const *second = &pack->sections[other];

			if (first->stored_size && second->stored_size &&
				first->offset < second->offset + second->stored_size &&
				second->offset < first->offset + first->stored_size)
			{
				return refuse(problem, "overlapping sections %s and %s", ae_pack_section_name(first->kind),
					ae_pack_section_name(second->kind));
			}
		}
	}
	return 1;
}

/* the resident sections read (and inflated); RESOURCE_DATA's place kept */
static int sections_load(
	struct ae_pack *pack,
	struct pack_source *source,
	struct ae_pack_problem *problem)
{
	unsigned int kind, index;

	for (kind = 1; kind < AE_PACK_SECTION_KIND_LIMIT; kind++)
	{
		struct ae_pack_section const *section = NULL;
		unsigned int size;

		for (index = 0; index < pack->header.section_count; index++)
		{
			if (pack->sections[index].kind == kind)
				section = &pack->sections[index];
		}
		if (kind == AE_PACK_SECTION_RESOURCE_DATA)
		{
			if (section)
			{
				pack->resource_data_offset = section->offset;
				pack->resource_data_size = section->stored_size;
			}
			continue;
		}
		size = section ? section->inflated_size : 0;
		/* (one more, zeroed: a terminator for CREDITS, and never an empty allocation) */
		pack->memory[kind] = (unsigned char *)calloc(1, size + 1);
		if (!pack->memory[kind])
			return refuse(problem, "out of memory");
		if (!section || !section->stored_size)
			continue;
		if (section->compression == AE_PACK_STORED)
		{
			if (!source_read(source, section->offset, pack->memory[kind], size))
				return refuse(problem, "%s: cannot read the section", ae_pack_section_name(kind));
		}
		else
		{
			unsigned char *stored = (unsigned char *)malloc(section->stored_size);
			int inflated;

			if (!stored)
				return refuse(problem, "out of memory");
			if (!source_read(source, section->offset, stored, section->stored_size))
			{
				free(stored);
				return refuse(problem, "%s: cannot read the section", ae_pack_section_name(kind));
			}
			inflated = inflate_exact(stored, section->stored_size, pack->memory[kind], size);
			free(stored);
			if (!inflated)
				return refuse(problem, "%s: inflated size lie or bad zlib stream", ae_pack_section_name(kind));
		}
	}
	pack->strings = (char const *)pack->memory[AE_PACK_SECTION_STRINGS];
	pack->tags = (struct ae_pack_tag_record const *)pack->memory[AE_PACK_SECTION_TAGS];
	pack->tag_bytes = pack->memory[AE_PACK_SECTION_TAG_BYTES];
	pack->fixups = (struct ae_pack_fixup_record const *)pack->memory[AE_PACK_SECTION_FIXUPS];
	pack->parts = (struct ae_pack_part_record const *)pack->memory[AE_PACK_SECTION_PARTS];
	pack->geometry = pack->memory[AE_PACK_SECTION_GEOMETRY];
	pack->resources = (struct ae_pack_resource_record const *)pack->memory[AE_PACK_SECTION_RESOURCES];
	pack->weapons = (struct ae_pack_weapon_record const *)pack->memory[AE_PACK_SECTION_WEAPONS];
	pack->credits = (char const *)pack->memory[AE_PACK_SECTION_CREDITS];
	for (index = 0; index < pack->header.section_count; index++)
	{
		struct ae_pack_section const *section = &pack->sections[index];

		switch (section->kind)
		{
		case AE_PACK_SECTION_STRINGS:
			pack->strings_size = section->inflated_size;
			break;
		case AE_PACK_SECTION_TAGS:
			pack->tag_count = section->inflated_size / sizeof(struct ae_pack_tag_record);
			break;
		case AE_PACK_SECTION_TAG_BYTES:
			pack->tag_bytes_size = section->inflated_size;
			break;
		case AE_PACK_SECTION_FIXUPS:
			pack->fixup_count = section->inflated_size / sizeof(struct ae_pack_fixup_record);
			break;
		case AE_PACK_SECTION_PARTS:
			pack->part_count = section->inflated_size / sizeof(struct ae_pack_part_record);
			break;
		case AE_PACK_SECTION_GEOMETRY:
			pack->geometry_size = section->inflated_size;
			break;
		case AE_PACK_SECTION_RESOURCES:
			pack->resource_count = section->inflated_size / sizeof(struct ae_pack_resource_record);
			break;
		case AE_PACK_SECTION_WEAPONS:
			pack->weapon_count = section->inflated_size / sizeof(struct ae_pack_weapon_record);
			break;
		case AE_PACK_SECTION_CREDITS:
			pack->credits_size = section->inflated_size;
			break;
		default:
			break;
		}
	}
	return 1;
}

/* a tag's records of one kind: inside their section, after the previous tag's; counted */
static int records_check(
	unsigned int first,
	unsigned int count,
	unsigned int total,
	unsigned int *cursor,
	unsigned int *sum)
{
	if (first > total || count > total - first)
		return 0;
	if (count)
	{
		if (first < *cursor)
			return 0;
		*cursor = first + count;
	}
	*sum += count;
	return 1;
}

static int tags_check(
	struct ae_pack *pack,
	struct ae_pack_problem *problem)
{
	unsigned int cursors[3] = { 0, 0, 0 };
	unsigned int sums[3] = { 0, 0, 0 };
	unsigned int bytes_cursor = 0;
	unsigned int index, other;

	if (pack->tag_count > AE_PACK_MAXIMUM_TAGS)
		return refuse(problem, "too many tags");
	pack->own_tag_count = 0;
	for (index = 0; index < pack->tag_count; index++)
	{
		struct ae_pack_tag_record const *tag = &pack->tags[index];

		if (!string_at(pack, tag->name))
			return refuse(problem, "tag %u: name past STRINGS", index);
		if (!text_valid(pack, tag->name, 1, AE_PACK_MAXIMUM_NAME, 1))
			return refuse(problem, "tag %u: bad name", index);
		if (!string_at(pack, tag->source_path))
			return refuse(problem, "tag %u: source path past STRINGS", index);
		if (!text_valid(pack, tag->source_path, 0, AE_PACK_MAXIMUM_NAME, 0))
			return refuse(problem, "tag %u: bad source path", index);
		if (!tag->group || tag->group == AE_PACK_NONE)
			return refuse(problem, "tag %u: bad group", index);
		if ((tag->kind != AE_PACK_TAG_OWN && tag->kind != AE_PACK_TAG_EXTERNAL) || tag->pad)
			return refuse(problem, "tag %u: bad kind", index);
		if (tag->flags & ~AE_PACK_TAG_KNOWN_FLAGS)
			return refuse(problem, "tag %u: unknown flag bits 0x%x", index, (unsigned int)tag->flags);
		if (tag->flags && (tag->kind != AE_PACK_TAG_OWN || tag->group != AE_PACK_GROUP_BITMAP))
			return refuse(problem, "tag %u: flags on a tag not an own bitmap", index);
		if (tag->kind == AE_PACK_TAG_OWN)
		{
			pack->own_tag_count++;
			if (tag->required)
				return refuse(problem, "tag %u: required set on an own tag", index);
			if (tag->bytes_size < 1 || tag->bytes_size > AE_PACK_MAXIMUM_TAG_SIZE)
				return refuse(problem, "tag %u: bad size %u", index, tag->bytes_size);
			if (tag->bytes_offset > pack->tag_bytes_size || tag->bytes_size > pack->tag_bytes_size - tag->bytes_offset)
				return refuse(problem, "tag %u: bytes past TAG_BYTES", index);
			/* (own tags' bytes in tag order: no two share or overlap) */
			if (tag->bytes_offset < bytes_cursor)
				return refuse(problem, "tag %u: bytes overlap the previous tag's", index);
			bytes_cursor = tag->bytes_offset + tag->bytes_size;
		}
		else
		{
			if (tag->required > 1)
				return refuse(problem, "tag %u: bad required", index);
			if (tag->bytes_offset || tag->bytes_size || tag->fixups_count || tag->parts_count || tag->resources_count)
				return refuse(problem, "tag %u: an external tag with contents", index);
		}
		if (!records_check(tag->fixups_first, tag->fixups_count, pack->fixup_count, &cursors[0], &sums[0]) ||
			!records_check(tag->parts_first, tag->parts_count, pack->part_count, &cursors[1], &sums[1]) ||
			!records_check(tag->resources_first, tag->resources_count, pack->resource_count, &cursors[2], &sums[2]))
		{
			return refuse(problem, "tag %u: records past their section or out of order", index);
		}
	}
	if (pack->own_tag_count > AE_PACK_MAXIMUM_OWN_TAGS)
		return refuse(problem, "too many own tags (%u)", pack->own_tag_count);
	if (sums[0] != pack->fixup_count || sums[1] != pack->part_count || sums[2] != pack->resource_count)
		return refuse(problem, "records no tag owns");
	for (index = 0; index < pack->tag_count; index++)
	{
		for (other = index + 1; other < pack->tag_count; other++)
		{
			if (pack->tags[index].group == pack->tags[other].group &&
				!strcmp(string_at(pack, pack->tags[index].name), string_at(pack, pack->tags[other].name)))
			{
				return refuse(problem, "tag %u: a second tag of that group and name", other);
			}
		}
	}
	return 1;
}

static int fixups_check(
	struct ae_pack const *pack,
	unsigned int index,
	struct ae_pack_problem *problem)
{
	struct ae_pack_tag_record const *tag = &pack->tags[index];
	unsigned int size = tag->bytes_size;
	unsigned int fixup_index;
	unsigned int end = 0;

	for (fixup_index = 0; fixup_index < tag->fixups_count; fixup_index++)
	{
		struct ae_pack_fixup_record const *fixup = &pack->fixups[tag->fixups_first + fixup_index];
		unsigned int width = fixup->kind == AE_PACK_FIXUP_REFERENCE ? AE_PACK_TAG_REFERENCE_SIZE : 4;

		if (fixup->pad)
			return refuse(problem, "tag %u: bad fixup", index);
		switch (fixup->kind)
		{
		case AE_PACK_FIXUP_POINTER:
			if (size < 4 || fixup->offset > size - 4 || fixup->value >= size)
				return refuse(problem, "tag %u: pointer fixup past its tag", index);
			break;
		case AE_PACK_FIXUP_REFERENCE:
			if (size < AE_PACK_TAG_REFERENCE_SIZE || fixup->offset > size - AE_PACK_TAG_REFERENCE_SIZE)
				return refuse(problem, "tag %u: reference fixup past its tag", index);
			if (fixup->value >= pack->tag_count && fixup->value != AE_PACK_NONE)
				return refuse(problem, "tag %u: reference fixup past the tags", index);
			break;
		case AE_PACK_FIXUP_TAG_INDEX:
			if (size < 4 || fixup->offset > size - 4)
				return refuse(problem, "tag %u: tag index fixup past its tag", index);
			if (fixup->value >= pack->tag_count)
				return refuse(problem, "tag %u: tag index fixup past the tags", index);
			break;
		default:
			return refuse(problem, "tag %u: unknown fixup kind %u", index, (unsigned int)fixup->kind);
		}
		/* (a tag's fixups by offset, none overlapping another: no two write the same bytes) */
		if (fixup_index && fixup->offset < end)
			return refuse(problem, "tag %u: fixups overlap or out of order", index);
		end = fixup->offset + width;
	}
	return 1;
}

static int parts_check(
	struct ae_pack const *pack,
	unsigned int index,
	struct ae_pack_problem *problem)
{
	struct ae_pack_tag_record const *tag = &pack->tags[index];
	unsigned int part_index;

	for (part_index = 0; part_index < tag->parts_count; part_index++)
	{
		struct ae_pack_part_record const *part = &pack->parts[tag->parts_first + part_index];
		unsigned int index_count, k;
		unsigned char const *indices;

		if (tag->bytes_size < AE_PACK_MODEL_PART_SIZE || part->offset > tag->bytes_size - AE_PACK_MODEL_PART_SIZE)
			return refuse(problem, "tag %u: part past its tag", index);
		if (part_index && part->offset < pack->parts[tag->parts_first + part_index - 1].offset + AE_PACK_MODEL_PART_SIZE)
			return refuse(problem, "tag %u: parts overlap or out of order", index);
		if ((part->vertex_type != AE_PACK_VERTEX_MODEL_UNCOMPRESSED &&
			part->vertex_type != AE_PACK_VERTEX_MODEL_COMPRESSED) ||
			(part->triangle_type != AE_PACK_TRIANGLES && part->triangle_type != AE_PACK_TRIANGLE_STRIP))
		{
			return refuse(problem, "tag %u: bad part type", index);
		}
		if (part->vertex_count < 1 || part->vertex_count > AE_PACK_MAXIMUM_PART_COUNT ||
			part->triangle_count < 1 || part->triangle_count > AE_PACK_MAXIMUM_PART_COUNT)
		{
			return refuse(problem, "tag %u: bad part count", index);
		}
		index_count = part->triangle_type == AE_PACK_TRIANGLES ? 3 * part->triangle_count : part->triangle_count + 2;
		if (part->vertices_size != part->vertex_count * vertex_sizes[part->vertex_type] ||
			part->indices_size != index_count * 2)
		{
			return refuse(problem, "tag %u: part sizes", index);
		}
		if (part->vertices_offset > pack->geometry_size ||
			part->vertices_size > pack->geometry_size - part->vertices_offset ||
			part->indices_offset > pack->geometry_size || part->indices_size > pack->geometry_size - part->indices_offset)
		{
			return refuse(problem, "tag %u: part past GEOMETRY", index);
		}
		indices = pack->geometry + part->indices_offset;
		for (k = 0; k < index_count; k++)
		{
			if (((unsigned int)indices[k * 2] | ((unsigned int)indices[k * 2 + 1] << 8)) >= part->vertex_count)
				return refuse(problem, "tag %u: part index past its vertices", index);
		}
	}
	return 1;
}

static int resources_check(
	struct ae_pack *pack,
	unsigned int index,
	struct ae_pack_problem *problem)
{
	struct ae_pack_tag_record const *tag = &pack->tags[index];
	unsigned int resource_index;

	for (resource_index = 0; resource_index < tag->resources_count; resource_index++)
	{
		struct ae_pack_resource_record const *resource = &pack->resources[tag->resources_first + resource_index];

		if (resource->tag != index)
			return refuse(problem, "tag %u: a resource of another tag", index);
		if (resource->size < 1 || resource->size > AE_PACK_MAXIMUM_RESOURCE_SIZE)
			return refuse(problem, "tag %u: resource too large (%u bytes)", index, resource->size);
		if (resource->map_offset > AE_PACK_NONE - resource->size)
			return refuse(problem, "tag %u: bad resource offset", index);
		if (resource->compression == AE_PACK_STORED)
		{
			if (resource->stored_size != resource->size)
				return refuse(problem, "tag %u: stored resource sizes differ", index);
		}
		else if (resource->compression == AE_PACK_ZLIB)
		{
			if (resource->stored_size < 1)
				return refuse(problem, "tag %u: empty zlib resource", index);
		}
		else
		{
			return refuse(problem, "tag %u: bad resource compression", index);
		}
		if (resource->data_offset > pack->resource_data_size ||
			resource->stored_size > pack->resource_data_size - resource->data_offset)
		{
			return refuse(problem, "tag %u: resource past RESOURCE_DATA", index);
		}
		if (resource->size > AE_PACK_MAXIMUM_RESOURCES_SIZE - pack->resources_size)
			return refuse(problem, "resources larger than 48 MiB");
		pack->resources_size += resource->size;
	}
	return 1;
}

/* a pickup text: UTF-8, 1..127 bytes, and a format the engine can fill safely: the weapon's has no '%' at
all; the ammo's exactly one, which is "%d" (no flags, width, precision or other conversion, no "%%") */
static int pickup_text_valid(
	struct ae_pack const *pack,
	unsigned int offset,
	int ammo)
{
	char const *text = string_at(pack, offset);
	unsigned int percents = 0, index;

	if (!text_valid(pack, offset, 1, AE_PACK_MAXIMUM_PICKUP_TEXT, 0))
		return 0;
	for (index = 0; text[index]; index++)
	{
		if (text[index] != '%')
			continue;
		percents++;
		if (text[index + 1] != 'd')
			return 0;
	}
	return ammo ? percents == 1 : percents == 0;
}

static int weapons_check(
	struct ae_pack const *pack,
	struct ae_pack_problem *problem)
{
	unsigned int index, other, which;

	for (index = 0; index < pack->weapon_count; index++)
	{
		struct ae_pack_weapon_record const *weapon = &pack->weapons[index];
		char const *key = string_at(pack, weapon->key);
		unsigned int length, k;

		if (!key)
			return refuse(problem, "weapon %u: string past STRINGS", index);
		length = string_length(pack, weapon->key, AE_PACK_MAXIMUM_KEY);
		if (length < 1 || length > AE_PACK_MAXIMUM_KEY)
			return refuse(problem, "weapon %u: bad key", index);
		for (k = 0; k < length; k++)
		{
			if (!key_character((unsigned char)key[k]))
				return refuse(problem, "weapon %u: bad key", index);
		}
		for (other = 0; other < index; other++)
		{
			if (!strcmp(key, string_at(pack, pack->weapons[other].key)))
				return refuse(problem, "weapon %u: a second weapon of that key", index);
		}
		if (weapon->root_tag >= pack->tag_count || pack->tags[weapon->root_tag].kind != AE_PACK_TAG_OWN ||
			pack->tags[weapon->root_tag].group != AE_PACK_GROUP_WEAPON)
		{
			return refuse(problem, "weapon %u: bad root tag", index);
		}
		if (weapon->item_collection_tag != AE_PACK_NONE && (weapon->item_collection_tag >= pack->tag_count ||
			pack->tags[weapon->item_collection_tag].group != AE_PACK_GROUP_ITEM_COLLECTION))
		{
			return refuse(problem, "weapon %u: bad item collection", index);
		}
		if (weapon->behaviour > AE_PACK_BEHAVIOUR_LAST || weapon->power > 1 || weapon->replaces == 1 ||
			weapon->replaces > AE_PACK_REPLACES_LAST)
		{
			return refuse(problem, "weapon %u: bad behaviour, power or replaces", index);
		}
		if (!text_valid(pack, weapon->label, 1, AE_PACK_MAXIMUM_LABEL, 1))
			return refuse(problem, "weapon %u: bad label", index);
		for (which = 0; which < 2; which++)
		{
			if (weapon->fallback_labels[which] != AE_PACK_NONE &&
				!text_valid(pack, weapon->fallback_labels[which], 1, AE_PACK_MAXIMUM_LABEL, 1))
			{
				return refuse(problem, "weapon %u: bad label", index);
			}
		}
		if (!text_valid(pack, weapon->menu_name, 1, AE_PACK_MAXIMUM_MENU_NAME, 0))
			return refuse(problem, "weapon %u: bad menu name", index);
		if (weapon->weapon_pickup_text != AE_PACK_NONE && !pickup_text_valid(pack, weapon->weapon_pickup_text, 0))
			return refuse(problem, "weapon %u: bad weapon pickup text", index);
		if (weapon->ammo_pickup_text != AE_PACK_NONE && !pickup_text_valid(pack, weapon->ammo_pickup_text, 1))
			return refuse(problem, "weapon %u: bad ammo pickup text", index);
		if (weapon->icon_bitmap_tag == AE_PACK_NONE)
		{
			if (weapon->icon_sequence)
				return refuse(problem, "weapon %u: an icon sequence without an icon", index);
		}
		else if (weapon->icon_bitmap_tag >= pack->tag_count ||
			pack->tags[weapon->icon_bitmap_tag].group != AE_PACK_GROUP_BITMAP ||
			weapon->icon_sequence > AE_PACK_MAXIMUM_ICON_SEQUENCE)
		{
			return refuse(problem, "weapon %u: bad icon", index);
		}
	}
	return 1;
}

static int contents_check(
	struct ae_pack *pack,
	struct ae_pack_problem *problem)
{
	unsigned int index;

	if (pack->strings_size && pack->strings[pack->strings_size - 1])
		return refuse(problem, "STRINGS not terminated");
	if (!pack->tag_count)
		return refuse(problem, "no tags");
	/* (a pack of tags only says so in its header; every other pack has weapons) */
	if (pack->header.flags & AE_PACK_FLAG_NO_WEAPONS)
	{
		if (pack->weapon_count)
			return refuse(problem, "header flags say tags only, but it has weapons");
	}
	else if (!pack->weapon_count)
	{
		return refuse(problem, "no weapons");
	}
	if (!tags_check(pack, problem))
		return 0;
	pack->resources_size = 0;
	for (index = 0; index < pack->tag_count; index++)
	{
		if (pack->tags[index].kind != AE_PACK_TAG_OWN)
			continue;
		if (!fixups_check(pack, index, problem) || !parts_check(pack, index, problem) ||
			!resources_check(pack, index, problem))
		{
			return 0;
		}
	}
	if (!weapons_check(pack, problem))
		return 0;
	if (memchr(pack->credits, 0, pack->credits_size) ||
		!utf8_valid((unsigned char const *)pack->credits, pack->credits_size))
	{
		return refuse(problem, "bad credits");
	}
	return 1;
}

/* the whole pack read from source and checked */
static int pack_load(
	struct ae_pack *pack,
	struct pack_source *source,
	struct ae_pack_problem *problem)
{
	unsigned char digest[AE_PACK_SHA256_SIZE];
	unsigned int index;

	if (!source_read(source, 0, &pack->header, AE_PACK_HEADER_SIZE))
		return refuse(problem, "truncated: no header");
	if (!header_check(&pack->header, problem))
		return 0;
	for (index = 0; index < pack->header.section_count; index++)
	{
		unsigned char entry[SECTION_ENTRY_SIZE];

		if (!source_read(source, AE_PACK_HEADER_SIZE + index * SECTION_ENTRY_SIZE, entry, SECTION_ENTRY_SIZE))
			return refuse(problem, "truncated: no section table");
		memcpy(&pack->sections[index], entry, SECTION_ENTRY_SIZE);
	}
	if (!source_hash(source, &pack->file_size, digest, problem))
		return 0;
	if (!sections_check(pack, problem))
		return 0;
	if (memcmp(digest, pack->header.sha256, AE_PACK_SHA256_SIZE) != 0)
		return refuse(problem, "SHA-256 mismatch");
	if (!sections_load(pack, source, problem))
		return 0;
	return contents_check(pack, problem);
}

int ae_pack_open(
	char const *path,
	struct ae_pack **result,
	struct ae_pack_problem *problem)
{
	struct ae_pack *pack;
	struct pack_source source;

	*result = NULL;
	if (problem)
		problem->text[0] = 0;
	pack = (struct ae_pack *)calloc(1, sizeof(*pack));
	if (!pack)
		return refuse(problem, "out of memory");
	memset(&source, 0, sizeof(source));
	source.file = fopen(path, "rb");
	if (!source.file)
	{
		free(pack);
		return refuse(problem, "cannot open the file");
	}
	pack->file = source.file;
	if (!pack_load(pack, &source, problem))
	{
		ae_pack_close(pack);
		return 0;
	}
	*result = pack;
	return 1;
}

void ae_pack_close(
	struct ae_pack *pack)
{
	unsigned int kind;

	if (!pack)
		return;
	for (kind = 0; kind < AE_PACK_SECTION_KIND_LIMIT; kind++)
		free(pack->memory[kind]);
	if (pack->file)
		fclose((FILE *)pack->file);
	free(pack);
}

/* size bytes of the resource from delta into buffer */
static int resource_read(
	struct ae_pack *pack,
	struct ae_pack_resource_record const *resource,
	unsigned int delta,
	unsigned int size,
	void *buffer)
{
	struct pack_source source;
	unsigned char *stored, *whole;
	int result;

	source.file = (FILE *)pack->file;
	source.image = pack->image;
	source.image_size = pack->file_size;
	if (resource->compression == AE_PACK_STORED)
		return source_read(&source, pack->resource_data_offset + resource->data_offset + delta, buffer, size);
	stored = (unsigned char *)malloc(resource->stored_size);
	if (!stored)
		return 0;
	if (!source_read(&source, pack->resource_data_offset + resource->data_offset, stored, resource->stored_size))
	{
		free(stored);
		return 0;
	}
	if (!delta && size == resource->size)
	{
		result = inflate_exact(stored, resource->stored_size, (unsigned char *)buffer, size);
	}
	else
	{
		whole = (unsigned char *)malloc(resource->size);
		result = whole && inflate_exact(stored, resource->stored_size, whole, resource->size);
		if (result)
			memcpy(buffer, whole + delta, size);
		free(whole);
	}
	free(stored);
	return result;
}

int ae_pack_read_resource(
	struct ae_pack *pack,
	unsigned int tag,
	unsigned int map_offset,
	unsigned int size,
	void *buffer)
{
	struct ae_pack_tag_record const *record;
	unsigned int index;

	if (!pack || !buffer || !size || tag >= pack->tag_count)
		return 0;
	record = &pack->tags[tag];
	for (index = 0; index < record->resources_count; index++)
	{
		struct ae_pack_resource_record const *resource = &pack->resources[record->resources_first + index];
		unsigned int delta;

		if (map_offset < resource->map_offset)
			continue;
		delta = map_offset - resource->map_offset;
		if (delta >= resource->size || size > resource->size - delta)
			continue;
		return resource_read(pack, resource, delta, size, buffer);
	}
	return 0;
}

int ae_pack_verify_resources(
	struct ae_pack *pack,
	struct ae_pack_problem *problem)
{
	unsigned int index;

	for (index = 0; index < pack->resource_count; index++)
	{
		struct ae_pack_resource_record const *resource = &pack->resources[index];
		unsigned char *data = (unsigned char *)malloc(resource->size);
		int result;

		if (!data)
			return refuse(problem, "out of memory");
		result = resource_read(pack, resource, 0, resource->size, data);
		free(data);
		if (!result)
			return refuse(problem, "resource %u of tag %u: inflated size lie or bad zlib stream", index, resource->tag);
	}
	return 1;
}

/* ---------- the writer */

static void buffer_reserve(
	struct buffer *buffer,
	unsigned int size)
{
	unsigned char *data;
	unsigned int capacity;

	if (buffer->failed)
		return;
	if (size > AE_PACK_MAXIMUM_FILE_SIZE - buffer->size)
	{
		buffer->failed = 1;
		return;
	}
	if (buffer->size + size <= buffer->capacity)
		return;
	capacity = buffer->capacity ? buffer->capacity : 256;
	while (capacity < buffer->size + size)
		capacity *= 2;
	data = (unsigned char *)realloc(buffer->data, capacity);
	if (!data)
	{
		buffer->failed = 1;
		return;
	}
	buffer->data = data;
	buffer->capacity = capacity;
}

static void buffer_append(
	struct buffer *buffer,
	void const *data,
	unsigned int size)
{
	buffer_reserve(buffer, size);
	if (buffer->failed || !size)
		return;
	memcpy(buffer->data + buffer->size, data, size);
	buffer->size += size;
}

/* data at the buffer's next aligned offset (zeros before it): that offset */
static unsigned int buffer_place(
	struct buffer *buffer,
	void const *data,
	unsigned int size)
{
	unsigned int offset = align(buffer->size);

	buffer_reserve(buffer, offset - buffer->size);
	if (!buffer->failed && offset > buffer->size)
	{
		memset(buffer->data + buffer->size, 0, offset - buffer->size);
		buffer->size = offset;
	}
	buffer_append(buffer, data, size);
	return offset;
}

static void buffer_u32(
	struct buffer *buffer,
	unsigned int value)
{
	buffer_append(buffer, &value, 4);
}

/* a string's offset in STRINGS (added the first time it is met), or AE_PACK_NONE for NULL */
static unsigned int string_add(
	struct string_table *table,
	char const *text)
{
	unsigned int index, length;

	if (!text || table->failed)
		return AE_PACK_NONE;
	for (index = 0; index < table->count; index++)
	{
		if (!strcmp(table->texts[index], text))
			return table->offsets[index];
	}
	length = (unsigned int)strlen(text) + 1;
	if (length > AE_PACK_MAXIMUM_STRINGS - table->size)
	{
		table->failed = 1;
		return AE_PACK_NONE;
	}
	if (table->count == table->texts_capacity)
	{
		unsigned int capacity = table->texts_capacity ? table->texts_capacity * 2 : 64;
		char const **texts = (char const **)realloc((void *)table->texts, capacity * sizeof(*texts));
		unsigned int *offsets;

		if (!texts)
		{
			table->failed = 1;
			return AE_PACK_NONE;
		}
		table->texts = texts;
		offsets = (unsigned int *)realloc(table->offsets, capacity * sizeof(*offsets));
		if (!offsets)
		{
			table->failed = 1;
			return AE_PACK_NONE;
		}
		table->offsets = offsets;
		table->texts_capacity = capacity;
	}
	if (table->size + length > table->capacity)
	{
		unsigned int capacity = table->capacity ? table->capacity : 1024;
		char *data;

		while (capacity < table->size + length)
			capacity *= 2;
		data = (char *)realloc(table->data, capacity);
		if (!data)
		{
			table->failed = 1;
			return AE_PACK_NONE;
		}
		table->data = data;
		table->capacity = capacity;
	}
	memcpy(table->data + table->size, text, length);
	table->texts[table->count] = text;
	table->offsets[table->count] = table->size;
	table->count++;
	table->size += length;
	return table->size - length;
}

int ae_pack_replace_file(
	char const *temporary,
	char const *path,
	int (*move)(char const *from, char const *to))
{
	unsigned int length;
	char *old;
	int result;

	if (move(temporary, path) == 0)
		return 1;
	/* (where a rename does not replace a file, Windows': the old pack is moved aside, the new one put in place,
	and the old one deleted only then, or put back) */
	length = (unsigned int)strlen(path);
	old = (char *)malloc(length + 5);
	if (!old)
		return 0;
	memcpy(old, path, length);
	memcpy(old + length, ".old", 5);
	remove(old);
	result = 0;
	if (move(path, old) == 0)
	{
		if (move(temporary, path) == 0)
		{
			remove(old);
			result = 1;
		}
		else
		{
			move(old, path);
		}
	}
	free(old);
	return result;
}

/* fixup records in a total order: by offset, then kind, value, pad */
static int fixup_compare(
	void const *a,
	void const *b)
{
	struct ae_pack_fixup_record const *first = (struct ae_pack_fixup_record const *)a;
	struct ae_pack_fixup_record const *second = (struct ae_pack_fixup_record const *)b;

	if (first->offset != second->offset)
		return first->offset < second->offset ? -1 : 1;
	if (first->kind != second->kind)
		return first->kind < second->kind ? -1 : 1;
	if (first->value != second->value)
		return first->value < second->value ? -1 : 1;
	if (first->pad != second->pad)
		return first->pad < second->pad ? -1 : 1;
	return 0;
}

/* a NUL-padded header field from text (cut at size bytes, as the Python writer cuts it) */
static void header_text(
	char *field,
	unsigned int size,
	char const *text)
{
	unsigned int index;

	memset(field, 0, size);
	for (index = 0; text && index < size && text[index]; index++)
		field[index] = text[index];
}

int ae_pack_write(
	char const *path,
	struct ae_pack_build_data const *data,
	struct ae_pack_problem *problem)
{
	struct string_table strings;
	struct buffer sections[AE_PACK_SECTION_KIND_LIMIT];
	struct buffer image;
	struct ae_pack_header header;
	unsigned int fixup_count = 0, part_count = 0, resource_count = 0;
	unsigned int index, kind, k;
	int result = 0;
	int failed = 0;

	if (problem)
		problem->text[0] = 0;
	memset(&strings, 0, sizeof(strings));
	memset(sections, 0, sizeof(sections));
	memset(&image, 0, sizeof(image));
	if (!data || data->tag_count > AE_PACK_MAXIMUM_TAGS || data->weapon_count > AE_PACK_MAXIMUM_WEAPONS ||
		(data->tag_count && !data->tags) || (data->weapon_count && !data->weapons))
	{
		return refuse(problem, "too many tags or weapons");
	}

	/* STRINGS, in the order the Python writer meets them */
	for (index = 0; index < data->tag_count; index++)
	{
		string_add(&strings, data->tags[index].name);
		string_add(&strings, data->tags[index].source_path ? data->tags[index].source_path : "");
	}
	for (index = 0; index < data->weapon_count; index++)
	{
		struct ae_pack_build_weapon const *weapon = &data->weapons[index];

		string_add(&strings, weapon->key);
		string_add(&strings, weapon->label);
		string_add(&strings, weapon->fallback_labels[0]);
		string_add(&strings, weapon->fallback_labels[1]);
		string_add(&strings, weapon->menu_name);
		string_add(&strings, weapon->weapon_pickup_text);
		string_add(&strings, weapon->ammo_pickup_text);
	}
	if (strings.failed)
	{
		result = refuse(problem, "STRINGS too large");
		goto done;
	}
	buffer_append(&sections[AE_PACK_SECTION_STRINGS], strings.data, strings.size);

	/* the tags, their bytes, fixups, parts and resources */
	for (index = 0; index < data->tag_count && !failed; index++)
	{
		struct ae_pack_build_tag const *tag = &data->tags[index];
		struct ae_pack_tag_record record;

		if (tag->bytes_size > AE_PACK_MAXIMUM_TAG_SIZE || tag->fixup_count > AE_PACK_MAXIMUM_FIXUPS ||
			tag->part_count > AE_PACK_MAXIMUM_PARTS || tag->resource_count > AE_PACK_MAXIMUM_RESOURCES ||
			(tag->bytes_size && !tag->bytes) || (tag->fixup_count && !tag->fixups) ||
			(tag->part_count && !tag->parts) || (tag->resource_count && !tag->resources))
		{
			result = refuse(problem, "tag %u: past the limits", index);
			goto done;
		}
		/* (an external tag has no bytes: refused, never dropped) */
		if (tag->kind != AE_PACK_TAG_OWN && tag->bytes_size)
		{
			result = refuse(problem, "tag %u: an external tag with bytes", index);
			goto done;
		}
		memset(&record, 0, sizeof(record));
		record.group = tag->group;
		record.parent_groups[0] = tag->parent_groups[0];
		record.parent_groups[1] = tag->parent_groups[1];
		record.name = string_add(&strings, tag->name);
		record.kind = tag->kind;
		record.required = tag->required;
		record.flags = tag->flags;
		record.source_path = string_add(&strings, tag->source_path ? tag->source_path : "");
		if (tag->kind == AE_PACK_TAG_OWN)
		{
			if (tag->bytes_size)
				record.bytes_offset = buffer_place(&sections[AE_PACK_SECTION_TAG_BYTES], tag->bytes, tag->bytes_size);
			record.bytes_size = tag->bytes_size;
		}
		record.fixups_first = fixup_count;
		record.fixups_count = tag->fixup_count;
		record.parts_first = part_count;
		record.parts_count = tag->part_count;
		record.resources_first = resource_count;
		record.resources_count = tag->resource_count;
		buffer_append(&sections[AE_PACK_SECTION_FIXUPS], tag->fixups,
			tag->fixup_count * (unsigned int)sizeof(struct ae_pack_fixup_record));
		/* (a tag's fixups by offset, as the reader wants them: the order the schema walk met them in is not) */
		if (!sections[AE_PACK_SECTION_FIXUPS].failed && tag->fixup_count > 1)
		{
			qsort(sections[AE_PACK_SECTION_FIXUPS].data + fixup_count * sizeof(struct ae_pack_fixup_record),
				tag->fixup_count, sizeof(struct ae_pack_fixup_record), fixup_compare);
		}
		for (k = 0; k < tag->part_count; k++)
		{
			struct ae_pack_build_part const *part = &tag->parts[k];
			struct ae_pack_part_record part_record;

			if (part->vertices_size > AE_PACK_MAXIMUM_GEOMETRY || part->indices_size > AE_PACK_MAXIMUM_GEOMETRY ||
				(part->vertices_size && !part->vertices) || (part->indices_size && !part->indices))
			{
				result = refuse(problem, "tag %u: part past the limits", index);
				goto done;
			}
			memset(&part_record, 0, sizeof(part_record));
			part_record.offset = part->offset;
			part_record.vertex_type = part->vertex_type;
			part_record.triangle_type = part->triangle_type;
			part_record.vertex_count = part->vertex_count;
			part_record.triangle_count = part->triangle_count;
			part_record.vertices_offset = buffer_place(&sections[AE_PACK_SECTION_GEOMETRY], part->vertices,
				part->vertices_size);
			part_record.vertices_size = part->vertices_size;
			part_record.indices_offset = buffer_place(&sections[AE_PACK_SECTION_GEOMETRY], part->indices,
				part->indices_size);
			part_record.indices_size = part->indices_size;
			buffer_append(&sections[AE_PACK_SECTION_PARTS], &part_record, sizeof(part_record));
		}
		for (k = 0; k < tag->resource_count; k++)
		{
			struct ae_pack_build_resource const *resource = &tag->resources[k];
			struct ae_pack_resource_record resource_record;

			if (resource->size > AE_PACK_MAXIMUM_RESOURCE_SIZE || (resource->size && !resource->data))
			{
				result = refuse(problem, "tag %u: resource past the limits", index);
				goto done;
			}
			memset(&resource_record, 0, sizeof(resource_record));
			resource_record.tag = index;
			resource_record.map_offset = resource->map_offset;
			resource_record.size = resource->size;
			resource_record.data_offset = buffer_place(&sections[AE_PACK_SECTION_RESOURCE_DATA], resource->data,
				resource->size);
			resource_record.stored_size = resource->size;
			resource_record.compression = AE_PACK_STORED;
			buffer_append(&sections[AE_PACK_SECTION_RESOURCES], &resource_record, sizeof(resource_record));
		}
		fixup_count += tag->fixup_count;
		part_count += tag->part_count;
		resource_count += tag->resource_count;
		buffer_append(&sections[AE_PACK_SECTION_TAGS], &record, sizeof(record));
		for (kind = 1; kind < AE_PACK_SECTION_KIND_LIMIT; kind++)
			failed |= sections[kind].failed;
	}

	/* the weapons, the credits */
	for (index = 0; index < data->weapon_count; index++)
	{
		struct ae_pack_build_weapon const *weapon = &data->weapons[index];
		struct buffer *weapons = &sections[AE_PACK_SECTION_WEAPONS];

		buffer_u32(weapons, string_add(&strings, weapon->key));
		buffer_u32(weapons, weapon->root_tag);
		buffer_u32(weapons, weapon->item_collection_tag);
		buffer_u32(weapons, weapon->behaviour);
		buffer_u32(weapons, string_add(&strings, weapon->label));
		buffer_u32(weapons, string_add(&strings, weapon->fallback_labels[0]));
		buffer_u32(weapons, string_add(&strings, weapon->fallback_labels[1]));
		buffer_u32(weapons, string_add(&strings, weapon->menu_name));
		buffer_u32(weapons, string_add(&strings, weapon->weapon_pickup_text));
		buffer_u32(weapons, string_add(&strings, weapon->ammo_pickup_text));
		buffer_u32(weapons, weapon->icon_bitmap_tag);
		buffer_u32(weapons, weapon->icon_sequence);
		buffer_u32(weapons, weapon->replaces);
		buffer_u32(weapons, weapon->power);
	}
	if (data->credits)
	{
		unsigned int length = 0;

		while (length <= AE_PACK_MAXIMUM_CREDITS && data->credits[length])
			length++;

		if (length > AE_PACK_MAXIMUM_CREDITS)
		{
			result = refuse(problem, "CREDITS too large");
			goto done;
		}
		buffer_append(&sections[AE_PACK_SECTION_CREDITS], data->credits, length);
	}
	for (kind = 1; kind < AE_PACK_SECTION_KIND_LIMIT; kind++)
		failed |= sections[kind].failed;
	if (failed || strings.failed)
	{
		result = refuse(problem, "the pack would be larger than the format allows (or out of memory)");
		goto done;
	}

	/* the header, the section table, the sections in kind order */
	memset(&header, 0, sizeof(header));
	memcpy(header.magic, AE_PACK_MAGIC, sizeof(header.magic));
	header.format_version = AE_PACK_FORMAT_VERSION;
	header.header_size = AE_PACK_HEADER_SIZE;
	header.flags = data->weapon_count ? 0 : AE_PACK_FLAG_NO_WEAPONS;
	header_text(header.pack_id, sizeof(header.pack_id), data->pack_id);
	header.pack_revision = data->pack_revision;
	header_text(header.display_name, sizeof(header.display_name), data->display_name);
	header.tag_layout = AE_PACK_TAG_LAYOUT_XBOX;
	header.section_count = WRITTEN_SECTIONS;
	buffer_append(&image, &header, sizeof(header));
	buffer_reserve(&image, WRITTEN_SECTIONS * SECTION_ENTRY_SIZE);
	if (!image.failed)
	{
		memset(image.data + image.size, 0, WRITTEN_SECTIONS * SECTION_ENTRY_SIZE);
		image.size += WRITTEN_SECTIONS * SECTION_ENTRY_SIZE;
	}
	for (kind = 1; kind < AE_PACK_SECTION_KIND_LIMIT && !image.failed; kind++)
	{
		struct ae_pack_section entry;

		entry.kind = kind;
		entry.offset = buffer_place(&image, sections[kind].data, sections[kind].size);
		entry.stored_size = sections[kind].size;
		entry.inflated_size = sections[kind].size;
		entry.compression = AE_PACK_STORED;
		if (!image.failed)
			memcpy(image.data + AE_PACK_HEADER_SIZE + (kind - 1) * SECTION_ENTRY_SIZE, &entry, SECTION_ENTRY_SIZE);
	}
	if (image.failed)
	{
		result = refuse(problem, "the pack would be larger than 64 MiB (or out of memory)");
		goto done;
	}
	ae_pack_sha256(image.data, image.size, header.sha256);
	memcpy(image.data + AE_PACK_SHA256_OFFSET, header.sha256, AE_PACK_SHA256_SIZE);

	/* checked as a reader checks it */
	{
		struct ae_pack *pack = (struct ae_pack *)calloc(1, sizeof(*pack));
		struct pack_source source;
		struct ae_pack_problem reason;
		int valid;

		reason.text[0] = 0;
		if (!pack)
		{
			result = refuse(problem, "out of memory");
			goto done;
		}
		memset(&source, 0, sizeof(source));
		source.image = image.data;
		source.image_size = image.size;
		pack->image = image.data;
		valid = pack_load(pack, &source, &reason) && ae_pack_verify_resources(pack, &reason);
		ae_pack_close(pack);
		if (!valid)
		{
			result = refuse(problem, "a reader would refuse the pack: %s", reason.text);
			goto done;
		}
	}

	/* written to <path>.tmp, then put in place (ae_pack_replace_file): a failed write leaves any pack at path as
	it was */
	{
		unsigned int length = (unsigned int)strlen(path);
		char *temporary = (char *)malloc(length + 5);
		FILE *file;

		if (!temporary)
		{
			result = refuse(problem, "out of memory");
			goto done;
		}
		memcpy(temporary, path, length);
		memcpy(temporary + length, ".tmp", 5);
		file = fopen(temporary, "wb");
		if (!file)
		{
			result = refuse(problem, "cannot write %s", temporary);
			free(temporary);
			goto done;
		}
		result = fwrite(image.data, 1, image.size, file) == image.size;
		if (fclose(file) != 0)
			result = 0;
		if (result)
			result = ae_pack_replace_file(temporary, path, rename);
		if (!result)
		{
			remove(temporary);
			refuse(problem, "cannot write %s", path);
		}
		free(temporary);
	}

done:
	for (kind = 0; kind < AE_PACK_SECTION_KIND_LIMIT; kind++)
		free(sections[kind].data);
	free(image.data);
	free(strings.data);
	free((void *)strings.texts);
	free(strings.offsets);
	return result;
}
