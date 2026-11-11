/*
AE_SERVER_LISTING_RULES_TEST.C

The AE master listing's pure rules (ae_server_listing_rules.c): a placeholder key is never usable, the AE extras block
is the master's canonical form, the signed text, and the browser's merge of what masters say.
*/

#include <stdio.h>
#include <string.h>

#include "../game/ae_server_listing_rules.h"

static int failures;

#define CHECK(condition) do { if (!(condition)) { printf("FAILED line %d: %s\n", __LINE__, #condition); failures++; } } while (0)

#define REAL "ed25519:89f4eda71229f11f3538679f49ef7fb0c2b407b380093f7f4fe02105b92225aa"

static void test_keys(void)
{
	unsigned char bytes[AE_LISTING_KEY_BYTES], other[AE_LISTING_KEY_BYTES];

	CHECK(ae_listing_key_usable(REAL, bytes));
	CHECK(bytes[0] == 0x89 && bytes[31] == 0xaa);
	/* a placeholder: all zeros, any one digit repeated, short, long, not hex, wrong prefix, NULL */
	CHECK(!ae_listing_key_usable("ed25519:0000000000000000000000000000000000000000000000000000000000000000", NULL));
	CHECK(!ae_listing_key_usable("ed25519:ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff", NULL));
	CHECK(!ae_listing_key_usable("ed25519:89f4", NULL));
	CHECK(!ae_listing_key_usable(REAL "0", NULL));
	CHECK(!ae_listing_key_usable("ed25519:zzf4eda71229f11f3538679f49ef7fb0c2b407b380093f7f4fe02105b92225aa", NULL));
	CHECK(!ae_listing_key_usable("ed2551x:89f4eda71229f11f3538679f49ef7fb0c2b407b380093f7f4fe02105b92225aa", NULL));
	CHECK(!ae_listing_key_usable(NULL, NULL));
	/* the AE master: the pinned key (when usable), or one of the extra keys */
	ae_listing_key_usable(REAL, other);
	CHECK(ae_listing_is_ae_master(other, REAL, NULL));
	CHECK(!ae_listing_is_ae_master(other, NULL, NULL));
	CHECK(!ae_listing_is_ae_master(other, "ed25519:0000000000000000000000000000000000000000000000000000000000000000", NULL));
	CHECK(ae_listing_is_ae_master(other, NULL, "ed25519:" "11223344556677881122334455667788112233445566778811223344556677ab, " REAL));
	CHECK(!ae_listing_is_ae_master(other, NULL, "ed25519:11223344556677881122334455667788112233445566778811223344556677ab"));
}

static void test_extras(void)
{
	char canonical[AE_LISTING_EXTRAS_SIZE], text[AE_LISTING_EXTRAS_SIZE + 64], tiny[8];

	CHECK(ae_listing_extras_canonical(canonical, sizeof(canonical), "ae", "us-west") > 0);
	CHECK(!strcmp(canonical, "{\"entries\":[],\"join_now\":\"ae\",\"join_range\":[\"ae\"],\"region\":\"us-west\"}"));
	/* the region cleaned: lowercase; one the master would refuse is empty */
	CHECK(ae_listing_extras_canonical(canonical, sizeof(canonical), "all", "EU-1") > 0);
	CHECK(strstr(canonical, "\"region\":\"eu-1\"}"));
	CHECK(ae_listing_extras_canonical(canonical, sizeof(canonical), "all", "Sao Paulo") > 0);
	CHECK(strstr(canonical, "\"region\":\"\"}"));
	CHECK(ae_listing_extras_canonical(canonical, sizeof(canonical), "all", "0123456789abcdefX") > 0);
	CHECK(strstr(canonical, "\"region\":\"\"}"));
	/* a join word must be a-z and + */
	CHECK(!ae_listing_extras_canonical(canonical, sizeof(canonical), "AE", ""));
	CHECK(!ae_listing_extras_canonical(canonical, sizeof(canonical), "", ""));
	CHECK(!ae_listing_extras_canonical(canonical, sizeof(canonical), "a\"b", ""));
	CHECK(!ae_listing_extras_canonical(tiny, sizeof(tiny), "ae", "us"));
	CHECK(ae_listing_signed_text(text, sizeof(text), "0123456789abcdef", "{}") > 0);
	CHECK(!strcmp(text, "AE-Extras-v1\n0123456789abcdef\n{}"));
	CHECK(!ae_listing_signed_text(tiny, sizeof(tiny), "0123456789abcdef", "{}"));
}

