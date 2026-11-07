#include <stdio.h>
#include <string.h>
#include "ae_silent_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define ME 0x00020001L
#define OTHER 0x00030002L

static unsigned char byte_of(int movement, int weapons)
{
	return ae_silent_byte_set(ae_silent_byte_set(0, AE_SILENT_ROLE_MOVEMENT, movement), AE_SILENT_ROLE_WEAPON, weapons);
}

/* every mode of both options, for both roles and for the listener's own sound and another's: muted exactly as the
table of the rules says */
static void test_mute_table(void)
{
	int movement, weapons, role;

	for (movement = 0; movement < NUMBER_OF_AE_SILENT_MODES; movement++)
	{
		for (weapons = 0; weapons < NUMBER_OF_AE_SILENT_MODES; weapons++)
		{
			unsigned char byte = byte_of(movement, weapons);

			CHECK(ae_silent_mode(byte, AE_SILENT_ROLE_MOVEMENT) == movement);
			CHECK(ae_silent_mode(byte, AE_SILENT_ROLE_WEAPON) == weapons);
			CHECK((byte & ~AE_SILENT_BYTE_MASK) == 0);
			CHECK(ae_silent_any(byte) == (movement != AE_SILENT_NORMAL || weapons != AE_SILENT_NORMAL));
			for (role = 0; role < NUMBER_OF_AE_SILENT_ROLES; role++)
			{
				int mode = role == AE_SILENT_ROLE_MOVEMENT ? movement : role == AE_SILENT_ROLE_WEAPON ? weapons :
					AE_SILENT_NORMAL;
				int own = ae_silent_muted(byte, role, ME, ME);
				int others = ae_silent_muted(byte, role, OTHER, ME);

				/* NORMAL: nobody muted; JUST ME: only others' sounds; SILENT: everyone's, own included */
				CHECK(own == (mode == AE_SILENT_SILENT));
				CHECK(others == (mode != AE_SILENT_NORMAL));
				/* (an owner not known, or a listener with no player: muted unless NORMAL) */
				CHECK(ae_silent_muted(byte, role, -1, ME) == (mode != AE_SILENT_NORMAL));
				CHECK(ae_silent_muted(byte, role, ME, -1) == (mode != AE_SILENT_NORMAL));
			}
			/* the other role (firing, reloading, pickups...) is never muted */
			CHECK(!ae_silent_muted(byte, AE_SILENT_ROLE_NONE, OTHER, ME));
			CHECK(!ae_silent_muted(byte, AE_SILENT_ROLE_NONE, -1, -1));
		}
	}
}

/* the byte: the high nibble only, its other bits kept (the weapon pack's low nibble), 3 read as SILENT, an older
gametype's 0 NORMAL */
static void test_byte(void)
{
	unsigned char byte;

	CHECK(ae_silent_mode(0, AE_SILENT_ROLE_MOVEMENT) == AE_SILENT_NORMAL);
	CHECK(ae_silent_mode(0, AE_SILENT_ROLE_WEAPON) == AE_SILENT_NORMAL);
	CHECK(!ae_silent_any(0));
	CHECK(!ae_silent_any(0x0F));
	byte = ae_silent_byte_set(0x0F, AE_SILENT_ROLE_MOVEMENT, AE_SILENT_JUST_ME);
	CHECK(byte == 0x1F);
	byte = ae_silent_byte_set(byte, AE_SILENT_ROLE_WEAPON, AE_SILENT_SILENT);
	CHECK(byte == 0x9F);
	byte = ae_silent_byte_set(byte, AE_SILENT_ROLE_MOVEMENT, AE_SILENT_NORMAL);
	CHECK(byte == 0x8F);
	CHECK(ae_silent_mode(0x30, AE_SILENT_ROLE_MOVEMENT) == AE_SILENT_SILENT);
	CHECK(ae_silent_mode(0xC0, AE_SILENT_ROLE_WEAPON) == AE_SILENT_SILENT);
	CHECK(ae_silent_mode(0xFF, AE_SILENT_ROLE_NONE) == AE_SILENT_NORMAL);
	CHECK(ae_silent_byte_set(0x5A, AE_SILENT_ROLE_NONE, AE_SILENT_SILENT) == 0x5A);
	/* (a mode out of range is written as SILENT, never as 3) */
	CHECK(ae_silent_byte_set(0, AE_SILENT_ROLE_WEAPON, 7) == 0x80);
	CHECK(ae_silent_split_screen(byte_of(AE_SILENT_SILENT, AE_SILENT_SILENT)));
	CHECK(!ae_silent_split_screen(byte_of(AE_SILENT_SILENT, AE_SILENT_JUST_ME)));
	CHECK(!ae_silent_split_screen(byte_of(AE_SILENT_NORMAL, AE_SILENT_SILENT)));
	CHECK(!strcmp(ae_silent_mode_name(AE_SILENT_JUST_ME), "just me"));
	CHECK(!strcmp(ae_silent_role_name(AE_SILENT_ROLE_WEAPON), "weapons"));
}

