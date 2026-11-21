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
		/* (the items of a server across its connections: at most AE_JF_ITEMS_MAXIMUM, kept beside its bytes) */
		{
			static struct ae_jf_servers items;
			unsigned long long key = (0x0A000002ull << 16) | 5150;

			CHECK(ae_jf_server_items(&items, key) == 0);
			for (at = 0; at < AE_JF_ITEMS_MAXIMUM; at++)
				CHECK(ae_jf_server_item_add(&items, key, 1000 + (unsigned long long)at));
			CHECK(!ae_jf_server_item_add(&items, key, 5000));
			/* (a rejoin asks for the same items again: not counted twice, and still let through) */
			CHECK(ae_jf_server_item_add(&items, key, 1000) && ae_jf_server_item_add(&items, key, 1000 + AE_JF_ITEMS_MAXIMUM - 1));
			CHECK(ae_jf_server_items(&items, key) == AE_JF_ITEMS_MAXIMUM);
			CHECK(ae_jf_server_items(&items, key) == AE_JF_ITEMS_MAXIMUM);
			ae_jf_server_add(&items, key, 9);
			CHECK(ae_jf_server_bytes(&items, key) == 9 && ae_jf_server_items(&items, key) == AE_JF_ITEMS_MAXIMUM);
			CHECK(ae_jf_server_item_add(&items, key + 1, 1000) && ae_jf_server_items(&items, key + 1) == 1);
			CHECK(ae_jf_server_bytes(&items, key + 1) == 0);
		}
	}

	/* a server's hold of a joiner fetching the game's Workshop map */
	CHECK(ae_jf_hold_allowed("flycam@ws2899709584"));
	CHECK(!ae_jf_hold_allowed("bloodgulch") && !ae_jf_hold_allowed("x@mcc") && !ae_jf_hold_allowed("") && !ae_jf_hold_allowed(NULL));
	CHECK(!ae_jf_hold_expired(1000, 1000 + AE_JF_HOLD_MAXIMUM_MS - 1));
	CHECK(ae_jf_hold_expired(1000, 1000 + AE_JF_HOLD_MAXIMUM_MS));
	CHECK(ae_jf_hold_expired(0, 5));

	/* (the game's map the records below belong to) */
