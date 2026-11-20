/* ae_join_fetch_rules.c: which asked Workshop maps are fetched from Steam, the limits, the refusal reasons and words */
#include <stdio.h>
#include <string.h>

#include "ae_join_fetch_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	unsigned long long id;
	int at;
	char text[256];

	/* an item number only from <file>@ws<digits> */
	CHECK(ae_jf_item_of("gothra mcc debug flycam pm@ws2899688264", &id) && id == 2899688264ull);
	CHECK(ae_jf_item_of("pat_damnation@ws2899341154", &id) && id == 2899341154ull);
	CHECK(!ae_jf_item_of("bloodgulch@mcc", &id));
	CHECK(!ae_jf_item_of("bloodgulch", &id));
	CHECK(!ae_jf_item_of("x@ws", &id));
	CHECK(!ae_jf_item_of("x@ws12a", &id));
	CHECK(!ae_jf_item_of("x@ws0", &id));
	CHECK(!ae_jf_item_of("../x@ws12", &id));
	CHECK(!ae_jf_item_of("x@ws99999999999999999999999", &id));
	CHECK(!ae_jf_item_of("mcc_maps\\123\\x", &id));
	CHECK(!ae_jf_item_of("mcc_maps\\x@ws12", &id));
	CHECK(!ae_jf_item_of("mcc_maps/x@ws12", &id));
	CHECK(!ae_jf_item_of("a\nb@ws12", &id));
	CHECK(!ae_jf_item_of("c:x@ws12", &id));
	CHECK(!ae_jf_item_of("", &id) && !ae_jf_item_of(NULL, &id));

	/* the gate */
	CHECK(ae_jf_gate(0, 1, 1, 4, 0) == AE_JF_OFF);
	CHECK(ae_jf_gate(1, 0, 1, 4, 0) == AE_JF_NOT_BUILT);
	CHECK(ae_jf_gate(1, 1, 0, 4, 0) == AE_JF_NO_MCC);
	CHECK(ae_jf_gate(1, 1, 1, 4, 0) == AE_JF_OK);
	CHECK(ae_jf_gate(1, 1, 1, 0, 0) == AE_JF_NOT_SIGNED_IN);
	CHECK(ae_jf_gate(1, 1, 1, 5, 0) == AE_JF_NOT_SIGNED_IN);
	CHECK(ae_jf_gate(1, 1, 1, 0, 1) == AE_JF_WAIT);
	CHECK(ae_jf_gate(1, 1, 1, 1, 0) == AE_JF_WAIT);
	CHECK(ae_jf_gate(1, 1, 1, 3, 0) == AE_JF_WAIT);

	/* the one budget: DOWNLOAD LIMIT less what was taken, and each server's 1 GB */
	CHECK(ae_jf_budget_left(0, 0, 0) == 0 && ae_jf_budget_left(-5, 0, 0) == 0);
	CHECK(ae_jf_budget_left(4096, 0, 0) == AE_JF_SERVER_BYTES_MAXIMUM);
	CHECK(ae_jf_budget_left(512, 0, 0) == (512ull << 20));
	CHECK(ae_jf_budget_left(512, 100ull << 20, 0) == (412ull << 20));
	CHECK(ae_jf_budget_left(4096, 0, 300ull << 20) == (724ull << 20));
	CHECK(ae_jf_budget_left(4096, 0, AE_JF_SERVER_BYTES_MAXIMUM) == 0);
	CHECK(ae_jf_budget_left(512, 600ull << 20, 0) == 0);
	CHECK(ae_jf_budget_left(65536, 0, 1) == AE_JF_SERVER_BYTES_MAXIMUM - 1);
	{
		static struct ae_jf_servers servers;

		CHECK(ae_jf_server_bytes(&servers, 5) == 0);
		ae_jf_server_add(&servers, 5, 100);
		ae_jf_server_add(&servers, 5, 50);
		ae_jf_server_add(&servers, 6, 7);
		ae_jf_server_add(&servers, 7, 0);
		CHECK(ae_jf_server_bytes(&servers, 5) == 150 && ae_jf_server_bytes(&servers, 6) == 7);
		CHECK(ae_jf_server_bytes(&servers, 7) == 0);
		/* (one address, two ports: two servers) */
		ae_jf_server_add(&servers, (0x0A000001ull << 16) | 5150, 10);
		ae_jf_server_add(&servers, (0x0A000001ull << 16) | 5151, 20);
		CHECK(ae_jf_server_bytes(&servers, (0x0A000001ull << 16) | 5150) == 10);
		CHECK(ae_jf_server_bytes(&servers, (0x0A000001ull << 16) | 5151) == 20);
		for (at = 0; at < 40; at++)
			ae_jf_server_add(&servers, 100 + (unsigned long long)at, 1);
		CHECK(ae_jf_server_bytes(&servers, 139) == 1);
		CHECK(ae_jf_server_bytes(&servers, 5) == 0);
	}

	/* the words */
	ae_jf_why_text(AE_JF_NOT_SIGNED_IN, text, sizeof(text));
	CHECK(!strcmp(text, "Sign in on SETTINGS > SYSTEM > MCC > STEAM ACCOUNT to get this server's Workshop maps."));
	ae_jf_why_text(AE_JF_OK, text, sizeof(text));
	CHECK(!text[0]);
	for (at = AE_JF_NOT_BUILT; at <= AE_JF_NOT_THE_MAP; at++)
	{
		ae_jf_why_text(at, text, sizeof(text));
		CHECK(text[0] && strlen(text) < 110);
	}
	ae_jf_why_text(AE_JF_NO_ROOM, text, 12);
	CHECK(strlen(text) == 11);
	ae_jf_start_text("Dredwerkz", 2, 7, text, sizeof(text));
	CHECK(!strcmp(text, "Getting \"Dredwerkz\" from Steam (2 of 7)."));
	ae_jf_start_text("Dredwerkz", 1, 1, text, sizeof(text));
	CHECK(!strcmp(text, "Getting \"Dredwerkz\" from Steam."));
	ae_jf_done_text("Dredwerkz", 2, 7, text, sizeof(text));
	CHECK(!strcmp(text, "Got \"Dredwerkz\" from Steam (2 of 7)."));
	ae_jf_progress_text(52ull << 20, 120ull << 20, text, sizeof(text));
	CHECK(!strcmp(text, "52 of 120 MB, from Steam."));
	ae_jf_progress_text(0, 0, text, sizeof(text));
	CHECK(!strcmp(text, "Asking Steam for it..."));

	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
