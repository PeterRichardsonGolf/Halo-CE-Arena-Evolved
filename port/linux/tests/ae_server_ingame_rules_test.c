/* ae_server_ingame_rules_test.c: running a dedicated server from inside the game (ae_server_ingame_rules.h) */
#include <stdio.h>
#include <string.h>

#include "ae_server_ingame_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* the stock command table's roles: say, kick, ban, mute and sv_end_game a moderator's; sv_rotation_begin a moderator's too (0.7.7);
sv_restart the owner's; sv_team (AE's) a moderator's */
static int stock_role(void *context, const char *command)
{
	(void)context;
	if (!strcmp(command, "sv_restart"))
		return 3;
	return 1;
}

/* a command_access.txt that gives sv_restart to admins, and has no sv_team */
static int access_role(void *context, const char *command)
{
	(void)context;
	if (!strcmp(command, "sv_restart"))
		return 2;
	if (!strcmp(command, "sv_team"))
		return -1;
	return 1;
}

static void test_roles(void)
{
	unsigned int moderator = ae_ingame_allowed(AE_INGAME_ROLE_MODERATOR, stock_role, NULL);
	unsigned int admin = ae_ingame_allowed(AE_INGAME_ROLE_ADMIN, stock_role, NULL);
	unsigned int owner = ae_ingame_allowed(AE_INGAME_ROLE_OWNER, stock_role, NULL);

	/* (moderators move, mute and kick; bans are an admin's; the restart the owner's here) */
	CHECK(ae_ingame_allows(moderator, AE_INGAME_KICK));
	CHECK(ae_ingame_allows(moderator, AE_INGAME_MOVE));
	CHECK(ae_ingame_allows(moderator, AE_INGAME_MUTE));
	CHECK(!ae_ingame_allows(moderator, AE_INGAME_BAN));
	CHECK(ae_ingame_allows(moderator, AE_INGAME_NEXT) && ae_ingame_allows(admin, AE_INGAME_NEXT));
	CHECK(ae_ingame_allows(admin, AE_INGAME_BAN));
	CHECK(!ae_ingame_allows(admin, AE_INGAME_RESTART));
	CHECK(owner == (1u << AE_INGAME_ACTIONS) - 1);
	CHECK(!ae_ingame_allowed(AE_INGAME_ROLE_NONE, stock_role, NULL));
	CHECK(!ae_ingame_allowed(AE_INGAME_ROLE_ADMIN, NULL, NULL));
	/* (the server's own access: a command it lacks is no action) */
	admin = ae_ingame_allowed(AE_INGAME_ROLE_ADMIN, access_role, NULL);
	CHECK(ae_ingame_allows(admin, AE_INGAME_RESTART));
	CHECK(!ae_ingame_allows(admin, AE_INGAME_MOVE));
	CHECK(!ae_ingame_allows(admin, -1) && !ae_ingame_allows(admin, AE_INGAME_ACTIONS));
	CHECK(!strcmp(ae_ingame_command(AE_INGAME_NEXT), "sv_rotation_begin"));
	CHECK(!strcmp(ae_ingame_command(99), ""));
	CHECK(!strcmp(ae_ingame_role_word(AE_INGAME_ROLE_ADMIN), "ADMIN"));
	CHECK(!strcmp(ae_ingame_role_word(0), ""));
}

static void test_ban_lengths(void)
{
	char text[64];

	CHECK(ae_ingame_ban_minutes(0) == 60 && ae_ingame_ban_minutes(1) == 1440 && ae_ingame_ban_minutes(2) == 10080);
	CHECK(ae_ingame_ban_minutes(3) == 0);
	CHECK(ae_ingame_ban_minutes(9) == 1440);
	CHECK(!strcmp(ae_ingame_ban_word(AE_INGAME_BAN_FIRST), "1 DAY"));
	ae_ingame_ban_yes("Goat", 1, text, sizeof(text));
	CHECK(!strcmp(text, "BAN GOAT FOR 1 DAY"));
	ae_ingame_ban_yes("Goat", 3, text, sizeof(text));
	CHECK(!strcmp(text, "BAN GOAT FOREVER"));
	ae_ingame_ban_command_length(1440, text, sizeof(text));
	CHECK(!strcmp(text, "1440m"));
	ae_ingame_ban_command_length(0, text, sizeof(text));
	CHECK(!strcmp(text, "forever"));
}

