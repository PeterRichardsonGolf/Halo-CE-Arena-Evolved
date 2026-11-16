#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ae_number_field.h"
#include "ae_setup_options.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_number_range seconds = { 5, 60, "SECOND", "SECONDS", "INSTANT" };
	struct ae_number_range plain = { 60, 150, NULL, NULL, NULL };
	char text[64];
	long value;
	int option;

	/* typing: digits only, a few of them */
	CHECK(ae_number_typing_ok("", 4) && ae_number_typing_ok("1234", 4) && !ae_number_typing_ok("12345", 4));
	CHECK(!ae_number_typing_ok("1.5", 4) && !ae_number_typing_ok("-3", 4) && !ae_number_typing_ok("a", 4));
	CHECK(!ae_number_typing_ok(NULL, 4));
	CHECK(ae_number_parse("37", &value) && value == 37 && !ae_number_parse("", &value) && !ae_number_parse("1x", &value));
	CHECK(!ae_number_parse("1234567890", &value));
	/* held to the range; a zero word keeps 0 as a value */
	CHECK(ae_number_clamp(&plain, 20) == 60 && ae_number_clamp(&plain, 0) == 60 && ae_number_clamp(&plain, 999) == 150);
	CHECK(ae_number_clamp(&seconds, 0) == 0 && ae_number_clamp(&seconds, 2) == 5 && ae_number_clamp(&seconds, 61) == 60);
	/* shown */
	ae_number_format(&seconds, 0, text, sizeof(text));
	CHECK(!strcmp(text, "INSTANT"));
	ae_number_format(&seconds, 1, text, sizeof(text));
	CHECK(!strcmp(text, "1 SECOND"));
	ae_number_format(&seconds, 12, text, sizeof(text));
	CHECK(!strcmp(text, "12 SECONDS"));
	ae_number_format(&plain, 97, text, sizeof(text));
	CHECK(!strcmp(text, "97"));
	ae_number_hint(&plain, text, sizeof(text));
	CHECK(!strcmp(text, "60 TO 150"));
	ae_number_hint(&seconds, text, sizeof(text));
	CHECK(!strcmp(text, "0 FOR INSTANT, OR 5 TO 60"));

	/* every typable GAME OPTIONS row: the range fits the stored field, holds every number its list has, and its zero
	word is the list's word for 0 */
	for (option = 0; option < AE_SETUP_OPTIONS; option++)
	{
		struct ae_number_option entry;
		struct ae_setup_row const *row = ae_setup_row(option);
		int index;

		if (!ae_number_option(option, &entry))
			continue;
		CHECK(row != NULL);
		CHECK(entry.range.minimum >= 1 && entry.range.maximum >= entry.range.minimum && entry.scale >= 1);
		CHECK(entry.range.maximum * entry.scale <= entry.field_maximum);
		for (index = 0; row && index < row->value_count; index++)
		{
			const char *list = row->values[index];
			char *end;
			long number = strtol(list, &end, 10);

			if (end == list)
			{
				/* a word: the one for 0, and it is the first value of the list */
				CHECK(entry.range.zero_word && !strcmp(list, entry.range.zero_word) && index == 0);
				continue;
			}
			if (*end == '.')
				continue;   /* 7.5 SECONDS: a half second no whole number has */
			CHECK(number >= entry.range.minimum && number <= entry.range.maximum);
		}
	}
	CHECK(ae_number_option(AE_SO_KILLS_TO_WIN, NULL) && !ae_number_option(AE_SO_JUMP, NULL) &&
		!ae_number_option(AE_SO_NAME, NULL) && !ae_number_option(-1, NULL));
	if (failures)
		printf("%d failures\n", failures);
	return failures != 0;
}
