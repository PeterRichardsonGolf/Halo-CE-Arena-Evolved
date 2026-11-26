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
#include "ae_dev_cmd.h"
#include "ae_glue_bot_records.h"
#include "ae_dev_events.h"
#include "ae_dev_mode.h"
#include "ae_hooks.h"
#include "ae_input.h"
#include "ae_pack_format.h"
#include "ae_test_run.h"
#include "ae_test_run_agent.h"
#include "ae_test_run_card.h"
#include "ae_test_run_clip.h"
#include "ae_test_run_input.h"
#include "ae_test_run_lines.h"
#include "ae_test_run_machine.h"
#include "ae_test_run_note.h"
#include "ae_test_run_results.h"
#include "ae_test_run_setup.h"
#include "ae_screen_test_run.h"
#include "ae_ui.h"
#include "ae_variant_ext.h"
#include "ae_weapon_selection_game.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "ae_companion.h"

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
int update_executable_path(char *path, int size);
/* ae_draw.h's: the next frame's boxes as JSON (the frame lint's), and the card's box as an overlay in them */
void ae_draw_boxes_next_frame(const char *path);
void ae_draw_boxes_overlay(const float box[4]);

/* a test that is not ready this long after its setup was applied did not load */
#define LOAD_TIMEOUT_MS 120000
/* a recording asked for that is not seen this long after the request did not start */
#define AUTO_MAXIMUM 8
#define PATH_BYTES 1024

static struct ae_test_run_list list; /* (about 310 KB: static) */
/* an event of the run in DEV MODE's event stream (below) */
static void test_event(short index, const char *state, const char *extra);
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
/* The run-by-hand mode presses verdicts as a tester does (ae_test_run_input_press), and the run takes the path
of a run by hand: the NOTE box and its choice open, nothing is typed or picked for them */
static int owner_way;

/* whether the run takes the automatic run's own path (no NOTE box, nothing waits for a person). Every place where an
automatic run goes another way than a run by hand asks this, never `automatics`: this mode uses the run-by-hand path
with scripted verdict presses, and a preflight is useful only when it exercises that path (tools/test_ae_test_run.py
reads this file for a bare `automatics` outside the verdict script's own functions) */
static int automatic_path(void)
{
	return automatics && !owner_way;
}
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

/* a list's `press` lines (preflight runs only: a run by hand leaves them to the person): the test whose presses are
going, the next check and the next button in its line, and when the next is sent */
#define PRESS_FIRST_MS 1000
#define PRESS_EVERY_MS 600
static struct { short test; short check; short at; unsigned long next; } pressing = { -1, 0, 0, 0 };

/* whether a verdict script runs (debug.test_run_auto), by hand or not: what every scripted run does and a run by hand
does not, the scene's picture and a list's presses, asks this (a person presses for themselves) */
static int scripted_run(void)
{
	return automatics != 0;
}

static void presses_begin(short index)
{
	pressing.test = scripted_run() ? index : -1;
	pressing.check = pressing.at = 0;
	pressing.next = system_milliseconds() + PRESS_FIRST_MS;
}

/* whether the test's presses are still to go: its verdict waits for them */
static int presses_pending(void)
{
	return pressing.test != -1 && pressing.test == ae_test_run_machine_test(&machine) &&
		machine.phase == AE_TEST_RUN_PHASE_READY;
}

