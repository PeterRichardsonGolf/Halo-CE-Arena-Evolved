#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_mcc_music_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define CRC 0x632B1FACu

static void put32(unsigned char *p, uint32_t v)
{
	p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}

/* a version 1 Vorbis bank: names[i], stereo 48 kHz (rate index 9, channels bit 1), frames[i], data sizes[i] (each a
multiple of 32), every stream with a Vorbis chunk naming CRC */
static size_t make_bank(unsigned char *bank, size_t capacity, char const *const *names, uint32_t const *frames,
	uint32_t const *sizes, int count, size_t *table)
{
	size_t streams_size = (size_t)count * (8 + 4 + 4), names_size = 0, position, data = 0;
	int index;

	for (index = 0; index < count; index++)
		names_size += strlen(names[index]) + 1;
	names_size = 4 * (size_t)count + names_size;
	names_size = (names_size + 31) & ~(size_t)31;
	for (index = 0; index < count; index++)
		data += sizes[index];
	*table = 60 + streams_size + names_size;
	if (*table + data > capacity)
		return 0;
	memset(bank, 0, *table + data);
	memcpy(bank, "FSB5", 4);
	put32(bank + 4, 1);
	put32(bank + 8, (uint32_t)count);
	put32(bank + 12, (uint32_t)streams_size);
	put32(bank + 16, (uint32_t)names_size);
	put32(bank + 20, (uint32_t)data);
	put32(bank + 24, 15);
	position = 60;
	data = 0;
	for (index = 0; index < count; index++)
	{
		uint64_t word = 1 | (9u << 1) | (1u << 5) | ((uint64_t)(data / 32) << 7) | ((uint64_t)frames[index] << 34);

		put32(bank + position, (uint32_t)word);
		put32(bank + position + 4, (uint32_t)(word >> 32));
		/* one chunk, the last: type 11, 4 bytes */
		put32(bank + position + 8, (11u << 25) | (4u << 1));
		put32(bank + position + 12, CRC);
		position += 16;
		data += sizes[index];
	}
	{
		size_t names_at = 60 + streams_size, text = 4 * (size_t)count;

		for (index = 0; index < count; index++)
		{
			put32(bank + names_at + 4 * (size_t)index, (uint32_t)text);
			memcpy(bank + names_at + text, names[index], strlen(names[index]) + 1);
			text += strlen(names[index]) + 1;
		}
	}
	return *table + data;
}

static void test_roles(void)
{
	CHECK(ae_mcc_music_role("in") == AE_MCC_MUSIC_ROLE_INTRO);
	CHECK(ae_mcc_music_role("perc3_pulse_in") == AE_MCC_MUSIC_ROLE_INTRO);
	CHECK(ae_mcc_music_role("longin") == AE_MCC_MUSIC_ROLE_INTRO);
	CHECK(ae_mcc_music_role("vari1a_groove1_alttransin") == AE_MCC_MUSIC_ROLE_INTRO);
	CHECK(ae_mcc_music_role("lp1") == AE_MCC_MUSIC_ROLE_LOOP);
	CHECK(ae_mcc_music_role("loop3") == AE_MCC_MUSIC_ROLE_LOOP);
	CHECK(ae_mcc_music_role("piece5") == AE_MCC_MUSIC_ROLE_LOOP);
	CHECK(ae_mcc_music_role("ar6_sax1_lp7") == AE_MCC_MUSIC_ROLE_LOOP);
	CHECK(ae_mcc_music_role("out") == AE_MCC_MUSIC_ROLE_OUTRO);
	CHECK(ae_mcc_music_role("new_alt_out") == AE_MCC_MUSIC_ROLE_OUTRO);
	CHECK(ae_mcc_music_role("5_full_nl") == AE_MCC_MUSIC_ROLE_WHOLE);
	CHECK(ae_mcc_music_role("m70_credits_music") == AE_MCC_MUSIC_ROLE_WHOLE);
	CHECK(ae_mcc_music_role("Unforgotten") == AE_MCC_MUSIC_ROLE_WHOLE);
	/* not music: ambience and foley, a word that only ends in digits after something else */
	CHECK(ae_mcc_music_role("amb_wind_mountains_loop") == AE_MCC_MUSIC_ROLE_NONE);
	CHECK(ae_mcc_music_role("010la_foley_1_3") == AE_MCC_MUSIC_ROLE_NONE);
	CHECK(ae_mcc_music_role("help1") == AE_MCC_MUSIC_ROLE_NONE);
	CHECK(ae_mcc_music_role("pin") == AE_MCC_MUSIC_ROLE_NONE);
	CHECK(ae_mcc_music_role("") == AE_MCC_MUSIC_ROLE_NONE);
	CHECK(ae_mcc_music_role(NULL) == AE_MCC_MUSIC_ROLE_NONE);
}

