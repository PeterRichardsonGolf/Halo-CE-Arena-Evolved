/*
AE_TEST_RUN_MACHINE.H
TEST RUN's decision machine (DEV MODE): which test is up, what a verdict key does, and the order things happen in.
Pure: no engine calls. Each call returns the effects the runner (ae_test_run.c) carries out, in order, in machine->effect.
	LOADING  a test is being set up (APPLY was the last effect); keys are dropped
	READY    the test is playable; the verdict keys act
	SHOT     FAIL or NOTE was pressed and its screenshot asked for; the box opens on the next call, so it is not in the shot
	NOTE     the note box has the keyboard until the runner says it is done
	VERDICT  a NOTE was saved: the owner picks PASS, FAIL or CONTINUE (SKIP), or stays (records NOTE); one record follows
	SUMMARY  the last test is done; any verdict key ends the run
A test that cannot load (a list error, a setup error, a timeout) is "blocked": ready is ignored, PASS and SKIP
record a SKIP and FAIL and NOTE record a FAIL, each with the error as its note and no box, and the run moves on; RELOAD and
BACK work.
*/
#ifndef AE_TEST_RUN_MACHINE_H
#define AE_TEST_RUN_MACHINE_H

#include "ae_test_run_input.h"
#include "ae_test_run_list.h"
#include "ae_test_run_results.h"

enum ae_test_run_phase { AE_TEST_RUN_PHASE_IDLE, AE_TEST_RUN_PHASE_LOADING, AE_TEST_RUN_PHASE_READY,
	AE_TEST_RUN_PHASE_SHOT, AE_TEST_RUN_PHASE_NOTE, AE_TEST_RUN_PHASE_SUMMARY, AE_TEST_RUN_PHASE_VERDICT };

enum ae_test_run_effect_kind
{
	AE_TEST_RUN_EFFECT_NONE,
	AE_TEST_RUN_EFFECT_APPLY,           /* set the current test up (machine->blocked tells the runner's result back) */
	AE_TEST_RUN_EFFECT_SHOT,            /* ask for the screenshot */
	AE_TEST_RUN_EFFECT_NOTE_OPEN,       /* open the note box */
	AE_TEST_RUN_EFFECT_RECORD,          /* write a record (verdict, note_kind) */
	AE_TEST_RUN_EFFECT_VIDEO_START,     /* start the test's recording (if not already) */
	AE_TEST_RUN_EFFECT_VIDEO_KEEP,      /* stop the recording and keep the clip */
	AE_TEST_RUN_EFFECT_VIDEO_DROP,      /* stop the recording and delete the clip, unless it was kept */
	AE_TEST_RUN_EFFECT_SUMMARY,         /* the last test is done */
	AE_TEST_RUN_EFFECT_FINISH           /* the run ends */
};

enum ae_test_run_note_kind { AE_TEST_RUN_NOTE_NONE, AE_TEST_RUN_NOTE_TYPED, AE_TEST_RUN_NOTE_ERROR };

struct ae_test_run_effect
{
	enum ae_test_run_effect_kind kind;
	enum ae_test_run_verdict verdict;    /* RECORD */
	enum ae_test_run_note_kind note_kind; /* RECORD: no note, the typed one, the test's error text */
	short test;                           /* the list index of the test the effect is for (RECORD: the machine has moved on) */
};

#define AE_TEST_RUN_EFFECTS 8

struct ae_test_run_machine
{
	struct ae_test_run_list const *list;
	short order[AE_TEST_RUN_TESTS]; /* the list's tests this run plays, in list order (a resumed run: from its resume point, then the rest) */
	short count, position;
	short phase, blocked;
	short note_for; /* the verdict the note box is for */
	short passed, failed, skipped, noted;
	struct ae_test_run_effect effect[AE_TEST_RUN_EFFECTS];
	short effects;
};

/* starts a run over the list's pending tests (states: the list's, from its results); 0 = not started: DEV MODE off,
or nothing pending. The first effect is APPLY */
int ae_test_run_machine_start(struct ae_test_run_machine *machine, struct ae_test_run_list const *list,
	enum ae_test_run_state const *states, int dev_mode);
/* as _start, for a run resuming after a crash: the pending tests from list index `first` come first, then the pending
ones before it (the FAILs and SKIPs the crashed run gave stay pending and come last, so a finished resume leaves no
RESUME behind). first < 0 or past the list: list order, as _start */
int ae_test_run_machine_start_at(struct ae_test_run_machine *machine, struct ae_test_run_list const *list,
	enum ae_test_run_state const *states, int dev_mode, short first);
int ae_test_run_machine_active(struct ae_test_run_machine const *machine);
/* the list index of the test up */
short ae_test_run_machine_test(struct ae_test_run_machine const *machine);
/* one frame: the key pressed (NONE for none) and whether the setup is ready; returns the number of effects */
int ae_test_run_step(struct ae_test_run_machine *machine, enum ae_test_run_key key, int ready);
/* the note box finished: 1 saved, -1 cancelled; returns the number of effects. A saved FAIL records it and moves on; a
cancelled FAIL records nothing and the test is up again (READY). A saved NOTE records nothing yet: the machine waits in
VERDICT for ae_test_run_machine_choose */
int ae_test_run_machine_note_done(struct ae_test_run_machine *machine, int result);
/* the owner's pick after a saved NOTE: PASS, FAIL or SKIP records that verdict with the note and the shot and moves on (FAIL
opens no second box); NOTE = stay: records NOTE and stays on the test. Anything else, or not in VERDICT: ignored (0).
Returns the number of effects */
int ae_test_run_machine_choose(struct ae_test_run_machine *machine, enum ae_test_run_verdict verdict);
/* the test could not be set up (or did not load in time) */
void ae_test_run_machine_block(struct ae_test_run_machine *machine);

#endif