/* the roles by event: a weapon's ready / put-away only; a unit's actions 3 and 4 only */
static void test_roles(void)
{
	int action;

	CHECK(ae_silent_weapon_role(1) == AE_SILENT_ROLE_WEAPON);
	CHECK(ae_silent_weapon_role(0) == AE_SILENT_ROLE_NONE);
	for (action = 0; action < 12; action++)
		CHECK(ae_silent_unit_action_role(action) == (action == 3 || action == 4 ? AE_SILENT_ROLE_WEAPON : AE_SILENT_ROLE_NONE));
}

/* the join's mark: written after the name's end, read back; a stock join (zeros), a full-length name and a name
running into the mark tell nothing */
static void test_join(void)
{
	unsigned short name[32];
	int index;
	int movement, weapons;

	memset(name, 0, sizeof(name));
	CHECK(ae_silent_join_capability(name, 32) == 0);
	name[0] = 'B'; name[1] = 'o'; name[2] = 'b';
	ae_silent_join_mark(name, 32);
	CHECK(name[0] == 'B' && name[2] == 'b' && name[3] == 0);
	CHECK(ae_silent_join_capability(name, 32) == AE_SILENT_CAPABILITY);
	/* (a 31-character name loses its last character to the end, and keeps the mark) */
	for (index = 0; index < 32; index++)
		name[index] = 'x';
	name[31] = 0;
	ae_silent_join_mark(name, 32);
	CHECK(name[29] == 'x' && name[30] == 0);
	CHECK(ae_silent_join_capability(name, 32) == AE_SILENT_CAPABILITY);
	/* (32 characters and no end, the last one the mark's: not a mark) */
	for (index = 0; index < 32; index++)
		name[index] = 'x';
	name[31] = (unsigned short)(AE_SILENT_JOIN_MARK | 1);
	CHECK(ae_silent_join_capability(name, 32) == 0);
	/* (another last character: no mark) */
	memset(name, 0, sizeof(name));
	name[31] = 0xAE40;
	CHECK(ae_silent_join_capability(name, 32) == 0);
	CHECK(ae_silent_join_capability(NULL, 32) == 0);
	/* 0.7.6: the feature mark (the grenade layer's version) three before the end, beside the mark; the cache block is written
	below it. Hosts of before read the mark as it was */
	CHECK(ae_silent_join_feature_grenades(0) == 0 && ae_silent_join_feature_grenades(1) == 0x03 &&
		ae_silent_join_feature_grenades(2) == 0x05 && ae_silent_join_feature_grenades(9) == 0x0F);
	CHECK(ae_silent_join_grenade_version(0) == 0 && ae_silent_join_grenade_version(0x03) == 1 &&
		ae_silent_join_grenade_version(0x0F) == 7 && ae_silent_join_grenade_version(0x02) == 0);
	memset(name, 0, sizeof(name));
	name[0] = 'B'; name[1] = 'o'; name[2] = 'b';
	ae_silent_join_mark(name, 32);
	ae_silent_join_features(name, 32, ae_silent_join_feature_grenades(1));
	CHECK(name[0] == 'B' && name[2] == 'b' && name[3] == 0);
	CHECK(ae_silent_join_capability(name, 32) == 1);
	CHECK(ae_silent_join_features_told(name, 32) == 0x03 && ae_silent_join_grenade_version(0x03) == 1);
	CHECK(name[29] == (unsigned short)(AE_SILENT_JOIN_FEATURE_MARK | 0x03));
	/* (a long name is cut four before the end) */
	for (index = 0; index < 32; index++)
		name[index] = 'x';
	name[31] = 0;
	ae_silent_join_mark(name, 32);
	ae_silent_join_features(name, 32, 0x03);
	CHECK(name[27] == 'x' && name[28] == 0 && name[30] == 0 && ae_silent_join_capability(name, 32) == 1 &&
		ae_silent_join_features_told(name, 32) == 0x03);
	/* (a name not marked, no bits, a short buffer: left as they are; nothing told without the mark or the name's end) */
	memset(name, 0, sizeof(name));
	name[0] = 'B';
	ae_silent_join_features(name, 32, 0x03);
	CHECK(name[29] == 0 && ae_silent_join_features_told(name, 32) == 0);
	ae_silent_join_mark(name, 32);
	ae_silent_join_features(name, 32, 0);
	CHECK(name[29] == 0 && ae_silent_join_features_told(name, 32) == 0);
	ae_silent_join_features(NULL, 32, 0x03);
	CHECK(ae_silent_join_features_told(NULL, 32) == 0);
	for (index = 0; index < 32; index++)
		name[index] = 'x';
	name[29] = (unsigned short)(AE_SILENT_JOIN_FEATURE_MARK | 0x03);
	name[31] = (unsigned short)(AE_SILENT_JOIN_MARK | 1);
	CHECK(ae_silent_join_features_told(name, 32) == 0);
	name[5] = 0;
	CHECK(ae_silent_join_features_told(name, 32) == 0x03);
	name[29] = (unsigned short)(AE_SILENT_JOIN_FEATURE_MARK | 0x05);
	CHECK(ae_silent_join_features_told(name, 32) == 0x05 && ae_silent_join_grenade_version(0x05) == 2);
	/* refusals: only while an option is on, only a machine without the capability, never the host's own */
	for (movement = 0; movement < NUMBER_OF_AE_SILENT_MODES; movement++)
	{
		for (weapons = 0; weapons < NUMBER_OF_AE_SILENT_MODES; weapons++)
		{
			unsigned char byte = byte_of(movement, weapons);
			int on = movement != AE_SILENT_NORMAL || weapons != AE_SILENT_NORMAL;

			CHECK(ae_silent_join_refused(byte, 0, 0) == on);
			CHECK(!ae_silent_join_refused(byte, AE_SILENT_CAPABILITY, 0));
			CHECK(!ae_silent_join_refused(byte, 0, 1));
		}
	}
}


