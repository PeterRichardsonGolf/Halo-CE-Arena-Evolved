#include <stdio.h>
#include <string.h>
#include "ae_server_sec_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_sec_login_budget a = { 0, 0 }, b = { 0, 0 }, g = { 0, 0 };
	char out[32];
	int index, n;

	/* M2: one address cannot drain the global budget; it meets its own ceiling first */
	for (index = 0; index < AE_SEC_LOGIN_PER_ADDRESS; index++)
		CHECK(ae_sec_login_check(&a, &g, 5, 0) == 0);
	CHECK(ae_sec_login_check(&a, &g, 5, 0) == 1);
	CHECK(g.used == AE_SEC_LOGIN_PER_ADDRESS);
	/* ... and another address still signs in */
	CHECK(ae_sec_login_check(&b, &g, 5, 0) == 0);
	/* a new minute starts over */
	CHECK(ae_sec_login_check(&a, &g, 6, 0) == 0 && a.used == 1 && g.used == 1);
	/* the global ceiling is for many addresses */
	{
		int refused = 0;

		for (index = 0; index < 200; index++)
		{
			struct ae_sec_login_budget fresh = { 0, 0 };

			if (ae_sec_login_check(&fresh, &g, 6, 0) == 2)
				refused++;
		}
		CHECK(refused == 200 - (AE_SEC_LOGIN_GLOBAL - 1));
	}
	/* a local connection never meets it, and has a larger budget of its own */
	{
		struct ae_sec_login_budget l = { 0, 0 };

		for (index = 0; index < AE_SEC_LOGIN_PER_ADDRESS_LOCAL; index++)
			CHECK(ae_sec_login_check(&l, &g, 6, 1) == 0);
		CHECK(ae_sec_login_check(&l, &g, 6, 1) == 1);
	}

	/* L1: the Host */
	CHECK(ae_sec_host_allowed("", ""));
	CHECK(ae_sec_host_allowed("127.0.0.1:8080", ""));
	CHECK(ae_sec_host_allowed("192.168.1.20", NULL));
	CHECK(ae_sec_host_allowed("[::1]:8080", ""));
	CHECK(ae_sec_host_allowed("LocalHost:8080", ""));
	CHECK(!ae_sec_host_allowed("evil.test", ""));
	CHECK(!ae_sec_host_allowed("evil.test:8080", "halo.example.org"));
	CHECK(ae_sec_host_allowed("Halo.Example.org:8080", "other.test, halo.example.org"));
	CHECK(!ae_sec_host_allowed("halo.example.org.evil.test", "halo.example.org"));
	CHECK(!ae_sec_host_allowed("1.2.3.4.evil.test", ""));
	CHECK(!ae_sec_host_allowed("999.1.1", ""));
	CHECK(!ae_sec_host_allowed(":8080", ""));

	/* L3 */
	CHECK(ae_sec_ws_admit(0, 0, 8, 0, 0) == 0);
	CHECK(ae_sec_ws_admit(3, 0, 8, AE_SEC_WS_UNAUTHENTICATED_PER_ADDRESS, 0) == 1);
	CHECK(ae_sec_ws_admit(AE_SEC_WS_TOTAL, 0, 8, 0, 0) == 2);
	CHECK(ae_sec_ws_admit(10, 8, 8, 0, 1) == 3);
	CHECK(ae_sec_ws_admit(10, 7, 8, 5, 1) == 0);
	/* unauthenticated sockets do not use the signed-in sockets' places */
	CHECK(ae_sec_ws_admit(10, 0, 8, 1, 0) == 0);

	/* L4 */
	n = ae_sec_command_word("kick", out, sizeof(out));
	CHECK(n == 5 && !strcmp(out, " kick"));
	n = ae_sec_command_word("two words", out, sizeof(out));
	CHECK(n == 12 && !strcmp(out, " \"two words\""));
	n = ae_sec_command_word("", out, sizeof(out));
	CHECK(n == 3 && !strcmp(out, " \"\""));
	CHECK(ae_sec_command_word("a\" sv_stop \"b", out, sizeof(out)) == -1);
	CHECK(ae_sec_command_word("a\nb", out, sizeof(out)) == -1);
	CHECK(ae_sec_command_word("this is much too long for the buffer", out, 16) == -1);

	/* M3 */
	CHECK(ae_sec_password_minimum(1, 0) == 8 && ae_sec_password_minimum(0, 1) == 8 && ae_sec_password_minimum(0, 0) == 12);
	CHECK(ae_sec_bind_allowed(1, 0, 0));
	CHECK(!ae_sec_bind_allowed(0, 0, 0));
	CHECK(ae_sec_bind_allowed(0, 1, 0));
	CHECK(ae_sec_bind_allowed(0, 0, 1));

	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
