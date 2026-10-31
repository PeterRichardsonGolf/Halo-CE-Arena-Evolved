/* ae_test_run.c: Arena Evolved TEST RUN, the runner (see ae_test_run.h). The decisions are the machine's
(ae_test_run_machine.c); this file reads the keys, carries out the machine's effects in the engine, and keeps the
results file, the evidence and the clock. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cseries.h"
#include "../src/capture.h"
#include "../src/capture_child.h"
#include "../src/build_identity.h"
#include "../src/port_config.h"
#include "ae_dev_mode.h"
#include "ae_hooks.h"
#include "ae_test_run.h"
#include "ae_test_run_clip.h"
#include "ae_test_run_input.h"
#include "ae_test_run_machine.h"
#include "ae_test_run_note.h"
#include "ae_test_run_results.h"
#include "ae_test_run_setup.h"
#include "ae_screen_test_run.h"
#include "ae_ui.h"

/* (stdio.h's fopen and remove take Xbox paths under the data root, as the port's game code uses them: the results file,
the clips and the config folder are host paths, so these two are the C library's. The list is opened by the list unit
(ae_test_run_list.c), with the wrapped fopen, by its path under the data root) */
#undef fopen
#undef remove

/* main.c's, platform.h's, ui_widget.c's, the posix layer's */
void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
const char *platform_data_root(void);
void main_goto_main_menu(void);
boolean main_menu_is_active(void);
int posix_make_directory(const char *path);

/* a test that is not ready this long after its setup was applied did not load */
#define LOAD_TIMEOUT_MS 120000
/* a recording asked for that is not seen this long after the request did not start */
#define AUTO_MAXIMUM 8
#define PATH_BYTES 1024

static struct ae_test_run_list list; /* (about 225 KB: static) */
static enum ae_test_run_state states[AE_TEST_RUN_TESTS];
static struct ae_test_run_machine machine;
static FILE *results_file;
static char run_time[24]; /* "2026-10-25T14:03:11" */
static char run_stamp[24]; /* the same with "-" for ":", for file names */
static char note[256];
static char error[96];
static unsigned long applied_at;
static int booted;
/* the run ended with its summary: the TEST RUN screen opens once the main menu is up (and since when) */
static int screen_pending;
static unsigned long screen_since;

/* the recording's decisions (ae_test_run_clip.c) and the file the recording in progress writes */
static struct ae_test_run_clip clip;
static char clip_path[160];
/* the evidence of the test up: its shot, its clip (what its record names), and why it has none */
static struct
{
	char shot[160];
	char current[160];
	int test;                  /* the list index it belongs to */
	unsigned serial;           /* the serial of the recording started for this showing of the test, 0 none */
	int no_ffmpeg, no_clip;    /* no recorder at all; a recording that was never started or seen */
} evidence;
static int sequence;       /* numbers the run's evidence files, so a test shown again does not replace its earlier ones */
static int save_failed;    /* a record could not be written: the card says so for the rest of the run */

/* debug.test_run_auto */
static struct { enum ae_test_run_key key; int seconds; } automatic[AUTO_MAXIMUM];
static int automatics, automatic_next, automatic_signature;
static unsigned long automatic_since;

/* ---------- names and paths */

static void clean_name(char *to, size_t size, const char *from)
{
	size_t at;

	for (at = 0; from[at] && at + 1 < size; at++)
	{
		char c = from[at];

		to[at] = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
			c == '-' ? c : '_';
	}
	to[at] = 0;
}

/* <config folder>relative */
static void in_config(char *path, size_t size, const char *relative)
{
	char folder[PATH_BYTES];

	config_folder(folder, sizeof(folder));
	snprintf(path, size, "%s%s", folder, relative);
}

static void evidence_name(char *relative, size_t size, const char *folder, const char *extension, const char *test)
{
	char list_id[40], test_id[40];

	clean_name(list_id, sizeof(list_id), list.id);
	clean_name(test_id, sizeof(test_id), test);
	snprintf(relative, size, "test-runs/%s/%s-%s-%s-%d.%s", folder, list_id, test_id, run_stamp, ++sequence, extension);
}

