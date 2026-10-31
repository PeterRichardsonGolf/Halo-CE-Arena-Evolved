/*
AE_TEST_RUN_RESULTS.H

TEST RUN's results file: one JSON object per line, appended as the owner
verdicts each test, kept across runs. Pure: stdio and string only, no engine
headers and no file paths (the runner decides where the file lives).

A record's keys come in this fixed order:
	run, list, revision, test, verdict, note, shot, video, build, time
The writer escapes '"', '\' and control characters; bytes >= 0x80 pass through.
A reader skips a line that does not parse (a torn last line after a crash). If a torn
fragment has a record appended right after it, the reader recovers that record from the last {"run": in the line.

A test is PASSED, FAILED or SKIPPED by its latest PASS, FAIL or SKIP record for
that list and test, if that record's revision is at least the test's retest
(0 when absent); else it is TODO. A NOTE record never changes a state. Pending
is every test not PASSED, and a run plays only the pending tests, in list order.
*/

#ifndef AE_TEST_RUN_RESULTS_H
#define AE_TEST_RUN_RESULTS_H

#include <stdio.h>

#include "ae_test_run_list.h"

enum ae_test_run_verdict { AE_TEST_RUN_NONE, AE_TEST_RUN_PASS, AE_TEST_RUN_FAIL, AE_TEST_RUN_SKIP, AE_TEST_RUN_NOTE };

struct ae_test_run_record
{
	char run[24], list[32], test[32], note[256], shot[160], video[160], build[48], time[24];
	short revision;
	enum ae_test_run_verdict verdict;
};

/* writes the record (every char field NUL-terminated; revision >= 0) as one line ending in "\n", then fflush + fsync (_commit on Windows): 1 = ok, 0 = any step failed */
int ae_test_run_record_write(FILE *file, struct ae_test_run_record const *record);
/* writes a line end and syncs, as the record writer does: for a file whose text does not end in one (a crash tore its last
line), so the next record starts a line of its own and is not glued to the fragment. 1 = ok */
int ae_test_run_file_end_line(FILE *file);
/* parses one line (with or without its line end): 0 on a torn or bad line, and then out is unspecified */
int ae_test_run_record_parse(const char *line, struct ae_test_run_record *out);

enum ae_test_run_state { AE_TEST_RUN_TODO, AE_TEST_RUN_PASSED, AE_TEST_RUN_FAILED, AE_TEST_RUN_SKIPPED };

/* the state of each test of a list from its results text (NULL or empty: all TODO); states holds list->tests entries */
void ae_test_run_states(struct ae_test_run_list const *list, const char *results_text, enum ae_test_run_state *states);
/* a crashed run's resume point: the first test (list order) that was not PASSED before the list's last run (the run of the
last record that reads) and that run did not record, a NOTE too; -1 when the last run left none, or there is no record. A
test with a list error is never one. Use it with the file's text (NULL: -1). */
short ae_test_run_resume_index(struct ae_test_run_list const *list, const char *results_text);
/* how many tests are to run: those not PASSED */
short ae_test_run_pending(struct ae_test_run_list const *list, enum ae_test_run_state const *states);
/* the index of the first test to run, or -1 when none is */
short ae_test_run_first_pending(struct ae_test_run_list const *list, enum ae_test_run_state const *states);

#endif
