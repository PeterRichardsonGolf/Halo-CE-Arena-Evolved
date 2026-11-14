#include <stdio.h>
#include <string.h>
#include "ae_weapon_selection.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void test_sets(void)
{
	struct ae_weapon_id_set set;
	unsigned int id;

	ae_weapon_id_set_clear(&set);
	CHECK(ae_weapon_id_set_add(&set, 9) && ae_weapon_id_set_add(&set, 3) && ae_weapon_id_set_add(&set, 20));
	CHECK(!ae_weapon_id_set_add(&set, 3) && !ae_weapon_id_set_add(&set, 0));
	CHECK(set.count == 3 && set.ids[0] == 3 && set.ids[1] == 9 && set.ids[2] == 20);
	CHECK(ae_weapon_id_set_has(&set, 9) && !ae_weapon_id_set_has(&set, 10));
	CHECK(ae_weapon_id_set_remove(&set, 9) && !ae_weapon_id_set_remove(&set, 9) && set.count == 2 && set.ids[1] == 20);
	for (id = 1; id <= 100; id++)
		ae_weapon_id_set_add(&set, id);
	CHECK(set.count == AE_WEAPON_SELECTION_MAX_NAMED);
}

/* legacy ids stay in the legacy bytes, byte for byte as 0.7.5; a selection of ids 1-16 writes no AECW */
static void test_legacy_bytes(void)
{
	struct ae_weapon_selection selection, back;
	struct ae_weapon_selection_bytes bytes;
	unsigned int id;

	for (id = 1; id <= 16; id++)
	{
		ae_weapon_selection_clear(&selection);
		ae_weapon_id_set_add(&selection.named, id);
		ae_weapon_selection_write(&bytes, &selection);
		CHECK(bytes.weapon_pack == (id <= 8 ? 1u << (id - 1) : 0));
		CHECK(bytes.weapon_pack_high == (id > 8 ? 1u << (id - 9) : 0));
		CHECK(!bytes.aecw_present && !bytes.grenades && !bytes.pack_start[0]);
		ae_weapon_selection_read(&back, &bytes, NULL);
		CHECK(back.named.count == 1 && back.named.ids[0] == id && back.uses_cache);
	}
	/* (0.7.5's file: the mask 0x2F, START 3 and 2, nothing past it) */
	memset(&bytes, 0, sizeof(bytes));
	bytes.weapon_pack = 0x2F;
	bytes.pack_start[0] = 3;
	bytes.pack_start[1] = 2;
	ae_weapon_selection_read(&back, &bytes, NULL);
	CHECK(back.named.count == 5 && back.start[0].weapon == 3 && back.start[1].weapon == 2 && !back.start[0].config);
	ae_weapon_selection_write(&bytes, &back);
	CHECK(bytes.weapon_pack == 0x2F && bytes.pack_start[0] == 3 && bytes.pack_start[1] == 2 && !bytes.aecw_present);
	/* (nothing named: not a cache game; the rank rules' START applies when no START is stored) */
	memset(&bytes, 0, sizeof(bytes));
	{
		unsigned int ranks[2] = { 4, 0 };

		ae_weapon_selection_read(&back, &bytes, ranks);
		CHECK(back.start[0].weapon == 4 && back.start[1].weapon == 0);
		CHECK(back.named.count == 1 && back.named.ids[0] == 4);
	}
	ae_weapon_selection_read(&back, &bytes, NULL);
	CHECK(!back.uses_cache);
}

#define CFG(n) (AE_WEAPON_SELECTION_CONFIG_BASE + (n))

/* sign nothing: the block's own bytes only (the signature is the saved-game checksum, tested in the engine build) */
static void make_block(unsigned char *block, unsigned int version, unsigned int size, unsigned char const *payload)
{
	memset(block, 0, AE_WEAPON_SELECTION_AECW_SIZE);
	block[0] = 'A'; block[1] = 'E'; block[2] = 'C'; block[3] = 'W';
	block[4] = (unsigned char)version; block[5] = (unsigned char)(version >> 8);
	block[6] = (unsigned char)size; block[7] = (unsigned char)(size >> 8);
	memcpy(block + 8, payload, AE_WEAPON_SELECTION_AECW_PAYLOAD_SIZE);
}

