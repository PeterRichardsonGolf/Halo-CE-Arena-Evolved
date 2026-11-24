/*
AE_SOCIAL_NAMES_RULES.C

The offline social career's bot names, as pure rules (ae_social_names_rules.h).
*/

#include "ae_social_names_rules.h"

#include <ctype.h>
#include <stddef.h>

static char const *const pool[] =
{
#include "ae_social_names_pool.inc"
};

#define POOL_COUNT ((int)(sizeof pool / sizeof pool[0]))
/* the most names one pick looks through (the pool's size, rounded up) */
#define PICK_MOST 1024

int ae_social_names_count(void)
{
	return POOL_COUNT;
}

char const *ae_social_names_get(int index)
{
	return index >= 0 && index < POOL_COUNT ? pool[index] : NULL;
}

static int same_name(char const *a, char const *b)
{
	while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b))
	{
		a++;
		b++;
	}
	return !*a && !*b;
}

/* (xorshift32: the same numbers on every machine) */
static unsigned long next(unsigned long *state)
{
	unsigned long x = *state & 0xFFFFFFFFUL;

	x ^= (x << 13) & 0xFFFFFFFFUL;
	x ^= x >> 17;
	x ^= (x << 5) & 0xFFFFFFFFUL;
	*state = x;
	return x;
}

int ae_social_names_pick(unsigned long seed, int wanted, char const *const *avoid, int avoid_count, int *out)
{
	static int order[PICK_MOST];
	unsigned long state = (seed & 0xFFFFFFFFUL) ^ 0x9E3779B9UL;
	int count = POOL_COUNT < PICK_MOST ? POOL_COUNT : PICK_MOST, index, picked = 0;

	if (!state)
		state = 0x2545F491UL;
	for (index = 0; index < count; index++)
		order[index] = index;
	/* (a shuffle of the pool's order as far as needed, the avoided names passed over) */
	for (index = 0; index < count && picked < wanted; index++)
	{
		int swap = index + (int)(next(&state) % (unsigned long)(count - index)), held = order[index], other;

		order[index] = order[swap];
		order[swap] = held;
		for (other = 0; other < avoid_count; other++)
		{
			if (avoid[other] && same_name(avoid[other], pool[order[index]]))
				break;
		}
		if (other < avoid_count)
			continue;
		out[picked++] = order[index];
	}
	return picked;
}

unsigned long ae_social_name_hash(char const *name)
{
	unsigned long hash = 2166136261UL;

	for (; name && *name; name++)
	{
		hash ^= (unsigned long)tolower((unsigned char)*name);
		hash = (hash * 16777619UL) & 0xFFFFFFFFUL;
	}
	return hash;
}
