/*
AE_TEST_RUN_LINES.H
TEST RUN's setup lines as pure rules: the order a test's `setup` lines go to the game in, and what happens when the game
refuses one. No engine includes: port/linux/tests/ae_test_run_lines_test.c compiles this alone.

A test's lines run strictly in list order, one at a time: a line is sent, and the next is sent only when the game has
answered it and accepted it. The first line waits until the game has settled (the reset between two tests of a group
done, the first player alive). A refused line is sent again every AE_TEST_RUN_LINES_RETRY_MS until it is accepted or
AE_TEST_RUN_LINES_RETRY_FOR_MS have passed since it was first sent: what it needs (a living player, the weapon just
given in hand, a dummy at its spot) is often a few ticks away. A line the game answers "not yet" (what it acts on is on
its way: a replay that loads, or plays to a seek's second) is sent again so for AE_TEST_RUN_LINES_NOT_YET_FOR_MS
instead, and a line it answers "being made" (what it needs is being written: a tag pack built from the game's maps on
its first use) every AE_TEST_RUN_LINES_BEING_MADE_MS for AE_TEST_RUN_LINES_BEING_MADE_FOR_MS. A line still refused then, a line with no answer in
AE_TEST_RUN_LINES_ANSWER_MS, or a game that never settles, fails the setup: the runner shows it on the card as a SETUP
ERROR with the game's own answer, and the test is not judged as if its scene were there.
*/
#ifndef AE_TEST_RUN_LINES_H
#define AE_TEST_RUN_LINES_H

/* the game settles in well under a second; a line's answer comes on the next tick (a line waiting for a dummy to join:
a second or two); a refused line goes again every three ticks or so, for three seconds */
#define AE_TEST_RUN_LINES_SETTLE_MS 10000
#define AE_TEST_RUN_LINES_ANSWER_MS 10000
#define AE_TEST_RUN_LINES_RETRY_MS 100
#define AE_TEST_RUN_LINES_RETRY_FOR_MS 3000
/* a replay loads in a second or two and plays to a seek's second at 4 times (a minute's in 15 s) */
#define AE_TEST_RUN_LINES_NOT_YET_FOR_MS 20000
/* a tag pack is written in well under a minute on a fast disk; a slow one takes a few */
#define AE_TEST_RUN_LINES_BEING_MADE_MS 1000
#define AE_TEST_RUN_LINES_BEING_MADE_FOR_MS 300000

enum ae_test_run_lines_action
{
	AE_TEST_RUN_LINES_WAIT,   /* nothing to do this frame */
	AE_TEST_RUN_LINES_SEND,   /* send line `line` to the game now, then call again */
	AE_TEST_RUN_LINES_DONE,   /* every line was accepted */
	AE_TEST_RUN_LINES_FAILED  /* the setup failed: why, and at which line */
};

enum ae_test_run_lines_why
{
	AE_TEST_RUN_LINES_OK,
	AE_TEST_RUN_LINES_NOT_SETTLED, /* the game did not settle for the first line */
	AE_TEST_RUN_LINES_REFUSED,     /* a line was still refused after its retries */
	AE_TEST_RUN_LINES_NO_ANSWER    /* a line had no answer */
};

struct ae_test_run_lines
{
	short count;            /* the test's lines */
	short next;             /* the line to send next, or the one out */
	short out;              /* a line is out: its answer is awaited */
	short tries;            /* how often line `next` was sent */
	short settle;           /* the first line waits for the game to settle */
	short finished;         /* DONE or FAILED was returned */
	short why;              /* FAILED: enum ae_test_run_lines_why */
	short slow;             /* line `next` was answered being made: it goes again at that pace */
	unsigned long since;    /* when the wait for the game to settle began */
	unsigned long sent_at;  /* when line `next` was last sent */
	unsigned long first_at; /* when line `next` was first sent */
};

/* a test's `count` lines begin at `now` (milliseconds); settle: the game must settle before the first line (a map test
with lines, or one that goes on in the game of the test before it) */
void ae_test_run_lines_start(struct ae_test_run_lines *lines, short count, int settle, unsigned long now);
/* one step. settled: the game is settled now (asked only before the first line); answered, ok: the line out has its answer
now, and whether the game accepted it (1), refused it (0), answered not yet (-1) or being made (-2). Returns the action; SEND and FAILED name the line in *line (FAILED before any line:
-1). After DONE or FAILED every call returns the same */
enum ae_test_run_lines_action ae_test_run_lines_step(struct ae_test_run_lines *lines, int settled, int answered, int ok,
	unsigned long now, short *line);
/* whether the lines are still under way (not DONE, not FAILED) */
int ae_test_run_lines_pending(struct ae_test_run_lines const *lines);

#endif
