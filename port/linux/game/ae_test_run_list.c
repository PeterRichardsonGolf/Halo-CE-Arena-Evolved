/* ae_test_run_list.c: Arena Evolved TEST RUN, the list parser (see ae_test_run_list.h). No engine includes. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_test_run_list.h"

/* (a list file is a few KB; a bigger one is not a list) */
#define LIST_FILE_MAX (1024 * 1024)

/* (the limits' macros in the messages, so a change of a limit changes its message) */
#define STRING_OF_(x) #x
#define STRING_OF(x) STRING_OF_(x)

/* copies text into a buffer of size bytes, cut short rather than overrun */
static void copy_text(char *to, int size, const char *from, int length)
{
	if (length > size - 1)
		length = size - 1;
	memcpy(to, from, (size_t)length);
	to[length] = 0;
}

/* an id is [a-z0-9-], 1 to size-1 characters */
static int id_ok(const char *text, int length, int size)
{
	int i;

	if (length < 1 || length > size - 1)
		return 0;
	for (i = 0; i < length; i++)
		if (!((text[i] >= 'a' && text[i] <= 'z') || (text[i] >= '0' && text[i] <= '9') || text[i] == '-'))
			return 0;
	return 1;
}

int ae_test_run_list_id_ok(const char *id)
{
	return id && id_ok(id, (int)strlen(id), (int)sizeof(((struct ae_test_run_list *)0)->id));
}

/* a whole number in 0..max in the text, or -1 */
static int number_of(const char *text, int length, int max)
{
	int i, n = 0;

	if (length < 1)
		return -1;
	for (i = 0; i < length; i++)
	{
		if (text[i] < '0' || text[i] > '9')
			return -1;
		n = n * 10 + (text[i] - '0');
		if (n > max)
			return -1;
	}
	return n;
}

/* keeps the first problem of a test or list, with its line */
static void note_error(char *error, int line, const char *what, const char *detail, int detail_length)
{
	if (error[0])
		return;
	if (detail_length > 24)
		detail_length = 24;
	snprintf(error, 96, "line %d: %s%s%.*s", line, what, detail_length ? " " : "", detail_length, detail);
}

/* what a finished test must have */
static void finish_test(struct ae_test_run_test *test, int line)
{
	if (!test->checks)
		note_error(test->error, line, "test has no check", "", 0);
	if ((test->map[0] != 0) + (test->screen != 0) + (test->menus != 0) > 1)
		note_error(test->error, line, "test has both map and screen (or menus)", "", 0);
	else if (!test->map[0] && !test->screen && !test->menus)
		note_error(test->error, line, "test has neither map nor screen (nor menus)", "", 0);
}

