/* ae_test_run_results_test.c: TEST RUN's results file (ae_test_run_results.h): escaping, a torn last line, the
pending and resume rules. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_test_run_results.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_test_run_list list;

/* a list of tests a, b, c (c needs a retest since revision 3) */
static void make_list(void)
{
	memset(&list, 0, sizeof list);
	strcpy(list.id, "demo");
	list.revision = 3;
	list.tests = 3;
	strcpy(list.test[0].id, "a");
	strcpy(list.test[1].id, "b");
	strcpy(list.test[2].id, "c");
	list.test[2].retest = 3;
}

static void fill(struct ae_test_run_record *r, const char *test, int revision, enum ae_test_run_verdict verdict)
{
	memset(r, 0, sizeof *r);
	strcpy(r->run, "2026-10-25T14:03:11");
	strcpy(r->list, "demo");
	strcpy(r->test, test);
	strcpy(r->build, "abc1234");
	strcpy(r->time, "2026-10-25T14:05:40");
	r->revision = (short)revision;
	r->verdict = verdict;
}

/* the record as the text of its line */
static void line_of(struct ae_test_run_record const *r, char *text, int size)
{
	FILE *f = tmpfile();
	size_t n;

	CHECK(f && ae_test_run_record_write(f, r) == 1);
	if (!f)
	{
		text[0] = 0;
		return;
	}
	rewind(f);
	n = fread(text, 1, (size_t)size - 1, f);
	text[n] = 0;
	fclose(f);
}

static void note_escaping_round_trip(void)
{
	struct ae_test_run_record in, out;
	char line[8192];
	const char *prefix = "{\"run\":\"2026-10-25T14:03:11\",\"list\":\"demo\",\"revision\":3,\"test\":\"a\",\"verdict\":\"FAIL\",\"note\":\"say";

	fill(&in, "a", 3, AE_TEST_RUN_FAIL);
	strcpy(in.note, "say \"hi\" \\ back\nnew\tline \xC3\xA9 \x01 end");
	strcpy(in.shot, "test-runs/shots/a.png");
	line_of(&in, line, sizeof line);
	CHECK(strlen(line) > 2 && line[strlen(line) - 1] == '\n');
	CHECK(strchr(line, '\n') == line + strlen(line) - 1); /* the only newline is the last byte */
	CHECK(!strncmp(line, prefix, strlen(prefix)));
	CHECK(ae_test_run_record_parse(line, &out) == 1);
	CHECK(!strcmp(out.note, in.note) && !strcmp(out.shot, in.shot) && !strcmp(out.run, in.run));
	CHECK(!strcmp(out.list, "demo") && !strcmp(out.test, "a") && !strcmp(out.build, "abc1234"));
	CHECK(!strcmp(out.time, in.time) && !out.video[0] && out.revision == 3 && out.verdict == AE_TEST_RUN_FAIL);
}

static void resume_after_partial_file(void)
{
	struct ae_test_run_record r;
	enum ae_test_run_state st[3];
	char text[8192], one[2048];

	make_list();
	text[0] = 0;
	fill(&r, "a", 3, AE_TEST_RUN_PASS);
	line_of(&r, one, sizeof one);
	strcat(text, one);
	fill(&r, "b", 3, AE_TEST_RUN_FAIL);
	line_of(&r, one, sizeof one);
	strcat(text, one);
	fill(&r, "c", 3, AE_TEST_RUN_SKIP);
	line_of(&r, one, sizeof one);
	strcat(text, one);
	/* (a 4th record torn in the middle, no newline) */
	fill(&r, "a", 3, AE_TEST_RUN_FAIL);
	line_of(&r, one, sizeof one);
	one[strlen(one) - 8] = 0;
	strcat(text, one);
	ae_test_run_states(&list, text, st);
	CHECK(st[0] == AE_TEST_RUN_PASSED && st[1] == AE_TEST_RUN_FAILED && st[2] == AE_TEST_RUN_SKIPPED);
	CHECK(ae_test_run_pending(&list, st) == 2 && ae_test_run_first_pending(&list, st) == 1);
}

