/* ae_section_folds.c: the folded sections of CHANGE MAP's folders, kept as text */

#include <stdio.h>
#include <string.h>

#include "../game/ae_section_folds.h"

static int failures;
#define CHECK(condition) do { if (!(condition)) { printf("FAIL line %d: %s\n", __LINE__, #condition); failures++; } } while (0)

int main(void)
{
	static struct ae_section_folds folds, again;
	static char text[AE_SECTION_FOLDS_TEXT];
	char key[AE_SECTION_FOLDS_KEY];
	long length;

	ae_section_folds_init(&folds);
	CHECK(!ae_section_folds_get(&folds, "maps", "WORKSHOP/OTHER"));
	CHECK(ae_section_folds_set(&folds, "maps", "WORKSHOP/OTHER", 1) && !ae_section_folds_set(&folds, "maps", "WORKSHOP/OTHER", 1));
	CHECK(ae_section_folds_set(&folds, "maps", "COMMUNITY/OTHER", 1) && ae_section_folds_get(&folds, "maps", "WORKSHOP/OTHER"));
	CHECK(!ae_section_folds_get(&folds, "gametypes", "WORKSHOP/OTHER"));
	length = ae_section_folds_write(&folds, text, sizeof(text));
	CHECK(length > 0 && ae_section_folds_read(&again, text) && again.count == 2);
	CHECK(ae_section_folds_get(&again, "maps", "COMMUNITY/OTHER") && ae_section_folds_get(&again, "maps", "WORKSHOP/OTHER"));
	/* open again: gone, the other stays; opening what is open changes nothing */
	CHECK(ae_section_folds_set(&folds, "maps", "WORKSHOP/OTHER", 0) && !ae_section_folds_set(&folds, "maps", "WORKSHOP/OTHER", 0));
	CHECK(!ae_section_folds_get(&folds, "maps", "WORKSHOP/OTHER") && ae_section_folds_get(&folds, "maps", "COMMUNITY/OTHER"));
	/* bad lines, CRLF, comments, a key too long */
	CHECK(ae_section_folds_read(&again, "# x\r\nmaps\tA/B\r\nnonsense\n\tonly key\nmaps\t\n") && again.count == 1 &&
		ae_section_folds_get(&again, "maps", "A/B"));
	memset(text, 'x', 200);
	text[200] = 0;
	CHECK(!ae_section_folds_set(&folds, "maps", text, 1) && !ae_section_folds_read(NULL, "") && !ae_section_folds_read(&again, NULL));
	CHECK(ae_section_folds_write(&folds, text, 8) == -1);
	ae_section_folds_key(key, sizeof(key), "WORKSHOP", "PRO EDITION");
	CHECK(!strcmp(key, "WORKSHOP/PRO EDITION"));
	/* the list is full: one more is refused, an open one frees room */
	ae_section_folds_init(&folds);
	{
		int index;

		for (index = 0; index < AE_SECTION_FOLDS_MAXIMUM; index++)
		{
			snprintf(key, sizeof(key), "F/S%d", index);
			CHECK(ae_section_folds_set(&folds, "maps", key, 1));
		}
		CHECK(!ae_section_folds_set(&folds, "maps", "F/MORE", 1) && ae_section_folds_set(&folds, "maps", "F/S3", 0) &&
			ae_section_folds_set(&folds, "maps", "F/MORE", 1));
	}
	if (failures) return 1;
	printf("ae_section_folds: ok\n");
	return 0;
}