static void test_bots(void)
{
	char canonical[AE_LISTING_EXTRAS_SIZE], text[48];

	/* the block with bots: "bots" sorts first; not said, the block is as before */
	CHECK(ae_listing_extras_canonical_bots(canonical, sizeof(canonical), "all", "us-west", 8) > 0);
	CHECK(!strcmp(canonical, "{\"bots\":8,\"entries\":[],\"join_now\":\"all\",\"join_range\":[\"all\"],\"region\":\"us-west\"}"));
	CHECK(ae_listing_extras_canonical_bots(canonical, sizeof(canonical), "all", "us-west", 0) > 0 && strstr(canonical, "\"bots\":0,"));
	CHECK(ae_listing_extras_canonical_bots(canonical, sizeof(canonical), "all", "us-west", -1) > 0 && !strstr(canonical, "bots"));
	CHECK(!ae_listing_extras_canonical_bots(canonical, sizeof(canonical), "AE", "", 3));
	/* seats: bots are free seats; a bot-only server is empty; a server that said nothing reads as before */
	CHECK(ae_browser_seat_flags(0, 24, 24) == 2);
	CHECK(ae_browser_seat_flags(0, 16, 8) == 2);
	CHECK(ae_browser_seat_flags(3, 16, 8) == 0);
	CHECK(ae_browser_seat_flags(16, 16, 0) == 1);
	CHECK(ae_browser_seat_flags(24, 24, -1) == 1);
	CHECK(ae_browser_seat_flags(0, 24, -1) == 2);
	CHECK(ae_browser_seat_flags(1, 0, -1) == 0);
	ae_browser_players_text(text, sizeof(text), 0, 16, 8);
	CHECK(!strcmp(text, "0 / 16 \xC2\xB7 8 bots"));
	ae_browser_players_text(text, sizeof(text), 2, 16, 1);
	CHECK(!strcmp(text, "2 / 16 \xC2\xB7 1 bot"));
	ae_browser_players_text(text, sizeof(text), 2, 16, 0);
	CHECK(!strcmp(text, "2 / 16"));
	ae_browser_players_text(text, sizeof(text), 2, 16, -1);
	CHECK(!strcmp(text, "2 / 16"));
}

static void test_merge(void)
{
	/* one row per game: what several masters say of it merges to the richest word, the first among equals */
	CHECK(ae_browser_merge_word("", "ae"));
	CHECK(ae_browser_merge_word(NULL, "all"));
	CHECK(!ae_browser_merge_word("ae", "all"));
	CHECK(!ae_browser_merge_word("ae", ""));
	CHECK(!ae_browser_merge_word("", ""));
	CHECK(!strcmp(ae_browser_badge_word("all"), "ANYONE"));
	CHECK(!strcmp(ae_browser_badge_word("ae"), "AE"));
	CHECK(!strcmp(ae_browser_badge_word("ae+mcc"), "AE+MCC"));
	CHECK(!strcmp(ae_browser_badge_word("ae+workshop"), "AE+WORKSHOP"));
	CHECK(!strcmp(ae_browser_badge_word(""), "AE"));
}

