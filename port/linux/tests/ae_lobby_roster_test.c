#include <stdio.h>
#include <string.h>
#include "ae_lobby_roster.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_lobby_roster roster;

static void add(const char *name, short machine, short controller, int host, int local)
{
	struct ae_lobby_player *player = &roster.players[roster.count++];

	memset(player, 0, sizeof(*player));
	ae_lobby_copy_utf8(player->name, sizeof(player->name), name);
	player->machine = machine;
	player->controller = controller;
	player->host = host;
	player->local = local;
}

int main(void)
{
	char text[64];
	unsigned short wide[16];
	int index;

	/* six, the host (machine 0) listed fourth: the host first, then by machine, equal machines in the game's order;
	slots 1-6 */
	memset(&roster, 0, sizeof(roster));
	add("Bot A", 2, 0, 0, 0);
	add("Bot B", 1, 0, 0, 0);
	add("Bot C", 3, 0, 0, 0);
	add("Player 1", 0, 0, 1, 1);
	add("Guest", 0, 1, 1, 1);
	add("Bot D", 1, 0, 0, 0);
	ae_lobby_roster_order(&roster);
	CHECK(roster.count == 6);
	CHECK(!strcmp(roster.players[0].name, "Player 1") && !strcmp(roster.players[1].name, "Guest"));
	CHECK(!strcmp(roster.players[2].name, "Bot B") && !strcmp(roster.players[3].name, "Bot D"));
	CHECK(!strcmp(roster.players[4].name, "Bot A") && !strcmp(roster.players[5].name, "Bot C"));
	for (index = 0; index < 6; index++)
		CHECK(roster.players[index].slot == index + 1);
	/* shrinking from 6 to 2: slots 1-2, contiguous */
	memset(&roster, 0, sizeof(roster));
	add("Bot B", 1, 0, 0, 0);
	add("Player 1", 0, 0, 1, 1);
	ae_lobby_roster_order(&roster);
	CHECK(roster.count == 2 && roster.players[0].slot == 1 && roster.players[1].slot == 2 &&
		!strcmp(roster.players[0].name, "Player 1"));
	/* (an empty roster, a count out of range) */
	memset(&roster, 0, sizeof(roster));
	ae_lobby_roster_order(&roster);
	CHECK(roster.count == 0);
	roster.count = 500;
	ae_lobby_roster_order(&roster);
	CHECK(roster.count == AE_LOBBY_MAXIMUM_PLAYERS);
	ae_lobby_roster_order(NULL);
	/* names: 47 bytes kept whole; 60 cut on a UTF-8 boundary (an "é" across the 47th byte is not split) */
	memset(text, 'a', 47);
	text[47] = 0;
	CHECK(ae_lobby_copy_utf8(roster.players[0].name, AE_LOBBY_NAME_SIZE, text) == 47 &&
		!strcmp(roster.players[0].name, text));
	memset(text, 'a', 46);
	strcpy(text + 46, "\xC3\xA9" "bbbbbbbbbbbb");
	CHECK(strlen(text) == 60);
	CHECK(ae_lobby_copy_utf8(roster.players[0].name, AE_LOBBY_NAME_SIZE, text) == 46 &&
		strlen(roster.players[0].name) == 46);
	/* (a sequence cut short by the text's end is dropped) */
	CHECK(ae_lobby_copy_utf8(text, sizeof(text), "ab\xE2\x82") == 2 && !strcmp(text, "ab"));
	/* UTF-16 names: ASCII, a 2-byte and a 3-byte character, a surrogate pair, a lone surrogate */
	wide[0] = 'P'; wide[1] = 0xE9; wide[2] = 0x20AC; wide[3] = 0xD83D; wide[4] = 0xDE00; wide[5] = 0xD800; wide[6] = 'x';
	wide[7] = 0;
	CHECK(ae_lobby_utf16_to_utf8(text, sizeof(text), wide, 12) == 1 + 2 + 3 + 4 + 3 + 1);
	CHECK(!strcmp(text, "P\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\xEF\xBF\xBDx"));
	/* (cut on a boundary: room for "P" and the 2-byte one, not the 3-byte one) */
	CHECK(ae_lobby_utf16_to_utf8(text, 5, wide, 12) == 3 && !strcmp(text, "P\xC3\xA9"));
	/* (stops at count, without a 0) */
	CHECK(ae_lobby_utf16_to_utf8(text, sizeof(text), wide, 1) == 1 && !strcmp(text, "P"));
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
