/* ae_test_run_machine.c: Arena Evolved TEST RUN's decision machine (see ae_test_run_machine.h). */

#include <string.h>

#include "ae_test_run_machine.h"

static void effect(struct ae_test_run_machine *machine, enum ae_test_run_effect_kind kind,
	enum ae_test_run_verdict verdict, enum ae_test_run_note_kind note_kind)
{
	if (machine->effects < AE_TEST_RUN_EFFECTS)
	{
		machine->effect[machine->effects].kind = kind;
		machine->effect[machine->effects].verdict = verdict;
		machine->effect[machine->effects].note_kind = note_kind;
		machine->effect[machine->effects].test = machine->order[machine->position];
		machine->effects++;
	}
}

static struct ae_test_run_test const *current(struct ae_test_run_machine const *machine)
{
	return &machine->list->test[machine->order[machine->position]];
}

static void apply(struct ae_test_run_machine *machine)
{
	machine->phase = AE_TEST_RUN_PHASE_LOADING;
	machine->blocked = 0;
	effect(machine, AE_TEST_RUN_EFFECT_APPLY, AE_TEST_RUN_NONE, AE_TEST_RUN_NOTE_NONE);
}

/* the test is done: the next one, or the summary */
static void advance(struct ae_test_run_machine *machine)
{
	if (++machine->position >= machine->count)
	{
		machine->position = machine->count - 1;
		machine->phase = AE_TEST_RUN_PHASE_SUMMARY;
		effect(machine, AE_TEST_RUN_EFFECT_SUMMARY, AE_TEST_RUN_NONE, AE_TEST_RUN_NOTE_NONE);
	}
	else
		apply(machine);
}

static void record(struct ae_test_run_machine *machine, enum ae_test_run_verdict verdict,
	enum ae_test_run_note_kind note_kind)
{
	effect(machine, AE_TEST_RUN_EFFECT_RECORD, verdict, note_kind);
	switch (verdict)
	{
		case AE_TEST_RUN_PASS: machine->passed++; break;
		case AE_TEST_RUN_FAIL: machine->failed++; break;
		case AE_TEST_RUN_SKIP: machine->skipped++; break;
		case AE_TEST_RUN_NOTE: machine->noted++; break;
		default: break;
	}
}

int ae_test_run_machine_start(struct ae_test_run_machine *machine, struct ae_test_run_list const *list,
	enum ae_test_run_state const *states, int dev_mode)
{
	return ae_test_run_machine_start_at(machine, list, states, dev_mode, -1);
}

int ae_test_run_machine_start_at(struct ae_test_run_machine *machine, struct ae_test_run_list const *list,
	enum ae_test_run_state const *states, int dev_mode, short first)
{
	short index;

	memset(machine, 0, sizeof(*machine));
	if (!dev_mode || !list || !ae_test_run_pending(list, states))
		return 0;
	machine->list = list;
	if (first < 0 || first >= list->tests)
		first = 0;
	for (index = first; index < list->tests; index++)
	{
		if (states[index] != AE_TEST_RUN_PASSED)
			machine->order[machine->count++] = index;
	}
	for (index = 0; index < first; index++)
	{
		if (states[index] != AE_TEST_RUN_PASSED)
			machine->order[machine->count++] = index;
	}
	apply(machine);
	return 1;
}

int ae_test_run_machine_active(struct ae_test_run_machine const *machine)
{
	return machine->phase != AE_TEST_RUN_PHASE_IDLE;
}

short ae_test_run_machine_test(struct ae_test_run_machine const *machine)
{
	return machine->order[machine->position];
}

void ae_test_run_machine_block(struct ae_test_run_machine *machine)
{
	if (machine->phase == AE_TEST_RUN_PHASE_LOADING)
		machine->blocked = 1;
}

/* the verdict keys PASS, FAIL, NOTE, SKIP; RELOAD and BACK are the others */
static int verdict_key(enum ae_test_run_key key)
{
	return key >= AE_TEST_RUN_KEY_PASS && key <= AE_TEST_RUN_KEY_SKIP;
}

/* a key on a test that is up (READY) or blocked (LOADING) */
static void act(struct ae_test_run_machine *machine, enum ae_test_run_key key)
{
	int blocked = machine->blocked;
	struct ae_test_run_test const *test = current(machine);

	/* (a test that did not load has nothing to pass or note: PASS and SKIP record SKIP, FAIL and NOTE record FAIL, the
	error is the note, no box and no shot) */
	if (blocked && verdict_key(key))
	{
		record(machine, key == AE_TEST_RUN_KEY_PASS || key == AE_TEST_RUN_KEY_SKIP ? AE_TEST_RUN_SKIP : AE_TEST_RUN_FAIL,
			AE_TEST_RUN_NOTE_ERROR);
		advance(machine);
		return;
	}
	switch (key)
	{
		case AE_TEST_RUN_KEY_PASS:
			record(machine, AE_TEST_RUN_PASS, AE_TEST_RUN_NOTE_NONE);
			if (test->video)
				effect(machine, AE_TEST_RUN_EFFECT_VIDEO_DROP, AE_TEST_RUN_NONE, AE_TEST_RUN_NOTE_NONE);
			advance(machine);
			break;
		case AE_TEST_RUN_KEY_SKIP:
			record(machine, AE_TEST_RUN_SKIP, AE_TEST_RUN_NOTE_NONE);
			if (test->video)
				effect(machine, AE_TEST_RUN_EFFECT_VIDEO_DROP, AE_TEST_RUN_NONE, AE_TEST_RUN_NOTE_NONE);
			advance(machine);
			break;
		case AE_TEST_RUN_KEY_FAIL:
		case AE_TEST_RUN_KEY_NOTE:
			machine->note_for = (short)(key == AE_TEST_RUN_KEY_FAIL ? AE_TEST_RUN_FAIL : AE_TEST_RUN_NOTE);
			effect(machine, AE_TEST_RUN_EFFECT_SHOT, AE_TEST_RUN_NONE, AE_TEST_RUN_NOTE_NONE);
			if (key == AE_TEST_RUN_KEY_FAIL && test->video)
				effect(machine, AE_TEST_RUN_EFFECT_VIDEO_KEEP, AE_TEST_RUN_NONE, AE_TEST_RUN_NOTE_NONE);
			machine->phase = AE_TEST_RUN_PHASE_SHOT;
			break;
		case AE_TEST_RUN_KEY_RELOAD:
			apply(machine);
			break;
		case AE_TEST_RUN_KEY_BACK:
			if (machine->position > 0)
			{
				machine->position--;
				apply(machine);
			}
			break;
		default:
			break;
	}
}

