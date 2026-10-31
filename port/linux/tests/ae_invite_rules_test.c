/* ae_invite_rules.c: an invite's code in a player's text (JOIN WITH INVITE, the lobby's join) */
#include <stdio.h>
#include <string.h>

#include "ae_invite_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define CODE "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
#define CODE_UPPER "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"

int main(void)
{
	char code[80], small[64];

	/* a link, in any case, with the whitespace a paste brings */
	CHECK(ae_invite_code("halo://join/" CODE, code, sizeof(code)) && !strcmp(code, CODE));
	CHECK(ae_invite_code("HALO://Join/" CODE_UPPER, code, sizeof(code)) && !strcmp(code, CODE));
	CHECK(ae_invite_code("  halo://join/" CODE "\r\n", code, sizeof(code)) && !strcmp(code, CODE));
	/* a link inside a message, and with something after its code */
	CHECK(ae_invite_code("join me: halo://join/" CODE " now", code, sizeof(code)) && !strcmp(code, CODE));
	CHECK(ae_invite_code("halo://join/" CODE "/", code, sizeof(code)) && !strcmp(code, CODE));
	/* the code alone (upper case too), trimmed */
	CHECK(ae_invite_code(CODE, code, sizeof(code)) && !strcmp(code, CODE));
	CHECK(ae_invite_code("\t" CODE_UPPER " ", code, sizeof(code)) && !strcmp(code, CODE));
	/* not invites: empty, cut short, one digit too many, a code with other text, another link, a key link */
	CHECK(!ae_invite_code("", code, sizeof(code)));
	CHECK(!ae_invite_code("   ", code, sizeof(code)));
	CHECK(!ae_invite_code("halo://join/0123456789abcdef", code, sizeof(code)));
	CHECK(!ae_invite_code("halo://join/" CODE "0", code, sizeof(code)));
	CHECK(!ae_invite_code(CODE "0", code, sizeof(code)));
	CHECK(!ae_invite_code("code " CODE, code, sizeof(code)));
	CHECK(!ae_invite_code("halo://key/" CODE, code, sizeof(code)));
	CHECK(!ae_invite_code("192.168.1.20", code, sizeof(code)));
	CHECK(!ae_invite_code("halo://join/", code, sizeof(code)));
	CHECK(!ae_invite_code("halo://joi", code, sizeof(code)));
	/* (no room for the code and its end; no text) */
	CHECK(!ae_invite_code(CODE, small, sizeof(small)));
	CHECK(!ae_invite_code(NULL, code, sizeof(code)));
	CHECK(!ae_invite_code(CODE, NULL, sizeof(code)));
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