/* the weapon cache's block: written last below the marks, read back, the older readers unmoved, the decision */
static void test_cache_block(void)
{
	unsigned short name[64], plain[64];
	unsigned char digest[4] = { 0x11, 0x22, 0x33, 0x44 }, got[4];
	unsigned int format, generation;
	int length;

	for (length = 8; length <= 48; length += (length < 32 ? 8 : 16))
	{
		int level, grenades;

		for (level = 0; level <= 2; level++)
		{
			memset(name, 0, sizeof(name));
			name[0] = 'B'; name[1] = 'o'; name[2] = 'b';
			ae_silent_join_mark(name, length);
			/* (the widen's level character and the grenade layer's feature mark, which are not in this tree: written by
			hand where those builds put them, the name cut before them) */
			if (length >= 5 && level == 2)
			{
				name[length - 3] = 0;
				name[length - 2] = 0xAD00 | 24;
				name[length - 1] = AE_SILENT_JOIN_MARK | 15;
			}
			grenades = length >= 5;
			if (grenades)
			{
				int at;

				for (at = 0; at < length - 4 && name[at]; at++)
					;
				for (; at <= length - 4; at++)
					name[at] = 0;
				name[length - 3] = 0xAC00 | 0x07;
			}
			memcpy(plain, name, sizeof(name));
			ae_silent_join_cache(name, length, 1, 7, digest);
			CHECK(ae_silent_join_cache_told(name, length, &format, &generation, got));
			CHECK(format == 1 && generation == 7 && !memcmp(got, digest, 4));
			/* (the older readers read the same) */
			CHECK(ae_silent_join_capability(name, length) == ae_silent_join_capability(plain, length));
			CHECK(memcmp(name + length - 3, plain + length - 3, 3 * sizeof(unsigned short)) == 0);
			CHECK(!ae_silent_join_cache_told(plain, length, NULL, NULL, NULL));
		}
	}
	/* (the name cut at index length - 8; the marks at 29, 30, 31 and the block at 25-28 of 32) */
	memset(name, 0, sizeof(name));
	{
		int index;

		for (index = 0; index < 28; index++)
			name[index] = 'x';
	}
	ae_silent_join_mark(name, 32);
	ae_silent_join_cache(name, 32, 255, 0xFFFF, digest);
	CHECK(name[24] == 0 && name[23] == 'x' && name[28] == (AE_SILENT_JOIN_CACHE_MARK | 255) && name[27] == 0xFFFF);
	CHECK(name[26] == 0x2211 && name[25] == 0x4433 && name[31] == (AE_SILENT_JOIN_MARK | AE_SILENT_CAPABILITY));
	/* (an unmarked name takes no block; a name too short either; a name running into the block is no block) */
	memset(name, 0, sizeof(name));
	ae_silent_join_cache(name, 32, 1, 1, digest);
	CHECK(name[28] == 0);
	ae_silent_join_mark(name, 32);
	ae_silent_join_cache(name, 7, 1, 1, digest);
	CHECK(!ae_silent_join_cache_told(name, 7, NULL, NULL, NULL));
	ae_silent_join_cache(name, 32, 1, 1, digest);
	{
		int index;

		for (index = 0; index < 25; index++)
			name[index] = 'y';
	}
	CHECK(!ae_silent_join_cache_told(name, 32, NULL, NULL, NULL));
	/* (the decision, design 8.3) */
	{
		unsigned char host[4] = { 1, 2, 3, 4 }, other[4] = { 1, 2, 3, 5 };

		CHECK(ae_silent_join_cache_decide(0, 0, 0, 0, NULL, 1, 3, host) == AE_CACHE_JOIN_ACCEPT);
		CHECK(ae_silent_join_cache_decide(1, 0, 0, 0, NULL, 1, 3, host) == AE_CACHE_JOIN_NO_BLOCK);
		CHECK(ae_silent_join_cache_decide(1, 1, 2, 3, host, 1, 3, host) == AE_CACHE_JOIN_FORMAT);
		CHECK(ae_silent_join_cache_decide(1, 1, 1, 2, host, 1, 3, host) == AE_CACHE_JOIN_CLIENT_OLDER);
		CHECK(ae_silent_join_cache_decide(1, 1, 1, 4, host, 1, 3, host) == AE_CACHE_JOIN_HOST_OLDER);
		CHECK(ae_silent_join_cache_decide(1, 1, 1, 3, other, 1, 3, host) == AE_CACHE_JOIN_DIFFERENT_BUILD);
		CHECK(ae_silent_join_cache_decide(1, 1, 1, 3, host, 1, 3, host) == AE_CACHE_JOIN_ACCEPT);
		CHECK(AE_CACHE_REJECTION_CODE == 69);
	}
}

