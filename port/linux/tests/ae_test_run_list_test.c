/* ae_test_run_list_test.c: TEST RUN's list format (ae_test_run_list.h): the sample list, bad lines kept with their
line numbers, the limits, comments and blank lines. Run from the repository root (the sample is read by its path). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_test_run_list.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_test_run_list list;

static void parses_sample(void)
{
	const struct ae_test_run_test *t;

	CHECK(ae_test_run_list_load("tools/ae_test/test_runs/sample.txt", &list) == 1);
	CHECK(!strcmp(list.id, "sample") && !strcmp(list.title, "SAMPLE LIST") && list.revision == 3 && list.tests == 3);
	CHECK(!list.error[0]);
	t = &list.test[0];
	CHECK(!strcmp(t->id, "sling-swap") && !strcmp(t->title, "Rifle slung after a swap") && !t->error[0]);
	CHECK(t->checks == 2 && !strcmp(t->check[0], "Pick up the pistol: the rifle shows on your back"));
	CHECK(!strcmp(t->check[1], "F2 shoulder cam: no floating or clipping"));
	CHECK(!strcmp(t->map, "bloodgulch") && t->screen == 0 && !strcmp(t->gametype, "AE SLAYER"));
	CHECK(t->sets == 1 && !strcmp(t->set[0].key, "debug.network_test_loadout"));
	CHECK(!strcmp(t->set[0].value, "2,3"));
	CHECK(t->players == 2 && t->retest == 3 && !t->video && !t->card_bottom);
	t = &list.test[1];
	CHECK(!strcmp(t->id, "menu-screen") && t->screen == 1 && !t->map[0] && t->video == 1 && t->card_bottom == 1);
	CHECK(t->players == 0 && t->retest == 0 && !t->error[0]);
	t = &list.test[2];
	CHECK(!strcmp(t->id, "plain-map") && !strcmp(t->map, "hangemhigh") && !t->title[0] && !t->gametype[0]);
	CHECK(t->checks == 1 && t->sets == 0 && !t->error[0]);
	CHECK(ae_test_run_list_load("tools/ae_test/test_runs/no-such-file.txt", &list) == 0);
	CHECK(ae_test_run_list_parse("revision 2\n", &list) == 0 && ae_test_run_list_parse(0, &list) == 0);
}

static void bad_lines_reported(void)
{
	/* (line 5 is unknown; the next test is still fine) */
	const char *text =
		"list bad\n"
		"revision 1\n"
		"test a\n"
		"check one\n"
		"frobnicate yes\n"
		"map bloodgulch\n"
		"test b\n"
		"check one\n"
		"map bloodgulch\n";

	CHECK(ae_test_run_list_parse(text, &list) == 1 && list.tests == 2 && !list.error[0]);
	CHECK(strstr(list.test[0].error, "line 5") && strstr(list.test[0].error, "frobnicate"));
	CHECK(!list.test[1].error[0] && !strcmp(list.test[1].map, "bloodgulch"));

	CHECK(ae_test_run_list_parse("list x\ntest a\ncheck c\n", &list) == 1 && list.tests == 1);
	CHECK(strstr(list.test[0].error, "neither"));
	CHECK(ae_test_run_list_parse("list x\ntest a\ncheck c\nmap m\nscreen 2\n", &list) == 1);
	CHECK(strstr(list.test[0].error, "both"));
	/* a menus test: no map, no screen, nothing loaded */
	CHECK(ae_test_run_list_parse("list x\ntest a\ncheck c\nmenus\n", &list) == 1);
	CHECK(!list.test[0].error[0] && list.test[0].menus == 1 && !list.test[0].map[0] && !list.test[0].screen);
	CHECK(ae_test_run_list_parse("list x\ntest a\ncheck c\nmenus\nmap m\n", &list) == 1);
	CHECK(strstr(list.test[0].error, "both"));
	CHECK(ae_test_run_list_parse("list x\ntest a\nmap m\n", &list) == 1 && strstr(list.test[0].error, "no check"));
	CHECK(ae_test_run_list_parse("list x\ntest a\ncheck 1\ncheck 2\ncheck 3\ncheck 4\nmap m\n", &list) == 1);
	CHECK(list.test[0].checks == 3 && strstr(list.test[0].error, "line 6") && strstr(list.test[0].error, "3 checks"));
	CHECK(ae_test_run_list_parse("list x\ntest A_b\ncheck c\nmap m\n", &list) == 1 && strstr(list.test[0].error, "id"));
	CHECK(ae_test_run_list_parse("list x\ntest a\ncheck c\nmap m\ntest a\ncheck c\nmap m\n", &list) == 1);
	CHECK(!list.test[0].error[0] && strstr(list.test[1].error, "duplicate"));
	CHECK(ae_test_run_list_parse("list x\ntest a\ncheck c\nmap m\nplayers 5\n", &list) == 1);
	CHECK(strstr(list.test[0].error, "players") && list.test[0].players == 0);
	CHECK(ae_test_run_list_parse("list x\ntest a\ncheck c\nmap m\ncard left\n", &list) == 1 && list.test[0].error[0]);
	CHECK(ae_test_run_list_parse("list x\ntest a\ncheck c\nmap m\nset only.key\n", &list) == 1 && list.test[0].error[0]);
	/* (a line before any test that means nothing is the list's) */
	CHECK(ae_test_run_list_parse("list x\ncheck c\ntest a\ncheck c\nmap m\n", &list) == 1);
	CHECK(strstr(list.error, "line 2") && !list.test[0].error[0]);
}

