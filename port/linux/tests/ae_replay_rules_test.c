#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_replay_rules.h"

static int failures;
static long checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define NETWORK 44UL
#define MB (1024ULL * 1024ULL)

static unsigned long seed = 12345;
static unsigned char next_byte(void)
{
	seed = seed * 1103515245UL + 12345UL;
	return (unsigned char)((seed >> 16) & 0xFF);
}

static void put_long(unsigned char *at, unsigned long value)
{
	at[0] = (unsigned char)value;
	at[1] = (unsigned char)(value >> 8);
	at[2] = (unsigned char)(value >> 16);
	at[3] = (unsigned char)(value >> 24);
}

static void good_header(unsigned char *bytes)
{
	struct ae_replay_header header;

	memset(&header, 0, sizeof header);
	header.format = AE_REPLAY_FORMAT;
	header.network_version = NETWORK;
	header.build = 7;
	header.ticks_per_second = AE_REPLAY_TICKS_PER_SECOND;
	header.start_tick = 3;
	header.time = 0x123456789ULL;
	strcpy(header.map, "levels\\test\\bloodgulch\\bloodgulch");
	strcpy(header.variant, "AE Slayer");
	strcpy(header.version, "1.0.0-rc2");
	header.length_ticks = 3600;
	strcpy(header.server, "server \xC3\xA9");
	strcpy(header.player, "player");
	ae_replay_header_write(bytes, &header);
}

/* every value of a 32-bit field at offset: only `good` reads OK, and the answer is always one of the enum's */
static void field_sweep(int offset, unsigned long good, int bad_answer)
{
	static const unsigned long values[] = { 0, 1, 2, 4, 5, 6, 29, 30, 31, 383, 384, 385, 0x7FFFFFFF, 0x80000000UL,
		0xFFFFFFFFUL, AE_REPLAY_LONGEST_TICKS, AE_REPLAY_LONGEST_TICKS + 1, 44, 43, 45 };
	unsigned char bytes[AE_REPLAY_HEADER_SIZE];
	size_t index;

	for (index = 0; index < sizeof values / sizeof values[0]; index++)
	{
		int answer;

		good_header(bytes);
		put_long(bytes + offset, values[index]);
		answer = ae_replay_header_read(bytes, sizeof bytes, NETWORK, NULL, NULL, 0);
		if (values[index] == good)
			CHECK(answer == AE_REPLAY_HEADER_OK);
		else if (offset == AE_REPLAY_LENGTH_OFFSET)
			CHECK(values[index] <= AE_REPLAY_LONGEST_TICKS ? answer == AE_REPLAY_HEADER_OK : answer == bad_answer);
		else if (offset == AE_REPLAY_FLAGS_OFFSET)
			CHECK(values[index] <= 1 ? answer == AE_REPLAY_HEADER_OK : answer == bad_answer);
		else
			CHECK(answer == bad_answer);
	}
}

/* a text field with no end within its bytes, and a control byte inside it */
static void text_sweep(int offset, int size, int ascii)
{
	unsigned char bytes[AE_REPLAY_HEADER_SIZE];
	char why[64];

	good_header(bytes);
	memset(bytes + offset, 'a', (size_t)size);
	CHECK(ae_replay_header_read(bytes, sizeof bytes, NETWORK, NULL, why, sizeof why) == AE_REPLAY_HEADER_DAMAGED);
	CHECK(!strcmp(why, "its header is damaged"));
	good_header(bytes);
	bytes[offset] = 'a';
	bytes[offset + 1] = 0x07;
	CHECK(ae_replay_header_read(bytes, sizeof bytes, NETWORK, NULL, NULL, 0) == AE_REPLAY_HEADER_DAMAGED);
	good_header(bytes);
	memset(bytes + offset, 0, (size_t)size);
	bytes[offset + 2] = 'x';
	CHECK(ae_replay_header_read(bytes, sizeof bytes, NETWORK, NULL, NULL, 0) == AE_REPLAY_HEADER_DAMAGED);
	good_header(bytes);
	memset(bytes + offset, 0, (size_t)size);
	bytes[offset] = (unsigned char)0xC3;
	bytes[offset + 1] = (unsigned char)0xA9;
	CHECK(ae_replay_header_read(bytes, sizeof bytes, NETWORK, NULL, NULL, 0) ==
		(ascii ? AE_REPLAY_HEADER_DAMAGED : AE_REPLAY_HEADER_OK));
}

