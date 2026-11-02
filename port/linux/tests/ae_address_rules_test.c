/* ae_address_rules.c: JOIN BY ADDRESS's typed text and RECENT list */
#include <stdio.h>
#include <string.h>

#include "ae_address_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_address a;
	struct ae_address_recent r, r2;
	char text[2048], long_text[100];
	struct tm then, now;
	char label[32];
	long length;

	/* host, host:port, with the default port when none is typed */
	CHECK(ae_address_parse("192.168.1.20", 5150, &a) == AE_ADDRESS_OK && !strcmp(a.host, "192.168.1.20") &&
		a.port == 5150 && !a.port_typed && !strcmp(a.text, "192.168.1.20"));
	CHECK(ae_address_parse("  arena-host:2310 ", 5150, &a) == AE_ADDRESS_OK && !strcmp(a.host, "arena-host") &&
		a.port == 2310 && a.port_typed && !strcmp(a.text, "arena-host:2310"));
	CHECK(ae_address_parse("my_pc.tail1234.ts.net", 1, &a) == AE_ADDRESS_OK);
	CHECK(ae_address_parse("h:65535", 1, &a) == AE_ADDRESS_OK && a.port == 65535);
	CHECK(ae_address_parse("h:1", 5, &a) == AE_ADDRESS_OK && a.port == 1);
	/* bad: empty, no host, bad ports, bad characters, two colons, too long */
	CHECK(ae_address_parse("", 1, &a) == AE_ADDRESS_EMPTY);
	CHECK(ae_address_parse("   ", 1, &a) == AE_ADDRESS_EMPTY);
	CHECK(ae_address_parse(NULL, 1, &a) == AE_ADDRESS_EMPTY);
	CHECK(ae_address_parse(":2302", 1, &a) == AE_ADDRESS_NO_HOST);
	CHECK(ae_address_parse("host:", 1, &a) == AE_ADDRESS_BAD_PORT);
	CHECK(ae_address_parse("host:0", 1, &a) == AE_ADDRESS_BAD_PORT);
	CHECK(ae_address_parse("host:65536", 1, &a) == AE_ADDRESS_BAD_PORT);
	CHECK(ae_address_parse("host:12ab", 1, &a) == AE_ADDRESS_BAD_PORT);
	CHECK(ae_address_parse("host:-5", 1, &a) == AE_ADDRESS_BAD_PORT);
	CHECK(ae_address_parse("host:000001", 1, &a) == AE_ADDRESS_BAD_PORT);
	CHECK(ae_address_parse("ho st", 1, &a) == AE_ADDRESS_BAD_CHARACTER);
	CHECK(ae_address_parse("host/x", 1, &a) == AE_ADDRESS_BAD_CHARACTER);
	CHECK(ae_address_parse("a:1:2", 1, &a) == AE_ADDRESS_BAD_CHARACTER);
	memset(long_text, 'a', 65);
	long_text[65] = 0;
	CHECK(ae_address_parse(long_text, 1, &a) == AE_ADDRESS_TOO_LONG);
	long_text[64] = 0;
	CHECK(ae_address_parse(long_text, 1, &a) == AE_ADDRESS_OK);
	CHECK(ae_address_reason(AE_ADDRESS_BAD_PORT)[0] && !ae_address_reason(AE_ADDRESS_OK)[0]);

	/* RECENT: newest first, the same address (any case) moves up and keeps a name, 10 at most, forget */
	ae_address_recent_init(&r);
	CHECK(ae_address_recent_add(&r, "Home Server", "192.168.1.20", 100));
	CHECK(ae_address_recent_add(&r, "", "Arena-Host", 200));
	CHECK(r.count == 2 && !strcmp(r.entries[0].text, "Arena-Host"));
	CHECK(ae_address_recent_add(&r, "", "192.168.1.20", 300));
	CHECK(r.count == 2 && !strcmp(r.entries[0].text, "192.168.1.20") && !strcmp(r.entries[0].name, "Home Server") &&
		r.entries[0].when == 300);
	CHECK(ae_address_recent_add(&r, "x", "ARENA-host", 400) && r.count == 2 && !strcmp(r.entries[0].text, "ARENA-host"));
	{
		int i;
		char host[32];

		for (i = 0; i < 14; i++)
		{
			snprintf(host, sizeof(host), "10.0.0.%d", i);
			CHECK(ae_address_recent_add(&r, "n", host, 500 + i));
		}
	}
	CHECK(r.count == AE_ADDRESS_RECENT_LIMIT && !strcmp(r.entries[0].text, "10.0.0.13"));
	CHECK(ae_address_recent_forget(&r, 0) && r.count == 9 && !strcmp(r.entries[0].text, "10.0.0.12"));
	CHECK(!ae_address_recent_forget(&r, 9) && !ae_address_recent_forget(&r, -1));
	CHECK(!ae_address_recent_add(&r, "n", "", 1));

	/* written and read back; a bad line dropped */
	length = ae_address_recent_write(&r, text, sizeof(text));
	CHECK(length > 0);
	CHECK(ae_address_recent_read(&r2, text) == 1 && r2.count == r.count && !strcmp(r2.entries[3].text, r.entries[3].text) &&
		!strcmp(r2.entries[3].name, r.entries[3].name) && r2.entries[3].when == r.entries[3].when);
	CHECK(ae_address_recent_read(&r2, "12\tgood.host:2310\tName\nnonsense\nx\tbad host\tn\n") == 0 && r2.count == 1 &&
		!strcmp(r2.entries[0].text, "good.host:2310"));
	CHECK(ae_address_recent_write(&r, text, 10) == -1);

	/* when */
	memset(&now, 0, sizeof(now));
	now.tm_year = 126; now.tm_mon = 9; now.tm_mday = 30; now.tm_hour = 22;
	then = now; then.tm_hour = 20; then.tm_min = 41;
	ae_address_when_label(&then, &now, label, sizeof(label));
	CHECK(!strcmp(label, "TODAY 20:41"));
	then.tm_mday = 29;
	ae_address_when_label(&then, &now, label, sizeof(label));
	CHECK(!strcmp(label, "YESTERDAY"));
	then.tm_mday = 27;
	ae_address_when_label(&then, &now, label, sizeof(label));
	CHECK(!strcmp(label, "3 DAYS AGO"));
	then.tm_mon = 8; then.tm_mday = 30;
	ae_address_when_label(&then, &now, label, sizeof(label));
	CHECK(!strcmp(label, "SEPTEMBER 30"));
	then = now; then.tm_mon = 8; then.tm_mday = 29;     /* across a month end: 31 days */
	now.tm_mon = 10; now.tm_mday = 1;
	then.tm_mon = 9; then.tm_mday = 31;
	ae_address_when_label(&then, &now, label, sizeof(label));
	CHECK(!strcmp(label, "YESTERDAY"));
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
