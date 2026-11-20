/*
TAG_PACK.C

The tag packs (tag_pack.h): tag_pack/<name>.pack, one for each manifest in
port/assets/tag_packs (port/linux/src/port_tag_packs.c), written by the game
itself at the main menu from the maps the manifest names, and read by the
tag importer (tag_import.c) as a level loads, which never reads a map for
its tags. tools/tag_pack.py writes the same file on a computer.

At start-up the main menu moves the packs an older build wrote among the
maps (maps/<name>.pack, or a name the pack once had: randomizer.pack is
watanoge_randomizer.pack now) into tag_pack/, so they are kept, not written
again; then asks for each pack its manifest wants then ("always", a setting
on, or a condition of tag_pack_conditions.c's) that is not there, is stale
(its header's content hash is not its manifest's) or no longer matches its
maps; code asks for one later with tag_pack_request (a setting turned on, a
game joined that wants it). A pack none of whose maps is there is not asked
for.

The packs asked for are written together, on a thread of their own, while
the main menu shows a checklist of them over itself (tag_pack_draw): each
pack's name and file, and whether it waits, is being written (how far), is
done or failed. Nothing is asked of the player, and the menus take no input
until it is gone, a moment after the last is done.

A pack is 'rpak', format 2 (TAG_PACK_FORMAT_VERSION), its content hash and
how many maps, then a 56-byte record per map (tag_pack.h); then each map's
pixels and samples, its inflated tag section and its table of ranges.
Every place is an offset in the file, so the parts may come in any order.
Each distinct blob of pixels or samples is kept once.

A disc map is a plain 0x800-byte header and a zlib stream, and its tag
section is at the end, so each map is inflated twice: once for its tags,
which say which ranges to keep, and again to copy those ranges as the
stream passes them. A map several packs are made of (c40 is in three) is
read and inflated those two times for all of them at once, each pack given
the ranges it keeps as the stream passes them. Each
pack is written as <name>.pack.partial and renamed when it is whole, so a
stop part-way leaves no pack that looks complete.
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "../src/ae_worker.h"

#include "cseries.h"
#include "cseries/errors.h"
#include "cseries/cseries_windows.h"
#include "cache/cache_files.h"
#include "cache/physical_memory_map.h"
#include "cutscene/cinematics.h"
#include "interface/interface.h"
#include "main/main.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "text/unicode.h"

#include "halo_port_limits.h"
#include "tag_pack.h"
#include "../src/port_tag_packs.h"

#include <xtl.h>
/* the port's zlib, the same one that unpacks a map while precaching it */
#include "../../third_party/zlib/zlib_prefixed.h"

/* the addresses a map file gives its tag pointers (halo_port_window.h) */
#define PACK_WINDOW 0x803A6000UL
#define MAP_HEADER_SIZE 0x800
#define PACK_LIMIT 0x7F000000L
#define RANGE_LIMIT (64L * 1024L * 1024L)
#define IO_CHUNK 0x40000
#define SEEN_SLOTS 0x20000
/* the most packs written together (as many as there are manifests) */
#define BATCH_PACKS 8
/* the checklist kept up after the last pack is done, and after one failed */
#define CHECKLIST_DONE_MILLISECONDS 1500
#define CHECKLIST_FAILED_MILLISECONDS 5000
/* the folder of the packs, in the data's (d:\), and the maps' language's
below it (maps_de: tag_pack\de\) */
#define PACK_DIRECTORY "d:\\tag_pack\\"

extern int config_boolean(char const *name);
extern void platform_log(char const *format, ...);

/* bitmap_group's bitmaps block, and a bitmap_data's fields */
#define BITMAPS_COUNT 0x60
#define BITMAPS_ADDRESS 0x64
#define BITMAP_DATA_SIZE 0x30
/* sound_definition's pitch ranges, a range's permutations, a permutation's samples */
#define PITCH_COUNT 0x98
#define PITCH_ADDRESS 0x9C
#define PITCH_SIZE 0x48
#define PERMUTATION_COUNT 0x3C
#define PERMUTATION_ADDRESS 0x40
#define PERMUTATION_SIZE 0x7C
#define SAMPLES_SIZE 0x40
#define SAMPLES_OFFSET 0x48

struct pack_range
{
	long offset;
	long size;
	long data_at;
};

struct pack_record
{
	char leaf[TAG_PACK_MAP_NAME_SIZE];
	long file_length;
	long tag_offset;
	long tag_size;
	long tags_at;
	long range_count;
	long ranges_at;
};

struct pack_stream
{
	HANDLE file;
	int compressed;
	int inflating;
	z_stream zlib;
	unsigned char *input;
	long produced;
};

struct pack_seen
{
	unsigned long first;
	unsigned long second;
	long size;
	long data_at;
};

/* bitmaps.c bitmap_format_bits_per_pixel_table, through p8-bump */
static short const bitmap_bits[] = { 8, 8, 8, 16, 0, 0, 16, 0, 16, 16, 32, 32, 0, 0, 4, 8, 8, 8 };

static unsigned long read_u32(unsigned char const *bytes)
{
	return (unsigned long)bytes[0] | ((unsigned long)bytes[1] << 8) |
		((unsigned long)bytes[2] << 16) | ((unsigned long)bytes[3] << 24);
}

static long read_s32(unsigned char const *bytes)
{
	unsigned long value = read_u32(bytes);

	return (value & 0x80000000UL) ? -(long)(0xFFFFFFFFUL - value) - 1 : (long)value;
}

static short read_s16(unsigned char const *bytes)
{
	return (short)((unsigned short)bytes[0] | ((unsigned short)bytes[1] << 8));
}

static void write_u32(unsigned char *bytes, unsigned long value)
{
	bytes[0] = (unsigned char)value;
	bytes[1] = (unsigned char)(value >> 8);
	bytes[2] = (unsigned char)(value >> 16);
	bytes[3] = (unsigned char)(value >> 24);
}

/* ---------- files */

static void map_path(char *path, unsigned int size, char const *leaf)
{
	snprintf(path, size, "%s%s.map", cache_files_map_directory(), leaf);
}

static int file_at(HANDLE file, long offset)
{
	DWORD result = SetFilePointer(file, offset, NULL, FILE_BEGIN);

	return result != INVALID_SET_FILE_POINTER || GetLastError() == 0;
}

static int file_read(HANDLE file, void *buffer, unsigned long size)
{
	unsigned long done = 0;

	while (done < size)
	{
		DWORD chunk = (DWORD)(size - done);
		DWORD got = 0;

		if (chunk > IO_CHUNK)
			chunk = IO_CHUNK;
		if (!ReadFile(file, (char *)buffer + done, chunk, &got, NULL) || got == 0)
			return 0;
		done += got;
	}

	return 1;
}

static int file_exists(char const *path)
{
	HANDLE file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (file == INVALID_HANDLE_VALUE)
		return 0;
	CloseHandle(file);
	return 1;
}

int tag_pack_map_header(char const *leaf, long *file_length, long *tag_offset, long *tag_size, int *compressed)
{
	char path[512];
	unsigned char header[0x18];
	HANDLE file;
	DWORD physical;
	int good;

	map_path(path, sizeof(path), leaf);
	file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return 0;
	good = file_read(file, header, sizeof(header));
	physical = GetFileSize(file, NULL);
	CloseHandle(file);
	if (!good)
		return 0;
	*file_length = read_s32(header + 8);
	*tag_offset = read_s32(header + 0x10);
	*tag_size = read_s32(header + 0x14);
	if (read_u32(header) != 'head' || read_s32(header + 4) != 5 ||
		*file_length < MAP_HEADER_SIZE || *file_length > 0x11600000L ||
		*tag_offset < MAP_HEADER_SIZE || *tag_size < 36 || *tag_size > (long)TAG_CACHE_SIZE ||
		*tag_size > *file_length || *tag_offset > *file_length - *tag_size)
		return 0;
	if (compressed)
		*compressed = physical == INVALID_FILE_SIZE || (long)physical < *file_length;

	return 1;
}

