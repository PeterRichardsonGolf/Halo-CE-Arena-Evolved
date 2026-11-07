/*
AE_TEST_RUN_SAVED.H

TEST RUN's undo bookkeeping: the config values a test's setup changed, each with the value it had before the
first change, so that undo can put every one back exactly. Pure: string only, no engine headers.
*/

#ifndef AE_TEST_RUN_SAVED_H
#define AE_TEST_RUN_SAVED_H

/* (a test's 16 sets and the 3 keys the setup itself changes, with room) */
#define AE_TEST_RUN_SAVED_MAXIMUM 24

struct ae_test_run_saved
{
	short count;
	struct { char key[64]; char value[128]; } item[AE_TEST_RUN_SAVED_MAXIMUM];
};

void ae_test_run_saved_clear(struct ae_test_run_saved *saved);
/* notes that key had this value before a change: only the first note of a key is kept (a later one saw a changed value);
0 when the table is full or a text does not fit (nothing noted) */
int ae_test_run_saved_note(struct ae_test_run_saved *saved, const char *key, const char *value);
/* calls restore(key, value, context) for every noted key, the last noted first, then clears the table. Returns how many
restore calls returned 0 */
int ae_test_run_saved_undo(struct ae_test_run_saved *saved, int (*restore)(const char *key, const char *value, void *context),
	void *context);

#endif
