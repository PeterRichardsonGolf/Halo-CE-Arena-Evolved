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

	/* hearing: none below NORMAL, farther each level */
	CHECK(ae_bots_team_hearing_range(AE_BOTS_LEVEL_EASY) == 0.0f);
	CHECK(ae_bots_team_hearing_range(AE_BOTS_LEVEL_NORMAL) == 15.0f);
	for (level = AE_BOTS_LEVEL_HEROIC; level < AE_BOTS_LEVELS; level++)
		CHECK(ae_bots_team_hearing_range(level) > ae_bots_team_hearing_range(level - 1));

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
	/* MINI SQUADS (owner 10-30): big teams only; squads of 3 or 4; every squad its own route; teams start apart */
	CHECK(ae_bots_team_mini_squads(5, 5) == 0);
	CHECK(ae_bots_team_mini_squads(7, 7) == 2);
	CHECK(ae_bots_team_mini_squads(16, 16) == 5);
	CHECK(ae_bots_team_mini_squads(4, 7) == 2);
	CHECK(ae_bots_team_mini_squads(1, 7) == 0);
	for (int bots = 2; bots <= 32; bots++)
	{
		int sizes[16] = { 0 };
		int squads = ae_bots_team_mini_squads(bots, 32);

		for (int ordinal = 0; ordinal < bots; ordinal++)
			sizes[ae_bots_team_mini_squad_of(ordinal, bots, squads)]++;
		for (int squad = 0; squad < squads; squad++)
			CHECK(bots >= 6 ? sizes[squad] >= 3 && sizes[squad] <= 4 : sizes[squad] >= 2 && sizes[squad] <= 3);
	}
	/* a lone human gets a squad; several share the bots */
	CHECK(ae_bots_team_human_squad(1, 0) == 0);
	CHECK(ae_bots_team_human_squad(1, 3) == 1);
	CHECK(ae_bots_team_human_squad(1, 4) == 2);
	CHECK(ae_bots_team_human_squad(1, 7) == 3);
	CHECK(ae_bots_team_human_squad(2, 6) == 1);
	CHECK(ae_bots_team_human_squad(2, 8) == 2);
	/* two flag crews left: 8 a side with a human, one defender, leaves 2 with the human and 4 free */
	CHECK(ae_bots_team_human_squad_leaving_crews(3, 6) == 2);
	CHECK(ae_bots_team_human_squad_leaving_crews(3, 7) == 3);
	CHECK(ae_bots_team_human_squad_leaving_crews(2, 4) == 1);
	CHECK(ae_bots_team_human_squad_leaving_crews(1, 2) == 1);
	CHECK(ae_bots_team_mini_squads(4, 6) == 2);
	CHECK(ae_bots_team_mini_squad_route(0, 0, 0) != ae_bots_team_mini_squad_route(1, 0, 0));
	CHECK(ae_bots_team_mini_squad_route(0, 0, 0) != ae_bots_team_mini_squad_route(0, 1, 0));
	CHECK(ae_bots_team_mini_squad_route(0, 0, 0) != ae_bots_team_mini_squad_route(0, 0, 1));
	return failures != 0;
}