static int is_part(const char *name)
{
	size_t length = strlen(name);

	return length > 5 && !strcmp(name + length - 5, ".part");
}

/* ---------- the clip */

static void clip_forget(void)
{
	memset(&evidence, 0, sizeof(evidence));
	evidence.test = -1;
	/* (a recording still going from an earlier run is finished by the service) */
	if (!clip.active)
		ae_test_run_clip_clear(&clip);
	else
		clip.want = 0;
}

/* the clip's service, once a frame and after the run ends: carries out what the decisions ask; never waits */
static void clip_service(void)
{
	int actions = ae_test_run_clip_service(&clip, capture_recording_state(), system_milliseconds());

	if (actions & AE_TEST_RUN_CLIP_STOP)
		capture_request_recording_stop();
	if (actions & AE_TEST_RUN_CLIP_DELETE)
	{
		char path[PATH_BYTES];

		in_config(path, sizeof(path), clip_path);
		remove(path);
	}
	if (actions & AE_TEST_RUN_CLIP_START_FAILED)
	{
		platform_log("test run: the recording %s did not start", clip_path);
		if (clip.ended_test == evidence.test)
			evidence.no_clip = 1;
	}
	if (actions & AE_TEST_RUN_CLIP_NO_FFMPEG)
		evidence.no_ffmpeg = 1;
	if (actions & AE_TEST_RUN_CLIP_START)
	{
		evidence_name(clip_path, sizeof(clip_path), "clips", "mp4", list.test[clip.test].id);
		capture_request_recording_as(clip_path);
		if (clip.test == evidence.test)
		{
			snprintf(evidence.current, sizeof(evidence.current), "%s", clip_path);
			evidence.serial = clip.serial;
		}
	}
}

/* ---------- records */

static void write_record(short test_index, enum ae_test_run_verdict verdict, enum ae_test_run_note_kind note_kind)
{
	static struct ae_test_run_record record;
	struct ae_test_run_test const *test = &list.test[test_index];
	static const char *const name[] = { "", "PASS", "FAIL", "SKIP", "NOTE" };

	memset(&record, 0, sizeof(record));
	snprintf(record.run, sizeof(record.run), "%s", run_time);
	snprintf(record.list, sizeof(record.list), "%s", list.id);
	snprintf(record.test, sizeof(record.test), "%s", test->id);
	snprintf(record.note, sizeof(record.note), "%s", note_kind == AE_TEST_RUN_NOTE_TYPED ? note :
		note_kind == AE_TEST_RUN_NOTE_ERROR ? error : "");
	/* (a PASS or SKIP that came after a note carries the note's shot too) */
	if (verdict == AE_TEST_RUN_FAIL || verdict == AE_TEST_RUN_NOTE || evidence.shot[0])
		snprintf(record.shot, sizeof(record.shot), "%s", evidence.shot);
	if (verdict == AE_TEST_RUN_FAIL || verdict == AE_TEST_RUN_NOTE)
	{
		if (test->video)
			snprintf(record.video, sizeof(record.video), "%s", evidence.no_ffmpeg ? "no ffmpeg" :
				evidence.no_clip ? "no clip (recorder busy)" : evidence.current);
	}
	snprintf(record.build, sizeof(record.build), "%s", build_identity_commit());
	{
		time_t now = time(NULL);

		strftime(record.time, sizeof(record.time), "%Y-%m-%dT%H:%M:%S", localtime(&now));
	}
	record.revision = list.revision;
	record.verdict = verdict;
	if (!results_file || !ae_test_run_record_write(results_file, &record))
	{
		save_failed = 1;
		platform_log("test run: the record of %s could not be written", test->id);
	}
	platform_log("test run: %s %s %s", list.id, test->id, name[verdict]);
}

/* ---------- effects */

