#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_skull_pack_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* an index of made-up records laid out as the medal pack's: a header, then a 480-byte record each (a hash, the name, the size, the
data's size and place) */
static unsigned char *make_index(char const *const *names, int count, size_t *size)
{
	size_t total = AE_MEDAL_INDEX_FIRST + (size_t)(count - 1) * AE_MEDAL_INDEX_STRIDE + AE_MEDAL_INDEX_USED;
	unsigned char *index = calloc(1, total);
	int each;

	index[0] = 0x90; index[1] = 0xA0; index[2] = 0xBF; index[3] = 0xCD;
	for (each = 0; each < count; each++)
	{
		unsigned char *record = index + AE_MEDAL_INDEX_FIRST + (size_t)each * AE_MEDAL_INDEX_STRIDE;

		memcpy(record + 4, names[each], strlen(names[each]));
		record[0x34] = 0x80; record[0x36] = 0x80;
		record[0x48] = 0x00; record[0x49] = 0x40;
		record[0x50] = (unsigned char)(0x10 + each);
	}
	*size = total;
	return index;
}

int main(void)
{
	static char const *const names[] = { "ANGER", "BANDANA", "BLACKEYE", "BLACKEYE", "THATSJUSTWRONG", "GRUNTBIRTHDAYPARTY", "SKULL" };
	size_t size;
	unsigned char *index = make_index(names, 7, &size);
	struct ae_medal_record record;

	CHECK(ae_skull_index_count(index, size) == 7);
	CHECK(ae_skull_index_record(index, size, 4, &record) && !strcmp(record.name, "THATSJUSTWRONG") && record.width == 128 &&
		record.height == 128 && record.size == 0x4000 && record.offset == 0x14);
	CHECK(!ae_skull_index_record(index, size, 7, &record) && !ae_skull_index_record(index, size, -1, &record));
	/* a key finds its record by the squashed name; the first of two with a name */
	CHECK(ae_skull_index_find(index, size, "anger") == 0);
	CHECK(ae_skull_index_find(index, size, "thats_just_wrong") == 4);
	CHECK(ae_skull_index_find(index, size, "grunt_birthday_party") == 5);
	CHECK(ae_skull_index_find(index, size, "black_eye") == 2);
	/* spelled another way in MCC */
	CHECK(ae_skull_index_find(index, size, "bandanna") == 1);
	/* a skull MCC has no picture for, and keys that are none */
	CHECK(ae_skull_index_find(index, size, "famine") == -1);
	CHECK(ae_skull_index_find(index, size, "") == -1 && ae_skull_index_find(index, size, NULL) == -1);
	CHECK(ae_skull_index_find(index, size, "a_very_long_skull_key_that_is_longer_than_a_record_name_field_can_hold") == -1);
	/* a file that is cut short, not an index, or too small gives nothing and never reads past its end */
	CHECK(ae_skull_index_count(index, size - 1) == 6);
	CHECK(ae_skull_index_count(index, 10) == 0 && ae_skull_index_count(NULL, 0) == 0);
	index[0] = 0;
	CHECK(ae_skull_index_count(index, size) == 0 && ae_skull_index_find(index, size, "anger") == -1);
	free(index);
	/* a name that is not capitals ends the row */
	{
		static char const *const mixed[] = { "ANGER", "Anger", "BOOM" };
		unsigned char *odd = make_index(mixed, 3, &size);

		CHECK(ae_skull_index_count(odd, size) == 1);
		free(odd);
	}
	/* transparent margins are left out, including an alpha at the threshold; the visible pixels keep their rows and columns */
	{
		unsigned char rgba[6 * 5 * 4] = { 0 };
		int width = 0, height = 0;

		rgba[(0 * 6 + 0) * 4 + 3] = 8;
		rgba[(2 * 6 + 1) * 4 + 0] = 17;
		rgba[(2 * 6 + 1) * 4 + 3] = 9;
		rgba[(3 * 6 + 4) * 4 + 0] = 29;
		rgba[(3 * 6 + 4) * 4 + 3] = 255;
		CHECK(ae_skull_rgba_trim(rgba, 6, 5, &width, &height));
		CHECK(width == 4 && height == 2);
		CHECK(rgba[0] == 17 && rgba[3] == 9);
		CHECK(rgba[(1 * 4 + 3) * 4 + 0] == 29 && rgba[(1 * 4 + 3) * 4 + 3] == 255);
	}
	/* a left-only margin compacts overlapping pixels in the same rows */
	{
		unsigned char rgba[4 * 2 * 4] = { 0 };
		int width, height;

		rgba[(0 * 4 + 1) * 4 + 0] = 31;
		rgba[(0 * 4 + 1) * 4 + 3] = 255;
		rgba[(1 * 4 + 2) * 4 + 0] = 47;
		rgba[(1 * 4 + 2) * 4 + 3] = 255;
		CHECK(ae_skull_rgba_trim(rgba, 4, 2, &width, &height));
		CHECK(width == 2 && height == 2);
		CHECK(rgba[0] == 31 && rgba[(1 * 2 + 1) * 4 + 0] == 47);
	}
	/* an already tight picture keeps its dimensions and pixels */
	{
		unsigned char rgba[2 * 2 * 4] = { 0 };
		int width, height;

		rgba[3] = rgba[(2 * 2 - 1) * 4 + 3] = 255;
		CHECK(ae_skull_rgba_trim(rgba, 2, 2, &width, &height));
		CHECK(width == 2 && height == 2 && rgba[3] == 255 && rgba[(2 * 2 - 1) * 4 + 3] == 255);
	}
	/* a picture with no visible content, and malformed arguments, do not produce a box */
	{
		unsigned char rgba[4 * 4 * 4] = { 0 };
		int width, height;

		CHECK(!ae_skull_rgba_trim(rgba, 4, 4, &width, &height));
		CHECK(!ae_skull_rgba_trim(NULL, 4, 4, &width, &height));
		CHECK(!ae_skull_rgba_trim(rgba, 0, 4, &width, &height));
		CHECK(!ae_skull_rgba_trim(rgba, 4, 4, NULL, &height));
	}

	if (failures)
		return 1;
	printf("ae_skull_pack_rules: ok\n");
	return 0;
}