/* ---------- what a map's tags point at */

static long locate(unsigned char const *blob, long blob_size, unsigned long address)
{
	if (address < PACK_WINDOW || address - PACK_WINDOW >= (unsigned long)blob_size)
		return -1;

	return (long)(address - PACK_WINDOW);
}

static char const *tag_name(unsigned char const *blob, long blob_size, long instance)
{
	long at = locate(blob, blob_size, read_u32(blob + instance + 0x10));
	long room;

	if (at < 0)
		return "";
	room = blob_size - at;
	if (room > 256)
		room = 256;
	if (!memchr(blob + at, 0, (size_t)room))
		return "";

	return (char const *)blob + at;
}

static long bitmap_bytes(short width, short height, short depth, short kind, short format, unsigned short flags,
	short mipmaps)
{
	long pixels = 0;
	short level;

	if (format < 0 || format >= (short)NUMBEROF(bitmap_bits) || !bitmap_bits[format])
		return 0;
	if (mipmaps < 0 || mipmaps > 16 || width <= 0 || height <= 0 || depth <= 0 ||
		width > 4096 || height > 4096 || depth > 4096)
		return 0;
	for (level = 0; level <= mipmaps; level++)
	{
		long wide = width >> level;
		long high = height >> level;
		long deep = depth >> level;

		if (wide < 1)
			wide = 1;
		if (high < 1)
			high = 1;
		if (deep < 1)
			deep = 1;
		if (flags & 2)
		{
			wide = (wide + 3) & ~3L;
			high = (high + 3) & ~3L;
		}
		pixels += wide * high * deep * (kind == 2 ? 6 : 1);
		if (pixels > 64L * 1024L * 1024L)
			return 0;
	}

	return pixels / 8 * bitmap_bits[format] + (pixels % 8) * bitmap_bits[format] / 8;
}

struct range_list
{
	struct pack_range *ranges;
	long count;
	long capacity;
};

static int range_add(struct range_list *list, long file_length, long offset, long size)
{
	if (size <= 0 || size > RANGE_LIMIT || offset < 0 || offset > file_length - size)
		return 1;
	if (list->count == list->capacity)
	{
		long capacity = list->capacity ? list->capacity * 2 : 4096;
		struct pack_range *grown = malloc((unsigned long)capacity * sizeof(*grown));

		if (!grown)
			return 0;
		if (list->ranges)
		{
			memcpy(grown, list->ranges, (unsigned long)list->count * sizeof(*grown));
			free(list->ranges);
		}
		list->ranges = grown;
		list->capacity = capacity;
	}
	list->ranges[list->count].offset = offset;
	list->ranges[list->count].size = size;
	list->ranges[list->count].data_at = 0;
	list->count++;

	return 1;
}

static int range_less(void const *left, void const *right)
{
	struct pack_range const *a = left;
	struct pack_range const *b = right;

	if (a->offset != b->offset)
		return a->offset < b->offset ? -1 : 1;

	return a->size < b->size ? -1 : a->size > b->size;
}

static void bitmap_ranges(struct range_list *list, unsigned char const *blob, long blob_size, long base,
	long file_length, int *ok)
{
	long count;
	long at;
	long index;

	if (base + BITMAPS_ADDRESS + 4 > blob_size)
		return;
	count = read_s32(blob + base + BITMAPS_COUNT);
	at = locate(blob, blob_size, read_u32(blob + base + BITMAPS_ADDRESS));
	if (at < 0 || count <= 0 || count > 512 || at + count * BITMAP_DATA_SIZE > blob_size)
		return;
	for (index = 0; index < count && *ok; index++)
	{
		unsigned char const *record = blob + at + index * BITMAP_DATA_SIZE;
		long pixels_offset = read_s32(record + 0x18);
		long need = read_s32(record + 0x1C);
		long read;

		if (pixels_offset < 0)
			continue;
		read = bitmap_bytes(read_s16(record + 4), read_s16(record + 6), read_s16(record + 8),
			read_s16(record + 0x0A), read_s16(record + 0x0C), (unsigned short)read_s16(record + 0x0E),
			read_s16(record + 0x14));
		if (read > need)
			need = read;
		*ok = range_add(list, file_length, pixels_offset, need);
	}
}

static void sound_ranges(struct range_list *list, unsigned char const *blob, long blob_size, long base,
	long file_length, int *ok)
{
	long pitch_count;
	long pitch_at;
	long pitch;

	if (base + PITCH_ADDRESS + 4 > blob_size)
		return;
	pitch_count = read_s32(blob + base + PITCH_COUNT);
	pitch_at = locate(blob, blob_size, read_u32(blob + base + PITCH_ADDRESS));
	if (pitch_at < 0 || pitch_count <= 0 || pitch_count > 32 || pitch_at + pitch_count * PITCH_SIZE > blob_size)
		return;
	for (pitch = 0; pitch < pitch_count && *ok; pitch++)
	{
		long at = pitch_at + pitch * PITCH_SIZE;
		long count = read_s32(blob + at + PERMUTATION_COUNT);
		long permutations = locate(blob, blob_size, read_u32(blob + at + PERMUTATION_ADDRESS));
		long index;

		if (permutations < 0 || count <= 0 || count > 256 || permutations + count * PERMUTATION_SIZE > blob_size)
			continue;
		for (index = 0; index < count && *ok; index++)
		{
			unsigned char const *sample = blob + permutations + index * PERMUTATION_SIZE;

			*ok = range_add(list, file_length, read_s32(sample + SAMPLES_OFFSET), read_s32(sample + SAMPLES_SIZE));
		}
	}
}

/* the ranges a map's bitmaps and sounds read, in order, the overlapping
ones joined. 0 when the tags are not a campaign map's */
static int collect(long pack, struct range_list *list, unsigned char const *blob, long blob_size, long file_length)
{
	long count;
	long instances;
	long index;
	long bitmaps = 0;
	long sounds = 0;
	long kept;
	int ok = 1;

	if (blob_size < 36 || read_u32(blob + 0x20) != 'tags')
		return 0;
	count = read_s32(blob + 0x0C);
	instances = locate(blob, blob_size, read_u32(blob));
	if (count <= 0 || count > 100000 || instances < 0 || instances + count * 0x20 > blob_size)
		return 0;
	for (index = 0; index < count && ok; index++)
	{
		long instance = instances + index * 0x20;
		unsigned long group = read_u32(blob + instance);
		long base = locate(blob, blob_size, read_u32(blob + instance + 0x14));

		if (base < 0 || (group != 'bitm' && group != 'snd!'))
			continue;
		if (!port_tag_packs_keep(pack, group, tag_name(blob, blob_size, instance)))
			continue;
		if (group == 'bitm')
		{
			if (base + BITMAPS_ADDRESS + 4 <= blob_size)
				bitmaps++;
			bitmap_ranges(list, blob, blob_size, base, file_length, &ok);
		}
		else
		{
			if (base + PITCH_ADDRESS + 4 <= blob_size)
				sounds++;
			sound_ranges(list, blob, blob_size, base, file_length, &ok);
		}
	}
	if (!ok || bitmaps < port_tag_packs_minimum_bitmaps(pack) || sounds < port_tag_packs_minimum_sounds(pack))
		return 0;
	qsort(list->ranges, (unsigned long)list->count, sizeof(*list->ranges), range_less);
	kept = 0;
	for (index = 0; index < list->count; index++)
	{
		struct pack_range *range = list->ranges + index;

		if (kept && range->offset <= list->ranges[kept - 1].offset + list->ranges[kept - 1].size)
		{
			struct pack_range *last = list->ranges + kept - 1;
			long end = range->offset + range->size;

			if (end > last->offset + last->size)
				last->size = end - last->offset;
		}
		else
			list->ranges[kept++] = *range;
	}
	list->count = kept;

	return 1;
}

