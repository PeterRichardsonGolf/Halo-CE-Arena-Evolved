#include <stdio.h>
#include <string.h>
#include "ae_credits_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_credits_block blocks[16];
	int count;

	/* credits_blocks: headings, paragraphs, bullets; a line after a block goes on with it; a blank line ends it */
	count = ae_credits_parse("# ONE\nA paragraph\ngoes on.\n\nAnother.\n- first bullet\n  and its second line\n- second\n# TWO\nText.\n",
		blocks, 16);
	CHECK(count == 7);
	CHECK(blocks[0].kind == AE_CREDITS_HEADING && !strcmp(blocks[0].text, "ONE"));
	CHECK(blocks[1].kind == AE_CREDITS_PARAGRAPH && !strcmp(blocks[1].text, "A paragraph goes on."));
	CHECK(blocks[2].kind == AE_CREDITS_PARAGRAPH && !strcmp(blocks[2].text, "Another."));
	CHECK(blocks[3].kind == AE_CREDITS_BULLET && !strcmp(blocks[3].text, "first bullet and its second line"));
	CHECK(blocks[4].kind == AE_CREDITS_BULLET && !strcmp(blocks[4].text, "second"));
	CHECK(blocks[5].kind == AE_CREDITS_HEADING && !strcmp(blocks[5].text, "TWO"));
	CHECK(blocks[6].kind == AE_CREDITS_PARAGRAPH && !strcmp(blocks[6].text, "Text."));
	/* credits_heading_is_one_line: the line right after a heading starts a block of its own */
	count = ae_credits_parse("# H\nBody", blocks, 16);
	CHECK(count == 2 && blocks[1].kind == AE_CREDITS_PARAGRAPH && !strcmp(blocks[1].text, "Body"));
	/* credits_line_ends: Windows line ends, trailing spaces, a missing last newline */
	count = ae_credits_parse("# H \r\n- a \r\n\r\n- b", blocks, 16);
	CHECK(count == 3 && !strcmp(blocks[0].text, "H") && !strcmp(blocks[1].text, "a") && !strcmp(blocks[2].text, "b"));
	/* credits_limits: the most blocks, a long block cut, NULL and empty */
	count = ae_credits_parse("- a\n- b\n- c\n", blocks, 2);
	CHECK(count == 2);
	{
		char text[3000];
		int each;

		strcpy(text, "- ");
		for (each = 0; each < 400; each++)
			strcat(text, "word ");
		count = ae_credits_parse(text, blocks, 16);
		CHECK(count == 1 && strlen(blocks[0].text) == AE_CREDITS_TEXT_SIZE - 1);
	}
	CHECK(ae_credits_parse(NULL, blocks, 16) == 0 && ae_credits_parse("", blocks, 16) == 0 && ae_credits_parse("x", blocks, 0) == 0);
	/* credits_fallback: headed, and says the game needs the player's Halo files */
	count = ae_credits_parse(ae_credits_fallback(), blocks, 16);
	CHECK(count >= 4 && blocks[0].kind == AE_CREDITS_HEADING && strstr(ae_credits_fallback(), "YOUR OWN HALO FILES"));
	if (!failures)
		printf("ae_credits_rules_test: all passed\n");
	return failures ? 1 : 0;
}