static void test_header(void)
{
	unsigned char bytes[AE_REPLAY_HEADER_SIZE];
	struct ae_replay_header header;
	char why[64];
	size_t size;
	int round;

	/* a header written reads back as it was */
	good_header(bytes);
	CHECK(ae_replay_header_read(bytes, sizeof bytes, NETWORK, &header, why, sizeof why) == AE_REPLAY_HEADER_OK);
	CHECK(header.build == 7 && header.start_tick == 3 && header.time == 0x123456789ULL);
	CHECK(!strcmp(header.map, "levels\\test\\bloodgulch\\bloodgulch") && !strcmp(header.variant, "AE Slayer"));
	CHECK(!strcmp(header.version, "1.0.0-rc2") && header.length_ticks == 3600 && header.flags == 0);
	CHECK(!strcmp(header.server, "server \xC3\xA9") && !strcmp(header.player, "player") && !why[0]);
	/* every length short of the header: not a replay */
	for (size = 0; size < AE_REPLAY_HEADER_SIZE; size++)
		CHECK(ae_replay_header_read(bytes, size, NETWORK, &header, NULL, 0) == AE_REPLAY_HEADER_NOT_REPLAY);
	CHECK(ae_replay_header_read(NULL, 4096, NETWORK, &header, why, sizeof why) == AE_REPLAY_HEADER_NOT_REPLAY);
	CHECK(!strcmp(why, "not a replay"));
	bytes[0] = 'X';
	CHECK(ae_replay_header_read(bytes, sizeof bytes, NETWORK, &header, NULL, 0) == AE_REPLAY_HEADER_NOT_REPLAY);
	/* each count and length field over its values */
	field_sweep(8, AE_REPLAY_FORMAT, AE_REPLAY_HEADER_OTHER_FORMAT);
	field_sweep(12, NETWORK, AE_REPLAY_HEADER_OTHER_NETWORK);
	field_sweep(20, AE_REPLAY_TICKS_PER_SECOND, AE_REPLAY_HEADER_DAMAGED);
	field_sweep(28, AE_REPLAY_HEADER_SIZE, AE_REPLAY_HEADER_DAMAGED);
	field_sweep(AE_REPLAY_LENGTH_OFFSET, 3600, AE_REPLAY_HEADER_DAMAGED);
	field_sweep(AE_REPLAY_FLAGS_OFFSET, 0, AE_REPLAY_HEADER_DAMAGED);
	good_header(bytes);
	put_long(bytes + 8, 6);
	ae_replay_header_read(bytes, sizeof bytes, NETWORK, NULL, why, sizeof why);
	CHECK(!strcmp(why, "made by a later build"));
	put_long(bytes + 8, 4);
	ae_replay_header_read(bytes, sizeof bytes, NETWORK, NULL, why, sizeof why);
	CHECK(!strcmp(why, "made by an earlier build"));
	/* each text field: no end, a control byte, bytes after its end; UTF-8 only in the names */
	text_sweep(AE_REPLAY_MAP_OFFSET, AE_REPLAY_MAP_SIZE, 1);
	text_sweep(AE_REPLAY_VARIANT_OFFSET, AE_REPLAY_VARIANT_SIZE, 1);
	text_sweep(AE_REPLAY_VERSION_OFFSET, AE_REPLAY_VERSION_SIZE, 1);
	text_sweep(AE_REPLAY_SERVER_OFFSET, AE_REPLAY_SERVER_SIZE, 0);
	text_sweep(AE_REPLAY_PLAYER_OFFSET, AE_REPLAY_PLAYER_SIZE, 0);
	/* no map: damaged */
	good_header(bytes);
	memset(bytes + AE_REPLAY_MAP_OFFSET, 0, AE_REPLAY_MAP_SIZE);
	CHECK(ae_replay_header_read(bytes, sizeof bytes, NETWORK, NULL, NULL, 0) == AE_REPLAY_HEADER_DAMAGED);
	/* written text too long or unprintable is cut and cleaned, and still reads */
	memset(&header, 0, sizeof header);
	header.format = AE_REPLAY_FORMAT;
	header.network_version = NETWORK;
	header.ticks_per_second = AE_REPLAY_TICKS_PER_SECOND;
	memset(header.map, 'm', AE_REPLAY_MAP_SIZE);
	header.variant[0] = 0x01;
	header.variant[1] = 'v';
	ae_replay_header_write(bytes, &header);
	CHECK(ae_replay_header_read(bytes, sizeof bytes, NETWORK, &header, NULL, 0) == AE_REPLAY_HEADER_OK);
	CHECK(strlen(header.map) == AE_REPLAY_MAP_SIZE - 1 && !strcmp(header.variant, "_v"));
	/* random bytes and random damage to a good header: an answer of the enum, the texts ended */
	for (round = 0; round < 20000; round++)
	{
		int answer, flips;

		if (round & 1)
		{
			for (size = 0; size < sizeof bytes; size++)
				bytes[size] = next_byte();
			memcpy(bytes, AE_REPLAY_MAGIC, 8);
		}
		else
		{
			good_header(bytes);
			for (flips = 0; flips < 1 + (int)(next_byte() % 8); flips++)
				bytes[8 + (next_byte() | (next_byte() << 8)) % (AE_REPLAY_HEADER_SIZE - 8)] = next_byte();
		}
		answer = ae_replay_header_read(bytes, sizeof bytes, NETWORK, &header, why, sizeof why);
		CHECK(answer >= AE_REPLAY_HEADER_OK && answer <= AE_REPLAY_HEADER_DAMAGED);
		CHECK(strlen(header.map) <= AE_REPLAY_MAP_SIZE && strlen(header.player) <= AE_REPLAY_PLAYER_SIZE);
		CHECK(answer != AE_REPLAY_HEADER_OK || (header.length_ticks <= AE_REPLAY_LONGEST_TICKS && header.map[0]));
	}
}

