/* ae_path_cost_test.c: a vehicle search pays more for narrow edges, a search on foot pays nothing. */

#include <assert.h>
#include <stdio.h>

#include "ae_path_cost.h"

int main(void)
{
	float base = 10.0f, wide, narrow, other;

	/* on foot: nothing changes */
	ae_path_cost_begin(0.25f, 7);
	assert(ae_path_cost_edge(base, 1.0f, 3, 4) == base);
	ae_path_cost_end();

	/* a vehicle: a wide edge pays only the noise share, a narrow edge pays more */
	ae_path_cost_begin(1.0f, 7);
	wide = ae_path_cost_edge(base, 16.0f, 3, 4);
	narrow = ae_path_cost_edge(base, 1.0f, 3, 4);
	assert(wide >= base && wide <= base * 1.151f);
	assert(narrow > wide);
	assert(narrow <= base * 1.651f);
	ae_path_cost_end();

	/* disarmed again */
	assert(ae_path_cost_edge(base, 1.0f, 3, 4) == base);

	/* another driver tastes differently or the same, but always within the noise share */
	ae_path_cost_begin(1.0f, 7);
	narrow = ae_path_cost_edge(base, 1.0f, 3, 4);
	ae_path_cost_end();
	ae_path_cost_begin(1.0f, 1234567);
	other = ae_path_cost_edge(base, 1.0f, 3, 4);
	ae_path_cost_end();
	assert(other >= base && other <= base * 1.651f);
	assert(narrow >= base && narrow <= base * 1.651f);

	/* a zero or negative cost passes through untouched */
	ae_path_cost_begin(1.0f, 7);
	assert(ae_path_cost_edge(0.0f, 1.0f, 3, 4) == 0.0f);
	ae_path_cost_end();

	/* per edge: the same seed and edge repeat exactly; edges differ; two seeds split the same equal edges; never below
	the base cost (the search's estimate stays a lower bound) */
	{
		int differ_edges = 0, differ_seeds = 0, edge;
		float lowest = 2.0f, highest = -1.0f;

		for (edge = 0; edge < 200; edge++)
		{
			float a = ae_path_cost_noise(11, edge, edge + 1), b = ae_path_cost_noise(11, edge, edge + 1);
			float c = ae_path_cost_noise(11, edge + 1, edge + 2), d = ae_path_cost_noise(99, edge, edge + 1);

			assert(a == b);
			assert(a >= 0.0f && a <= 1.0f);
			lowest = a < lowest ? a : lowest;
			highest = a > highest ? a : highest;
			differ_edges += a != c;
			differ_seeds += a != d;
		}
		assert(differ_edges > 150 && differ_seeds > 150);
		assert(lowest < 0.15f && highest > 0.85f);
		ae_path_cost_begin(1.0f, 7);
		for (edge = 0; edge < 200; edge++)
		{
			float cost = ae_path_cost_edge(base, 100.0f, edge, edge + 5);

			assert(cost >= base && cost <= base * 1.151f);
		}
		/* equal alternatives: the cheaper of two parallel edges differs between drivers */
		{
			int wins_a = 0, seed;

			for (seed = 1; seed <= 64; seed++)
			{
				float one, two;

				ae_path_cost_begin(1.0f, (unsigned long)seed);
				one = ae_path_cost_edge(base, 100.0f, 10, 20);
				two = ae_path_cost_edge(base, 100.0f, 10, 21);
				wins_a += one < two;
			}
			assert(wins_a > 10 && wins_a < 54);
		}
		ae_path_cost_end();
	}

	/* LEARNT WEDGES: a piece wedged on twice costs a vehicle more, more for more wedges up to the most; on foot nothing; off
	(the classic driving) nothing; a new map starts clean */
	{
		float base = 10.0f, plain, two, many;
		int index;

		ae_path_cost_wedges_clear();
		ae_path_cost_begin(1.0f, 3);
		plain = ae_path_cost_edge(base, 100.0f, 1, 40);
		ae_path_cost_wedge(40);
		assert(ae_path_cost_wedges(40) == 1 && ae_path_cost_edge(base, 100.0f, 1, 40) == plain);
		ae_path_cost_wedge(40);
		two = ae_path_cost_edge(base, 100.0f, 1, 40);
		assert(two > plain + base * 1.9f && two < plain + base * 2.1f);
		for (index = 0; index < 10; index++)
			ae_path_cost_wedge(40);
		many = ae_path_cost_edge(base, 100.0f, 1, 40);
		assert(many > plain + base * 3.9f && many < plain + base * 4.1f);
		/* (only the edge into the piece: out of it costs as before) */
		assert(ae_path_cost_edge(base, 100.0f, 40, 1) < base * 1.151f);
		ae_path_cost_wedges_on(0);
		assert(ae_path_cost_edge(base, 100.0f, 1, 40) == plain);
		ae_path_cost_wedges_on(1);
		ae_path_cost_end();
		assert(ae_path_cost_edge(base, 100.0f, 1, 40) == base);
		ae_path_cost_begin(0.25f, 3);
		assert(ae_path_cost_edge(base, 100.0f, 1, 40) == base);
		ae_path_cost_end();
		/* a full list: the least wedged gives way, the much-wedged stays */
		for (index = 0; index < AE_PATH_WEDGE_PIECES + 5; index++)
			ae_path_cost_wedge(1000 + index);
		assert(ae_path_cost_wedges(40) == 12);
		ae_path_cost_wedge(-1);
		ae_path_cost_wedges_clear();
		assert(ae_path_cost_wedges(40) == 0);
	}

	puts("ae_path_cost_test: all passed");
	return 0;
}
