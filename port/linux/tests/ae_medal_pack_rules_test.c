/* ae_medal_pack_rules_test.c: the medal pictures' index, pixels, career list rows, block decode and names (ae_medal_pack_rules.h).
Every buffer is made here, small, on the heap with exactly its size so the sanitizers see a read past its end. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_medal_pack_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void put16(unsigned char *at, unsigned int value)
{
	at[0] = (unsigned char)value;
	at[1] = (unsigned char)(value >> 8);
}

static void put32(unsigned char *at, unsigned long value)
{
	put16(at, (unsigned int)(value & 0xFFFFu));
	put16(at + 2, (unsigned int)(value >> 16));
}

/* an index of the names given, each picture 8 by 8 (four blocks, 64 bytes) one after the other from the pixels' header */
static unsigned char *make_index(char const *const *names, int count, size_t *size)
{
	unsigned char *index;
	int i;

	*size = AE_MEDAL_INDEX_FIRST + (size_t)count * AE_MEDAL_INDEX_STRIDE;
	index = calloc(1, *size);
	put32(index, AE_MEDAL_INDEX_MAGIC);
	for (i = 0; i < count; i++)
	{
		unsigned char *record = index + AE_MEDAL_INDEX_FIRST + (size_t)i * AE_MEDAL_INDEX_STRIDE;

		strcpy((char *)record + 4, names[i]);
		put16(record + 0x34, 8);
		put16(record + 0x36, 8);
		put32(record + 0x48, 64);
		put32(record + 0x50, AE_MEDAL_PIXELS_HEADER + (unsigned long)i * 64);
	}
	return index;
}

static unsigned char *copy_of(unsigned char const *bytes, size_t size)
{
	unsigned char *copy = malloc(size ? size : 1);

	memcpy(copy, bytes, size);
	return copy;
}