/* one button a call when its time has come; the line's words: a b x y up down left right start select lb rb delete */
static void press_service(void)
{
	static const struct { const char *name; int action; } buttons[] =
	{
		{ "a", AE_ACTION_ACCEPT }, { "b", AE_ACTION_BACK }, { "x", AE_ACTION_X }, { "y", AE_ACTION_Y },
		{ "up", AE_ACTION_UP }, { "down", AE_ACTION_DOWN }, { "left", AE_ACTION_LEFT }, { "right", AE_ACTION_RIGHT },
		{ "start", AE_ACTION_START }, { "select", AE_ACTION_SELECT }, { "lb", AE_ACTION_TAB_PREVIOUS },
		{ "rb", AE_ACTION_TAB_NEXT }, { "delete", AE_ACTION_DELETE },
	};
	struct ae_test_run_test const *test;

	if (!presses_pending() || (long)(system_milliseconds() - pressing.next) < 0)
		return;
	test = &list.test[pressing.test];
	while (pressing.check < AE_TEST_RUN_CHECKS)
	{
		const char *line = test->press[pressing.check];
		char word[16];
		int skip = pressing.at, length, index;

		while (skip-- > 0 && *line)
		{
			line += strspn(line, " \t");
			line += strcspn(line, " \t");
		}
		line += strspn(line, " \t");
		length = (int)strcspn(line, " \t");
		if (!length)
		{
			pressing.check++;
			pressing.at = 0;
			continue;
		}
		snprintf(word, sizeof(word), "%.*s", length < 15 ? length : 15, line);
		for (index = 0; index < (int)(sizeof(buttons) / sizeof(buttons[0])); index++)
		{
			if (!strcmp(word, buttons[index].name))
				break;
		}
		platform_log("test run: %s %s press %d %s%s", list.id, test->id, pressing.check + 1, word,
			index < (int)(sizeof(buttons) / sizeof(buttons[0])) && ae_input_press(buttons[index].action) ? "" :
			": not pressed (no such button, or the queue is full)");
		pressing.at++;
		pressing.next = system_milliseconds() + PRESS_EVERY_MS;
		return;
	}
	pressing.test = -1;
}

/* the SHA-256 of the program file (hex), read once a process at the first run's start: each record names the very program
that played it, as the commit alone cannot (a build with changes, a binary copied by hand); "" when it cannot be read */
static char program_digest[72];

static void program_digest_read(void)
{
	static int read_once;
	static unsigned char block[1 << 16];
	char path[PATH_BYTES];
	struct ae_pack_sha256 context;
	unsigned char digest[32];
	FILE *file;
	size_t got;
	int index;

	if (read_once)
		return;
	read_once = 1;
	if (!update_executable_path(path, (int)sizeof(path)) || (file = fopen(path, "rb")) == NULL)
		return;
	ae_pack_sha256_begin(&context);
	while ((got = fread(block, 1, sizeof(block), file)) > 0)
		ae_pack_sha256_add(&context, block, (unsigned int)got);
	fclose(file);
	ae_pack_sha256_end(&context, digest);
	for (index = 0; index < 32; index++)
		snprintf(program_digest + index * 2, 3, "%02x", digest[index]);
}

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
	snprintf(record.program, sizeof(record.program), "%s", program_digest);
	/* (the live judge's verdicts beside the tester's, when it spoke; the stream it judged, for its evidence) */
	{
		struct ae_test_run_agent const *agent = ae_test_run_agent_get();
		short check;
		int used = 0;

		for (check = 0; agent->heard && check < test->checks && used < (int)sizeof(record.agent) - 1; check++)
		{
			int wrote = snprintf(record.agent + used, sizeof(record.agent) - (size_t)used, "%s%d %s%s%s",
				check ? " | " : "", check + 1, ae_test_run_agent_word(agent->verdict[check]),
				agent->reason[check][0] ? " " : "", agent->reason[check]);

			/* (cut at the field's end: snprintf says what it would have written) */
			if (wrote < 0)
				break;
			used += wrote;
		}
		record.ask_later = note_kind == AE_TEST_RUN_NOTE_LATER;
		snprintf(record.events, sizeof(record.events), "%s", ae_dev_events_path());
	}
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
	{
		char extra[64];

		snprintf(extra, sizeof extra, "\"verdict\":\"%s\"", name[verdict]);
		test_event(test_index, "verdict", extra);
	}
}

/* ---------- effects */

/* the test whose setup is up, NONE when none is */
static short loaded_test = NONE;

/* a test's setup lines, once it is up: pending (the verdict waits while it is), whether a reset goes first (the test
goes on in the game the one before it left), the reset sent, since when it waits; the lines' order and retries
(ae_test_run_lines.h), the line out, and the number of the dev commands' last answer when it was sent */
static struct
{
	int pending, reset, reset_sent, started;
	unsigned long since;
	struct ae_test_run_lines lines;
	short out;
	long answers_before;
	char answer[160];
} setup;