static void retest_reopens_pass(void)
{
	struct ae_test_run_record r;
	enum ae_test_run_state st[3];
	char text[4096], one[2048];

	make_list();
	text[0] = 0;
	fill(&r, "c", 2, AE_TEST_RUN_PASS);
	line_of(&r, one, sizeof one);
	strcat(text, one);
	fill(&r, "a", 2, AE_TEST_RUN_PASS);
	line_of(&r, one, sizeof one);
	strcat(text, one);
	ae_test_run_states(&list, text, st);
	CHECK(st[2] == AE_TEST_RUN_TODO && st[0] == AE_TEST_RUN_PASSED);
	/* (an old FAIL is stale too; a PASS at the retest revision counts) */
	fill(&r, "c", 2, AE_TEST_RUN_FAIL);
	line_of(&r, one, sizeof one);
	ae_test_run_states(&list, one, st);
	CHECK(st[2] == AE_TEST_RUN_TODO);
	fill(&r, "c", 3, AE_TEST_RUN_PASS);
	line_of(&r, one, sizeof one);
	ae_test_run_states(&list, one, st);
	CHECK(st[2] == AE_TEST_RUN_PASSED);
}

static void note_is_not_a_verdict(void)
{
	struct ae_test_run_record r;
	enum ae_test_run_state st[3];
	char text[4096], one[2048];

	make_list();
	fill(&r, "a", 3, AE_TEST_RUN_NOTE);
	line_of(&r, one, sizeof one);
	ae_test_run_states(&list, one, st);
	CHECK(st[0] == AE_TEST_RUN_TODO);
	/* (a NOTE after a PASS leaves the PASS) */
	fill(&r, "a", 3, AE_TEST_RUN_PASS);
	line_of(&r, text, sizeof text);
	fill(&r, "a", 3, AE_TEST_RUN_NOTE);
	line_of(&r, one, sizeof one);
	strcat(text, one);
	ae_test_run_states(&list, text, st);
	CHECK(st[0] == AE_TEST_RUN_PASSED);
}

static void latest_wins(void)
{
	struct ae_test_run_record r;
	enum ae_test_run_state st[3];
	char text[4096], one[2048];

	make_list();
	fill(&r, "a", 3, AE_TEST_RUN_FAIL);
	line_of(&r, text, sizeof text);
	fill(&r, "a", 3, AE_TEST_RUN_PASS);
	line_of(&r, one, sizeof one);
	strcat(text, one);
	ae_test_run_states(&list, text, st);
	CHECK(st[0] == AE_TEST_RUN_PASSED);
	fill(&r, "a", 3, AE_TEST_RUN_FAIL);
	line_of(&r, one, sizeof one);
	strcat(text, one);
	ae_test_run_states(&list, text, st);
	CHECK(st[0] == AE_TEST_RUN_FAILED);
	/* (another list's records and unknown tests are ignored) */
	fill(&r, "a", 3, AE_TEST_RUN_PASS);
	strcpy(r.list, "other");
	line_of(&r, text, sizeof text);
	fill(&r, "zzz", 3, AE_TEST_RUN_PASS);
	line_of(&r, one, sizeof one);
	strcat(text, one);
	ae_test_run_states(&list, text, st);
	CHECK(st[0] == AE_TEST_RUN_TODO && st[1] == AE_TEST_RUN_TODO && st[2] == AE_TEST_RUN_TODO);
}

static void pending_counts(void)
{
	enum ae_test_run_state st[3] = { AE_TEST_RUN_PASSED, AE_TEST_RUN_PASSED, AE_TEST_RUN_PASSED };

	make_list();
	CHECK(ae_test_run_pending(&list, st) == 0 && ae_test_run_first_pending(&list, st) == -1);
	st[2] = AE_TEST_RUN_SKIPPED;
	CHECK(ae_test_run_pending(&list, st) == 1 && ae_test_run_first_pending(&list, st) == 2);
	st[0] = AE_TEST_RUN_FAILED;
	st[1] = AE_TEST_RUN_TODO;
	CHECK(ae_test_run_pending(&list, st) == 3 && ae_test_run_first_pending(&list, st) == 0);
	/* (no file: all TODO, also for NULL and empty text) */
	ae_test_run_states(&list, NULL, st);
	CHECK(ae_test_run_pending(&list, st) == 3);
	st[1] = AE_TEST_RUN_PASSED;
	ae_test_run_states(&list, "", st);
	CHECK(ae_test_run_pending(&list, st) == 3);
}

