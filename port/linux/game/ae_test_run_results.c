/* ae_test_run_results.c: Arena Evolved TEST RUN, the results file's records and the pending rules (see
ae_test_run_results.h). No engine includes. */

/* (fsync and fileno are POSIX, not C99) */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#if defined(__APPLE__)
/* (the macOS build's -D_ANSI_SOURCE hides unistd.h's POSIX calls) */
int fsync(int descriptor);
#endif
#include "ae_test_run_results.h"

/* (a line the writer makes: every byte of every field escaped as \u00XX is the worst case, about 4.5 KB) */
#define LINE_MAX_BYTES 5120

static const char *const verdict_name[] = { "", "PASS", "FAIL", "SKIP", "NOTE" };

/* ---- writing ---- */

/* appends text, JSON-escaped and quoted, at *at; 0 when the buffer is full */
static int put_string(char *buffer, int size, int *at, const char *text)
{
	int n = *at;

	if (n + 2 > size)
		return 0;
	buffer[n++] = '"';
	for (; *text; text++)
	{
		unsigned char c = (unsigned char)*text;

		if (n + 8 > size)
			return 0;
		if (c == '"' || c == '\\')
		{
			buffer[n++] = '\\';
			buffer[n++] = (char)c;
		}
		else if (c == '\n' || c == '\t' || c == '\r')
		{
			buffer[n++] = '\\';
			buffer[n++] = c == '\n' ? 'n' : c == '\t' ? 't' : 'r';
		}
		else if (c < 0x20)
			n += snprintf(buffer + n, 7, "\\u%04x", c);
		else
			buffer[n++] = (char)c;
	}
	buffer[n++] = '"';
	*at = n;
	return 1;
}

/* appends the literal text at *at */
static int put_text(char *buffer, int size, int *at, const char *text)
{
	int length = (int)strlen(text);

	if (*at + length >= size)
		return 0;
	memcpy(buffer + *at, text, (size_t)length);
	*at += length;
	return 1;
}

int ae_test_run_record_write(FILE *file, struct ae_test_run_record const *record)
{
	char buffer[LINE_MAX_BYTES], number[16];
	int at = 0;

	if (!file || !record || record->verdict <= AE_TEST_RUN_NONE || record->revision < 0 || record->verdict > AE_TEST_RUN_NOTE)
		return 0;
	snprintf(number, sizeof number, "%d", record->revision);
	if (!put_text(buffer, LINE_MAX_BYTES, &at, "{\"run\":") || !put_string(buffer, LINE_MAX_BYTES, &at, record->run)
		|| !put_text(buffer, LINE_MAX_BYTES, &at, ",\"list\":") || !put_string(buffer, LINE_MAX_BYTES, &at, record->list)
		|| !put_text(buffer, LINE_MAX_BYTES, &at, ",\"revision\":") || !put_text(buffer, LINE_MAX_BYTES, &at, number)
		|| !put_text(buffer, LINE_MAX_BYTES, &at, ",\"test\":") || !put_string(buffer, LINE_MAX_BYTES, &at, record->test)
		|| !put_text(buffer, LINE_MAX_BYTES, &at, ",\"verdict\":") || !put_string(buffer, LINE_MAX_BYTES, &at, verdict_name[record->verdict])
		|| !put_text(buffer, LINE_MAX_BYTES, &at, ",\"note\":") || !put_string(buffer, LINE_MAX_BYTES, &at, record->note)
		|| !put_text(buffer, LINE_MAX_BYTES, &at, ",\"shot\":") || !put_string(buffer, LINE_MAX_BYTES, &at, record->shot)
		|| !put_text(buffer, LINE_MAX_BYTES, &at, ",\"video\":") || !put_string(buffer, LINE_MAX_BYTES, &at, record->video)
		|| !put_text(buffer, LINE_MAX_BYTES, &at, ",\"build\":") || !put_string(buffer, LINE_MAX_BYTES, &at, record->build)
		|| !put_text(buffer, LINE_MAX_BYTES, &at, ",\"time\":") || !put_string(buffer, LINE_MAX_BYTES, &at, record->time)
		|| !put_text(buffer, LINE_MAX_BYTES, &at, "}\n"))
		return 0;
	if (fwrite(buffer, 1, (size_t)at, file) != (size_t)at || fflush(file) != 0)
		return 0;
#ifdef _WIN32
	return _commit(_fileno(file)) == 0;
#else
	return fsync(fileno(file)) == 0;
#endif
}