static void test_bank_and_songs(void)
{
	/* a 40 s song in 3 pieces, ambience, a 20 s piece alone (too short), a 60 s whole one, a 35 s song of loops */
	static char const *const names[] = { "in", "lp1", "out", "amb_rain_loop", "edh1", "x20_nl", "loop1", "loop2" };
	static uint32_t const frames[] = { 10 * 48000, 20 * 48000, 10 * 48000, 50 * 48000, 20 * 48000, 60 * 48000,
		20 * 48000, 15 * 48000 };
	static uint32_t const sizes[] = { 64, 96, 32, 32, 64, 128, 64, 32 };
	static uint32_t const known[] = { CRC };
	static unsigned char bank[4096];
	struct ae_mcc_music_stream streams[16];
	struct ae_mcc_music_song songs[8];
	size_t table, size = make_bank(bank, sizeof(bank), names, frames, sizes, 8, &table);
	int count, song_count;

	CHECK(size != 0);
	CHECK(ae_mcc_music_fsb5_table_size(bank, size) == table);
	count = ae_mcc_music_fsb5_streams(bank, table, size, streams, 16);
	CHECK(count == 8);
	CHECK(!strcmp(streams[0].name, "in") && !strcmp(streams[7].name, "loop2"));
	CHECK(streams[1].channels == 2 && streams[1].rate == 48000 && streams[1].setup_crc == CRC);
	CHECK(streams[1].frames == 20 * 48000);
	CHECK(streams[0].offset == table && streams[0].size == 64);
	CHECK(streams[1].offset == table + 64 && streams[1].size == 96);
	CHECK(streams[7].offset == table + size - table - 32 && streams[7].size == 32);
	/* a bank cut short: the streams past its end have no data */
	CHECK(ae_mcc_music_fsb5_streams(bank, table, size - 16, streams, 16) == 8 && streams[7].size == 0);
	ae_mcc_music_fsb5_streams(bank, table, size, streams, 16);
	song_count = ae_mcc_music_songs(streams, count, known, 1, songs, 8);
	CHECK(song_count == 3);
	CHECK(songs[0].first == 0 && songs[0].count == 3 && songs[0].frames == 40 * 48000);
	CHECK(songs[1].first == 5 && songs[1].count == 1);
	CHECK(songs[2].first == 6 && songs[2].count == 2 && songs[2].frames == 35 * 48000);
	/* an unknown setup header: nothing plays */
	{
		uint32_t other = 1;

		CHECK(ae_mcc_music_songs(streams, count, &other, 1, songs, 8) == 0);
	}
	/* a mono stream is not used */
	streams[5].channels = 1;
	CHECK(ae_mcc_music_songs(streams, count, known, 1, songs, 8) == 2);
	streams[5].channels = 2;
	/* the same song in another bank */
	{
		struct ae_mcc_music_song a = songs[0], b = songs[0];

		CHECK(ae_mcc_music_same_song(streams, &a, streams, &b));
		b.first = 6;
		b.count = 2;
		b.frames = 35 * 48000;
		CHECK(!ae_mcc_music_same_song(streams, &a, streams, &b));
	}
	/* damaged tables: not a bank, not Vorbis, a stream table past its end */
	CHECK(ae_mcc_music_fsb5_table_size((unsigned char const *)"FSB4", 4) == 0);
	bank[24] = 2;
	CHECK(ae_mcc_music_fsb5_table_size(bank, size) == 0);
	bank[24] = 15;
	CHECK(ae_mcc_music_fsb5_streams(bank, table - 1, size, streams, 16) == -1);
	put32(bank + 8, 1000);
	CHECK(ae_mcc_music_fsb5_streams(bank, table, size, streams, 16) == -1);
}

/* a run of pieces whose lengths would wrap 32 bits is no song */
static void test_song_length_wraps(void)
{
	static uint32_t const known[] = { CRC };
	struct ae_mcc_music_stream streams[2];
	struct ae_mcc_music_song songs[2];
	int index;

	memset(streams, 0, sizeof(streams));
	for (index = 0; index < 2; index++)
	{
		snprintf(streams[index].name, sizeof(streams[index].name), "lp%d", index + 1);
		streams[index].channels = 2;
		streams[index].rate = 48000;
		streams[index].frames = 0xF0000000u;
		streams[index].setup_crc = CRC;
		streams[index].size = 32;
	}
	CHECK(ae_mcc_music_songs(streams, 2, known, 1, songs, 2) == 0);
	CHECK(ae_mcc_music_songs(streams, 1, known, 1, songs, 2) == 1);
}