static void index_test(void)
{
	static char const *const names[] = { "MEDAL_DOUBLEKILL", "MEDAL_OPENSEASON", "MEDAL_OPENSEASON", "MEDAL_Z" };
	struct ae_medal_record record;
	size_t size;
	unsigned char *index = make_index(names, 4, &size);
	unsigned char *exact;

	CHECK(ae_medal_index_count(index, size) == 4);
	CHECK(ae_medal_index_record(index, size, 1, &record) && !strcmp(record.name, "MEDAL_OPENSEASON") && record.width == 8 &&
		record.height == 8 && record.size == 64 && record.offset == AE_MEDAL_PIXELS_HEADER + 64);
	CHECK(!ae_medal_index_record(index, size, 4, &record) && !ae_medal_index_record(index, size, -1, &record));
	CHECK(!ae_medal_index_record(index, size, 1, NULL));
	/* the career list's name finds a record: case and underscores aside, the first of two of a name */
	CHECK(ae_medal_index_find(index, size, "Medal_OpenSeason") == 1);
	CHECK(ae_medal_index_find(index, size, "MEDAL_OPEN_SEASON") == 1);
	CHECK(ae_medal_index_find(index, size, "OpenSeason") == 1);
	CHECK(ae_medal_index_find(index, size, "Medal_DoubleKill") == 0);
	CHECK(ae_medal_index_find(index, size, "Medal_BallKill") == -1);
	CHECK(ae_medal_index_find(index, size, "") == -1 && ae_medal_index_find(index, size, NULL) == -1);
	/* an icon name too long for the matching form is none */
	{
		char long_name[300];

		memset(long_name, 'a', sizeof(long_name) - 1);
		long_name[sizeof(long_name) - 1] = 0;
		CHECK(ae_medal_index_find(index, size, long_name) == -1);
	}
	/* a record cut short is not counted; the last record need hold only the bytes that are read (0x54), one byte short drops it */
	exact = copy_of(index, AE_MEDAL_INDEX_FIRST + 3 * AE_MEDAL_INDEX_STRIDE + AE_MEDAL_INDEX_USED);
	CHECK(ae_medal_index_count(exact, AE_MEDAL_INDEX_FIRST + 3 * AE_MEDAL_INDEX_STRIDE + AE_MEDAL_INDEX_USED) == 4);
	CHECK(ae_medal_index_record(exact, AE_MEDAL_INDEX_FIRST + 3 * AE_MEDAL_INDEX_STRIDE + AE_MEDAL_INDEX_USED, 3, &record));
	free(exact);
	exact = copy_of(index, AE_MEDAL_INDEX_FIRST + 3 * AE_MEDAL_INDEX_STRIDE + AE_MEDAL_INDEX_USED - 1);
	CHECK(ae_medal_index_count(exact, AE_MEDAL_INDEX_FIRST + 3 * AE_MEDAL_INDEX_STRIDE + AE_MEDAL_INDEX_USED - 1) == 3 &&
		!ae_medal_index_record(exact, AE_MEDAL_INDEX_FIRST + 3 * AE_MEDAL_INDEX_STRIDE + AE_MEDAL_INDEX_USED - 1, 3, &record));
	free(exact);
	exact = copy_of(index, AE_MEDAL_INDEX_FIRST);
	CHECK(ae_medal_index_count(exact, AE_MEDAL_INDEX_FIRST) == 0);
	free(exact);
	exact = copy_of(index, AE_MEDAL_INDEX_FIRST - 1);
	CHECK(ae_medal_index_count(exact, AE_MEDAL_INDEX_FIRST - 1) == 0 && ae_medal_index_find(exact, AE_MEDAL_INDEX_FIRST - 1,
		"Medal_DoubleKill") == -1);
	free(exact);
	CHECK(ae_medal_index_count(NULL, 100) == 0 && ae_medal_index_count(index, 0) == 0);
	/* the wrong magic */
	index[0] ^= 1;
	CHECK(ae_medal_index_count(index, size) == 0 && ae_medal_index_find(index, size, "Medal_DoubleKill") == -1);
	index[0] ^= 1;
	/* a row ends at the first name without MEDAL_ */
	index[AE_MEDAL_INDEX_FIRST + 2 * AE_MEDAL_INDEX_STRIDE + 4] = 'X';
	CHECK(ae_medal_index_count(index, size) == 2);
	index[AE_MEDAL_INDEX_FIRST + 2 * AE_MEDAL_INDEX_STRIDE + 4] = 'M';
	/* a name not ended in its field, or with a control character, is no record */
	memset(index + AE_MEDAL_INDEX_FIRST + 4, 'A', 48);
	memcpy(index + AE_MEDAL_INDEX_FIRST + 4, "MEDAL_", 6);
	CHECK(!ae_medal_index_record(index, size, 0, &record));
	memset(index + AE_MEDAL_INDEX_FIRST + 4, 0, 48);
	strcpy((char *)index + AE_MEDAL_INDEX_FIRST + 4, "MEDAL_A\x01");
	CHECK(!ae_medal_index_record(index, size, 0, &record));
	free(index);
}

static void fits_test(void)
{
	struct ae_medal_record record;

	memset(&record, 0, sizeof(record));
	record.width = 80;
	record.height = 80;
	record.size = 6400;
	record.offset = 16;
	CHECK(ae_medal_record_fits(&record, 16 + 6400));
	CHECK(!ae_medal_record_fits(&record, 16 + 6399) && !ae_medal_record_fits(&record, 0));
	CHECK(!ae_medal_record_fits(NULL, 100000));
	/* an offset before the header, or one that would wrap */
	record.offset = 15;
	CHECK(!ae_medal_record_fits(&record, 100000));
	record.offset = 0xFFFFFFF0ul;
	CHECK(!ae_medal_record_fits(&record, 100000));
	record.offset = 16;
	/* a size that is not a byte a pixel, a side that is not whole blocks, too small, too large */
	record.size = 3200;
	CHECK(!ae_medal_record_fits(&record, 100000));
	record.size = 6400;
	record.width = 78;
	CHECK(!ae_medal_record_fits(&record, 100000));
	record.width = 0;
	record.height = 0;
	record.size = 0;
	CHECK(!ae_medal_record_fits(&record, 100000));
	record.width = 256;
	record.height = 256;
	record.size = 65536;
	CHECK(!ae_medal_record_fits(&record, 1000000));
	record.width = AE_MEDAL_PICTURE_MOST;
	record.height = AE_MEDAL_PICTURE_MOST;
	record.size = AE_MEDAL_PICTURE_MOST * AE_MEDAL_PICTURE_MOST;
	CHECK(ae_medal_record_fits(&record, 1000000));
}