/* CODE 69 WIDTH: the four cases ride the HIGH BYTE of the 16-bit rejection word (69 | case << 8). Whether the network layer
carries the whole word from the host's drop to the client's ae_silent_join_rejected is NOT yet shown (integration check
pending); this pins the word the rules make, so a layer that keeps one byte shows here and in play, not silently */
static void test_cache_code_width(void)
{
	int cache_case;

	CHECK(ae_cache_rejection_word(AE_CACHE_JOIN_NO_BLOCK) == AE_PACKS_REJECTION_CODE);
	CHECK(ae_cache_rejection_case(AE_PACKS_REJECTION_CODE) == -1);
	for (cache_case = AE_CACHE_JOIN_FORMAT; cache_case <= AE_CACHE_JOIN_DIFFERENT_BUILD; cache_case++)
	{
		unsigned int word = ae_cache_rejection_word(cache_case);

		CHECK((word & 0xFFu) == 69 && (word >> 8) == (unsigned int)cache_case && word > 0xFFu && word <= 0xFFFFu);
		CHECK(ae_cache_rejection_case(word) == cache_case);
		/* (the full word survives a 16-bit round trip and a short; a low byte alone loses the case) */
		CHECK(ae_cache_rejection_case((unsigned short)(short)word) == cache_case);
		CHECK(ae_cache_rejection_case(word & 0xFFu) == 0);
	}
	CHECK(ae_cache_rejection_word(AE_CACHE_JOIN_FORMAT) == 0x0245 && ae_cache_rejection_word(AE_CACHE_JOIN_DIFFERENT_BUILD) == 0x0545);
	CHECK(ae_cache_rejection_case(0x10069) == -1 && ae_cache_rejection_case(64) == -1);
}

