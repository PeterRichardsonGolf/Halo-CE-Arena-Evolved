/*
AE_TEST_RUN.H
TEST RUN, the in-game test runner for the owner (DEV MODE only). One running game plays a whole test list: for each
pending test it sets the scenario up (ae_test_run_setup.h), takes the owner's verdict (ae_test_run_input.h), captures
the evidence (a screenshot of a FAIL or NOTE, a recording of a video test, kept only for a FAIL or NOTE), writes the
record (ae_test_run_results.h) and loads the next test, with no restart. The decisions are ae_test_run_machine.c's;
this file carries them out in the engine.

Files: the list is <data root>/test-runs/<id>.txt, the results <config folder>/test-runs/<id>.results.jsonl (appended
to; a run reads it whole when it starts, to know what is pending), shots test-runs/shots/<list>-<test>-<run>-<seq>.png and
clips test-runs/clips/<list>-<test>-<run>-<seq>.mp4 (<run>: the run's time with "-" for ":"; <seq>: a per-run
sequence number; ids cleaned to [A-Za-z0-9_.-]). Each record logs "test run: <list> <test> <VERDICT>".

Settings: debug.test_run (HALO_TEST_RUN) starts that list once the main menu has loaded; debug.test_run_auto
(HALO_TEST_RUN_AUTO), e.g. "PASS:6,FAIL:6,NOTE:6,SKIP:6", fires those verdicts N seconds after a test is ready,
cycling, with the note text "auto" (the automated tests).
*/
#ifndef AE_TEST_RUN_H
#define AE_TEST_RUN_H

#include <stddef.h>

/* loads <data root>/test-runs/<id>.txt and its results and starts at the first pending test: 0 = not started (why is
logged: DEV MODE off, no such list, nothing pending, a run already on) */
int ae_test_run_start(const char *list_id);
/* the same, but every test is played from the first, whatever the results say (the TEST RUN screen's RESTART ALL). The
results file is kept and appended to: nothing is erased. The same DEV MODE gate */
int ae_test_run_start_all(const char *list_id);
/* a run is on */
int ae_test_run_active(void);
/* the list's file under the data root ("test-runs/<id>.txt", for the game's wrapped fopen) and its results file (a host
path under the config folder): 1 = written, 0 = not a list id or no room */
int ae_test_run_list_path(const char *list_id, char *path, size_t size);
int ae_test_run_results_path(const char *list_id, char *path, size_t size);
/* a file's whole text, malloc'd (the caller frees it; "" when it cannot be read; NULL only when out of memory) */
char *ae_test_run_read_results(const char *path);
/* once a frame (main.c, next to network_test_update): the keys, the setup, the records. Nothing while no run is on,
except starting debug.test_run at boot */
void ae_test_run_update(void);

/* the view's video_state (for a test with video; 0 for any other) */
enum
{
	AE_TEST_RUN_VIDEO_NO_FFMPEG = -1, /* NO VIDEO (FFMPEG) */
	AE_TEST_RUN_VIDEO_NONE = 0,       /* no video, or not started yet */
	AE_TEST_RUN_VIDEO_RECORDING = 1,
	AE_TEST_RUN_VIDEO_SAVING = 2,     /* the clip is being saved */
	AE_TEST_RUN_VIDEO_WAITING = 3,    /* the recorder is busy with an earlier clip; this one starts when it is free */
	AE_TEST_RUN_VIDEO_NO_CLIP = 4     /* NO CLIP (RECORDER BUSY): the verdict came before it could start, or it did not start */
};

/* what the card (ae_test_run_view.c) draws */
struct ae_test_run_view
{
	int active, loading, note_open, verdict_open, summary, hidden, card_bottom;
	int video_state; /* AE_TEST_RUN_VIDEO_*, below */
	short number, count; /* this test's place in the run, and the run's size */
	char tag[96], title[80], check[3][64], error[96], note[256];
	char list[64];  /* the list's title (the feature the card's header names) */
	short choice;   /* verdict_open: the chip the cursor is on (0 PASS, 1 FAIL, 2 CONTINUE) */
	int note_fail;  /* the note box is for a FAIL (else a NOTE) */
	int shot_saved; /* the verdict's screenshot was asked for */
	short passed, failed, skipped, noted;
};
void ae_test_run_view(struct ae_test_run_view *out);

/* the NOTE box has the keyboard (ae_test_run_note.h) */
int ae_test_run_typing(void);

#endif