static void test_music_paths(void)
{
	CHECK(ae_mcc_music_path_is_music("data\\sound\\music\\end_credits\\credits_music.aif", 256));
	CHECK(ae_mcc_music_path_is_music("DATA\\SOUND\\MUSIC\\misc\\bia_nl\\track1\\loop\\bia_nl.aif", 256));
	CHECK(ae_mcc_music_path_is_music("data\\sound\\cinematics\\m70\\m70_carter\\music\\m70_carter_music.aif", 256));
	CHECK(ae_mcc_music_path_is_music("data\\sound\\cinematics\\m52\\m52_reflection\\m52_reflection_music_fr0.aif", 256));
	CHECK(!ae_mcc_music_path_is_music("data\\sound\\levels\\atlas\\construct\\construct_outside_lp1.aif", 256));
	CHECK(!ae_mcc_music_path_is_music("data\\sound\\cinematics\\m70\\m70_carter\\foley\\m70_carter_foley.aif", 256));
	CHECK(!ae_mcc_music_path_is_music("data\\sound\\music", 10));
	CHECK(!ae_mcc_music_path_is_music("", 256));
	CHECK(!ae_mcc_music_path_is_music(NULL, 0));
}

static void test_order(void)
{
	int order[50], seen[50], index, moved = 0;

	ae_mcc_music_order(order, 50, 0, 7);
	for (index = 0; index < 50; index++)
		CHECK(order[index] == index);
	ae_mcc_music_order(order, 50, 1, 7);
	memset(seen, 0, sizeof(seen));
	for (index = 0; index < 50; index++)
	{
		CHECK(order[index] >= 0 && order[index] < 50);
		if (order[index] >= 0 && order[index] < 50)
			seen[order[index]]++;
		moved += order[index] != index;
	}
	for (index = 0; index < 50; index++)
		CHECK(seen[index] == 1);
	CHECK(moved > 25);
	ae_mcc_music_order(order, 0, 1, 7);
}

static uint32_t read32(unsigned char const *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void test_ogg(void)
{
	static unsigned char setup[100], data[2000], out[8192];
	size_t size, position = 0, written;
	int pages = 0, packets = 0, last_flags = 0;
	uint32_t last_granule = 0;

	memset(setup, 0, sizeof(setup));
	setup[0] = 5;
	memcpy(setup + 1, "vorbis", 6);
	/* 3 packets of 300, 10, 600 bytes, then the end mark */
	data[position++] = 300 & 255; data[position++] = 300 >> 8; position += 300;
	data[position++] = 10; data[position++] = 0; position += 10;
	data[position++] = 600 & 255; data[position++] = 600 >> 8; position += 600;
	data[position++] = 0; data[position++] = 0;
	size = ae_mcc_music_ogg(data, sizeof(data), 2, 48000, 12345, setup, sizeof(setup), out, sizeof(out));
	CHECK(size > 0 && size <= ae_mcc_music_ogg_capacity(sizeof(data), sizeof(setup)));
	for (written = 0; written + 27 <= size; pages++)
	{
		unsigned char const *page = out + written;
		size_t segments = page[26], body = 0, index;

		CHECK(!memcmp(page, "OggS", 4));
		for (index = 0; index < segments; index++)
		{
			body += page[27 + index];
			packets += page[27 + index] < 255;
		}
		last_flags = page[5];
		last_granule = read32(page + 6);
		written += 27 + segments + body;
	}
	CHECK(written == size);
	CHECK(pages == 3);
	CHECK(packets == 6); /* identification, comment, setup, and the three */
	CHECK(last_flags == 4 && last_granule == 12345);
	CHECK(out[5] == 2 && out[28] == 1 && !memcmp(out + 29, "vorbis", 6));
	/* too little room, a packet past the data, no packets */
	CHECK(ae_mcc_music_ogg(data, sizeof(data), 2, 48000, 1, setup, sizeof(setup), out, 200) == 0);
	data[0] = 0xFE; data[1] = 0xFF;
	CHECK(ae_mcc_music_ogg(data, sizeof(data), 2, 48000, 1, setup, sizeof(setup), out, sizeof(out)) == 0);
	data[0] = 0; data[1] = 0;
	CHECK(ae_mcc_music_ogg(data, sizeof(data), 2, 48000, 1, setup, sizeof(setup), out, sizeof(out)) == 0);
}

int main(void)
{
	test_roles();
	test_bank_and_songs();
	test_song_length_wraps();
	test_music_paths();
	test_order();
	test_ogg();
	if (failures)
	{
		printf("ae_mcc_music_rules_test: %d failed\n", failures);
		return 1;
	}
	printf("ae_mcc_music_rules_test: ok\n");
	return 0;
}