/* the scopes nest, overflow and come back; the table keeps a tag for its handle only (a slot reused by another
handle is not tagged), and a datum made outside any scope clears its slot */
static void test_scopes_and_table(void)
{
	struct ae_silent_scopes scopes;
	struct ae_silent_tag table[16];
	long player;
	int index;

	memset(&scopes, 0, sizeof(scopes));
	memset(table, 0, sizeof(table));
	CHECK(ae_silent_scope_role(&scopes, &player) == AE_SILENT_ROLE_NONE && player == -1);
	ae_silent_scope_push(&scopes, AE_SILENT_ROLE_MOVEMENT, ME);
	ae_silent_scope_push(&scopes, AE_SILENT_ROLE_WEAPON, OTHER);
	CHECK(ae_silent_scope_role(&scopes, &player) == AE_SILENT_ROLE_WEAPON && player == OTHER);
	ae_silent_scope_push(&scopes, AE_SILENT_ROLE_NONE, OTHER);
	CHECK(ae_silent_scope_role(&scopes, &player) == AE_SILENT_ROLE_NONE);
	ae_silent_scope_pop(&scopes);
	ae_silent_scope_pop(&scopes);
	CHECK(ae_silent_scope_role(&scopes, &player) == AE_SILENT_ROLE_MOVEMENT && player == ME);
	ae_silent_scope_pop(&scopes);
	ae_silent_scope_pop(&scopes);
	CHECK(scopes.depth == 0);
	for (index = 0; index < AE_SILENT_SCOPE_DEPTH + 3; index++)
		ae_silent_scope_push(&scopes, AE_SILENT_ROLE_WEAPON, index);
	CHECK(scopes.depth == AE_SILENT_SCOPE_DEPTH && scopes.overflow == 3);
	for (index = 0; index < AE_SILENT_SCOPE_DEPTH + 3; index++)
		ae_silent_scope_pop(&scopes);
	CHECK(scopes.depth == 0 && scopes.overflow == 0);

	CHECK(ae_silent_table_get(table, 16, 0, &player) == AE_SILENT_ROLE_NONE);
	ae_silent_table_set(table, 16, 0x12340005L, AE_SILENT_ROLE_MOVEMENT, ME);
	CHECK(ae_silent_table_get(table, 16, 0x12340005L, &player) == AE_SILENT_ROLE_MOVEMENT && player == ME);
	CHECK(ae_silent_table_get(table, 16, 0x12350005L, &player) == AE_SILENT_ROLE_NONE && player == -1);
	ae_silent_table_set(table, 16, 0x12350005L, AE_SILENT_ROLE_NONE, ME);
	CHECK(ae_silent_table_get(table, 16, 0x12340005L, &player) == AE_SILENT_ROLE_NONE);
	CHECK(ae_silent_table_get(table, 16, 0x12350005L, &player) == AE_SILENT_ROLE_NONE);
	/* (past the table, and none: nothing) */
	ae_silent_table_set(table, 16, 0x00010020L, AE_SILENT_ROLE_WEAPON, ME);
	CHECK(ae_silent_table_get(table, 16, 0x00010020L, &player) == AE_SILENT_ROLE_NONE);
	ae_silent_table_set(table, 16, -1, AE_SILENT_ROLE_WEAPON, ME);
	CHECK(ae_silent_table_get(table, 16, -1, &player) == AE_SILENT_ROLE_NONE);
}

