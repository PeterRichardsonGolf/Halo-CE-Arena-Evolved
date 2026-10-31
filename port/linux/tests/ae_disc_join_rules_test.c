#include <stdio.h>
#include <string.h>
#include "ae_disc_join_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* network_game_generate_join_game_token's token in the port's builds (DEBUG): "message in a bottle", 16 bytes */
static void stock_token(unsigned char token[16])
{
	memcpy(token, "message in a bottle", 16);
}

int main(void)
{
	unsigned char stock[16], token[16], other[16];

	/* no disc: no identity, and the token is the stock one (stock OpenCE and Chupathingy machines match it) */
	CHECK(ae_disc_join_identity(NULL) == 0);
	CHECK(ae_disc_join_identity("") == 0);
	stock_token(stock);
	stock_token(token);
	ae_disc_join_fold(token, 16, 0);
	CHECK(!memcmp(token, stock, 16));

	/* a disc: an identity; the same disc, letters in any case, the same one; another disc another */
	CHECK(ae_disc_join_identity("NHE") != 0);
	CHECK(ae_disc_join_identity("NHE") == ae_disc_join_identity("nhe"));
	CHECK(ae_disc_join_identity("NHE") != ae_disc_join_identity("CE+ X"));
	CHECK(ae_disc_join_identity("NHE") != ae_disc_join_identity("NHE2"));

	/* its token differs from the stock one in every byte's place it can, and from another disc's */
	stock_token(token);
	ae_disc_join_fold(token, 16, ae_disc_join_identity("NHE"));
	CHECK(memcmp(token, stock, 16));
	stock_token(other);
	ae_disc_join_fold(other, 16, ae_disc_join_identity("CE+ X"));
	CHECK(memcmp(token, other, 16));
	/* the host and a joining machine on the same disc work out the same token */
	stock_token(other);
	ae_disc_join_fold(other, 16, ae_disc_join_identity("nhe"));
	CHECK(!memcmp(token, other, 16));
	/* folded twice it is the stock token again (a fold, not a hash of the token) */
	ae_disc_join_fold(token, 16, ae_disc_join_identity("NHE"));
	CHECK(!memcmp(token, stock, 16));
	/* no token, or no bytes: nothing written */
	ae_disc_join_fold(NULL, 16, ae_disc_join_identity("NHE"));
	stock_token(token);
	ae_disc_join_fold(token, 0, ae_disc_join_identity("NHE"));
	CHECK(!memcmp(token, stock, 16));

	/* a disc's maps that change a game: stock multiplayer and campaign maps' replacements, any case */
	CHECK(ae_disc_join_replaces_stock_map("bloodgulch.map"));
	CHECK(ae_disc_join_replaces_stock_map("BloodGulch.MAP"));
	CHECK(ae_disc_join_replaces_stock_map("beavercreek.map"));
	CHECK(ae_disc_join_replaces_stock_map("a10.map"));
	/* not: the menus' ui.map, Custom Edition's resource maps, a disc's own maps (map sharing sends them), other
	files and names */
	CHECK(!ae_disc_join_replaces_stock_map("ui.map"));
	CHECK(!ae_disc_join_replaces_stock_map("UI.map"));
	CHECK(!ae_disc_join_replaces_stock_map("bitmaps.map"));
	CHECK(!ae_disc_join_replaces_stock_map("sounds.map"));
	CHECK(!ae_disc_join_replaces_stock_map("loc.map"));
	CHECK(!ae_disc_join_replaces_stock_map("badcreek.map"));
	CHECK(!ae_disc_join_replaces_stock_map("modcreek.map"));
	CHECK(!ae_disc_join_replaces_stock_map("bloodgulch.txt"));
	CHECK(!ae_disc_join_replaces_stock_map("bloodgulch"));
	CHECK(!ae_disc_join_replaces_stock_map(".map"));
	CHECK(!ae_disc_join_replaces_stock_map(""));
	CHECK(!ae_disc_join_replaces_stock_map(NULL));
	CHECK(!ae_disc_join_replaces_stock_map("bloodgulch.map.bak"));
	{
		char long_name[200];

		memset(long_name, 'a', sizeof(long_name));
		memcpy(long_name + sizeof(long_name) - 5, ".map", 5);
		CHECK(!ae_disc_join_replaces_stock_map(long_name));
	}

	if (failures)
		return 1;
	printf("ae_disc_join_rules: all passed\n");
	return 0;
}
