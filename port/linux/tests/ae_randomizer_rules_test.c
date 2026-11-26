/* ae_randomizer_rules.c: the randomizer plays no multiplayer game */
#include <stdio.h>

#include "ae_randomizer_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	int network;
	int solo;

	/* a campaign level played alone or in split screen on one machine */
	CHECK(ae_randomizer_rules_plays(0, 0, 0, 1) == 1);
	/* a multiplayer map load, alone or hosted or joined, and the menus' own level: nothing is read, copied or written */
	for (solo = 0; solo < 2; solo++)
		for (network = 0; network < 2; network++)
			if (network || !solo)
				CHECK(ae_randomizer_rules_plays(0, 0, network, solo) == 0);
	/* co-op: a campaign level with other machines is not randomized */
	CHECK(ae_randomizer_rules_plays(0, 0, 1, 1) == 0);
	/* the game about to be played decides Firefight: the last game's variant is not an input. Firefight before, Slayer next
	-> not Firefight; a Firefight variant next, or a Firefight network game, is */
	CHECK(ae_randomizer_rules_firefight_game(0, 0) == 0);
	CHECK(ae_randomizer_rules_plays(ae_randomizer_rules_firefight_game(0, 0), 0, 0, 0) == 0);
	CHECK(ae_randomizer_rules_plays(ae_randomizer_rules_firefight_game(0, 0), 0, 1, 0) == 0);
	CHECK(ae_randomizer_rules_firefight_game(1, 0) == 1);
	CHECK(ae_randomizer_rules_firefight_game(0, 1) == 1);
	CHECK(ae_randomizer_rules_plays(1, 0, 0, 0) == 1);
	CHECK(ae_randomizer_rules_plays(1, 0, 1, 0) == 1);
	/* the range's level plays alone, never with other machines */
	CHECK(ae_randomizer_rules_plays(0, 1, 0, 0) == 1);
	CHECK(ae_randomizer_rules_plays(0, 1, 1, 0) == 0);
	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("ae_randomizer_rules_test: all passed\n");

	return 0;
}
