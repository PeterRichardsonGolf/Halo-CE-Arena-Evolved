#include <stdio.h>
#include "ae_bots_team_rules.h"
#include "ae_bots_combat_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	int level;

	/* the numbers game: NORMAL and up; two behind holds, two ahead pushes, one either way as ever */
	CHECK(ae_bots_team_stance(AE_BOTS_LEVEL_NORMAL, 2, 4) == -1);
	CHECK(ae_bots_team_stance(AE_BOTS_LEVEL_NORMAL, 4, 2) == 1);
	CHECK(ae_bots_team_stance(AE_BOTS_LEVEL_HEROIC, 3, 4) == 0);
	CHECK(ae_bots_team_stance(AE_BOTS_LEVEL_DEMON, 1, 3) == -1);
	CHECK(ae_bots_team_stance(AE_BOTS_LEVEL_EASY, 1, 4) == 0);
	CHECK(ae_bots_team_stance(AE_BOTS_LEVEL_RECRUIT, 4, 1) == 0);

	/* outnumbered: nothing when even or ahead, or below NORMAL; more each level, 0.6 at most */
	CHECK(ae_bots_team_outnumbered_penalty(AE_BOTS_LEVEL_HEROIC, 1, 1) == 0.0f);
	CHECK(ae_bots_team_outnumbered_penalty(AE_BOTS_LEVEL_HEROIC, 1, 2) == 0.0f);
	CHECK(ae_bots_team_outnumbered_penalty(AE_BOTS_LEVEL_EASY, 3, 1) == 0.0f);
	CHECK(ae_bots_team_outnumbered_penalty(AE_BOTS_LEVEL_NORMAL, 2, 1) == 0.2f);
	CHECK(ae_bots_team_outnumbered_penalty(AE_BOTS_LEVEL_HEROIC, 2, 1) == 0.25f);
	CHECK(ae_bots_team_outnumbered_penalty(AE_BOTS_LEVEL_LEGENDARY, 2, 1) == 0.3f);
	CHECK(ae_bots_team_outnumbered_penalty(AE_BOTS_LEVEL_DEMON, 2, 1) == 0.3f);
	CHECK(ae_bots_team_outnumbered_penalty(AE_BOTS_LEVEL_DEMON, 6, 1) == 0.6f);
	/* (a full-strength 2v1 keeps it fighting: 0.5 even, less 0.3, stays above the retreat's 0.1) */
	CHECK(0.5f - ae_bots_team_outnumbered_penalty(AE_BOTS_LEVEL_DEMON, 2, 1) >
		ae_bots_combat_retreat_threshold(AE_BOTS_LEVEL_DEMON));

	/* angle isolation: HEROIC and up, nothing for one angle, more each level, 0.6 at most */
	CHECK(ae_bots_team_exposed_penalty(AE_BOTS_LEVEL_NORMAL, 3) == 0.0f);
	CHECK(ae_bots_team_exposed_penalty(AE_BOTS_LEVEL_HEROIC, 1) == 0.0f);
	CHECK(ae_bots_team_exposed_penalty(AE_BOTS_LEVEL_HEROIC, 2) == 0.1f);
	CHECK(ae_bots_team_exposed_penalty(AE_BOTS_LEVEL_LEGENDARY, 2) == 0.15f);
	CHECK(ae_bots_team_exposed_penalty(AE_BOTS_LEVEL_DEMON, 3) == 0.4f);
	CHECK(ae_bots_team_exposed_penalty(AE_BOTS_LEVEL_DEMON, 9) == 0.6f);

	/* wingman: EASY and up; past 8 units goes to the player, within moves about them */
	CHECK(!ae_bots_team_wingman_level(AE_BOTS_LEVEL_RECRUIT));
	for (level = AE_BOTS_LEVEL_EASY; level < AE_BOTS_LEVELS; level++)
		CHECK(ae_bots_team_wingman_level(level));
	CHECK(ae_bots_team_wingman_move(12.0f) == 1);
	CHECK(ae_bots_team_wingman_move(5.0f) == 2);

	if (failures)
		printf("%d failed\n", failures);
	else
		printf("ae_bots_team_rules: all passed\n");
	return failures != 0;
}