static void apply_current(void)
{
	short index = ae_test_run_machine_test(&machine);
	struct ae_test_run_test const *test = &list.test[index];

	error[0] = 0;
	evidence.shot[0] = 0;
	/* (a recording that is not this test's stops, its clip deleted unless that test kept it) */
	ae_test_run_clip_leave(&clip, index);
	if (evidence.test != index)
	{
		evidence.test = index;
		evidence.current[0] = 0;
		evidence.serial = 0;
		evidence.no_ffmpeg = evidence.no_clip = 0;
	}
	applied_at = system_milliseconds();
	if (test->error[0])
	{
		snprintf(error, sizeof(error), "%s", test->error);
		ae_test_run_machine_block(&machine);
	}
	else if (!ae_test_run_setup_apply(test))
	{
		snprintf(error, sizeof(error), "%s", ae_test_run_setup_error());
		ae_test_run_machine_block(&machine);
	}
	if (machine.blocked)
		platform_log("test run: %s %s LIST ERROR %s", list.id, test->id, error);
}

static void finish_run(int with_screen)
{
	if (results_file)
		fclose(results_file);
	results_file = NULL;
	ae_test_run_note_close();
	/* (the TEST RUN screen opens from ae_test_run_update once the main menu is up) */
	screen_pending = with_screen;
	screen_since = system_milliseconds();
	ae_test_run_setup_undo();
	/* (a run that ended on a screen test: AE's main menu reads debug.ae_test_screen again, now back to its old value) */
	ae_ui_test_screen_rearm();
	main_goto_main_menu();
	ae_test_run_input_enable(0);
	platform_log("test run: %s ended (%d passed, %d failed, %d skipped, %d noted)", list.id, (int)machine.passed,
		(int)machine.failed, (int)machine.skipped, (int)machine.noted);
}

static void run_effects(void)
{
	int index;

	for (index = 0; index < machine.effects; index++)
	{
		struct ae_test_run_effect const *item = &machine.effect[index];

		switch (item->kind)
		{
			case AE_TEST_RUN_EFFECT_APPLY:
				apply_current();
				break;
			case AE_TEST_RUN_EFFECT_SHOT:
				evidence_name(evidence.shot, sizeof(evidence.shot), "shots", "png",
					list.test[ae_test_run_machine_test(&machine)].id);
				capture_request_screenshot_as(evidence.shot);
				break;
			case AE_TEST_RUN_EFFECT_NOTE_OPEN:
				note[0] = 0;
				if (!automatics)
					ae_test_run_note_open();
				break;
			case AE_TEST_RUN_EFFECT_RECORD:
				write_record(item->test, item->verdict, item->note_kind);
				if (item->verdict == AE_TEST_RUN_NOTE)
					ae_test_run_clip_mark_kept(&clip, item->test, evidence.serial);
				break;
			case AE_TEST_RUN_EFFECT_VIDEO_START:
				/* (a reload or a NOTE leaves the clip going) */
				ae_test_run_clip_want(&clip, item->test);
				break;
			case AE_TEST_RUN_EFFECT_VIDEO_KEEP:
				if (ae_test_run_clip_keep(&clip, item->test, evidence.serial))
					evidence.no_clip = 1;
				break;
			case AE_TEST_RUN_EFFECT_VIDEO_DROP:
				ae_test_run_clip_drop(&clip, item->test, evidence.serial);
				break;
			case AE_TEST_RUN_EFFECT_FINISH:
				finish_run(1);
				break;
			default:
				break;
		}
	}
}

/* ---------- debug.test_run_auto */

static void parse_automatic(const char *text)
{
	static const struct { const char *name; enum ae_test_run_key key; } keys[] =
	{
		{ "PASS", AE_TEST_RUN_KEY_PASS }, { "FAIL", AE_TEST_RUN_KEY_FAIL }, { "NOTE", AE_TEST_RUN_KEY_NOTE },
		{ "SKIP", AE_TEST_RUN_KEY_SKIP },
	};
	int index;

	automatics = automatic_next = 0;
	while (*text && automatics < AUTO_MAXIMUM)
	{
		for (index = 0; index < 4; index++)
		{
			size_t length = strlen(keys[index].name);

			if (!strncmp(text, keys[index].name, length) && text[length] == ':')
			{
				automatic[automatics].key = keys[index].key;
				automatic[automatics].seconds = atoi(text + length + 1);
				automatics++;
				break;
			}
		}
		text = strchr(text, ',');
		if (!text)
			break;
		text++;
	}
}