static void test_records(void)
{
	unsigned char bytes[AE_REPLAY_RECORD_HEADER_SIZE + 300];
	struct ae_replay_record record;
	size_t size;
	int kind, round;

	CHECK(ae_replay_record_header_write(bytes, AE_REPLAY_RECORD_DATAGRAM, 300, 1000));
	CHECK(ae_replay_record_take(bytes, sizeof bytes, 0, 2000, &record) == AE_REPLAY_RECORD_OK);
	CHECK(record.kind == AE_REPLAY_RECORD_DATAGRAM && record.size == 300 && record.tick == 1000);
	/* every length short of the record: more to come */
	for (size = 0; size < sizeof bytes; size++)
		CHECK(ae_replay_record_take(bytes, size, 0, 2000, &record) == AE_REPLAY_RECORD_NEED_MORE);
	CHECK(ae_replay_record_take(NULL, 100, 0, 2000, &record) == AE_REPLAY_RECORD_NEED_MORE);
	/* every kind: only the known ones */
	for (kind = 0; kind < 256; kind++)
	{
		bytes[0] = (unsigned char)kind;
		CHECK(ae_replay_record_take(bytes, sizeof bytes, 0, 2000, NULL) ==
			(kind >= AE_REPLAY_RECORD_DATAGRAM && kind < AE_REPLAY_RECORD_KINDS ? AE_REPLAY_RECORD_OK :
				AE_REPLAY_RECORD_BAD));
	}
	bytes[0] = AE_REPLAY_RECORD_DATAGRAM;
	/* the byte left 0 */
	bytes[1] = 1;
	CHECK(ae_replay_record_take(bytes, sizeof bytes, 0, 2000, NULL) == AE_REPLAY_RECORD_BAD);
	bytes[1] = 0;
	/* ticks: not before the last, not past the most */
	CHECK(ae_replay_record_take(bytes, sizeof bytes, 1000, 1000, NULL) == AE_REPLAY_RECORD_OK);
	CHECK(ae_replay_record_take(bytes, sizeof bytes, 1001, 2000, NULL) == AE_REPLAY_RECORD_BAD);
	CHECK(ae_replay_record_take(bytes, sizeof bytes, 0, 999, NULL) == AE_REPLAY_RECORD_BAD);
	/* the size field over what is left, at its most */
	bytes[2] = 0xFF;
	bytes[3] = 0xFF;
	CHECK(ae_replay_record_take(bytes, sizeof bytes, 0, 2000, NULL) == AE_REPLAY_RECORD_NEED_MORE);
	/* writing: kinds and sizes out of bounds refused */
	CHECK(!ae_replay_record_header_write(bytes, 0, 1, 1));
	CHECK(!ae_replay_record_header_write(bytes, AE_REPLAY_RECORD_KINDS, 1, 1));
	CHECK(!ae_replay_record_header_write(bytes, AE_REPLAY_RECORD_GAME, AE_REPLAY_RECORD_MOST + 1, 1));
	CHECK(ae_replay_record_header_write(bytes, AE_REPLAY_RECORD_GAME, AE_REPLAY_RECORD_MOST, 0xFFFFFFFFUL));
	CHECK(ae_replay_record_take(bytes, AE_REPLAY_RECORD_HEADER_SIZE + AE_REPLAY_RECORD_MOST - 1, 0, 0xFFFFFFFFUL,
		&record) == AE_REPLAY_RECORD_NEED_MORE);
	/* random bytes: an answer of the enum, and an OK record fits what is left */
	for (round = 0; round < 50000; round++)
	{
		int answer;
		size_t length = next_byte() % sizeof bytes;

		for (size = 0; size < length; size++)
			bytes[size] = next_byte();
		answer = ae_replay_record_take(bytes, length, 0, 0xFFFFFFFFUL, &record);
		CHECK(answer >= AE_REPLAY_RECORD_OK && answer <= AE_REPLAY_RECORD_BAD);
		CHECK(answer != AE_REPLAY_RECORD_OK || record.size + AE_REPLAY_RECORD_HEADER_SIZE <= length);
	}
}