/* ---------- each blob once */

static unsigned long rotate(unsigned long value, int bits)
{
	value &= 0xFFFFFFFFUL;

	return ((value << bits) | (value >> (32 - bits))) & 0xFFFFFFFFUL;
}

static unsigned long finish_hash(unsigned long value)
{
	value &= 0xFFFFFFFFUL;
	value ^= value >> 16;
	value = (value * 0x85EBCA6BUL) & 0xFFFFFFFFUL;
	value ^= value >> 13;
	value = (value * 0xC2B2AE35UL) & 0xFFFFFFFFUL;
	value ^= value >> 16;

	return value;
}

/* two 32-bit lanes, whole words. A blob is hashed in IO_CHUNK pieces, a
multiple of four, so only its last piece has a tail */
static void hash_bytes(unsigned long *first, unsigned long *second, unsigned char const *bytes, long size)
{
	long index;

	for (index = 0; index + 4 <= size; index += 4)
	{
		unsigned long word = read_u32(bytes + index);

		*first ^= rotate(word * 0xCC9E2D51UL, 15) * 0x1B873593UL;
		*first = rotate(*first, 13) * 5 + 0xE6546B64UL;
		*second ^= rotate(word * 0x85EBCA6BUL, 17) * 0xC2B2AE35UL;
		*second = rotate(*second, 11) * 9 + 0x52DCE729UL;
	}
	for (; index < size; index++)
	{
		*first = rotate(*first ^ bytes[index], 7) * 0x01000193UL;
		*second = rotate(*second ^ bytes[index], 9) * 0x0100019DUL;
	}
}

static struct pack_seen *seen_slot(struct pack_seen *table, unsigned long first, unsigned long second, long size)
{
	unsigned long slot = (first ^ (second * 31)) & (SEEN_SLOTS - 1);

	for (;;)
	{
		struct pack_seen *seen = table + slot;

		if (!seen->size || (seen->first == first && seen->second == second && seen->size == size))
			return seen;
		slot = (slot + 1) & (SEEN_SLOTS - 1);
	}
}


/* ---------- the packs being written

Each pack of a batch its own file, its blobs seen, its records, and the
range it is copying as the stream passes it; how far it has got (done of
total, its maps' bytes inflated twice), as a percent the checklist reads */

enum
{
	_job_waiting = 0,
	_job_building,
	_job_done,
	_job_failed
};

struct pack_job
{
	long pack;
	char path[512];
	char partial[520];
	HANDLE out;
	long cursor;
	struct pack_seen *seen;
	long seen_count;
	/* a write refused: the card is full, or the pack is too large */
	int failed;
	struct pack_record records[TAG_PACK_MAPS];
	int record_count;
	long directory_size;
	double done;
	double total;
	/* the map now: whether it is of this pack, its ranges and the one
	being copied (from start, left bytes to go, hashed so far) */
	int in_map;
	struct range_list list;
	long range_index;
	int copying;
	long copy_start;
	long copy_left;
	unsigned long first;
	unsigned long second;
	/* (read by the main thread: tag_pack_draw) */
	volatile int state;
	volatile int percent;
};

static struct
{
	struct pack_job jobs[BATCH_PACKS];
	int count;
	unsigned char *chunk;
	/* the jobs the map being inflated is of (a bit each), for the progress */
	unsigned long inflating_jobs;
	/* started, its thread running (a worker of the platform layer's, on its own); its thread done */
	volatile int running;
	volatile int finished;
	unsigned long started_at;
	/* when the checklist's last pack was done, and whether one failed */
	unsigned long ended_at;
	int any_failed;
	int shown;
} batch;

/* why the last pack was left short or not written, for the state a screen shows: the first reason of
a writing, kept until the next */
static char pack_note_text[160];

static void pack_log(char const *format, ...)
{
	char line[256];
	va_list arguments;

	va_start(arguments, format);
	vsnprintf(line, sizeof(line), format, arguments);
	va_end(arguments);
	if (!pack_note_text[0])
		snprintf(pack_note_text, sizeof(pack_note_text), "%s", line);
	platform_log("%s", line);
}

static void job_progress(struct pack_job *job)
{
	int percent = job->total > 0.0 ? (int)(job->done * 100.0 / job->total) : 0;

	job->percent = percent < 0 ? 0 : percent > 99 ? 99 : percent;
}

static int job_write(struct pack_job *job, void const *buffer, unsigned long size)
{
	unsigned long done = 0;

	if (job->failed)
		return 0;
	if ((unsigned long)job->cursor + size > (unsigned long)PACK_LIMIT)
	{
		platform_log("tag pack: %s past what a 32-bit offset can name", port_tag_packs_name(job->pack));
		job->failed = 1;
		return 0;
	}
	while (done < size)
	{
		DWORD chunk = (DWORD)(size - done);
		DWORD wrote = 0;

		if (chunk > IO_CHUNK)
			chunk = IO_CHUNK;
		if (!WriteFile(job->out, (char const *)buffer + done, chunk, &wrote, NULL) || wrote == 0)
		{
			platform_log("tag pack: could not write %s (error %lu; is the card full?)", job->partial,
				(unsigned long)GetLastError());
			job->failed = 1;
			return 0;
		}
		done += wrote;
	}
	job->cursor += (long)size;

	return 1;
}

/* ---------- a map's bytes, front to back */

static int stream_begin(struct pack_stream *stream, HANDLE file, int compressed)
{
	memset(stream, 0, sizeof(*stream));
	stream->file = file;
	stream->compressed = compressed;
	stream->produced = MAP_HEADER_SIZE;
	if (!file_at(file, MAP_HEADER_SIZE))
		return 0;
	if (!compressed)
		return 1;
	stream->input = malloc(IO_CHUNK);
	if (!stream->input || inflateInit(&stream->zlib) != Z_OK)
		return 0;
	stream->inflating = 1;

	return 1;
}

static void stream_end(struct pack_stream *stream)
{
	if (stream->inflating)
		inflateEnd(&stream->zlib);
	if (stream->input)
		free(stream->input);
	memset(stream, 0, sizeof(*stream));
}

/* the next `size` bytes, into `out`, or passed over when it is NULL; each
pack the map is of the further on (batch.inflating_jobs) */
static int stream_take(struct pack_stream *stream, long size, unsigned char *out)
{
	while (size > 0)
	{
		unsigned char *into = out ? out : batch.chunk;
		long want = size > IO_CHUNK ? IO_CHUNK : size;
		int index;

		if (!stream->compressed)
		{
			if (out ? !file_read(stream->file, into, (unsigned long)want) :
				!file_at(stream->file, stream->produced + want))
				return 0;
		}
		else
		{
			int result;

			stream->zlib.next_out = (Bytef *)into;
			stream->zlib.avail_out = (uInt)want;
			while (stream->zlib.avail_out > 0)
			{
				if (stream->zlib.avail_in == 0)
				{
					DWORD got = 0;

					if (!ReadFile(stream->file, stream->input, IO_CHUNK, &got, NULL) || got == 0)
						return 0;
					stream->zlib.next_in = (Bytef *)stream->input;
					stream->zlib.avail_in = got;
				}
				result = inflate(&stream->zlib, Z_NO_FLUSH);
				if (result == Z_STREAM_END && stream->zlib.avail_out > 0)
					return 0;
				if (result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR)
					return 0;
			}
		}
		stream->produced += want;
		size -= want;
		if (out)
			out += want;
		for (index = 0; index < batch.count; index++)
		{
			if (batch.inflating_jobs & (1UL << index))
			{
				batch.jobs[index].done += (double)want;
				job_progress(&batch.jobs[index]);
			}
		}
	}

	return 1;
}

