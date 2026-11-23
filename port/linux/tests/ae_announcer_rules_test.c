/* The announcer's pure rules (port/linux/game/ae_announcer_rules.h) */
#include <stdio.h>

#include "ae_announcer_rules.h"

static int failures;
#define AE_CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void auto_pick_checks(void)
{
	int none[1] = { 0 };
	int three[3] = { 50, 50, 55 };
	int tie[3] = { 12, 40, 40 };
	int few[2] = { AE_ANNOUNCER_AUTO_LEAST - 1, 3 };
	int least[2] = { AE_ANNOUNCER_AUTO_LEAST - 1, AE_ANNOUNCER_AUTO_LEAST };

	/* the folder whose banks hold the most lines */
	AE_CHECK(ae_announcer_auto_pick(three, 3) == 2);
	/* a tie: the first listed */
	AE_CHECK(ae_announcer_auto_pick(tie, 3) == 1);
	/* too few lines anywhere, no folders, no list: none (the game's own announcer) */
	AE_CHECK(ae_announcer_auto_pick(few, 2) == -1);
	AE_CHECK(ae_announcer_auto_pick(none, 1) == -1);
	AE_CHECK(ae_announcer_auto_pick(none, 0) == -1);
	AE_CHECK(ae_announcer_auto_pick(NULL, 3) == -1);
	/* exactly the least is enough */
	AE_CHECK(ae_announcer_auto_pick(least, 2) == 1);
}

int main(void)
{
	unsigned long draw;
	int counts[6], index;

	AE_CHECK(ae_announcer_take(0, 0, 0) == -1 && ae_announcer_take(-1, -2, 7) == -1);
	for (draw = 0; draw < 1000; draw++)
		AE_CHECK(ae_announcer_take(1, 0, draw) == 0);
	for (index = 0; index < 6; index++)
		counts[index] = 0;
	for (draw = 0; draw < 20000; draw++)
		counts[ae_announcer_take(2, 0, draw)]++;
	AE_CHECK(counts[0] == 10000 && counts[1] == 10000);
	AE_CHECK(ae_announcer_take(1, 1, 0) == 1 && ae_announcer_take(1, 1, 100) == 1);
	AE_CHECK(ae_announcer_take(1, 1, 1) == 0 && ae_announcer_take(1, 1, 99) == 0 && ae_announcer_take(1, 1, 101) == 0);
	for (index = 0; index < 6; index++)
		counts[index] = 0;
	for (draw = 0; draw < 100000; draw++)
		counts[ae_announcer_take(1, 3, draw)]++;
	AE_CHECK(counts[0] == 99000 && counts[1] + counts[2] + counts[3] == 1000);
	AE_CHECK(counts[1] > 300 && counts[2] > 300 && counts[3] > 300);
	for (index = 0; index < 6; index++)
		counts[index] = 0;
	for (draw = 0; draw < 200000; draw++)
		counts[ae_announcer_take(2, 1, draw)]++;
	AE_CHECK(counts[2] == 2000 && counts[0] > 98000 && counts[1] > 98000);
	AE_CHECK(ae_announcer_take(0, 2, 0) >= 0 && ae_announcer_take(0, 2, 12345) < 2);

	{
		int run = 0, hits = 0, kill;

		for (kill = 1; kill <= 20; kill++)
		{
			if (ae_announcer_run_kill(&run))
			{
				hits++;
				AE_CHECK(kill == AE_ANNOUNCER_RUN_KILLS);
			}
		}
		AE_CHECK(hits == 1);
		ae_announcer_run_end(&run);
		AE_CHECK(run == 0);
		for (kill = 1; kill <= 7; kill++)
			AE_CHECK(!ae_announcer_run_kill(&run));
		ae_announcer_run_end(&run);
		for (kill = 1; kill <= 7; kill++)
			AE_CHECK(!ae_announcer_run_kill(&run));
		AE_CHECK(ae_announcer_run_kill(&run));
	}

	{
		struct ae_announcer_burst burst = { 0, 0, 0, 0 };

		AE_CHECK(!ae_announcer_burst_kill(&burst, 100, 1, 7));
		AE_CHECK(!ae_announcer_burst_kill(&burst, 100, 1, 7));
		AE_CHECK(ae_announcer_burst_kill(&burst, 100, 1, 7));
		AE_CHECK(!ae_announcer_burst_kill(&burst, 100, 1, 7));
		AE_CHECK(!ae_announcer_burst_kill(&burst, 200, 1, 7));
		AE_CHECK(!ae_announcer_burst_kill(&burst, 200, 1, 7));
		AE_CHECK(!ae_announcer_burst_kill(&burst, 201, 1, 7));
		AE_CHECK(!ae_announcer_burst_kill(&burst, 201, 1, 7));
		AE_CHECK(!ae_announcer_burst_kill(&burst, 300, 1, 7));
		AE_CHECK(!ae_announcer_burst_kill(&burst, 300, 2, 7));
		AE_CHECK(!ae_announcer_burst_kill(&burst, 300, 2, 8));
		AE_CHECK(!ae_announcer_burst_kill(&burst, 300, 2, 8));
		AE_CHECK(ae_announcer_burst_kill(&burst, 300, 2, 8));
	}
	auto_pick_checks();
	/* a pack speaks only the medals said aloud in play */
	AE_CHECK(ae_announcer_pack_speaks_medal("medal_double_kill"));
	AE_CHECK(ae_announcer_pack_speaks_medal("medal_killing_spree"));
	AE_CHECK(ae_announcer_pack_speaks_medal("medal_killjoy"));
	/* a perfect kill is said aloud */
	AE_CHECK(ae_announcer_pack_speaks_medal("medal_perfect"));
	AE_CHECK(ae_announcer_pack_speaks_medal("medal_unfriggenbelievable_row_wit_unfriggenbelievable"));
	AE_CHECK(!ae_announcer_pack_speaks_medal("medal_kill"));
	AE_CHECK(!ae_announcer_pack_speaks_medal("medal_assist"));
	AE_CHECK(!ae_announcer_pack_speaks_medal("medal_headshot"));
	AE_CHECK(!ae_announcer_pack_speaks_medal("medal_melee_kill"));
	AE_CHECK(!ae_announcer_pack_speaks_medal(""));
	AE_CHECK(!ae_announcer_pack_speaks_medal(NULL));
	if (failures)
		return 1;
	printf("ae_announcer_rules_test: ok\n");
	return 0;
}