/* the verdict the automatic run fires now: N seconds after the state it is in came up */
static enum ae_test_run_key automatic_key(void)
{
	int signature = machine.phase * 4096 + machine.position * 16 + (machine.blocked ? 1 : 0);
	unsigned long now = system_milliseconds();
	enum ae_test_run_key key;

	if (!automatics || (machine.phase != AE_TEST_RUN_PHASE_READY && machine.phase != AE_TEST_RUN_PHASE_SUMMARY &&
		!(machine.phase == AE_TEST_RUN_PHASE_LOADING && machine.blocked)))
	{
		automatic_signature = -1;
		return AE_TEST_RUN_KEY_NONE;
	}
	if (signature != automatic_signature)
	{
		automatic_signature = signature;
		automatic_since = now;
		return AE_TEST_RUN_KEY_NONE;
	}
	if (now - automatic_since < (unsigned long)automatic[automatic_next % automatics].seconds * 1000)
		return AE_TEST_RUN_KEY_NONE;
	if (machine.phase == AE_TEST_RUN_PHASE_SUMMARY)
		return AE_TEST_RUN_KEY_PASS;
	if (machine.blocked)
		return AE_TEST_RUN_KEY_SKIP;
	key = automatic[automatic_next % automatics].key;
	automatic_next++;
	automatic_signature = -1;
	return key;
}

/* ---------- start and update */

int ae_test_run_active(void)
{
	return ae_test_run_machine_active(&machine);
}

int ae_test_run_list_path(const char *list_id, char *path, size_t size)
{
	if (!ae_test_run_list_id_ok(list_id))
		return 0;
	/* (<data root>/test-runs/<id>.txt: the data root is where the port's file calls start) */
	return snprintf(path, size, "test-runs/%s.txt", list_id) < (int)size;
}

int ae_test_run_results_path(const char *list_id, char *path, size_t size)
{
	if (!ae_test_run_list_id_ok(list_id))
		return 0;
	in_config(path, size, "test-runs/");
	return strlen(path) + strlen(list_id) + 15 < size && snprintf(path + strlen(path), size - strlen(path),
		"%s.results.jsonl", list_id) > 0;
}

char *ae_test_run_read_results(const char *path)
{
	FILE *file = fopen(path, "rb");
	char *text;
	long size;

	if (file && !fseek(file, 0, SEEK_END) && (size = ftell(file)) >= 0 && !fseek(file, 0, SEEK_SET) &&
		(text = malloc((size_t)size + 1)))
	{
		size = (long)fread(text, 1, (size_t)size, file);
		text[size] = 0;
		fclose(file);
		return text;
	}
	if (file)
		fclose(file);
	text = malloc(1);
	if (text)
		text[0] = 0;
	return text;
}

