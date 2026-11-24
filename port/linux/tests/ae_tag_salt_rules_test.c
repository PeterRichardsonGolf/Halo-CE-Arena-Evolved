/* ae_tag_salt_rules.c: an added tag's salt, after the table's highest, back to 0x8000 past 0xfffe */
#include <stdio.h>

#include "ae_tag_salt_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	long added;

	/* (a small map: the salts after its highest) */
	CHECK(ae_tag_salt_after(0xe174 + 1000, 0) == 0xe174 + 1001);
	CHECK(ae_tag_salt_after(0xe174 + 1000, 9) == 0xe174 + 1010);
	CHECK(ae_tag_salt_after(0, 0) == 1);
	/* (the last salt, then back to the first: 0xffff is never one) */
	CHECK(ae_tag_salt_after(0xfffd, 0) == 0xfffe);
	CHECK(ae_tag_salt_after(0xfffd, 1) == 0x8000);
	CHECK(ae_tag_salt_after(0xfffe, 0) == 0x8000);
	CHECK(ae_tag_salt_after(0xffff, 0) == 0x8000);
	CHECK(ae_tag_salt_after(0xffff, 1) == 0x8001);
	/* (a large MCC map: 5330 tags, salts 0xe174 up, and a cache's 2566 tags after them) */
	for (added = 0; added < 2566; added++)
	{
		long salt = ae_tag_salt_after(0xe174 + 5329, added);

		CHECK(salt >= AE_TAG_SALT_FIRST && salt <= AE_TAG_SALT_LAST);
		if (failures)
			break;
	}
	CHECK(ae_tag_salt_after(0xe174 + 5329, 0) == 0xf646);
	CHECK(ae_tag_salt_after(0xe174 + 5329, 0xfffe - 0xf646) == 0xfffe);
	CHECK(ae_tag_salt_after(0xe174 + 5329, 0xfffe - 0xf646 + 1) == 0x8000);
	/* (more tags than there are salts: around again) */
	CHECK(ae_tag_salt_after(0xfffe, 0x7fff) == 0x8000);
	/* (out of range in: kept in range) */
	CHECK(ae_tag_salt_after(-5, 0) == 1);
	CHECK(ae_tag_salt_after(0x12345, 0) == 0x8000);
	CHECK(ae_tag_salt_after(10, -3) == 11);
	if (!failures)
		printf("ok\n");
	return failures ? 1 : 0;
}
