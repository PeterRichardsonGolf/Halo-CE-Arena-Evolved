/* ae_test_run_saved.c: Arena Evolved TEST RUN, the undo table of a test's config changes (see
ae_test_run_saved.h). No engine includes. */

#include <string.h>

#include "ae_test_run_saved.h"

void ae_test_run_saved_clear(struct ae_test_run_saved *saved)
{
	saved->count = 0;
}

int ae_test_run_saved_note(struct ae_test_run_saved *saved, const char *key, const char *value)
{
	short index;

	if (strlen(key) >= sizeof(saved->item[0].key) || strlen(value) >= sizeof(saved->item[0].value))
		return 0;
	for (index = 0; index < saved->count; index++)
	{
		if (!strcmp(saved->item[index].key, key))
			return 1;
	}
	if (saved->count >= AE_TEST_RUN_SAVED_MAXIMUM)
		return 0;
	strcpy(saved->item[saved->count].key, key);
	strcpy(saved->item[saved->count].value, value);
	saved->count++;
	return 1;
}

int ae_test_run_saved_undo(struct ae_test_run_saved *saved, int (*restore)(const char *key, const char *value, void *context),
	void *context)
{
	int failed = 0;

	while (saved->count > 0)
	{
		saved->count--;
		if (!restore(saved->item[saved->count].key, saved->item[saved->count].value, context))
			failed++;
	}
	return failed;
}