static int start_list(const char *list_id, int all)
{
	char path[PATH_BYTES], folder[PATH_BYTES], *text;
	time_t now;
	short resume;
	size_t used;
	int torn;

	if (!ae_dev_mode_on())
	{
		platform_log("test run: not started, DEV MODE is off");
		return 0;
	}
	if (ae_test_run_active())
	{
		platform_log("test run: not started, a run is on");
		return 0;
	}
	if (!ae_test_run_list_id_ok(list_id))
	{
		platform_log("test run: not started, %s is not a list id", list_id ? list_id : "(none)");
		return 0;
	}
	screen_pending = 0;
	ae_test_run_list_path(list_id, path, sizeof(path));
	if (!ae_test_run_list_load(path, &list) || !list.tests)
	{
		platform_log("test run: not started, cannot read the list test-runs/%s.txt under %s", list_id,
			platform_data_root());
		return 0;
	}
	if (list.error[0])
		platform_log("test run: list error: %s", list.error);
	in_config(folder, sizeof(folder), "test-runs");
	posix_make_directory(folder);
	if (!ae_test_run_results_path(list.id, path, sizeof(path)))
		return 0;
	text = ae_test_run_read_results(path);
	if (!text)
		return 0;
	/* (RESTART ALL: every test is played from the first whatever the results say; the file is kept and appended to) */
	ae_test_run_states(&list, all ? "" : text, states);
	/* (a run that crashed or was killed resumes at the first test it did not record) */
	resume = all ? -1 : ae_test_run_resume_index(&list, text);
	/* (a crash can leave a last line with no line end: it is ended before the first record, so no reader sees a glued line) */
	used = strlen(text);
	torn = used && text[used - 1] != '\n';
	free(text);
	if (!ae_test_run_machine_start_at(&machine, &list, states, 1, resume))
	{
		platform_log("test run: nothing to run in %s (every test has passed)", list.id);
		return 0;
	}
	results_file = fopen(path, "ab");
	if (!results_file)
	{
		platform_log("test run: not started, cannot append to %s", path);
		memset(&machine, 0, sizeof(machine));
		return 0;
	}
	if (torn && !ae_test_run_file_end_line(results_file))
		platform_log("test run: could not end the torn last line of %s", path);
	now = time(NULL);
	strftime(run_time, sizeof(run_time), "%Y-%m-%dT%H:%M:%S", localtime(&now));
	strcpy(run_stamp, run_time);
	for (text = run_stamp; *text; text++)
	{
		if (*text == ':')
			*text = '-';
	}
	/* (a clip an earlier run left half made) */
	in_config(folder, sizeof(folder), "test-runs/clips");
#ifndef HALO_ANDROID
	/* (Android builds no capture: posix_capture.c is not in its game) */
	capture_folder_clean(folder, 0, is_part);
#endif
	clip_forget();
	sequence = save_failed = 0;
	note[0] = error[0] = 0;
	automatic_signature = -1;
	parse_automatic(config_string("debug.test_run_auto"));
	platform_log("test run: %s started%s, run %s, %d of %d tests, build %s", list.id, all ? " (all)" : "", run_time,
		(int)machine.count, (int)list.tests, build_identity_commit());
	ae_test_run_input_enable(1);
	run_effects();
	return 1;
}

int ae_test_run_start(const char *list_id)
{
	return start_list(list_id, 0);
}

int ae_test_run_start_all(const char *list_id)
{
	return start_list(list_id, 1);
}

void ae_test_run_update(void)
{
	enum ae_test_run_key key;
	int ready = 0;

	/* (not every frame of an idle game: capture_recording_state looks for ffmpeg on the PATH) */
	if (ae_test_run_active() || clip.active || clip.want)
		clip_service();
	if (!ae_test_run_active())
	{
		const char *boot = config_string("debug.test_run");

		/* (after a run: the TEST RUN screen over the main menu, once it is up and the screens have settled) */
		/* (a run that never got back to the menu in 20 s does not open it at a later, surprising time) */
		if (screen_pending && system_milliseconds() - screen_since > 20000)
			screen_pending = 0;
		if (screen_pending && main_menu_is_active() && ae_ui_depth() > 0 && system_milliseconds() - screen_since > 500)
		{
			screen_pending = 0;
			ae_screen_test_run_open();
		}

		if (!booted && *boot && main_menu_is_active())
		{
			booted = 1;
			ae_test_run_start(boot);
		}
		return;
	}
	/* (0.7.0: only the setting ends a run. A test that searches or joins makes this machine a network client, where
	DEV MODE's tools are off, and the run stopped there; its checks and verdicts are this PC's own and stay on) */
	if (!ae_dev_mode_setting())
	{
		/* (the setting was switched off: the run ends, with no summary) */
		platform_log("test run: DEV MODE is off, the run stops");
		machine.phase = AE_TEST_RUN_PHASE_IDLE;
		ae_test_run_clip_leave(&clip, -1);
		finish_run(0);
		return;
	}
	/* (once a frame: the input layer's pad staleness has no slack) */
	key = ae_test_run_input_pressed();
	if (key == AE_TEST_RUN_KEY_NONE)
		key = automatic_key();
	if (machine.phase == AE_TEST_RUN_PHASE_NOTE)
	{
		int result = automatics ? 1 : ae_test_run_note_poll(note, sizeof(note));

		if (automatics)
			snprintf(note, sizeof(note), "auto");
		if (result)
		{
			ae_test_run_machine_note_done(&machine, result);
			/* (a saved NOTE: the box stays up as the chooser; the automatic run picks CONTINUE next frame) */
			if (machine.phase == AE_TEST_RUN_PHASE_VERDICT && !automatics)
				ae_test_run_note_choose_open();
			else
				ae_test_run_note_close();
			run_effects();
		}
		return;
	}
	if (machine.phase == AE_TEST_RUN_PHASE_VERDICT)
	{
		int pick = automatics ? AE_TEST_RUN_CHOICE_CONTINUE : ae_test_run_note_choose_poll();

		if (pick)
		{
			ae_test_run_note_close();
			ae_test_run_machine_choose(&machine, pick == AE_TEST_RUN_CHOICE_PASS ? AE_TEST_RUN_PASS :
				pick == AE_TEST_RUN_CHOICE_FAIL ? AE_TEST_RUN_FAIL : pick == AE_TEST_RUN_CHOICE_CONTINUE ?
				AE_TEST_RUN_SKIP : AE_TEST_RUN_NOTE);
			run_effects();
		}
		return;
	}
	if (machine.phase == AE_TEST_RUN_PHASE_LOADING && !machine.blocked)
	{
		ready = ae_test_run_setup_ready();
		if (!ready && system_milliseconds() - applied_at > LOAD_TIMEOUT_MS)
		{
			snprintf(error, sizeof(error), "did not load");
			ae_test_run_machine_block(&machine);
			platform_log("test run: %s did not load in time", list.test[ae_test_run_machine_test(&machine)].id);
		}
	}
	ae_test_run_step(&machine, key, ready);
	run_effects();
}