/* a stream of records into bytes: kind, size, tick each */
static size_t stream_put(unsigned char *bytes, const int (*records)[3], int count)
{
	size_t at = 0;
	int index;

	for (index = 0; index < count; index++)
	{
		ae_replay_record_header_write(bytes + at, records[index][0], (unsigned long)records[index][1],
			(unsigned long)records[index][2]);
		memset(bytes + at + AE_REPLAY_RECORD_HEADER_SIZE, 0x11, (size_t)records[index][1]);
		at += AE_REPLAY_RECORD_HEADER_SIZE + (size_t)records[index][1];
	}
	return at;
}

#define GAME 100
#define SETUP 80

static int stream_ok(const int (*records)[3], int count, char *why, struct ae_replay_stream *stream)
{
	static unsigned char bytes[32768];
	size_t size = stream_put(bytes, records, count);

	return ae_replay_stream_check(bytes, size, 50, GAME, SETUP, 2, 1200, 0x1000, stream, why, 64);
}

static void test_stream(void)
{
	static const int good[][3] = { { 5, GAME, 50 }, { 7, SETUP, 50 }, { 1, 40, 51 }, { 2, 10, 51 }, { 4, 6, 60 },
		{ 1, 40, 61 } };
	static const int no_setup[][3] = { { 5, GAME, 50 }, { 1, 40, 51 } };
	static const int no_game[][3] = { { 1, 40, 51 }, { 5, GAME, 52 } };
	static const int small_game[][3] = { { 5, GAME - 1, 50 }, { 1, 40, 51 } };
	static const int two_games[][3] = { { 5, GAME, 50 }, { 1, 40, 51 }, { 5, GAME, 52 } };
	static const int late_setup[][3] = { { 5, GAME, 50 }, { 1, 40, 51 }, { 7, SETUP, 52 } };
	static const int odd_setup[][3] = { { 5, GAME, 50 }, { 7, SETUP + 1, 50 }, { 1, 40, 51 } };
	static const int short_message[][3] = { { 5, GAME, 50 }, { 1, 1, 51 } };
	static const int backwards[][3] = { { 5, GAME, 50 }, { 1, 40, 60 }, { 1, 40, 59 } };
	static const int early[][3] = { { 5, GAME, 49 }, { 1, 40, 51 } };
	static const int no_datagram[][3] = { { 5, GAME, 50 }, { 2, 10, 51 } };
	static unsigned char bytes[8192];
	struct ae_replay_stream stream;
	char why[64];
	size_t size, cut;
	int round;

	CHECK(stream_ok(good, 6, why, &stream) && stream.records == 6 && stream.kinds[1] == 2 && stream.last_tick == 61);
	CHECK(stream.game_at == AE_REPLAY_RECORD_HEADER_SIZE && stream.play_from == 2 * AE_REPLAY_RECORD_HEADER_SIZE + GAME + SETUP);
	CHECK(!stream.cut && stream.first_tick == 50);
	CHECK(stream_ok(no_setup, 2, why, &stream) && stream.play_from == AE_REPLAY_RECORD_HEADER_SIZE + GAME);
	CHECK(!stream_ok(no_game, 2, why, NULL) && !strcmp(why, "it has no game to play"));
	CHECK(!stream_ok(small_game, 2, why, NULL) && !strcmp(why, "its game was recorded by another build"));
	CHECK(!stream_ok(two_games, 3, why, NULL) && !strcmp(why, "it has a second game"));
	CHECK(!stream_ok(late_setup, 3, why, NULL) && !strcmp(why, "its level setup is damaged"));
	CHECK(!stream_ok(odd_setup, 3, why, NULL) && !strcmp(why, "its level setup is damaged"));
	CHECK(!stream_ok(short_message, 2, why, NULL) && !strcmp(why, "a record of it is damaged"));
	CHECK(!stream_ok(backwards, 3, why, NULL) && !strcmp(why, "a record of it is damaged"));
	CHECK(!stream_ok(early, 2, why, NULL));
	CHECK(!stream_ok(no_datagram, 2, why, NULL) && !strcmp(why, "it has nothing of the game to show"));
	CHECK(!ae_replay_stream_check(NULL, 10, 0, GAME, SETUP, 2, 1200, 0x1000, &stream, why, sizeof why));
	CHECK(!ae_replay_stream_check(bytes, AE_REPLAY_INFLATED_MOST + 1, 0, GAME, SETUP, 2, 1200, 0x1000, &stream, why, sizeof why) &&
		!strcmp(why, "it is too large to play"));
	/* every cut of a good stream: refused before its first datagram is whole, else played to its last whole record */
	size = stream_put(bytes, good, 6);
	for (cut = 0; cut <= size; cut++)
	{
		int answer = ae_replay_stream_check(bytes, cut, 50, GAME, SETUP, 2, 1200, 0x1000, &stream, why, sizeof why);
		size_t first_datagram = 3 * AE_REPLAY_RECORD_HEADER_SIZE + GAME + SETUP + 40;

		CHECK(answer == (cut >= first_datagram));
		CHECK(!answer || (stream.end <= cut && stream.cut == (stream.end < cut)));
	}
	/* random damage to a good stream: an answer either way, and an OK one never past the bytes */
	for (round = 0; round < 30000; round++)
	{
		int flips;

		size = stream_put(bytes, good, 6);
		for (flips = 0; flips < 1 + (int)(next_byte() % 4); flips++)
			bytes[(next_byte() | (next_byte() << 8)) % size] = next_byte();
		if (ae_replay_stream_check(bytes, size, 50, GAME, SETUP, 2, 1200, 0x1000, &stream, why, sizeof why))
			CHECK(stream.end <= size && stream.play_from <= stream.end && stream.game_at < stream.end);
	}
}

