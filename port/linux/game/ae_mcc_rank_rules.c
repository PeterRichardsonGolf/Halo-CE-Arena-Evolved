/* The MCC rank pictures' rules (see the header). Pure code. */
#include <stdio.h>
#include <string.h>

#include "ae_mcc_rank_rules.h"

/* the index's layout is the medal pack's: a record's name is 48 bytes from its byte 4, its sides at 0x34, its size at 0x48 and its
   place in the pixels' file at 0x50 */
enum { RECORD_NAME = 0x04, RECORD_SIZE = 0x34, RECORD_DATA_SIZE = 0x48, RECORD_DATA_OFFSET = 0x50, RECORD_USED = 0x54 };

static unsigned int read16(unsigned char const *bytes)
{
	return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8);
}

static unsigned int read32(unsigned char const *bytes)
{
	return read16(bytes) | (read16(bytes + 2) << 16);
}

int ae_mcc_rank_name(int rank, int large, char *out, size_t size)
{
	int length;

	if (rank < AE_MCC_RANK_FIRST || rank > AE_MCC_RANK_LAST || !out || size < 1)
		return 0;
	length = snprintf(out, size, "%s_%03d", large ? "LARGERANKICON" : "SMALLRANKICON", rank);
	if (length < 0 || (size_t)length >= size)
	{
		out[0] = 0;
		return 0;
	}
	return 1;
}

int ae_mcc_rank_record(unsigned char const *index, size_t index_size, int rank, int large, struct ae_medal_record *record)
{
	char wanted[AE_MEDAL_NAME_SIZE];
	size_t wanted_length;
	int each;

	if (!record || !index || index_size < AE_MEDAL_INDEX_FIRST || read32(index) != AE_MEDAL_INDEX_MAGIC ||
		!ae_mcc_rank_name(rank, large, wanted, sizeof(wanted)))
		return 0;
	wanted_length = strlen(wanted);
	for (each = 0; each < AE_MCC_RANK_INDEX_MOST; each++)
	{
		unsigned long long end = (unsigned long long)AE_MEDAL_INDEX_FIRST + (unsigned long long)each * AE_MEDAL_INDEX_STRIDE +
			RECORD_USED;
		unsigned char const *bytes;

		if (end > (unsigned long long)index_size)
			return 0;
		bytes = index + AE_MEDAL_INDEX_FIRST + (size_t)each * AE_MEDAL_INDEX_STRIDE;
		if (strncmp((char const *)bytes + RECORD_NAME, wanted, wanted_length) || bytes[RECORD_NAME + wanted_length])
			continue;
		memset(record, 0, sizeof(*record));
		memcpy(record->name, wanted, wanted_length + 1);
		record->width = read16(bytes + RECORD_SIZE);
		record->height = read16(bytes + RECORD_SIZE + 2);
		record->size = read32(bytes + RECORD_DATA_SIZE);
		record->offset = (unsigned long)read32(bytes + RECORD_DATA_OFFSET);
		return record->width == record->height && record->width >= 4 && record->width <= AE_MCC_RANK_PICTURE_MOST;
	}
	return 0;
}

int ae_mcc_rank_record_fits(struct ae_medal_record const *record, size_t pixels_size)
{
	unsigned long long end;

	if (!record || record->width < 4 || record->height < 4 || record->width > AE_MCC_RANK_PICTURE_MOST ||
		record->height > AE_MCC_RANK_PICTURE_MOST || (record->width & 3u) || (record->height & 3u))
		return 0;
	if (record->size != record->width * record->height || record->offset < AE_MEDAL_PIXELS_HEADER)
		return 0;
	end = (unsigned long long)record->offset + record->size;
	return end <= (unsigned long long)pixels_size;
}
