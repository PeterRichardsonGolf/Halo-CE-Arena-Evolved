#include <stdio.h>
#include <string.h>
#include "ae_bot_names_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static char names[8][AE_BOT_NAMES_LENGTH + 1];

int main(void)
{
	char name[16];
	int count;

	/* an empty file, comments and blanks only: no names (the built-in list stays) */
	CHECK(ae_bot_names_parse("", names, 8) == 0);
	CHECK(ae_bot_names_parse("\n\n  \n# a comment\n   # another\n", names, 8) == 0);
	CHECK(ae_bot_names_parse(NULL, names, 8) == 0);
	/* names: trimmed, comments and CRLF ignored, duplicates kept */
	count = ae_bot_names_parse("# bots\n  Ann \r\nBob\n\nAnn\n\tCy\t\n", names, 8);
	CHECK(count == 4 && !strcmp(names[0], "Ann") && !strcmp(names[1], "Bob") && !strcmp(names[2], "Ann") && !strcmp(names[3], "Cy"));
	/* a name clipped to 11 characters, a last line with no newline, non-ASCII bytes dropped */
	count = ae_bot_names_parse("Bartholomew the Great\nZed", names, 8);
	CHECK(count == 2 && !strcmp(names[0], "Bartholomew") && strlen(names[0]) == 11 && !strcmp(names[1], "Zed"));
	count = ae_bot_names_parse("J\xc3\xb6rg\n\xc3\xa9\n", names, 8);
	CHECK(count == 1 && !strcmp(names[0], "Jrg"));
	/* the most read */
	count = ae_bot_names_parse("a\nb\nc\nd\n", names, 3);
	CHECK(count == 3);
	/* a name after a # in the middle of a line is part of the name; the clip leaves no trailing space */
	count = ae_bot_names_parse("Ann # x\nAbcdefghij k\n", names, 8);
	CHECK(count == 2 && !strcmp(names[0], "Ann # x") && !strcmp(names[1], "Abcdefghij"));
	/* picking: in order, then the names again with a number */
	ae_bot_names_parse("Ann\nBartholomew\n", names, 8);
	ae_bot_names_pick(names, 2, 0, name, sizeof(name));
	CHECK(!strcmp(name, "Ann"));
	ae_bot_names_pick(names, 2, 1, name, sizeof(name));
	CHECK(!strcmp(name, "Bartholomew"));
	ae_bot_names_pick(names, 2, 2, name, sizeof(name));
	CHECK(!strcmp(name, "Ann 2"));
	ae_bot_names_pick(names, 2, 3, name, sizeof(name));
	CHECK(!strcmp(name, "Bartholo 2"));
	ae_bot_names_pick(names, 2, 4, name, sizeof(name));
	CHECK(!strcmp(name, "Ann 3"));
	ae_bot_names_pick(names, 0, 0, name, sizeof(name));
	CHECK(name[0] == 0);
	printf("%d checks, %d failures\n", checks, failures);
	return failures != 0;
}