static void test_event(short index, const char *state, const char *extra)
{
	char list_text[96], test_text[96];

	if (!ae_dev_events_on() || index < 0 || index >= list.tests)
		return;
	ae_dev_event_text(list_text, sizeof list_text, list.id);
	ae_dev_event_text(test_text, sizeof test_text, list.test[index].id);
	ae_dev_event("test", "\"list\":\"%s\",\"test\":\"%s\",\"state\":\"%s\"%s%s", list_text, test_text, state,
		extra && extra[0] ? "," : "", extra ? extra : "");
}

/* a test whose setup failed: said on the card as a SETUP ERROR (the test is blocked: its scene is not the one asked for,
so it is not judged), in the log and in the event stream */
static void setup_failed(short index, const char *what)
{
	struct ae_test_run_test const *test = &list.test[index];

	snprintf(error, sizeof(error), "SETUP ERROR: %s", what);
	platform_log("test run: %s %s SETUP ERROR: %s", list.id, test->id, what);
	setup.pending = 0;
	ae_test_run_machine_setup_failed(&machine);
	{
		char text[256], extra[288];

		ae_dev_event_text(text, sizeof text, what);
		snprintf(extra, sizeof extra, "\"error\":\"%s\"", text);
		test_event(index, "setup_failed", extra);
	}
}

/* an automatic run (debug.test_run_auto, a headless run before the list is played) keeps one picture of each test's
scene: asked for a second after its last setup line was accepted, so the view has settled, as
test-runs/preflight/<list>-<test>.png in the config folder */
#define PREFLIGHT_SHOT_MS 1000
static struct { unsigned long at; short test; } preflight_shot = { 0, NONE };

static void preflight_shot_service(void)
{
	char folder[PATH_BYTES], relative[160], list_id[40], test_id[40];

	if (!preflight_shot.at || (long)(system_milliseconds() - preflight_shot.at) < 0)
		return;
	preflight_shot.at = 0;
	if (machine.phase != AE_TEST_RUN_PHASE_READY || ae_test_run_machine_test(&machine) != preflight_shot.test)
		return;
	in_config(folder, sizeof(folder), "test-runs/preflight");
	posix_make_directory(folder);
	clean_name(list_id, sizeof(list_id), list.id);
	clean_name(test_id, sizeof(test_id), list.test[preflight_shot.test].id);
	snprintf(relative, sizeof(relative), "test-runs/preflight/%s-%s.png", list_id, test_id);
	capture_request_screenshot_as(relative);
	/* (and the AE menus' boxes of that frame beside it, <list>-<test>.boxes.json: tools/ae_test/frame_lint.py) */
	{
		char boxes[PATH_BYTES], boxes_relative[180];

		snprintf(boxes_relative, sizeof(boxes_relative), "test-runs/preflight/%s-%s.boxes.json", list_id, test_id);
		float card[4];

		in_config(boxes, sizeof(boxes), boxes_relative);
		ae_draw_boxes_next_frame(boxes);
		if (ae_test_run_card_box(card))
			ae_draw_boxes_overlay(card);
	}
	{
		int placed, in_view;

		/* (the AI the setup placed, and how many are ahead of the player: a picture with none of them in it is a
		scene the tester cannot judge; tools/ae_test/preflight.py fails it) */
		float below, ahead;

		ae_dev_cmd_actors_in_view(&placed, &in_view);
		ae_dev_cmd_drops(&below, &ahead);
		platform_log("test run: %s %s scene picture %s, actors placed %d, in view %d, drop below %.1f, ahead %.1f",
			list.id, list.test[preflight_shot.test].id, relative, placed, in_view, (double)below, (double)ahead);
	}
}