static void parse_rejects_bad_lines(void)
{
	struct ae_test_run_record r;
	char line[8192], good[2048], big[600];

	fill(&r, "a", 3, AE_TEST_RUN_PASS);
	line_of(&r, good, sizeof good);
	CHECK(ae_test_run_record_parse(good, &r) == 1);
	/* (with no line end, and with a CRLF end, it still parses) */
	good[strlen(good) - 1] = 0;
	CHECK(ae_test_run_record_parse(good, &r) == 1);
	strcpy(line, good);
	strcat(line, "\r\n");
	CHECK(ae_test_run_record_parse(line, &r) == 1);
	/* (torn: no closing brace) */
	good[strlen(good) - 1] = 0;
	CHECK(ae_test_run_record_parse(good, &r) == 0);
	/* (unknown verdict) */
	CHECK(ae_test_run_record_parse("{\"run\":\"r\",\"list\":\"l\",\"revision\":1,\"test\":\"t\",\"verdict\":\"MAYBE\","
		"\"note\":\"\",\"shot\":\"\",\"video\":\"\",\"build\":\"\",\"time\":\"\"}", &r) == 0);
	/* (a note longer than its buffer) */
	memset(big, 'x', sizeof big);
	big[sizeof big - 1] = 0;
	snprintf(line, sizeof line, "{\"run\":\"r\",\"list\":\"l\",\"revision\":1,\"test\":\"t\",\"verdict\":\"PASS\","
		"\"note\":\"%s\",\"shot\":\"\",\"video\":\"\",\"build\":\"\",\"time\":\"\"}", big);
	CHECK(ae_test_run_record_parse(line, &r) == 0);
	CHECK(ae_test_run_record_parse("", &r) == 0 && ae_test_run_record_parse("\n", &r) == 0);
	CHECK(ae_test_run_record_parse(0, &r) == 0 && ae_test_run_record_parse("garbage}", &r) == 0);
	/* (the writer refuses a record with no verdict) */
	{
		FILE *f = tmpfile();

		fill(&r, "a", 3, AE_TEST_RUN_NONE);
		CHECK(f && ae_test_run_record_write(f, &r) == 0);
		if (f)
			fclose(f);
	}
}

static void append_after_torn_line(void)
{
	struct ae_test_run_record r;
	enum ae_test_run_state st[3];
	char text[8192], one[2048];

	make_list();
	text[0] = 0;
	fill(&r, "a", 3, AE_TEST_RUN_PASS);
	line_of(&r, one, sizeof one);
	strcat(text, one);
	fill(&r, "b", 3, AE_TEST_RUN_FAIL);
	line_of(&r, one, sizeof one);
	strcat(text, one);
	/* (a torn fragment with no line end, then the next run's first record glued on) */
	fill(&r, "a", 3, AE_TEST_RUN_FAIL);
	line_of(&r, one, sizeof one);
	one[strlen(one) - 12] = 0;
	strcat(text, one);
	fill(&r, "c", 3, AE_TEST_RUN_PASS);
	line_of(&r, one, sizeof one);
	strcat(text, one);
	ae_test_run_states(&list, text, st);
	CHECK(st[0] == AE_TEST_RUN_PASSED && st[1] == AE_TEST_RUN_FAILED && st[2] == AE_TEST_RUN_PASSED);
}

