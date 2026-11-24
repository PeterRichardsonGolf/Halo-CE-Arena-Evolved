/*
AE_DEV_EVENTS_RULES.C

DEV MODE's event files: which old streams go (ae_dev_events_rules.h).
*/

#include <string.h>

#include "ae_dev_events_rules.h"

static int digits(const char *text, int count)
{
	int index;

	for (index = 0; index < count; index++)
	{
		if (text[index] < '0' || text[index] > '9')
			return 0;
	}
	return 1;
}

int ae_dev_events_is_stream(const char *name)
{
	/* events-20261103-215952.jsonl */
	return name && strlen(name) == 28 && !strncmp(name, "events-", 7) && digits(name + 7, 8) && name[15] == '-' &&
		digits(name + 16, 6) && !strcmp(name + 22, ".jsonl");
}

int ae_dev_events_to_remove(const char *const *names, int count, int keep, int *remove)
{
	int index, picked = 0;

	if (!names || !remove || count <= 0)
		return 0;
	if (keep < 0)
		keep = 0;
	for (index = 0; index < count; index++)
	{
		int other, newer = 0;

		if (!ae_dev_events_is_stream(names[index]))
			continue;
		/* (newer: a later name, or the same name further on: a name listed twice counts once as the newer) */
		for (other = 0; other < count; other++)
		{
			int order;

			if (other == index || !ae_dev_events_is_stream(names[other]))
				continue;
			order = strcmp(names[other], names[index]);
			if (order > 0 || (order == 0 && other > index))
				newer++;
		}
		if (newer >= keep)
			remove[picked++] = index;
	}
	/* (the oldest first) */
	for (index = 1; index < picked; index++)
	{
		int value = remove[index], at = index;

		while (at > 0 && strcmp(names[remove[at - 1]], names[value]) > 0)
		{
			remove[at] = remove[at - 1];
			at--;
		}
		remove[at] = value;
	}
	return picked;
}