static void test_names(void)
{
	char name[AE_REPLAY_NAME_SIZE];
	char map[200];

	CHECK(ae_replay_name_make(name, sizeof name, "20261118-183000", "bloodgulch") &&
		!strcmp(name, "20261118-183000_bloodgulch.ocreplay"));
	CHECK(ae_replay_name_is_own(name) && ae_replay_name_is_safe(name));
	CHECK(ae_replay_name_make(name, sizeof name, "20261118-183000", "../x y") &&
		!strcmp(name, "20261118-183000____x_y.ocreplay"));
	CHECK(ae_replay_name_is_safe(name) && ae_replay_name_is_own(name));
	CHECK(!ae_replay_name_is_safe("20261118-183000_..ocreplay") && !ae_replay_name_is_own("20261118-183000_..ocreplay"));
	CHECK(ae_replay_name_make(name, sizeof name, "20261118-183000", "") &&
		!strcmp(name, "20261118-183000_map.ocreplay"));
	CHECK(ae_replay_name_make(name, sizeof name, "20261118-183000", NULL) && ae_replay_name_is_own(name));
	CHECK(!ae_replay_name_make(name, sizeof name, "2026111-183000", "a"));
	CHECK(!ae_replay_name_make(name, sizeof name, "20261118-183000x", "a"));
	CHECK(!ae_replay_name_make(name, 20, "20261118-183000", "bloodgulch"));
	memset(map, 'a', sizeof map - 1);
	map[sizeof map - 1] = 0;
	CHECK(!ae_replay_name_make(name, sizeof name, "20261118-183000", map) && !name[0]);
	CHECK(!ae_replay_name_is_own("notes.txt") && !ae_replay_name_is_own("20261118-183000_a.txt"));
	CHECK(!ae_replay_name_is_own("20261118-183000.ocreplay") && !ae_replay_name_is_own("x_bloodgulch.ocreplay"));
	CHECK(ae_replay_name_is_safe("mine.ocreplay") && !ae_replay_name_is_own("mine.ocreplay"));
	CHECK(!ae_replay_name_is_safe("a/b.ocreplay") && !ae_replay_name_is_safe("a\\b.ocreplay"));
	CHECK(!ae_replay_name_is_safe(".ocreplay") && !ae_replay_name_is_safe(NULL) && !ae_replay_name_is_safe(""));
	CHECK(ae_replay_kept(0) == 1 && ae_replay_kept(30) == 30 && ae_replay_kept(9999) == 500);
	CHECK(ae_replay_budget_mb(-5) == 100 && ae_replay_budget_mb(2000) == 2000 && ae_replay_budget_mb(1L << 30) == 100000);
}

