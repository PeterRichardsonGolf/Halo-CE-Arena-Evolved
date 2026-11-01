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

int main(void)
{
	test_mute_table();
	test_byte();
	test_roles();
	test_join();
	test_scopes_and_table();
	printf("ae_silent_rules: %d checks, %d failed\n", checks, failures);
	return failures ? 1 : 0;
}