static void pixels_test(void)
{
	unsigned char header[16] = { 0 };
	unsigned char *exact;

	put32(header, AE_MEDAL_PIXELS_MAGIC);
	CHECK(ae_medal_pixels_valid(header, 16));
	exact = copy_of(header, 15);
	CHECK(!ae_medal_pixels_valid(exact, 15));
	free(exact);
	header[3] ^= 0x80;
	CHECK(!ae_medal_pixels_valid(header, 16) && !ae_medal_pixels_valid(NULL, 16));
}

/* a block: alpha ends, the alpha picks (3 bits a pixel, packed), colour ends, the colour picks (2 bits a pixel) */
static void make_block(unsigned char *block, unsigned int a0, unsigned int a1, unsigned long long alpha_picks,
	unsigned int c0, unsigned int c1, unsigned long colour_picks)
{
	int i;

	block[0] = (unsigned char)a0;
	block[1] = (unsigned char)a1;
	for (i = 0; i < 6; i++)
		block[2 + i] = (unsigned char)(alpha_picks >> (8 * i));
	put16(block + 8, c0);
	put16(block + 10, c1);
	put32(block + 12, colour_picks);
}

static void decode_test(void)
{
	unsigned char blocks[64];
	unsigned char rgba[8 * 8 * 4];
	unsigned char *exact;
	unsigned long long picks = 0;
	int i;

	/* four blocks (8 by 8): the first solid red, alpha 255; the second white to black with colour picks 0 1 2 3 round and alpha
	picks 0 to 7 round (a0 not above a1: four between, then 0 and 255); the third blue, a0 above a1 (six between); the
	fourth green */
	memset(blocks, 0, sizeof(blocks));
	make_block(blocks, 255, 255, 0, 0xF800, 0x0000, 0);
	for (i = 0; i < 16; i++)
		picks |= (unsigned long long)(i % 8) << (3 * i);
	{
		unsigned long colour = 0;

		for (i = 0; i < 16; i++)
			colour |= (unsigned long)(i % 4) << (2 * i);
		make_block(blocks + 16, 0, 255, picks, 0xFFFF, 0x0000, colour);
	}
	make_block(blocks + 32, 240, 30, 0x0A3, 0x001F, 0x001F, 0);
	make_block(blocks + 48, 10, 10, 0, 0x07E0, 0x07E0, 0);
	CHECK(ae_medal_decode_bc3(blocks, sizeof(blocks), 8, 8, rgba));
	/* the first block, pixel (0, 0) and (3, 3): red, opaque */
	CHECK(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 255);
	CHECK(rgba[(3 * 8 + 3) * 4] == 255 && rgba[(3 * 8 + 3) * 4 + 3] == 255);
	/* the second block (x 4..7, y 0..3): pixel i has colour pick i % 4 and alpha pick i % 8 */
	{
		unsigned char const *p0 = rgba + (0 * 8 + 4) * 4;	/* i = 0: white, alpha pick 0 = 0 */
		unsigned char const *p1 = rgba + (0 * 8 + 5) * 4;	/* i = 1: black, alpha pick 1 = 255 */
		unsigned char const *p2 = rgba + (0 * 8 + 6) * 4;	/* i = 2: 2/3 white, alpha pick 2 = (4 * 0 + 255) / 5 = 51 */
		unsigned char const *p3 = rgba + (0 * 8 + 7) * 4;	/* i = 3: 1/3 white, alpha pick 3 = (3 * 0 + 2 * 255) / 5 = 102 */

		CHECK(p0[0] == 255 && p0[1] == 255 && p0[2] == 255 && p0[3] == 0);
		CHECK(p1[0] == 0 && p1[1] == 0 && p1[2] == 0 && p1[3] == 255);
		CHECK(p2[0] == 170 && p2[1] == 170 && p2[2] == 170 && p2[3] == 51);
		CHECK(p3[0] == 85 && p3[1] == 85 && p3[2] == 85 && p3[3] == 102);
		/* i = 6 (row 1, column 2): alpha pick 6 is 0, i = 7 (row 1, column 3): pick 7 is 255 */
		CHECK(rgba[(1 * 8 + 6) * 4 + 3] == 0 && rgba[(1 * 8 + 7) * 4 + 3] == 255);
	}
	/* the third block (x 0..3, y 4..7): blue, its alpha picks 0xA3: pixel 0 pick 3, pixel 1 pick 4; with a0 above a1 a pick k
	is ((8 - k) * a0 + (k - 1) * a1) / 7 */
	{
		unsigned char const *p0 = rgba + (4 * 8 + 0) * 4;
		unsigned char const *p1 = rgba + (4 * 8 + 1) * 4;

		CHECK(p0[0] == 0 && p0[1] == 0 && p0[2] == 255);
		CHECK(p0[3] == (5 * 240 + 2 * 30) / 7 && p1[3] == (4 * 240 + 3 * 30) / 7);
	}
	/* the fourth block: green (565 0x07E0: 6 bits all on) */
	CHECK(rgba[(4 * 8 + 4) * 4] == 0 && rgba[(4 * 8 + 4) * 4 + 1] == 255 && rgba[(4 * 8 + 4) * 4 + 2] == 0);
	/* a size that is not width * height, a side not in blocks, nothing: nothing written */
	memset(rgba, 0x55, sizeof(rgba));
	CHECK(!ae_medal_decode_bc3(blocks, 63, 8, 8, rgba) && !ae_medal_decode_bc3(blocks, 64, 8, 7, rgba));
	CHECK(!ae_medal_decode_bc3(blocks, 64, 0, 8, rgba) && !ae_medal_decode_bc3(NULL, 64, 8, 8, rgba));
	CHECK(!ae_medal_decode_bc3(blocks, 64, 8, 8, NULL) && !ae_medal_decode_bc3(blocks, 64, 100000, 8, rgba));
	CHECK(rgba[0] == 0x55);
	/* exactly sized (the sanitizers watch the read) */
	exact = copy_of(blocks, sizeof(blocks));
	CHECK(ae_medal_decode_bc3(exact, sizeof(blocks), 8, 8, rgba));
	free(exact);
}