static void test_caps(void)
{
	struct ae_replay_file files[6];
	int picked[6], count;

	files[0].name = "20261118-100000_a.ocreplay";
	files[0].bytes = 100 * MB;
	files[1].name = "notes.txt";
	files[1].bytes = 5000 * MB;
	files[2].name = "20261117-090000_b.ocreplay";
	files[2].bytes = 100 * MB;
	files[3].name = "20261118-120000_c.ocreplay";
	files[3].bytes = 100 * MB;
	files[4].name = "mine.ocreplay";
	files[4].bytes = 5000 * MB;
	files[5].name = "20261116-080000_d.ocreplay";
	files[5].bytes = 100 * MB;
	/* under both caps: none */
	CHECK(ae_replay_pick_removals(files, 6, 30, 2000 * MB, 10 * MB, picked, 6) == 0);
	/* the count: 4 own, 3 kept with the new one, so the 2 oldest go, oldest first; others never */
	count = ae_replay_pick_removals(files, 6, 3, 2000 * MB, 10 * MB, picked, 6);
	CHECK(count == 2 && picked[0] == 5 && picked[1] == 2);
	/* the size: 400 MB own + 150 MB new under 400: the oldest two (450 is reached with one) */
	CHECK(ae_replay_pick_removals(files, 6, 30, 450 * MB, 150 * MB, picked, 6) == 1 && picked[0] == 5);
	count = ae_replay_pick_removals(files, 6, 30, 400 * MB, 150 * MB, picked, 6);
	CHECK(count == 2 && picked[0] == 5 && picked[1] == 2);
	/* a new one larger than the budget: every own one, and still no other file */
	count = ae_replay_pick_removals(files, 6, 30, 100 * MB, 500 * MB, picked, 6);
	CHECK(count == 4 && picked[3] == 3);
	/* the room for picks bounds them */
	CHECK(ae_replay_pick_removals(files, 6, 1, 2000 * MB, 0, picked, 1) == 1 && picked[0] == 5);
	CHECK(ae_replay_pick_removals(NULL, 6, 1, 0, 0, picked, 6) == 0);
	CHECK(ae_replay_pick_removals(files, 0, 1, 0, 0, picked, 6) == 0);
	CHECK(ae_replay_pick_removals(files, 6, 1, 0, 0, NULL, 6) == 0);
	/* the disk */
	CHECK(!ae_replay_disk_allows(0) && !ae_replay_disk_allows(1023 * MB) && ae_replay_disk_allows(1024 * MB));
}

static void test_setup_cache(void)
{
	unsigned char setup[AE_REPLAY_SETUP_SIZE], other[AE_REPLAY_CACHE_ID_SIZE];
	unsigned char id[AE_REPLAY_CACHE_ID_SIZE] = { 0x2d, 0x31, 0x77, 0x86, 0x1f, 0x24, 0x07, 0x2d };
	char why[160];
	int at;

	memset(setup, 0, sizeof(setup));
	setup[0] = 2;
	/* (a recording from before: matches with a cache or none) */
	CHECK(ae_replay_setup_cache_matches(setup, 6, id, why, sizeof(why)) == 1);
	CHECK(ae_replay_setup_cache_matches(setup, 0, NULL, why, sizeof(why)) == 1);
	ae_replay_setup_cache_write(setup, 6, id);
	CHECK(setup[0] == 2 && setup[2] == 1 && setup[4] == 6 && setup[5] == 0 && !memcmp(setup + 8, id, sizeof(id)));
	CHECK(ae_replay_setup_cache_matches(setup, 6, id, why, sizeof(why)) == 1 && !why[0]);
	CHECK(ae_replay_setup_cache_matches(setup, 7, id, why, sizeof(why)) == 0);
	CHECK(strstr(why, "weapon set 6, another than this machine's (7)") != NULL);
	CHECK(ae_replay_setup_cache_matches(setup, 0, NULL, why, sizeof(why)) == 0);
	CHECK(strstr(why, "no weapon set loaded") != NULL);
	for (at = 0; at < AE_REPLAY_CACHE_ID_SIZE; at++)
	{
		memcpy(other, id, sizeof(other));
		other[at] ^= 0x01;
		CHECK(ae_replay_setup_cache_matches(setup, 6, other, why, sizeof(why)) == 0);
	}
	/* (no cache loaded when recording: none written, the bytes left 0) */
	ae_replay_setup_cache_write(setup, 0, id);
	CHECK(setup[2] == 0 && setup[4] == 0 && setup[8] == 0);
	ae_replay_setup_cache_write(setup, 0x01020304UL, id);
	CHECK(setup[4] == 4 && setup[5] == 3 && setup[6] == 2 && setup[7] == 1);
	CHECK(ae_replay_setup_cache_matches(setup, 0x01020304UL, id, why, sizeof(why)) == 1);
	/* (a tiny why buffer is never overrun) */
	CHECK(ae_replay_setup_cache_matches(setup, 9, id, why, 4) == 0 && strlen(why) < 4);
}