int ae_test_run_step(struct ae_test_run_machine *machine, enum ae_test_run_key key, int ready)
{
	machine->effects = 0;
	switch (machine->phase)
	{
		case AE_TEST_RUN_PHASE_LOADING:
			if (machine->blocked)
				act(machine, key);
			else if (ready)
			{
				/* (a key on the frame it comes ready is dropped) */
				machine->phase = AE_TEST_RUN_PHASE_READY;
				if (current(machine)->video)
					effect(machine, AE_TEST_RUN_EFFECT_VIDEO_START, AE_TEST_RUN_NONE, AE_TEST_RUN_NOTE_NONE);
			}
			break;
		case AE_TEST_RUN_PHASE_READY:
			act(machine, key);
			break;
		case AE_TEST_RUN_PHASE_SHOT:
			machine->phase = AE_TEST_RUN_PHASE_NOTE;
			effect(machine, AE_TEST_RUN_EFFECT_NOTE_OPEN, AE_TEST_RUN_NONE, AE_TEST_RUN_NOTE_NONE);
			break;
		case AE_TEST_RUN_PHASE_SUMMARY:
			if (verdict_key(key))
			{
				machine->phase = AE_TEST_RUN_PHASE_IDLE;
				effect(machine, AE_TEST_RUN_EFFECT_FINISH, AE_TEST_RUN_NONE, AE_TEST_RUN_NOTE_NONE);
			}
			break;
		default:
			break;
	}
	return machine->effects;
}

int ae_test_run_machine_note_done(struct ae_test_run_machine *machine, int result)
{
	machine->effects = 0;
	if (machine->phase != AE_TEST_RUN_PHASE_NOTE)
		return 0;
	if (machine->note_for == AE_TEST_RUN_FAIL && result > 0)
	{
		record(machine, AE_TEST_RUN_FAIL, AE_TEST_RUN_NOTE_TYPED);
		advance(machine);
	}
	else if (machine->note_for == AE_TEST_RUN_FAIL)
	{
		/* (a cancelled FAIL takes the FAIL back, as the box's "Back to the test" says (owner 2026-10-26: a FAIL pressed by
		mistake): nothing recorded, the test up again, its recording started again, the kept clip left as it is) */
		machine->phase = AE_TEST_RUN_PHASE_READY;
		if (current(machine)->video)
			effect(machine, AE_TEST_RUN_EFFECT_VIDEO_START, AE_TEST_RUN_NONE, AE_TEST_RUN_NOTE_NONE);
	}
	else if (result > 0)
		/* (a saved note: PASS, FAIL or CONTINUE comes next, ae_test_run_machine_choose) */
		machine->phase = AE_TEST_RUN_PHASE_VERDICT;
	else
		machine->phase = AE_TEST_RUN_PHASE_READY;
	return machine->effects;
}

int ae_test_run_machine_choose(struct ae_test_run_machine *machine, enum ae_test_run_verdict verdict)
{
	struct ae_test_run_test const *test = current(machine);

	machine->effects = 0;
	if (machine->phase != AE_TEST_RUN_PHASE_VERDICT)
		return 0;
	switch (verdict)
	{
		case AE_TEST_RUN_PASS:
		case AE_TEST_RUN_SKIP:
			record(machine, verdict, AE_TEST_RUN_NOTE_TYPED);
			if (test->video)
				effect(machine, AE_TEST_RUN_EFFECT_VIDEO_DROP, AE_TEST_RUN_NONE, AE_TEST_RUN_NOTE_NONE);
			advance(machine);
			break;
		case AE_TEST_RUN_FAIL:
			if (test->video)
				effect(machine, AE_TEST_RUN_EFFECT_VIDEO_KEEP, AE_TEST_RUN_NONE, AE_TEST_RUN_NOTE_NONE);
			record(machine, AE_TEST_RUN_FAIL, AE_TEST_RUN_NOTE_TYPED);
			advance(machine);
			break;
		case AE_TEST_RUN_NOTE:
			machine->phase = AE_TEST_RUN_PHASE_READY;
			record(machine, AE_TEST_RUN_NOTE, AE_TEST_RUN_NOTE_TYPED);
			break;
		default:
			break;
	}
	return machine->effects;
}