/* the list given as a text exactly its length (not ended) */
static int icon_of(char const *text, char const *id, char *icon, size_t icon_size)
{
	size_t length = strlen(text);
	char *exact = malloc(length ? length : 1);
	int found;

	memcpy(exact, text, length);
	found = ae_medal_career_icon(exact, length, id, icon, icon_size);
	free(exact);
	return found;
}

static void career_test(void)
{
	char icon[32];
	static char const list[] =
		"<?xml version=\"1.0\"?>\n<Career>\n<MPMedals>\n"
		"<Medal id=\"medal_killing_spree\" name=\"$a\" desc=\"$b\" points=\"5\" smallIcon=\"Medal_KillingSpree\" "
		"largeIcon=\"LargeMedal_KillingSpree\" type=\"eMedalType_Spree\" />\n"
		"\t<Medal\n\tsmallIcon='Medal_OpenSeason' largeIcon=\"x\" id='medal_open_season' />\n"
		"<Medal id=\"medal_no_icon\" points=\"1\" />\n"
		"<Medal id=\"medal_empty_icon\" smallIcon=\"\" />\n"
		"<Medal id=\"medal_long_icon\" smallIcon=\"Medal_AVeryLongIconNameThatDoesNotFit\" />\n"
		"<Medal subid=\"medal_sub\" smallIcon=\"Medal_Sub\" />\n"
		"</MPMedals>\n<MissionMedals>\n<Medal id=\"medal_campaign_headshot\" smallIcon=\"Medal_CampaignHeadshot\" />\n"
		"</MissionMedals>\n</Career>\n";

	CHECK(icon_of(list, "medal_killing_spree", icon, sizeof(icon)) && !strcmp(icon, "Medal_KillingSpree"));
	/* the other order of attributes, single quotes, a tag across lines, the id's case */
	CHECK(icon_of(list, "medal_open_season", icon, sizeof(icon)) && !strcmp(icon, "Medal_OpenSeason"));
	CHECK(icon_of(list, "MEDAL_OPEN_SEASON", icon, sizeof(icon)) && !strcmp(icon, "Medal_OpenSeason"));
	CHECK(icon_of(list, "medal_campaign_headshot", icon, sizeof(icon)) && !strcmp(icon, "Medal_CampaignHeadshot"));
	/* none: a row without an icon, with an empty one, one that does not fit, a name that is a part of another, an unknown id */
	CHECK(!icon_of(list, "medal_no_icon", icon, sizeof(icon)) && !icon_of(list, "medal_empty_icon", icon, sizeof(icon)));
	CHECK(!icon_of(list, "medal_long_icon", icon, sizeof(icon)) && !icon_of(list, "medal_sub", icon, sizeof(icon)));
	CHECK(!icon_of(list, "medal_open", icon, sizeof(icon)) && !icon_of(list, "medal_nothing", icon, sizeof(icon)));
	CHECK(!icon_of(list, "", icon, sizeof(icon)) && !icon_of(list, "medal_open_season", icon, 1));
	/* cut short anywhere: no crash, and a row cut inside a value is not found */
	{
		size_t length = strlen(list), cut;

		for (cut = 0; cut <= length; cut++)
		{
			char *exact = malloc(cut ? cut : 1);

			memcpy(exact, list, cut);
			(void)ae_medal_career_icon(exact, cut, "medal_open_season", icon, sizeof(icon));
			(void)ae_medal_career_icon(exact, cut, "medal_campaign_headshot", icon, sizeof(icon));
			free(exact);
		}
		CHECK(icon_of("<Medal id=\"medal_a\" smallIcon=\"Medal_A", "medal_a", icon, sizeof(icon)) == 0);
		CHECK(icon_of("<Medal id=\"medal_a\" smallIcon=\"Medal_A\"", "medal_a", icon, sizeof(icon)) == 1);
		CHECK(icon_of("<Medal id", "medal_a", icon, sizeof(icon)) == 0 && icon_of("<Medal", "medal_a", icon, sizeof(icon)) == 0);
	}
	CHECK(!ae_medal_career_icon(NULL, 10, "medal_a", icon, sizeof(icon)) && !ae_medal_career_icon("x", 1, NULL, icon, sizeof(icon)));
}