/* a map opened to be read front to back, read ahead on a thread of its own
(the card's reads alongside zlib's inflating: xbox_files.c) */
static HANDLE map_open(char const *leaf)
{
	char path[512];
	HANDLE file;

	map_path(path, sizeof(path), leaf);
	file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return INVALID_HANDLE_VALUE;
	return file;
}

/* ---------- a pack's ranges, as the stream passes them */

/* the end of the range being copied: the blob kept, at the end of the pack,
unless the same bytes already are in it (then the next blob writes over
this copy) */
static int job_range_end(struct pack_job *job)
{
	struct pack_range *range = job->list.ranges + job->range_index;
	struct pack_seen *seen;
	unsigned long first = finish_hash(job->first ^ (unsigned long)range->size);
	unsigned long second = finish_hash(job->second + (unsigned long)range->size);

	job->copying = 0;
	job->range_index++;
	if (job->seen_count >= SEEN_SLOTS / 4 * 3)
	{
		range->data_at = job->copy_start;
		return 1;
	}
	seen = seen_slot(job->seen, first, second, range->size);
	if (seen->size)
	{
		range->data_at = seen->data_at;
		job->cursor = job->copy_start;
		return file_at(job->out, job->copy_start);
	}
	seen->first = first;
	seen->second = second;
	seen->size = range->size;
	seen->data_at = job->copy_start;
	job->seen_count++;
	range->data_at = job->copy_start;

	return 1;
}

/* the map's bytes from offset (size of them, at bytes): those of the pack's
ranges among them written to it. The ranges are in order and apart, and the
bytes come in order, the header's first */
static int job_feed(struct pack_job *job, long offset, unsigned char const *bytes, long size)
{
	while (size > 0 && job->range_index < job->list.count && !job->failed)
	{
		struct pack_range *range = job->list.ranges + job->range_index;
		long from;
		long skip;
		long take;

		if (!job->copying)
		{
			if (offset + size <= range->offset)
				return 1;
			/* (a range begun before these bytes, but not copied: passed) */
			if (range->offset < offset)
			{
				platform_log("tag pack: %s's range at %ld passed by (at %ld)", port_tag_packs_name(job->pack),
					range->offset, offset);
				return 0;
			}
			job->copying = 1;
			job->copy_start = job->cursor;
			job->copy_left = range->size;
			job->first = 0x52414E44UL;
			job->second = 0x4F4D495AUL;
		}
		from = range->offset + (range->size - job->copy_left);
		skip = from - offset;
		take = size - skip;
		if (take > job->copy_left)
			take = job->copy_left;
		hash_bytes(&job->first, &job->second, bytes + skip, take);
		if (!job_write(job, bytes + skip, (unsigned long)take))
			return 0;
		job->copy_left -= take;
		offset += skip + take;
		bytes += skip + take;
		size -= skip + take;
		if (!job->copy_left && !job_range_end(job))
			return 0;
	}
	return !job->failed;
}

/* where the pack wants the stream next (the next byte of its range), or
-1 when it wants no more of this map */
static long job_wants(struct pack_job const *job)
{
	struct pack_range const *range;

	if (!job->in_map || job->failed || job->range_index >= job->list.count)
		return -1;
	range = job->list.ranges + job->range_index;
	return job->copying ? range->offset + (range->size - job->copy_left) : range->offset;
}

/* ---------- a map, for each pack of it */

static int job_record(struct pack_job *job, char const *leaf, long file_length, long tag_offset, long tag_size,
	unsigned char const *blob)
{
	struct pack_record *record = job->records + job->record_count;
	long index;
	long resource_bytes = 0;

	memset(record, 0, sizeof(*record));
	snprintf(record->leaf, sizeof(record->leaf), "%s", leaf);
	record->file_length = file_length;
	record->tag_offset = tag_offset;
	record->tag_size = tag_size;
	record->tags_at = job->cursor;
	if (!job_write(job, blob, (unsigned long)tag_size))
		return 0;
	record->ranges_at = job->cursor;
	record->range_count = job->list.count;
	for (index = 0; index < job->list.count; index++)
	{
		unsigned char entry[12];

		write_u32(entry, (unsigned long)job->list.ranges[index].offset);
		write_u32(entry + 4, (unsigned long)job->list.ranges[index].size);
		write_u32(entry + 8, (unsigned long)job->list.ranges[index].data_at);
		if (!job_write(job, entry, sizeof(entry)))
			return 0;
		resource_bytes += job->list.ranges[index].size;
	}
	job->record_count++;
	platform_log("tag pack: %s: %s.map, %ld ranges, %ld resource bytes, pack now %ld bytes",
		port_tag_packs_name(job->pack), leaf, job->list.count, resource_bytes, job->cursor);

	return 1;
}

static int pack_has_map(long pack, char const *leaf)
{
	long level;

	for (level = 0; level < port_tag_packs_map_count(pack) && level < TAG_PACK_MAPS; level++)
	{
		if (!_stricmp(port_tag_packs_map(pack, level), leaf))
			return 1;
	}
	return 0;
}