#define MAP ae_jf_hold_map_key("flycam@ws2899709584")
	/* a server's hold records (a machine's address and join time) */
	{
		struct ae_jf_hold hold;
		unsigned long t;

		memset(&hold, 0, sizeof(hold));
		CHECK(!ae_jf_hold_active(&hold, 7, 100, MAP, 0, 1000));
		/* an honest fetch: held while it reports, not held once it is quiet */
		ae_jf_hold_progress(&hold, 7, 100, MAP, 0, 1000);
		CHECK(ae_jf_hold_active(&hold, 7, 100, MAP, 1, 1000 + 5000) && ae_jf_hold_active(&hold, 7, 100, MAP, 0, 1000 + 5000));
		ae_jf_hold_progress(&hold, 7, 100, MAP, 0, 1000 + 15000);
		CHECK(ae_jf_hold_active(&hold, 7, 100, MAP, 1, 1000 + 25000));
		CHECK(!ae_jf_hold_active(&hold, 7, 100, MAP, 1, 1000 + 15000 + AE_JF_HOLD_FRESH_MS + 1));
		/* another address, or the same slot's next join, is not held by it */
		CHECK(!ae_jf_hold_active(&hold, 8, 100, MAP, 1, 1000 + 5000) && !ae_jf_hold_active(&hold, 7, 101, MAP, 1, 1000 + 5000));

		/* attack 1: done again and again holds for one window only */
		memset(&hold, 0, sizeof(hold));
		ae_jf_hold_progress(&hold, 7, 100, MAP, 0, 1000);
		ae_jf_hold_progress(&hold, 7, 100, MAP, 1, 2000);
		CHECK(ae_jf_hold_active(&hold, 7, 100, MAP, 0, 2000 + AE_JF_HOLD_DONE_MS) && !ae_jf_hold_active(&hold, 7, 100, MAP, 1, 2000));
		for (t = 2000; t < 2000 + 10 * AE_JF_HOLD_DONE_MS; t += 10000)
			ae_jf_hold_progress(&hold, 7, 100, MAP, 1, t);
		CHECK(!ae_jf_hold_active(&hold, 7, 100, MAP, 0, 2000 + AE_JF_HOLD_DONE_MS + 1));
		ae_jf_hold_progress(&hold, 7, 100, MAP, 0, 2000 + 10 * AE_JF_HOLD_DONE_MS);
		CHECK(!ae_jf_hold_active(&hold, 7, 100, MAP, 0, 2000 + 10 * AE_JF_HOLD_DONE_MS));

		/* attack 2: a hold that ran its longest is final: a done or failed message and a new fetch do not renew it */
		memset(&hold, 0, sizeof(hold));
		for (t = 1000; t < 1000 + AE_JF_HOLD_MAXIMUM_MS; t += 2000)
			ae_jf_hold_progress(&hold, 7, 100, MAP, 0, t);
		CHECK(ae_jf_hold_active(&hold, 7, 100, MAP, 1, t - 2000));
		t = 1000 + AE_JF_HOLD_MAXIMUM_MS + 1000;
		CHECK(!ae_jf_hold_active(&hold, 7, 100, MAP, 1, t));
		CHECK(ae_jf_hold_lapsed(&hold, 7, 100, MAP, t) && ae_jf_hold_lapsed(&hold, 7, 100, MAP, t + 5));
		ae_jf_hold_progress(&hold, 7, 100, MAP, 2, t + 10);
		ae_jf_hold_progress(&hold, 7, 100, MAP, 0, t + 20);
		CHECK(!ae_jf_hold_active(&hold, 7, 100, MAP, 1, t + 30) && !ae_jf_hold_active(&hold, 7, 100, MAP, 0, t + 30));
		/* (the machine that takes the slot next starts clean) */
		ae_jf_hold_progress(&hold, 9, 5000, MAP, 0, t + 40);
		CHECK(ae_jf_hold_active(&hold, 9, 5000, MAP, 1, t + 50) && !ae_jf_hold_active(&hold, 7, 100, MAP, 1, t + 50));

		/* a failed fetch is final too, and a quiet one lapses */
		memset(&hold, 0, sizeof(hold));
		ae_jf_hold_progress(&hold, 7, 100, MAP, 0, 1000);
		ae_jf_hold_progress(&hold, 7, 100, MAP, 2, 2000);
		ae_jf_hold_progress(&hold, 7, 100, MAP, 0, 3000);
		CHECK(!ae_jf_hold_active(&hold, 7, 100, MAP, 1, 3000) && !ae_jf_hold_lapsed(&hold, 7, 100, MAP, 3000));
		memset(&hold, 0, sizeof(hold));
		ae_jf_hold_progress(&hold, 7, 100, MAP, 0, 1000);
		CHECK(ae_jf_hold_lapsed(&hold, 7, 100, MAP, 1000 + AE_JF_HOLD_FRESH_MS + 2));
		ae_jf_hold_progress(&hold, 7, 100, MAP, 0, 1000 + AE_JF_HOLD_FRESH_MS + 2);
		CHECK(!ae_jf_hold_active(&hold, 7, 100, MAP, 1, 1000 + AE_JF_HOLD_FRESH_MS + 2));
	}

	/* a new map is a new record: the hold of the old map does not hold, or lapse, the next game */
	{
		struct ae_jf_hold hold;
		unsigned long a = ae_jf_hold_map_key("flycam@ws2899709584"), b = ae_jf_hold_map_key("other@ws2899709585");

		CHECK(a && b && a != b && ae_jf_hold_map_key("") && ae_jf_hold_map_key(NULL));
		memset(&hold, 0, sizeof(hold));
		ae_jf_hold_progress(&hold, 7, 100, a, 0, 1000);
		CHECK(ae_jf_hold_active(&hold, 7, 100, a, 1, 2000) && !ae_jf_hold_active(&hold, 7, 100, b, 1, 2000));
		CHECK(!ae_jf_hold_lapsed(&hold, 7, 100, b, 1000 + AE_JF_HOLD_FRESH_MS + 5));
		/* (the joiner, held for the next map, has what is left of the join's 15 minutes, not a new 15) */
		ae_jf_hold_progress(&hold, 7, 100, b, 0, 1000 + AE_JF_HOLD_MAXIMUM_MS - 5);
		CHECK(ae_jf_hold_active(&hold, 7, 100, b, 1, 1000 + AE_JF_HOLD_MAXIMUM_MS - 3));
		CHECK(!ae_jf_hold_active(&hold, 7, 100, b, 1, 1000 + AE_JF_HOLD_MAXIMUM_MS + 100));
		/* a join that only ever sends done is bounded by the join's 15 minutes too: a window a map until they are used */
		memset(&hold, 0, sizeof(hold));
		ae_jf_hold_progress(&hold, 7, 100, a, 1, 1000);
		CHECK(ae_jf_hold_active(&hold, 7, 100, a, 0, 1000 + AE_JF_HOLD_DONE_MS));
		ae_jf_hold_progress(&hold, 7, 100, b, 1, 1000 + 600000);
		CHECK(ae_jf_hold_active(&hold, 7, 100, b, 0, 1000 + 600000 + 1000));
		ae_jf_hold_progress(&hold, 7, 100, a, 1, 1000 + 1200000);
		CHECK(!ae_jf_hold_active(&hold, 7, 100, a, 0, 1000 + 1200000 + 1000));
		CHECK(ae_jf_hold_lapsed(&hold, 7, 100, a, 1000 + 1200000 + 1000));
		/* a rotation of maps, a progress message every 2 s, the key changing every 10 minutes: the hold ends at 15 minutes
		for the join, and a lapse stays final on the maps after it */
		{
			unsigned long t, keys[3];
			int held_at_end = 0;

			keys[0] = a;
			keys[1] = b;
			keys[2] = ae_jf_hold_map_key("third@ws2899709586");
			memset(&hold, 0, sizeof(hold));
			for (t = 1000; t < 1000 + 40 * 60000ul; t += 2000)
			{
				unsigned long map = keys[(t - 1000) / 600000ul % 3];

				ae_jf_hold_progress(&hold, 7, 100, map, 0, t);
				if (t >= 1000 + AE_JF_HOLD_MAXIMUM_MS + 2000 && ae_jf_hold_active(&hold, 7, 100, map, 1, t))
					held_at_end++;
			}
			CHECK(held_at_end == 0);
			CHECK(ae_jf_hold_lapsed(&hold, 7, 100, keys[2], 1000 + 40 * 60000ul));
			/* (a, b, a again: nothing starts over) */
			ae_jf_hold_progress(&hold, 7, 100, a, 0, 1000 + 40 * 60000ul + 2000);
			CHECK(!ae_jf_hold_active(&hold, 7, 100, a, 1, 1000 + 40 * 60000ul + 2000));
		}
		/* (a done sent as the map changed gives the new map's record its one window, no more) */
		memset(&hold, 0, sizeof(hold));
		ae_jf_hold_progress(&hold, 7, 100, b, 1, 5000);
		CHECK(ae_jf_hold_active(&hold, 7, 100, b, 0, 5000 + AE_JF_HOLD_DONE_MS) && !ae_jf_hold_active(&hold, 7, 100, b, 0, 5000 + AE_JF_HOLD_DONE_MS + 1));
	}

	/* the clock wraps every 2^32 ms, on a 64-bit long too: a hold across the wrap runs and ends on time */
	{
		struct ae_jf_hold hold;
		unsigned long start = 0xFFFFFF00ul, wrapped = (unsigned long)(unsigned int)(start + 5000u);

		CHECK(ae_jf_hold_clock(0) == 1 && ae_jf_hold_clock(7) == 7);
		CHECK(!ae_jf_hold_expired(start, wrapped));
		CHECK(ae_jf_hold_expired(start, (unsigned long)(unsigned int)(start + AE_JF_HOLD_MAXIMUM_MS)));
		CHECK(!ae_jf_hold_expired(start, (unsigned long)(unsigned int)(start + AE_JF_HOLD_MAXIMUM_MS - 1)));
		memset(&hold, 0, sizeof(hold));
		ae_jf_hold_progress(&hold, 7, 100, MAP, 0, start);
		ae_jf_hold_progress(&hold, 7, 100, MAP, 0, wrapped);
		CHECK(ae_jf_hold_active(&hold, 7, 100, MAP, 1, wrapped));
		/* (quiet across the wrap: not held, and it lapses) */
		CHECK(!ae_jf_hold_active(&hold, 7, 100, MAP, 1, (unsigned long)(unsigned int)(wrapped + AE_JF_HOLD_FRESH_MS + 1)));
		CHECK(ae_jf_hold_lapsed(&hold, 7, 100, MAP, (unsigned long)(unsigned int)(wrapped + AE_JF_HOLD_FRESH_MS + 1)));
	}

	/* the words */
	ae_jf_why_text(AE_JF_NOT_SIGNED_IN, text, sizeof(text));
	CHECK(!strcmp(text, "Sign in on SETTINGS > SYSTEM > MCC > STEAM ACCOUNT to get this server's Workshop maps."));
	ae_jf_why_text(AE_JF_OK, text, sizeof(text));
	CHECK(!text[0]);
	for (at = AE_JF_NOT_BUILT; at <= AE_JF_TIMEOUT; at++)
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
