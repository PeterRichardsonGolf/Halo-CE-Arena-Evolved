/* ae_joiner_notice_rules.c: the host's chat line for a machine that joins without Arena Evolved */
#include <stdio.h>
#include <string.h>

#include "ae_joiner_notice_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char text[201], small[20];
	/* (16 characters of 3 bytes each: the longest name in UTF-8) */
	char const *long_name = "\xe6\xbc\xa2\xe6\xbc\xa2\xe6\xbc\xa2\xe6\xbc\xa2\xe6\xbc\xa2\xe6\xbc\xa2\xe6\xbc\xa2"
		"\xe6\xbc\xa2\xe6\xbc\xa2\xe6\xbc\xa2\xe6\xbc\xa2\xe6\xbc\xa2\xe6\xbc\xa2\xe6\xbc\xa2\xe6\xbc\xa2\xe6\xbc\xa2";
	char const *long_build = "0123456789012345678901234567890";
	int length;

	/* OpenCE's machine, a Delta build, an unknown one, no name */
	length = ae_joiner_notice_text(text, (int)sizeof(text), "Goat", 0, "");
	CHECK(length > 0 && !strcmp(text, "Goat plays on OpenCE: some Arena Evolved extras won't show for them. "
		"Get AE: " AE_JOINER_NOTICE_LINK));
	CHECK(ae_joiner_notice_text(text, (int)sizeof(text), "Goat", 1, "ChupathingyCE 0.8.0h") > 0 &&
		!strncmp(text, "Goat plays on ChupathingyCE 0.8.0h: ", 36));
	CHECK(ae_joiner_notice_text(text, (int)sizeof(text), "Goat", 1, "") > 0 &&
		!strncmp(text, "Goat plays on another build: ", 29));
	CHECK(ae_joiner_notice_text(text, (int)sizeof(text), "Goat", -1, "x") > 0 &&
		!strncmp(text, "Goat plays on another build: ", 29));
	CHECK(ae_joiner_notice_text(text, (int)sizeof(text), NULL, 0, NULL) > 0 && !strncmp(text, "A player plays on ", 18));
	CHECK(ae_joiner_notice_text(text, (int)sizeof(text), "", 0, NULL) > 0 && !strncmp(text, "A player plays on ", 18));
	/* the longest name and build still fit chat's 200 characters, the link whole */
	length = ae_joiner_notice_text(text, (int)sizeof(text), long_name, 1, long_build);
	CHECK(length > 0 && length <= 200 && strstr(text, AE_JOINER_NOTICE_LINK) != NULL);
	/* a buffer too small: nothing, never a cut line */
	CHECK(ae_joiner_notice_text(small, (int)sizeof(small), "Goat", 0, "") == 0 && small[0] == 0);
	CHECK(ae_joiner_notice_text(NULL, 10, "Goat", 0, "") == 0);
	if (failures)
		return 1;
	printf("ae_joiner_notice_rules_test: ok\n");
	return 0;
}