/* the map, read for every pack of the batch it is of: its tags (each pack's
ranges from them), then its pixels and samples, each pack given its own */
static void batch_map(char const *leaf)
{
	unsigned char header[MAP_HEADER_SIZE];
	unsigned char *blob = NULL;
	struct pack_stream stream;
	HANDLE file = INVALID_HANDLE_VALUE;
	long file_length, tag_offset, tag_size;
	double started[BATCH_PACKS];
	int compressed;
	int index;
	int any = 0;
	unsigned long jobs = 0;

	memset(&stream, 0, sizeof(stream));
	for (index = 0; index < batch.count; index++)
	{
		struct pack_job *job = batch.jobs + index;

		job->in_map = !job->failed && pack_has_map(job->pack, leaf);
		started[index] = job->done;
		if (job->in_map)
			jobs |= 1UL << index;
	}
	if (!jobs)
		return;
	if (!tag_pack_map_header(leaf, &file_length, &tag_offset, &tag_size, &compressed))
	{
		pack_log("tag pack: no usable %s.map", leaf);
		return;
	}
	batch.inflating_jobs = jobs;
	file = map_open(leaf);
	blob = malloc((unsigned long)tag_size);
	if (file == INVALID_HANDLE_VALUE || !blob || !file_read(file, header, sizeof(header)))
	{
		pack_log("tag pack: %s.map: %s", leaf, file == INVALID_HANDLE_VALUE ? "could not open it" :
			!blob ? "out of memory for its tags" : "could not read its header");
		goto done;
	}
	/* the tags, at the stream's end */
	if (!stream_begin(&stream, file, compressed) ||
		!stream_take(&stream, tag_offset - MAP_HEADER_SIZE, NULL) ||
		!stream_take(&stream, tag_size, blob))
	{
		pack_log("tag pack: %s.map ends early (a half-copied map?)", leaf);
		goto done;
	}
	stream_end(&stream);
	CloseHandle(file);
	file = INVALID_HANDLE_VALUE;
	for (index = 0; index < batch.count; index++)
	{
		struct pack_job *job = batch.jobs + index;

		if (!job->in_map)
			continue;
		memset(&job->list, 0, sizeof(job->list));
		job->range_index = 0;
		job->copying = 0;
		job->done = started[index] + (double)file_length;
		job_progress(job);
		if (!collect(job->pack, &job->list, blob, tag_size, file_length))
		{
			pack_log("tag pack: %s.map's tags are not the map %s wants", leaf, port_tag_packs_name(job->pack));
			job->in_map = 0;
			jobs &= ~(1UL << index);
			continue;
		}
		any = 1;
	}
	batch.inflating_jobs = jobs;
	if (!any)
		goto done;
	/* the pixels and samples, from the start again: the header's bytes
	first, then the stream's, as far as the furthest range wanted */
	file = map_open(leaf);
	if (file == INVALID_HANDLE_VALUE || !stream_begin(&stream, file, compressed))
	{
		pack_log("tag pack: %s.map could not be opened again for its pixels and samples", leaf);
		goto done;
	}
	for (index = 0; index < batch.count; index++)
	{
		if (batch.jobs[index].in_map && !job_feed(batch.jobs + index, 0, header, MAP_HEADER_SIZE))
			batch.jobs[index].in_map = 0;
	}
	for (;;)
	{
		long next = -1;
		long end = 0;
		long size;

		for (index = 0; index < batch.count; index++)
		{
			struct pack_job *job = batch.jobs + index;
			long wants = job_wants(job);

			if (wants < 0)
				continue;
			if (next < 0 || wants < next)
				next = wants;
			if (job->list.ranges[job->list.count - 1].offset + job->list.ranges[job->list.count - 1].size > end)
				end = job->list.ranges[job->list.count - 1].offset + job->list.ranges[job->list.count - 1].size;
		}
		if (next < 0)
			break;
		if (next > stream.produced && !stream_take(&stream, next - stream.produced, NULL))
			break;
		size = end - stream.produced;
		if (size > IO_CHUNK)
			size = IO_CHUNK;
		{
			long offset = stream.produced;

			if (size <= 0 || !stream_take(&stream, size, batch.chunk))
				break;
			for (index = 0; index < batch.count; index++)
			{
				if (job_wants(batch.jobs + index) >= 0 && !job_feed(batch.jobs + index, offset, batch.chunk, size))
				{
					pack_log("tag pack: %s could not copy %s.map's range near %ld",
						port_tag_packs_name(batch.jobs[index].pack), leaf, offset);
					batch.jobs[index].in_map = 0;
				}
			}
		}
	}
	for (index = 0; index < batch.count; index++)
	{
		struct pack_job *job = batch.jobs + index;

		if (!job->in_map)
			continue;
		if (job->range_index < job->list.count)
			pack_log("tag pack: %s.map ended before %s's ranges did", leaf, port_tag_packs_name(job->pack));
		else
			job_record(job, leaf, file_length, tag_offset, tag_size, blob);
	}
done:
	stream_end(&stream);
	if (file != INVALID_HANDLE_VALUE)
		CloseHandle(file);
	batch.inflating_jobs = 0;
	for (index = 0; index < batch.count; index++)
	{
		struct pack_job *job = batch.jobs + index;

		if (pack_has_map(job->pack, leaf))
		{
			job->done = started[index] + 2.0 * (double)file_length;
			job_progress(job);
		}
		if (job->list.ranges)
			free(job->list.ranges);
		memset(&job->list, 0, sizeof(job->list));
		job->in_map = 0;
	}
	if (blob)
		free(blob);
}

/* ---------- a pack, begun and finished */

static int job_finish(struct pack_job *job)
{
	unsigned char *directory = malloc((unsigned long)job->directory_size);
	int index;
	int ok;

	if (!directory)
		return 0;
	memset(directory, 0, (unsigned long)job->directory_size);
	write_u32(directory, 'rpak');
	write_u32(directory + 4, TAG_PACK_FORMAT_VERSION);
	write_u32(directory + 8, port_tag_packs_content(job->pack));
	write_u32(directory + 12, (unsigned long)job->record_count);
	for (index = 0; index < job->record_count; index++)
	{
		unsigned char *entry = directory + TAG_PACK_HEADER_SIZE + index * TAG_PACK_RECORD_SIZE;

		memcpy(entry, job->records[index].leaf, TAG_PACK_MAP_NAME_SIZE - 1);
		write_u32(entry + 32, (unsigned long)job->records[index].file_length);
		write_u32(entry + 36, (unsigned long)job->records[index].tag_offset);
		write_u32(entry + 40, (unsigned long)job->records[index].tag_size);
		write_u32(entry + 44, (unsigned long)job->records[index].tags_at);
		write_u32(entry + 48, (unsigned long)job->records[index].range_count);
		write_u32(entry + 52, (unsigned long)job->records[index].ranges_at);
	}
	/* a copy written over at the end leaves bytes past it */
	ok = file_at(job->out, job->cursor) && SetEndOfFile(job->out) && file_at(job->out, 0);
	job->cursor = 0;
	ok = ok && job_write(job, directory, (unsigned long)job->directory_size);
	free(directory);

	return ok;
}

static char const *pack_path(long pack, char *path, unsigned int size);
static void pack_directory_make(void);

