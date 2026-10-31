/*
AE_TEST_RUN_LIST.H

TEST RUN's list format and its parser. A list is one text file of tests the
owner plays in game (see tools/ae_test/test_runs/sample.txt). One line is a
key and a value (the rest of the line, trimmed); '#' lines and blank lines are
ignored; keys are lowercase. A value is the rest of the line, trimmed, and an
inline "#" is part of the value: only a line that starts with "#" is a comment.
Numbers are plain digits (leading zeros are fine). Pure: stdio and string only, no engine headers.

	list <id>        the list's id: [a-z0-9-], the file name without .txt
	revision <n>     raise it and put `retest <new revision>` on each test to play again (a bump alone re-opens nothing)
	title <text>     the list's title (before the first test), then each test's
	test <id>        starts a test: [a-z0-9-], unique in the list
	check <text>     1 to 3 per test: what to look at
	map <name>       or screen <n> (debug.ae_test_screen), or menus: exactly one of them. menus (no value) is a
	                 test of AE's menus out of gameplay (a lobby, PROFILES...): nothing is loaded, its checks show
	                 once the menus are up, wherever the owner is in them
	gametype <name>  optional with a map
	set <key> <value>  a config key applied for this test only (up to 16); a plain setting, not
	                 a per-player one or one that other settings derive from
	players <1-4>    optional: local players
	video            optional flag
	card top|bottom  optional: where the card sits (top by default)
	retest <n>       optional: the revision since which an older PASS no longer counts

A bad line never drops the list: it goes in that test's error (or the list's),
and the test shows as LIST ERROR. The parser does not check that a config key
exists; the runner does when it applies the set, and the linter before shipping.
*/

#ifndef AE_TEST_RUN_LIST_H
#define AE_TEST_RUN_LIST_H

#define AE_TEST_RUN_TESTS 64
#define AE_TEST_RUN_CHECKS 3
#define AE_TEST_RUN_SETS 16

struct ae_test_run_set { char key[64]; char value[128]; };

struct ae_test_run_test
{
	char id[32], title[80], check[AE_TEST_RUN_CHECKS][64];
	short checks, players, sets, card_bottom, video, retest, screen; /* screen 0 = a map test */
	short menus;     /* a menus test: no map and no screen */
	char map[64], gametype[64];
	struct ae_test_run_set set[AE_TEST_RUN_SETS];
	char error[96]; /* "" or why this test cannot run (shown as LIST ERROR) */
};

/* (about 225 KB: 64 tests of about 3.5 KB each. Callers keep one static or on the heap, never on the stack) */
struct ae_test_run_list
{
	char id[32], title[64];
	short revision, tests;
	struct ae_test_run_test test[AE_TEST_RUN_TESTS];
	char error[96]; /* "" or what is wrong with the list as a whole */
};

/* whether text is a list or test id as the parser takes it: [a-z0-9-], 1 to 31 characters */
int ae_test_run_list_id_ok(const char *id);
/* parses a list's text: 1 = usable (some tests may carry an error), 0 = no list (no "list" line) */
int ae_test_run_list_parse(const char *text, struct ae_test_run_list *out);
/* reads the file at path and parses it: 0 when it cannot be read or holds no list */
int ae_test_run_list_load(const char *path, struct ae_test_run_list *out);

#endif