static void test_card_words(void)
{
	char text[160];

	CHECK(!strcmp(ae_ingame_move_label(0), "MOVE TO BLUE TEAM"));
	CHECK(!strcmp(ae_ingame_move_label(1), "MOVE TO RED TEAM"));
	ae_ingame_card_role_line(AE_INGAME_ROLE_ADMIN, text, sizeof(text));
	CHECK(!strncmp(text, "You are an ADMIN on this server.", 32));
	ae_ingame_card_role_line(AE_INGAME_ROLE_MODERATOR, text, sizeof(text));
	CHECK(!strncmp(text, "You are a MODERATOR", 19));
	ae_ingame_card_role_line(0, text, sizeof(text));
	CHECK(!text[0]);
}

static void test_key_lines(void)
{
	char kept[80];
	unsigned char key[32];
	char back[80];
	const char *hex = "00112233445566778899AABBCCDDEEFF00112233445566778899aabbccddeeff";

	CHECK(ae_ingame_key_hex(hex, kept, sizeof(kept)));
	CHECK(!strcmp(kept, "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff"));
	CHECK(!ae_ingame_key_hex("0011", kept, sizeof(kept)));
	CHECK(!ae_ingame_key_hex("zz112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", kept, sizeof(kept)));
	CHECK(!ae_ingame_key_hex(hex, kept, 64));
	CHECK(!ae_ingame_key_hex(NULL, kept, sizeof(kept)));
	CHECK(ae_ingame_key_bytes(hex, key) && key[0] == 0 && key[10] == 0xAA && key[31] == 0xFF);
	ae_ingame_key_text(key, back);
	CHECK(!strcmp(back, kept));
}

static void test_clean_text(void)
{
	char kept[16];

	CHECK(ae_ingame_clean_text("  hi \"there\"\\ ", kept, sizeof(kept)) && !strcmp(kept, "hi there"));
	CHECK(!ae_ingame_clean_text("   ", kept, sizeof(kept)) && !kept[0]);
	CHECK(ae_ingame_clean_text("a\nb\x7f" "c", kept, sizeof(kept)) && !strcmp(kept, "abc"));
	CHECK(ae_ingame_clean_text("0123456789abcdefghij", kept, sizeof(kept)) && strlen(kept) == 15);
	CHECK(!ae_ingame_clean_text(NULL, kept, sizeof(kept)));
}

static void sample(struct ae_ingame_wire *state)
{
	memset(state, 0, sizeof(*state));
	state->role = AE_INGAME_ROLE_ADMIN;
	state->allowed = (unsigned short)ae_ingame_allowed(AE_INGAME_ROLE_ADMIN, stock_role, NULL);
	state->players = 5;
	state->bots = 2;
	state->max_players = 16;
	state->total = 6;
	state->count = 4;
	state->now = 1;
	state->next = 2;
	state->up_minutes = 134;
	snprintf(state->name, sizeof(state->name), "Friday Night 4v4");
	snprintf(state->entries[0].map, AE_INGAME_WIRE_MAP, "h2d_sanctuary");
	snprintf(state->entries[0].gametype, AE_INGAME_WIRE_GAMETYPE, "ae_king");
	state->entries[0].index = 2;
	snprintf(state->entries[1].map, AE_INGAME_WIRE_MAP, "missing");
	snprintf(state->entries[1].gametype, AE_INGAME_WIRE_GAMETYPE, "slayer");
	state->entries[1].index = 3;
	state->entries[1].flags = AE_INGAME_ENTRY_REJECTED;
	snprintf(state->entries[2].map, AE_INGAME_WIRE_MAP, "beavercreek@pro");
	snprintf(state->entries[2].gametype, AE_INGAME_WIRE_GAMETYPE, "ae_team_sly");
	state->entries[2].index = 4;
	snprintf(state->entries[3].map, AE_INGAME_WIRE_MAP, "h2d_salicreek");
	snprintf(state->entries[3].gametype, AE_INGAME_WIRE_GAMETYPE, "ae_team_sly");
	state->entries[3].index = 1;
}

