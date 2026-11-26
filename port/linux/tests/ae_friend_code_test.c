/* ae_friend_code.c: a friend code from a player key, and typed codes read back */
#include <stdio.h>
#include <string.h>

#include "ae_friend_code.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	unsigned char zero[32] = { 0 }, ramp[32], ones[32];
	char code[AE_FRIEND_CODE_SIZE], again[AE_FRIEND_CODE_SIZE];
	int index;

	for (index = 0; index < 32; index++)
	{
		ramp[index] = (unsigned char)index;
		ones[index] = 0xff;
	}
	/* known vectors (the party service's test holds the same values, made with another SHA-256) */
	CHECK(ae_friend_code_from_key(zero, code) && !strcmp(code, "CSM7-NBFR-CAYQ"));
	CHECK(ae_friend_code_from_key(ramp, code) && !strcmp(code, "CC6W-TAB6-RGSP"));
	CHECK(ae_friend_code_from_key(ones, code) && !strcmp(code, "NYB1-6XGF-E9HN"));
	CHECK(!ae_friend_code_from_key(NULL, code));

	/* a made code reads back as itself, in any case, with or without dashes and spaces */
	ae_friend_code_from_key(ramp, code);
	CHECK(ae_friend_code_parse(code, again) && !strcmp(again, code));
	CHECK(ae_friend_code_parse("cc6wtab6rgsp", again) && !strcmp(again, "CC6W-TAB6-RGSP"));
	CHECK(ae_friend_code_parse("  cc6w tab6 rgsp \n", again) && !strcmp(again, "CC6W-TAB6-RGSP"));

	/* I and L read as 1, O as 0 */
	CHECK(ae_friend_code_parse("k7qm-3rxa-w9td", again) && !strcmp(again, "K7QM-3RXA-W9TD"));
	CHECK(ae_friend_code_parse("IL0O-1100-ILOO", again) && !strcmp(again, "1100-1100-1100"));
	CHECK(ae_friend_code_parse("K7QM3RXAW9TD", again) && !strcmp(again, "K7QM-3RXA-W9TD"));

	/* not codes: U, symbols, too short, too long, empty, no text */
	CHECK(!ae_friend_code_parse("K7QM-3RXA-W9TU", again) && !again[0]);
	CHECK(!ae_friend_code_parse("K7QM-3RXA-W9T!", again));
	CHECK(!ae_friend_code_parse("K7QM-3RXA-W9T", again));
	CHECK(!ae_friend_code_parse("K7QM-3RXA-W9TD-0", again));
	CHECK(!ae_friend_code_parse("K7QM-3RXA-W9TD0", again));
	CHECK(!ae_friend_code_parse("", again));
	CHECK(!ae_friend_code_parse(NULL, again));
	CHECK(!ae_friend_code_parse("K7QM-3RXA-W9TD", NULL));

	/* the field's count: code characters only, the rest not counted */
	CHECK(ae_friend_code_count("") == 0);
	CHECK(ae_friend_code_count("K7Q") == 3);
	CHECK(ae_friend_code_count("K7QM-3R") == 6);
	CHECK(ae_friend_code_count("k7qu#") == 3);
	CHECK(ae_friend_code_count(NULL) == 0);

	printf(failures ? "FAILED\n" : "ok\n");
	return failures != 0;
}