int ae_test_run_list_parse(const char *text, struct ae_test_run_list *out)
{
	struct ae_test_run_test *test = 0;
	int line = 0, have_list = 0, skipping = 0;
	const char *at = text;

	memset(out, 0, sizeof(*out));
	if (!text)
		return 0;
	/* (a UTF-8 byte order mark some editors write is not part of the first line) */
	if (!memcmp(text, "\xEF\xBB\xBF", 3))
		at = text + 3;
	while (*at)
	{
		const char *start = at, *end, *key_end, *value;
		int key_length, value_length;
		char *error;

		while (*at && *at != '\n')
			at++;
		end = at;
		if (*at == '\n')
			at++;
		line++;
		/* (trim both ends; a CRLF file's \r goes with the spaces) */
		while (start < end && (*start == ' ' || *start == '\t'))
			start++;
		while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
			end--;
		if (start == end || *start == '#')
			continue;
		key_end = start;
		while (key_end < end && *key_end != ' ' && *key_end != '\t')
			key_end++;
		key_length = (int)(key_end - start);
		value = key_end;
		while (value < end && (*value == ' ' || *value == '\t'))
			value++;
		value_length = (int)(end - value);

#define KEY(name) (key_length == (int)sizeof(name) - 1 && !memcmp(start, name, sizeof(name) - 1))
		if (KEY("test"))
		{
			if (test)
				finish_test(test, line - 1);
			skipping = 0;
			if (out->tests >= AE_TEST_RUN_TESTS)
			{
				note_error(out->error, line, "more than " STRING_OF(AE_TEST_RUN_TESTS) " tests, the rest are ignored", "", 0);
				test = 0;
				skipping = 1;
				continue;
			}
			test = &out->test[out->tests++];
			if (!id_ok(value, value_length, (int)sizeof(test->id)))
				note_error(test->error, line, "bad test id", value, value_length);
			copy_text(test->id, (int)sizeof(test->id), value, value_length);
			{
				int i;

				for (i = 0; i < out->tests - 1; i++)
					if (!strcmp(out->test[i].id, test->id))
						note_error(test->error, line, "duplicate test id", value, value_length);
			}
			continue;
		}
		if (skipping)
			continue;
		error = test ? test->error : out->error;

		if (KEY("list") && !test)
		{
			if (!id_ok(value, value_length, (int)sizeof(out->id)))
				note_error(out->error, line, "bad list id", value, value_length);
			copy_text(out->id, (int)sizeof(out->id), value, value_length);
			have_list = 1;
		}
		else if (KEY("revision") && !test)
		{
			int n = number_of(value, value_length, 32767);

			if (n < 0)
				note_error(out->error, line, "bad revision", value, value_length);
			else
				out->revision = (short)n;
		}
		else if (KEY("title"))
		{
			if (test)
				copy_text(test->title, (int)sizeof(test->title), value, value_length);
			else
				copy_text(out->title, (int)sizeof(out->title), value, value_length);
		}
		else if (test && KEY("check"))
		{
			if (test->checks >= AE_TEST_RUN_CHECKS)
				note_error(error, line, "more than " STRING_OF(AE_TEST_RUN_CHECKS) " checks", "", 0);
			else if (!value_length)
				note_error(error, line, "empty check", "", 0);
			else
			{
				copy_text(test->check[test->checks], (int)sizeof(test->check[0]), value, value_length);
				test->checks++;
			}
		}
		else if (test && KEY("map"))
		{
			if (!value_length)
				note_error(error, line, "map has no name", "", 0);
			copy_text(test->map, (int)sizeof(test->map), value, value_length);
		}
		else if (test && KEY("screen"))
		{
			int n = number_of(value, value_length, 32767);

			if (n < 1)
				note_error(error, line, "bad screen number", value, value_length);
			else
				test->screen = (short)n;
		}
		else if (test && KEY("gametype"))
			copy_text(test->gametype, (int)sizeof(test->gametype), value, value_length);
		else if (test && KEY("set"))
		{
			const char *key_text = value, *rest;
			int set_key_length = 0;

			while (key_text + set_key_length < end && key_text[set_key_length] != ' ' && key_text[set_key_length] != '\t')
				set_key_length++;
			rest = key_text + set_key_length;
			while (rest < end && (*rest == ' ' || *rest == '\t'))
				rest++;
			if (!set_key_length || rest == end)
				note_error(error, line, "set needs a key and a value", "", 0);
			else if (test->sets >= AE_TEST_RUN_SETS)
				note_error(error, line, "more than " STRING_OF(AE_TEST_RUN_SETS) " sets", "", 0);
			else
			{
				copy_text(test->set[test->sets].key, (int)sizeof(test->set[0].key), key_text, set_key_length);
				copy_text(test->set[test->sets].value, (int)sizeof(test->set[0].value), rest, (int)(end - rest));
				test->sets++;
			}
		}
		else if (test && KEY("players"))
		{
			int n = number_of(value, value_length, 4);

			if (n < 1)
				note_error(error, line, "players must be 1 to 4", value, value_length);
			else
				test->players = (short)n;
		}
		else if (test && KEY("video"))
			test->video = 1;
		else if (test && KEY("menus"))
			test->menus = 1;
		else if (test && KEY("card"))
		{
			if (value_length == 6 && !memcmp(value, "bottom", 6))
				test->card_bottom = 1;
			else if (!(value_length == 3 && !memcmp(value, "top", 3)))
				note_error(error, line, "card is top or bottom", value, value_length);
		}
		else if (test && KEY("retest"))
		{
			int n = number_of(value, value_length, 32767);

			if (n < 0)
				note_error(error, line, "bad retest revision", value, value_length);
			else
				test->retest = (short)n;
		}
		else
			note_error(error, line, "unknown line, key", start, key_length);
#undef KEY
	}
	if (test)
		finish_test(test, line);
	return have_list;
}

int ae_test_run_list_load(const char *path, struct ae_test_run_list *out)
{
	FILE *file;
	char *text;
	long size;
	int usable;

	memset(out, 0, sizeof(*out));
	file = fopen(path, "rb");
	if (!file)
		return 0;
	if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 || size > LIST_FILE_MAX || fseek(file, 0, SEEK_SET) != 0)
	{
		fclose(file);
		return 0;
	}
	text = (char *)malloc((size_t)size + 1);
	if (!text || fread(text, 1, (size_t)size, file) != (size_t)size)
	{
		free(text);
		fclose(file);
		return 0;
	}
	fclose(file);
	text[size] = 0;
	usable = ae_test_run_list_parse(text, out);
	free(text);
	return usable;
}