int ae_test_run_file_end_line(FILE *file)
{
	if (!file || fputc('\n', file) == EOF || fflush(file) != 0)
		return 0;
#ifdef _WIN32
	return _commit(_fileno(file)) == 0;
#else
	return fsync(fileno(file)) == 0;
#endif
}

/* ---- parsing ---- */

/* the line is at *at; true and *at moved past the literal, or false */
static int take_text(const char **at, const char *literal)
{
	size_t length = strlen(literal);

	if (strncmp(*at, literal, length))
		return 0;
	*at += length;
	return 1;
}

/* a quoted, escaped string at *at into a buffer of size bytes; false on a bad escape, a missing quote or an overflow */
static int take_string(const char **at, char *to, int size)
{
	const char *p = *at;
	int n = 0;

	if (*p++ != '"')
		return 0;
	while (*p != '"')
	{
		unsigned char c = (unsigned char)*p++;

		if (!c)
			return 0;
		if (c == '\\')
		{
			c = (unsigned char)*p++;
			switch (c)
			{
			case '"': case '\\': case '/': break;
			case 'n': c = '\n'; break;
			case 't': c = '\t'; break;
			case 'r': c = '\r'; break;
			case 'b': c = '\b'; break;
			case 'f': c = '\f'; break;
			case 'u':
			{
				int code = 0, i;

				for (i = 0; i < 4; i++, p++)
				{
					if (*p >= '0' && *p <= '9')
						code = code * 16 + (*p - '0');
					else if (*p >= 'a' && *p <= 'f')
						code = code * 16 + (*p - 'a' + 10);
					else if (*p >= 'A' && *p <= 'F')
						code = code * 16 + (*p - 'A' + 10);
					else
						return 0;
				}
				if (code == 0 || code > 0x7f) /* (the writer only escapes below 0x20) */
					return 0;
				c = (unsigned char)code;
				break;
			}
			default: return 0;
			}
		}
		if (n + 1 >= size)
			return 0;
		to[n++] = (char)c;
	}
	to[n] = 0;
	*at = p + 1;
	return 1;
}

/* a whole number 0..32767 at *at */
static int take_number(const char **at, short *to)
{
	const char *p = *at;
	long value = 0;

	if (*p < '0' || *p > '9')
		return 0;
	for (; *p >= '0' && *p <= '9'; p++)
	{
		value = value * 10 + (*p - '0');
		if (value > 32767)
			return 0;
	}
	*to = (short)value;
	*at = p;
	return 1;
}

int ae_test_run_record_parse(const char *line, struct ae_test_run_record *out)
{
	struct ae_test_run_record r;
	char verdict[16];
	const char *p = line;
	size_t length;
	int i;

	if (!line || !out)
		return 0;
	/* (a torn line has no closing brace: the line end may follow it, nothing else) */
	length = strlen(line);
	while (length && (line[length - 1] == '\n' || line[length - 1] == '\r'))
		length--;
	if (!length || line[length - 1] != '}')
		return 0;
	memset(&r, 0, sizeof r);
	if (!take_text(&p, "{\"run\":") || !take_string(&p, r.run, sizeof r.run)
		|| !take_text(&p, ",\"list\":") || !take_string(&p, r.list, sizeof r.list)
		|| !take_text(&p, ",\"revision\":") || !take_number(&p, &r.revision)
		|| !take_text(&p, ",\"test\":") || !take_string(&p, r.test, sizeof r.test)
		|| !take_text(&p, ",\"verdict\":") || !take_string(&p, verdict, sizeof verdict)
		|| !take_text(&p, ",\"note\":") || !take_string(&p, r.note, sizeof r.note)
		|| !take_text(&p, ",\"shot\":") || !take_string(&p, r.shot, sizeof r.shot)
		|| !take_text(&p, ",\"video\":") || !take_string(&p, r.video, sizeof r.video)
		|| !take_text(&p, ",\"build\":") || !take_string(&p, r.build, sizeof r.build)
		|| !take_text(&p, ",\"time\":") || !take_string(&p, r.time, sizeof r.time)
		|| *p++ != '}' || (size_t)(p - line) != length)
		return 0;
	for (i = AE_TEST_RUN_PASS; i <= AE_TEST_RUN_NOTE; i++)
		if (!strcmp(verdict, verdict_name[i]))
			r.verdict = (enum ae_test_run_verdict)i;
	if (r.verdict == AE_TEST_RUN_NONE)
		return 0;
	*out = r;
	return 1;
}

/* ---- states ---- */