/* (each frame of a ready test) its group's and its own setup lines: on a map test after the reset (when it goes on in
the game) and once the game has settled, then one at a time in list order, each sent when the one before was accepted
(ae_test_run_lines.h). The judged window of its expect lines ("start") opens just before the first line goes, after the
reset; the verdict waits until they have run */
/* the rules the test's game really plays, read when its scene is about to be set up: the gametype's name and its extra
rules as the game has them now (not as a gametype was chosen before the game started), the start weapons its weapon
block names and the bots setting. One line in the log ("test run rules: ..."), and one log event a rule in the event
stream ("live rule: flag pickup hold"), for a list's expect lines */
static void live_rules(short index)
{
	static const char *const returns[] = { "ce", "h2", "stand" };
	struct ae_variant_ext const *ext = ae_variant_ext_game();
	struct game_variant *variant = game_engine_running() ? game_engine_get_variant() : NULL;
	char name[16], rule[11][48], line[560];
	int rules = 0, each, length;

	if (!list.test[index].map[0] || !variant)
		return;
	for (each = 0; each < (int)sizeof(name) - 1 && variant->human_readable_game_description[each]; each++)
		name[each] = variant->human_readable_game_description[each] < 127 && variant->human_readable_game_description[each] >= 32 ?
			(char)variant->human_readable_game_description[each] : '?';
	name[each] = 0;
	snprintf(rule[rules++], sizeof(rule[0]), "gametype %s", name);
	snprintf(rule[rules++], sizeof(rule[0]), "shootable grenades %s",
		ext->shootable_grenades == AE_EXT_SHOOTABLE_GRENADES_ON ? "on" : "off");
	snprintf(rule[rules++], sizeof(rule[0]), "split screen join %s", ext->split_join == AE_EXT_SPLIT_JOIN_ON ? "on" : "off");
	snprintf(rule[rules++], sizeof(rule[0]), "flag pickup %s", (ext->ctf_flags & AE_EXT_CTF_HOLD_PICKUP) ? "hold" : "touch");
	snprintf(rule[rules++], sizeof(rule[0]), "flag return %s", ext->ctf_return < NUMBEROF(returns) ?
		returns[ext->ctf_return] : "?");
	snprintf(rule[rules++], sizeof(rule[0]), "flag melee %s", (ext->ctf_flags & AE_EXT_CTF_MASSIVE_DAMAGE) ? "on" : "off");
	snprintf(rule[rules++], sizeof(rule[0]), "drive with flag %s", (ext->ctf_flags & AE_EXT_CTF_NO_DRIVING) ? "no" : "yes");
	snprintf(rule[rules++], sizeof(rule[0]), "own flag waypoint %d", (int)ext->ctf_own_flag_waypoint);
	snprintf(rule[rules++], sizeof(rule[0]), "slow carrier %s", (ext->ctf_flags & AE_EXT_CTF_SLOW_CARRIER) ? "on" : "off");
	snprintf(rule[rules++], sizeof(rule[0]), "start weapons %u %u", ae_weapon_selection_running_start(0),
		ae_weapon_selection_running_start(1));
	snprintf(rule[rules++], sizeof(rule[0]), "bots.count %ld", config_integer("bots.count"));
	length = snprintf(line, sizeof(line), "test run rules: %s %s:", list.id, list.test[index].id);
	for (each = 0; each < rules && length < (int)sizeof(line); each++)
	{
		char event[64];

		length += snprintf(line + length, sizeof(line) - (size_t)length, "%s %s", each ? "," : "", rule[each]);
		snprintf(event, sizeof(event), "live rule: %s", rule[each]);
		ae_dev_events_log(event);
	}
	platform_log("%s", line);
}

/* the test's scene is about to be set up: its window in the event stream opens. 0 when the game is not the one the test
asked for (a `set` value lost while it loaded): the setup has failed, nothing more is sent */
static int test_started(short index)
{
	char what[200];

	test_event(index, "start", "");
	/* (a test's made-up bot records are its own, a kept game's tests too: a test without `setup botrecord demo` reads
	the file's) */
	ae_bot_records_demo_end("a new test");
	live_rules(index);
	/* (a test with a load of its own; one that goes on in the game before it has that game's values, which its setup
	lines may have changed) */
	if (!setup.reset && ae_test_run_setup_lost(what, (int)sizeof(what)))
	{
		setup_failed(index, what);
		return 0;
	}
	return 1;
}