static void test_wire(void)
{
	struct ae_ingame_wire state, read;
	unsigned char bytes[AE_INGAME_WIRE_MAXIMUM];
	int size, index;

	sample(&state);
	size = ae_ingame_wire_encode(&state, bytes, sizeof(bytes));
	CHECK(size == AE_INGAME_WIRE_HEAD + 4 * AE_INGAME_WIRE_ENTRY);
	CHECK(ae_ingame_wire_decode(bytes, size, &read));
	CHECK(read.role == 2 && read.allowed == state.allowed && read.players == 5 && read.bots == 2 && read.max_players == 16);
	CHECK(read.total == 6 && read.count == 4 && read.now == 1 && read.next == 2 && read.up_minutes == 134);
	CHECK(!strcmp(read.name, "Friday Night 4v4"));
	CHECK(!strcmp(read.entries[2].map, "beavercreek@pro") && read.entries[2].index == 4);
	CHECK(read.entries[1].flags == AE_INGAME_ENTRY_REJECTED);
	CHECK(AE_INGAME_WIRE_MAXIMUM <= 560);
	/* (every truncation refused) */
	for (index = 0; index < size; index++)
		CHECK(!ae_ingame_wire_decode(bytes, index, &read));
	/* (an unended text, a bad role, a bad flag, an entry past the total, a version not known) */
	sample(&state);
	size = ae_ingame_wire_encode(&state, bytes, sizeof(bytes));
	bytes[16 + AE_INGAME_WIRE_NAME - 1] = 'x';
	CHECK(!ae_ingame_wire_decode(bytes, size, &read));
	size = ae_ingame_wire_encode(&state, bytes, sizeof(bytes));
	bytes[16] = 7;
	CHECK(!ae_ingame_wire_decode(bytes, size, &read));
	size = ae_ingame_wire_encode(&state, bytes, sizeof(bytes));
	bytes[1] = 0;
	CHECK(!ae_ingame_wire_decode(bytes, size, &read));
	size = ae_ingame_wire_encode(&state, bytes, sizeof(bytes));
	bytes[2] = 0x80;
	CHECK(!ae_ingame_wire_decode(bytes, size, &read));
	size = ae_ingame_wire_encode(&state, bytes, sizeof(bytes));
	bytes[AE_INGAME_WIRE_HEAD + AE_INGAME_WIRE_ENTRY - 1] = 6;
	CHECK(!ae_ingame_wire_decode(bytes, size, &read));
	size = ae_ingame_wire_encode(&state, bytes, sizeof(bytes));
	bytes[0] = 2;
	CHECK(!ae_ingame_wire_decode(bytes, size, &read));
	/* (states not to send) */
	sample(&state);
	state.role = 0;
	CHECK(!ae_ingame_wire_encode(&state, bytes, sizeof(bytes)));
	sample(&state);
	state.count = AE_INGAME_WIRE_ENTRIES + 1;
	CHECK(!ae_ingame_wire_encode(&state, bytes, sizeof(bytes)));
	sample(&state);
	CHECK(!ae_ingame_wire_encode(&state, bytes, AE_INGAME_WIRE_HEAD));
	/* (garbage of every length never reads past its end: the sanitizers watch) */
	for (index = 0; index < (int)sizeof(bytes); index++)
		bytes[index] = (unsigned char)(index * 37 + 11);
	for (index = 0; index <= (int)sizeof(bytes); index++)
		ae_ingame_wire_decode(bytes, index, &read);
}

static void test_next_step(void)
{
	struct ae_ingame_wire state;

	sample(&state);
	/* (from the next, 0: right skips the rejected one, 1; left goes round to 3) */
	CHECK(ae_ingame_next_step(&state, 0, 1) == 2);
	CHECK(ae_ingame_next_step(&state, 2, 1) == 0);
	/* (3 is the game played now: skipped) */
	CHECK(ae_ingame_next_step(&state, 0, -1) == 2);
	/* (only the game played now: it is offered) */
	state.count = 1;
	state.entries[0].index = 1;
	CHECK(ae_ingame_next_step(&state, 0, 1) == 0);
	state.count = 0;
	CHECK(ae_ingame_next_step(&state, 0, 1) == 0);
}

static void test_card_pick(void)
{
	char const *mates[2] = { "Ann", "Bob" };

	CHECK(ae_ingame_card_pick(2, mates, "Bob") == 1 && ae_ingame_card_pick(2, mates, "Ann") == 0);
	/* (the named player left: nobody, not the machine's first player; no name: the first) */
	CHECK(ae_ingame_card_pick(2, mates, "Cy") == -1 && ae_ingame_card_pick(1, mates, "Bob") == -1);
	CHECK(ae_ingame_card_pick(2, mates, "") == 0 && ae_ingame_card_pick(2, mates, NULL) == 0);
	CHECK(ae_ingame_card_pick(0, mates, "Ann") == -1 && ae_ingame_card_pick(0, NULL, NULL) == -1);
}

