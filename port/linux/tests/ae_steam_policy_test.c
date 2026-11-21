/* ae_steam_policy.h: the rules a Workshop item fetched for a game server is checked against */
#include <stdio.h>
#include <string.h>

#include "ae_steam_policy.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	CHECK(steam_item_tags_multiplayer("Halo1,Multiplayer"));
	CHECK(steam_item_tags_multiplayer("multiplayer, HALO1"));
	CHECK(steam_item_tags_multiplayer("Map\t,Halo1\t,\tMultiplayer ,Pro"));
	CHECK(!steam_item_tags_multiplayer("Halo1,Campaign"));
	CHECK(!steam_item_tags_multiplayer("Halo2,Multiplayer"));
	CHECK(!steam_item_tags_multiplayer("Halo1Multiplayer"));
	CHECK(!steam_item_tags_multiplayer("Halo11,Multiplayers"));
	CHECK(!steam_item_tags_multiplayer(""));
	CHECK(steam_fetch_size_check(0, 100, 100) == STEAM_FETCH_OK);
	CHECK(steam_fetch_size_check(100, 100, 100) == STEAM_FETCH_OK);
	CHECK(steam_fetch_size_check(101, 100, 1000) == STEAM_FETCH_TOO_LARGE);
	CHECK(steam_fetch_size_check(100, 200, 99) == STEAM_FETCH_OVER_BUDGET);
	CHECK(steam_fetch_size_check(1, 200, 0) == STEAM_FETCH_OVER_BUDGET);
	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
