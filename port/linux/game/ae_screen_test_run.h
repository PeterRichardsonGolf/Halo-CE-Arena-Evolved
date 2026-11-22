/*
AE_SCREEN_TEST_RUN.H

The TEST RUN screen (ae_screen_test_run.c; the design: the owner's mockup 8): a row per test list, the files
<data root>/test-runs/*.txt, under TO TEST (lists with tests left to play) and DONE (every test passed, or a list that
cannot be read, greyed with why). A row shows the list's title, the state of its tests
(see value_text in ae_screen_test_run.c: ALL PASSED . OCT 24, N TO TEST, or what is left: 1 FAIL . 1 NEW . RESUME). A plays the pending tests (ae_test_run_start), X plays every
test again (ae_test_run_start_all; it erases nothing), B goes back. DEV MODE only: it does not open without it.
Opened from SETTINGS > SYSTEM > TEST RUN, from the end of a run (ae_test_run.c), and by debug.ae_test_screen 36.

Its log: "ae test run screen: open (N lists)", "row <id>: <its value>" for each list, "start <id> (pending|all)", "closed".
*/

#ifndef __AE_SCREEN_TEST_RUN_H
#define __AE_SCREEN_TEST_RUN_H

/* the TEST RUN screen over the stack; 0 when DEV MODE is off or the stack is full */
int ae_screen_test_run_open(void);

/* SETTINGS' TEST RUN row's value: "3 LISTS TO TEST", "ALL PASSED" or "NO LISTS" (the lists read at most every 2 s) */
const char *ae_screen_test_run_summary(void);

#endif