void ae_test_run_view(struct ae_test_run_view *out)
{
	struct ae_test_run_test const *test;

	memset(out, 0, sizeof(*out));
	if (!ae_test_run_active())
		return;
	test = &list.test[ae_test_run_machine_test(&machine)];
	out->active = 1;
	out->loading = machine.phase == AE_TEST_RUN_PHASE_LOADING && !machine.blocked;
	out->note_open = machine.phase == AE_TEST_RUN_PHASE_NOTE;
	out->verdict_open = machine.phase == AE_TEST_RUN_PHASE_VERDICT;
	out->choice = (short)ae_test_run_note_choice_index();
	out->summary = machine.phase == AE_TEST_RUN_PHASE_SUMMARY;
	out->hidden = ae_test_run_input_hide_held();
	out->card_bottom = test->card_bottom;
	out->number = (short)(machine.position + 1);
	out->count = machine.count;
	snprintf(out->tag, sizeof(out->tag), "%s %s", list.title[0] ? list.title : list.id, test->id);
	snprintf(out->title, sizeof(out->title), "%s", test->title[0] ? test->title : test->id);
	{
		int check;

		for (check = 0; check < 3 && check < test->checks; check++)
			snprintf(out->check[check], sizeof(out->check[check]), "%s", test->check[check]);
	}
	snprintf(out->error, sizeof(out->error), "%s", save_failed ? "RESULT NOT SAVED" : machine.blocked ? error : "");
	snprintf(out->note, sizeof(out->note), "%s", note);
	snprintf(out->list, sizeof(out->list), "%s", list.title[0] ? list.title : list.id);
	out->note_fail = machine.note_for == AE_TEST_RUN_FAIL;
	out->shot_saved = evidence.shot[0] != 0;
	out->passed = machine.passed;
	out->failed = machine.failed;
	out->skipped = machine.skipped;
	out->noted = machine.noted;
	if (test->video)
	{
		short index = ae_test_run_machine_test(&machine);

		if (evidence.no_ffmpeg || capture_recording_state() == -1)
			out->video_state = AE_TEST_RUN_VIDEO_NO_FFMPEG;
		else if (evidence.no_clip)
			out->video_state = AE_TEST_RUN_VIDEO_NO_CLIP;
		else if (clip.active && clip.test == index)
			out->video_state = clip.stop_sent ? AE_TEST_RUN_VIDEO_SAVING : AE_TEST_RUN_VIDEO_RECORDING;
		else if (clip.want && clip.want_test == index)
			out->video_state = AE_TEST_RUN_VIDEO_WAITING;
	}
}