static void setup_service(void)
{
	short index = ae_test_run_machine_test(&machine);
	struct ae_test_run_test const *test = &list.test[index];
	int guard;

	if (!setup.pending || machine.phase != AE_TEST_RUN_PHASE_READY || machine.blocked)
		return;
	if (test->map[0] && setup.reset && !setup.reset_sent)
	{
		/* (a campaign level's test goes on in the level the test before it left: the reset there keeps the AI squad's
		players and puts their bodies beside the player (who this test's setup lines then move to the scene, the squad
		with them); the multiplayer reset would kill every player, which in co-op is a team wipe) */
		ae_dev_cmd_line(strcmp(test->gametype, "coop") ? "reset" : "companion reset", AE_DEV_CMD_FROM_TEST_RUN);
		setup.reset_sent = 1;
		setup.since = system_milliseconds();
		return;
	}
	if (!setup.started)
	{
		setup.started = 1;
		setup.out = NONE;
		ae_test_run_lines_start(&setup.lines, test->setups, test->map[0] && (setup.reset || test->setups),
			setup.since);
	}
	/* (a line answered as it is sent, one that needs no tick, lets the next go in the same frame) */
	for (guard = 0; guard <= AE_TEST_RUN_SETUPS; guard++)
	{
		int answered = 0, ok = 0;
		short line = 0;
		enum ae_test_run_lines_action action;

		if (setup.out != NONE)
			answered = ae_dev_cmd_test_run_answer(test->setup[setup.out], setup.answers_before, &ok, setup.answer,
				(int)sizeof(setup.answer));
		action = ae_test_run_lines_step(&setup.lines, setup.lines.settle ? ae_dev_cmd_settled() : 1, answered, ok,
			system_milliseconds(), &line);
		if (action == AE_TEST_RUN_LINES_SEND)
		{
			if (!line && setup.lines.tries == 1 && !test_started(index))
				return;
			setup.out = line;
			setup.answers_before = ae_dev_cmd_test_run_answers();
			setup.answer[0] = 0;
			/* (not a dev command at all: nothing will answer it) */
			if (!ae_dev_cmd_line(test->setup[line], AE_DEV_CMD_FROM_TEST_RUN))
			{
				char what[200];

				snprintf(what, sizeof(what), "%.40s: not a dev command", test->setup[line]);
				setup_failed(index, what);
				return;
			}
			continue;
		}
		if (action == AE_TEST_RUN_LINES_DONE)
		{
			/* (a test with no line: its window opens here) */
			if (!test->setups && !test_started(index))
				return;
			setup.pending = 0;
			/* (a campaign test: the AI squad's bodies go to where its lines left the player, a goto's spot) */
			if (!strcmp(test->gametype, "coop"))
				ae_companion_reset();
			/* (a test player's route set by the setup sets off now, after the squad's reset) */
			ae_dev_cmd_puppet_go();
			test_event(index, "setup", "");
			presses_begin(index);
			/* (the scene's picture: taken on any run with a verdict script, the run-by-hand path too; a picture, not a path) */
			if (scripted_run())
			{
				preflight_shot.at = system_milliseconds() + PREFLIGHT_SHOT_MS;
				preflight_shot.at += !preflight_shot.at;
				preflight_shot.test = index;
			}
		}
		else if (action == AE_TEST_RUN_LINES_FAILED)
		{
			char what[200];

			if (setup.lines.why == AE_TEST_RUN_LINES_NOT_SETTLED)
				snprintf(what, sizeof(what), "the game did not settle in %d s, no line was run",
					AE_TEST_RUN_LINES_SETTLE_MS / 1000);
			else if (setup.lines.why == AE_TEST_RUN_LINES_NO_ANSWER)
				snprintf(what, sizeof(what), "%.40s: no answer in %d s", test->setup[line],
					AE_TEST_RUN_LINES_ANSWER_MS / 1000);
			else
				snprintf(what, sizeof(what), "%.40s: %s", test->setup[line], setup.answer);
			setup_failed(index, what);
		}
		return;
	}
}

