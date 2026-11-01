/* ae_test_run_test.c: TEST RUN's decision machine (ae_test_run_machine.h): keys while loading, a held key counted
once, FAIL and NOTE through the note box, BACK and RELOAD, SKIP, the summary, the DEV MODE gate. */

#include <stdio.h>
#include <string.h>

#include "ae_test_run_clip.h"
#include "ae_test_run_machine.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_test_run_list list;
static enum ae_test_run_state states[AE_TEST_RUN_TESTS];
static struct ae_test_run_machine machine;

static void make(int tests)
{
	char id[32];
	int index;

	memset(&list, 0, sizeof(list));
	memset(states, 0, sizeof(states));
	list.tests = (short)tests;
	for (index = 0; index < tests; index++)
	{
		snprintf(id, sizeof(id), "t%d", index);
		strcpy(list.test[index].id, id);
	}
}

static int has(enum ae_test_run_effect_kind kind)
{
	int index;

	for (index = 0; index < machine.effects; index++)
	{
		if (machine.effect[index].kind == kind)
			return 1;
	}
	return 0;
}

static int has_record(enum ae_test_run_verdict verdict)
{
	int index;

	for (index = 0; index < machine.effects; index++)
	{
		if (machine.effect[index].kind == AE_TEST_RUN_EFFECT_RECORD && machine.effect[index].verdict == verdict)
			return 1;
	}
	return 0;
}

/* the effects in order, as letters: A apply, S shot, N note box, R record, V video start, K video keep, D video drop,
Z summary, F finish */
static const char *order(void)
{
	static char text[16];
	int index;

	for (index = 0; index < machine.effects; index++)
		text[index] = "?ASNRVKDZF"[machine.effect[index].kind];
	text[machine.effects] = 0;
	return text;
}

static void to_ready(void)
{
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_NONE, 1);
}

