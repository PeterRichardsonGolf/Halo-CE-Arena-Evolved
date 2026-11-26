/* ae_steam_friends_rules.c: STEAM FRIENDS' hashes, the query chunks and the rows */
#include <stdio.h>
#include <string.h>

#include "ae_steam_friends_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char hash[AE_STEAM_HASH], csv[600];
	struct ae_steam_friend friends[60];
	struct ae_pc_steam found[3];
	struct ae_steam_row rows[8];
	int index, others = -1, made;

	/* the hash is the service's (made with another SHA-256) */
	ae_steam_hash(76561190000000001ull, hash);
	CHECK(!strcmp(hash, "96d3391cf79f"));
	ae_steam_hash(76561190000000002ull, hash);
	CHECK(!strcmp(hash, "e9b597766e27"));

	/* chunks of 40 hashes, never past the buffer */
	for (index = 0; index < 60; index++)
	{
		friends[index].id = 76561190000000001ull + (unsigned long long)index;
		snprintf(friends[index].name, sizeof(friends[index].name), "F%d", index);
	}
	CHECK(ae_steam_hashes_csv(friends, 60, 0, csv, sizeof(csv)) == 40 && strlen(csv) == 40 * 12 + 39);
	CHECK(!strncmp(csv, "96d3391cf79f,e9b597766e27,", 26));
	CHECK(ae_steam_hashes_csv(friends, 60, 40, csv, sizeof(csv)) == 20 && strlen(csv) == 20 * 12 + 19);
	CHECK(ae_steam_hashes_csv(friends, 60, 60, csv, sizeof(csv)) == 0 && !csv[0]);
	{
		char small[30];

		CHECK(ae_steam_hashes_csv(friends, 60, 0, small, sizeof(small)) == 2 && strlen(small) == 25);
	}

	/* rows: only friends the service found, in the Steam list's order; the rest counted */
	memset(found, 0, sizeof(found));
	ae_steam_hash(friends[5].id, found[0].hash);
	snprintf(found[0].code, sizeof(found[0].code), "AAAA-BBBB-CCCC");
	snprintf(found[0].name, sizeof(found[0].name), "Rook");
	snprintf(found[0].state, sizeof(found[0].state), "menus");
	ae_steam_hash(friends[2].id, found[1].hash);
	snprintf(found[1].code, sizeof(found[1].code), "DDDD-EEEE-FFFF");
	snprintf(found[1].name, sizeof(found[1].name), "Pickle");
	snprintf(found[1].state, sizeof(found[1].state), "searching");
	snprintf(found[2].hash, sizeof(found[2].hash), "000000000000");           /* not on this PC's list: ignored */
	made = ae_steam_rows(friends, 60, found, 3, rows, 8, &others);
	CHECK(made == 2 && others == 58);
	CHECK(!strcmp(rows[0].steam_name, "F2") && !strcmp(rows[0].ae_name, "Pickle") && !strcmp(rows[0].code, "DDDD-EEEE-FFFF"));
	CHECK(!strcmp(rows[1].steam_name, "F5") && !strcmp(rows[1].ae_name, "Rook"));
	made = ae_steam_rows(friends, 60, found, 3, rows, 1, &others);
	CHECK(made == 1 && others == 59);
	CHECK(ae_steam_rows(friends, 0, found, 3, rows, 8, &others) == 0 && others == 0);
	CHECK(ae_steam_rows(friends, 60, NULL, 0, rows, 8, NULL) == 0);

	/* the state's words */
	CHECK(!strcmp(ae_steam_state_word("menus", 0), "IN THE MENUS") && !strcmp(ae_steam_state_word("searching", 0), "IN MATCHMAKING"));
	CHECK(!strcmp(ae_steam_state_word("party", 0), "IN A PARTY") && !strcmp(ae_steam_state_word("menus", 1), "IN YOUR PARTY"));

	printf(failures ? "FAILED\n" : "ok\n");
	return failures != 0;
}