static void test_words(void)
{
	char text[40];

	ae_ingame_map_word("h2d_sanctuary", text, sizeof(text));
	CHECK(!strcmp(text, "H2D SANCTUARY"));
	ae_ingame_map_word("pat_beavercreek@ws2899342285", text, sizeof(text));
	CHECK(!strcmp(text, "PAT BEAVERCREEK"));
	ae_ingame_map_word("custom_maps\\hugeass", text, sizeof(text));
	CHECK(!strcmp(text, "HUGEASS"));
	ae_ingame_gametype_word("ae_team_sly", text, sizeof(text));
	CHECK(!strcmp(text, "AE TEAM SLY"));
	ae_ingame_up_text(134, text, sizeof(text));
	CHECK(!strcmp(text, "2 h 14 min"));
	ae_ingame_up_text(14, text, sizeof(text));
	CHECK(!strcmp(text, "14 min"));
	ae_ingame_up_text(120, text, sizeof(text));
	CHECK(!strcmp(text, "2 h"));
	ae_ingame_up_text(0, text, sizeof(text));
	CHECK(!strcmp(text, "under a minute"));
}

static void test_offer(void)
{
	struct ae_ingame_offer offer, back;
	unsigned char mine[32], other[32], bytes[AE_INGAME_OFFER_MAXIMUM + 8];
	int index, size;

	for (index = 0; index < 32; index++)
	{
		mine[index] = (unsigned char)(index * 7 + 1);
		other[index] = (unsigned char)(index * 5 + 3);
	}
	memset(&offer, 0, sizeof(offer));
	for (index = 0; index < AE_INGAME_OFFER_SALT; index++)
		offer.salt[index] = (unsigned char)(0xA0 + index);
	/* a list without this key: no sign-in; the key not sent */
	CHECK(ae_ingame_offer_add(&offer, other));
	CHECK(!ae_ingame_offer_matches(&offer, mine));
	CHECK(!ae_ingame_offer_matches(&offer, NULL));
	/* with it: sign in, after a trip over the wire */
	CHECK(ae_ingame_offer_add(&offer, mine));
	size = ae_ingame_offer_encode(&offer, bytes, (int)sizeof(bytes));
	CHECK(size == AE_INGAME_OFFER_HEAD + 2 * AE_INGAME_OFFER_HASH && size % 2 == 0);
	CHECK(ae_ingame_offer_decode(bytes, size, &back));
	CHECK(ae_ingame_offer_matches(&back, mine));
	CHECK(!ae_ingame_offer_matches(&back, NULL));
	/* another server's salt gives other hashes: a list is no use to a different salt */
	back.salt[0] ^= 1;
	CHECK(!ae_ingame_offer_matches(&back, mine));
	/* an address row: signs in without a key row */
	memset(&offer, 0, sizeof(offer));
	offer.flags = AE_INGAME_OFFER_BY_ADDRESS;
	CHECK(ae_ingame_offer_matches(&offer, mine));
	/* the list is full at 32; bad bodies refused */
	memset(&offer, 0, sizeof(offer));
	for (index = 0; index < AE_INGAME_OFFER_HASHES; index++)
	{
		other[0] = (unsigned char)index;
		CHECK(ae_ingame_offer_add(&offer, other));
	}
	CHECK(!ae_ingame_offer_add(&offer, mine));
	size = ae_ingame_offer_encode(&offer, bytes, (int)sizeof(bytes));
	CHECK(size == AE_INGAME_OFFER_MAXIMUM);
	CHECK(ae_ingame_offer_decode(bytes, size, &back));
	CHECK(!ae_ingame_offer_decode(bytes, size - 1, &back));
	bytes[0] = 2;
	CHECK(!ae_ingame_offer_decode(bytes, size, &back));
	bytes[0] = 1;
	bytes[2] = 33;
	CHECK(!ae_ingame_offer_decode(bytes, size, &back));
	bytes[2] = 32;
	bytes[1] = 2;
	CHECK(!ae_ingame_offer_decode(bytes, size, &back));
	CHECK(!ae_ingame_offer_decode(bytes, 3, &back));
}

int main(void)
{
	test_offer();
	test_roles();
	test_ban_lengths();
	test_card_words();
	test_key_lines();
	test_clean_text();
	test_wire();
	test_next_step();
	test_card_pick();
	test_words();
	if (failures)
		return 1;
	printf("ae_server_ingame_rules: ok\n");
	return 0;
}
