#include <stdio.h>
#include "ae_ally_reticle_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* is_ally(is_self, dead, team_game, side_game, same_team, engine_enemy) */
int main(void)
{
	/* a team game: a living teammate is an ally, an opponent is not */
	CHECK(ae_ally_reticle_rules_is_ally(0, 0, 1, 0, 1, 0) == 1);
	CHECK(ae_ally_reticle_rules_is_ally(0, 0, 1, 0, 0, 1) == 0);
	CHECK(ae_ally_reticle_rules_is_ally(0, 0, 1, 0, 0, 0) == 0);
	/* never yourself, never the dead, never one the game calls an enemy */
	CHECK(ae_ally_reticle_rules_is_ally(1, 0, 1, 0, 1, 0) == 0);
	CHECK(ae_ally_reticle_rules_is_ally(0, 1, 1, 0, 1, 0) == 0);
	CHECK(ae_ally_reticle_rules_is_ally(0, 0, 1, 0, 1, 1) == 0);
	/* a free for all game: nobody, even on the same team index */
	CHECK(ae_ally_reticle_rules_is_ally(0, 0, 0, 0, 1, 0) == 0);
	CHECK(ae_ally_reticle_rules_is_ally(0, 0, 0, 0, 0, 1) == 0);
	/* co-op and the campaign: any unit that is not an enemy, on any team index (a marine) */
	CHECK(ae_ally_reticle_rules_is_ally(0, 0, 0, 1, 0, 0) == 1);
	CHECK(ae_ally_reticle_rules_is_ally(0, 0, 0, 1, 1, 0) == 1);
	CHECK(ae_ally_reticle_rules_is_ally(0, 0, 0, 1, 0, 1) == 0);
	CHECK(ae_ally_reticle_rules_is_ally(1, 0, 0, 1, 1, 0) == 0);
	CHECK(ae_ally_reticle_rules_is_ally(0, 1, 0, 1, 1, 0) == 0);

	/* green only with the setting on, an ally, and the plain frame (the red frame wins) */
	CHECK(ae_ally_reticle_rules_green(1, 1, 0) == 1);
	CHECK(ae_ally_reticle_rules_green(0, 1, 0) == 0);
	CHECK(ae_ally_reticle_rules_green(1, 0, 0) == 0);
	CHECK(ae_ally_reticle_rules_green(1, 1, 1) == 0);

	/* the green keeps the reticle's alpha */
	CHECK(ae_ally_reticle_rules_color(0xFF3366FFul) == 0xFF00FF00ul);
	CHECK(ae_ally_reticle_rules_color(0x80FFFFFFul) == 0x8000FF00ul);
	CHECK(ae_ally_reticle_rules_color(0x00123456ul) == 0x0000FF00ul);

	if (failures)
		return 1;
	printf("ae_ally_reticle_rules_test: all passed\n");
	return 0;
}
