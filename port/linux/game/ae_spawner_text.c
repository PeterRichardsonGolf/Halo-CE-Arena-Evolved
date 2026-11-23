/*
AE_SPAWNER_TEXT.C

The line reader the spawner text files share (ae_spawner_text.h).
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_spawner_text.h"

void ae_spawner_text_begin(struct ae_spawner_text *reader, char const *text, long length)
{
	memset(reader, 0, sizeof(*reader));
	reader->text = text;
	reader->length = text ? length : 0;
}

void ae_spawner_text_say(char *error, int error_size, char const *text, char const *word)
{
	if (error && error_size > 0)
		snprintf(error, (size_t)error_size, "%s%s%s%s", text, word ? " \"" : "", word ? word : "", word ? "\"" : "");
}

int ae_spawner_text_next(struct ae_spawner_text *reader, char *error, int error_size)
{
	if (!reader->text || reader->length < 0 || reader->length > AE_SPAWNER_TEXT_MAXIMUM_FILE_SIZE)
	{
		ae_spawner_text_say(error, error_size, "the file is missing or bigger than 64 KB", NULL);
		return -1;
	}
	while (reader->position < reader->length)
	{
		long start = reader->position, size;
		char *walk, *hash;

		reader->count = 0;
		while (reader->position < reader->length && reader->text[reader->position] != '\n')
			reader->position++;
		size = reader->position - start;
		reader->position++;
		reader->line_number++;
		if (reader->line_number > AE_SPAWNER_TEXT_MAXIMUM_LINES)
		{
			ae_spawner_text_say(error, error_size, "more than 512 lines", NULL);
			return -1;
		}
		if (size > 0 && reader->text[start + size - 1] == '\r')
			size--;
		if (size > AE_SPAWNER_TEXT_MAXIMUM_LINE)
		{
			ae_spawner_text_say(error, error_size, "a line longer than 200 characters", NULL);
			return -1;
		}
		memcpy(reader->line, reader->text + start, (size_t)size);
		reader->line[size] = 0;
		if (memchr(reader->line, 0, (size_t)size))
		{
			ae_spawner_text_say(error, error_size, "a NUL in the line", NULL);
			return -1;
		}
		hash = strchr(reader->line, '#');
		if (hash)
			*hash = 0;
		for (walk = reader->line; *walk;)
		{
			while (*walk == ' ' || *walk == '\t')
				*walk++ = 0;
			if (!*walk)
				break;
			if (reader->count == AE_SPAWNER_TEXT_MAXIMUM_WORDS)
			{
				ae_spawner_text_say(error, error_size, "too many words", NULL);
				return -1;
			}
			if (*walk == '"')
			{
				char *close = strchr(walk + 1, '"');

				if (!close)
				{
					ae_spawner_text_say(error, error_size, "a quote not closed", NULL);
					return -1;
				}
				*close = 0;
				reader->words[reader->count++] = walk + 1;
				walk = close + 1;
				if (*walk && *walk != ' ' && *walk != '\t')
				{
					ae_spawner_text_say(error, error_size, "a quoted word runs into the next", NULL);
					return -1;
				}
				continue;
			}
			reader->words[reader->count++] = walk;
			while (*walk && *walk != ' ' && *walk != '\t')
				walk++;
		}
		if (reader->count)
			return 1;
	}
	reader->count = 0;
	return 0;
}

int ae_spawner_text_same(char const *a, char const *b)
{
	if (!a || !b) return 0;
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

int ae_spawner_text_whole(char const *word, long low, long high, long *value)
{
	char *end;
	long number;

	if (!word || !*word) return 0;
	number = strtol(word, &end, 10);
	if (end == word || *end || number < low || number > high)
		return 0;
	*value = number;
	return 1;
}

int ae_spawner_text_real(char const *word, real *value)
{
	char *end;
	double number;

	if (!word || !*word) return 0;
	number = strtod(word, &end);
	if (end == word || *end || number != number || number > 1.0e7 || number < -1.0e7)
		return 0;
	*value = (real)number;
	return 1;
}

int ae_spawner_text_hash(char const *word, unsigned long *value)
{
	char *end;
	unsigned long number;
	int index;

	if (!word || strlen(word) != 8)
		return 0;
	for (index = 0; index < 8; index++)
		if (!isxdigit((unsigned char)word[index]))
			return 0;
	number = strtoul(word, &end, 16);
	if (*end)
		return 0;
	*value = number;
	return 1;
}

int ae_spawner_text_name(char const *word, char *out, int size)
{
	int length = 0;

	if (!word || !*word || size < 2) return 0;
	for (; word[length]; length++)
	{
		int c = (unsigned char)word[length];

		if (!(isalnum(c) || c == '_' || c == ':' || c == '.' || c == '-'))
			return 0;
		if (length >= size - 1)
			return 0;
	}
	memcpy(out, word, (size_t)length + 1);
	return 1;
}