/* AECW VERSION 3: the toggle, the set id, the START pairs; byte map pinned */
static void test_aecw(void)
{
	struct ae_weapon_selection selection, back;
	struct ae_weapon_selection_bytes bytes, decoded;
	unsigned char block[AE_WEAPON_SELECTION_AECW_SIZE];
	unsigned char payload[AE_WEAPON_SELECTION_AECW_PAYLOAD_SIZE];

	/* (the sizes: 8 + 12 + 20 = 40 bytes, 0x80-0xA8, inside the proven-free window 0x7C-0x100) */
	CHECK(AE_WEAPON_SELECTION_AECW_SIZE == 40 && AE_WEAPON_SELECTION_AECW_PAYLOAD_SIZE == 12);
	CHECK(AE_WEAPON_SELECTION_AECW_OFFSET + AE_WEAPON_SELECTION_AECW_WINDOW == 0x100);
	CHECK(AE_WEAPON_SELECTION_AECW_VERSION == 3);

	ae_weapon_selection_clear(&selection);
	ae_weapon_id_set_add(&selection.named, 2);
	selection.ae_weapons = 1;
	selection.set_id = 2;
	selection.start[0].weapon = 24;
	selection.start[0].config = CFG(3);
	selection.start[1].weapon = 40;
	ae_weapon_selection_write(&bytes, &selection);
	CHECK(bytes.weapon_pack == (1u << 1) && bytes.aecw_present);
	/* (the byte map: flags, 0, set, 0, start 0 = weapon u16 + config u16, start 1) */
	CHECK(bytes.aecw[0] == 1 && bytes.aecw[1] == 0 && bytes.aecw[2] == 2 && bytes.aecw[3] == 0);
	CHECK(bytes.aecw[4] == 24 && bytes.aecw[5] == 0 && bytes.aecw[6] == 3 && bytes.aecw[7] == 0);
	CHECK(bytes.aecw[8] == 40 && bytes.aecw[9] == 0 && bytes.aecw[10] == 0 && bytes.aecw[11] == 0);
	CHECK(ae_weapon_selection_aecw_encode(&bytes, block) == AE_WEAPON_SELECTION_AECW_SIZE);
	CHECK(!memcmp(block, "AECW", 4) && block[4] == 3 && block[5] == 0 && block[6] == 12 && block[7] == 0);
	CHECK(!memcmp(block + 8, bytes.aecw, 12) && block[20] == 0);
	memset(&decoded, 0, sizeof(decoded));
	CHECK(ae_weapon_selection_aecw_decode(&decoded, block));
	decoded.weapon_pack = bytes.weapon_pack;
	ae_weapon_selection_read(&back, &decoded, NULL);
	CHECK(back.ae_weapons == 1 && back.set_id == 2 && back.start[0].weapon == 24 && back.start[0].config == CFG(3) &&
		back.start[1].weapon == 40 && !back.start[1].config && back.uses_cache && back.named.count == 1);

	/* (exactly version 3: 1 and 2, 4, a wrong size, a wrong magic, a nonzero reserved byte, an unknown flag, a set past
	15 are all refused) */
	memcpy(payload, bytes.aecw, sizeof(payload));
	{
		unsigned int version;

		for (version = 0; version < 6; version++)
		{
			make_block(block, version, 12, payload);
			CHECK(ae_weapon_selection_aecw_decode(&decoded, block) == (version == 3));
		}
	}
	make_block(block, 3, 11, payload);
	CHECK(!ae_weapon_selection_aecw_decode(&decoded, block));
	make_block(block, 3, 84, payload);
	CHECK(!ae_weapon_selection_aecw_decode(&decoded, block));
	make_block(block, 3, 12, payload);
	block[0] = 'X';
	CHECK(!ae_weapon_selection_aecw_decode(&decoded, block));
	payload[1] = 1;
	make_block(block, 3, 12, payload);
	CHECK(!ae_weapon_selection_aecw_decode(&decoded, block));
	payload[1] = 0;
	payload[3] = 1;
	make_block(block, 3, 12, payload);
	CHECK(!ae_weapon_selection_aecw_decode(&decoded, block));
	payload[3] = 0;
	payload[0] = 3;
	make_block(block, 3, 12, payload);
	CHECK(!ae_weapon_selection_aecw_decode(&decoded, block));
	payload[0] = 1;
	payload[2] = 16;
	make_block(block, 3, 12, payload);
	CHECK(!ae_weapon_selection_aecw_decode(&decoded, block));
	payload[2] = 15;
	make_block(block, 3, 12, payload);
	CHECK(ae_weapon_selection_aecw_decode(&decoded, block));
	/* (an old 128-byte version 2 block of 84-byte-era layout: refused whole) */
	{
		unsigned char old[AE_WEAPON_SELECTION_AECW_SIZE];

		make_block(old, 2, 100, payload);
		CHECK(!ae_weapon_selection_aecw_decode(&decoded, old));
	}
	/* (an empty payload writes nothing) */
	memset(&bytes, 0, sizeof(bytes));
	CHECK(ae_weapon_selection_aecw_encode(&bytes, block) == 0);
}