static void apply_current(void)
{
	short index = ae_test_run_machine_test(&machine);
	struct ae_test_run_test const *test = &list.test[index];

	error[0] = 0;
	evidence.shot[0] = 0;
	ae_test_run_agent_clear();
	setup.pending = 1;
	setup.reset = 0;
	setup.reset_sent = 0;
	setup.started = 0;
	setup.out = NONE;
	setup.since = system_milliseconds();
	test_event(index, "load", "");
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
	/* (the same setup as the test before it: it goes on in the running game; RELOAD on a test always loads) */
	else if (loaded_test != NONE && loaded_test != index && ae_test_run_same_setup(&list.test[loaded_test], test) &&
		ae_test_run_setup_keep(test))
	{
		loaded_test = index;
		/* (it goes on in the game the test before it left: that game's state goes back first, and what the dev commands
		bots and config changed in the tests before: a test's config lines are its own) */
		ae_dev_cmd_settings_back();
		setup.reset = 1;
	}
	else
	{
		/* (a load of its own: what the dev commands bots and config changed in the tests before goes back first) */
		ae_dev_cmd_settings_back();
		/* (a menus test: its staged scene starts without the screens and dialogs of the tests before it, run by hand or
		automatic; a dialog left under the next scene's screens would hold its player's dialog slot and refuse every
		dialog that scene opens) */
		if (test->menus)
			ae_ui_test_screen_rearm();
		if (!ae_test_run_setup_apply(test))
		{
			snprintf(error, sizeof(error), "%s", ae_test_run_setup_error());
			ae_test_run_machine_block(&machine);
		}
		else
			loaded_test = index;
	}
	if (machine.blocked)
		loaded_test = NONE;
	if (machine.blocked)
	{
		setup.pending = 0;
		platform_log("test run: %s %s LIST ERROR %s", list.id, test->id, error);
	}
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
	loaded_test = NONE;
	ae_dev_cmd_settings_back();
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
				if (!automatic_path())
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

	automatics = automatic_next = owner_way = 0;
	while (*text && automatics < AUTO_MAXIMUM)
	{
		if (!strncmp(text, "OWNER", 5) && (text[5] == ',' || !text[5]))
			owner_way = 1;
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
	/* (the run-by-hand path gives PASS and SKIP only: a FAIL or NOTE opens the NOTE box, which waits for typing) */
	if (owner_way)
	{
		for (index = 0; index < automatics; index++)
		{
			if (automatic[index].key != AE_TEST_RUN_KEY_PASS && automatic[index].key != AE_TEST_RUN_KEY_SKIP)
				automatic[index].key = AE_TEST_RUN_KEY_PASS;
		}
		if (!automatics)
		{
			automatic[0].key = AE_TEST_RUN_KEY_PASS;
			automatic[0].seconds = 6;
			automatics = 1;
		}
		platform_log("test run: verdicts pressed as in a run by hand, on the first pad, after %d s", automatic[0].seconds);
	}
}

/* the verdict the automatic run fires now: N seconds after the state it is in came up */
static enum ae_test_run_key automatic_key(void)
{
	int signature = machine.phase * 4096 + machine.position * 16 + (machine.blocked ? 1 : 0);
	unsigned long now = system_milliseconds();
	enum ae_test_run_key key;

	if (!automatics || (machine.phase != AE_TEST_RUN_PHASE_READY && machine.phase != AE_TEST_RUN_PHASE_SUMMARY &&
		machine.phase != AE_TEST_RUN_PHASE_CHOOSE && !(machine.phase == AE_TEST_RUN_PHASE_LOADING && machine.blocked)))
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
	/* (an automatic REJECT's choice: no note, the agent asks later) */
	if (machine.phase == AE_TEST_RUN_PHASE_CHOOSE)
		return AE_TEST_RUN_KEY_SKIP;
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

void ae_test_run_abandon(void)
{
	if (!ae_test_run_active())
		return;
	platform_log("test run: the run left on is dropped, a new start was asked for");
	ae_test_run_machine_abandon(&machine);
	ae_test_run_clip_leave(&clip, -1);
	finish_run(0);
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
	program_digest_read();
	platform_log("test run: %s started%s, run %s, %d of %d tests, build %s, program %.12s", list.id, all ? " (all)" : "",
		run_time, (int)machine.count, (int)list.tests, build_identity_commit(), program_digest[0] ? program_digest : "unknown");
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
	/* (a test's setup under way: no verdict yet, and the automatic one counts from when it is done) */
	if ((setup.pending || presses_pending()) && machine.phase == AE_TEST_RUN_PHASE_READY && !machine.blocked)
	{
		key = AE_TEST_RUN_KEY_NONE;
		automatic_signature = -1;
	}
	else if (key == AE_TEST_RUN_KEY_NONE && owner_way)
		ae_test_run_input_press(automatic_key());
	else if (key == AE_TEST_RUN_KEY_NONE)
		key = automatic_key();
	if (machine.phase == AE_TEST_RUN_PHASE_NOTE)
	{
		int result = automatic_path() ? 1 : ae_test_run_note_poll(note, sizeof(note));

		if (automatic_path())
			snprintf(note, sizeof(note), "auto");
		if (result)
		{
			ae_test_run_machine_note_done(&machine, result);
			/* (a saved NOTE: the box stays up as the chooser; the automatic run picks CONTINUE next frame) */
			if (machine.phase == AE_TEST_RUN_PHASE_VERDICT && !automatic_path())
				ae_test_run_note_choose_open();
			else
				ae_test_run_note_close();
			run_effects();
		}
		return;
	}
	if (machine.phase == AE_TEST_RUN_PHASE_VERDICT)
	{
		int pick = automatic_path() ? AE_TEST_RUN_CHOICE_CONTINUE : ae_test_run_note_choose_poll();

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
			ae_test_run_setup_give_up();
		}
	}
	{
		int was_loading = machine.phase == AE_TEST_RUN_PHASE_LOADING;

		ae_test_run_step(&machine, key, ready);
		run_effects();
		if (was_loading && machine.phase == AE_TEST_RUN_PHASE_READY)
		{
			setup.since = system_milliseconds();
			test_event(ae_test_run_machine_test(&machine), "ready", "");
		}
	}
	setup_service();
	preflight_shot_service();
	press_service();
}

void ae_test_run_view(struct ae_test_run_view *out)
{
	static short hidden_test = NONE;
	struct ae_test_run_test const *test;
	short test_index;

	memset(out, 0, sizeof(*out));
	if (!ae_test_run_active())
	{
		hidden_test = NONE;
		return;
	}
	test_index = ae_test_run_machine_test(&machine);
	test = &list.test[test_index];
	out->active = 1;
	/* (a test whose setup lines are still running is not up yet: its checks show when its scene is there) */
	out->loading = (machine.phase == AE_TEST_RUN_PHASE_LOADING && !machine.blocked) ||
		(machine.phase == AE_TEST_RUN_PHASE_READY && setup.pending);
	out->note_open = machine.phase == AE_TEST_RUN_PHASE_NOTE;
	out->verdict_open = machine.phase == AE_TEST_RUN_PHASE_VERDICT;
	out->choice = (short)ae_test_run_note_choice_index();
	out->summary = machine.phase == AE_TEST_RUN_PHASE_SUMMARY;
	out->hidden = ae_test_run_input_hidden(test_index != hidden_test || out->note_open || out->verdict_open || out->summary);
	hidden_test = test_index;
	out->card_bottom = test->card_bottom;
	out->card_left = test->card_bottom && test->menus;
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
	out->choose_open = machine.phase == AE_TEST_RUN_PHASE_CHOOSE;
	{
		struct ae_test_run_agent const *agent = ae_test_run_agent_get();
		unsigned long now = system_milliseconds();
		int check;

		out->agent_heard = agent->heard;
		for (check = 0; check < 3; check++)
		{
			out->agent_verdict[check] = agent->verdict[check];
			snprintf(out->agent_reason[check], sizeof(out->agent_reason[check]), "%s", agent->reason[check]);
			out->agent_verdict_age[check] = agent->verdict_at[check] ? now - agent->verdict_at[check] : ~0UL;
		}
		snprintf(out->agent_message, sizeof(out->agent_message), "%s", agent->message);
		out->agent_message_age = agent->message_at ? now - agent->message_at : ~0UL;
	}
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
