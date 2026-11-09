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

	puts("ae_path_cost_test: all passed");
	return 0;
}
