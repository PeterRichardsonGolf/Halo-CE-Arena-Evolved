/* ae_test_run_saved_test.c: TEST RUN's undo table (ae_test_run_saved.h): the first value of a key wins, undo goes
last noted first, a full table and a long text are refused. */

#include <stdio.h>
#include <string.h>

#include "ae_test_run_saved.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static char order[512];
static int refuse;

static int restore(const char *key, const char *value, void *context)
{
	(void)context;
	strcat(order, key);
	strcat(order, "=");
	strcat(order, value);
	strcat(order, ";");
	return !refuse;
}

int main(void)
{
	static struct ae_test_run_saved saved;
	char key[16];
	int index;

	ae_test_run_saved_clear(&saved);
	/* (a key changed twice goes back to what it was before the first change) */
	CHECK(ae_test_run_saved_note(&saved, "a.x", "1"));
	CHECK(ae_test_run_saved_note(&saved, "b.y", ""));
	CHECK(ae_test_run_saved_note(&saved, "a.x", "2"));
	CHECK(saved.count == 2);
	CHECK(ae_test_run_saved_undo(&saved, restore, NULL) == 0);
	CHECK(!strcmp(order, "b.y=;a.x=1;"));
	CHECK(saved.count == 0);

	/* (a second undo has nothing to do) */
	order[0] = 0;
	CHECK(ae_test_run_saved_undo(&saved, restore, NULL) == 0);
	CHECK(order[0] == 0);

	/* (a failed restore is counted and the rest still goes back) */
	CHECK(ae_test_run_saved_note(&saved, "a.x", "1"));
	CHECK(ae_test_run_saved_note(&saved, "b.y", "2"));
	refuse = 1;
	CHECK(ae_test_run_saved_undo(&saved, restore, NULL) == 2);
	CHECK(saved.count == 0);
	refuse = 0;

	/* (full table, long texts) */
	for (index = 0; index < AE_TEST_RUN_SAVED_MAXIMUM; index++)
	{
		snprintf(key, sizeof key, "k.%d", index);
		CHECK(ae_test_run_saved_note(&saved, key, "v"));
	}
	CHECK(!ae_test_run_saved_note(&saved, "k.extra", "v"));
	CHECK(ae_test_run_saved_note(&saved, "k.3", "again"));
	ae_test_run_saved_clear(&saved);
	{
		char longkey[100], longvalue[200];

		memset(longkey, 'k', sizeof longkey - 1);
		longkey[sizeof longkey - 1] = 0;
		memset(longvalue, 'v', sizeof longvalue - 1);
		longvalue[sizeof longvalue - 1] = 0;
		CHECK(!ae_test_run_saved_note(&saved, longkey, "v"));
		CHECK(!ae_test_run_saved_note(&saved, "a.x", longvalue));
		CHECK(saved.count == 0);
	}
	if (failures)
		return 1;
	printf("ae_test_run_saved_test: ok\n");
	return 0;
}