/* 0.7.7: the MCC mark, beside the cache block: it reads back, the older readers read the same, a name of 24 characters is cut to 23 */
static void test_mcc_mark(void)
{
	unsigned short name[64], before[64];
	unsigned char digest[4] = { 1, 2, 3, 4 }, got[4];
	unsigned int format, generation;
	int length, has, index;

	for (length = 16; length <= 48; length += 16)
	{
		for (has = 0; has <= 1; has++)
		{
			memset(name, 0, sizeof(name));
			for (index = 0; index < length - 9; index++)
				name[index] = (unsigned short)('a' + index % 20);	/* (a name as long as can be) */
			ae_silent_join_mark(name, length);
			CHECK(ae_silent_join_mcc_told(name, length) == -1);	/* (nothing told yet) */
			ae_silent_join_cache(name, length, 1, 7, digest);
			memcpy(before, name, sizeof(name));
			ae_silent_join_mcc(name, length, has);
			CHECK(ae_silent_join_mcc_told(name, length) == has);
			/* (the older readers read the same: the capability, the cache block, the feature place) */
			CHECK(ae_silent_join_capability(name, length) == ae_silent_join_capability(before, length));
			CHECK(ae_silent_join_cache_told(name, length, &format, &generation, got));
			CHECK(format == 1 && generation == 7 && !memcmp(got, digest, 4));
			CHECK(memcmp(name + length - 7, before + length - 7, 7 * sizeof(unsigned short)) == 0);
			/* (a reader that stops at the name's end sees the name 23 characters long at most) */
			for (index = 0; index < length && name[index]; index++)
				;
			CHECK(index <= length - 9);
		}
	}
	/* (no AE mark, or too short a name: no mark, and nothing written) */
	memset(name, 0, sizeof(name));
	name[0] = 'x';
	ae_silent_join_mcc(name, 32, 1);
	CHECK(ae_silent_join_mcc_told(name, 32) == -1 && name[24] == 0);
	ae_silent_join_mark(name, 8);
	ae_silent_join_mcc(name, 8, 1);
	CHECK(ae_silent_join_mcc_told(name, 8) == -1);
	/* (a mark whose name runs into it is no mark) */
	memset(name, 'z', sizeof(name));
	name[31] = AE_SILENT_JOIN_MARK | 1;
	name[24] = AE_SILENT_JOIN_MCC_MARK | 1;
	CHECK(ae_silent_join_mcc_told(name, 32) == -1);
}

int main(void)
{
	test_mute_table();
	test_byte();
	test_roles();
	test_join();
	test_cache_block();
	test_mcc_mark();
	test_cache_code_width();
	test_scopes_and_table();
	printf("ae_silent_rules: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