/* the toggle forces uses_cache (every set can draw a cache weapon), even with nothing else named; the set id lives only
with the toggle on; a set past the shipped range reads as casual; START in the legacy byte writes no AECW */
static void test_toggle_and_set(void)
{
	struct ae_weapon_selection selection, back;
	struct ae_weapon_selection_bytes bytes;

	ae_weapon_selection_clear(&selection);
	selection.ae_weapons = 1;
	ae_weapon_selection_derive(&selection);
	CHECK(selection.uses_cache && selection.named.count == 0);
	ae_weapon_selection_write(&bytes, &selection);
	CHECK(bytes.aecw_present && bytes.aecw[0] == 1 && bytes.aecw[2] == 0 && !bytes.weapon_pack && !bytes.grenades);
	ae_weapon_selection_read(&back, &bytes, NULL);
	CHECK(back.ae_weapons == 1 && back.set_id == 0 && back.uses_cache);
	/* (toggle off: the set id is dropped, nothing is written, not a cache game) */
	ae_weapon_selection_clear(&selection);
	selection.set_id = 2;
	ae_weapon_selection_write(&bytes, &selection);
	CHECK(!bytes.aecw_present && bytes.aecw[2] == 0);
	ae_weapon_selection_read(&back, &bytes, NULL);
	CHECK(!back.uses_cache && back.set_id == 0);
	/* (a set past 15 reads as casual) */
	ae_weapon_selection_clear(&selection);
	selection.ae_weapons = 1;
	selection.set_id = 99;
	ae_weapon_selection_sanitize(&selection);
	CHECK(selection.set_id == 0);
	selection.set_id = 15;
	ae_weapon_selection_sanitize(&selection);
	CHECK(selection.set_id == 15);
	/* (a config start of a legacy weapon goes to AECW and keeps pack_start 0; a plain one stays legacy) */
	ae_weapon_selection_clear(&selection);
	selection.start[0].weapon = 5;
	selection.start[0].config = CFG(1);
	selection.start[1].weapon = 6;
	ae_weapon_selection_write(&bytes, &selection);
	CHECK(bytes.pack_start[0] == 0 && bytes.pack_start[1] == 6 && bytes.aecw_present && bytes.aecw[4] == 5 && bytes.aecw[6] == 1);
	ae_weapon_selection_read(&back, &bytes, NULL);
	CHECK(back.start[0].weapon == 5 && back.start[0].config == CFG(1) && back.start[1].weapon == 6);
	/* (named: only the legacy 16; a start past 16 needs no named id) */
	ae_weapon_selection_clear(&selection);
	ae_weapon_id_set_add(&selection.named, 17);
	ae_weapon_id_set_add(&selection.named, 3);
	selection.start[0].weapon = 30;
	ae_weapon_selection_sanitize(&selection);
	CHECK(selection.named.count == 1 && selection.named.ids[0] == 3 && selection.start[0].weapon == 30);
	CHECK(!ae_weapon_selection_can_name(&selection, 17) && ae_weapon_selection_can_name(&selection, 16));
}

/* THE NO-CACHE GUARD (design 8.5): a network game that does not use the cache refuses cache objects; offline never */
static void test_guard(void)
{
	CHECK(ae_weapon_selection_guard_refuses(1, 0));
	CHECK(!ae_weapon_selection_guard_refuses(1, 1));
	CHECK(!ae_weapon_selection_guard_refuses(0, 0));
	CHECK(!ae_weapon_selection_guard_refuses(0, 1));
}