static void limits(void)
{
	char *text = malloc(64 * 1024), *at;
	int i;

	at = text;
	at += sprintf(at, "list big\n");
	for (i = 0; i < 65; i++)
		at += sprintf(at, "test t%d\ncheck c\nmap m\n", i);
	CHECK(ae_test_run_list_parse(text, &list) == 1 && list.tests == AE_TEST_RUN_TESTS);
	CHECK(strstr(list.error, "line") && strstr(list.error, "64") && !list.test[0].error[0] && !list.test[63].error[0]);
	CHECK(!strcmp(list.test[63].id, "t63"));
	/* (long values are cut, never overflowed: under the sanitizers) */
	at = text;
	at += sprintf(at, "list l\ntitle ");
	for (i = 0; i < 200; i++)
		*at++ = 'T';
	at += sprintf(at, "\ntest a\ntitle ");
	for (i = 0; i < 200; i++)
		*at++ = 'U';
	at += sprintf(at, "\ncheck ");
	for (i = 0; i < 200; i++)
		*at++ = 'C';
	at += sprintf(at, "\nmap ");
	for (i = 0; i < 200; i++)
		*at++ = 'M';
	at += sprintf(at, "\nset ");
	for (i = 0; i < 200; i++)
		*at++ = 'K';
	at += sprintf(at, " ");
	for (i = 0; i < 300; i++)
		*at++ = 'V';
	at += sprintf(at, "\n");
	CHECK(ae_test_run_list_parse(text, &list) == 1);
	CHECK(strlen(list.title) == sizeof(list.title) - 1 && strlen(list.test[0].title) == sizeof(list.test[0].title) - 1);
	CHECK(strlen(list.test[0].check[0]) == 63 && strlen(list.test[0].map) == 63);
	CHECK(strlen(list.test[0].set[0].key) == 63 && strlen(list.test[0].set[0].value) == 127);
	/* (17 sets: the 17th is an error) */
	at = text;
	at += sprintf(at, "list s\ntest a\ncheck c\nmap m\n");
	for (i = 0; i < 17; i++)
		at += sprintf(at, "set k%d v\n", i);
	CHECK(ae_test_run_list_parse(text, &list) == 1 && list.test[0].sets == AE_TEST_RUN_SETS);
	CHECK(strstr(list.test[0].error, "16 sets"));
	free(text);
}

static void comments_and_blanks(void)
{
	const char *text =
		"# a comment\r\n"
		"\r\n"
		"   list   cb   \r\n"
		"\t# indented comment\n"
		"\n"
		"test a\r\n"
		"  check   spaced out  \r\n"
		"map m\n"
		"# trailing comment\n";

	CHECK(ae_test_run_list_parse(text, &list) == 1 && !strcmp(list.id, "cb") && list.tests == 1);
	CHECK(!list.test[0].error[0] && !strcmp(list.test[0].check[0], "spaced out") && !strcmp(list.test[0].id, "a"));
	/* (a byte order mark before the first key is skipped) */
	CHECK(ae_test_run_list_parse("\xEF\xBB\xBF" "list bom\ntest a\ncheck c\nmap m\n", &list) == 1);
	CHECK(!strcmp(list.id, "bom") && !list.error[0] && !list.test[0].error[0]);
	/* (an inline # stays in the value; leading zeros are numbers) */
	CHECK(ae_test_run_list_parse("list x\ntest a\ncheck c # not a comment\nmap m\nplayers 04\n", &list) == 1);
	CHECK(!strcmp(list.test[0].check[0], "c # not a comment") && list.test[0].players == 4);
	CHECK(ae_test_run_list_parse("", &list) == 0 && list.tests == 0);
	CHECK(ae_test_run_list_parse("# only a comment", &list) == 0);
}

int main(void)
{
	parses_sample();
	bad_lines_reported();
	limits();
	comments_and_blanks();
	if (failures)
		printf("%d failures\n", failures);
	return failures != 0;
}
