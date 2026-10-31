/* ae_mcc_sound_names.c: an MCC stock map's sound permutations found by name in Custom Edition's sounds.map
(synthetic items only) */

#include <stdio.h>
#include <string.h>

#include "ae_mcc_sound_names.h"

static int failures;

#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void put_long(unsigned char *at, unsigned long value)
{
	at[0] = (unsigned char)value;
	at[1] = (unsigned char)(value >> 8);
	at[2] = (unsigned char)(value >> 16);
	at[3] = (unsigned char)(value >> 24);
}

/* a sound item: 2 pitch ranges of 1 and 2 permutations, each permutation's samples (size, then file offset) */
static unsigned long make_item(unsigned char *item, unsigned long size)
{
	unsigned long ranges = AE_MCC_SOUND_HEADER_SIZE, permutations;

	memset(item, 0, size);
	put_long(item + AE_MCC_SOUND_PITCH_RANGES_OFFSET, 2);
	put_long(item + ranges + AE_MCC_PITCH_RANGE_PERMUTATIONS_OFFSET, 1);
	put_long(item + ranges + AE_MCC_SOUND_PITCH_RANGE_SIZE + AE_MCC_PITCH_RANGE_PERMUTATIONS_OFFSET, 2);
	permutations = ranges + 2 * AE_MCC_SOUND_PITCH_RANGE_SIZE;
	put_long(item + permutations + AE_MCC_PERMUTATION_SAMPLES_OFFSET, 100);
	put_long(item + permutations + AE_MCC_PERMUTATION_SAMPLES_OFFSET + 8, 0x1000);
	permutations += AE_MCC_SOUND_PERMUTATION_SIZE;
	put_long(item + permutations + AE_MCC_PERMUTATION_SAMPLES_OFFSET, 200);
	put_long(item + permutations + AE_MCC_PERMUTATION_SAMPLES_OFFSET + 8, 0x2000);
	permutations += AE_MCC_SOUND_PERMUTATION_SIZE;
	put_long(item + permutations + AE_MCC_PERMUTATION_SAMPLES_OFFSET, 300);
	put_long(item + permutations + AE_MCC_PERMUTATION_SAMPLES_OFFSET + 8, 0x3000);
	return permutations + AE_MCC_SOUND_PERMUTATION_SIZE;
}

int main(void)
{
	static unsigned char item[4096];
	unsigned long size = make_item(item, sizeof(item)), offset, bytes;
	char tag[256];
	unsigned long range, permutation;

	/* the name MCC's resources use: <tag>__<pitch range>__<permutation> */
	CHECK(ae_mcc_sound_name_split("sound\\sfx\\impulse\\a__1__0", tag, sizeof(tag), &range, &permutation));
	CHECK(!strcmp(tag, "sound\\sfx\\impulse\\a") && range == 1 && permutation == 0);
	CHECK(ae_mcc_sound_name_split("weird__name__2__13", tag, sizeof(tag), &range, &permutation));
	CHECK(!strcmp(tag, "weird__name") && range == 2 && permutation == 13);
	CHECK(!ae_mcc_sound_name_split("no_numbers", tag, sizeof(tag), &range, &permutation));
	CHECK(!ae_mcc_sound_name_split("x__1__", tag, sizeof(tag), &range, &permutation));
	CHECK(!ae_mcc_sound_name_split("x__a__1", tag, sizeof(tag), &range, &permutation));
	CHECK(!ae_mcc_sound_name_split("__1__1", tag, sizeof(tag), &range, &permutation));
	CHECK(!ae_mcc_sound_name_split("xy__1__1", tag, 2, &range, &permutation));

	/* each permutation's samples, by its range and its place in the range */
	CHECK(ae_mcc_sound_permutation(item, size, 0, 0, &offset, &bytes) && offset == 0x1000 && bytes == 100);
	CHECK(ae_mcc_sound_permutation(item, size, 1, 0, &offset, &bytes) && offset == 0x2000 && bytes == 200);
	CHECK(ae_mcc_sound_permutation(item, size, 1, 1, &offset, &bytes) && offset == 0x3000 && bytes == 300);
	/* (no such range or permutation; an item too short for its own counts; counts too large) */
	CHECK(!ae_mcc_sound_permutation(item, size, 2, 0, &offset, &bytes));
	CHECK(!ae_mcc_sound_permutation(item, size, 0, 1, &offset, &bytes));
	CHECK(!ae_mcc_sound_permutation(item, size - 1, 1, 1, &offset, &bytes));
	CHECK(!ae_mcc_sound_permutation(item, AE_MCC_SOUND_HEADER_SIZE - 1, 0, 0, &offset, &bytes));
	put_long(item + AE_MCC_SOUND_PITCH_RANGES_OFFSET, 0x7fffffff);
	CHECK(!ae_mcc_sound_permutation(item, size, 0, 0, &offset, &bytes));
	put_long(item + AE_MCC_SOUND_PITCH_RANGES_OFFSET, 2);
	put_long(item + AE_MCC_SOUND_HEADER_SIZE + AE_MCC_PITCH_RANGE_PERMUTATIONS_OFFSET, 0xffffffffUL);
	CHECK(!ae_mcc_sound_permutation(item, size, 1, 0, &offset, &bytes));

	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_mcc_sound_names: all passed\n");
	return 0;
}
