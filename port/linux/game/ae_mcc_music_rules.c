/*
AE_MCC_MUSIC_RULES.C

The menus' MCC music: its rules (ae_mcc_music_rules.h).
*/

#include <string.h>

#include "ae_mcc_music_rules.h"

#define FSB5_VORBIS 15
#define FSB5_CHUNK_CHANNELS 1
#define FSB5_CHUNK_RATE 2
#define FSB5_CHUNK_VORBIS 11
#define OGG_SEGMENTS 255
#define OGG_LARGEST_PACKET (255 * 254) /* a packet in one page (the stream's are a few kilobytes) */

static uint32_t read_u32(unsigned char const *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t read_u64(unsigned char const *p)
{
	return (uint64_t)read_u32(p) | ((uint64_t)read_u32(p + 4) << 32);
}

static size_t header_size(uint32_t version)
{
	return version == 1 ? 60 : 64;
}

size_t ae_mcc_music_fsb5_table_size(unsigned char const *head, size_t size)
{
	uint32_t version, streams_size, names_size;

	if (!head || size < AE_MCC_MUSIC_FSB5_HEADER || memcmp(head, "FSB5", 4) != 0)
		return 0;
	version = read_u32(head + 4);
	streams_size = read_u32(head + 12);
	names_size = read_u32(head + 16);
	/* (Vorbis only; a table past 64 MB is not a bank's) */
	if (version > 1 || read_u32(head + 24) != FSB5_VORBIS || streams_size > (64u << 20) || names_size > (64u << 20))
		return 0;
	return header_size(version) + (size_t)streams_size + (size_t)names_size;
}

int ae_mcc_music_fsb5_streams(unsigned char const *table, size_t table_size, uint64_t file_size,
	struct ae_mcc_music_stream *streams, int maximum)
{
	static unsigned const rates[16] = { 4000, 8000, 11000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 96000 };
	static unsigned const channel_counts[4] = { 1, 2, 6, 8 };
	uint32_t version, count, streams_size, names_size, data_size, index;
	size_t start, position, names;
	int written = 0;

	if (ae_mcc_music_fsb5_table_size(table, table_size) != table_size || maximum < 0)
		return -1;
	version = read_u32(table + 4);
	count = read_u32(table + 8);
	streams_size = read_u32(table + 12);
	names_size = read_u32(table + 16);
	data_size = read_u32(table + 20);
	start = header_size(version);
	names = start + streams_size;
	position = start;
	for (index = 0; index < count; index++)
	{
		struct ae_mcc_music_stream stream;
		uint64_t word;
		int more;

		if (position + 8 > names)
			return -1;
		memset(&stream, 0, sizeof(stream));
		word = read_u64(table + position);
		position += 8;
		more = (int)(word & 1);
		stream.rate = rates[(word >> 1) & 15];
		stream.channels = channel_counts[(word >> 5) & 3];
		stream.offset = ((word >> 7) & 0x7FFFFFFu) * 32u;
		stream.frames = (uint32_t)(word >> 34);
		while (more)
		{
			uint32_t chunk, chunk_size, type;

			if (position + 4 > names)
				return -1;
			chunk = read_u32(table + position);
			position += 4;
			more = (int)(chunk & 1);
			chunk_size = (chunk >> 1) & 0xFFFFFFu;
			type = (chunk >> 25) & 0x7Fu;
			if (chunk_size > names - position)
				return -1;
			if (type == FSB5_CHUNK_CHANNELS && chunk_size >= 1)
				stream.channels = table[position];
			else if (type == FSB5_CHUNK_RATE && chunk_size >= 4)
				stream.rate = read_u32(table + position);
			else if (type == FSB5_CHUNK_VORBIS && chunk_size >= 4)
				stream.setup_crc = read_u32(table + position);
			position += chunk_size;
		}
		if (names_size >= 4 * (size_t)count)
		{
			uint32_t name = read_u32(table + names + 4 * (size_t)index);
			size_t length = 0;

			while (name + length < names_size && length + 1 < sizeof(stream.name) && table[names + name + length])
			{
				stream.name[length] = (char)table[names + name + length];
				length++;
			}
		}
		if (written < maximum)
			streams[written] = stream;
		written++;
	}
	if (written > maximum)
		written = maximum;
	/* each stream's data runs to the next's (the bank keeps them in order), the last's to the data's end */
	for (index = 0; index < (uint32_t)written; index++)
	{
		uint64_t end = data_size;

		if (index + 1 < (uint32_t)written && streams[index + 1].offset > streams[index].offset)
			end = streams[index + 1].offset;
		streams[index].size = end > streams[index].offset ? end - streams[index].offset : 0;
		streams[index].offset += table_size;
		if (streams[index].offset + streams[index].size > file_size)
			streams[index].size = 0;
	}
	return written;
}

static int ends_with(char const *name, char const *end)
{
	size_t length = strlen(name), end_length = strlen(end);

	return length >= end_length && !strcmp(name + length - end_length, end);
}

/* the name ends in word plus digits ("lp2", "x_loop3", "piece4"), after a start or an underscore */
static int numbered(char const *name, char const *word)
{
	size_t length = strlen(name), word_length = strlen(word), digits = 0;

	while (digits < length && name[length - 1 - digits] >= '0' && name[length - 1 - digits] <= '9')
		digits++;
	if (!digits || length < digits + word_length)
		return 0;
	if (strncmp(name + length - digits - word_length, word, word_length) != 0)
		return 0;
	return length == digits + word_length || name[length - digits - word_length - 1] == '_';
}

int ae_mcc_music_role(char const *name)
{
	static char const *const whole[] = { "credits_music", "unforgotten", "new_dreamy", "edh1", "edh2",
		"opening_monks", "monks_and_fx", "reason_pad" };
	char lower[AE_MCC_MUSIC_NAME];
	size_t index;

	if (!name)
		return AE_MCC_MUSIC_ROLE_NONE;
	for (index = 0; name[index] && index + 1 < sizeof(lower); index++)
		lower[index] = (char)(name[index] >= 'A' && name[index] <= 'Z' ? name[index] - 'A' + 'a' : name[index]);
	lower[index] = 0;
	if (numbered(lower, "lp") || numbered(lower, "loop") || numbered(lower, "piece"))
		return AE_MCC_MUSIC_ROLE_LOOP;
	if (!strcmp(lower, "in") || !strcmp(lower, "longin") || ends_with(lower, "_in") || ends_with(lower, "transin"))
		return AE_MCC_MUSIC_ROLE_INTRO;
	if (!strcmp(lower, "out") || ends_with(lower, "_out"))
		return AE_MCC_MUSIC_ROLE_OUTRO;
	if (ends_with(lower, "_nl") || strstr(lower, "_music") || strstr(lower, "_mus_") || ends_with(lower, "_mus") ||
		!strncmp(lower, "music_", 6))
		return AE_MCC_MUSIC_ROLE_WHOLE;
	for (index = 0; index < sizeof(whole) / sizeof(whole[0]); index++)
		if (!strcmp(lower, whole[index]))
			return AE_MCC_MUSIC_ROLE_WHOLE;
	return AE_MCC_MUSIC_ROLE_NONE;
}

int ae_mcc_music_path_is_music(char const *path, size_t size)
{
	char lower[256];
	size_t index;
	char const *file;

	if (!path)
		return 0;
	for (index = 0; index < size && index + 1 < sizeof(lower) && path[index]; index++)
		lower[index] = (char)(path[index] >= 'A' && path[index] <= 'Z' ? path[index] - 'A' + 'a' : path[index]);
	lower[index] = 0;
	if (strstr(lower, "sound\\music\\") || strstr(lower, "\\music\\"))
		return 1;
	file = strrchr(lower, '\\');
	file = file ? file + 1 : lower;
	return strstr(lower, "sound\\cinematics\\") && strstr(file, "music");
}

int ae_mcc_music_playable(struct ae_mcc_music_stream const *stream, uint32_t const *known, int known_count)
{
	int index;

	if (!stream || stream->channels != 2 || stream->rate != AE_MCC_MUSIC_RATE || !stream->size || !stream->frames)
		return 0;
	for (index = 0; index < known_count; index++)
		if (known[index] == stream->setup_crc)
			return 1;
	return 0;
}

/* frames added to a song's, kept from wrapping (a song a day long is no song: it is not listed) */
static void song_add(struct ae_mcc_music_song *song, uint32_t frames)
{
	song->frames = frames > UINT32_MAX - song->frames ? UINT32_MAX : song->frames + frames;
}

static int song_end(struct ae_mcc_music_song *song, struct ae_mcc_music_song *songs, int count, int maximum)
{
	if (song->count && song->frames >= (uint32_t)AE_MCC_MUSIC_SHORTEST_SONG * AE_MCC_MUSIC_RATE &&
		song->frames < UINT32_MAX && count < maximum)
		songs[count++] = *song;
	song->count = 0;
	song->frames = 0;
	return count;
}

int ae_mcc_music_songs(struct ae_mcc_music_stream const *streams, int count, uint32_t const *known, int known_count,
	struct ae_mcc_music_song *songs, int maximum)
{
	struct ae_mcc_music_song song = { 0, 0, 0 };
	int index, written = 0;

	for (index = 0; index < count; index++)
	{
		int role = ae_mcc_music_playable(streams + index, known, known_count) ?
			ae_mcc_music_role(streams[index].name) : AE_MCC_MUSIC_ROLE_NONE;

		switch (role)
		{
		case AE_MCC_MUSIC_ROLE_WHOLE:
			written = song_end(&song, songs, written, maximum);
			song.first = (unsigned)index;
			song.count = 1;
			song.frames = streams[index].frames;
			written = song_end(&song, songs, written, maximum);
			break;
		case AE_MCC_MUSIC_ROLE_INTRO:
			/* (an intro opens the next song) */
			written = song_end(&song, songs, written, maximum);
			song.first = (unsigned)index;
			song.count = 1;
			song.frames = streams[index].frames;
			break;
		case AE_MCC_MUSIC_ROLE_LOOP:
			if (!song.count)
				song.first = (unsigned)index;
			song.count++;
			song_add(&song, streams[index].frames);
			break;
		case AE_MCC_MUSIC_ROLE_OUTRO:
			/* (an outro on its own is a few seconds of a song left out) */
			if (song.count)
			{
				song.count++;
				song_add(&song, streams[index].frames);
			}
			written = song_end(&song, songs, written, maximum);
			break;
		default:
			written = song_end(&song, songs, written, maximum);
			break;
		}
	}
	return song_end(&song, songs, written, maximum);
}

int ae_mcc_music_same_song(struct ae_mcc_music_stream const *a_streams, struct ae_mcc_music_song const *a,
	struct ae_mcc_music_stream const *b_streams, struct ae_mcc_music_song const *b)
{
	unsigned index;

	if (a->count != b->count || a->frames != b->frames)
		return 0;
	for (index = 0; index < a->count; index++)
		if (a_streams[a->first + index].frames != b_streams[b->first + index].frames ||
			a_streams[a->first + index].size != b_streams[b->first + index].size)
			return 0;
	return 1;
}

void ae_mcc_music_order(int *order, int count, int shuffled, uint32_t seed)
{
	uint32_t state = seed ? seed : 0x9E3779B9u;
	int index;

	for (index = 0; index < count; index++)
		order[index] = index;
	if (!shuffled)
		return;
	for (index = count - 1; index > 0; index--)
	{
		int other, swap;

		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		other = (int)(state % (uint32_t)(index + 1));
		swap = order[index];
		order[index] = order[other];
		order[other] = swap;
	}
}

/* ---------- Ogg pages */

static uint32_t ogg_crc(unsigned char const *bytes, size_t size)
{
	static uint32_t table[256];
	static int made;
	uint32_t crc = 0;
	size_t index;

	if (!made)
	{
		uint32_t entry;

		for (index = 0; index < 256; index++)
		{
			int bit;

			entry = (uint32_t)index << 24;
			for (bit = 0; bit < 8; bit++)
				entry = (entry & 0x80000000u) ? (entry << 1) ^ 0x04C11DB7u : entry << 1;
			table[index] = entry;
		}
		made = 1;
	}
	for (index = 0; index < size; index++)
		crc = (crc << 8) ^ table[((crc >> 24) ^ bytes[index]) & 0xFF];
	return crc;
}

static void write_u32(unsigned char *p, uint32_t value)
{
	p[0] = (unsigned char)value;
	p[1] = (unsigned char)(value >> 8);
	p[2] = (unsigned char)(value >> 16);
	p[3] = (unsigned char)(value >> 24);
}

struct ogg_writer
{
	unsigned char *out;
	size_t capacity, used;
	uint32_t sequence;
	int failed;
};

/* one page of the packets (count, each with its size), the last of the stream when last */
static void ogg_page(struct ogg_writer *writer, unsigned char const *const *packets, size_t const *sizes, int count,
	int64_t granule, int flags)
{
	size_t segments = 0, body = 0, header, position;
	unsigned char *page;
	int index;

	for (index = 0; index < count; index++)
	{
		segments += sizes[index] / 255 + 1;
		body += sizes[index];
	}
	header = 27 + segments;
	if (writer->failed || segments > OGG_SEGMENTS || writer->capacity - writer->used < header + body)
	{
		writer->failed = 1;
		return;
	}
	page = writer->out + writer->used;
	memcpy(page, "OggS", 4);
	page[4] = 0;
	page[5] = (unsigned char)flags;
	write_u32(page + 6, (uint32_t)(uint64_t)granule);
	write_u32(page + 10, (uint32_t)((uint64_t)granule >> 32));
	write_u32(page + 14, 1);
	write_u32(page + 18, writer->sequence++);
	write_u32(page + 22, 0);
	page[26] = (unsigned char)segments;
	position = 27;
	for (index = 0; index < count; index++)
	{
		size_t left = sizes[index];

		while (left >= 255)
		{
			page[position++] = 255;
			left -= 255;
		}
		page[position++] = (unsigned char)left;
	}
	for (index = 0; index < count; index++)
	{
		memcpy(page + position, packets[index], sizes[index]);
		position += sizes[index];
	}
	write_u32(page + 22, ogg_crc(page, position));
	writer->used += position;
}

size_t ae_mcc_music_ogg_capacity(size_t data_size, size_t setup_size)
{
	/* (the packets lose their sizes; each page adds at most 27 + 255 bytes for at least 255 bytes of packets or
	a packet of its own: half the data again is room enough, and the headers' pages) */
	return data_size + data_size / 2 + setup_size + 1024;
}

size_t ae_mcc_music_ogg(unsigned char const *data, size_t data_size, unsigned channels, unsigned rate, uint32_t frames,
	unsigned char const *setup, size_t setup_size, unsigned char *out, size_t capacity)
{
	struct ogg_writer writer;
	unsigned char identification[30], comment[18];
	unsigned char const *packets[OGG_SEGMENTS];
	size_t sizes[OGG_SEGMENTS], position = 0;
	int count = 0, segments = 0;

	if (!data || !setup || !out || !channels || channels > 255 || !rate || setup_size < 7)
		return 0;
	memset(&writer, 0, sizeof(writer));
	writer.out = out;
	writer.capacity = capacity;
	/* identification: version 0, channels, rate, no bitrates, blocks of 256 and 2048 samples (FMOD's), framing */
	memset(identification, 0, sizeof(identification));
	identification[0] = 1;
	memcpy(identification + 1, "vorbis", 6);
	identification[11] = (unsigned char)channels;
	write_u32(identification + 12, rate);
	identification[28] = (11 << 4) | 8;
	identification[29] = 1;
	/* comment: a vendor of 2 bytes, no comments, framing */
	memset(comment, 0, sizeof(comment));
	comment[0] = 3;
	memcpy(comment + 1, "vorbis", 6);
	write_u32(comment + 7, 2);
	memcpy(comment + 11, "AE", 2);
	write_u32(comment + 13, 0);
	comment[17] = 1;
	packets[0] = identification;
	sizes[0] = sizeof(identification);
	ogg_page(&writer, packets, sizes, 1, 0, 2);
	packets[0] = comment;
	sizes[0] = sizeof(comment);
	packets[1] = setup;
	sizes[1] = setup_size;
	if (setup_size > OGG_LARGEST_PACKET - 255)
		return 0;
	ogg_page(&writer, packets, sizes, 2, 0, 0);
	while (position + 2 <= data_size)
	{
		size_t size = (size_t)data[position] | ((size_t)data[position + 1] << 8);
		int packet_segments;

		if (!size || size == 0xFFFF)
			break;
		if (size > OGG_LARGEST_PACKET || size > data_size - position - 2)
			return 0;
		packet_segments = (int)(size / 255 + 1);
		if (count == OGG_SEGMENTS || segments + packet_segments > OGG_SEGMENTS)
		{
			ogg_page(&writer, packets, sizes, count, -1, 0);
			count = 0;
			segments = 0;
		}
		packets[count] = data + position + 2;
		sizes[count] = size;
		count++;
		segments += packet_segments;
		position += 2 + size;
	}
	if (!count)
		return 0;
	ogg_page(&writer, packets, sizes, count, (int64_t)frames, 4);
	return writer.failed ? 0 : writer.used;
}
