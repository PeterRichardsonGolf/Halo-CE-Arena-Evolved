#include <stdio.h>

#include "ae_dev_ammo_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	/* a player's weapon, with DEV MODE and the setting on */
	CHECK(ae_dev_ammo_applies(1, 1, 1, 0));
	/* DEV MODE off (or a network game's client): the setting is not read */
	CHECK(!ae_dev_ammo_applies(0, 1, 1, 0));
	/* the setting off */
	CHECK(!ae_dev_ammo_applies(1, 0, 1, 0));
	/* a weapon on the ground, or held by no player (an AI's) */
	CHECK(!ae_dev_ammo_applies(1, 1, 0, 0));
	/* a bot's weapon (and a dummy's): as always */
	CHECK(!ae_dev_ammo_applies(1, 1, 1, 1));
	if (failures)
		return 1;
	printf("ae_dev_ammo_rules: all passed\n");
	return 0;
}
