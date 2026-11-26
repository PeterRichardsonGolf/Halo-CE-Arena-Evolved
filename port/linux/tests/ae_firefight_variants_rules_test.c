#include <stdio.h>
#include <string.h>

#include "ae_firefight_variants_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_ffv_record record(const char *name, long first)
{
	struct ae_ffv_record r;
	int row;

	memset(&r, 0, sizeof(r));
	snprintf(r.name, sizeof(r.name), "%s", name);
	for (row = 0; row < AE_FFV_ROWS; row++)
		r.raw[row] = first + row;
	r.raw[3] = -1;
	r.skulls[0] = 0x80000001UL;
	r.skulls[1] = 0x20UL;
	r.item_set = 0xDEADBEEFUL;
	return r;
}

int main(void)
{
	static char big[AE_FFV_SAVED * (AE_FFV_LINE + 128)];
	char text[AE_FFV_LINE + 128], name[AE_FFV_NAME];
	struct ae_ffv_record a = record("My Wave", 1), back, list[AE_FFV_SAVED + 2];
	int count, dropped, index;

	/* names: printable ASCII, no bar, spaces trimmed and made single, 11 characters */
	CHECK(ae_ffv_name_clean("  My   Wave  ", name, sizeof(name)) && !strcmp(name, "My Wave"));
	CHECK(ae_ffv_name_clean("a|b;c\"d", name, sizeof(name)) && !strcmp(name, "abcd"));
	CHECK(ae_ffv_name_clean("ABCDEFGHIJKLMNOP", name, sizeof(name)) && !strcmp(name, "ABCDEFGHIJK"));
	CHECK(!ae_ffv_name_clean("   ", name, sizeof(name)) && !name[0]);
	CHECK(!ae_ffv_name_clean(NULL, name, sizeof(name)));
	CHECK(ae_ffv_name_same("Gruntpocalypse", "GRUNTPOCALYPSE") && !ae_ffv_name_same("Fiesta", "Fiesta 2") && !ae_ffv_name_same("a", ""));
	CHECK(ae_ffv_name_clean("é ok", name, sizeof(name)) && !strcmp(name, "ok"));

	/* a record round trips through its line, the hostile numbers refused */
	CHECK(ae_ffv_format(&a, text, sizeof(text)));
	CHECK(ae_ffv_parse(text, &back) && !memcmp(&a, &back, sizeof(a)));
	CHECK(!ae_ffv_format(&a, text, 20));
	CHECK(ae_ffv_format(&a, text, sizeof(text)));
	{
		char bad[AE_FFV_LINE + 128];

		CHECK(!ae_ffv_parse("", &back) && !ae_ffv_parse("v1|x|1,2|0,0|0", &back) && !ae_ffv_parse("v2|ab|1|0,0|0", &back));
		snprintf(bad, sizeof(bad), "%s", text);
		bad[strlen(bad) - 1] = 'x';
		CHECK(!ae_ffv_parse(bad, &back));
		strcpy(bad, text);
		strcat(bad, ",7");
		CHECK(!ae_ffv_parse(bad, &back));
		/* a name with a space at its end or a bar in it is not one the editor makes */
		CHECK(!ae_ffv_parse("v1|ab |1|0,0|0", &back));
		/* a huge value */
		{
			struct ae_ffv_record big_value = a;

			big_value.raw[0] = 99999999L;
			CHECK(ae_ffv_format(&big_value, bad, sizeof(bad)) && !ae_ffv_parse(bad, &back));
		}
		CHECK(!ae_ffv_parse("v1|ab|" "1,2,3|0,0|0", &back));
	}

	/* the file: a bad line left out, a repeated name once, the eight kept, a ninth replaces the oldest */
	count = 0;
	for (index = 0; index < AE_FFV_SAVED + 1; index++)
	{
		char n[16];
		struct ae_ffv_record r;

		snprintf(n, sizeof(n), "V%d", index);
		r = record(n, index * 100);
		count = ae_ffv_list_add(list, count, &r, &dropped);
		CHECK(dropped == (index == AE_FFV_SAVED));
	}
	CHECK(count == AE_FFV_SAVED && !strcmp(list[0].name, "V1") && !strcmp(list[AE_FFV_SAVED - 1].name, "V8"));
	{
		struct ae_ffv_record again = record("v3", 7);

		CHECK(ae_ffv_list_add(list, count, &again, &dropped) == count && !dropped && list[2].raw[0] == 7 && !strcmp(list[2].name, "v3"));
	}
	CHECK(ae_ffv_file_write(list, count, big, sizeof(big)));
	{
		struct ae_ffv_record read[AE_FFV_SAVED + 2];
		static char damaged[2 * sizeof(big) + 64];

		CHECK(ae_ffv_file_read(big, read, AE_FFV_SAVED + 2) == AE_FFV_SAVED && !memcmp(read, list, sizeof(list[0]) * AE_FFV_SAVED));
		strcpy(damaged, "garbage line\n");
		strcat(damaged, big);
		strcat(damaged, "v1|zz\n");
		CHECK(ae_ffv_file_read(damaged, read, AE_FFV_SAVED + 2) == AE_FFV_SAVED);
		CHECK(ae_ffv_file_read(damaged, read, 3) == 3);
		strcpy(damaged, big);
		strcat(damaged, big);
		CHECK(ae_ffv_file_read(damaged, read, AE_FFV_SAVED + 2) == AE_FFV_SAVED);
		CHECK(ae_ffv_file_read("", read, 4) == 0 && ae_ffv_file_read(NULL, read, 4) == 0);
		CHECK(!ae_ffv_file_write(list, count, big, 100));
	}
	count = ae_ffv_list_remove(list, count, 0);
	CHECK(count == AE_FFV_SAVED - 1 && !strcmp(list[0].name, "V2"));
	CHECK(ae_ffv_list_remove(list, count, 99) == count && ae_ffv_list_remove(list, count, -1) == count);

	if (failures)
		return 1;
	printf("ae_firefight_variants_rules_test: ok\n");
	return 0;
}