static void test_start_tick(void)
{
	unsigned char bytes[AE_REPLAY_HEADER_SIZE];
	struct ae_replay_header header;
	char why[64];
	static const unsigned long ticks[] = { 0, AE_REPLAY_START_TICK_MOST, AE_REPLAY_START_TICK_MOST + 1, 0x3FFFFFFFUL,
		0x7FFFFFFFUL, 0x80000000UL, 0xFFFFFFFFUL };
	int index;

	for (index = 0; index < (int)(sizeof ticks / sizeof ticks[0]); index++)
	{
		good_header(bytes);
		put_long(bytes + 24, ticks[index]);
		CHECK(ae_replay_header_read(bytes, sizeof bytes, NETWORK, &header, why, sizeof why) ==
			(ticks[index] <= AE_REPLAY_START_TICK_MOST ? AE_REPLAY_HEADER_OK : AE_REPLAY_HEADER_DAMAGED));
	}
}

static void test_sizes(void)
{
	static const int datagram_most[][3] = { { 5, GAME, 50 }, { 1, 1200, 51 } };
	static const int datagram_over[][3] = { { 5, GAME, 50 }, { 1, 1201, 51 } };
	static const int message_most[][3] = { { 5, GAME, 50 }, { 1, 10, 51 }, { 2, 0x1000, 52 }, { 3, 0x1000, 53 },
		{ 4, 0x1000, 54 } };
	static const int reliable_over[][3] = { { 5, GAME, 50 }, { 1, 10, 51 }, { 2, 0x1001, 52 } };
	static const int machine_over[][3] = { { 5, GAME, 50 }, { 1, 10, 51 }, { 3, 0x1001, 52 } };
	static const int game_message_over[][3] = { { 5, GAME, 50 }, { 1, 10, 51 }, { 4, 0x1001, 52 } };
	struct ae_replay_stream stream;
	char why[64];

	CHECK(stream_ok(datagram_most, 2, why, &stream));
	CHECK(!stream_ok(datagram_over, 2, why, &stream) && strstr(why, "larger than a game sends"));
	CHECK(stream_ok(message_most, 5, why, &stream));
	CHECK(!stream_ok(reliable_over, 3, why, &stream) && strstr(why, "larger than a game sends"));
	CHECK(!stream_ok(machine_over, 3, why, &stream));
	CHECK(!stream_ok(game_message_over, 3, why, &stream));
}