static void test_address(void)
{
	char host[48];
	unsigned short port = 0;

	CHECK(ae_listing_address_split("69.9.176.16:10033", host, sizeof(host), &port) && !strcmp(host, "69.9.176.16") && port == 10033);
	CHECK(ae_listing_address_split("play.example-host.com:2302", host, sizeof(host), &port) && !strcmp(host, "play.example-host.com") && port == 2302);
	CHECK(!ae_listing_address_split("69.9.176.16", host, sizeof(host), &port));          /* no port */
	CHECK(!ae_listing_address_split("69.9.176.16:0", host, sizeof(host), &port));
	CHECK(!ae_listing_address_split("69.9.176.16:65536", host, sizeof(host), &port));
	CHECK(!ae_listing_address_split("69.9.176.16:12a", host, sizeof(host), &port));
	CHECK(!ae_listing_address_split("69.9.176.300:2302", host, sizeof(host), &port));    /* octet > 255 */
	CHECK(!ae_listing_address_split("69.9.176:2302", host, sizeof(host), &port));        /* three numbers */
	CHECK(!ae_listing_address_split("a..b:2302", host, sizeof(host), &port));
	CHECK(!ae_listing_address_split("a b:2302", host, sizeof(host), &port));
	CHECK(!ae_listing_address_split("[::1]:2302", host, sizeof(host), &port));
	CHECK(!ae_listing_address_split("a:b:2302", host, sizeof(host), &port));
	CHECK(!ae_listing_address_split(":2302", host, sizeof(host), &port));
	CHECK(!ae_listing_address_split("", host, sizeof(host), &port));
	CHECK(!ae_listing_address_split(NULL, host, sizeof(host), &port));
	CHECK(!ae_listing_address_split("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:1", host, sizeof(host), &port));
	/* a listed game joins direct only with a good address; else the invite (brokers) */
	CHECK(ae_browser_joins_direct("69.9.176.16:10033"));
	CHECK(!ae_browser_joins_direct(""));
	CHECK(!ae_browser_joins_direct(NULL));
	CHECK(!ae_browser_joins_direct("junk"));
}

static void test_name_tag(void)
{
	char out[33], name[40];

	ae_listing_name_tagged(out, sizeof(out), "player", 1, 32);
	CHECK(!strcmp(out, "player | haloae.com"));
	ae_listing_name_tagged(out, sizeof(out), "player", 0, 32);
	CHECK(!strcmp(out, "player"));
	/* (a long name cut to fit the 32 with the tag, no space left before it) */
	ae_listing_name_tagged(out, sizeof(out), "AE Official #1", 1, 32);
	CHECK(!strcmp(out, "AE Official #1" AE_LISTING_NAME_TAG));
	ae_listing_name_tagged(out, sizeof(out), "AE Official Server #1 West Coast", 1, 32);
	/* (19 kept before the 13 of the tag, the space before it trimmed) */
	CHECK(!strcmp(out, "AE Official Server" AE_LISTING_NAME_TAG));
	ae_listing_name_tagged(out, sizeof(out), "AE BTB+ #1          longer", 1, 32);
	CHECK(!strcmp(out, "AE BTB+ #1" AE_LISTING_NAME_TAG));
	ae_listing_name_tagged(out, sizeof(out), NULL, 1, 32);
	CHECK(!strcmp(out, AE_LISTING_NAME_TAG));
	/* (a field too small for the tag: the name alone) */
	ae_listing_name_tagged(out, 10, "player123456", 1, 32);
	CHECK(!strcmp(out, "player123"));
	strcpy(name, "player | haloae.com");
	ae_listing_name_untag(name);
	CHECK(!strcmp(name, "player"));
	strcpy(name, "plain");
	ae_listing_name_untag(name);
	CHECK(!strcmp(name, "plain"));
	ae_listing_name_untag(NULL);
}

int main(void)
{
	test_keys();
	test_extras();
	test_merge();
	test_bots();
	test_address();
	test_name_tag();
	if (failures)
		return 1;
	printf("ae_server_listing_rules_test: ok\n");
	return 0;
}