int main(void)
{
	/* inert_without_dev_mode */
	make(3);
	CHECK(!ae_test_run_machine_start(&machine, &list, states, 0));
	CHECK(!ae_test_run_machine_active(&machine));
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_PASS, 1) == 0);

	/* nothing pending: not started */
	make(2);
	states[0] = states[1] = AE_TEST_RUN_PASSED;
	CHECK(!ae_test_run_machine_start(&machine, &list, states, 1));

	/* a run plays the pending tests only, from the first */
	make(4);
	states[0] = AE_TEST_RUN_PASSED;
	states[2] = AE_TEST_RUN_PASSED;
	CHECK(ae_test_run_machine_start(&machine, &list, states, 1));
	CHECK(machine.count == 2 && machine.order[0] == 1 && machine.order[1] == 3 && ae_test_run_machine_test(&machine) == 1);
	CHECK(!strcmp(order(), "A"));

	/* RESTART ALL (ae_test_run_start_all): the results are read as empty, so a list that has all passed plays every test,
	from the first */
	make(3);
	{
		struct ae_test_run_record record;
		char passed[1024] = "";
		int at;

		strcpy(list.id, "x");
		for (at = 0; at < 3; at++)
		{
			FILE *file = tmpfile();
			size_t length = strlen(passed);

			memset(&record, 0, sizeof(record));
			strcpy(record.run, "r");
			strcpy(record.list, "x");
			snprintf(record.test, sizeof(record.test), "t%d", at);
			record.verdict = AE_TEST_RUN_PASS;
			CHECK(file && ae_test_run_record_write(file, &record));
			rewind(file);
			length += fread(passed + length, 1, sizeof(passed) - length - 1, file);
			passed[length] = 0;
			fclose(file);
		}
		ae_test_run_states(&list, passed, states);
		CHECK(ae_test_run_pending(&list, states) == 0 && !ae_test_run_machine_start(&machine, &list, states, 1));
		ae_test_run_states(&list, "", states);
		CHECK(ae_test_run_machine_start(&machine, &list, states, 1));
		CHECK(machine.count == 3 && machine.order[0] == 0 && machine.order[2] == 2);
	}

	/* a crash resumes at the first test the crashed run did not record: FAIL, FAIL, then a crash on test 3 (4 tests) */
	make(4);
	{
		struct ae_test_run_record record;
		char text[2048] = "";
		short resume;
		int at;

		strcpy(list.id, "x");
		for (at = 0; at < 2; at++)
		{
			FILE *file = tmpfile();
			size_t length = strlen(text);

			memset(&record, 0, sizeof(record));
			strcpy(record.run, "r1");
			strcpy(record.list, "x");
			snprintf(record.test, sizeof(record.test), "t%d", at);
			record.verdict = AE_TEST_RUN_FAIL;
			CHECK(file && ae_test_run_record_write(file, &record));
			rewind(file);
			length += fread(text + length, 1, sizeof(text) - length - 1, file);
			text[length] = 0;
			fclose(file);
		}
		ae_test_run_states(&list, text, states);
		resume = ae_test_run_resume_index(&list, text);
		CHECK(resume == 2);
		CHECK(ae_test_run_machine_start_at(&machine, &list, states, 1, resume));
		/* (test 3 first, the rest of the pending in list order after it, the two FAILs last) */
		CHECK(machine.count == 4 && machine.order[0] == 2 && machine.order[1] == 3 && machine.order[2] == 0 &&
			machine.order[3] == 1 && ae_test_run_machine_test(&machine) == 2);
		/* no resume point: list order, as _start */
		CHECK(ae_test_run_machine_start_at(&machine, &list, states, 1, -1));
		CHECK(machine.order[0] == 0 && machine.order[1] == 1 && machine.order[2] == 2 && machine.order[3] == 3);
		/* a start past the list is list order too */
		CHECK(ae_test_run_machine_start_at(&machine, &list, states, 1, 9));
		CHECK(machine.order[0] == 0);
	}

	/* keys_ignored_while_loading */
	make(3);
	CHECK(ae_test_run_machine_start(&machine, &list, states, 1));
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_PASS, 0) == 0);
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_FAIL, 0) == 0);
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_RELOAD, 0) == 0);
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_BACK, 0) == 0);
	CHECK(machine.phase == AE_TEST_RUN_PHASE_LOADING && ae_test_run_machine_test(&machine) == 0);

	/* ready: a key on the frame it comes is dropped; a video test starts its recording */
	list.test[0].video = list.test[1].video = 1;
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_PASS, 1) == 1 && has(AE_TEST_RUN_EFFECT_VIDEO_START));
	CHECK(machine.phase == AE_TEST_RUN_PHASE_READY);

	/* held_key_counts_once: PASS records, loads the next; a second PASS before it is ready does nothing */
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_PASS, 1) == 3 && !strcmp(order(), "RDA"));
	/* (a record is for the test that was up, though the machine has moved on to the next) */
	CHECK(machine.effect[0].test == 0 && machine.order[machine.position] == 1);
	CHECK(has_record(AE_TEST_RUN_PASS) && machine.passed == 1 && ae_test_run_machine_test(&machine) == 1);
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_PASS, 0) == 0);
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_PASS, 1) == 1 && machine.phase == AE_TEST_RUN_PHASE_READY);

	/* fail_opens_note_then_next: the shot on the key's frame, the box a frame later, the record when it is done */
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_FAIL, 1) == 2 && !strcmp(order(), "SK"));
	CHECK(machine.phase == AE_TEST_RUN_PHASE_SHOT);
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_PASS, 1) == 1 && !strcmp(order(), "N"));
	CHECK(machine.phase == AE_TEST_RUN_PHASE_NOTE);
	/* (keys while the box is open do nothing) */
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_SKIP, 1) == 0);
	CHECK(ae_test_run_machine_note_done(&machine, 1) == 2 && !strcmp(order(), "RA"));
	CHECK(machine.effect[0].verdict == AE_TEST_RUN_FAIL && machine.effect[0].note_kind == AE_TEST_RUN_NOTE_TYPED);
	CHECK(machine.failed == 1 && ae_test_run_machine_test(&machine) == 2);

	/* a cancelled FAIL box takes the FAIL back: nothing recorded, the same test up again; then a FAIL saved with no
	text records FAIL and moves on */
	to_ready();
	list.test[2].video = 0;
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_FAIL, 1) == 1 && !strcmp(order(), "S"));
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_NONE, 1);
	CHECK(ae_test_run_machine_note_done(&machine, -1) == 0 && machine.phase == AE_TEST_RUN_PHASE_READY &&
		ae_test_run_machine_test(&machine) == 2 && machine.failed == 1);
	list.test[2].video = 1;
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_FAIL, 1) == 2 && !strcmp(order(), "SK"));
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_NONE, 1);
	CHECK(ae_test_run_machine_note_done(&machine, -1) == 1 && !strcmp(order(), "V") && machine.phase == AE_TEST_RUN_PHASE_READY);
	list.test[2].video = 0;
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_FAIL, 1);
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_NONE, 1);
	CHECK(ae_test_run_machine_note_done(&machine, 1) == 2 && machine.effect[0].verdict == AE_TEST_RUN_FAIL &&
		machine.effect[0].note_kind == AE_TEST_RUN_NOTE_TYPED);

	/* the last test done: the summary, any verdict key ends it */
	CHECK(machine.phase == AE_TEST_RUN_PHASE_SUMMARY && !strcmp(order(), "RZ") && machine.failed == 2);
	CHECK(ae_test_run_machine_active(&machine));
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_RELOAD, 0) == 0);
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_SKIP, 0) == 1 && !strcmp(order(), "F"));
	CHECK(!ae_test_run_machine_active(&machine));
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_PASS, 1) == 0);

	/* note_then_stay: NOTE takes the shot, the box, then asks; Esc/B (stay) records NOTE and stays on the same test */
	make(2);
	CHECK(ae_test_run_machine_start(&machine, &list, states, 1));
	to_ready();
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_NOTE, 1) == 1 && !strcmp(order(), "S"));
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_NONE, 1);
	/* (a saved note records nothing yet: the machine asks) */
	CHECK(ae_test_run_machine_note_done(&machine, 1) == 0 && machine.phase == AE_TEST_RUN_PHASE_VERDICT);
	/* (keys and the step do nothing while it asks; a choice that is not a pick is ignored) */
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_PASS, 1) == 0 && machine.phase == AE_TEST_RUN_PHASE_VERDICT);
	CHECK(ae_test_run_machine_choose(&machine, AE_TEST_RUN_NONE) == 0 && machine.phase == AE_TEST_RUN_PHASE_VERDICT);
	CHECK(ae_test_run_machine_choose(&machine, AE_TEST_RUN_NOTE) == 1 && !strcmp(order(), "R") && has_record(AE_TEST_RUN_NOTE));
	CHECK(machine.effect[0].note_kind == AE_TEST_RUN_NOTE_TYPED);
	CHECK(machine.phase == AE_TEST_RUN_PHASE_READY && ae_test_run_machine_test(&machine) == 0 && machine.noted == 1);
	/* choosing when nothing asks does nothing */
	CHECK(ae_test_run_machine_choose(&machine, AE_TEST_RUN_PASS) == 0 && machine.passed == 0);
	/* a cancelled NOTE box records nothing, does not ask, and stays */
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_NOTE, 1);
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_NONE, 1);
	CHECK(ae_test_run_machine_note_done(&machine, -1) == 0 && machine.phase == AE_TEST_RUN_PHASE_READY &&
		machine.noted == 1);

	/* note_then_pass: one PASS record with the note, then the next test */
	to_ready();
	list.test[0].video = 1;
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_NOTE, 1) == 1);
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_NONE, 1);
	ae_test_run_machine_note_done(&machine, 1);
	CHECK(ae_test_run_machine_choose(&machine, AE_TEST_RUN_PASS) == 3 && !strcmp(order(), "RDA") && has_record(AE_TEST_RUN_PASS));
	CHECK(machine.effect[0].note_kind == AE_TEST_RUN_NOTE_TYPED && machine.effect[0].test == 0);
	CHECK(machine.passed == 1 && machine.noted == 1 && ae_test_run_machine_test(&machine) == 1 &&
		machine.phase == AE_TEST_RUN_PHASE_LOADING);

	/* note_then_fail: one FAIL record carrying the note, the clip kept, no second box */
	make(3);
	list.test[0].video = 1;
	CHECK(ae_test_run_machine_start(&machine, &list, states, 1));
	to_ready();
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_NOTE, 1);
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_NONE, 1);
	ae_test_run_machine_note_done(&machine, 1);
	CHECK(ae_test_run_machine_choose(&machine, AE_TEST_RUN_FAIL) == 3 && !strcmp(order(), "KRA") &&
		machine.effect[1].verdict == AE_TEST_RUN_FAIL && machine.effect[1].note_kind == AE_TEST_RUN_NOTE_TYPED);
	CHECK(!has(AE_TEST_RUN_EFFECT_SHOT) && !has(AE_TEST_RUN_EFFECT_NOTE_OPEN) && machine.failed == 1 && machine.noted == 0);
	CHECK(machine.phase == AE_TEST_RUN_PHASE_LOADING && ae_test_run_machine_test(&machine) == 1);
	/* (the next frames never open a box for it) */
	to_ready();
	CHECK(machine.phase == AE_TEST_RUN_PHASE_READY);

	/* note_then_continue: one SKIP record with the note, the clip dropped, the next test */
	make(2);
	list.test[0].video = 1;
	CHECK(ae_test_run_machine_start(&machine, &list, states, 1));
	to_ready();
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_NOTE, 1);
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_NONE, 1);
	ae_test_run_machine_note_done(&machine, 1);
	CHECK(ae_test_run_machine_choose(&machine, AE_TEST_RUN_SKIP) == 3 && !strcmp(order(), "RDA") && has_record(AE_TEST_RUN_SKIP));
	CHECK(machine.effect[0].note_kind == AE_TEST_RUN_NOTE_TYPED && machine.skipped == 1 && machine.noted == 0 &&
		ae_test_run_machine_test(&machine) == 1);
	/* on the last test, a pick ends in the summary */
	to_ready();
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_NOTE, 1);
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_NONE, 1);
	ae_test_run_machine_note_done(&machine, 1);
	CHECK(ae_test_run_machine_choose(&machine, AE_TEST_RUN_SKIP) == 2 && !strcmp(order(), "RZ") &&
		machine.phase == AE_TEST_RUN_PHASE_SUMMARY);

	/* a FAIL's own box is unchanged: it records at once, no asking */
	make(2);
	CHECK(ae_test_run_machine_start(&machine, &list, states, 1));
	to_ready();
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_FAIL, 1);
	ae_test_run_step(&machine, AE_TEST_RUN_KEY_NONE, 1);
	CHECK(ae_test_run_machine_note_done(&machine, 1) == 2 && machine.phase == AE_TEST_RUN_PHASE_LOADING);

	/* re-enter the state the following checks use */
	make(2);
	CHECK(ae_test_run_machine_start(&machine, &list, states, 1));
	to_ready();

	/* back_and_reload: RELOAD applies the same test again; BACK goes to the previous test of the run (not before the
	first) */
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_BACK, 1) == 0 && ae_test_run_machine_test(&machine) == 0);
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_RELOAD, 1) == 1 && !strcmp(order(), "A") &&
		machine.phase == AE_TEST_RUN_PHASE_LOADING);
	to_ready();
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_PASS, 1) == 2 && ae_test_run_machine_test(&machine) == 1);
	to_ready();
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_BACK, 1) == 1 && !strcmp(order(), "A") &&
		ae_test_run_machine_test(&machine) == 0 && machine.phase == AE_TEST_RUN_PHASE_LOADING);
	/* (while loading, BACK and RELOAD are dropped too) */
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_BACK, 0) == 0);

	/* skip_records_skip */
	to_ready();
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_SKIP, 1) >= 2 && has_record(AE_TEST_RUN_SKIP) && machine.skipped == 1);

	/* a test that did not load (blocked): PASS and SKIP record SKIP, FAIL and NOTE record FAIL, the error is the note,
	no box; RELOAD and BACK work */
	make(5);
	CHECK(ae_test_run_machine_start(&machine, &list, states, 1));
	ae_test_run_machine_block(&machine);
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_NONE, 1) == 0 && machine.phase == AE_TEST_RUN_PHASE_LOADING);
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_RELOAD, 0) == 1 && !strcmp(order(), "A") && !machine.blocked);
	ae_test_run_machine_block(&machine);
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_PASS, 0) == 2 && !strcmp(order(), "RA") &&
		machine.effect[0].verdict == AE_TEST_RUN_SKIP && machine.effect[0].note_kind == AE_TEST_RUN_NOTE_ERROR &&
		machine.effect[0].test == 0);
	ae_test_run_machine_block(&machine);
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_FAIL, 0) == 2 && !strcmp(order(), "RA") &&
		machine.effect[0].verdict == AE_TEST_RUN_FAIL && machine.effect[0].note_kind == AE_TEST_RUN_NOTE_ERROR);
	ae_test_run_machine_block(&machine);
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_NOTE, 0) == 2 && !strcmp(order(), "RA") &&
		machine.effect[0].verdict == AE_TEST_RUN_FAIL && machine.phase == AE_TEST_RUN_PHASE_LOADING);
	ae_test_run_machine_block(&machine);
	CHECK(ae_test_run_step(&machine, AE_TEST_RUN_KEY_SKIP, 0) == 2 && has_record(AE_TEST_RUN_SKIP));
	CHECK(machine.phase == AE_TEST_RUN_PHASE_LOADING && ae_test_run_machine_test(&machine) == 4);

	/* the recording's decisions: a FAILed video test's clip survives the next video test's PASS while the writer is
	still saving */
	{
		struct ae_test_run_clip clip = { 0 };
		int actions;

		ae_test_run_clip_clear(&clip);
		ae_test_run_clip_want(&clip, 0);
		CHECK(ae_test_run_clip_service(&clip, 0, 100) == AE_TEST_RUN_CLIP_START && clip.active && clip.test == 0);
		/* (the recorder is seen going, then test 0 FAILs: kept, stopped) */
		CHECK(ae_test_run_clip_service(&clip, 1, 200) == 0);
		CHECK(!ae_test_run_clip_keep(&clip, 0, clip.serial));
		CHECK(ae_test_run_clip_service(&clip, 1, 300) == AE_TEST_RUN_CLIP_STOP);
		/* (the next test is set up and wants a recording; the writer is still saving: state 2) */
		ae_test_run_clip_leave(&clip, 1);
		ae_test_run_clip_want(&clip, 1);
		CHECK(ae_test_run_clip_service(&clip, 2, 400) == 0 && clip.want);
		/* (test 1 is PASSed before the writer finishes: its drop touches nothing of test 0's clip) */
		ae_test_run_clip_drop(&clip, 1, clip.serial);
		CHECK(!clip.want && clip.keep && !clip.delete_after);
		actions = ae_test_run_clip_service(&clip, 0, 500);
		CHECK(!(actions & AE_TEST_RUN_CLIP_DELETE) && !clip.active);
		CHECK(!(actions & AE_TEST_RUN_CLIP_START));

		/* a PASSed test's clip is deleted once the writer is done; a NOTEd one is kept */
		ae_test_run_clip_want(&clip, 2);
		CHECK(ae_test_run_clip_service(&clip, 0, 600) == AE_TEST_RUN_CLIP_START);
		ae_test_run_clip_service(&clip, 1, 700);
		ae_test_run_clip_drop(&clip, 2, clip.serial);
		CHECK(ae_test_run_clip_service(&clip, 1, 800) == AE_TEST_RUN_CLIP_STOP);
		CHECK(ae_test_run_clip_service(&clip, 2, 900) == 0);
		CHECK(ae_test_run_clip_service(&clip, 0, 1000) == AE_TEST_RUN_CLIP_DELETE);
		ae_test_run_clip_want(&clip, 3);
		ae_test_run_clip_service(&clip, 0, 1100);
		ae_test_run_clip_service(&clip, 1, 1200);
		ae_test_run_clip_mark_kept(&clip, 3, clip.serial);
		ae_test_run_clip_leave(&clip, 4); /* (BACK or the next test while it still records) */
		ae_test_run_clip_service(&clip, 1, 1300);
		CHECK(clip.stop_wanted && !clip.delete_after);
		ae_test_run_clip_service(&clip, 0, 1400);
		CHECK(!clip.active);
		/* (a drop for a test that does not own the recording changes nothing) */
		ae_test_run_clip_want(&clip, 5);
		ae_test_run_clip_service(&clip, 0, 1500);
		ae_test_run_clip_service(&clip, 1, 1600);
		ae_test_run_clip_drop(&clip, 6, clip.serial);
		CHECK(!clip.stop_wanted && !clip.delete_after);
		/* (a recording never seen is given up, its file removed, and says which test has no clip) */
		ae_test_run_clip_leave(&clip, 5);
		ae_test_run_clip_clear(&clip);
		ae_test_run_clip_want(&clip, 7);
		ae_test_run_clip_service(&clip, 0, 2000);
		actions = ae_test_run_clip_service(&clip, 0, 2000 + AE_TEST_RUN_CLIP_START_MS + 1);
		CHECK((actions & AE_TEST_RUN_CLIP_START_FAILED) && (actions & AE_TEST_RUN_CLIP_DELETE) && clip.ended_test == 7);
		/* BACK to a test whose earlier clip was PASSed and is still saving: a FAIL of the new showing does not keep the
		old clip (it is deleted, as its PASS said), and the new showing has no clip */
		ae_test_run_clip_clear(&clip);
		ae_test_run_clip_want(&clip, 2);
		CHECK(ae_test_run_clip_service(&clip, 0, 3000) == AE_TEST_RUN_CLIP_START);
		{
			unsigned old_serial = clip.serial;

			ae_test_run_clip_service(&clip, 1, 3100);
			ae_test_run_clip_drop(&clip, 2, old_serial);
			CHECK(ae_test_run_clip_service(&clip, 1, 3200) == AE_TEST_RUN_CLIP_STOP);
			/* (test 3, then BACK to test 2: its new showing wants a recording, has no serial, the writer still saves) */
			ae_test_run_clip_leave(&clip, 3);
			ae_test_run_clip_leave(&clip, 2);
			ae_test_run_clip_want(&clip, 2);
			CHECK(ae_test_run_clip_service(&clip, 2, 3300) == 0 && clip.want);
			CHECK(ae_test_run_clip_keep(&clip, 2, 0) == 1);
			CHECK(!clip.keep && clip.delete_after && clip.serial == old_serial);
			CHECK(ae_test_run_clip_service(&clip, 0, 3400) == AE_TEST_RUN_CLIP_DELETE);
			/* (a later recording of the test has a new serial) */
			ae_test_run_clip_want(&clip, 2);
			CHECK(ae_test_run_clip_service(&clip, 0, 3500) == AE_TEST_RUN_CLIP_START && clip.serial != old_serial);
		}
		/* (a recorder still busy at the verdict: keep says the test had none; no ffmpeg is said once) */
		ae_test_run_clip_clear(&clip);
		ae_test_run_clip_want(&clip, 8);
		CHECK(ae_test_run_clip_service(&clip, 2, 10) == 0 && ae_test_run_clip_keep(&clip, 8, 0));
		ae_test_run_clip_want(&clip, 9);
		CHECK(ae_test_run_clip_service(&clip, -1, 20) == AE_TEST_RUN_CLIP_NO_FFMPEG && !clip.want);
	}

	/* an id as the list parser takes it */
	CHECK(ae_test_run_list_id_ok("a-b0") && !ae_test_run_list_id_ok("A") && !ae_test_run_list_id_ok("") &&
		!ae_test_run_list_id_ok("a_b") && !ae_test_run_list_id_ok("a:b") &&
		!ae_test_run_list_id_ok("0123456789012345678901234567890123") && ae_test_run_list_id_ok("0123456789012345678901234567890"));

	printf(failures ? "FAILED\n" : "ok\n");
	return failures != 0;
}