static void name_test(void)
{
	char name[40];

	CHECK(ae_medal_name_from_key("medal_open_season", name, sizeof(name), 1) == 11 && !strcmp(name, "OPEN SEASON"));
	CHECK(ae_medal_name_from_key("medal_double_kill", name, sizeof(name), 0) == 11 && !strcmp(name, "Double Kill"));
	CHECK(ae_medal_name_from_key("medal_kill", name, sizeof(name), 0) == 4 && !strcmp(name, "Kill"));
	/* a doubled underscore is one space; a key with no start is read as it is */
	CHECK(ae_medal_name_from_key("medal_splatter_with_a__splatter", name, sizeof(name), 0) > 0 &&
		!strcmp(name, "Splatter"));
	CHECK(ae_medal_name_from_key("flag_taken", name, sizeof(name), 0) > 0 && !strcmp(name, "Flag Taken"));
	CHECK(ae_medal_name_from_key("_x_", name, sizeof(name), 0) == 1 && !strcmp(name, "X"));
	/* none, empty, and too small a place: cut short, ended */
	CHECK(ae_medal_name_from_key(NULL, name, sizeof(name), 0) == 0 && name[0] == 0);
	CHECK(ae_medal_name_from_key("medal_", name, sizeof(name), 0) == 0 && name[0] == 0);
	CHECK(ae_medal_name_from_key("medal_double_kill", name, 7, 0) == 6 && !strcmp(name, "Double"));
	CHECK(ae_medal_name_from_key("medal_double_kill", name, 8, 0) == 6 && !strcmp(name, "Double"));
	CHECK(ae_medal_name_from_key("medal_double_kill", name, 1, 0) == 0 && name[0] == 0);
	CHECK(ae_medal_name_from_key("medal_double_kill", NULL, 10, 0) == 0 && ae_medal_name_from_key("x", name, 0, 0) == 0);
}