/* OLD BUILDS JOIN ONLY NO-AE-WEAPON GAMES (owner): a selection with any weapon, config, grenade type or the toggle uses
the cache, so the join gate refuses a build without the cache ("update"); an empty one does not */
static void test_old_builds_join_only_no_weapon_games(void)
{
	struct ae_weapon_selection selection;

	ae_weapon_selection_clear(&selection);
	CHECK(!selection.uses_cache);
	ae_weapon_selection_derive(&selection);
	CHECK(!selection.uses_cache);
	ae_weapon_id_set_add(&selection.named, 1);
	ae_weapon_selection_derive(&selection);
	CHECK(selection.uses_cache);
	ae_weapon_selection_clear(&selection);
	selection.start[0].config = CFG(1);
	ae_weapon_selection_derive(&selection);
	CHECK(selection.uses_cache);
	ae_weapon_selection_clear(&selection);
	selection.start[0].weapon = 3;
	ae_weapon_selection_derive(&selection);
	CHECK(selection.uses_cache);
	ae_weapon_selection_clear(&selection);
	selection.grenades = 2;
	ae_weapon_selection_derive(&selection);
	CHECK(selection.uses_cache);
	ae_weapon_selection_clear(&selection);
	selection.ae_weapons = 1;
	ae_weapon_selection_derive(&selection);
	CHECK(selection.uses_cache);
}

static void test_sanitize(void)
{
	struct ae_weapon_selection selection;
	unsigned int id;

	/* (ids past the legacy 16 go) */
	ae_weapon_selection_clear(&selection);
	for (id = 1; id <= 40; id++)
		ae_weapon_id_set_add(&selection.named, id);
	ae_weapon_selection_sanitize(&selection);
	CHECK(selection.named.count == 16 && selection.named.ids[15] == 16);
	/* (a plain legacy start names its weapon; a config start does not; a config without a weapon goes) */
	ae_weapon_selection_clear(&selection);
	selection.start[0].weapon = 3;
	selection.start[1].weapon = 30;
	selection.start[1].config = CFG(2);
	ae_weapon_selection_sanitize(&selection);
	CHECK(ae_weapon_id_set_has(&selection.named, 3) && !ae_weapon_id_set_has(&selection.named, 30) &&
		selection.start[1].config == CFG(2));
	ae_weapon_selection_clear(&selection);
	selection.start[0].config = 3;
	ae_weapon_selection_sanitize(&selection);
	CHECK(selection.start[0].config == 0 && !selection.uses_cache);
	ae_weapon_selection_clear(&selection);
	selection.start[0].weapon = 0x10000;
	selection.start[1].weapon = 7;
	selection.start[1].config = CFG(0x10000);
	ae_weapon_selection_sanitize(&selection);
	CHECK(selection.start[0].weapon == 0 && selection.start[1].weapon == 7 && selection.start[1].config == 0);
	ae_weapon_selection_clear(&selection);
	selection.grenades = 1;
	ae_weapon_selection_derive(&selection);
	CHECK(selection.uses_cache);
}

/* a 0.7.6 test build's grenade mask in pack_start's bits 5-7 is read once, and moved to the byte on the next write */
static void test_old_grenade_bits(void)
{
	struct ae_weapon_selection selection;
	struct ae_weapon_selection_bytes bytes, written;

	memset(&bytes, 0, sizeof(bytes));
	bytes.pack_start[0] = 0x20 | 0x80 | 3;
	bytes.pack_start[1] = 0x40 | 2;
	ae_weapon_selection_read(&selection, &bytes, NULL);
	CHECK(selection.grenades == (0x05 | (0x02 << 3)));
	CHECK(selection.start[0].weapon == 3 && selection.start[1].weapon == 2);
	ae_weapon_selection_write(&written, &selection);
	CHECK(written.grenades == 0x15 && written.pack_start[0] == 3 && written.pack_start[1] == 2);
	/* (the byte wins when it is set) */
	bytes.grenades = 0x01;
	ae_weapon_selection_read(&selection, &bytes, NULL);
	CHECK(selection.grenades == 1);
}

int main(void)
{
	test_sets();
	test_legacy_bytes();
	test_aecw();
	test_toggle_and_set();
	test_guard();
	test_old_builds_join_only_no_weapon_games();
	test_sanitize();
	test_old_grenade_bits();
	printf("ae_weapon_selection: %d checks, %d failed\n", checks, failures);
	return failures != 0;
}