static void more_bad_input(void)
{
	struct ae_test_run_record r;
	char line[2048];
	enum ae_test_run_state st[1];

	/* (a \u00XX escape decodes) */
	snprintf(line, sizeof line, "{\"run\":\"r\",\"list\":\"l\",\"revision\":1,\"test\":\"t\",\"verdict\":\"PASS\","
		"\"note\":\"a\\u0041\\u001fb\",\"shot\":\"\",\"video\":\"\",\"build\":\"\",\"time\":\"\"}");
	CHECK(ae_test_run_record_parse(line, &r) == 1 && !strcmp(r.note, "aA\x1f" "b"));
	/* (a line ending in a truncated escape) */
	CHECK(ae_test_run_record_parse("{\"run\":\"r\\}", &r) == 0);
	CHECK(ae_test_run_record_parse("{\"run\":\"r\\u00}", &r) == 0);
	CHECK(ae_test_run_record_parse("{\"run\":\"r\\u00\"}", &r) == 0);
	/* (revision out of range) */
	CHECK(ae_test_run_record_parse("{\"run\":\"r\",\"list\":\"l\",\"revision\":-1,\"test\":\"t\",\"verdict\":\"PASS\","
		"\"note\":\"\",\"shot\":\"\",\"video\":\"\",\"build\":\"\",\"time\":\"\"}", &r) == 0);
	CHECK(ae_test_run_record_parse("{\"run\":\"r\",\"list\":\"l\",\"revision\":40000,\"test\":\"t\",\"verdict\":\"PASS\","
		"\"note\":\"\",\"shot\":\"\",\"video\":\"\",\"build\":\"\",\"time\":\"\"}", &r) == 0);
	{
		FILE *f = tmpfile();

		fill(&r, "a", -1, AE_TEST_RUN_PASS);
		CHECK(f && ae_test_run_record_write(f, &r) == 0);
		if (f)
			fclose(f);
	}
	/* (an empty list, and NULL arguments) */
	make_list();
	list.tests = 0;
	st[0] = AE_TEST_RUN_TODO;
	CHECK(ae_test_run_pending(&list, st) == 0 && ae_test_run_first_pending(&list, st) == -1);
	ae_test_run_states(NULL, "x", st);
	ae_test_run_states(&list, "x", NULL);
	CHECK(ae_test_run_pending(NULL, st) == 0 && ae_test_run_pending(&list, NULL) == 0);
	CHECK(ae_test_run_first_pending(NULL, st) == -1 && ae_test_run_first_pending(&list, NULL) == -1);
}

/* the text of records of one run, as the writer makes them: tests given as letters with their verdicts ("aF" is test a FAIL) */
static void run_text(char *text, const char *run, const char *verdicts)
{
	const char *at;

	for (at = verdicts; at[0] && at[1]; at += 2)
	{
		struct ae_test_run_record r;
		char one[2048], test[2] = { at[0], 0 };

		fill(&r, test, 3, at[1] == 'P' ? AE_TEST_RUN_PASS : at[1] == 'F' ? AE_TEST_RUN_FAIL :
			at[1] == 'S' ? AE_TEST_RUN_SKIP : AE_TEST_RUN_NOTE);
		strcpy(r.run, run);
		line_of(&r, one, sizeof one);
		strcat(text, one);
	}
}

/* a crashed run resumes at the first test it did not record */
static void resume_index(void)
{
	char text[16384] = "";

	make_list();
	CHECK(ae_test_run_resume_index(&list, NULL) == -1 && ae_test_run_resume_index(&list, "") == -1);
	CHECK(ae_test_run_resume_index(NULL, "x") == -1);
	/* FAIL, FAIL, then a crash on test c */
	run_text(text, "r1", "aFbF");
	CHECK(ae_test_run_resume_index(&list, text) == 2);
	/* a complete run leaves none */
	text[0] = 0;
	run_text(text, "r1", "aFbFcP");
	CHECK(ae_test_run_resume_index(&list, text) == -1);
	/* the next run plays the pending a and b, and crashes after a: b is the point */
	run_text(text, "r2", "aP");
	CHECK(ae_test_run_resume_index(&list, text) == 1);
	/* ... and finishing b leaves none */
	run_text(text, "r2", "bS");
	CHECK(ae_test_run_resume_index(&list, text) == -1);
	/* a SKIP and a NOTE count as recorded; a first run that crashed before its first record leaves no point */
	text[0] = 0;
	run_text(text, "r1", "aSbN");
	CHECK(ae_test_run_resume_index(&list, text) == 2);
	/* a torn last line is not a record: the point is that test */
	text[0] = 0;
	run_text(text, "r1", "aPbP");
	strcat(text, "{\"run\":\"r1\",\"list\":\"demo\",\"revis");
	CHECK(ae_test_run_resume_index(&list, text) == 2);
	/* a test with a list error is never the point */
	text[0] = 0;
	run_text(text, "r1", "aPbP");
	strcpy(list.test[2].error, "no map");
	CHECK(ae_test_run_resume_index(&list, text) == -1);
}

int main(void)
{
	note_escaping_round_trip();
	resume_after_partial_file();
	retest_reopens_pass();
	note_is_not_a_verdict();
	latest_wins();
	pending_counts();
	parse_rejects_bad_lines();
	append_after_torn_line();
	more_bad_input();
	resume_index();
	if (failures)
		printf("%d failures\n", failures);
	return failures != 0;
}