static void art_name_test(void)
{
	char art[40];

	/* AE's own picture of a medal is named by its key without "medal_" */
	CHECK(ae_medal_art_name_from_key("medal_bomber", art, sizeof(art)) == 6 && !strcmp(art, "bomber"));
	CHECK(ae_medal_art_name_from_key("medal_yard_sale", art, sizeof(art)) == 9 && !strcmp(art, "yard_sale"));
	/* the one key that is a number */
	CHECK(ae_medal_art_name_from_key("medal_360", art, sizeof(art)) == 11 && !strcmp(art, "three_sixty"));
	/* no key, none, too small a place: no name, ended */
	CHECK(ae_medal_art_name_from_key("medal_", art, sizeof(art)) == 0 && art[0] == 0);
	CHECK(ae_medal_art_name_from_key("bomber", art, sizeof(art)) == 0 && art[0] == 0);
	CHECK(ae_medal_art_name_from_key(NULL, art, sizeof(art)) == 0 && art[0] == 0);
	CHECK(ae_medal_art_name_from_key("medal_yard_sale", art, 4) == 0 && art[0] == 0);
	CHECK(ae_medal_art_name_from_key("medal_bomber", NULL, 10) == 0 && ae_medal_art_name_from_key("medal_bomber", art, 0) == 0);
}

static void known_names_test(void)
{
	char name[48];

	ae_medal_name_from_key("medal_triple_of_each_kill", name, sizeof(name), 0);
	CHECK(!strcmp(name, "Triple Kill"));
	ae_medal_name_from_key("medal_first_poiint", name, sizeof(name), 1);
	CHECK(!strcmp(name, "FIRST POINT"));
	ae_medal_name_from_key("medal_smooth_with_th_moves", name, sizeof(name), 0);
	CHECK(!strcmp(name, "Smooth Moves"));
	/* Wingman's key in the career list says its 5 assists; its name is the plain one */
	ae_medal_name_from_key("medal_wingman_5_assis_wingman", name, sizeof(name), 1);
	CHECK(!strcmp(name, "WINGMAN"));
	ae_medal_name_from_key("medal_wingman_5_assis_wingman", name, sizeof(name), 0);
	CHECK(!strcmp(name, "Wingman"));
	ae_medal_name_from_key("medal_killtacular", name, sizeof(name), 0);
	CHECK(!strcmp(name, "Killtacular"));
}

int main(void)
{
	known_names_test();
	index_test();
	fits_test();
	pixels_test();
	decode_test();
	career_test();
	name_test();
	art_name_test();
	if (!failures)
		printf("ae_medal_pack_rules_test: ok\n");
	return failures != 0;
}