static int job_begin(struct pack_job *job)
{
	long map_count = port_tag_packs_map_count(job->pack);
	long level;
	unsigned char *blank;
	int ok;

	if (map_count > TAG_PACK_MAPS)
		map_count = TAG_PACK_MAPS;
	job->directory_size = TAG_PACK_HEADER_SIZE + map_count * TAG_PACK_RECORD_SIZE;
	pack_path(job->pack, job->path, sizeof(job->path));
	snprintf(job->partial, sizeof(job->partial), "%s.partial", job->path);
	for (level = 0; level < map_count; level++)
	{
		long file_length, tag_offset, tag_size;

		if (tag_pack_map_header(port_tag_packs_map(job->pack, level), &file_length, &tag_offset, &tag_size, NULL))
			job->total += 2.0 * (double)file_length;
	}
	job->seen = malloc(SEEN_SLOTS * sizeof(*job->seen));
	if (!job->seen || job->total <= 0.0)
	{
		pack_log("tag pack: cannot write %s (%s)", job->path,
			job->total <= 0.0 ? "none of its maps is there" : "out of memory");
		return 0;
	}
	memset(job->seen, 0, SEEN_SLOTS * sizeof(*job->seen));
	job->out = CreateFileA(job->partial, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (job->out == INVALID_HANDLE_VALUE)
	{
		pack_log("tag pack: could not write %s (error %lu)", job->partial, (unsigned long)GetLastError());
		job->out = NULL;
		return 0;
	}
	blank = malloc((unsigned long)job->directory_size);
	if (!blank)
		return 0;
	memset(blank, 0, (unsigned long)job->directory_size);
	ok = job_write(job, blank, (unsigned long)job->directory_size);
	free(blank);
	platform_log("tag pack: writing %s (once)", job->path);

	return ok;
}

static void job_end(struct pack_job *job)
{
	int ok = !job->failed && job->out && job->record_count > 0 && job_finish(job);

	if (job->out)
		CloseHandle(job->out);
	job->out = NULL;
	if (ok)
	{
		DeleteFileA(job->path);
		ok = MoveFileA(job->partial, job->path);
		if (!ok)
			pack_log("tag pack: could not rename %s (error %lu)", job->partial, (unsigned long)GetLastError());
	}
	if (!ok)
		DeleteFileA(job->partial);
	else
		platform_log("tag pack: wrote %s (%d maps, %ld distinct pixel and sample blobs)", job->path,
			job->record_count, job->seen_count);
	if (job->seen)
		free(job->seen);
	job->seen = NULL;
	job->percent = ok ? 100 : job->percent;
	job->state = ok ? _job_done : _job_failed;
}

/* the batch's thread: every pack begun, each map once for all of them, every
pack finished */
static void *batch_thread(void *context)
{
	char const *leaves[BATCH_PACKS * TAG_PACK_MAPS];
	int leaf_count = 0;
	int index;
	unsigned long started = system_milliseconds();

	(void)context;
	pack_directory_make();
	for (index = 0; index < batch.count; index++)
	{
		struct pack_job *job = batch.jobs + index;
		long level;

		job->state = _job_building;
		if (!job_begin(job))
		{
			job->failed = 1;
			continue;
		}
		/* (its maps, each once in the batch's order) */
		for (level = 0; level < port_tag_packs_map_count(job->pack) && level < TAG_PACK_MAPS; level++)
		{
			char const *leaf = port_tag_packs_map(job->pack, level);
			int seen;

			for (seen = 0; seen < leaf_count && _stricmp(leaves[seen], leaf); seen++)
				;
			if (seen == leaf_count)
				leaves[leaf_count++] = leaf;
		}
	}
	for (index = 0; index < leaf_count; index++)
	{
		int job_index;

		batch_map(leaves[index]);
		/* (a pack whose maps are all read is done now, not at the batch's end) */
		for (job_index = 0; job_index < batch.count; job_index++)
		{
			struct pack_job *job = batch.jobs + job_index;
			int later;
			int remaining = 0;

			if (job->state != _job_building)
				continue;
			for (later = index + 1; later < leaf_count; later++)
				remaining |= pack_has_map(job->pack, leaves[later]);
			if (!remaining || job->failed)
				job_end(job);
		}
	}
	for (index = 0; index < batch.count; index++)
	{
		if (batch.jobs[index].state == _job_building)
			job_end(batch.jobs + index);
	}
	platform_log("tag pack: %d pack%s written in %.1f s", batch.count, batch.count == 1 ? "" : "s",
		(double)(system_milliseconds() - started) / 1000.0);
	batch.finished = 1;

	return NULL;
}

/* ---------- where the packs are

tag_pack\<name>.pack in the data (d:\), or tag_pack\<language>\ for the
maps of another language (maps_de: tag_pack\de\), whose maps' packs differ */

static void pack_directory(char *directory, unsigned int size)
{
	char const *maps = cache_files_map_directory();
	char const *language = strstr(maps, "maps_");

	if (language)
	{
		char name[32];
		int length = 0;

		for (language += 5; *language && *language != '\\' && length < (int)sizeof(name) - 1; language++)
			name[length++] = *language;
		name[length] = 0;
		snprintf(directory, size, "%s%s\\", PACK_DIRECTORY, name);
	}
	else
		snprintf(directory, size, "%s", PACK_DIRECTORY);
}

static void pack_directory_make(void)
{
	char directory[512];
	size_t length;

	CreateDirectoryA(PACK_DIRECTORY, NULL);
	pack_directory(directory, sizeof(directory));
	length = strlen(directory);
	if (length && directory[length - 1] == '\\')
		directory[length - 1] = 0;
	CreateDirectoryA(directory, NULL);
}

static char const *pack_path(long pack, char *path, unsigned int size)
{
	char directory[512];

	pack_directory(directory, sizeof(directory));
	snprintf(path, size, "%s%s.pack", directory, port_tag_packs_name(pack));
	return path;
}

/* a file moved: renamed, or (another drive: a maps folder moved,
game.maps.path) copied and the old one deleted */
static int file_move(char const *from, char const *to)
{
	HANDLE in;
	HANDLE out;
	unsigned char *buffer;
	int ok = 1;

	if (MoveFileA(from, to))
		return 1;
	in = CreateFileA(from, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (in == INVALID_HANDLE_VALUE)
		return 0;
	out = CreateFileA(to, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	buffer = malloc(IO_CHUNK);
	if (out == INVALID_HANDLE_VALUE || !buffer)
		ok = 0;
	while (ok)
	{
		DWORD got = 0;
		DWORD wrote = 0;

		if (!ReadFile(in, buffer, IO_CHUNK, &got, NULL))
			ok = 0;
		else if (!got)
			break;
		else if (!WriteFile(out, buffer, got, &wrote, NULL) || wrote != got)
			ok = 0;
	}
	CloseHandle(in);
	if (out != INVALID_HANDLE_VALUE)
		CloseHandle(out);
	if (buffer)
		free(buffer);
	if (ok)
		DeleteFileA(from);
	else
		DeleteFileA(to);
	return ok;
}

/* the pack's files an older build left among the maps (its own name, or one
it once had: legacy_file_names) moved into tag_pack\, not written again: the
first there taken, the others deleted (a pack is only ever written again),
and their .partial files deleted */
static void pack_move_old(long pack)
{
	char path[512];
	long index;

	pack_path(pack, path, sizeof(path));
	for (index = -1; index < port_tag_packs_legacy_count(pack); index++)
	{
		char old[512];
		char partial[520];

		if (index < 0)
			snprintf(old, sizeof(old), "%s%s.pack", cache_files_map_directory(), port_tag_packs_name(pack));
		else
			snprintf(old, sizeof(old), "%s%s", cache_files_map_directory(), port_tag_packs_legacy(pack, index));
		snprintf(partial, sizeof(partial), "%s.partial", old);
		DeleteFileA(partial);
		if (!file_exists(old))
			continue;
		if (file_exists(path))
		{
			if (DeleteFileA(old))
				error(_error_silent, "tag pack: deleted %s, which %s is now", old, path);
		}
		else if (file_move(old, path))
			error(_error_silent, "tag pack: moved %s to %s", old, path);
		else
			error(_error_silent, "tag pack: could not move %s to %s (error %lu)", old, path,
				(unsigned long)GetLastError());
	}
}

/* ---------- the packs */

/* whether any map of the pack is there to write it from */
static int pack_buildable(long pack)
{
	long level;

	for (level = 0; level < port_tag_packs_map_count(pack); level++)
	{
		long file_length, tag_offset, tag_size;

		if (tag_pack_map_header(port_tag_packs_map(pack, level), &file_length, &tag_offset, &tag_size, NULL))
			return 1;
	}
	return 0;
}

/* the pack is there, of this format and content, and each map it has is
the one in maps/ */
static int pack_ready(long pack)
{
	char path[512];
	unsigned char head[TAG_PACK_HEADER_SIZE];
	char leaves[TAG_PACK_MAPS][TAG_PACK_MAP_NAME_SIZE];
	HANDLE file;
	unsigned long count;
	unsigned long index;
	long level;
	int ready;

	if (pack < 0)
		return 0;
	memset(leaves, 0, sizeof(leaves));
	file = CreateFileA(pack_path(pack, path, sizeof(path)), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return 0;
	ready = file_read(file, head, sizeof(head)) && read_u32(head) == 'rpak' &&
		read_u32(head + 4) == TAG_PACK_FORMAT_VERSION && read_u32(head + 8) == port_tag_packs_content(pack);
	count = ready ? read_u32(head + 12) : 0;
	if (!count || count > TAG_PACK_MAPS)
		ready = 0;
	for (index = 0; ready && index < count; index++)
	{
		unsigned char record[TAG_PACK_RECORD_SIZE];
		long file_length, tag_offset, tag_size;

		if (!file_read(file, record, sizeof(record)))
		{
			ready = 0;
			break;
		}
		memcpy(leaves[index], record, TAG_PACK_MAP_NAME_SIZE - 1);
		leaves[index][TAG_PACK_MAP_NAME_SIZE - 1] = 0;
		/* (a map no longer there is read from nowhere: the pack's is as good) */
		if (tag_pack_map_header(leaves[index], &file_length, &tag_offset, &tag_size, NULL) &&
			(file_length != read_s32(record + 32) || tag_offset != read_s32(record + 36) ||
				tag_size != read_s32(record + 40)))
			ready = 0;
	}
	CloseHandle(file);
	/* (and each of the manifest's maps there is, in it: a pack written
	before one of its maps was copied in has not got it) */
	for (level = 0; ready && level < port_tag_packs_map_count(pack) && level < TAG_PACK_MAPS; level++)
	{
		long file_length, tag_offset, tag_size;
		char const *leaf = port_tag_packs_map(pack, level);

		if (!tag_pack_map_header(leaf, &file_length, &tag_offset, &tag_size, NULL))
			continue;
		for (index = 0; index < count && _stricmp(leaves[index], leaf); index++)
			;
		if (index == count)
			ready = 0;
	}

	return ready;
}

/* those asked for, written in the next batch */
static long pack_queue[BATCH_PACKS];
static int pack_queued;

static void pack_request(long pack)
{
	int index;

	if (pack < 0)
		return;
	for (index = 0; index < pack_queued; index++)
	{
		if (pack_queue[index] == pack)
			return;
	}
	/* (being written now) */
	for (index = 0; batch.running && index < batch.count; index++)
	{
		if (batch.jobs[index].pack == pack && batch.jobs[index].state <= _job_building)
			return;
	}
	if (pack_queued >= (int)NUMBEROF(pack_queue) || pack_ready(pack) || !pack_buildable(pack))
		return;
	pack_queue[pack_queued++] = pack;
	error(_error_silent, "tag pack: %s wanted; written at the main menu", port_tag_packs_name(pack));
}

/* whether a manifest wants its pack at start-up */
static int pack_wanted(long pack)
{
	long index;

	for (index = 0; index < port_tag_packs_wanted_count(pack); index++)
	{
		char const *wanted = port_tag_packs_wanted(pack, index);

		if (!strcmp(wanted, "always"))
			return 1;
		if (!strncmp(wanted, "setting:", 8) && config_boolean(wanted + 8))
			return 1;
		if (!strncmp(wanted, "condition:", 10))
		{
			int condition;

			for (condition = 0; tag_pack_conditions[condition].name; condition++)
			{
				if (!strcmp(tag_pack_conditions[condition].name, wanted + 10) &&
					tag_pack_conditions[condition].wanted())
					return 1;
			}
		}
	}
	return 0;
}

char const *tag_pack_file_name(char const *name)
{
	static char path[512];
	long pack = port_tag_packs_find(name);

	return pack < 0 ? NULL : pack_path(pack, path, sizeof(path));
}

unsigned long tag_pack_content(char const *name)
{
	return port_tag_packs_content(port_tag_packs_find(name));
}

int tag_pack_ready(char const *name)
{
	long pack = port_tag_packs_find(name);
	int index;

	/* (not while it is being written) */
	for (index = 0; batch.running && index < batch.count; index++)
	{
		if (batch.jobs[index].pack == pack && batch.jobs[index].state <= _job_building)
			return 0;
	}
	return pack_ready(pack);
}

void tag_pack_request(char const *name)
{
	long pack = port_tag_packs_find(name);

	if (pack < 0)
		error(_error_silent, "tag pack: no manifest is named %s", name ? name : "(none)");
	pack_request(pack);
}

int tag_pack_busy(void)
{
	return batch.running || pack_queued > 0;
}

static void batch_start(void);

void tag_pack_start_now(void)
{
	if (!batch.running && pack_queued)
		batch_start();
}

void tag_pack_state(char const *name, struct tag_pack_state *state)
{
	long pack = port_tag_packs_find(name);
	long level;
	int index;

	memset(state, 0, sizeof(*state));
	if (pack < 0)
	{
		snprintf(state->reason, sizeof(state->reason), "no manifest is named %s", name ? name : "(none)");
		state->failed = 1;
		return;
	}
	for (level = 0; level < port_tag_packs_map_count(pack) && level < TAG_PACK_MAPS; level++)
	{
		long file_length, tag_offset, tag_size;

		if (tag_pack_map_header(port_tag_packs_map(pack, level), &file_length, &tag_offset, &tag_size, NULL))
			state->maps_wanted++;
	}
	for (index = 0; index < pack_queued; index++)
		state->writing |= pack_queue[index] == pack;
	for (index = 0; index < batch.count; index++)
	{
		struct pack_job *job = batch.jobs + index;

		if (job->pack != pack)
			continue;
		if (batch.running && job->state <= _job_building)
		{
			state->writing = 1;
			state->maps_done = job->record_count;
		}
		else if (job->state == _job_failed)
			state->failed = 1;
	}
	if (!state->writing)
	{
		state->done = pack_ready(pack);
		state->maps_done = state->done ? state->maps_wanted : state->maps_done;
		if (!state->maps_wanted)
		{
			state->failed = 1;
			snprintf(state->reason, sizeof(state->reason), "none of its maps is there");
		}
	}
	if (pack_note_text[0] && !state->reason[0])
		snprintf(state->reason, sizeof(state->reason), "%s", pack_note_text);
}

/* a pack's maps' bytes, as they unpack (those that are there) */
static double pack_map_bytes(long pack)
{
	double total = 0.0;
	long level;

	for (level = 0; level < port_tag_packs_map_count(pack) && level < TAG_PACK_MAPS; level++)
	{
		long file_length, tag_offset, tag_size;

		if (tag_pack_map_header(port_tag_packs_map(pack, level), &file_length, &tag_offset, &tag_size, NULL))
			total += (double)file_length;
	}
	return total;
}

/* the queued packs written together on the batch's thread */
static void batch_start(void)
{
	int index;

	memset(&batch, 0, sizeof(batch));
	pack_note_text[0] = 0;
	batch.chunk = malloc(IO_CHUNK);
	if (!batch.chunk)
	{
		error(_error_silent, "tag pack: out of memory to write the packs");
		pack_queued = 0;
		return;
	}
	for (index = 0; index < pack_queued && index < BATCH_PACKS; index++)
	{
		batch.jobs[index].pack = pack_queue[index];
		batch.jobs[index].state = _job_waiting;
	}
	batch.count = index;
	/* the smallest first (the fewest maps' bytes): its maps are read first,
	so it is done, and usable, soonest. Each map is still read once for all
	the packs of it, so the batch takes no longer */
	{
		double size[BATCH_PACKS];
		int sorted;

		for (index = 0; index < batch.count; index++)
			size[index] = pack_map_bytes(batch.jobs[index].pack);
		for (sorted = 1; sorted < batch.count; sorted++)
		{
			for (index = sorted; index > 0 && size[index] < size[index - 1]; index--)
			{
				double size_swap = size[index];
				long pack_swap = batch.jobs[index].pack;

				size[index] = size[index - 1];
				size[index - 1] = size_swap;
				batch.jobs[index].pack = batch.jobs[index - 1].pack;
				batch.jobs[index - 1].pack = pack_swap;
			}
		}
	}
	pack_queued = 0;
	batch.running = 1;
	batch.shown = 1;
	batch.started_at = system_milliseconds();
	if (!ae_worker_start(batch_thread, NULL))
	{
		error(_error_silent, "tag pack: could not start the packs' thread; writing them here");
		batch_thread(NULL);
	}
	error(_error_silent, "tag pack: writing %d pack%s", batch.count, batch.count == 1 ? "" : "s");
}

void tag_pack_update(boolean main_menu)
{
	static int checked;
	int index;

	/* (the batch's thread done: its packs' outcome kept for the checklist a
	moment) */
	if (batch.running && batch.finished)
	{
		batch.running = 0;
		batch.ended_at = system_milliseconds() | 1;
		for (index = 0; index < batch.count; index++)
		{
			if (batch.jobs[index].state != _job_done)
				batch.any_failed = 1;
			error(_error_silent, "tag pack: %s %s", port_tag_packs_name(batch.jobs[index].pack),
				batch.jobs[index].state == _job_done ? "written" : "failed");
		}
		free(batch.chunk);
		batch.chunk = NULL;
	}
	if (!main_menu)
		return;
	if (!checked)
	{
		long pack;

		checked = 1;
		for (pack = 0; pack < port_tag_packs_count(); pack++)
			pack_move_old(pack);
		for (pack = 0; pack < port_tag_packs_count(); pack++)
		{
			if (pack_wanted(pack))
				pack_request(pack);
		}
	}
	if (!batch.running && pack_queued)
		batch_start();
}

/* ---------- the checklist (interface.c, over everything: the main menu's)

A panel in the middle of the screen: a title, then a row a pack: its name
(the manifest's "building" words, "Building game mode data:" as "Game mode
data") and its file at the left, and at the right whether it waits, how far
it is (a bar under the row), done or failed */

int tag_pack_checklist_shown(void)
{
	unsigned long now = system_milliseconds();

	if (!batch.shown)
		return 0;
	if (batch.running)
		return 1;
	if (batch.ended_at && now - batch.ended_at <
		(unsigned long)(batch.any_failed ? CHECKLIST_FAILED_MILLISECONDS : CHECKLIST_DONE_MILLISECONDS))
	{
		return 1;
	}
	batch.shown = 0;
	return 0;
}

/* a pack's name, as the checklist shows it */
static void pack_display_name(long pack, wchar_t *name, int size)
{
	char const *words = port_tag_packs_text(pack, "building");
	int out = 0;

	if (words && !strncmp(words, "Building ", 9))
	{
		words += 9;
		for (; *words && *words != ':' && *words != '\n' && out < size - 1; words++)
		{
			char character = *words;

			if (!out && character >= 'a' && character <= 'z')
				character = (char)(character - 'a' + 'A');
			name[out++] = (wchar_t)(unsigned char)character;
		}
	}
	else
	{
		for (words = port_tag_packs_name(pack); *words && out < size - 1; words++)
			name[out++] = (wchar_t)(unsigned char)*words;
	}
	name[out] = 0;
}

static void checklist_text(long font_index, real scale, rectangle2d const *bounds, real_argb_color const *color,
	short justification, wchar_t const *text)
{
	rectangle2d scaled = *bounds;

	/* (the text's scale shrinks its clip rectangle too: widened to the box's) */
	scaled.x1 = (short)(bounds->x0 + (bounds->x1 - bounds->x0) / scale + 0.5f);
	rasterizer_text_set_scale(scale, (real)scaled.x0, (real)scaled.y0);
	draw_string_set_draw_mode(font_index, NONE, justification, 0, color);
	rasterizer_draw_unicode_string(&scaled, NULL, NULL, 0, text);
	rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);
}

void tag_pack_draw(void)
{
	long font_index;
	struct font_header *font;
	real scale;
	short line;
	short screen_width;
	short screen_height;
	short width;
	short height;
	short row_height;
	short padding;
	rectangle2d panel;
	rectangle2d bounds;
	real_argb_color white;
	real_argb_color grey;
	real_argb_color blue;
	real_argb_color red;
	int index;

	if (!tag_pack_checklist_shown())
		return;
	font_index = tag_loaded(FONT_GROUP_TAG, "ui\\small_ui");
	if (font_index == NONE)
		font_index = interface_get_tag_index(_interface_font_terminal);
	if (font_index == NONE)
		return;
	screen_width = (short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	screen_height = (short)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	if (screen_width <= 0 || screen_height <= 0)
		return;
	font = font_definition_get(font_index);
	scale = (real)screen_height / 480.0f;
	line = (short)((font->ascending_height + font->descending_height + font->leading_height) * scale + 0.5f);
	if (line <= 0)
		return;
	padding = (short)(line / 2 + 1);
	row_height = (short)(line * 2 + padding);
	width = (short)(screen_width * 6 / 10);
	height = (short)(padding * 3 + line * 2 + row_height * batch.count);
	panel.x0 = (short)((screen_width - width) / 2);
	panel.x1 = (short)(panel.x0 + width);
	panel.y0 = (short)((screen_height - height) / 2);
	panel.y1 = (short)(panel.y0 + height);
	draw_quad(&panel, 0xE0000000UL);
	white.alpha = 1.0f;
	white.red = white.green = white.blue = 1.0f;
	grey.alpha = 1.0f;
	grey.red = grey.green = grey.blue = 0.65f;
	blue.alpha = 1.0f;
	blue.red = 0.16f;
	blue.green = 0.59f;
	blue.blue = 1.0f;
	red.alpha = 1.0f;
	red.red = 0.9f;
	red.green = 0.3f;
	red.blue = 0.28f;
	bounds.x0 = (short)(panel.x0 + padding * 2);
	bounds.x1 = (short)(panel.x1 - padding * 2);
	bounds.y0 = (short)(panel.y0 + padding);
	bounds.y1 = (short)(bounds.y0 + line);
	checklist_text(font_index, scale, &bounds, &white, 0, batch.running ?
		L"Generating data from your maps..." : batch.any_failed ? L"Some data could not be generated." :
		L"Your data is ready.");
	bounds.y0 = (short)(bounds.y1);
	bounds.y1 = (short)(bounds.y0 + line);
	checklist_text(font_index, scale, &bounds, &grey, 0, L"Done once: nothing to do but wait.");
	bounds.y0 = (short)(bounds.y1 + padding);
	for (index = 0; index < batch.count; index++)
	{
		struct pack_job const *job = batch.jobs + index;
		wchar_t name[64];
		wchar_t file[64];
		wchar_t status[16];
		char const *pack_name = port_tag_packs_name(job->pack);
		int state = job->state;
		int percent = job->percent;
		int out;
		rectangle2d row;
		rectangle2d bar;

		pack_display_name(job->pack, name, (int)NUMBEROF(name));
		for (out = 0; pack_name[out] && out < (int)NUMBEROF(file) - 6; out++)
			file[out] = (wchar_t)(unsigned char)pack_name[out];
		file[out++] = L'.';
		file[out++] = L'p';
		file[out++] = L'a';
		file[out++] = L'c';
		file[out++] = L'k';
		file[out] = 0;
		if (state == _job_waiting)
			usnprintf(status, NUMBEROF(status), L"WAITING");
		else if (state == _job_building)
			usnprintf(status, NUMBEROF(status), L"%d%%", percent);
		else if (state == _job_done)
			usnprintf(status, NUMBEROF(status), L"DONE");
		else
			usnprintf(status, NUMBEROF(status), L"FAILED");
		row = bounds;
		row.y1 = (short)(row.y0 + line);
		checklist_text(font_index, scale, &row, state == _job_failed ? &red : &white, 0, name);
		checklist_text(font_index, scale, &row, state == _job_done ? &blue : state == _job_failed ? &red : &grey,
			2 /* right */, status);
		row.y0 = row.y1;
		row.y1 = (short)(row.y0 + line);
		checklist_text(font_index, scale, &row, &grey, 0, file);
		/* (the bar, under the row) */
		bar.x0 = bounds.x0;
		bar.x1 = bounds.x1;
		bar.y0 = (short)(row.y1 + padding / 4);
		bar.y1 = (short)(bar.y0 + MAX(padding / 3, 2));
		draw_quad(&bar, 0x40FFFFFFUL);
		if (state != _job_waiting)
		{
			bar.x1 = (short)(bar.x0 + (long)(bounds.x1 - bounds.x0) * (state == _job_building ? percent : 100) / 100);
			draw_quad(&bar, state == _job_failed ? 0xE0E04C47UL : 0xE02896FFUL);
		}
		bounds.y0 = (short)(bounds.y0 + row_height);
	}
}
