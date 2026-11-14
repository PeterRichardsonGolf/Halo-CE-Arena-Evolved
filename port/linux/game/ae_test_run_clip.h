/*
AE_TEST_RUN_CLIP.H
TEST RUN's recording decisions (DEV MODE), pure: no engine calls. A recording belongs to the test it was started for, and
its keep and delete decisions belong to it alone: a later test's verdict, or the setup of the next test, never changes
what happens to an earlier test's clip (a FAILed test's clip is named by its record and must survive).
The runner (ae_test_run.c) tells this unit what happened and carries out the actions it returns.
*/
#ifndef AE_TEST_RUN_CLIP_H
#define AE_TEST_RUN_CLIP_H

struct ae_test_run_clip
{
	short active;      /* a recording was asked for and is not finished */
	short test;        /* the list index it belongs to */
	unsigned serial;   /* which recording of that test (1 up, never reused): a test shown again has a new one */
	unsigned next_serial;
	short keep, delete_after, stop_wanted, stop_sent, seen;
	short ended_test;  /* the test of the recording that last finished (for START_FAILED) */
	short want, want_test; /* a test is ready and wants a recording started */
	unsigned long asked_at;
};

/* what ae_test_run_clip_service asks the runner to do (bits) */
enum
{
	AE_TEST_RUN_CLIP_START = 1,        /* request the recording for clip->test */
	AE_TEST_RUN_CLIP_STOP = 2,         /* request the stop */
	AE_TEST_RUN_CLIP_DELETE = 4,       /* delete the finished recording's file */
	AE_TEST_RUN_CLIP_START_FAILED = 8, /* the recording was not seen to start: its test has no clip */
	AE_TEST_RUN_CLIP_NO_FFMPEG = 16    /* want_test cannot be recorded: no ffmpeg */
};

#define AE_TEST_RUN_CLIP_START_MS 5000

void ae_test_run_clip_clear(struct ae_test_run_clip *clip);
/* the test is ready and has video: a recording of it wanted (the one already going for it is enough) */
void ae_test_run_clip_want(struct ae_test_run_clip *clip, short test);
/* A recording is its test and its serial (clip->serial when it started, 0 for a visit that has none): the calls below
name both, so a test's earlier recording, still saving when it is shown again, is never taken for the new visit's. */
/* FAIL: the test's clip is kept; 1 when it is still waiting for a recorder that was never started (no clip) */
int ae_test_run_clip_keep(struct ae_test_run_clip *clip, short test, unsigned serial);
/* NOTE: the test goes on recording, and its clip is kept whatever follows */
void ae_test_run_clip_mark_kept(struct ae_test_run_clip *clip, short test, unsigned serial);
/* PASS or SKIP: the test's recording stops, and its clip is deleted unless kept */
void ae_test_run_clip_drop(struct ae_test_run_clip *clip, short test, unsigned serial);
/* another test is set up: a recording that is not its own stops (deleted unless kept), a wish for another test goes */
void ae_test_run_clip_leave(struct ae_test_run_clip *clip, short test);
/* once a frame: state is capture_recording_state(); returns the actions */
int ae_test_run_clip_service(struct ae_test_run_clip *clip, int state, unsigned long now);

#endif