/* a line's record, or after a torn fragment glued in front of it (a crash left no line end) the record that starts at
the last "{"run":" in the line (a quote inside a field is escaped, so this text only starts a record) */
static int record_of_line(const char *line, struct ae_test_run_record *out)
{
	const char *key = "{\"run\":", *last = NULL, *at = line;

	if (ae_test_run_record_parse(line, out))
		return 1;
	while ((at = strstr(at, key)) != NULL)
		last = at++;
	return last && last != line && ae_test_run_record_parse(last, out);
}

/* the states from the results text, leaving out the records of run `skip` (NULL: none left out); when `last_run` is not
NULL it is filled with the run of the last record that reads */
static void states_of(struct ae_test_run_list const *list, const char *results_text, const char *skip,
	enum ae_test_run_state *states, char *last_run)
{
	const char *p = results_text;
	char line[LINE_MAX_BYTES];
	int i;

	for (i = 0; i < list->tests; i++)
		states[i] = AE_TEST_RUN_TODO;
	while (p && *p)
	{
		struct ae_test_run_record record;
		const char *end = strchr(p, '\n');
		size_t length = end ? (size_t)(end - p) : strlen(p);

		/* (a line longer than the writer makes is garbage, skipped like a torn one) */
		if (length < sizeof line)
		{
			memcpy(line, p, length);
			line[length] = 0;
			if (record_of_line(line, &record))
			{
				if (last_run)
					snprintf(last_run, 24, "%s", record.run);
				if (!strcmp(record.list, list->id) && record.verdict != AE_TEST_RUN_NOTE
					&& !(skip && !strcmp(record.run, skip)))
					for (i = 0; i < list->tests; i++)
						if (!strcmp(record.test, list->test[i].id))
						{
							if (record.revision >= list->test[i].retest)
								states[i] = record.verdict == AE_TEST_RUN_PASS ? AE_TEST_RUN_PASSED
									: record.verdict == AE_TEST_RUN_FAIL ? AE_TEST_RUN_FAILED : AE_TEST_RUN_SKIPPED;
							break;
						}
			}
		}
		p = end ? end + 1 : NULL;
	}
}

void ae_test_run_states(struct ae_test_run_list const *list, const char *results_text, enum ae_test_run_state *states)
{
	if (!list || !states)
		return;
	states_of(list, results_text, NULL, states, NULL);
}

short ae_test_run_resume_index(struct ae_test_run_list const *list, const char *results_text)
{
	enum ae_test_run_state before[AE_TEST_RUN_TESTS], scratch[AE_TEST_RUN_TESTS];
	short in_last[AE_TEST_RUN_TESTS];
	char last_run[24] = "";
	const char *p = results_text;
	char line[LINE_MAX_BYTES];
	int i;

	if (!list || !results_text || list->tests > AE_TEST_RUN_TESTS)
		return -1;
	states_of(list, results_text, NULL, scratch, last_run);
	if (!last_run[0])
		return -1;
	/* (the states the last run started from, and which tests it recorded: any verdict, a NOTE too) */
	states_of(list, results_text, last_run, before, NULL);
	memset(in_last, 0, sizeof in_last);
	while (p && *p)
	{
		struct ae_test_run_record record;
		const char *end = strchr(p, '\n');
		size_t length = end ? (size_t)(end - p) : strlen(p);

		if (length < sizeof line)
		{
			memcpy(line, p, length);
			line[length] = 0;
			if (record_of_line(line, &record) && !strcmp(record.run, last_run) && !strcmp(record.list, list->id))
				for (i = 0; i < list->tests; i++)
					if (!strcmp(record.test, list->test[i].id))
					{
						in_last[i] = 1;
						break;
					}
		}
		p = end ? end + 1 : NULL;
	}
	for (i = 0; i < list->tests; i++)
		if (before[i] != AE_TEST_RUN_PASSED && !in_last[i] && !list->test[i].error[0])
			return (short)i;
	return -1;
}

short ae_test_run_pending(struct ae_test_run_list const *list, enum ae_test_run_state const *states)
{
	short count = 0;
	int i;

	if (!list || !states)
		return 0;
	for (i = 0; i < list->tests; i++)
		if (states[i] != AE_TEST_RUN_PASSED)
			count++;
	return count;
}

short ae_test_run_first_pending(struct ae_test_run_list const *list, enum ae_test_run_state const *states)
{
	int i;

	if (!list || !states)
		return -1;
	for (i = 0; i < list->tests; i++)
		if (states[i] != AE_TEST_RUN_PASSED)
			return (short)i;
	return -1;
}
