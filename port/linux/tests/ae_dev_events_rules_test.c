#include <stdio.h>
#include <string.h>
#include "ae_dev_events_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static char made[120][48];

int main(void)
{
	const char *names[130];
	int remove[130], count, index;

	/* (the stream's own names only) */
	CHECK(ae_dev_events_is_stream("events-20261103-215952.jsonl"));
	CHECK(!ae_dev_events_is_stream("events-20261103-215952.jsonl.bak"));
	CHECK(!ae_dev_events_is_stream("events-20261103-215952.json"));
	CHECK(!ae_dev_events_is_stream("events-2026110-2159521.jsonl"));
	CHECK(!ae_dev_events_is_stream("events-2026110x-215952.jsonl"));
	CHECK(!ae_dev_events_is_stream("Events-20261103-215952.jsonl"));
	CHECK(!ae_dev_events_is_stream("events-1762212000.jsonl"));
	CHECK(!ae_dev_events_is_stream("notes.txt"));
	CHECK(!ae_dev_events_is_stream(""));
	CHECK(!ae_dev_events_is_stream(NULL));

	/* (50 or fewer: nothing goes) */
	for (index = 0; index < 120; index++)
		snprintf(made[index], sizeof made[index], "events-202611%02d-%06d.jsonl", 1 + index / 60, index % 60);
	for (index = 0; index < 50; index++)
		names[index] = made[index];
	CHECK(ae_dev_events_to_remove(names, 50, AE_DEV_EVENTS_KEEP, remove) == 0);

	/* (120 streams listed newest first, with other files among them: the 70 oldest go, oldest first, never another
	file) */
	count = 0;
	for (index = 119; index >= 0; index--)
	{
		names[count++] = made[index];
		if (index % 40 == 0)
			names[count++] = index ? "events-old.jsonl" : "keep-me.jsonl";
	}
	CHECK(ae_dev_events_to_remove(names, count, AE_DEV_EVENTS_KEEP, remove) == 70);
	for (index = 0; index < 70; index++)
		CHECK(!strcmp(names[remove[index]], made[index]));

	/* (a part of the folder: the picks are outlived by at least keep newer ones there, so never one of the newest) */
	for (index = 0; index < 60; index++)
		names[index] = made[2 * index];
	count = ae_dev_events_to_remove(names, 60, AE_DEV_EVENTS_KEEP, remove);
	CHECK(count == 10);
	for (index = 0; index < count; index++)
		CHECK(!strcmp(names[remove[index]], made[2 * index]));

	/* (keep 0: every stream; a name twice: one copy stays with keep 1) */
	names[0] = made[3];
	names[1] = "x.txt";
	names[2] = made[5];
	CHECK(ae_dev_events_to_remove(names, 3, 0, remove) == 2 && remove[0] == 0 && remove[1] == 2);
	names[1] = made[5];
	CHECK(ae_dev_events_to_remove(names, 3, 1, remove) == 2 && remove[0] == 0 && remove[1] == 1);
	CHECK(ae_dev_events_to_remove(NULL, 3, 1, remove) == 0);
	CHECK(ae_dev_events_to_remove(names, 0, 1, remove) == 0);
	if (failures)
		printf("%d failures\n", failures);
	return failures != 0;
}