static void test_game_record(void)
{
	long machines[16], controllers[16];
	char why[96];
	int index;

	/* the counts: as the live path's bounds (128 machines, 16 players, 4 difficulties here) */
	CHECK(ae_replay_game_counts_check(2, 8, 16, 1, 128, 16, 4, why, sizeof why));
	CHECK(ae_replay_game_counts_check(0, 0, 1, 0, 128, 16, 4, why, sizeof why));
	CHECK(ae_replay_game_counts_check(128, 16, 16, 3, 128, 16, 4, why, sizeof why));
	CHECK(!ae_replay_game_counts_check(-1, 8, 16, 1, 128, 16, 4, why, sizeof why));
	CHECK(!ae_replay_game_counts_check(129, 8, 16, 1, 128, 16, 4, why, sizeof why));
	CHECK(!ae_replay_game_counts_check(2, 17, 16, 1, 128, 16, 4, why, sizeof why));
	CHECK(!ae_replay_game_counts_check(2, -1, 16, 1, 128, 16, 4, why, sizeof why));
	CHECK(!ae_replay_game_counts_check(2, 8, 0, 1, 128, 16, 4, why, sizeof why));
	CHECK(!ae_replay_game_counts_check(2, 8, 17, 1, 128, 16, 4, why, sizeof why));
	CHECK(!ae_replay_game_counts_check(2, 8, 16, 4, 128, 16, 4, why, sizeof why) && strstr(why, "difficulty"));
	CHECK(!ae_replay_game_counts_check(2, 8, 16, -1, 128, 16, 4, why, sizeof why));
	CHECK(!ae_replay_game_counts_check(2, 8, 16, 0x7FFFFFFFL, 128, 16, 4, why, sizeof why));
	/* the players: four on machine 0's four controllers fit; a fifth on one of them is two players of one controller */
	for (index = 0; index < 5; index++)
	{
		machines[index] = 0;
		controllers[index] = index % 4;
	}
	CHECK(ae_replay_players_check(machines, controllers, 4, 128, 4, why, sizeof why));
	CHECK(!ae_replay_players_check(machines, controllers, 5, 128, 4, why, sizeof why) && strstr(why, "two players"));
	/* five on machine 0, controller 0: refused */
	for (index = 0; index < 5; index++)
		controllers[index] = 0;
	CHECK(!ae_replay_players_check(machines, controllers, 5, 128, 4, why, sizeof why));
	/* out of range: no player, skipped as the live path skips it */
	machines[0] = -1; controllers[0] = 0;
	machines[1] = 128; controllers[1] = 0;
	machines[2] = 0; controllers[2] = 4;
	machines[3] = 0; controllers[3] = -1;
	machines[4] = 1; controllers[4] = 0;
	CHECK(ae_replay_players_check(machines, controllers, 5, 128, 4, why, sizeof why));
	/* the limits themselves out of range */
	CHECK(!ae_replay_players_check(machines, controllers, 5, 257, 4, why, sizeof why));
	CHECK(!ae_replay_players_check(machines, controllers, 5, 128, 9, why, sizeof why));
	CHECK(!ae_replay_players_check(machines, controllers, -1, 128, 4, why, sizeof why));
}

static void test_map_names(void)
{
	char name[32];
	static const char *const good[] = { "levels\\test\\bloodgulch\\bloodgulch", "[H2]_Lockout", "beavercreek@ce",
		"a.b", "con1", "comx", "lpt", "nul_city", "my map (v2)+" };
	static const char *const bad[] = { "../../../x", "levels\\..\\x", "a/b", "c:x", "x:", "", "levels\\",
		"levels\\...", "levels\\ . ", "con", "CON.map", "nul", "aux@ce", "prn x", "com1", "COM9.map", "lpt3",
		"levels\\com1", "tab\tname", "semi;colon", "quote\"", "dollar$", "star*", "percent%" };
	int index;

	for (index = 0; index < (int)(sizeof good / sizeof good[0]); index++)
		CHECK(ae_replay_map_name_safe(good[index], strlen(good[index]) + 1));
	for (index = 0; index < (int)(sizeof bad / sizeof bad[0]); index++)
	{
		if (!bad[index][0])
			CHECK(!ae_replay_map_name_safe(bad[index], 1));
		else
			CHECK(!ae_replay_map_name_safe(bad[index], strlen(bad[index]) + 1));
	}
	/* no end inside the field: refused; the end just inside: read */
	memset(name, 'a', sizeof name);
	CHECK(!ae_replay_map_name_safe(name, sizeof name));
	name[sizeof name - 1] = 0;
	CHECK(ae_replay_map_name_safe(name, sizeof name));
	CHECK(!ae_replay_map_name_safe(NULL, 4));
	CHECK(!ae_replay_map_name_safe(name, 0));
	/* every byte value as the one character */
	for (index = 1; index < 256; index++)
	{
		char one[2];
		int c = index;
		int allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
			c == '-' || c == '[' || c == ']' || c == '(' || c == ')' || c == '+' || c == '@';

		one[0] = (char)c;
		one[1] = 0;
		CHECK(ae_replay_map_name_safe(one, 2) == allowed);
	}
}

static void test_cost(void)
{
	struct ae_replay_cost cost;

	memset(&cost, 0, sizeof cost);
	CHECK(ae_replay_cost_mean(&cost) == 0);
	ae_replay_cost_add(&cost, 10);
	ae_replay_cost_add(&cost, 30);
	ae_replay_cost_add(&cost, 2);
	CHECK(cost.ticks == 3 && cost.most_us == 30 && ae_replay_cost_mean(&cost) == 14);
}

int main(void)
{
	test_header();
	test_records();
	test_stream();
	test_names();
	test_caps();
	test_cost();
	test_setup_cache();
	test_start_tick();
	test_sizes();
	test_game_record();
	test_map_names();
	printf("ae_replay_rules: %ld checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
